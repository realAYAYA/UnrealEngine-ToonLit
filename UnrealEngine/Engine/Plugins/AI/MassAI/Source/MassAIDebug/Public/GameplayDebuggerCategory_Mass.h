// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Required first for WITH_MASSGAMEPLAY_DEBUG
#include "MassCommonTypes.h"

#if WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG

#include "HAL/IConsoleManager.h"
#include "GameplayDebuggerCategory.h"

struct FMassEntityManager;
class UMassDebuggerSubsystem;
class APlayerController;
class AActor;

class FGameplayDebuggerCategory_Mass : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Mass();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;
	
	void SetCachedEntity(const FMassEntityHandle Entity, const FMassEntityManager& EntityManager);

	void OnToggleArchetypes() { bShowArchetypes = !bShowArchetypes; }
	void OnToggleShapes() { bShowShapes = !bShowShapes; }
	void OnToggleAgentFragments() { bShowAgentFragments = !bShowAgentFragments; }
	void OnPickEntity() { bPickEntity = true; }
	void OnToggleEntityDetails() { bShowEntityDetails = !bShowEntityDetails; }
	void OnToggleNearEntityOverview() { bShowNearEntityOverview = !bShowNearEntityOverview; }
	void OnToggleNearEntityAvoidance() { bShowNearEntityAvoidance = !bShowNearEntityAvoidance; }
	void OnToggleNearEntityPath() { bShowNearEntityPath = !bShowNearEntityPath; }
	void OnToggleDebugLocalEntityManager();
	
	void PickEntity(const FVector& ViewLocation, const FVector& ViewDirection, const UWorld& World, FMassEntityManager& EntityManager, const bool bLimitAngle = true);

	UE_DEPRECATED(5.3, "This flavor of PickEntity has been deprecated. Use the one getting ViewLocation and ViewDirection parameters instead.")
	void PickEntity(const APlayerController& OwnerPC, const UWorld& World, FMassEntityManager& EntityManager, const bool bLimitAngle = true);

protected:
	AActor* CachedDebugActor;
	FMassEntityHandle CachedEntity;
	bool bShowArchetypes;
	bool bShowShapes;
	bool bShowAgentFragments;
	bool bPickEntity;
	bool bShowEntityDetails;
	bool bShowNearEntityOverview;
	bool bShowNearEntityAvoidance;
	bool bShowNearEntityPath;
	bool bMarkEntityBeingDebugged;
	bool bDebugLocalEntityManager;
	int32 ToggleDebugLocalEntityManagerInputIndex = INDEX_NONE;

	struct FEntityDescription
	{
		FEntityDescription() = default;
		FEntityDescription(const float InScore, const FVector& InLocation, const FString& InDescription) : Score(InScore), Location(InLocation), Description(InDescription) {}

		float Score = 0.0f;
		FVector Location = FVector::ZeroVector;
		FString Description;
	};
	TArray<FEntityDescription> NearEntityDescriptions;

	TArray<FAutoConsoleCommand> ConsoleCommands;
};

#endif // WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG
