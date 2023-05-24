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

#include "src/core/lib/event_engine/cf_engine/cfhost_dns_resolver.h"
#include "src/core/lib/event_engine/trace.h"

namespace grpc_event_engine {
namespace experimental {

CFHostDNSResolverImpl::CFHostDNSResolverImpl(
    std::shared_ptr<CFEventEngine> engine)
    : engine_(std::move(engine)) {}

CFHostDNSResolverImpl::~CFHostDNSResolverImpl() {}

void CFHostDNSResolverImpl::Shutdown() {}

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE
