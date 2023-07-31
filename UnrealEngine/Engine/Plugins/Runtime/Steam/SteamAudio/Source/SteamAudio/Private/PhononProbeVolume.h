//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "PhononCommon.h"
#include "GameFramework/Volume.h"
#include "PhononProbeVolume.generated.h"

UENUM()
enum class EPhononProbePlacementStrategy : uint8
{
	// Places a single probe at the centroid of the volume.
	CENTROID = 0 UMETA(DisplayName = "Centroid"),
	// Places uniformly spaced probes along the floor at a specified height.
	UNIFORM_FLOOR = 1 UMETA(DisplayName = "Uniform Floor")
};

UENUM()
enum class EPhononProbeMobility : uint8
{
	// Static probes remain fixed at runtime.
	STATIC = 0 UMETA(DisplayName = "Static"),
	// Dynamic probes inherit this volume's offset at runtime.
	DYNAMIC = 1 UMETA(DisplayName = "Dynamic")
};

USTRUCT()
struct FBakedDataInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	int32 Size;
};

inline bool operator==(const FBakedDataInfo& Lhs, const FBakedDataInfo& Rhs)
{
	return Lhs.Name == Rhs.Name && Lhs.Size == Rhs.Size;
}

inline bool operator<(const FBakedDataInfo& Lhs, const FBakedDataInfo& Rhs)
{
	return Lhs.Name.LexicalLess(Rhs.Name);
}

/**
 * Phonon Probe volumes generate a set of probes at which acoustic information will be sampled
 * at bake time.
 */
UCLASS(HideCategories = (Actor, Advanced, Attachment, Collision), meta = (BlueprintSpawnableComponent))
class STEAMAUDIO_API APhononProbeVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITOR

	/** Creates a probe box with the current probe placement parameters and writes it to disk. Updates ProbeDataSize for display. */
	void PlaceProbes(IPLhandle PhononScene, IPLProbePlacementProgressCallback ProbePlacementCallback, TArray<IPLSphere>& ProbeSpheres);

	/** Writes probe box out to disk. Creates a new probe batch and writes it to disk. Updates ProbeDataSize for display. */
	void UpdateProbeData(IPLhandle ProbeBox);

	/** Writes a given probe box out to the EditorOnly folder. Returns file size, or 0 if no file was written. */
	int32 SaveProbeBoxToDisk(IPLhandle ProbeBox);

	/** Writes a given probe batch out to the Runtime folder. Returns file size, or 0 if no file was written. */
	int32 SaveProbeBatchToDisk(IPLhandle ProbeBatch);

	virtual bool CanEditChange(const FProperty* InProperty) const override;

#endif

	/** Returns size of probe box file on disk, or 0 if none exists. */
	int32 GetProbeBoxDataSize() const;

	/** Returns size of probe batch file on disk, or 0 if none exists. */
	int32 GetProbeBatchDataSize() const;

	/** Returns data size for the specified source from member data. */
	int32 GetDataSizeForSource(const FName& UniqueIdentifier) const;

	/** Composes a probe box filename for this actor. TODO: ensure uniqueness. */
	FString GetProbeBoxFileName() const;

	/** Composes a probe batch filename for this actor. TODO: ensure uniqueness. */
	FString GetProbeBatchFileName() const;

	bool LoadProbeBoxFromDisk(IPLhandle* ProbeBox) const;

	bool LoadProbeBatchFromDisk(IPLhandle* ProbeBatch) const;

	class UPhononProbeComponent* GetPhononProbeComponent() const { return PhononProbeComponent; }

	// Method by which probes are placed within the volume.
	UPROPERTY(EditAnywhere, Category = ProbeGeneration)
	EPhononProbePlacementStrategy PlacementStrategy;

	// How far apart to place probes.
	UPROPERTY(EditAnywhere, Category = ProbeGeneration, meta = (ClampMin = "0.0", ClampMax = "5000.0", UIMin = "0.0", UIMax = "5000.0"))
	float HorizontalSpacing;

	// How high above the floor to place probes.
	UPROPERTY(EditAnywhere, Category = ProbeGeneration, meta = (ClampMin = "0.0", ClampMax = "5000.0", UIMin = "0.0", UIMax = "5000.0"))
	float HeightAboveFloor;

	// Number of probes contained in this probe volume.
	UPROPERTY(VisibleAnywhere, Category = ProbeVolumeStatistics, meta = (DisplayName = "Probe Points"))
	int32 NumProbes;

	// Size of probe data in bytes.
	UPROPERTY(VisibleAnywhere, Category = ProbeVolumeStatistics, meta = (DisplayName = "Probe Data Size"))
	int32 ProbeDataSize;

	// Useful information for each baked source.
	UPROPERTY(VisibleAnywhere, Category = ProbeVolumeStatistics, meta = (DisplayName = "Detailed Statistics"))
	TArray<FBakedDataInfo> BakedDataInfo;

	// Component used for visualization.
	UPROPERTY()
	TObjectPtr<class UPhononProbeComponent> PhononProbeComponent;

	// Current filename where probe box data is stored. Used to maintain connection if volume is renamed.
	UPROPERTY()
	FString ProbeBoxFileName;

	// Current filename where probe batch data is stored. Used to maintain connection if volume is renamed.
	UPROPERTY()
	FString ProbeBatchFileName;
};
