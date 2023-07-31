// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Application/SlateApplication.h"
#include "SSceneOutliner.h"

#include "SceneOutlinerActorInfoColumn.h"
#include "SceneOutlinerGutter.h"
#include "SceneOutlinerItemLabelColumn.h"
#include "SceneOutlinerActorSCCColumn.h"
#include "SceneOutlinerPinnedColumn.h"
#include "SceneOutlinerTextInfoColumn.h"
#include "SceneOutlinerPublicTypes.h"

#include "ActorPickingMode.h"
#include "ActorBrowsingMode.h"
#include "ActorTreeItem.h"
#include "ActorFolderTreeItem.h"
#include "ComponentTreeItem.h"
#include "ActorDescTreeItem.h"
#include "FolderTreeItem.h"
#include "WorldTreeItem.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerModule"

/* FSceneOutlinerModule interface
 *****************************************************************************/

FSceneOutlinerModule::FSceneOutlinerModule()
: ColumnPermissionList(MakeShareable(new FNamePermissionList()))
{
	ColumnPermissionList->OnFilterChanged().AddLambda([this]() { ColumnPermissionListChanged.Broadcast(); });
}

void FSceneOutlinerModule::StartupModule()
{
	RegisterDefaultColumnType< FSceneOutlinerItemLabelColumn >(FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10, FCreateSceneOutlinerColumn(), false, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));

	// Register builtin column types which are not active by default
	RegisterColumnType<FSceneOutlinerGutter>();
	RegisterColumnType<FTypeInfoColumn>();
	RegisterColumnType<FSceneOutlinerActorSCCColumn>();
	RegisterColumnType<FSceneOutlinerPinnedColumn>();
}


void FSceneOutlinerModule::ShutdownModule()
{
	UnRegisterColumnType<FSceneOutlinerGutter>();
	UnRegisterColumnType<FSceneOutlinerItemLabelColumn>();
	UnRegisterColumnType<FTypeInfoColumn>();
	UnRegisterColumnType<FSceneOutlinerActorSCCColumn>();
	UnRegisterColumnType<FSceneOutlinerPinnedColumn>();
}

TSharedRef<ISceneOutliner> FSceneOutlinerModule::CreateSceneOutliner(const FSceneOutlinerInitializationOptions& InitOptions) const
{
	return SNew(SSceneOutliner, InitOptions)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}

TSharedRef<ISceneOutliner> FSceneOutlinerModule::CreateActorPicker(const FSceneOutlinerInitializationOptions& InInitOptions, const FOnActorPicked& OnActorPickedDelegate, TWeakObjectPtr<UWorld> SpecifiedWorld) const
{
	auto OnItemPicked = FOnSceneOutlinerItemPicked::CreateLambda([OnActorPickedDelegate](TSharedRef<ISceneOutlinerTreeItem> Item)
		{
			if (FActorTreeItem* ActorItem = Item->CastTo<FActorTreeItem>())
			{
				if (ActorItem->IsValid())
				{
					OnActorPickedDelegate.ExecuteIfBound(ActorItem->Actor.Get());

				}
			}
		});

	FCreateSceneOutlinerMode ModeFactory = FCreateSceneOutlinerMode::CreateLambda([&OnItemPicked, &SpecifiedWorld](SSceneOutliner* Outliner)
		{
			FActorModeParams Params;
			Params.SceneOutliner = Outliner;
			Params.SpecifiedWorldToDisplay = SpecifiedWorld;
			Params.bHideComponents = true;
			Params.bHideLevelInstanceHierarchy = true;
			Params.bHideUnloadedActors = true;
			Params.bHideEmptyFolders = true;
			return new FActorPickingMode(Params, OnItemPicked);
		});

	FSceneOutlinerInitializationOptions InitOptions(InInitOptions);
	InitOptions.ModeFactory = ModeFactory;
	if (InitOptions.ColumnMap.Num() == 0)
	{
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), false, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Pinned(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Invisible, 5, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Pinned_Localized()));
		CreateActorInfoColumns(InitOptions);
	}
	return CreateSceneOutliner(InitOptions);
}

