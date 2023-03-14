// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCacheTrack.h"
#include "GeometryCacheMeshData.h"
#include "GeometryCacheTrackUSDTypes.h"
#include "GeometryCacheUSDStream.h"
#include "UsdWrappers/UsdStage.h"

#include "GeometryCacheTrackUSD.generated.h"

/** GeometryCacheTrack for querying USD */
UCLASS(collapsecategories, hidecategories = Object, BlueprintType, config = Engine)
class GEOMETRYCACHEUSD_API UGeometryCacheTrackUsd : public UGeometryCacheTrack
{
	GENERATED_BODY()

	UGeometryCacheTrackUsd();

public:
	void Initialize(
		const UE::FUsdStage& InStage,
		const FString& InPrimPath,
		const FName& InRenderContext,
		const TMap< FString, TMap< FString, int32 > >& InMaterialToPrimvarToUVIndex,
		int32 InStartFrameIndex,
		int32 InEndFrameIndex,
		FReadUsdMeshFunction InReadFunc
	);

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	//~ Begin UGeometryCacheTrack Interface.
	virtual const bool UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData) override;
	virtual const bool UpdateBoundsData(const float Time, const bool bLooping, const bool bIsPlayingBackward, int32& InOutBoundsSampleIndex, FBox& OutBounds) override;
	virtual const FGeometryCacheTrackSampleInfo& GetSampleInfo(float Time, const bool bLooping) override;
	virtual bool GetMeshDataAtTime(float Time, FGeometryCacheMeshData& OutMeshData) override;
	virtual void UpdateTime(float Time, bool bLooping) override;
	//~ End UGeometryCacheTrack Interface.

	const int32 FindSampleIndexFromTime(const float Time, const bool bLooping) const;

	int32 GetStartFrameIndex() const { return StartFrameIndex; }
	int32 GetEndFrameIndex() const { return EndFrameIndex;  }

	bool GetMeshData(int32 SampleIndex, FGeometryCacheMeshData& OutMeshData);

	// Upgrades our CurrentStageWeak into CurrentStagePinned, or re-opens the stage if its stale. Returns whether the stage was successfully opened or not
	bool LoadUsdStage();

	// Discards our CurrentStagePinned to release the stage
	void UnloadUsdStage();

public:
	FName RenderContext;
	int32 StartFrameIndex;
	int32 EndFrameIndex;
	TMap< FString, TMap< FString, int32 > > MaterialToPrimvarToUVIndex;

	FString PrimPath;

	UE::FUsdStage CurrentStagePinned;
	UE::FUsdStageWeak CurrentStageWeak;
	FString StageRootLayerPath;

private:
	FGeometryCacheMeshData MeshData;
	TArray<FGeometryCacheTrackSampleInfo> SampleInfos;

	TUniquePtr<FGeometryCacheUsdStream> UsdStream;
};
