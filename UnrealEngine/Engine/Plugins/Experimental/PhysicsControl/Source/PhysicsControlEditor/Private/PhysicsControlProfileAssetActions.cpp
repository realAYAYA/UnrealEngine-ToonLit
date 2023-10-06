// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileAssetActions.h"
#include "PhysicsControlProfileAsset.h"
#include "PhysicsControlProfileEditorToolkit.h"

//======================================================================================================================
UClass* FPhysicsControlProfileAssetActions::GetSupportedClass() const
{
	return UPhysicsControlProfileAsset::StaticClass();
}

//======================================================================================================================
FText FPhysicsControlProfileAssetActions::GetName() const
{
	return INVTEXT("Physics Control Profile");
}

//======================================================================================================================
FColor FPhysicsControlProfileAssetActions::GetTypeColor() const
{
	// Match the "standard" physics color - they tend to be variations around this value
	return FColor(255, 192, 128);
}

//======================================================================================================================
uint32 FPhysicsControlProfileAssetActions::GetCategories()
{
	return EAssetTypeCategories::Physics;
}

//======================================================================================================================
void FPhysicsControlProfileAssetActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = 
		EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (UPhysicsControlProfileAsset* Asset = Cast<UPhysicsControlProfileAsset>(Object))
		{
			TSharedRef<FPhysicsControlProfileEditorToolkit> NewEditor(new FPhysicsControlProfileEditorToolkit());
			NewEditor->InitAssetEditor(Mode, EditWithinLevelEditor, Asset);
		}
	}
}
