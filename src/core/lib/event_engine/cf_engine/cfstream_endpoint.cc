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

#include <grpc/support/port_platform.h>

#ifdef GPR_APPLE

#include "src/core/lib/event_engine/cf_engine/cfstream_endpoint.h"

namespace grpc_event_engine {
namespace experimental {

namespace {
absl::Status CFErrorToStatus(CFTypeUniqueRef<CFErrorRef> cf_error) {
  CFErrorDomain cf_domain = CFErrorGetDomain((cf_error));
  CFIndex code = CFErrorGetCode((cf_error));
  CFTypeUniqueRef<CFStringRef> cf_desc = CFErrorCopyDescription((cf_error));
  char domain_buf[256];
  char desc_buf[256];
  CFStringGetCString(cf_domain, domain_buf, 256, kCFStringEncodingUTF8);
  CFStringGetCString(cf_desc, desc_buf, 256, kCFStringEncodingUTF8);
  return absl::Status(absl::StatusCode::kInternal,
                      absl::StrFormat("(domain:%s, code:%ld, description:%s)",
                                      domain_buf, code, desc_buf));
}
}  // namespace

void CFStreamEndpoint::Connect(EventEngine::OnConnectCallback on_connect,
                               EventEngine::ResolvedAddress addr,
                               EventEngine::Duration timeout) {
  peer_address_ = std::move(addr);
  std::string host_port =
      grpc_sockaddr_to_string(reinterpret_cast<const grpc_resolved_address*>(
                                  peer_address_.address()),
                              true)
          .value();
  gpr_log(GPR_INFO, "CFStreamClient::connect, host_port: %s",
          host_port.c_str());
  std::string host_string;
  std::string port_string;
  grpc_core::SplitHostPort(host_port, &host_string, &port_string);
  CFStringRef host = CFStringCreateWithCString(NULL, host_string.c_str(),
                                               kCFStringEncodingUTF8);
  int port = ResolvedAddressGetPort(peer_address_);
  CFStreamCreatePairWithSocketToHost(NULL, host, port, &cf_read_stream_,
                                     &cf_write_stream_);

  CFStreamClientContext cf_context = {0, static_cast<void*>(this), nullptr,
                                      nullptr, nullptr};
  CFReadStreamSetClient(
      cf_read_stream_,
      kCFStreamEventOpenCompleted | kCFStreamEventHasBytesAvailable |
          kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered,
      ReadCallback, &cf_context);
  CFWriteStreamSetClient(
      cf_write_stream_,
      kCFStreamEventOpenCompleted | kCFStreamEventCanAcceptBytes |
          kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered,
      WriteCallback, &cf_context);
  CFReadStreamSetDispatchQueue(cf_read_stream_,
                               dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0));
  CFWriteStreamSetDispatchQueue(
      cf_write_stream_, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0));

  if (!CFReadStreamOpen(cf_read_stream_)) {
    auto status = CFErrorToStatus(CFReadStreamCopyError(cf_read_stream_));
    on_connect(std::move(status));
    delete this;
    return;
  }

  if (!CFWriteStreamOpen(cf_write_stream_)) {
    auto status = CFErrorToStatus(CFWriteStreamCopyError(cf_write_stream_));
    on_connect(std::move(status));
    delete this;
    return;
  }

  auto connect_timeout_timer = engine_->RunAfter(timeout, [this]() {
    open_event_.SetShutdown(absl::DeadlineExceededError("Connect timed out"));
  });

  open_event_.NotifyOn(new PosixEngineClosure(
      [this, on_connect = std::move(on_connect),
       connect_timeout_timer](absl::Status status) mutable {
        engine_->Cancel(connect_timeout_timer);
        if (status.ok()) {
          on_connect(std::unique_ptr<EventEngine::Endpoint>(this));
        } else {
          on_connect(std::move(status));
          delete this;
        }
      },
      false /* is_permanent */));
}

/* static */ void CFStreamEndpoint::ReadCallback(CFReadStreamRef stream,
                                                 CFStreamEventType type,
                                                 void* client_callback_info) {
  auto self = static_cast<CFStreamEndpoint*>(client_callback_info);
  gpr_log(GPR_INFO, "CFStream ReadCallback (%p, %p, %lu)", self, stream, type);

  switch (type) {
    case kCFStreamEventOpenCompleted:
      break;
    case kCFStreamEventHasBytesAvailable:
    case kCFStreamEventEndEncountered:
      self->read_event_.SetReady();
      break;
    case kCFStreamEventErrorOccurred: {
      auto status = CFErrorToStatus(CFReadStreamCopyError(stream));
      gpr_log(GPR_ERROR, "CFStream Read error: %s", status.ToString().c_str());
      self->open_event_.SetShutdown(status);
      self->read_event_.SetShutdown(status);
      self->write_event_.SetShutdown(status);
    } break;
    default:
      GPR_UNREACHABLE_CODE(return);
  }
}

/* static */
void CFStreamEndpoint::WriteCallback(CFWriteStreamRef stream,
                                     CFStreamEventType type,
                                     void* client_callback_info) {
  auto self = static_cast<CFStreamEndpoint*>(client_callback_info);
  gpr_log(GPR_INFO, "CFStream WriteCallback (%p, %p, %lu)", self, stream, type);

  switch (type) {
    case kCFStreamEventOpenCompleted:
      self->open_event_.SetReady();
      break;
    case kCFStreamEventCanAcceptBytes:
    case kCFStreamEventEndEncountered:
      self->write_event_.SetReady();
      break;
    case kCFStreamEventErrorOccurred: {
      auto status = CFErrorToStatus(CFWriteStreamCopyError(stream));
      gpr_log(GPR_ERROR, "CFStream Write error: %s", status.ToString().c_str());
      self->open_event_.SetShutdown(status);
      self->read_event_.SetShutdown(status);
      self->write_event_.SetShutdown(status);
    } break;
    default:
      GPR_UNREACHABLE_CODE(return);
  }
}

CFStreamEndpoint::CFStreamEndpoint(std::shared_ptr<CFEventEngine> engine,
                                   MemoryAllocator memory_allocator)
    : engine_(std::move(engine)),
      memory_allocator_(std::move(memory_allocator)),
      open_event_(engine_.get()),
      read_event_(engine_.get()),
      write_event_(engine_.get()) {
  gpr_log(GPR_DEBUG, "CFStreamEndpoint(): %p", this);

  open_event_.InitEvent();
  read_event_.InitEvent();
  write_event_.InitEvent();
}

CFStreamEndpoint::~CFStreamEndpoint() {
  CFReadStreamClose(cf_read_stream_);
  CFWriteStreamClose(cf_write_stream_);

  open_event_.SetShutdown(absl::OkStatus());
  read_event_.SetShutdown(absl::OkStatus());
  write_event_.SetShutdown(absl::OkStatus());
  open_event_.DestroyEvent();
  read_event_.DestroyEvent();
  write_event_.DestroyEvent();
  gpr_log(GPR_DEBUG, "~CFStreamEndpoint(): %p", this);
}

void CFStreamEndpoint::Read(absl::AnyInvocable<void(absl::Status)> on_read,
                            SliceBuffer* buffer, const ReadArgs* args) {
  gpr_log(GPR_DEBUG, "CFStreamEndpoint::Read: read_hint_bytes: %ld",
          args ? args->read_hint_bytes : -1);

  gpr_log(GPR_DEBUG, "buffer: %p, count: %lu, length: %lu", buffer,
          buffer->Count(), buffer->Length());

  auto min_progress_size =
      (args && args->read_hint_bytes > 0) ? args->read_hint_bytes : 1;

  read_event_.NotifyOn(new PosixEngineClosure(
      [this, on_read = std::move(on_read), buffer,
       min_progress_size](absl::Status status) mutable {
        if (status.ok()) {
          DoRead(std::move(on_read), buffer);
        } else {
          on_read(status);
        }
      },
      false /* is_permanent*/));
}

void CFStreamEndpoint::DoRead(absl::AnyInvocable<void(absl::Status)> on_read,
                              SliceBuffer* buffer) {
  int buffer_size = 8192;

  gpr_log(GPR_DEBUG, "min_progress_size_: %d", 0 /*min_progress_size*/);

  auto buffer_index =
      buffer->AppendIndexed(Slice(memory_allocator_.MakeSlice(buffer_size)));

  CFIndex read_size = CFReadStreamRead(
      cf_read_stream_,
      internal::SliceCast<MutableSlice>(buffer->MutableSliceAt(buffer_index))
          .begin(),
      buffer_size);

  gpr_log(GPR_DEBUG, "CFReadStreamRead: read_size: %d, buffer_size: %d",
          read_size, buffer_size);

  if (read_size < 0) {
    auto status = CFErrorToStatus(CFReadStreamCopyError(cf_read_stream_));
    gpr_log(GPR_ERROR, "CFStream read error: %s", status.ToString().c_str());
    on_read(status);
    return;
  }

  buffer->RemoveLastNBytes(buffer->Length() - read_size);

  gpr_log(GPR_DEBUG, "called on_read");
  on_read(absl::OkStatus());
}

void CFStreamEndpoint::Write(absl::AnyInvocable<void(absl::Status)> on_writable,
                             SliceBuffer* data, const WriteArgs* args) {
  gpr_log(GPR_DEBUG, "CFStreamEndpoint::Write: max_frame_size: %d",
          args ? args->max_frame_size : -1);

  write_event_.NotifyOn(new PosixEngineClosure(
      [this, on_writable = std::move(on_writable),
       data](absl::Status status) mutable {
        if (status.ok()) {
          DoWrite(std::move(on_writable), data);
        } else {
          on_writable(status);
        }
      },
      false /* is_permanent*/));
}

void CFStreamEndpoint::DoWrite(
    absl::AnyInvocable<void(absl::Status)> on_writable, SliceBuffer* data) {
  for (int i = 0; i < data->Count(); i++) {
    auto slice = data->RefSlice(i);
    CFIndex written_size =
        CFWriteStreamWrite(cf_write_stream_, slice.begin(), slice.size());

    gpr_log(GPR_DEBUG,
            "CFStreamEndpoint::Write: wite_size: %d, written_size: %d",
            slice.size(), written_size);
  }
  on_writable(absl::OkStatus());
}

}  // namespace experimental
}  // namespace grpc_event_engine

#endif  // GPR_APPLE
