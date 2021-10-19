/*
 *
 * Copyright 2021 gRPC authors.
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

#import <XCTest/XCTest.h>

#include <absl/functional/bind_front.h>
#include <absl/time/time.h>

#include <grpc/event_engine/event_engine.h>
#include <grpc/grpc.h>
#include <grpc/test/core/util/test_config.h>

#include "src/core/lib/event_engine/uv/libuv_event_engine.h"
#include "test/core/event_engine/test_suite/event_engine_test.h"

@interface EventEngineTimerTests : XCTestCase

@end

@implementation EventEngineTimerTests

+ (void)setUp {
  grpc_init();
  testing::InitGoogleTest();

  SetEventEngineFactory(
      []() { return absl::make_unique<grpc_event_engine::experimental::LibuvEventEngine>(); });
}

+ (void)tearDown {
  grpc_shutdown();
}

- (void)setUp {
}

- (void)tearDown {
}

- (void)testAll {
  auto gtest_result = RUN_ALL_TESTS();
  XCTAssertEqual(gtest_result, 0);
}

@end
