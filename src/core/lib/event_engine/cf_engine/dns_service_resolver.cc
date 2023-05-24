// Copyright 2023 The gRPC Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <grpc/support/port_platform.h>

#ifdef GPR_APPLE

#include "absl/strings/str_format.h"

#include "src/core/lib/address_utils/parse_address.h"
#include "src/core/lib/event_engine/cf_engine/dns_service_resolver.h"
#include "src/core/lib/event_engine/posix_engine/lockfree_event.h"
#include "src/core/lib/event_engine/tcp_socket_utils.h"
#include "src/core/lib/event_engine/trace.h"
#include "src/core/lib/gprpp/host_port.h"

namespace grpc_event_engine {
namespace experimental {

void DNSServiceResolverImpl::LookupHostname(
    EventEngine::DNSResolver::LookupHostnameCallback on_resolve,
    absl::string_view name, absl::string_view default_port) {
  GRPC_EVENT_ENGINE_DNS_TRACE(
      "DNSServiceResolverImpl::LookupHostname: name: %.*s, default_port: %.*s, "
      "this: %p",
      static_cast<int>(name.length()), name.data(),
      static_cast<int>(default_port.length()), default_port.data(), this);

  absl::string_view host;
  absl::string_view port_string;
  if (!grpc_core::SplitHostPort(name, &host, &port_string)) {
    on_resolve(
        absl::InvalidArgumentError(absl::StrCat("Unparseable name: ", name)));
    return;
  }
  GPR_ASSERT(!host.empty());
  if (port_string.empty()) {
    if (default_port.empty()) {
      on_resolve(absl::InvalidArgumentError(absl::StrFormat(
          "No port in name %s or default_port argument", name)));
      return;
    }
    port_string = default_port;
  }

  int port = 0;
  if (port_string == "http") {
    port = 80;
  } else if (port_string == "https") {
    port = 443;
  } else if (!absl::SimpleAtoi(port_string, &port)) {
    on_resolve(absl::InvalidArgumentError(
        absl::StrCat("Failed to parse port in name: ", name)));
    return;
  }

  // TODO(yijiem): Change this when refactoring code in
  // src/core/lib/address_utils to use EventEngine::ResolvedAddress.
  grpc_resolved_address addr;
  const std::string hostport = grpc_core::JoinHostPort(host, port);
  if (grpc_parse_ipv4_hostport(hostport.c_str(), &addr,
                               true /* log errors */) ||
      grpc_parse_ipv6_hostport(hostport.c_str(), &addr,
                               true /* log errors */)) {
    // Early out if the target is an ipv4 or ipv6 literal.
    std::vector<EventEngine::ResolvedAddress> result;
    result.emplace_back(reinterpret_cast<sockaddr*>(addr.addr), addr.len);
    on_resolve(std::move(result));
    return;
  }

  DNSServiceRef sdRef;
  auto host_string = std::string{host};
  auto error = DNSServiceGetAddrInfo(
      &sdRef, kDNSServiceFlagsTimeout | kDNSServiceFlagsReturnIntermediates, 0,
      kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6, host_string.c_str(),
      &DNSServiceResolverImpl::ResolveCallback, this /* do not Ref */);

  if (error != kDNSServiceErr_NoError) {
    on_resolve(absl::UnknownError(
        absl::StrFormat("DNSServiceGetAddrInfo failed with error:%d", error)));
    return;
  }

  grpc_core::ReleasableMutexLock lock(&request_mu_);

  error = DNSServiceSetDispatchQueue(sdRef, queue_);
  if (error != kDNSServiceErr_NoError) {
    on_resolve(absl::UnknownError(absl::StrFormat(
        "DNSServiceSetDispatchQueue failed with error:%d", error)));
    return;
  }

  requests_.try_emplace(
      sdRef, DNSServiceRequest{
                 std::move(on_resolve), static_cast<uint16_t>(port), {}});
}

/* static */
void DNSServiceResolverImpl::ResolveCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char* hostname,
    const struct sockaddr* address, uint32_t ttl, void* context) {
  GRPC_EVENT_ENGINE_DNS_TRACE(
      "DNSServiceResolverImpl::ResolveCallback: sdRef: %p, flags: %x, "
      "interface: %d, errorCode: %d, hostname: %s, addressFamily: %d, ttl: "
      "%d, "
      "this: %p",
      sdRef, flags, interfaceIndex, errorCode, hostname, address->sa_family,
      ttl, context);

  // more results are coming, ignore intermediate no record error
  if (errorCode == kDNSServiceErr_NoSuchRecord &&
      flags & kDNSServiceFlagsMoreComing) {
    return;
  }

  // no need to increase refcount here, since ResolveCallback and Shutdown is
  // called from the serial queue and it is guarenteed that it won't be called
  // after the sdRef is deallocated
  auto that = static_cast<DNSServiceResolverImpl*>(context);

  grpc_core::ReleasableMutexLock lock(&that->request_mu_);
  auto request_it = that->requests_.find(sdRef);
  GPR_ASSERT(request_it != that->requests_.end());
  auto& request = request_it->second;

  if (errorCode != kDNSServiceErr_NoError) {
    auto status =
        errorCode == kDNSServiceErr_NoSuchRecord
            ? absl::NotFoundError(absl::StrFormat(
                  "address lookup failed for %s: Domain name not found",
                  hostname))
            : absl::UnknownError(
                  absl::StrFormat("address lookup failed for %s: errorCode: %d",
                                  hostname, errorCode));
    request.on_resolve_(std::move(status));
    that->requests_.erase(request_it);
    DNSServiceRefDeallocate(sdRef);
    return;
  }

  request.result_.emplace_back(address, address->sa_len);
  auto& resolved_address = request.result_.back();
  if (address->sa_family == AF_INET) {
    ((struct sockaddr_in*)resolved_address.address())->sin_port =
        htons(request.port_);
  } else if (address->sa_family == AF_INET6) {
    ((struct sockaddr_in6*)resolved_address.address())->sin6_port =
        htons(request.port_);
  }

  GRPC_EVENT_ENGINE_DNS_TRACE(
      "DNSServiceResolverImpl::ResolveCallback: "
      "sdRef: %p, hostname: %s, addressPort: %s, this: %p",
      sdRef, hostname,
      ResolvedAddressToURI(resolved_address).value_or("ERROR").c_str(),
      context);

  if (!(flags & kDNSServiceFlagsMoreComing)) {
    request.on_resolve_(std::move(request.result_));
    that->requests_.erase(request_it);
    DNSServiceRefDeallocate(sdRef);
  }
}

void DNSServiceResolverImpl::Shutdown() {
  dispatch_async_f(queue_, Ref().release(), [](void* thatPtr) {
    grpc_core::RefCountedPtr<DNSServiceResolverImpl> that{
        static_cast<DNSServiceResolverImpl*>(thatPtr)};
    grpc_core::MutexLock lock(&that->request_mu_);
    for (auto& [sdRef, request] : that->requests_) {
      GRPC_EVENT_ENGINE_DNS_TRACE(
          "DNSServiceResolverImpl::Shutdown sdRef: %p, this: %p", sdRef,
          thatPtr);

      request.on_resolve_(
          absl::CancelledError("DNSServiceResolverImpl::Shutdown"));
      DNSServiceRefDeallocate(static_cast<DNSServiceRef>(sdRef));
    }
    that->requests_.clear();
  });
}

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE
