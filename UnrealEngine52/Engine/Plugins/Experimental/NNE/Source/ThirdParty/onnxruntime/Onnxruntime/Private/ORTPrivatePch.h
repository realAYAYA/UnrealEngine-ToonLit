// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

THIRD_PARTY_INCLUDES_START

#include "onnx/defs/schema.h"
#include "onnx/defs/shape_inference.h"
#include "core/framework/data_types.h"
#include "core/framework/op_kernel.h"
#include "core/framework/tensor.h"
#include "core/providers/cpu/math/element_wise_ops.h"
#include "core/providers/cpu/tensor/utils.h"
#include "onnx/onnx_pb.h"

THIRD_PARTY_INCLUDES_END