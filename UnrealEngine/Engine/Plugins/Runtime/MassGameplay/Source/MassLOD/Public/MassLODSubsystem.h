// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassCommonTypes.h"
#include "MassProcessor.h"
#include "IndexedHandle.h"
#include "MassLODTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassExternalSubsystemTraits.h"
#include "MassLODSubsystem.generated.h"

class UMassLODSubsystem;

/*
 * Handle that lets you reference the concept of a viewer
 */
USTRUCT()
struct MASSLOD_API FMassViewerHandle : public FIndexedHandleBase
{
	GENERATED_BODY()

	friend class UMassLODSubsystem;
};

USTRUCT()
struct FViewerInfo
{
	GENERATED_BODY()

	UPROPERTY(transient)
	TObjectPtr<APlayerController> PlayerController = nullptr;
	
	FName StreamingSourceName;

#if WITH_EDITOR
	int8 EditorViewportClientIndex = INDEX_NONE;
#endif // WITH_EDITOR

	FMassViewerHandle Handle;
	uint32 HashValue = 0;

	FVector Location;
	FRotator Rotation;
	float FOV = 90.0f;
	float AspectRatio = 16.0f / 9.0f;

	bool bEnabled = true;

	void Reset();

	bool IsLocal() const;
};

UE_DEPRECATED(5.3, "FOnViewerAdded is deprecated. Use UMassLODSubsystem::FOnViewerAdded instead.")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnViewerAdded, FMassViewerHandle ViewerHandle, APlayerController* PlayerController, FName StreamingSourceName);
UE_DEPRECATED(5.3, "FOnViewerRemoved is deprecated. Use UMassLODSubsystem::FOnViewerRemoved instead.")
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnViewerRemoved, FMassViewerHandle ViewerHandle, APlayerController* PlayerController, FName StreamingSourceName);

/*
 * Manager responsible to manage and synchronized available viewers
 */
UCLASS()
class MASSLOD_API UMassLODSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewerAdded, const FViewerInfo& ViewerInfo);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnViewerRemoved, const FViewerInfo& ViewerInfo);

public:
	/** Checks the validity of a viewer handle */
	bool IsValidViewer(const FMassViewerHandle& ViewerHandle) const { return GetValidViewerIdx(ViewerHandle) != INDEX_NONE; }

	/** Returns the index of the viewer if valid, otherwise INDEX_NONE is return */
	int32 GetValidViewerIdx(const FMassViewerHandle& ViewerHandle) const;

	/** Returns the array of viewers */
	const TArray<FViewerInfo>& GetViewers() const { return Viewers; }

	/** Synchronize the viewers if not done this frame and returns the updated array */
	const TArray<FViewerInfo>& GetSynchronizedViewers();

	/** Returns viewer handle from the PlayerController pointer */
	FMassViewerHandle GetViewerHandleFromPlayerController(const APlayerController* PlayerController) const;

	/** Returns viewer handle from the streaming source name */
	FMassViewerHandle GetViewerHandleFromStreamingSource(const FName StreamingSourceName) const;

	/** Returns PlayerController pointer from the viewer handle */
	APlayerController* GetPlayerControllerFromViewerHandle(const FMassViewerHandle& ViewerHandle) const;

	/** Returns the delegate called when new viewer are added to the list */
	FOnViewerAdded& GetOnViewerAddedDelegate() { return OnViewerAddedDelegate;  }

	/** Returns the delegate called when viewer are removed from the list */
	FOnViewerRemoved& GetOnViewerRemovedDelegate() { return OnViewerRemovedDelegate; }

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	/** Called at the start of the PrePhysics mass processing phase and calls SynchronizeViewers */ 
	void OnPrePhysicsPhaseStarted(float DeltaTime);

	/** Synchronizes the viewers from the engine PlayerController list */
	void SynchronizeViewers();

	UE_DEPRECATED(5.3, "AddViewer has been deprecated. Use AddPlayerViewer or AddStreamingSourceViewer instead")
	void AddViewer(APlayerController* PlayerController, FName StreamingSourceName = NAME_None);

	/** Adds the given player as a viewer to the list and sends notification about addition */
	void AddPlayerViewer(APlayerController& PlayerController);

	/** Adds the given streaming source as a viewer to the list and sends notification about addition */
	void AddStreamingSourceViewer(const FName StreamingSourceName);

#if WITH_EDITOR
	/** Adds the editor viewport client (identified via an index) as a viewer to the list and sends notification about addition */
	void AddEditorViewer(const int32 HashValue, const int32 ClientIndex);
#endif // WITH_EDITOR

	/** Removes a viewer to the list and send notification about removal */
	void RemoveViewer(const FMassViewerHandle& ViewerHandle);

	/** Returns the next new viewer serial number */
	uint32 GetNextViewerSerialNumber() { return ViewerSerialNumberCounter++; }

	/** Player controller EndPlay callback, removing viewers from the list */
	UFUNCTION()
	void OnPlayerControllerEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

private:
	/** Removes a viewer to the list and send notification about removal */
	void RemoveViewerInternal(const FMassViewerHandle& ViewerHandle);

	/** The actual array of viewer's information*/
	UPROPERTY(Transient)
	TArray<FViewerInfo> Viewers;

	/** The map that do reverse look up to get ViewerHandle */
	UPROPERTY(Transient)
	TMap<uint32, FMassViewerHandle> ViewerMap;

	uint64 LastSynchronizedFrame = 0;

	/** Viewer serial number counter */
	uint32 ViewerSerialNumberCounter = 0;

#if WITH_EDITOR
	bool bUseEditorLevelViewports = false;
	bool bIgnorePlayerControllersDueToSimulation = false;
#endif // WITH_EDITOR

	/** Free list of indices in the sparse viewer array */
	TArray<int32> ViewerFreeIndices;

	/** Delegates to notify anyone who needs to know about viewer changes */
	FOnViewerAdded OnViewerAddedDelegate;
	FOnViewerRemoved OnViewerRemovedDelegate;

};

template<>
struct TMassExternalSubsystemTraits<UMassLODSubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};