TSharedRef<ISceneOutliner> FSceneOutlinerModule::CreateComponentPicker(const FSceneOutlinerInitializationOptions& InInitOptions, const FOnComponentPicked& OnComponentPickedDelegate, TWeakObjectPtr<UWorld> SpecifiedWorld) const
{
	auto OnItemPicked = FOnSceneOutlinerItemPicked::CreateLambda([OnComponentPickedDelegate](TSharedRef<ISceneOutlinerTreeItem> Item)
		{
			if (FComponentTreeItem* ComponentItem = Item->CastTo<FComponentTreeItem>())
			{
				if (ComponentItem->IsValid())
				{
					OnComponentPickedDelegate.ExecuteIfBound(ComponentItem->Component.Get());
				}
			}
		});

	FCreateSceneOutlinerMode ModeFactory = FCreateSceneOutlinerMode::CreateLambda([&OnItemPicked, &SpecifiedWorld](SSceneOutliner* Outliner)
		{
			FActorModeParams Params;
			Params.SceneOutliner = Outliner;
			Params.SpecifiedWorldToDisplay = SpecifiedWorld;
			Params.bHideComponents = false;
			Params.bHideActorWithNoComponent = true;
			Params.bHideLevelInstanceHierarchy = true;
			Params.bHideUnloadedActors = true;
			Params.bHideEmptyFolders = true;
			return new FActorPickingMode(Params, OnItemPicked);
		});

	FSceneOutlinerInitializationOptions InitOptions(InInitOptions);
	InitOptions.ModeFactory = ModeFactory;
	if (InitOptions.ColumnMap.Num() == 0)
	{
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), false, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Label_Localized()));
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 10, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
		CreateActorInfoColumns(InitOptions);
	}
	return CreateSceneOutliner(InitOptions);
}

TSharedRef< ISceneOutliner > FSceneOutlinerModule::CreateActorBrowser(const FSceneOutlinerInitializationOptions& InInitOptions, TWeakObjectPtr<UWorld> SpecifiedWorld) const
{
	FCreateSceneOutlinerMode ModeFactory = FCreateSceneOutlinerMode::CreateLambda([&SpecifiedWorld](SSceneOutliner* Outliner)
		{
			return new FActorBrowsingMode(Outliner, SpecifiedWorld);
		});

	FSceneOutlinerInitializationOptions InitOptions(InInitOptions);
	InitOptions.ModeFactory = ModeFactory;

	if (InitOptions.ColumnMap.Num() == 0)
	{
		InitOptions.UseDefaultColumns();
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Gutter(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Gutter_Localized()));
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::ActorInfo(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 20, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::ActorInfo_Localized()));
		InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::SourceControl(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Invisible, 30, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::SourceControl_Localized()));

		// TODO: Figure out a better way to get the current world
		UWorld* WorldPtr = GEditor->GetEditorWorldContext().World();

		if (WorldPtr)
		{
			// We don't want the pinned column in non wp levels
			if (WorldPtr->IsPartitionedWorld())
			{
				InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Pinned(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 5, FCreateSceneOutlinerColumn(), true, TOptional<float>(), FSceneOutlinerBuiltInColumnTypes::Pinned_Localized()));
			}
			CreateActorInfoColumns(InitOptions, WorldPtr);
		}
		
	}

	return CreateSceneOutliner(InitOptions);
}

