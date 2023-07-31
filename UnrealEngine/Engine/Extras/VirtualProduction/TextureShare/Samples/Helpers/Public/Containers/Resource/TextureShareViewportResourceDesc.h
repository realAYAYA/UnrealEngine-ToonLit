// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Resource/TextureShareResourceSyncStep.h"
#include "Containers/Resource/TextureShareCustomResource.h"

/**
 * nDisplay viewport resource desc constructor
 */
struct FTextureShareViewportResourceDesc
	: public FTextureShareCoreResourceDesc
{
public:
	FTextureShareViewportResourceDesc(const wchar_t* InViewportId, const wchar_t* InResourceName, const ETextureShareTextureOp InOperationType = ETextureShareTextureOp::Read, const ETextureShareEyeType InEyeType = ETextureShareEyeType::Default)
		: FTextureShareCoreResourceDesc(
			InResourceName,
			FTextureShareCoreViewDesc(InViewportId, InEyeType),
			InOperationType,
			TextureShareResourceSyncStepHelper::GetDisplayClusterSyncStep(InResourceName, InOperationType))
	{ }
};
