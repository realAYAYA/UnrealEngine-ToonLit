// Copyright (c) 2019 Google LLC
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

#ifndef SOURCE_OPT_REDUCE_CONST_ARRAY_TO_STRUCT_PASS_
#define SOURCE_OPT_REDUCE_CONST_ARRAY_TO_STRUCT_PASS_

#include <unordered_map>

#include "source/opt/ir_context.h"
#include "source/opt/module.h"
#include "source/opt/pass.h"

namespace spvtools {
namespace opt {

// This pass attempts to reduce array with constant access to structs to minimize size of CPU to GPU transfer
class ReduceConstArrayToStructPass : public Pass {
 public:
  const char* name() const override { return "reduce-const-array-to-struct"; }
  Status Process() override;

 private:
  bool ReduceArray(Instruction* inst);
};

}  // namespace opt
}  // namespace spvtools

#endif  // SOURCE_OPT_ANDROID_DRIVER_PATCH_
