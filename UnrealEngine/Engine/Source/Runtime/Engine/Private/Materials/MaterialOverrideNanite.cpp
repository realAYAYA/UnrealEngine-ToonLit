// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialOverrideNanite.h"

#include "Interfaces/ITargetPlatform.h"
#include "Materials/MaterialInterface.h"
#include "RenderUtils.h"
#include "UObject/UObjectThreadContext.h"


bool FMaterialOverrideNanite::CanUseOverride(EShaderPlatform ShaderPlatform) const
{
	return DoesPlatformSupportNanite(ShaderPlatform);
}

#if WITH_EDITOR

void FMaterialOverrideNanite::RefreshOverrideMaterial()
{
	// We don't resolve the soft pointer if we're cooking. 
	// Instead we defer any resolve to LoadOverrideForPlatform() which should be called in BeginCacheForCookedPlatformData().
	if (FApp::CanEverRender())
	{
		check(IsInGameThread());
		
		if (FUObjectThreadContext::Get().IsRoutingPostLoad && !OverrideMaterialRef.IsNull())
		{
			UE_LOG(LogMaterial, Warning, TEXT("Attempting to resolve NaniteOverrideMaterial '%s' during PostLoad()."), *OverrideMaterialRef.GetAssetName());
		}

		OverrideMaterial = bEnableOverride && !OverrideMaterialRef.IsNull() ? OverrideMaterialRef.LoadSynchronous() : nullptr;
	}
}

#endif // WITH_EDITOR

bool FMaterialOverrideNanite::Serialize(FArchive& Ar)
{
	{
		// Use non-collecting serialization scope for override material.
		// This prevents the cook from automatically seeing it, so that we can avoid cooking it on non-nanite platforms.
		FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::NeverCollect, ESoftObjectPathSerializeType::AlwaysSerialize);
		Ar << OverrideMaterialRef;
	}

	Ar << bEnableOverride;

	// We don't want the hard references somehow serializing to saved maps.
	// So we only serialize hard references when loading, or when cooking supported platforms.
	// Note that this approach won't be correct for a multi-platform cook with both nanite and non-nanite platforms.
	bool bSerializeOverrideObject = Ar.IsLoading();
#if WITH_EDITOR
	if (Ar.IsCooking())
	{
		TArray<FName> ShaderFormats;
		Ar.CookingTarget()->GetAllTargetedShaderFormats(ShaderFormats);
		for (FName ShaderFormat : ShaderFormats)
		{
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormat);
			if (CanUseOverride(ShaderPlatform))
			{
				bSerializeOverrideObject = true;
				break;
			}
		}
	}
#endif

	if (bSerializeOverrideObject)
	{
		Ar << OverrideMaterial;
	}
	else
	{
		TObjectPtr<UMaterialInterface> Dummy;
		Ar << Dummy;
	}

	return true;
}

void FMaterialOverrideNanite::PostLoad()
{
#if WITH_EDITOR
	// Don't call RefreshOverrideMaterial() directly because we can't SyncLoad during PostLoad phase.
	bIsRefreshRequested = true;
#endif
}

#if WITH_EDITOR

void FMaterialOverrideNanite::PostEditChange()
{
	RefreshOverrideMaterial();
}

void FMaterialOverrideNanite::LoadOverrideForPlatform(const ITargetPlatform* TargetPlatform)
{
	bool bCookOverrideObject = false;
	TArray<FName> ShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);
	for (FName ShaderFormat : ShaderFormats)
	{
		const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormat);
		if (CanUseOverride(ShaderPlatform))
		{
			bCookOverrideObject = true;
			break;
		}
	}

	if (bCookOverrideObject)
	{
		OverrideMaterial = bEnableOverride ? OverrideMaterialRef.LoadSynchronous() : nullptr;
	}
}

void FMaterialOverrideNanite::ClearOverride()
{
	OverrideMaterial = nullptr;
}

#endif // WITH_EDITOR
