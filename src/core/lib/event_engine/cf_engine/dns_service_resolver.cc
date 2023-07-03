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
#include "src/core/lib/event_engine/trace.h"
#include "src/core/lib/gprpp/host_port.h"

namespace grpc_event_engine {
namespace experimental {

DNSServiceResolverImpl::DNSServiceResolverImpl(
    std::shared_ptr<CFEventEngine> engine)
    : engine_(std::move(engine)) {}

DNSServiceResolverImpl::~DNSServiceResolverImpl() {}

void DNSServiceResolverImpl::Shutdown() {
  GRPC_EVENT_ENGINE_DNS_TRACE("Shutdown: sdRef=%p", sdRef_);

  DNSServiceRefDeallocate(sdRef_);
}

void DNSServiceResolverImpl::LookupHostname(
    EventEngine::DNSResolver::LookupHostnameCallback on_resolve,
    absl::string_view name, absl::string_view default_port) {
  std::string host;
  std::string port_string;
  if (!grpc_core::SplitHostPort(name, &host, &port_string)) {
    on_resolve(
        absl::InvalidArgumentError(absl::StrCat("Unparseable name: ", name)));

    GRPC_EVENT_ENGINE_DNS_TRACE(
        "==== DNSServiceResolverImpl::LookupHostname: Unparseable name=%s",
        (name.data() ? name.data() : ""));
    return;
  }
  GPR_ASSERT(!host.empty());
  if (port_string.empty()) {
    if (default_port.empty()) {
      on_resolve(absl::InvalidArgumentError(absl::StrFormat(
          "No port in name %s or default_port argument", name)));
      return;
    }
    port_string = std::string(default_port);
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
  auto error = DNSServiceGetAddrInfo(
      &sdRef, kDNSServiceFlagsTimeout, 0,
      kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6, host.c_str(),
      &DNSServiceResolverImpl::ResolveCallback, Ref().release());

  if (error != kDNSServiceErr_NoError) {
    on_resolve(absl::UnknownError(
        absl::StrFormat("DNSServiceGetAddrInfo failed with error:%d", error)));

    GRPC_EVENT_ENGINE_DNS_TRACE(
        "DNSServiceGetAddrInfo failed: sdRef=%p, host=%s, error: %d", sdRef,
        host.c_str(), error);
    return;
  }

  error = DNSServiceSetDispatchQueue(
      sdRef, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0));
  if (error != kDNSServiceErr_NoError) {
    on_resolve(absl::UnknownError(absl::StrFormat(
        "DNSServiceSetDispatchQueue failed with error:%d", error)));

    GRPC_EVENT_ENGINE_DNS_TRACE(
        "DNSServiceSetDispatchQueue failed: sdRef=%p, host=%s, error: %d",
        sdRef, host.c_str(), error);
    return;
  }

  GRPC_EVENT_ENGINE_DNS_TRACE(
      "==== LookupHostname: sdRef=%p, host=%s, port: %s", sdRef, host.c_str(),
      port_string.c_str());

  sdRef_ = sdRef;

  on_resolve_ = std::move(on_resolve);
}

/* static */
void DNSServiceResolverImpl::ResolveCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char* hostname,
    const struct sockaddr* address, uint32_t ttl, void* context) {
  auto that = static_cast<DNSServiceResolverImpl*>(context);

  if (errorCode != kDNSServiceErr_NoError) {
    that->on_resolve_(absl::UnknownError(
        absl::StrFormat("ResolveCallback failed with error:%d", errorCode)));
    DNSServiceRefDeallocate(sdRef);

    GRPC_EVENT_ENGINE_DNS_TRACE(
        "ResolveCallback with error: sdRef=%p, hostname: %s, error: %d", sdRef,
        hostname, errorCode);
    return;
  }

  GRPC_EVENT_ENGINE_DNS_TRACE(
      "==== DNSServiceResolverImpl::ResolveCallback: sdRef=%p, flags=%x, "
      "interfaceIndex=%d, errorCode=%d, hostname=%s, address=%p, "
      "address_family: %d, address_len: %d, ttl=%d, context=%p",
      sdRef, flags, interfaceIndex, errorCode, hostname, address,
      address->sa_family, address->sa_len, ttl, context);

  //   address->sin_port = htons(hostname_qa->port);
  that->result_.emplace_back(address, address->sa_len);

  if (!(flags & kDNSServiceFlagsMoreComing)) {
    that->on_resolve_(std::move(that->result_));
    DNSServiceRefDeallocate(sdRef);
  }
}

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE
