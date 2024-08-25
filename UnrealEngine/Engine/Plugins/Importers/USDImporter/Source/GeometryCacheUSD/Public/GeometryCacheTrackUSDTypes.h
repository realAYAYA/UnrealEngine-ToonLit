// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"
#include "UObject/WeakObjectPtr.h"

class UGeometryCacheTrackUsd;
struct FGeometryCacheMeshData;

typedef TFunction<bool(const TWeakObjectPtr<UGeometryCacheTrackUsd>, float Time, FGeometryCacheMeshData&)> FReadUsdMeshFunction;
