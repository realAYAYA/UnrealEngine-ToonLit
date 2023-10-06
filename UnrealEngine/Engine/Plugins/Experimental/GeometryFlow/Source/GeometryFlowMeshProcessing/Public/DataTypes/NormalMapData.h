// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "MeshProcessingNodes/MeshProcessingDataTypes.h"

#include "Image/ImageBuilder.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;

struct FNormalMapImage
{
	DECLARE_GEOMETRYFLOW_DATA_TYPE_IDENTIFIER(EMeshProcessingDataTypes::NormalMapImage);

	TImageBuilder<FVector3f> Image;
};


// declares FDataNormalMapImage, FNormalMapImageInput, FNormalMapImageOutput, FNormalMapImageSourceNode
GEOMETRYFLOW_DECLARE_BASIC_TYPES(NormalMapImage, FNormalMapImage, (int)EMeshProcessingDataTypes::NormalMapImage)





}	// end namespace GeometryFlow
}	// end namespace UE

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "GeometryFlowMovableData.h"
#endif
