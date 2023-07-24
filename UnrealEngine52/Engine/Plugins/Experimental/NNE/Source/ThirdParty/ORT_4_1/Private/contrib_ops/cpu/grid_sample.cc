// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "ThirdPartyWarningDisabler.h" // WITH_UE: Avoids cryptic protobuf/stubs/strutil.h(364): warning C4127: conditional expression is constant; note: consider using 'if constexpr' statement instead
NNI_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/providers/cpu/tensor/grid_sample.h"
#include "core/providers/common.h"
NNI_THIRD_PARTY_INCLUDES_END // WITH_UE

namespace onnxruntime {
namespace contrib {

#define REGISTER_KERNEL_TYPED(T)                                    \
  ONNX_OPERATOR_TYPED_KERNEL_EX(                                    \
      GridSample,                                                   \
      kMSDomain,                                                    \
      1,                                                            \
      T,                                                            \
      kCpuExecutionProvider,                                        \
      KernelDefBuilder()                                            \
          .TypeConstraint("T1", DataTypeImpl::GetTensorType<T>())   \
          .TypeConstraint("T2", DataTypeImpl::GetTensorType<T>()),  \
      GridSample<T>);

REGISTER_KERNEL_TYPED(float)

}  // namespace contrib
}  // namespace onnxruntime
