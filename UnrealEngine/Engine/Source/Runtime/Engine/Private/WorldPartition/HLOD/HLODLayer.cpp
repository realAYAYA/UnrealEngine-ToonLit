// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HLODLayer.cpp: UHLODLayer class implementation
=============================================================================*/

#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODLayer)

#if WITH_EDITOR
#include "Serialization/ArchiveCrc32.h"

#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescView.h"
#include "WorldPartition/HLOD/IWorldPartitionHLODUtilitiesModule.h"

#include "Modules/ModuleManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHLODLayer, Log, All);

UHLODLayer::UHLODLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsSpatiallyLoaded(true)
	, CellSize(25600)
	, LoadingRange(51200)
{
}

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

	const FName HLODLayerName = InActorDesc.GetHLODLayer();
	if (UHLODLayer* HLODLayer = HLODLayerName.IsNone() ? nullptr : Cast<UHLODLayer>(FSoftObjectPath(HLODLayerName.ToString()).TryLoad()))
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
		CurrentHLODLayer = Cast<UHLODLayer>(CurrentHLODLayer->GetParentLayer().LoadSynchronous());
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
}

FName UHLODLayer::GetRuntimeGridName(uint32 InLODLevel, int32 InCellSize, double InLoadingRange)
{
	return *FString::Format(TEXT("HLOD{0}_{1}m_{2}m"), { InLODLevel, int32(InCellSize * 0.01f), int32(InLoadingRange * 0.01f)});
}

FName UHLODLayer::GetRuntimeGrid(uint32 InHLODLevel) const
{
	return !IsSpatiallyLoaded() ? NAME_None : GetRuntimeGridName(InHLODLevel, CellSize, LoadingRange);
}

const TSoftObjectPtr<UHLODLayer>& UHLODLayer::GetParentLayer() const
{
	static const TSoftObjectPtr<UHLODLayer> NullLayer;
	return !IsSpatiallyLoaded() ? NullLayer : ParentLayer;
}

const void UHLODLayer::SetParentLayer(const TSoftObjectPtr<UHLODLayer>& InParentLayer)
{
	ParentLayer = InParentLayer;
}

#endif // WITH_EDITOR
