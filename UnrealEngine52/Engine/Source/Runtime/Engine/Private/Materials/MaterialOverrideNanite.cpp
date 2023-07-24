// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialOverrideNanite.h"

#include "Interfaces/ITargetPlatform.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "RenderUtils.h"
#include "UObject/UObjectThreadContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialOverrideNanite)


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
		
		OverrideMaterial = bEnableOverride && !OverrideMaterialRef.IsNull() ? OverrideMaterialRef.LoadSynchronous() : nullptr;

		// When we are routing PostLoad, LoadSynchronous can return a valid object pointer when the asset has not loaded.
		// We can still store the TObjectPtr, but we need to ensure that the material re-inits on completion of the async load.
		if (OverrideMaterial && OverrideMaterial->HasAnyFlags(RF_NeedLoad))
		{ 
			const FString LongPackageName = OverrideMaterialRef.GetLongPackageName();
			UE_LOG(LogMaterial, Display, TEXT("Async loading NaniteOverrideMaterial '%s'"), *LongPackageName);
			LoadPackageAsync(LongPackageName, FLoadPackageAsyncDelegate::CreateLambda(
				[WeakOverrideMaterial = MakeWeakObjectPtr(OverrideMaterial)](const FName&, UPackage*, EAsyncLoadingResult::Type)
				{
					if (UMaterialInterface* Material = WeakOverrideMaterial.Get())
					{
						Material->ForceRecompileForRendering();
					}
				}));
		}
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

void FMaterialOverrideNanite::InitUnsafe(UMaterialInterface* InMaterial)
{
	OverrideMaterialRef = InMaterial;
	OverrideMaterial = InMaterial;
	bEnableOverride = true;
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
