// Copyright 2022 The SiliFuzz Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "./common/raw_insns_util.h"

#include <stdint.h>

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "./proto/snapshot.pb.h"
#include "./util/testing/status_macros.h"
#include "./util/testing/status_matchers.h"

namespace silifuzz {
namespace {

using silifuzz::testing::StatusIs;

TEST(RawInsnsUtil, InstructionsToSnapshot_X86_64) {
  auto config = DEFAULT_FUZZING_CONFIG<X86_64>;
  absl::StatusOr<Snapshot> snapshot =
      InstructionsToSnapshot<X86_64>("\xCC", config);
  ASSERT_OK(snapshot);
  // data page + code page
  EXPECT_EQ(snapshot->num_pages(), 2);
  // must be executable
  EXPECT_OK(snapshot->IsComplete(Snapshot::kUndefinedEndState));

  uint64_t rip = snapshot->ExtractRip(snapshot->registers());
  EXPECT_GE(rip, config.code_range.start_address);
  EXPECT_LT(rip, config.code_range.start_address + config.code_range.num_bytes);
}

TEST(RawInsnsUtil, InstructionsToSnapshot_X86_64_Stable) {
  absl::StatusOr<Snapshot> snapshot_2 = InstructionsToSnapshot<X86_64>("\xAA");
  ASSERT_OK(snapshot_2);

  absl::StatusOr<Snapshot> snapshot_3 = InstructionsToSnapshot<X86_64>("\xAA");
  ASSERT_OK(snapshot_3);
  EXPECT_EQ(snapshot_2->ExtractRip(snapshot_2->registers()),
            snapshot_3->ExtractRip(snapshot_3->registers()));
}

TEST(RawInsnsUtil, InstructionsToSnapshotId) {
  EXPECT_EQ(InstructionsToSnapshotId("Silifuzz"),
            "679016f223a6925ba69f055f513ea8aa0e0720ed");
}

TEST(RawInsnsUtil, InstructionsToSnapshot_AArch64) {
  auto config = DEFAULT_FUZZING_CONFIG<AArch64>;
  // nop
  std::string instruction({0x1f, 0x20, 0x03, 0xd5});
  absl::StatusOr<Snapshot> snapshot =
      InstructionsToSnapshot<AArch64>(instruction, config);
  ASSERT_OK(snapshot);
  // code page + stack page
  EXPECT_EQ(snapshot->num_pages(), 2);
  // must be executable
  EXPECT_OK(snapshot->IsComplete(Snapshot::kUndefinedEndState));

  uint64_t pc = snapshot->ExtractRip(snapshot->registers());
  EXPECT_GE(pc, config.code_range.start_address);
  EXPECT_LT(pc, config.code_range.start_address + config.code_range.num_bytes);
}

TEST(RawInsnsUtil, InstructionsToSnapshot_AArch64_Stable) {
  std::string instruction({0x0, 0xc0, 0xb0, 0x72});
  absl::StatusOr<Snapshot> snapshot_2 =
      InstructionsToSnapshot<AArch64>(instruction);
  ASSERT_OK(snapshot_2);

  absl::StatusOr<Snapshot> snapshot_3 =
      InstructionsToSnapshot<AArch64>(instruction);
  ASSERT_OK(snapshot_3);
  EXPECT_EQ(snapshot_2->ExtractRip(snapshot_2->registers()),
            snapshot_3->ExtractRip(snapshot_3->registers()));
}

TEST(RawInsnsUtil, InstructionsToSnapshot_AArch64_Filter) {
  // sqdecb    x11, vl8, mul #16
  std::string sve_insn({0x0b, 0xf9, 0x3f, 0x04});
  // ldumax   w5, w1, [x7]
  std::string load_insn({0xe1, 0x60, 0x25, 0xb8});
  // ld1d   z0.d, p0/z, [x0]
  std::string load_sve_insn({0x0, 0xa0, 0xe0, 0xa5});

  auto config = DEFAULT_FUZZING_CONFIG<AArch64>;
  config.sve_instructions_allowed = false;
  config.load_store_instructions_allowed = false;
  EXPECT_THAT(InstructionsToSnapshot<AArch64>(sve_insn, config),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(InstructionsToSnapshot<AArch64>(load_insn, config),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(InstructionsToSnapshot<AArch64>(load_sve_insn, config),
              StatusIs(absl::StatusCode::kInvalidArgument));

  config.load_store_instructions_allowed = true;
  EXPECT_THAT(InstructionsToSnapshot<AArch64>(sve_insn, config),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_OK(InstructionsToSnapshot<AArch64>(load_insn, config));
  EXPECT_THAT(InstructionsToSnapshot<AArch64>(load_sve_insn, config),
              StatusIs(absl::StatusCode::kInvalidArgument));

  config.sve_instructions_allowed = true;
  EXPECT_OK(InstructionsToSnapshot<AArch64>(sve_insn, config));
  EXPECT_OK(InstructionsToSnapshot<AArch64>(load_insn, config));
  EXPECT_OK(InstructionsToSnapshot<AArch64>(load_sve_insn, config));

  config.load_store_instructions_allowed = false;
  EXPECT_OK(InstructionsToSnapshot<AArch64>(sve_insn, config));
  EXPECT_THAT(InstructionsToSnapshot<AArch64>(load_insn, config),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(InstructionsToSnapshot<AArch64>(load_sve_insn, config),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace silifuzz
