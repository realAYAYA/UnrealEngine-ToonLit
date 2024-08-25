// Copyright Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialOverrideNanite.h"

#include "Interfaces/ITargetPlatform.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "RenderUtils.h"
#include "UObject/FortniteReleaseBranchCustomObjectVersion.h"
#include "UObject/UObjectBaseUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MaterialOverrideNanite)

bool FMaterialOverrideNanite::FixupLegacySoftReference(UObject* OptionalOwner)
{
#if WITH_EDITOR

	if (OverrideMaterialRef.IsNull())
	{
		return false;
	}

	// Need to resolve and load the soft ref here before we can deprecate it.
	OverrideMaterialEditor = OverrideMaterialRef.LoadSynchronous();
	if (!OverrideMaterialEditor)
	{
		UE_LOG(LogMaterial, Warning, TEXT("MaterialOverrideNanite with owner '%s' has a legacy override material reference (%s) but it could not be loaded."), OptionalOwner ? *OptionalOwner->GetPathName() : TEXT("UNKNOWN"), *OverrideMaterialRef.ToString());
		return false;
	}

	// When we are routing PostLoad, LoadSynchronous can return a valid object pointer when the asset has not loaded.
	// We can still store the TObjectPtr, but we need to ensure that the material re-inits on completion of the async load.
	if (OverrideMaterialEditor->HasAnyFlags(RF_NeedLoad))
	{
		const FString LongPackageName = OverrideMaterialRef.GetLongPackageName();
		UE_LOG(LogMaterial, Display, TEXT("Async loading NaniteOverrideMaterial '%s' for owner '%s'."), *LongPackageName, OptionalOwner ? *OptionalOwner->GetPathName() : TEXT("UNKNOWN"));
		LoadPackageAsync(LongPackageName, FLoadPackageAsyncDelegate::CreateLambda(
			[WeakOverrideMaterial = MakeWeakObjectPtr(OverrideMaterialEditor)](const FName&, UPackage*, EAsyncLoadingResult::Type)
			{
				if (UMaterialInterface* Material = WeakOverrideMaterial.Get())
				{
					// Use a MaterialUpdateContext to make sure dependent interfaces (e.g. MIDs) update as well
					FMaterialUpdateContext UpdateContext;
					UpdateContext.AddMaterialInterface(Material);

					Material->ForceRecompileForRendering();
				}
			}));
	}
	else
	{
		// Even if RF_NeedLoad is not there, it doesn't mean the object is fully postloaded
		OverrideMaterialEditor->ConditionalPostLoad();
	}

	// Reset the deprecated ref.
	OverrideMaterialRef.Reset();
	return true;

#else
	return false;
#endif // WITH_EDITOR
}

bool FMaterialOverrideNanite::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFortniteReleaseBranchCustomObjectVersion::GUID);
	
	const bool bEditorOnlyCook = Ar.CustomVer(FFortniteReleaseBranchCustomObjectVersion::GUID) >= FFortniteReleaseBranchCustomObjectVersion::NaniteMaterialOverrideUsesEditorOnly;

	// Path for loading legacy data.
	if (!bEditorOnlyCook)
	{
		Ar << OverrideMaterialRef;
		Ar << bEnableOverride;
		Ar << OverrideMaterial;
		// No default property serialization in this path.
		return true;
	}

	// We don't want to serialize OverrideMaterial to non-nanite platforms.
	// This reduces the cooked cost of the material and, just as importantly, its texture dependencies.
	bool bCookOverrideMaterial = false;

#if WITH_EDITOR
	if (Ar.IsCooking())
	{
		TArray<FName> ShaderFormats;
		Ar.CookingTarget()->GetAllTargetedShaderFormats(ShaderFormats);
		for (FName ShaderFormat : ShaderFormats)
		{
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormat);
			if (DoesPlatformSupportNanite(ShaderPlatform))
			{
				bCookOverrideMaterial = true;
				break;
			}
		}
	}
#endif

	bool bSerializeAsCookedData = Ar.IsCooking();
	Ar << bSerializeAsCookedData;

	if (bSerializeAsCookedData)
	{
		// In cooked data we serialize a UObject* here in the native serializer to hold the OverrideMaterial.
		if (Ar.IsSaving())
		{
#if WITH_EDITORONLY_DATA
			// Use this->OverrideMaterialEditor.
			// If we are choosing not to cook because of no nanite platform support then serialize nullptr.
			UObject* SavedOverrideMaterial = bCookOverrideMaterial ? OverrideMaterialEditor : nullptr;
#else
			UObject* SavedOverrideMaterial = OverrideMaterial;
#endif
			Ar << SavedOverrideMaterial;
		}
		else
		{
#if WITH_EDITORONLY_DATA
			// If we are loading a cooked package in an environment that has EditorOnlyData, load the OverrideMaterial into OverrideMaterialEditor.
			// That is the variable we read/write when WITH_EDITORONLY_DATA.
			Ar << OverrideMaterialEditor;
#else
			Ar << OverrideMaterial;
#endif
		}
	}
	else
	{
		// In non-cooked data we do not serialize a UObject* here.
		// The OverrideMaterial is stored in this->OverrideMaterialEditor, which is serialized through default property serialization.
	}

	// Continue serialization of other properties.
	return false;
}

void FMaterialOverrideNanite::SetOverrideMaterial(UMaterialInterface* InMaterial, bool bInOverride)
{
#if WITH_EDITORONLY_DATA
	OverrideMaterialEditor = InMaterial;
#else
	OverrideMaterial = InMaterial;
#endif
	bEnableOverride = bInOverride;
}
