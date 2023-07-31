// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackedLevelActor/PackedLevelActorBuilder.h"

#if WITH_EDITOR

#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/IPackedLevelActorBuilder.h"
#include "PackedLevelActor/PackedLevelActorISMBuilder.h"
#include "PackedLevelActor/PackedLevelActorRecursiveBuilder.h"

#include "LevelInstance/LevelInstanceSubsystem.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "Logging/MessageLog.h"

#include "Components/SceneComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/LevelBounds.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Brush.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "Factories/BlueprintFactory.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorDirectories.h"

#define LOCTEXT_NAMESPACE "FPackedLevelActorBuilder"

void FPackedLevelActorBuilderContext::ClusterLevelActor(AActor* InActor)
{
	if (!ActorDiscards.Contains(InActor))
	{
		PerActorClusteredComponents.FindOrAdd(InActor);

		for (const auto& Pair : Builders)
		{
			Pair.Value->GetPackClusters(*this, InActor);
		}
	}
}

void FPackedLevelActorBuilderContext::FindOrAddCluster(FPackedLevelActorBuilderClusterID&& InClusterID, UActorComponent* InComponent)
{
	TArray<UActorComponent*>& ClusterComponents = Clusters.FindOrAdd(MoveTemp(InClusterID));
	if (InComponent)
	{
		ClusterComponents.Add(InComponent);
		PerActorClusteredComponents.FindChecked(InComponent->GetOwner()).Add(InComponent);
	}
}

void FPackedLevelActorBuilderContext::DiscardActor(AActor* InActor)
{
	ActorDiscards.Add(InActor);
}

FPackedLevelActorBuilder::FPackedLevelActorBuilder()
{
}

const FString& FPackedLevelActorBuilder::GetPackedBPPrefix()
{
	static FString BPPrefix = "BPP_";
	return BPPrefix;
}

UBlueprint* FPackedLevelActorBuilder::CreatePackedLevelActorBlueprintWithDialog(TSoftObjectPtr<UBlueprint> InBlueprintAsset, TSoftObjectPtr<UWorld> InWorldAsset, bool bInCompile)
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = FPaths::GetPath(InBlueprintAsset.GetLongPackageName());
	SaveAssetDialogConfig.DefaultAssetName = InBlueprintAsset.GetAssetName();
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		TSoftObjectPtr<UBlueprint> ExistingBPAsset(SaveObjectPath);

		if (UBlueprint* BP = ExistingBPAsset.LoadSynchronous())
		{
			return BP;
		}
				
		return CreatePackedLevelActorBlueprint(ExistingBPAsset, InWorldAsset, bInCompile);
	}

	return nullptr;
}

UBlueprint* FPackedLevelActorBuilder::CreatePackedLevelActorBlueprint(TSoftObjectPtr<UBlueprint> InBlueprintAsset, TSoftObjectPtr<UWorld> InWorldAsset, bool bInCompile)
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = APackedLevelActor::StaticClass();
	BlueprintFactory->bSkipClassPicker = true;

	FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(BlueprintFactory);
	if (BlueprintFactory->ConfigureProperties())
	{
		FString PackageDir = FPaths::GetPath(InBlueprintAsset.GetLongPackageName());
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, PackageDir);

		if (UBlueprint* NewBP = Cast<UBlueprint>(AssetTools.CreateAsset(InBlueprintAsset.GetAssetName(), PackageDir, UBlueprint::StaticClass(), BlueprintFactory, FName("Create LevelInstance Blueprint"))))
		{
			APackedLevelActor* CDO = CastChecked<APackedLevelActor>(NewBP->GeneratedClass->GetDefaultObject());
			CDO->SetWorldAsset(InWorldAsset);

			if (bInCompile)
			{
				FKismetEditorUtilities::CompileBlueprint(NewBP, EBlueprintCompileOptions::None);
			}

			AssetTools.SyncBrowserToAssets(TArray<UObject*>{ NewBP });

			return NewBP;
		}
	}

	return nullptr;
}

