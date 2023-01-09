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
#ifndef GRPC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFRUNLOOP_POLLER_H
#define GRPC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFRUNLOOP_POLLER_H
#include <grpc/support/port_platform.h>

#ifdef GPR_APPLE

#include <CoreFoundation/CFRunLoop.h>

#include <grpc/event_engine/event_engine.h>

#include "src/core/lib/event_engine/poller.h"
#include "src/core/lib/gprpp/sync.h"
#include "src/core/lib/gprpp/thd.h"

namespace grpc_event_engine {
namespace experimental {

class CFRunLoopPoller : public grpc_event_engine::experimental::Poller {
  WorkResult Work(EventEngine::Duration timeout,
                  absl::FunctionRef<void()> schedule_poll_again) override{};

  void Kick() override{};

 private:
  static void RunLoopThreadFunc(void* arg) {
    auto self = static_cast<CFRunLoopPoller*>(arg);
    self->cf_run_loop_ = CFRunLoopGetCurrent();
    CFRunLoopRun();
  }

 private:
  grpc_core::Mutex mu_;
  CFRunLoopRef cf_run_loop_;
};

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE

#endif  // GRPC_CORE_LIB_EVENT_ENGINE_CF_ENGINE_CFRUNLOOP_POLLER_H