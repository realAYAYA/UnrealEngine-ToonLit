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

#ifndef SOURCE_OPT_ANDROID_DRIVER_PATCH_
#define SOURCE_OPT_ANDROID_DRIVER_PATCH_

#include <unordered_map>

#include "source/opt/ir_context.h"
#include "source/opt/module.h"
#include "source/opt/pass.h"

namespace spvtools {
namespace opt {

// This pass tries to fix validation error due to a mismatch of storage classes
// in instructions.  There is no guarantee that all such error will be fixed,
// and it is possible that in fixing these errors, it could lead to other
// errors.
class AndroidDriverPatchPass : public Pass {
 public:
  const char* name() const override { return "android-driver-patch-pass"; }
  Status Process() override;

 private:
  // Workaround for a bug on Adreno Vulkan drivers where OpPhi is not processed correctly in the driver 
  // Instead we deconstruct the matrix into components and individually call OpPhi for each vector component
  // Finally we reconstruct the matrix
  bool FixupOpPhiMatrix4x3(Instruction* inst, Instruction* entryPoint);

  // Workaround for a bug on Adreno Vulkan drivers where vector4 shuffles into into a vector3 generates invalid
  // code in the driver. We fix this by breaking the vector into float components and then composing to the final float
  bool FixupOpVectorShuffle(Instruction* inst);

  // Workaround for a bug on Adreno Vulkan drivers where passing relaxed precision with the function
  // declaration causes a driver error
  bool FixupOpVariableFunctionPrecision(Instruction* inst);

  // Workaround for a bug on Adreno Vulkan drivers where passing setting 
  // Depth=2 on OpTypeImage results in a crash
  bool FixupOpTypeImage(Instruction* inst);

 private:
  bool HasRelaxedPrecision(uint32_t operand_id);
  void AddRelaxedPrecision(uint32_t operand_id);
  bool RemoveRelaxedPrecision(uint32_t operand_id);
};

}  // namespace opt
}  // namespace spvtools

#endif  // SOURCE_OPT_ANDROID_DRIVER_PATCH_
