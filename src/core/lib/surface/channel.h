/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef GRPC_CORE_LIB_SURFACE_CHANNEL_H
#define GRPC_CORE_LIB_SURFACE_CHANNEL_H

#include <grpc/support/port_platform.h>

#include <map>

#include "src/core/lib/channel/channel_stack.h"
#include "src/core/lib/channel/channel_stack_builder.h"
#include "src/core/lib/channel/channelz.h"
#include "src/core/lib/gprpp/manual_constructor.h"
#include "src/core/lib/resource_quota/memory_quota.h"
#include "src/core/lib/surface/channel_stack_type.h"

/// Creates a grpc_channel.
grpc_channel* grpc_channel_create(const char* target,
                                  const grpc_channel_args* args,
                                  grpc_channel_stack_type channel_stack_type,
                                  grpc_transport* optional_transport,
                                  grpc_error_handle* error);

/** The same as grpc_channel_destroy, but doesn't create an ExecCtx, and so
 * is safe to use from within core. */
void grpc_channel_destroy_internal(grpc_channel* channel);

/// Creates a grpc_channel with a builder. See the description of
/// \a grpc_channel_create for variable definitions.
grpc_channel* grpc_channel_create_with_builder(
    grpc_channel_stack_builder* builder,
    grpc_channel_stack_type channel_stack_type,
    grpc_error_handle* error = nullptr);

/** Create a call given a grpc_channel, in order to call \a method.
    Progress is tied to activity on \a pollset_set. The returned call object is
    meant to be used with \a grpc_call_start_batch_and_execute, which relies on
    callbacks to signal completions. \a method and \a host need
    only live through the invocation of this function. If \a parent_call is
    non-NULL, it must be a server-side call. It will be used to propagate
    properties from the server call to this new client call, depending on the
    value of \a propagation_mask (see propagation_bits.h for possible values) */
grpc_call* grpc_channel_create_pollset_set_call(
    grpc_channel* channel, grpc_call* parent_call, uint32_t propagation_mask,
    grpc_pollset_set* pollset_set, const grpc_slice& method,
    const grpc_slice* host, grpc_millis deadline, void* reserved);

/** Get a (borrowed) pointer to this channels underlying channel stack */
grpc_channel_stack* grpc_channel_get_channel_stack(grpc_channel* channel);

grpc_core::channelz::ChannelNode* grpc_channel_get_channelz_node(
    grpc_channel* channel);

size_t grpc_channel_get_call_size_estimate(grpc_channel* channel);
void grpc_channel_update_call_size_estimate(grpc_channel* channel, size_t size);

namespace grpc_core {

struct RegisteredCall {
  Slice path;
  absl::optional<Slice> authority;

  explicit RegisteredCall(const char* method_arg, const char* host_arg);
  RegisteredCall(const RegisteredCall& other);
  RegisteredCall& operator=(const RegisteredCall&) = delete;

  ~RegisteredCall();
};

struct CallRegistrationTable {
  Mutex mu;
  // The map key should be owned strings rather than unowned char*'s to
  // guarantee that it outlives calls on the core channel (which may outlast the
  // C++ or other wrapped language Channel that registered these calls).
  std::map<std::pair<std::string, std::string>, RegisteredCall> map
      ABSL_GUARDED_BY(mu);
  int method_registration_attempts ABSL_GUARDED_BY(mu) = 0;
};

const grpc_channel* channel;

// Ping the channels peer (load balanced channels will select one sub-channel to
// ping); if the channel is not connected, posts a failed.
void grpc_channel_ping(grpc_channel* channel, grpc_completion_queue* cq,
                       void* tag, void* reserved);

#endif /* GRPC_CORE_LIB_SURFACE_CHANNEL_H */
