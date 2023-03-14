// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TextureRenderTarget2D.h"

namespace NiagaraDataInterfaceRenderTargetCommon
{
	extern int32 GReleaseResourceOnRemove;
	extern int32 GIgnoreCookedOut;
	extern float GResolutionMultiplier;
	extern int GAllowReads;

	extern bool GetRenderTargetFormat(bool bOverrideFormat, ETextureRenderTargetFormat OverrideFormat, ETextureRenderTargetFormat& OutRenderTargetFormat);
}
