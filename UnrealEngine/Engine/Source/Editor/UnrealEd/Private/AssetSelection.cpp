// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetSelection.h"
#include "Engine/Level.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"
#include "ActorFactories/ActorFactory.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Pawn.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Engine/Brush.h"
#include "Editor/GroupActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Particles/Emitter.h"
#include "Engine/Light.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DecalComponent.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "ScopedTransaction.h"

#include "LevelUtils.h"

#include "ComponentAssetBroker.h"

#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/CollectionDragDropOp.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "SnappingUtils.h"
#include "ActorEditorUtils.h"
#include "LevelEditorViewport.h"
#include "LandscapeProxy.h"
#include "Landscape.h"

#include "Editor/ActorPositioning.h"

#include "ObjectEditorUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Settings/LevelEditorMiscSettings.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelBounds.h"
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/MessageDialog.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementObjectInterface.h"

namespace AssetSelectionUtils
{
	bool IsClassPlaceable(const UClass* Class)
	{
		const bool bIsAddable =
			Class
			&& !(Class->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Deprecated | CLASS_Abstract))
			&& Class->IsChildOf( AActor::StaticClass() );
		return bIsAddable;
	}

	// Blueprints handle their own Abstract flag, but we should check for deprecation and NotPlaceable here
	bool IsChildBlueprintPlaceable(const UClass* Class)
	{
		const bool bIsAddable =
			Class
			&& !(Class->HasAnyClassFlags(CLASS_NotPlaceable | CLASS_Deprecated))
			&& Class->IsChildOf(AActor::StaticClass());
		return bIsAddable;
	}
	
	void GetSelectedAssets( TArray<FAssetData>& OutSelectedAssets )
	{
		// Add the selection from the content browser module
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().GetSelectedAssets(OutSelectedAssets);
	}

	FSelectedActorInfo BuildSelectedActorInfo( const TArray<AActor*>& SelectedActors)
	{
		FSelectedActorInfo ActorInfo;
		if( SelectedActors.Num() > 0 )
		{
			// Get the class type of the first actor.
			AActor* FirstActor = SelectedActors[0];

			if( FirstActor && !FirstActor->IsTemplate() )
			{
				UClass* FirstClass = FirstActor->GetClass();
				UObject* FirstArchetype = FirstActor->GetArchetype();

				ActorInfo.bAllSelectedAreBrushes = Cast< ABrush >( FirstActor ) != NULL;
				ActorInfo.SelectionClass = FirstClass;

				// Compare all actor types with the baseline.
				for ( int32 ActorIndex = 0; ActorIndex < SelectedActors.Num(); ++ActorIndex )
				{
					AActor* CurrentActor = SelectedActors[ ActorIndex ];

					if( CurrentActor->IsTemplate() )
					{
						continue;
					}

					ABrush* Brush = Cast< ABrush >( CurrentActor );
					if( !Brush)
					{
						ActorInfo.bAllSelectedAreBrushes = false;
					}
					else
					{
						if( !ActorInfo.bHaveBuilderBrush )
						{
							ActorInfo.bHaveBuilderBrush = FActorEditorUtils::IsABuilderBrush(Brush);
						}
						ActorInfo.bHaveBrush |= true;
						ActorInfo.bHaveBSPBrush |= (!Brush->IsVolumeBrush());
						ActorInfo.bHaveVolume |= Brush->IsVolumeBrush();
					}

					UClass* CurrentClass = CurrentActor->GetClass();
					if( FirstClass != CurrentClass )
					{
						ActorInfo.bAllSelectedActorsOfSameType = false;
						ActorInfo.SelectionClass = NULL;
						FirstClass = NULL;
					}
					else
					{
						ActorInfo.SelectionClass = CurrentActor->GetClass();
					}

					++ActorInfo.NumSelected;

					if( ActorInfo.bAllSelectedActorsBelongToCurrentLevel )
					{
						const ULevel* ActorLevel = CurrentActor->GetLevel();
						if( !ActorLevel || !ActorLevel->IsCurrentLevel() )
						{
							ActorInfo.bAllSelectedActorsBelongToCurrentLevel = false;
						}
					}

					if( ActorInfo.bAllSelectedActorsBelongToSameWorld )
					{
						if ( !ActorInfo.SharedWorld )
						{
							ActorInfo.SharedWorld = CurrentActor->GetWorld();
							check(ActorInfo.SharedWorld);
						}
						else
						{
							if( ActorInfo.SharedWorld != CurrentActor->GetWorld() )
							{
								ActorInfo.bAllSelectedActorsBelongToCurrentLevel = false;
								ActorInfo.SharedWorld = NULL;
							}
						}
					}

					// To prevent move to other level for Landscape if its components are distributed in streaming levels
					if (CurrentActor->IsA(ALandscape::StaticClass()))
					{
						ALandscape* Landscape = CastChecked<ALandscape>(CurrentActor);
						if (!Landscape || !Landscape->HasAllComponent())
						{
							if( !ActorInfo.bAllSelectedActorsBelongToCurrentLevel )
							{
								ActorInfo.bAllSelectedActorsBelongToCurrentLevel = true;
							}
						}
					}

					if ( ActorInfo.bSelectedActorsBelongToSameLevel )
					{
						ULevel* ActorLevel = CurrentActor->GetLevel();
						if ( !ActorInfo.SharedLevel )
						{
							// This is the first selected actor we've encountered.
							ActorInfo.SharedLevel = ActorLevel;
						}
						else
						{
							// Does this actor's level match the others?
							if ( ActorInfo.SharedLevel != ActorLevel )
							{
								ActorInfo.bSelectedActorsBelongToSameLevel = false;
								ActorInfo.SharedLevel = NULL;
							}
						}
					}


					AGroupActor* FoundGroup = Cast<AGroupActor>(CurrentActor);
					if(!FoundGroup)
					{
						FoundGroup = AGroupActor::GetParentForActor(CurrentActor);
					}
					if( FoundGroup )
					{
						if( !ActorInfo.bHaveSelectedSubGroup )
						{
							ActorInfo.bHaveSelectedSubGroup  = AGroupActor::GetParentForActor(FoundGroup) != NULL;
						}
						if( !ActorInfo.bHaveSelectedLockedGroup )
						{
							ActorInfo.bHaveSelectedLockedGroup = FoundGroup->IsLocked();
						}
						if( !ActorInfo.bHaveSelectedUnlockedGroup )
						{
							AGroupActor* FoundRoot = AGroupActor::GetRootForActor(CurrentActor);
							ActorInfo.bHaveSelectedUnlockedGroup = !FoundGroup->IsLocked() || ( FoundRoot && !FoundRoot->IsLocked() );
						}
					}
					else
					{
						++ActorInfo.NumSelectedUngroupedActors;
					}

					USceneComponent* RootComp = CurrentActor->GetRootComponent();
					if(RootComp != nullptr && RootComp->GetAttachParent() != nullptr)
					{
						ActorInfo.bHaveAttachedActor = true;
					}

					for (UActorComponent* Component : CurrentActor->GetComponents())
					{
						if (Component)
						{
							if( UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Component) )
							{
								if( SMComp->IsRegistered() )
								{
									ActorInfo.bHaveStaticMeshComponent = true;
								}
							}

							// Check for experimental/early-access classes in the component hierarchy
							bool bIsExperimental, bIsEarlyAccess;
							FString MostDerivedDevelopmentClassName;
							FObjectEditorUtils::GetClassDevelopmentStatus(Component->GetClass(), bIsExperimental, bIsEarlyAccess, MostDerivedDevelopmentClassName);

							ActorInfo.bHaveExperimentalClass |= bIsExperimental;
							ActorInfo.bHaveEarlyAccessClass |= bIsEarlyAccess;
						}
					}

					// Check for experimental/early-access classes in the actor hierarchy
					{
						bool bIsExperimental, bIsEarlyAccess;
						FString MostDerivedDevelopmentClassName;
						FObjectEditorUtils::GetClassDevelopmentStatus(CurrentClass, bIsExperimental, bIsEarlyAccess, MostDerivedDevelopmentClassName);

						ActorInfo.bHaveExperimentalClass |= bIsExperimental;
						ActorInfo.bHaveEarlyAccessClass |= bIsEarlyAccess;
					}

					if( CurrentActor->IsA( ALight::StaticClass() ) )
					{
						ActorInfo.bHaveLight = true;
					}

					if( CurrentActor->IsA( AStaticMeshActor::StaticClass() ) ) 
					{
						ActorInfo.bHaveStaticMesh = true;
						AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>( CurrentActor );
						if ( StaticMeshActor->GetStaticMeshComponent() )
						{
							UStaticMesh* StaticMesh = StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh();

							ActorInfo.bAllSelectedStaticMeshesHaveCollisionModels &= ( (StaticMesh && StaticMesh->GetBodySetup()) ? true : false );
						}
					}

					if( CurrentActor->IsA( ASkeletalMeshActor::StaticClass() ) )
					{
						ActorInfo.bHaveSkeletalMesh = true;
					}

					if( CurrentActor->IsA( APawn::StaticClass() ) )
					{
						ActorInfo.bHavePawn = true;
					}

					if( CurrentActor->IsA( AEmitter::StaticClass() ) )
					{
						ActorInfo.bHaveEmitter = true;
					}

					if ( CurrentActor->IsTemporarilyHiddenInEditor() )
					{
						ActorInfo.bHaveHidden = true;
					}

					if ( CurrentActor->IsA( ALandscapeProxy::StaticClass() ) )
					{
						ActorInfo.bHaveLandscape = true;
					}

					// Find our counterpart actor
					AActor* EditorWorldActor = EditorUtilities::GetEditorWorldCounterpartActor( CurrentActor );
					if( EditorWorldActor != NULL )
					{
						// Just count the total number of actors with counterparts
						++ActorInfo.NumSimulationChanges;
					}
				}

				if( ActorInfo.SelectionClass != NULL )
				{
					ActorInfo.SelectionStr = ActorInfo.SelectionClass->GetName();
				}
				else
				{
					ActorInfo.SelectionStr = TEXT("Actor");
				}


			}
		}

		// hack when only BSP is selected
		if( ActorInfo.SharedWorld == nullptr )
		{
			ActorInfo.SharedWorld = GWorld;
		}

		return ActorInfo;
	}

	FSelectedActorInfo GetSelectedActorInfo()
	{
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>( SelectedActors );
		return BuildSelectedActorInfo( SelectedActors );	
	}

	int32 GetNumSelectedSurfaces( UWorld* InWorld )
	{
		int32 NumSelectedSurfs = 0;
		UWorld* World = InWorld;
		if( !World )
		{
			World = GWorld;	// Fallback to GWorld
		}
		if( World )
		{
			const int32 NumLevels = World->GetNumLevels();
			for (int32 LevelIndex = 0; LevelIndex < NumLevels; LevelIndex++)
			{
				ULevel* Level = World->GetLevel(LevelIndex);
				UModel* Model = Level->Model;
				check(Model);
				const int32 NumSurfaces = Model->Surfs.Num();

				// Count the number of selected surfaces
				for (int32 Surface = 0; Surface < NumSurfaces; ++Surface)
				{
					FBspSurf *Poly = &Model->Surfs[Surface];

					if (Poly->PolyFlags & PF_Selected)
					{
						++NumSelectedSurfs;
					}
				}
			}
		}

		return NumSelectedSurfs;
	}

	bool IsAnySurfaceSelected( UWorld* InWorld )
	{
		UWorld* World = InWorld;
		if (!World)
		{
			World = GWorld;	// Fallback to GWorld
		}
		if (World)
		{
			const int32 NumLevels = World->GetNumLevels();
			for (int32 LevelIndex = 0; LevelIndex < NumLevels; LevelIndex++)
			{
				ULevel* Level = World->GetLevel(LevelIndex);
				UModel* Model = Level->Model;
				check(Model);
				const int32 NumSurfaces = Model->Surfs.Num();

				// Count the number of selected surfaces
				for (int32 Surface = 0; Surface < NumSurfaces; ++Surface)
				{
					FBspSurf *Poly = &Model->Surfs[Surface];

					if (Poly->PolyFlags & PF_Selected)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool IsBuilderBrushSelected()
	{
		bool bHasBuilderBrushSelected = false;

		for( FSelectionIterator SelectionIter = GEditor->GetSelectedActorIterator(); SelectionIter; ++SelectionIter )
		{
			AActor* Actor = Cast< AActor >(*SelectionIter);
			if( Actor && FActorEditorUtils::IsABuilderBrush( Actor ) )
			{
				bHasBuilderBrushSelected = true;
				break;
			}
		}

		return bHasBuilderBrushSelected;
	}
}

namespace ActorPlacementUtils
{
	bool IsLevelValidForActorPlacement(ULevel* InLevel, TArray<FTransform>& InActorTransforms)
	{
		if (FLevelUtils::IsLevelLocked(InLevel))
		{
			FNotificationInfo Info(NSLOCTEXT("UnrealEd", "Error_OperationDisallowedOnLockedLevel", "The requested operation could not be completed because the level is locked."));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
			return false;
		}
		if (InLevel && InLevel->OwningWorld && InLevel->OwningWorld->GetStreamingLevels().Num() == 0)
		{
			// if this is the only level
			return true;
		}
		if (GIsRunningUnattendedScript)
		{
			// Don't prompt user for checks in unattended mode
			return true;
		}
		if (InLevel && InLevel->GetPromptWhenAddingToLevelBeforeCheckout() && SourceControlHelpers::IsAvailable())
		{
			FString FileName = SourceControlHelpers::PackageFilename(InLevel->GetPathName());
			// Query file state also checks the source control status
			FSourceControlStatePtr SCState = ISourceControlModule::Get().GetProvider().GetState(FileName, EStateCacheUsage::Use);
			if (!(SCState->IsCheckedOut() || SCState->IsAdded() || SCState->CanAdd() || SCState->IsUnknown()))
			{
				FText Title = NSLOCTEXT("UnrealEd", "LevelCheckout_Title", "Level Checkout Warning");
				if (EAppReturnType::Ok != FMessageDialog::Open(EAppMsgType::OkCancel, NSLOCTEXT("UnrealEd","LevelNotCheckedOutMsg", "This actor will be placed in a level that is in source control but not currently checked out. Continue?"), &Title))
				{
					return false;
				}
				else
				{
					InLevel->bPromptWhenAddingToLevelBeforeCheckout = false;
				}
			}
		}
		if (InLevel && InLevel->OwningWorld)
		{
			int32 LevelCount = InLevel->OwningWorld->GetStreamingLevels().Num();
			int32 NumLockedLevels = 0;
			// Check for streaming level count b/c we know there is > 1 streaming level
			for (ULevelStreaming* StreamingLevel : InLevel->OwningWorld->GetStreamingLevels())
			{
				StreamingLevel->bLocked ? NumLockedLevels++ : 0;
			}
			// If there is only one unlocked level, a) ours is the unlocked level b/c of the previous IsLevelLocked test and b) we shouldn't try to check for level bounds on the next test
			if (LevelCount - NumLockedLevels == 1)
			{
				return true;
			}
		}
		if (InLevel && InLevel->GetPromptWhenAddingToLevelOutsideBounds())
		{
			FBox CurrentLevelBounds(ForceInit);
			if (InLevel->LevelBoundsActor.IsValid())
			{
				CurrentLevelBounds = InLevel->LevelBoundsActor.Get()->GetComponentsBoundingBox();
			}
			else
			{
				CurrentLevelBounds = ALevelBounds::CalculateLevelBounds(InLevel);
			}

			FVector BoundsExtent = CurrentLevelBounds.GetExtent();
			if (BoundsExtent.X < GetDefault<ULevelEditorMiscSettings>()->MinimumBoundsForCheckingSize.X
				&& BoundsExtent.Y < GetDefault<ULevelEditorMiscSettings>()->MinimumBoundsForCheckingSize.Y
				&& BoundsExtent.Z < GetDefault<ULevelEditorMiscSettings>()->MinimumBoundsForCheckingSize.Z)
			{
				return true;
			}
			CurrentLevelBounds = CurrentLevelBounds.ExpandBy(BoundsExtent * (GetDefault<ULevelEditorMiscSettings>()->PercentageThresholdForPrompt / 100.0f));
			for (int32 ActorTransformIndex = 0; ActorTransformIndex < InActorTransforms.Num(); ++ActorTransformIndex)
			{
				FTransform ActorTransform = InActorTransforms[ActorTransformIndex];
				if (!CurrentLevelBounds.IsInsideOrOn(ActorTransform.GetLocation()))
				{
					FText Title = NSLOCTEXT("UnrealEd", "ActorPlacement_Title", "Actor Placement Warning");
					if (EAppReturnType::Ok != FMessageDialog::Open(EAppMsgType::OkCancel, NSLOCTEXT("UnrealEd", "LevelBoundsMsg", "The actor will be placed outside the bounds of the current level. Continue?"), &Title))
					{
						return false;
					}
					else
					{
						InLevel->bPromptWhenAddingToLevelOutsideBounds = false;
						break;
					}
				}
			}
		}
		return true;
	}
}

/**
* Creates an actor using the specified factory.  
*
* Does nothing if ActorClass is NULL.
*/
static AActor* PrivateAddActor( UObject* Asset, UActorFactory* Factory, bool SelectActor = true, EObjectFlags ObjectFlags = RF_Transactional, const FName Name = NAME_None )
{
	if (!Factory)
	{
		return nullptr;
	}

	AActor* Actor = NULL;
	AActor* NewActorTemplate = Factory->GetDefaultActor( Asset );
	if ( !NewActorTemplate )
	{
		return nullptr;
	}

	UWorld* OldWorld = nullptr;

	// The play world needs to be selected if it exists
	if (GIsEditor && GEditor->PlayWorld && !GIsPlayInEditorWorld)
	{
		OldWorld = SetPlayInEditorWorld(GEditor->PlayWorld);
	}

	// For Brushes/Volumes, use the default brush as the template rather than the factory default actor
	if (NewActorTemplate->IsA(ABrush::StaticClass()) && GWorld->GetDefaultBrush() != nullptr)
	{
		NewActorTemplate = GWorld->GetDefaultBrush();
	}

	const FSnappedPositioningData PositioningData = FSnappedPositioningData(GCurrentLevelEditingViewportClient, GEditor->ClickLocation, GEditor->ClickPlane)
		.UseFactory(Factory)
		.UsePlacementExtent(NewActorTemplate->GetPlacementExtent());

	FTransform ActorTransform = FActorPositioning::GetSnappedSurfaceAlignedTransform(PositioningData);

	if (GetDefault<ULevelEditorViewportSettings>()->SnapToSurface.bEnabled)
	{
		// HACK: If we are aligning rotation to surfaces, we have to factor in the inverse of the actor's rotation and translation so that the resulting transform after SpawnActor is correct.

		if (auto* RootComponent = NewActorTemplate->GetRootComponent())
		{
			RootComponent->UpdateComponentToWorld();
		}

		FVector OrigActorScale3D = ActorTransform.GetScale3D();
		ActorTransform = NewActorTemplate->GetTransform().Inverse() * ActorTransform;
		ActorTransform.SetScale3D(OrigActorScale3D);
	}

	// Do not fade snapping indicators over time if the viewport is not realtime
	bool bClearImmediately = !GCurrentLevelEditingViewportClient || !GCurrentLevelEditingViewportClient->IsRealtime();
	FSnappingUtils::ClearSnappingHelpers( bClearImmediately );

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();

	// If DesireLevel is part of a LevelPartition find the proper DesiredLevel by asking the Partition
	if (const ILevelPartitionInterface* LevelPartition = DesiredLevel->GetLevelPartition())
	{
		if (ULevel* SubLevel = LevelPartition->GetSubLevel(ActorTransform.GetLocation()))
		{
			DesiredLevel = SubLevel;
		}
	}

	bool bSpawnActor = true;

	if ((ObjectFlags & RF_Transactional) != 0)
	{
		TArray<FTransform> SpawningActorTransforms;
		SpawningActorTransforms.Add(ActorTransform);
		bSpawnActor = ActorPlacementUtils::IsLevelValidForActorPlacement(DesiredLevel, SpawningActorTransforms);
	}

	if(bSpawnActor)
	{
		FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "CreateActor", "Create Actor"), (ObjectFlags & RF_Transactional) != 0 );
		
		// Create the actor.
		UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
		if (PlacementSubsystem)
		{
			FAssetPlacementInfo PlacementInfo;
			PlacementInfo.AssetToPlace = FAssetData(Asset);
			PlacementInfo.PreferredLevel = DesiredLevel;
			PlacementInfo.NameOverride = Name;
			PlacementInfo.FinalizedTransform = ActorTransform;
			PlacementInfo.FactoryOverride = Factory;

			FPlacementOptions PlacementOptions;
			PlacementOptions.bIsCreatingPreviewElements = FLevelEditorViewportClient::IsDroppingPreviewActor();

			TArray<FTypedElementHandle> PlacedElements = PlacementSubsystem->PlaceAsset(PlacementInfo, PlacementOptions);
			if (PlacedElements.Num())
			{
				if (TTypedElement<ITypedElementObjectInterface> ObjectInterface = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementObjectInterface>(PlacedElements[0]))
				{
					Actor = ObjectInterface.GetObjectAs<AActor>();
					if (Actor)
					{
						Actor->SetFlags(ObjectFlags);
					}
				}
			}
		}
		
		if (!Actor)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.ObjectFlags = ObjectFlags;
			SpawnParams.Name = Name;
			Actor = Factory->CreateActor(Asset, DesiredLevel, ActorTransform, SpawnParams);
		}

		if (Actor)
		{
			if ( SelectActor )
			{
				GEditor->SelectNone( false, true );
				GEditor->SelectActor( Actor, true, true );
			}

			Actor->InvalidateLightingCache();
			Actor->PostEditChange();
		}

		GEditor->RedrawLevelEditingViewports();
	}

	if ( Actor )
	{
		Actor->MarkPackageDirty();
		ULevel::LevelDirtiedEvent.Broadcast();
	}

	// Restore the old world if there was one
	if (OldWorld)
	{
		RestoreEditorWorld(OldWorld);
	}

	return Actor;
}


namespace AssetUtil
{
	TArray<FAssetData> ExtractAssetDataFromDrag(const FDragDropEvent &DragDropEvent)
	{
		return ExtractAssetDataFromDrag(DragDropEvent.GetOperation());
	}

	TArray<FAssetData> ExtractAssetDataFromDrag(const TSharedPtr<const FDragDropOperation>& Operation)
	{
		TArray<FAssetData> DroppedAssetData;

		if (!Operation.IsValid())
		{
			return DroppedAssetData;
		}

		if (Operation->IsOfType<FExternalDragOperation>())
		{
			TSharedPtr<const FExternalDragOperation> DragDropOp = StaticCastSharedPtr<const FExternalDragOperation>(Operation);
			if (DragDropOp->HasText())
			{
				TArray<FString> DroppedAssetStrings;
				const TCHAR AssetDelimiter[] = { AssetMarshalDefs::AssetDelimiter, TEXT('\0') };
				DragDropOp->GetText().ParseIntoArray(DroppedAssetStrings, AssetDelimiter, true);

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

				for (const FString& DroppedAssetString : DroppedAssetStrings)
				{
					if (DroppedAssetString.Len() < NAME_SIZE && FName::IsValidXName(DroppedAssetString, INVALID_OBJECTPATH_CHARACTERS))
					{
						FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(DroppedAssetString));
						if (AssetData.IsValid())
						{
							DroppedAssetData.Add(AssetData);
						}
					}
				}
			}
		}
		else if (Operation->IsOfType<FCollectionDragDropOp>())
		{
			TSharedPtr<const FCollectionDragDropOp> DragDropOp = StaticCastSharedPtr<const FCollectionDragDropOp>(Operation);
			DroppedAssetData.Append(DragDropOp->GetAssets());
		}
		else if (Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<const FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<const FAssetDragDropOp>(Operation);
			DroppedAssetData.Append(DragDropOp->GetAssets());
		}

