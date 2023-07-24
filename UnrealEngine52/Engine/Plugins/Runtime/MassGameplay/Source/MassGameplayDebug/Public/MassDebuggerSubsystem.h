// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassGameplayDebugTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassDebuggerSubsystem.generated.h"


class UMassDebugVisualizationComponent;
class AMassDebugVisualizer;

UCLASS()
class MASSGAMEPLAYDEBUG_API UMassDebuggerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	struct FShapeDesc
	{
		FVector Location = {}; // no init on purpose, value will come from constructor
		float Size = {};
		FShapeDesc(const FVector InLocation, const float InSize) : Location(InLocation), Size(InSize) {}
	};

	// Methods to optimize the collection of data to only when category is enabled
	bool IsCollectingData() const { return bCollectingData; }
	void SetCollectingData() { bCollectingData = true; }
	void DataCollected() { bCollectingData = false; }

	void AddShape(EMassEntityDebugShape Shape, FVector Location, float Size) { Shapes[uint8(Shape)].Add(FShapeDesc(Location, Size)); }
	const TArray<FShapeDesc>* GetShapes() const { return Shapes; }

	FMassEntityHandle GetSelectedEntity() const { return SelectedEntity; }
	void SetSelectedEntity(const FMassEntityHandle InSelectedEntity);

	void AppendSelectedEntityInfo(const FString& Info);
	const FString& GetSelectedEntityInfo() const { return SelectedEntityDetails; }
	
	/** Fetches the UMassDebugVisualizationComponent owned by lazily created DebugVisualizer */
	UMassDebugVisualizationComponent* GetVisualizationComponent();

#if WITH_EDITORONLY_DATA
	AMassDebugVisualizer& GetOrSpawnDebugVisualizer(UWorld& InWorld);
#endif

protected:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem END
	
	void OnProcessingPhaseStarted(const float DeltaSeconds);
	void PreTickProcessors();
	void OnEntitySelected(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle);

protected:
	bool bCollectingData = false;

	TArray<FShapeDesc> Shapes[uint8(EMassEntityDebugShape::MAX)];
	TArray<FMassEntityHandle> Entities;
	TArray<FVector> Locations;
	FMassEntityHandle SelectedEntity;
	FString SelectedEntityDetails;

	UPROPERTY(Transient)
	TObjectPtr<UMassDebugVisualizationComponent> VisualizationComponent;

	UPROPERTY(Transient)
	TObjectPtr<AMassDebugVisualizer> DebugVisualizer;

	FDelegateHandle OnEntitySelectedHandle;
};


template<>
struct TMassExternalSubsystemTraits<UMassDebuggerSubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};