TSharedPtr<FPackedLevelActorBuilder> FPackedLevelActorBuilder::CreateDefaultBuilder()
{
	TSharedPtr<FPackedLevelActorBuilder> Builder = MakeShared<FPackedLevelActorBuilder>();

	// Class Discards are used to validate the packing result.
	// Components or Actor classes in this set will not generate warnings
	Builder->ClassDiscards.Add(ALevelBounds::StaticClass());
	// Avoid dependency (find class)
	UClass* ChaosDebugClass = FindObject<UClass>(nullptr, TEXT("/Script/ChaosSolverEngine.ChaosDebugDrawComponent"));
	if (ChaosDebugClass)
	{
		Builder->ClassDiscards.Add(ChaosDebugClass);
	}

	// Root Components that are SceneComponents (not child class of)
	Builder->ClassDiscards.Add(USceneComponent::StaticClass());
	
	Builder->Builders.Add(FPackedLevelActorRecursiveBuilder::BuilderID, MakeUnique<FPackedLevelActorRecursiveBuilder>());
	Builder->Builders.Add(FPackedLevelActorISMBuilder::BuilderID, MakeUnique<FPackedLevelActorISMBuilder>());

	return Builder;
}

bool FPackedLevelActorBuilder::PackActor(APackedLevelActor* InPackedLevelActor)
{
	return PackActor(InPackedLevelActor, InPackedLevelActor);
}

bool FPackedLevelActorBuilder::PackActor(APackedLevelActor* InPackedLevelActor, ALevelInstance* InLevelInstanceToPack)
{
	FMessageLog Log("PackedLevelActor");
	Log.Info(FText::Format(LOCTEXT("PackingStarted", "Packing of '{0}' started..."), FText::FromString(InPackedLevelActor->GetWorldAssetPackage())));
	
	FPackedLevelActorBuilderContext Context(*this, InPackedLevelActor);

	InPackedLevelActor->DestroyPackedComponents();

	ULevelInstanceSubsystem* LevelInstanceSubystem = InPackedLevelActor->GetLevelInstanceSubsystem();
	check(LevelInstanceSubystem);
	
	ULevel* SourceLevel = LevelInstanceSubystem->GetLevelInstanceLevel(InLevelInstanceToPack);
	if (!SourceLevel)
	{
		Log.Error(FText::Format(LOCTEXT("FailedPackingNoLevel", "Packing of '{0}' failed"), FText::FromString(InPackedLevelActor->GetWorldAssetPackage())));
		return false;
	}

	ULevelStreaming* SourceLevelStreaming = ULevelStreaming::FindStreamingLevel(SourceLevel);
	AWorldSettings* WorldSettings = SourceLevel->GetWorldSettings();
	Context.DiscardActor(WorldSettings);

	// Build relative transform without rotation because pivots don't support rotation
	FTransform CurrentPivotTransform(SourceLevelStreaming->LevelTransform.GetRelativeTransform(InPackedLevelActor->GetActorTransform()).GetTranslation());
	FTransform NewPivotTransform(WorldSettings->LevelInstancePivotOffset);
	FTransform RelativePivotTransform(NewPivotTransform.GetRelativeTransform(CurrentPivotTransform));
		
	Context.SetRelativePivotTransform(RelativePivotTransform);
	
	if (AActor* DefaultBrush = SourceLevel->GetDefaultBrush())
	{
		Context.DiscardActor(DefaultBrush);
	}
		
	for (AActor* LevelActor : SourceLevel->Actors)
	{
		if (LevelActor)
		{
			Context.ClusterLevelActor(LevelActor);
		}
	}

	for (const auto& Pair : Context.GetClusters())
	{
		TUniquePtr<IPackedLevelActorBuilder>& Builder = Builders.FindChecked(Pair.Key.GetBuilderID());
		Builder->PackActors(Context, InPackedLevelActor, Pair.Key, Pair.Value);
	}
			
	Context.Report(Log);
	return true;
}

bool FPackedLevelActorBuilderContext::ShouldPackComponent(UActorComponent* ActorComponent) const
{
	return ActorComponent && !ActorComponent->IsVisualizationComponent();
}