		return DroppedAssetData;
	}

	FReply CanHandleAssetDrag( const FDragDropEvent &DragDropEvent )
	{
		TArray<FAssetData> DroppedAssetData = ExtractAssetDataFromDrag(DragDropEvent);
		for ( auto AssetIt = DroppedAssetData.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			const FAssetData& AssetData = *AssetIt;

			if ( AssetData.IsValid() && FComponentAssetBrokerage::GetPrimaryComponentForAsset( AssetData.GetClass() ) )
			{
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}
}



/* ==========================================================================================================
FActorFactoryAssetProxy
========================================================================================================== */

void FActorFactoryAssetProxy::GenerateActorFactoryMenuItems( const FAssetData& AssetData, TArray<FMenuItem>* OutMenuItems, bool ExcludeStandAloneFactories  )
{
	FText UnusedErrorMessage;
	const FAssetData NoAssetData {};
	for ( int32 FactoryIdx = 0; FactoryIdx < GEditor->ActorFactories.Num(); FactoryIdx++ )
	{
		UActorFactory* Factory = GEditor->ActorFactories[FactoryIdx];

		const bool FactoryWorksWithoutAsset = Factory->CanCreateActorFrom( NoAssetData, UnusedErrorMessage );
		const bool FactoryWorksWithAsset = AssetData.IsValid() && Factory->CanCreateActorFrom( AssetData, UnusedErrorMessage );
		const bool FactoryWorks = FactoryWorksWithAsset || FactoryWorksWithoutAsset;

		if ( FactoryWorks )
		{
			FMenuItem MenuItem = FMenuItem( Factory, NoAssetData );
			if ( FactoryWorksWithAsset )
			{
				MenuItem = FMenuItem( Factory, AssetData );
			}

			if ( FactoryWorksWithAsset || ( !ExcludeStandAloneFactories && FactoryWorksWithoutAsset ) )
			{
				OutMenuItems->Add( MenuItem );
			}
		}
	}
}

/**
* Find the appropriate actor factory for an asset by type.
*
* @param	AssetData			contains information about an asset that to get a factory for
* @param	bRequireValidObject	indicates whether a valid asset object is required.  specify false to allow the asset
*								class's CDO to be used in place of the asset if no asset is part of the drag-n-drop
*
* @return	the factory that is responsible for creating actors for the specified asset type.
*/
UActorFactory* FActorFactoryAssetProxy::GetFactoryForAsset( const FAssetData& AssetData, bool bRequireValidObject/*=false*/ )
{
	UObject* Asset = NULL;
	UClass* AssetClass = AssetData.GetClass();
	
	if ( AssetData.IsAssetLoaded() )
	{
		Asset = AssetData.GetAsset();
	}
	else if ( !bRequireValidObject && AssetClass )
	{
		Asset = AssetClass->GetDefaultObject();
	}

	return FActorFactoryAssetProxy::GetFactoryForAssetObject( Asset );
}

/**
* Find the appropriate actor factory for an asset.
*
* @param	AssetObj	The asset that to find the appropriate actor factory for
*
* @return	The factory that is responsible for creating actors for the specified asset
*/
UActorFactory* FActorFactoryAssetProxy::GetFactoryForAssetObject( UObject* AssetObj )
{
	UActorFactory* Result = NULL;

	// Attempt to find a factory that is capable of creating the asset
	const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
	FText UnusedErrorMessage;
	FAssetData AssetData( AssetObj );
	for ( int32 FactoryIdx = 0; Result == NULL && FactoryIdx < ActorFactories.Num(); ++FactoryIdx )
	{
		UActorFactory* ActorFactory = ActorFactories[FactoryIdx];
		// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
		if ( ActorFactory->CanCreateActorFrom( AssetData, UnusedErrorMessage ) )
		{
			Result = ActorFactory;
		}
	}

	return Result;
}

AActor* FActorFactoryAssetProxy::AddActorForAsset( UObject* AssetObj, bool SelectActor, EObjectFlags ObjectFlags, UActorFactory* FactoryToUse /*= NULL*/, const FName Name )
{
	AActor* Result = NULL;

	const FAssetData AssetData(AssetObj, FAssetData::ECreationFlags::AllowBlueprintClass);
	FText UnusedErrorMessage;
	if ( AssetObj != NULL )
	{
		// If a specific factory has been provided, verify its validity and then use it to create the actor
		if ( FactoryToUse )
		{
			if ( FactoryToUse->CanCreateActorFrom( AssetData, UnusedErrorMessage ) )
			{
				Result = PrivateAddActor( AssetObj, FactoryToUse, SelectActor, ObjectFlags, Name );
			}
		}
		// If no specific factory has been provided, find the highest priority one that is valid for the asset and use
		// it to create the actor
		else
		{
			const TArray<UActorFactory*>& ActorFactories = GEditor->ActorFactories;
			for ( int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); FactoryIdx++ )
			{
				UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

				// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
				if ( ActorFactory->CanCreateActorFrom( AssetData, UnusedErrorMessage ) )
				{
					Result = PrivateAddActor(AssetObj, ActorFactory, SelectActor, ObjectFlags, Name);
					if ( Result != NULL )
					{
						break;
					}
				}
			}
		}
	}


	return Result;
}

AActor* FActorFactoryAssetProxy::AddActorFromSelection( UClass* ActorClass, const FVector* ActorLocation, bool SelectActor, EObjectFlags ObjectFlags, UActorFactory* ActorFactory, const FName Name )
{
	check( ActorClass != NULL );

	if( !ActorFactory )
	{
		// Look for an actor factory capable of creating actors of the actors type.
		ActorFactory = GEditor->FindActorFactoryForActorClass( ActorClass );
	}

	AActor* Result = NULL;
	FText ErrorMessage;

	if ( ActorFactory )
	{
		UObject* TargetObject = GEditor->GetSelectedObjects()->GetTop<UObject>();

		if( TargetObject && ActorFactory->CanCreateActorFrom( FAssetData(TargetObject), ErrorMessage ) )
		{
			// Attempt to add the actor
			Result = PrivateAddActor( TargetObject, ActorFactory, SelectActor, ObjectFlags );
		}
	}

	return Result;
}

/**
* Determines if the provided actor is capable of having a material applied to it.
*
* @param	TargetActor	Actor to check for the validity of material application
*
* @return	true if the actor is valid for material application; false otherwise
*/
bool FActorFactoryAssetProxy::IsActorValidForMaterialApplication( AActor* TargetActor )
{
	bool bIsValid = false;

	//@TODO: PAPER2D: Extend this to support non mesh components (or make sprites a mesh component)

	// Check if the actor has a mesh or fog volume density. If so, it can likely have
	// a material applied to it. Otherwise, it cannot.
	if ( TargetActor )
	{
		for (UActorComponent* Component : TargetActor->GetComponents())
		{
			if (Cast<UMeshComponent>(Component))
			{
				bIsValid = true;
				break;
			}
		}
	}

	return bIsValid;
}
/**
* Attempts to apply the material to the specified actor.
*
* @param	TargetActor		the actor to apply the material to
* @param	MaterialToApply	the material to apply to the actor
* @param    OptionalMaterialSlot the material slot to apply to.
*
* @return	true if the material was successfully applied to the actor
*/
bool FActorFactoryAssetProxy::ApplyMaterialToActor( AActor* TargetActor, UMaterialInterface* MaterialToApply, int32 OptionalMaterialSlot )
{
	bool bResult = false;

	if ( TargetActor != NULL && MaterialToApply != NULL )
	{
		ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(TargetActor);
		if (Landscape != NULL)
		{
			FProperty* MaterialProperty = FindFProperty<FProperty>(ALandscapeProxy::StaticClass(), "LandscapeMaterial");
			Landscape->PreEditChange(MaterialProperty);
			Landscape->LandscapeMaterial = MaterialToApply;
			FPropertyChangedEvent PropertyChangedEvent(MaterialProperty);
			Landscape->PostEditChangeProperty(PropertyChangedEvent);
			bResult = true;
		}
		else
		{
			TArray<UActorComponent*> EditableComponents;
			FActorEditorUtils::GetEditableComponents( TargetActor, EditableComponents );

			// Some actors could potentially have multiple mesh components, so we need to store all of the potentially valid ones
			// (or else perform special cases with IsA checks on the target actor)
			TArray<USceneComponent*> FoundMeshComponents;

			// Find which mesh the user clicked on first.
			for (UActorComponent* Component : TargetActor->GetComponents())
			{
				USceneComponent* SceneComp = Cast<USceneComponent>(Component);
				
				// Only apply the material to editable components.  Components which are not exposed are not intended to be changed.
				if (SceneComp && EditableComponents.Contains( SceneComp ) )
				{
					UMeshComponent* MeshComponent = Cast<UMeshComponent>(SceneComp);

					if((MeshComponent && MeshComponent->IsRegistered()) ||
						SceneComp->IsA<UDecalComponent>())
					{
						// Intentionally do not break the loop here, as there could be potentially multiple mesh components
						FoundMeshComponents.AddUnique( SceneComp );
					}
				}
			}

			if ( FoundMeshComponents.Num() > 0 )
			{
				// Check each component that was found
				for ( TArray<USceneComponent*>::TConstIterator MeshCompIter( FoundMeshComponents ); MeshCompIter; ++MeshCompIter )
				{
					USceneComponent* SceneComp = *MeshCompIter;
					bResult = FComponentEditorUtils::AttemptApplyMaterialToComponent(SceneComp, MaterialToApply, OptionalMaterialSlot);
				}
			}
		}
	}


	return bResult;
}


// EOF




