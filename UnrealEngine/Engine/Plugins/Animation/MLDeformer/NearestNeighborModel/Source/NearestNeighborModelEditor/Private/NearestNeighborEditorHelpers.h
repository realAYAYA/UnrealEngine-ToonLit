// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "NearestNeighborEditorHelpers.generated.h"

class UAnimSequence;
UCLASS(Blueprintable)
class NEARESTNEIGHBORMODELEDITOR_API UNearestNeighborAnimStream : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Python")
	void Init(USkeleton* InSkeleton);

	UFUNCTION(BlueprintPure, Category = "Python")
	bool IsValid() const;

	UFUNCTION(BlueprintCallable, Category = "Python")
	bool AppendFrames(const UAnimSequence* Anim, TArray<int32> Frames);

	UFUNCTION(BlueprintPure, Category = "Python")
	bool ToAnim(UAnimSequence* OutAnim) const;

private:
	TObjectPtr<USkeleton> Skeleton;
	TArray<TArray<FVector3f>> PosKeys;
	TArray<TArray<FQuat4f>> RotKeys;
	TArray<TArray<FVector3f>> ScaleKeys;
};

class UGeometryCache;
UCLASS(Blueprintable)
class NEARESTNEIGHBORMODELEDITOR_API UNearestNeighborGeometryCacheStream : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Python")
	void Init(const UGeometryCache* InTemplateCache);

	UFUNCTION(BlueprintPure, Category = "Python")
	bool IsValid() const;

	UFUNCTION(BlueprintCallable, Category = "Python")
	bool AppendFrames(const UGeometryCache* Cache, TArray<int32> Frames);

	UFUNCTION(BlueprintPure, Category = "Python")
	bool ToGeometryCache(UGeometryCache* OutCache);

private:
	const UGeometryCache* TemplateCache = nullptr;
	int32 TemplateNumTracks = 0;
	TArray<int32> TemplateTrackNumVertices;
	TArray<TArray<TArray<FVector3f>>> TrackToFrameToPositions;
};