void FPackedLevelActorBuilderContext::Report(FMessageLog& Log) const
{
	TSet<UActorComponent*> NotClusteredComponents;
	uint32 TotalWarningCount = 0;

	for (const auto& Pair : PerActorClusteredComponents)
	{
		AActor* Actor = Pair.Key;
		const TSet<UActorComponent*>& ClusteredComponents = Pair.Value;

		if (ActorDiscards.Contains(Actor))
		{
			Log.Info(FText::Format(LOCTEXT("ActorDiscard", "Actor '{0}' ignored (Actor Discard)"), FText::FromString(Actor->GetPathName())));
			continue;
		}

		if (Actor->GetClass()->HasAnyClassFlags(CLASS_Transient))
		{
			Log.Info(FText::Format(LOCTEXT("ActorTransientClassDiscard", "Actor '{0}' of type '{1}' ignored (Transient Class Discard)"), FText::FromString(Actor->GetPathName()), FText::FromString(Actor->GetClass()->GetPathName())));
			continue;
		}

		// Class must match (not a child)
		if (ClassDiscards.Contains(Actor->GetClass()))
		{
			Log.Info(FText::Format(LOCTEXT("ActorClassDiscard", "Actor '{0}' of type '{1}' ignored (Class Discard)"), FText::FromString(Actor->GetPathName()), FText::FromString(Actor->GetClass()->GetPathName())));
			continue;
		}

		NotClusteredComponents = Actor->GetComponents().Difference(ClusteredComponents);
		uint32 WarningCount = 0;
		for (UActorComponent* Component : NotClusteredComponents)
		{
			if (!ShouldPackComponent(Component))
			{
				continue;
			}

			if (ClassDiscards.Contains(Component->GetClass()))
			{
				Log.Info(FText::Format(LOCTEXT("ComponentClassDiscard", "Component '{0}' of type '{1}' ignored (Class Discard)"), FText::FromString(Component->GetPathName()), FText::FromString(Component->GetClass()->GetPathName())));
				continue;
			}

			if (Actor->GetClass()->HasAnyClassFlags(CLASS_Transient))
			{
				Log.Info(FText::Format(LOCTEXT("ComopnentTransientClassDiscard", "Component '{0}' of type '{1}' ignored (Transient Class Discard)"), FText::FromString(Component->GetPathName()), FText::FromString(Component->GetClass()->GetPathName())));
				continue;
			}

			WarningCount++;

			Log.Warning(FText::Format(LOCTEXT("ComponentNotPacked", "Component '{0}' was not packed"), FText::FromString(Component->GetPathName())));
		}

		if (WarningCount)
		{
			Log.Warning(FText::Format(LOCTEXT("ActorNotPacked", "Actor '{0}' was not packed completely ({1} warning(s))"), FText::FromString(Actor->GetPathName()), FText::AsNumber(WarningCount)));
		}
		else
		{
			Log.Info(FText::Format(LOCTEXT("ActorPacked", "Actor '{0}' packed successfully"), FText::FromString(Actor->GetPathName())));
		}
		TotalWarningCount += WarningCount;
	}

	if (TotalWarningCount)
	{
		Log.Warning(LOCTEXT("WarningsReported", "Warnings have been reported. Consider using a regular ALevelInstance instead."));
		Log.Open();
	}
	Log.Info(FText::Format(LOCTEXT("PackCompleted", "Packing '{0}' completed with {1} warning(s)"), FText::FromString(PackedLevelActor->GetWorldAssetPackage()), FText::AsNumber(TotalWarningCount)));
}

ALevelInstance* FPackedLevelActorBuilder::CreateTransientLevelInstanceForPacking(TSoftObjectPtr<UWorld> InWorldAsset, const FVector& InLocation, const FRotator& InRotator)
{
	// Create Temp Actor for Packing
	FActorSpawnParameters SpawnParams;
	SpawnParams.bCreateActorPackage = false;
	SpawnParams.bHideFromSceneOutliner = true;
	SpawnParams.bNoFail = true;
	SpawnParams.ObjectFlags |= RF_Transient;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	check(World);
	SpawnParams.OverrideLevel = World->PersistentLevel;
	ALevelInstance* LevelInstance = World->SpawnActor<ALevelInstance>(InLocation, InRotator, SpawnParams);
	LevelInstance->SetWorldAsset(InWorldAsset);

	// Wait for load
	LevelInstance->GetLevelInstanceSubsystem()->BlockLoadLevelInstance(LevelInstance);

	return LevelInstance;
}

