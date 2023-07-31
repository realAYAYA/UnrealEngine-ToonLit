// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfiguratorAssetTypeActions.h"

#include "DisplayClusterConfiguratorBlueprintEditor.h"
#include "DisplayClusterConfiguratorUtils.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfiguratorFactory.h"

#include "DisplayClusterRootActor.h"
#include "Blueprints/DisplayClusterBlueprint.h"

#include "Subsystems/AssetEditorSubsystem.h"

UClass* FDisplayClusterConfiguratorAssetTypeActions::GetSupportedClass() const
{
	return UDisplayClusterBlueprint::StaticClass();
}

void FDisplayClusterConfiguratorAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (TArray<UObject*>::TConstIterator ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UDisplayClusterBlueprint* BP = Cast<UDisplayClusterBlueprint>(*ObjIt))
		{
			if (BP->bIsNewlyCreated && BP->GetConfig() == nullptr)
			{
				// This path can be hit if the BP was created by a default factory.
				UDisplayClusterConfiguratorFactory::SetupNewBlueprint(BP);
			}
			
			TSharedRef<FDisplayClusterConfiguratorBlueprintEditor> BlueprintEditor(new FDisplayClusterConfiguratorBlueprintEditor());
			BlueprintEditor->InitDisplayClusterBlueprintEditor(Mode, EditWithinLevelEditor, BP);
		}
	}
}

void FDisplayClusterConfiguratorAssetTypeActions::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (const UObject* Asset : TypeAssets)
	{
		if (const UDisplayClusterBlueprint* EditingObject = Cast<UDisplayClusterBlueprint>(Asset))
		{
			if (UDisplayClusterConfigurationData* ConfigData = EditingObject->GetConfig())
			{
				const FString& Path = ConfigData->ImportedPath;
				if (!Path.IsEmpty())
				{
					OutSourceFilePaths.Add(Path);
				}
			}
		}
	}
}

UClass* FDisplayClusterConfiguratorActorAssetTypeActions::GetSupportedClass() const
{
	return ADisplayClusterRootActor::StaticClass();
}
