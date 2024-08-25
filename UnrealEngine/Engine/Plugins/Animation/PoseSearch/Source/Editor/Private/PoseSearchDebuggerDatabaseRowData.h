// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "Templates/SharedPointer.h"

namespace UE::PoseSearch
{

class FDebuggerDatabaseSharedData : public TSharedFromThis<FDebuggerDatabaseSharedData>
{
public:
	TWeakObjectPtr<const UPoseSearchDatabase> SourceDatabase;
	FString DatabaseName = "";
	FString DatabasePath = "";
	TAlignedArray<float> QueryVector;
	TAlignedArray<float> PCAQueryVector;
};

class FDebuggerDatabaseRowData : public TSharedFromThis<FDebuggerDatabaseRowData>
{
public:
	explicit FDebuggerDatabaseRowData(TSharedRef<FDebuggerDatabaseSharedData> InSharedData) : SharedData(InSharedData) {}

	int32 PoseIdx = 0;
	EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None;
	FString AssetName = "";
	FString AssetPath = "";
	int32 DbAssetIdx = 0;
	int32 AnimFrame = 0;
	float AnimPercentage = 0.0f;
	float AssetTime = 0.0f;
	bool bMirrored = false;
	bool bLooping = false;
	FVector BlendParameters = FVector::Zero();
	FPoseSearchCost PoseCost;
	FLinearColor CostColor = FLinearColor::White;
	float PosePCACost = 0.f;
	FLinearColor PCACostColor = FLinearColor::White;
	TArray<float> CostBreakdowns;
	TArray<FLinearColor> CostBreakdownsColors;
	TArray<float> CostVector;

	TSharedRef<FDebuggerDatabaseSharedData> SharedData;
};

} // namespace UE::PoseSearch
