// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "Templates/SharedPointer.h"

namespace UE::PoseSearch
{

class FDebuggerDatabaseRowData : public TSharedFromThis<FDebuggerDatabaseRowData>
{
public:
	FDebuggerDatabaseRowData() = default;
	
	ESearchIndexAssetType AssetType = ESearchIndexAssetType::Invalid;
	int32 PoseIdx = 0;
	TWeakObjectPtr<const UPoseSearchDatabase> SourceDatabase = nullptr;
	EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None;
	FString DatabaseName = "";
	FString DatabasePath = "";
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
	TArray<float> CostBreakdowns;
	TArray<FLinearColor> CostBreakdownsColors;
	TArray<float> CostVector;
};

} // namespace UE::PoseSearch
