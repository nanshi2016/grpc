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
#ifndef GRPC_SRC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFHOST_DNS_RESOLVER_H
#define GRPC_SRC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFHOST_DNS_RESOLVER_H
#include <grpc/support/port_platform.h>

#ifdef GPR_APPLE

#include <CoreFoundation/CoreFoundation.h>

#include <grpc/event_engine/event_engine.h>

#include "src/core/lib/event_engine/cf_engine/cf_engine.h"
#include "src/core/lib/event_engine/cf_engine/cftype_unique_ref.h"
#include "src/core/lib/gprpp/ref_counted.h"
#include "src/core/lib/gprpp/ref_counted_ptr.h"

namespace grpc_event_engine {
namespace experimental {

class CFHostDNSResolverImpl
    : public grpc_core::RefCounted<CFHostDNSResolverImpl> {
 public:
  CFHostDNSResolverImpl(std::shared_ptr<CFEventEngine> engine);
  ~CFHostDNSResolverImpl();

  void Shutdown();

 private:
  std::shared_ptr<CFEventEngine> engine_;
};

class CFHostDNSResolver : public EventEngine::DNSResolver {
 public:
  CFHostDNSResolver(std::shared_ptr<CFEventEngine> engine) {
    impl_ =
        grpc_core::MakeRefCounted<CFHostDNSResolverImpl>(std::move((engine)));
  }

  ~CFHostDNSResolver() override { impl_->Shutdown(); }

  EventEngine::DNSResolver::LookupTaskHandle LookupHostname(
      EventEngine::DNSResolver::LookupHostnameCallback on_resolve,
      absl::string_view name, absl::string_view default_port,
      EventEngine::Duration timeout) override {
    return {0, 0};
  };

  EventEngine::DNSResolver::LookupTaskHandle LookupSRV(
      EventEngine::DNSResolver::LookupSRVCallback on_resolve,
      absl::string_view name, EventEngine::Duration timeout) override {
    on_resolve(absl::UnimplementedError(
        "The Native resolver does not support looking up SRV records"));
    return {0, 0};
  }

  EventEngine::DNSResolver::LookupTaskHandle LookupTXT(
      EventEngine::DNSResolver::LookupTXTCallback on_resolve,
      absl::string_view name, EventEngine::Duration timeout) override {
    on_resolve(absl::UnimplementedError(
        "The Native resolver does not support looking up TXT records"));
    return {0, 0};
  }

  bool CancelLookup(
      EventEngine::DNSResolver::LookupTaskHandle handle) override {
    return false;
  }

 private:
  std::shared_ptr<CFEventEngine> engine_;
  grpc_core::RefCountedPtr<CFHostDNSResolverImpl> impl_;
};

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE

#endif  // GRPC_SRC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFHOST_DNS_RESOLVER_H
