// Copyright 2022 The gRPC Authors
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
#ifndef GRPC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFSTREAM_CLIENT_H
#define GRPC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFSTREAM_CLIENT_H
#include <grpc/support/port_platform.h>

#ifdef GPR_APPLE

#include <CoreFoundation/CoreFoundation.h>

#include <grpc/event_engine/event_engine.h>

#include "src/core/lib/address_utils/sockaddr_utils.h"
#include "src/core/lib/event_engine/cf_engine/cfstream_endpoint.h"
#include "src/core/lib/event_engine/cf_engine/cftype_unique_ref.h"
#include "src/core/lib/event_engine/tcp_socket_utils.h"
#include "src/core/lib/gprpp/host_port.h"

namespace grpc_event_engine {
namespace experimental {

class CFStreamClient {};

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE

#endif  // GRPC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFSTREAM_CLIENT_H