// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CapsuleShadowRendering.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"

extern int32 GCapsuleShadows;
extern int32 GCapsuleDirectShadows;
extern int32 GCapsuleIndirectShadows;

inline bool SupportsCapsuleShadows(FStaticShaderPlatform ShaderPlatform)
{
	return GCapsuleShadows
		&& FDataDrivenShaderPlatformInfo::GetSupportsCapsuleShadows(ShaderPlatform);
}

inline bool SupportsCapsuleDirectShadows(FStaticShaderPlatform ShaderPlatform)
{
	return GCapsuleDirectShadows && SupportsCapsuleShadows(ShaderPlatform);
}

inline bool SupportsCapsuleIndirectShadows(FStaticShaderPlatform ShaderPlatform)
{
	return GCapsuleIndirectShadows && SupportsCapsuleShadows(ShaderPlatform);
}
