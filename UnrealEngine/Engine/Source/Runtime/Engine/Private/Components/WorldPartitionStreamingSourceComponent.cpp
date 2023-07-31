// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Math/Color.h"
#include "Math/RandomStream.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionStreamingSourceComponent)

UWorldPartitionStreamingSourceComponent::UWorldPartitionStreamingSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, DefaultVisualizerLoadingRange(10000.f)
#endif
	, TargetGrid(NAME_None)
	, DebugColor(FColor::MakeRedToGreenColorFromScalar(FRandomStream(GetFName()).GetFraction()))
	, Priority(EStreamingSourcePriority::Normal)
	, bStreamingSourceEnabled(true)
	, TargetState(EStreamingSourceTargetState::Activated)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldPartitionStreamingSourceComponent::OnRegister()
{
	Super::OnRegister();

	UWorld* World = GetWorld();

#if WITH_EDITOR
	if (!World->IsGameWorld())
	{
		return;
	}
#endif

	UWorldPartitionSubsystem* WorldPartitionSubsystem = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
	check(WorldPartitionSubsystem);
	WorldPartitionSubsystem->RegisterStreamingSourceProvider(this);
}

void UWorldPartitionStreamingSourceComponent::OnUnregister()
{
	Super::OnUnregister();

	UWorld* World = GetWorld();

#if WITH_EDITOR
	if (!World->IsGameWorld())
	{
		return;
	}
#endif

	UWorldPartitionSubsystem* WorldPartitionSubsystem = GetWorld()->GetSubsystem<UWorldPartitionSubsystem>();
	check(WorldPartitionSubsystem);
	verify(WorldPartitionSubsystem->UnregisterStreamingSourceProvider(this));
}

bool UWorldPartitionStreamingSourceComponent::GetStreamingSource(FWorldPartitionStreamingSource& OutStreamingSource) const
{
	if (bStreamingSourceEnabled)
	{
		AActor* Actor = GetOwner();

		OutStreamingSource.Name = *Actor->GetActorNameOrLabel();
		OutStreamingSource.Location = Actor->GetActorLocation();
		OutStreamingSource.Rotation = Actor->GetActorRotation();
		OutStreamingSource.TargetState = TargetState;
		OutStreamingSource.DebugColor = DebugColor;
		OutStreamingSource.TargetGrid = TargetGrid;
		OutStreamingSource.TargetHLODLayer = TargetHLODLayer;
		OutStreamingSource.Shapes = Shapes;
		OutStreamingSource.Priority = Priority;
		return true;
	}
	return false;
}

bool UWorldPartitionStreamingSourceComponent::IsStreamingCompleted() const
{
	if (UWorldPartitionSubsystem* WorldPartitionSubsystem = UWorld::GetSubsystem<UWorldPartitionSubsystem>(GetWorld()))
	{
		return WorldPartitionSubsystem->IsStreamingCompleted(this);
	}
	return true;
}

void UWorldPartitionStreamingSourceComponent::DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) const
{
#if WITH_EDITOR
	AActor* Actor = GetOwner();
	FStreamingSourceShapeHelper::ForEachShape(DefaultVisualizerLoadingRange, DefaultVisualizerLoadingRange, /*bInProjectIn2D*/ false, Actor->GetActorLocation(), Actor->GetActorRotation(), Shapes, [&PDI](const FSphericalSector& Shape)
	{
		if (Shape.IsSphere())
		{
			DrawWireSphere(PDI, Shape.GetCenter(), FColor::White, Shape.GetRadius(), 32, SDPG_World, 1.0, 0, true);
		}
		else
		{
			TArray<TPair<FVector, FVector>> Lines = Shape.BuildDebugMesh();
			for (const auto& Line : Lines)
			{
				PDI->DrawLine(Line.Key, Line.Value, FColor::White, SDPG_World, 1.0, 0, true);
			};
		}
	});
#endif
}

#if WITH_EDITOR
bool UWorldPartitionStreamingSourceComponent::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty && InProperty->GetName() == GET_MEMBER_NAME_STRING_CHECKED(UWorldPartitionStreamingSourceComponent, TargetGrid))
	{
		return TargetHLODLayer == nullptr;
	}
	return Super::CanEditChange(InProperty);
}
#endif

