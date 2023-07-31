// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Resource/TextureShareResourceSyncStep.h"

/**
 * Resource descriptor constructor
 */
struct FTextureShareResourceDesc
	: public FTextureShareCoreResourceDesc
{
public:
	FTextureShareResourceDesc(const wchar_t* InResourceName, const ETextureShareTextureOp InOperationType = ETextureShareTextureOp::Read, const ETextureShareEyeType InEyeType = ETextureShareEyeType::Default)
		: FTextureShareCoreResourceDesc(
			InResourceName,
			InEyeType,
			InOperationType,
			TextureShareResourceSyncStepHelper::GeSyncStep(InResourceName, InOperationType))
	{ }
};
