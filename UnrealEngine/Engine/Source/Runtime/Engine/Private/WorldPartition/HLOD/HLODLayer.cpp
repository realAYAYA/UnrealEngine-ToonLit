// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HLODLayer.cpp: UHLODLayer class implementation
=============================================================================*/

#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODActor.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/StringFormatArg.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilities.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescView.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODLayer)

DEFINE_LOG_CATEGORY_STATIC(LogHLODLayer, Log, All);

UHLODLayer::UHLODLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsSpatiallyLoaded(true)
	, CellSize(25600)
	, LoadingRange(51200)
	, HLODActorClass(AWorldPartitionHLOD::StaticClass())
{}

#if WITH_EDITOR
UHLODLayer* UHLODLayer::GetHLODLayer(const AActor* InActor)
{
	if (UHLODLayer* HLODLayer = InActor->GetHLODLayer())
	{
		return HLODLayer;
	}

	// Only fallback to the default HLODLayer for the first level of HLOD
	bool bIsHLOD0 = !InActor->IsA<AWorldPartitionHLOD>();
	if (bIsHLOD0) 
	{
		// Fallback to the world partition default HLOD layer
		if (UWorldPartition* WorldPartition = InActor->GetWorld()->GetWorldPartition())
		{
			return WorldPartition->GetDefaultHLODLayer();
		}
	}

	return nullptr;
}

UHLODLayer* UHLODLayer::GetHLODLayer(const FWorldPartitionActorDescView& InActorDesc, const UWorldPartition* InWorldPartition)
{
	check(InWorldPartition);

	const FSoftObjectPath HLODLayerPath = InActorDesc.GetHLODLayer();
	if (UHLODLayer* HLODLayer = Cast<UHLODLayer>(HLODLayerPath.TryLoad()))
	{
		return HLODLayer;
	}

	// Only fallback to the default HLODLayer for the first level of HLOD
	bool bIsHLOD0 = !InActorDesc.GetActorNativeClass()->IsChildOf<AWorldPartitionHLOD>();
	if (bIsHLOD0)
	{
		// Fallback to the world partition default HLOD layer
		return InWorldPartition->GetDefaultHLODLayer();
	}

	return nullptr;
}

UHLODLayer* UHLODLayer::GetHLODLayer(const FWorldPartitionActorDesc& InActorDesc, const UWorldPartition* InWorldPartition)
{
	return GetHLODLayer(FWorldPartitionActorDescView(&InActorDesc), InWorldPartition);
}

bool UHLODLayer::DoesRequireWarmup() const
{
	IWorldPartitionHLODUtilities* WPHLODUtilities = FModuleManager::Get().LoadModuleChecked<IWorldPartitionHLODUtilitiesModule>("WorldPartitionHLODUtilities").GetUtilities();
	if (WPHLODUtilities)
	{
		return WPHLODUtilities->GetHLODBuilderClass(this)->GetDefaultObject<UHLODBuilder>()->RequiresWarmup();
	}

	return false;
}

UHLODLayer* UHLODLayer::GetEngineDefaultHLODLayersSetup()
{
	UHLODLayer* Result = nullptr;

	if (FConfigFile* EngineConfig = GConfig->FindConfigFileWithBaseName(TEXT("Engine")))
	{
		FString DefaultHLODLayerName;
		if (EngineConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("DefaultWorldPartitionHLODLayer"), DefaultHLODLayerName))
		{
			FSoftObjectPath DefaultHLODLayerPath(*DefaultHLODLayerName);
			TSoftObjectPtr<UHLODLayer> EngineHLODLayerPath(DefaultHLODLayerPath);

			if (UHLODLayer* EngineHLODLayer = EngineHLODLayerPath.LoadSynchronous())
			{
				Result = EngineHLODLayer;
			}
		}
	}

	return Result;
}