bool FPackedLevelActorBuilder::PackActor(APackedLevelActor* InActor, TSoftObjectPtr<UWorld> InWorldAsset)
{
	ALevelInstance* TransientLevelInstance = CreateTransientLevelInstanceForPacking(InWorldAsset, InActor->GetActorLocation(), InActor->GetActorRotation());
	ON_SCOPE_EXIT
	{
		TransientLevelInstance->GetWorld()->DestroyActor(TransientLevelInstance);
	};
	
	return PackActor(InActor, TransientLevelInstance);
}

void FPackedLevelActorBuilder::UpdateBlueprint(UBlueprint* Blueprint, bool bCheckoutAndSave)
{
	APackedLevelActor* CDO = CastChecked<APackedLevelActor>(Blueprint->GeneratedClass->GetDefaultObject());
	check(CDO);

	CreateOrUpdateBlueprint(CDO->GetWorldAsset(), Blueprint, bCheckoutAndSave, /*bPromptForSave=*/false);
}

bool FPackedLevelActorBuilder::CreateOrUpdateBlueprint(TSoftObjectPtr<UWorld> InWorldAsset, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptForSave)
{
	bool bResult = true;
	
	ALevelInstance* TransientLevelInstance = CreateTransientLevelInstanceForPacking(InWorldAsset, FVector::ZeroVector, FRotator::ZeroRotator);
	
	bResult = CreateOrUpdateBlueprintFromUnpacked(TransientLevelInstance, InBlueprintAsset, bCheckoutAndSave, bPromptForSave);

	TransientLevelInstance->GetWorld()->DestroyActor(TransientLevelInstance);

	return bResult;
}

bool FPackedLevelActorBuilder::CreateOrUpdateBlueprint(ALevelInstance* InLevelInstance, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptForSave)
{
	if (APackedLevelActor* PackedLevelActor = Cast<APackedLevelActor>(InLevelInstance))
	{
		return CreateOrUpdateBlueprintFromPacked(PackedLevelActor, InBlueprintAsset, bCheckoutAndSave, bPromptForSave);
	}
	
	return CreateOrUpdateBlueprintFromUnpacked(InLevelInstance, InBlueprintAsset, bCheckoutAndSave, bPromptForSave);
}

bool FPackedLevelActorBuilder::CreateOrUpdateBlueprintFromUnpacked(ALevelInstance* InActor, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptForSave)
{
	bool bResult = true;
	
	// Create Temp Actor for Packing
	FActorSpawnParameters SpawnParams;
	SpawnParams.bCreateActorPackage = false;
	SpawnParams.bHideFromSceneOutliner = true;
	SpawnParams.bNoFail = true;
	SpawnParams.ObjectFlags |= RF_Transient;

	UWorld* World = InActor->GetWorld();
	SpawnParams.OverrideLevel = World->PersistentLevel;

	APackedLevelActor* PackedLevelActor = World->SpawnActor<APackedLevelActor>(InActor->GetActorLocation(), InActor->GetActorRotation(), SpawnParams);	
	PackedLevelActor->SetWorldAsset(InActor->GetWorldAsset());
	ON_SCOPE_EXIT
	{
		InActor->GetWorld()->DestroyActor(PackedLevelActor);
	};

	if (!PackActor(PackedLevelActor, InActor))
	{
		return false;
	}

	bResult &= CreateOrUpdateBlueprintFromPacked(PackedLevelActor, InBlueprintAsset, bCheckoutAndSave, bPromptForSave);

	return bResult;
}

bool FPackedLevelActorBuilder::CreateOrUpdateBlueprintFromPacked(APackedLevelActor* InActor, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptToSave)
{
	UBlueprint* BP = nullptr;
	if (!InBlueprintAsset.IsNull())
	{
		BP = InBlueprintAsset.LoadSynchronous();
	}

	if (!BP)
	{
		int32 LastSlashIndex = 0;
		FString LongPackageName = InActor->GetWorldAsset().GetLongPackageName();
		LongPackageName.FindLastChar('/', LastSlashIndex);

		FString PackagePath = LongPackageName.Mid(0, LastSlashIndex == INDEX_NONE ? MAX_int32 : LastSlashIndex);
		FString AssetName = GetPackedBPPrefix() + InActor->GetWorldAsset().GetAssetName();
		const bool bCompile = false;

		FString AssetPath = PackagePath + AssetName + "." + AssetName;
		BP = CreatePackedLevelActorBlueprintWithDialog(TSoftObjectPtr<UBlueprint>(AssetPath), InActor->GetWorldAsset(), bCompile);
	}

	if (BP && BP->SimpleConstructionScript)
	{
		TArray<USCS_Node*> AllNodes = BP->SimpleConstructionScript->GetAllNodes();
		for (USCS_Node* Node : AllNodes)
		{
			BP->SimpleConstructionScript->RemoveNodeAndPromoteChildren(Node);
		}
	}
	else
	{
		return false;
	}

	BP->Modify();
	// Avoid running construction script while dragging an instance of that BP for performance reasons
	BP->bRunConstructionScriptOnDrag = false;
	FGuid NewVersion = FGuid::NewGuid();
	APackedLevelActor* CDO = CastChecked<APackedLevelActor>(BP->GeneratedClass->GetDefaultObject());
	auto PropagatePropertiesToActor = [InActor, NewVersion](APackedLevelActor* TargetActor)
	{
		TargetActor->Modify(false);
		TargetActor->SetWorldAsset(InActor->GetWorldAsset());
		TargetActor->PackedBPDependencies = InActor->PackedBPDependencies;
		TargetActor->SetPackedVersion(NewVersion);
		// match root component mobility to source actor
		USceneComponent* Root = TargetActor->GetRootComponent();
		Root->SetMobility(InActor->GetRootComponent()->Mobility);
	};
	PropagatePropertiesToActor(CDO);
		
	// Prep AddComponentsToBlueprintParam
	FKismetEditorUtilities::FAddComponentsToBlueprintParams AddCompToBPParams;
	AddCompToBPParams.HarvestMode = FKismetEditorUtilities::EAddComponentToBPHarvestMode::None;
	AddCompToBPParams.bKeepMobility = true;

	// Add Components
	TArray<UActorComponent*> PackedComponents;
	InActor->GetPackedComponents(PackedComponents);
	
	// To avoid any delta serialization happening on those generated components, we make them non editable.
	for (UActorComponent* PackedComponent : PackedComponents)
	{
		PackedComponent->bEditableWhenInherited = false;
	}

	FKismetEditorUtilities::AddComponentsToBlueprint(BP, PackedComponents, AddCompToBPParams);
	// If we are packing the actors BP then destroy packed components as they are now part of the BPs construction script
	if (UBlueprint* GeneratedBy = Cast<UBlueprint>(InActor->GetClass()->ClassGeneratedBy))
	{
		InActor->DestroyPackedComponents();
	}

	// Propagate properties before BP Compilation so that they are considered default (no delta)
	if (BP->GeneratedClass)
	{
		TArray<UObject*> ObjectsOfClass;
		GetObjectsOfClass(BP->GeneratedClass, ObjectsOfClass, true, RF_ClassDefaultObject | RF_ArchetypeObject, EInternalObjectFlags::Garbage);
		for (UObject* ObjectOfClass : ObjectsOfClass)
		{
			APackedLevelActor* PackedLevelActor = CastChecked<APackedLevelActor>(ObjectOfClass);
			PropagatePropertiesToActor(PackedLevelActor);
		}
	}
			
	// Synchronous compile
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection);

	if (bCheckoutAndSave)
	{
		const bool bCheckDirty = false;
		TArray<UPackage*> OutFailedPackages;
		FEditorFileUtils::PromptForCheckoutAndSave({ BP->GetPackage() }, bCheckDirty, bPromptToSave, &OutFailedPackages);

		return !OutFailedPackages.Num();
	}

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE