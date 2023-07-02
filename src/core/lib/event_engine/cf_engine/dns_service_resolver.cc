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

#include "src/core/lib/event_engine/cf_engine/dns_service_resolver.h"
#include "src/core/lib/event_engine/trace.h"

namespace grpc_event_engine {
namespace experimental {

DNSServiceResolverImpl::DNSServiceResolverImpl(
    std::shared_ptr<CFEventEngine> engine)
    : engine_(std::move(engine)) {}

DNSServiceResolverImpl::~DNSServiceResolverImpl() {}

void DNSServiceResolverImpl::Shutdown() {}

void DNSServiceResolverImpl::LookupHostname(
    EventEngine::DNSResolver::LookupHostnameCallback on_resolve,
    absl::string_view name, absl::string_view default_port) {
  DNSServiceRef sdRef;
  auto error =
      DNSServiceGetAddrInfo(&sdRef, 0, 0, kDNSServiceProtocol_IPv4, name.data(),
                            &DNSServiceResolverImpl::ResolveCallback, this);

  if (error != kDNSServiceErr_NoError) {
    on_resolve(absl::UnknownError(
        absl::StrFormat("DNSServiceGetAddrInfo failed with error:%d", error)));
  }
}

/* static */
void DNSServiceResolverImpl::ResolveCallback(
    DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
    DNSServiceErrorType errorCode, const char* hostname,
    const struct sockaddr* address, uint32_t ttl, void* context) {
  auto that = static_cast<DNSServiceResolverImpl*>(context);
  GRPC_EVENT_ENGINE_DNS_TRACE(
      "DNSServiceResolverImpl::ResolveCallback: sdRef=%p, flags=%d, "
      "interfaceIndex=%d, errorCode=%d, hostname=%s, address=%p, ttl=%d, "
      "context=%p",
      sdRef, flags, interfaceIndex, errorCode, hostname, address, ttl, context);
}

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE
