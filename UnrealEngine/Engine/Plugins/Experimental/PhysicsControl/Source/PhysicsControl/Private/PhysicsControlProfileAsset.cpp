// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileAsset.h"
#include "Engine/SkeletalMesh.h"

//#ifdef WITH_EDITOR
//#include "Editor.h"
//#endif // WITH_EDITOR


//======================================================================================================================
UPhysicsControlProfileAsset::UPhysicsControlProfileAsset()
{
	// This needs to be explored further - a possible way to hook into the BP compilation. However, we would also
	// need to make sure we don't compile (and get marked as dirty) if there are no changes.
//#ifdef WITH_EDITOR
//	GEditor->OnBlueprintCompiled().AddUObject(this, &UPhysicsControlProfileAsset::Compile);
//#endif
}

#if WITH_EDITOR
//======================================================================================================================
void UPhysicsControlProfileAsset::ShowCompiledData() const
{
	UE_LOG(LogTemp, Log, TEXT("Character setup data:"));
	for (const FPhysicsControlLimbSetupData& LimbSetupData : CharacterSetupData.LimbSetupData)
	{
		UE_LOG(LogTemp, Log, TEXT("Limb %s"), *LimbSetupData.LimbName.ToString());
		UE_LOG(LogTemp, Log, TEXT("  Start bone %s"), *LimbSetupData.StartBone.ToString());
		UE_LOG(LogTemp, Log, TEXT("  Include parent bone %d"), LimbSetupData.bIncludeParentBone);
		UE_LOG(LogTemp, Log, TEXT("  Create world space controls %d"), LimbSetupData.bCreateWorldSpaceControls);
		UE_LOG(LogTemp, Log, TEXT("  Create parent space controls %d"), LimbSetupData.bCreateParentSpaceControls);
		UE_LOG(LogTemp, Log, TEXT("  Create body modifiers %d"), LimbSetupData.bCreateBodyModifiers);
	}
	UE_LOG(LogTemp, Log, TEXT("Additional controls and modifiers:"));
	UE_LOG(LogTemp, Log, TEXT("  Additional controls:"));
	for (TMap<FName, FPhysicsControlCreationData>::ElementType ControlPair : AdditionalControlsAndModifiers.Controls)
	{
		UE_LOG(LogTemp, Log, TEXT("    %s:"), *ControlPair.Key.ToString());
		UE_LOG(LogTemp, Log, TEXT("      Parent bone %s:"), *ControlPair.Value.Control.ParentBoneName.ToString());
		UE_LOG(LogTemp, Log, TEXT("      Child bone %s:"), *ControlPair.Value.Control.ParentBoneName.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("  Additional modifiers:"));
	for (TMap<FName, FPhysicsBodyModifierCreationData>::ElementType ModifierPair : AdditionalControlsAndModifiers.Modifiers)
	{
		UE_LOG(LogTemp, Log, TEXT("    %s:"), *ModifierPair.Key.ToString());
		UE_LOG(LogTemp, Log, TEXT("      Bone %s:"), *ModifierPair.Value.Modifier.BoneName.ToString());
	}
	UE_LOG(LogTemp, Log, TEXT("Profiles:"));
	for (TMap<FName, FPhysicsControlControlAndModifierUpdates>::ElementType ProfilePair : Profiles)
	{
		UE_LOG(LogTemp, Log, TEXT("  %s:"), *ProfilePair.Key.ToString());
	}

}

//======================================================================================================================
void UPhysicsControlProfileAsset::Compile()
{
	CharacterSetupData = GetCharacterSetupData();
	AdditionalControlsAndModifiers = GetAdditionalControlsAndModifiers();
	AdditionalSets = GetAdditionalSets();
	InitialControlAndModifierUpdates = GetInitialControlAndModifierUpdates();
	Profiles = GetProfiles();

	Modify();
}

//======================================================================================================================
FPhysicsControlCharacterSetupData UPhysicsControlProfileAsset::GetCharacterSetupData() const
{
	FPhysicsControlCharacterSetupData CompiledCharacterSetupData;
	if (ParentAsset.LoadSynchronous())
	{
		CompiledCharacterSetupData = ParentAsset->GetCharacterSetupData();
	}
	CompiledCharacterSetupData += MyCharacterSetupData;
	return CompiledCharacterSetupData;
}

//======================================================================================================================
FPhysicsControlAndBodyModifierCreationDatas UPhysicsControlProfileAsset::GetAdditionalControlsAndModifiers() const
{
	FPhysicsControlAndBodyModifierCreationDatas CompiledAdditionalControlsAndModifiers;
	if (ParentAsset.LoadSynchronous())
	{
		CompiledAdditionalControlsAndModifiers = ParentAsset->GetAdditionalControlsAndModifiers();
	}
	// this will overwrite duplicates with our value
	CompiledAdditionalControlsAndModifiers += MyAdditionalControlsAndModifiers; 
	return CompiledAdditionalControlsAndModifiers;
}

//======================================================================================================================
FPhysicsControlSetUpdates UPhysicsControlProfileAsset::GetAdditionalSets() const
{
	FPhysicsControlSetUpdates CompiledAdditionalSets;
	if (ParentAsset.LoadSynchronous())
	{
		CompiledAdditionalSets = ParentAsset->GetAdditionalSets();
	}
	CompiledAdditionalSets += MyAdditionalSets;
	return CompiledAdditionalSets;
}

//======================================================================================================================
TArray<FPhysicsControlControlAndModifierUpdates> UPhysicsControlProfileAsset::GetInitialControlAndModifierUpdates() const
{
	TArray<FPhysicsControlControlAndModifierUpdates> CompiledInitialControlAndModifierUpdates;
	if (ParentAsset.LoadSynchronous())
	{
		CompiledInitialControlAndModifierUpdates = ParentAsset->GetInitialControlAndModifierUpdates();
	}
	CompiledInitialControlAndModifierUpdates.Append(MyInitialControlAndModifierUpdates);
	return CompiledInitialControlAndModifierUpdates;
}

//======================================================================================================================
TMap<FName, FPhysicsControlControlAndModifierUpdates> UPhysicsControlProfileAsset::GetProfiles() const
{
	TMap<FName, FPhysicsControlControlAndModifierUpdates> CompiledProfiles;
	if (ParentAsset.LoadSynchronous())
	{
		CompiledProfiles = ParentAsset->GetProfiles();
	}
	for (const TSoftObjectPtr<UPhysicsControlProfileAsset>& AdditionalProfileAsset : AdditionalProfileAssets)
	{
		if (AdditionalProfileAsset.LoadSynchronous())
		{
			CompiledProfiles.Append(AdditionalProfileAsset->GetProfiles());
		}
	}
	CompiledProfiles.Append(MyProfiles); // this will overwrite duplicates with our value
	return CompiledProfiles;
}

#endif

#if WITH_EDITOR
//======================================================================================================================
const FName UPhysicsControlProfileAsset::GetPreviewMeshPropertyName()
{
	return GET_MEMBER_NAME_STRING_CHECKED(UPhysicsControlProfileAsset, PreviewSkeletalMesh);
};
#endif

//======================================================================================================================
void UPhysicsControlProfileAsset::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty)
{
#if WITH_EDITOR
	PreviewSkeletalMesh = PreviewMesh;
#endif
}

//======================================================================================================================
USkeletalMesh* UPhysicsControlProfileAsset::GetPreviewMesh() const
{
#if WITH_EDITOR
	return PreviewSkeletalMesh.LoadSynchronous();
#else
	return nullptr;
#endif
}