void FSceneOutlinerModule::CreateActorInfoColumns(FSceneOutlinerInitializationOptions& InInitOptions, UWorld *WorldPtr) const
{
	FGetTextForItem MobilityInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
		{
			AActor* Actor = ActorItem->Actor.Get();
			if (!Actor)
			{
				return FString();
			}

			FString Result;
			USceneComponent* RootComponent = Actor->GetRootComponent();
			if (RootComponent)
			{
				if (RootComponent->Mobility == EComponentMobility::Static)
				{
					Result = FString(TEXT("Static"));
				}
				if (RootComponent->Mobility == EComponentMobility::Stationary)
				{
					Result = FString(TEXT("Stationary"));
				}
				else if (RootComponent->Mobility == EComponentMobility::Movable)
				{
					Result = FString(TEXT("Movable"));
				}
			}
			return Result;
		}
		return FString();
	});

	FGetTextForItem LayerInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		FString Result;
		
		if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
		{
			AActor* Actor = ActorItem->Actor.Get();
			if (!Actor)
			{
				return FString();
			}

			for (const auto& Layer : Actor->Layers)
			{
				if (Result.Len())
				{
					Result += TEXT(", ");
				}

				Result += Layer.ToString();
			}
		}

		return Result;
	});

	FGetTextForItem DataLayerInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		TStringBuilder<128> Builder;
		TSet<FString> DataLayerShortNames;

		auto BuildDataLayers = [&Builder, &DataLayerShortNames](const auto& DataLayerInstances, bool bPartOfOtherLevel)
		{
			for (const UDataLayerInstance* DataLayerInstance : DataLayerInstances)
			{
				bool bIsAlreadyInSet = false;
				DataLayerShortNames.Add(DataLayerInstance->GetDataLayerShortName(), &bIsAlreadyInSet);
				if (!bIsAlreadyInSet)
				{
					if (Builder.Len())
					{
						Builder += TEXT(", ");
					}
					// Put a '*' in front of DataLayers that are not part of of the main world
					if (bPartOfOtherLevel)
					{
						Builder += "*";
					}
					Builder += DataLayerInstance->GetDataLayerShortName();
				}
			}
		};

		auto BuildDataLayersWithContext = [BuildDataLayers](const ISceneOutlinerTreeItem& Item, bool bUseLevelContext)
		{
			if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
			{
				if (AActor* Actor = ActorItem->Actor.Get())
				{
					BuildDataLayers(bUseLevelContext ? Actor->GetDataLayerInstancesForLevel() : Actor->GetDataLayerInstances(), bUseLevelContext);
				}
			}
			else if (const FActorDescTreeItem* ActorDescItem = Item.CastTo<FActorDescTreeItem>())
			{
				if (const FWorldPartitionActorDesc* ActorDesc = ActorDescItem->ActorDescHandle.Get(); ActorDesc && !ActorDesc->GetDataLayerInstanceNames().IsEmpty())
				{
					if (const UActorDescContainer* ActorDescContainer = ActorDescItem->ActorDescHandle.Container.Get())
					{
						const UWorld* World = ActorDescContainer->GetWorld();
						if (const UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(World))
						{
							TSet<const UDataLayerInstance*> DataLayerInstances;
							DataLayerInstances.Append(DataLayerSubsystem->GetDataLayerInstances(ActorDesc->GetDataLayerInstanceNames()));
							if (ULevelInstanceSubsystem* LevelInstanceSubsystem = UWorld::GetSubsystem<ULevelInstanceSubsystem>(World))
							{
								UWorld* OuterWorld = ActorDescContainer->GetTypedOuter<UWorld>();
								// Add parent container Data Layer Instances
								AActor* CurrentActor = OuterWorld ? Cast<AActor>(LevelInstanceSubsystem->GetOwningLevelInstance(OuterWorld->PersistentLevel)) : nullptr;
								while (CurrentActor)
								{
									DataLayerInstances.Append(bUseLevelContext ? CurrentActor->GetDataLayerInstancesForLevel() : CurrentActor->GetDataLayerInstances());
									CurrentActor = Cast<AActor>(LevelInstanceSubsystem->GetParentLevelInstance(CurrentActor));
								};
							}
							BuildDataLayers(DataLayerInstances, bUseLevelContext);
						}
					}
				}
			}
		};

		// List Actor's DataLayers part of the owning world, then those only part of the actor level
		BuildDataLayersWithContext(Item, false);
		BuildDataLayersWithContext(Item, true);

		return Builder.ToString();
	});

	FGetTextForItem SubPackageInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				if (const ILevelInstanceInterface* ActorAsLevelInstance = Cast<ILevelInstanceInterface>(Actor))
				{
					return ActorAsLevelInstance->GetWorldAssetPackage();
				}
			}
		}
		else if (const FActorDescTreeItem* ActorDescItem = Item.CastTo<FActorDescTreeItem>())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = ActorDescItem->ActorDescHandle.Get())
			{
				if (FName LevelPackage = ActorDesc->GetLevelPackage(); !LevelPackage.IsNone())
				{
					return ActorDesc->GetLevelPackage().ToString();
				}
			}
		}

		return FString();
	});

	FGetTextForItem SocketInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
		{
			AActor* Actor = ActorItem->Actor.Get();

			if (Actor)
			{
				return Actor->GetAttachParentSocketName().ToString();
			}
		}
		
		return FString();
	});

	FGetTextForItem InternalNameInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				return Actor->GetFName().ToString();
			}
		}
		else if (const FComponentTreeItem* ComponentItem = Item.CastTo<FComponentTreeItem>())
		{
			if (UActorComponent* Component = ComponentItem->Component.Get())
			{
				return Component->GetFName().ToString();
			}
		}
		else if (const FActorDescTreeItem* ActorDescItem = Item.CastTo<FActorDescTreeItem>())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = ActorDescItem->ActorDescHandle.Get())
			{
				return ActorDesc->GetActorName().ToString();
			}
		}
		else if (const FActorFolderTreeItem* ActorFolderItem = Item.CastTo<FActorFolderTreeItem>())
		{
			if (const UActorFolder* ActorFolder = Cast<UActorFolder>(ActorFolderItem->GetActorFolder()))
			{
				return ActorFolder->GetFName().ToString();
			}
		}

		return FString();
	});

	FGetTextForItem LevelInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				return FPackageName::GetShortName(Actor->GetPackage()->GetName());
			}
		}
		else if (const FActorDescTreeItem* ActorDescItem = Item.CastTo<FActorDescTreeItem>())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = ActorDescItem->ActorDescHandle.Get())
			{
				return FPackageName::GetShortName(ActorDesc->GetActorPackage());
			}
		}
		else if (const FActorFolderTreeItem* ActorFolderItem = Item.CastTo<FActorFolderTreeItem>())
		{
			if (const UActorFolder* ActorFolder = Cast<UActorFolder>(ActorFolderItem->GetActorFolder()))
			{
				return FPackageName::GetShortName(ActorFolder->GetPackage()->GetName());
			}
		}

		return FString();
	});

	FGetTextForItem UncachedLightsInfoText = FGetTextForItem::CreateLambda([](const ISceneOutlinerTreeItem& Item) -> FString
	{
		if (const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>())
		{
			AActor* Actor = ActorItem->Actor.Get();
			if (Actor)
			{
				return FString::Printf(TEXT("%7d"), Actor->GetNumUncachedStaticLightingInteractions());
			}
		}
		return FString();
	});

	auto AddTextInfoColumn = [&InInitOptions](FName ColumnID, TAttribute<FText> ColumnName, FGetTextForItem ColumnInfo)
	{
		InInitOptions.ColumnMap.Add(
			ColumnID,
			FSceneOutlinerColumnInfo(
				ESceneOutlinerColumnVisibility::Invisible,
				20,
				FCreateSceneOutlinerColumn::CreateStatic(
					&FTextInfoColumn::CreateTextInfoColumn,
					ColumnID,
					ColumnInfo,
					FText::GetEmpty()),
				true,
				TOptional<float>(),
				ColumnName));
	};

	// The "Level" column should be named "Package Short Name" in wp enabled levels
	auto LevelColumnName = TAttribute<FText>::CreateLambda([WorldPtr]() -> FText
	{
		if (WorldPtr && WorldPtr->IsPartitionedWorld())
		{
			return FSceneOutlinerBuiltInColumnTypes::PackageShortName_Localized();
		}

		return FSceneOutlinerBuiltInColumnTypes::Level_Localized();
	});

	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::Mobility(), FSceneOutlinerBuiltInColumnTypes::Mobility_Localized(), MobilityInfoText);
	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::Level(), LevelColumnName, LevelInfoText);
	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::Layer(), FSceneOutlinerBuiltInColumnTypes::Layer_Localized(), LayerInfoText);
	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::DataLayer(), FSceneOutlinerBuiltInColumnTypes::DataLayer_Localized(), DataLayerInfoText);
	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::SubPackage(), FSceneOutlinerBuiltInColumnTypes::SubPackage_Localized(), SubPackageInfoText);
	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::Socket(), FSceneOutlinerBuiltInColumnTypes::Socket_Localized(), SocketInfoText);
	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::IDName(), FSceneOutlinerBuiltInColumnTypes::IDName_Localized(), InternalNameInfoText);
	AddTextInfoColumn(FSceneOutlinerBuiltInColumnTypes::UncachedLights(), FSceneOutlinerBuiltInColumnTypes::UncachedLights_Localized(), UncachedLightsInfoText);
}

IMPLEMENT_MODULE(FSceneOutlinerModule, SceneOutliner);

#undef LOCTEXT_NAMESPACE //SceneOutlinerModule
