// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/TargetPlatformSettingsBase.h"
#include "AnalyticsEventAttribute.h"
#include "HAL/IConsoleManager.h"

bool FTargetPlatformSettingsBase::UsesForwardShading() const
{
	static IConsoleVariable* CVarForwardShading = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForwardShading"));
	return CVarForwardShading ? (CVarForwardShading->GetInt() != 0) : false;
}

bool FTargetPlatformSettingsBase::UsesDBuffer() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformSettingsBase::UsesBasePassVelocity() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VelocityOutputPass"));
	return CVar ? (CVar->GetInt() == 1) : false;
}

bool FTargetPlatformSettingsBase::VelocityEncodeDepth() const
{
	return true;
}

bool FTargetPlatformSettingsBase::UsesSelectiveBasePassOutputs() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SelectiveBasePassOutputs"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformSettingsBase::UsesDistanceFields() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFields"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformSettingsBase::UsesRayTracing() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

uint32 FTargetPlatformSettingsBase::GetSupportedHardwareMask() const
{
	return 0;
}

EOfflineBVHMode FTargetPlatformSettingsBase::GetStaticMeshOfflineBVHMode() const
{
	return EOfflineBVHMode::Disabled;
}

bool FTargetPlatformSettingsBase::GetStaticMeshOfflineBVHCompression() const
{
	return false;
}

bool FTargetPlatformSettingsBase::ForcesSimpleSkyDiffuse() const
{
	return false;
}

float FTargetPlatformSettingsBase::GetDownSampleMeshDistanceFieldDivider() const
{
	return 1.0f;
}

int32 FTargetPlatformSettingsBase::GetHeightFogModeForOpaque() const
{
	// Don't override the project setting by default
	// Platforms wish to support override need to implement the logic in their own target platform classes
	return 0;
}

bool FTargetPlatformSettingsBase::UsesMobileAmbientOcclusion() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.AmbientOcclusion"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformSettingsBase::UsesMobileDBuffer() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DBuffer"));
	return CVar ? (CVar->GetInt() != 0) : false;
}

bool FTargetPlatformSettingsBase::UsesASTCHDR() const
{
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("cook.ASTCTextureCompressor"));
	const bool bUsesARMCompressor = (CVar ? (CVar->GetInt() != 0) : false);

	static IConsoleVariable* CVarASTCHDR = IConsoleManager::Get().FindConsoleVariable(TEXT("cook.AllowASTCHDRProfile"));
	const bool bUsesASTCHDR = (CVarASTCHDR ? (CVarASTCHDR->GetInt() != 0) : false);

	return (bUsesARMCompressor && bUsesASTCHDR);
}

void FTargetPlatformSettingsBase::GetRayTracingShaderFormats(TArray<FName>& OutFormats) const
{
	if (UsesRayTracing())
	{
		GetAllTargetedShaderFormats(OutFormats);
	}
}