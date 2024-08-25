// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerHelpers.h"
#include "ActorTreeItem.h"
#include "ActorDescTreeItem.h"
#include "ActorFolderTreeItem.h"
#include "EditorClassUtils.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "UObject/Package.h"

#include "SourceControlHelpers.h"

namespace SceneOutliner
{
	FString FSceneOutlinerHelpers::GetExternalPackageName(const ISceneOutlinerTreeItem& TreeItem)
	{
		if (const FActorTreeItem* ActorItem = TreeItem.CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				if (Actor->IsPackageExternal())
				{
					return Actor->GetExternalPackage()->GetName();
				}

			}
		}
		else if (const FActorFolderTreeItem* ActorFolderItem = TreeItem.CastTo<FActorFolderTreeItem>())
		{
			if (const UActorFolder* ActorFolder = ActorFolderItem->GetActorFolder())
			{
				if (ActorFolder->IsPackageExternal())
				{
					return ActorFolder->GetExternalPackage()->GetName();
				}
			}
		}
		else if (const FActorDescTreeItem* ActorDescItem = TreeItem.CastTo<FActorDescTreeItem>())
		{
			if (const FWorldPartitionActorDescInstance* ActorDescInstance = *ActorDescItem->ActorDescHandle)
			{
				return ActorDescInstance->GetActorPackage().ToString();
			}
		}

		return FString();
	}
	
	UPackage* FSceneOutlinerHelpers::GetExternalPackage(const ISceneOutlinerTreeItem& TreeItem)
	{
		if (const FActorTreeItem* ActorItem = TreeItem.CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				if (Actor->IsPackageExternal())
				{
					return Actor->GetExternalPackage();
				}
			}
		}
		else if (const FActorFolderTreeItem* ActorFolderItem = TreeItem.CastTo<FActorFolderTreeItem>())
		{
			if (const UActorFolder* ActorFolder = ActorFolderItem->GetActorFolder())
			{
				if (ActorFolder->IsPackageExternal())
				{
					return ActorFolder->GetExternalPackage();
				}
			}
		}
		else if (const FActorDescTreeItem* ActorDescItem = TreeItem.CastTo<FActorDescTreeItem>())
		{
			if (const FWorldPartitionActorDescInstance* ActorDescInstance = *ActorDescItem->ActorDescHandle)
			{
				return FindPackage(nullptr, *ActorDescInstance->GetActorPackage().ToString());
			}
		}

		return nullptr;
	}

TSharedPtr<SWidget> FSceneOutlinerHelpers::GetClassHyperlink(UObject* InObject)
	{
		if (InObject)
		{
			if (UClass* Class = InObject->GetClass())
			{
				// Always show blueprints
				const bool bIsBlueprintClass = UBlueprint::GetBlueprintFromClass(Class) != nullptr;

				// Also show game or game plugin native classes (but not engine classes as that makes the scene outliner pretty noisy)
				bool bIsGameClass = false;
				if (!bIsBlueprintClass)
				{
					UPackage* Package = Class->GetOutermost();
					const FString ModuleName = FPackageName::GetShortName(Package->GetFName());

					FModuleStatus PackageModuleStatus;
					if (FModuleManager::Get().QueryModule(*ModuleName, /*out*/ PackageModuleStatus))
					{
						bIsGameClass = PackageModuleStatus.bIsGameModule;
					}
				}

				if (bIsBlueprintClass || bIsGameClass)
				{
					FEditorClassUtils::FSourceLinkParams SourceLinkParams;
					SourceLinkParams.Object = InObject;
					SourceLinkParams.bUseDefaultFormat = true;

					return FEditorClassUtils::GetSourceLink(Class, SourceLinkParams);
				}
			}
		}

		return nullptr;
	}

;}