// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCacheStreamBase.h"
#include "GeometryCacheTrackUSDTypes.h"

class UGeometryCacheTrackUsd;

class FGeometryCacheUsdStream : public FGeometryCacheStreamBase
{
public:
	FGeometryCacheUsdStream(UGeometryCacheTrackUsd* InUsdTrack, FReadUsdMeshFunction InReadFunc);

	//~ Begin IGeometryCacheStream Interface
	virtual bool GetFrameData(int32 FrameIndex, FGeometryCacheMeshData& OutMeshData) override;
	virtual void UpdateRequestStatus( TArray<int32>& OutFramesCompleted ) override;

protected:
	virtual void GetMeshData(int32 FrameIndex, int32 ConcurrencyIndex, FGeometryCacheMeshData& OutMeshData) override;
	//~ End IGeometryCacheStream Interface

	UGeometryCacheTrackUsd* UsdTrack;
	FReadUsdMeshFunction ReadFunc;
};