UHLODLayer* UHLODLayer::DuplicateHLODLayersSetup(UHLODLayer* HLODLayer, const FString& DestinationPath, const FString& Prefix)
{
	UHLODLayer* Result = nullptr;

	UHLODLayer* LastHLODLayer = nullptr;
	UHLODLayer* CurrentHLODLayer = HLODLayer;

	while (CurrentHLODLayer)
	{
		const FString PackageName = DestinationPath + TEXT("_") + CurrentHLODLayer->GetName();
		UPackage* Package = CreatePackage(*PackageName);
		// In case Package already exists setting this flag will allow overwriting it
		Package->MarkAsFullyLoaded();

		FObjectDuplicationParameters ObjParameters(CurrentHLODLayer, Package);
		ObjParameters.DestName = FName(Prefix + TEXT("_") + CurrentHLODLayer->GetName());
		ObjParameters.ApplyFlags = RF_Public | RF_Standalone;

		UHLODLayer* NewHLODLayer = CastChecked<UHLODLayer>(StaticDuplicateObjectEx(ObjParameters));
		check(NewHLODLayer);

		if (LastHLODLayer)
		{
			LastHLODLayer->SetParentLayer(NewHLODLayer);
		}
		else
		{
			Result = NewHLODLayer;
		}

		LastHLODLayer = NewHLODLayer;
		CurrentHLODLayer = CurrentHLODLayer->GetParentLayer();
	}

	return Result;
}

void UHLODLayer::PostLoad()
{
	Super::PostLoad();

	IWorldPartitionHLODUtilities* WPHLODUtilities = FModuleManager::Get().LoadModuleChecked<IWorldPartitionHLODUtilitiesModule>("WorldPartitionHLODUtilities").GetUtilities();
	if (WPHLODUtilities)
	{
		const UClass* BuilderClass = WPHLODUtilities->GetHLODBuilderClass(this);
		const UClass* BuilderSettingsClass = BuilderClass ? BuilderClass->GetDefaultObject<UHLODBuilder>()->GetSettingsClass() : nullptr;

		if (!HLODBuilderSettings || (BuilderSettingsClass && !HLODBuilderSettings->IsA(BuilderSettingsClass)))
		{
			HLODBuilderSettings = WPHLODUtilities->CreateHLODBuilderSettings(this);
		}
	}

	if (bAlwaysLoaded_DEPRECATED)
	{
		bIsSpatiallyLoaded = false;
	}
}

#if WITH_EDITORONLY_DATA
void UHLODLayer::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/WorldPartitionHLODUtilities.HLODBuilderInstancingSettings")));
	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/WorldPartitionHLODUtilities.HLODBuilderMeshMerge")));
	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/WorldPartitionHLODUtilities.HLODBuilderMeshSimplify")));
	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/WorldPartitionHLODUtilities.HLODBuilderMeshApproximate")));
}
#endif

void UHLODLayer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : FName();

	IWorldPartitionHLODUtilities* WPHLODUtilities = FModuleManager::Get().LoadModuleChecked<IWorldPartitionHLODUtilitiesModule>("WorldPartitionHLODUtilities").GetUtilities();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHLODLayer, LayerType) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UHLODLayer, HLODBuilderClass))
	{
		HLODBuilderSettings = WPHLODUtilities->CreateHLODBuilderSettings(this);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UHLODLayer, ParentLayer))
	{
		TSet<const UHLODLayer*> VisitedHLODLayers;
		const UHLODLayer* CurHLODLayer = ParentLayer;
		while (CurHLODLayer)
		{
			bool bHLODLayerWasAlreadyInSet;
			VisitedHLODLayers.Add(CurHLODLayer, &bHLODLayerWasAlreadyInSet);
			if (bHLODLayerWasAlreadyInSet)
			{
				UE_LOG(LogHLODLayer, Warning, TEXT("Circular HLOD parent chain detedted: HLODLayer=%s ParentLayer=%s"), *GetName(), *ParentLayer->GetName());
				ParentLayer = nullptr;
				break;
			}
			CurHLODLayer = CurHLODLayer->GetParentLayer();
		}
	}
}

FName UHLODLayer::GetRuntimeGridName(uint32 InLODLevel, int32 InCellSize, double InLoadingRange)
{
	return *FString::Format(TEXT("HLOD{0}_{1}m_{2}m"), { InLODLevel, int32(InCellSize * 0.01f), int32(InLoadingRange * 0.01f)});
}

FName UHLODLayer::GetRuntimeGrid(uint32 InHLODLevel) const
{
	return !IsSpatiallyLoaded() ? NAME_None : GetRuntimeGridName(InHLODLevel, CellSize, LoadingRange);
}
#endif // WITH_EDITOR
