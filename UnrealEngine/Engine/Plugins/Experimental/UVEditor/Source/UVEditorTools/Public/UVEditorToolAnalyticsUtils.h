// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineAnalytics.h"
#include "ToolTargets/UVEditorToolMeshInput.h"

namespace UE
{
namespace Geometry
{
	
namespace UVEditorAnalytics
{
	
template <typename EnumType>
FAnalyticsEventAttribute AnalyticsEventAttributeEnum(const FString& AttributeName, EnumType EnumValue)
{
	static_assert(TIsEnum<EnumType>::Value, "");
	const FString EnumValueName = StaticEnum<EnumType>()->GetNameStringByValue(static_cast<int64>(EnumValue));
	return FAnalyticsEventAttribute(AttributeName, EnumValueName);
}

static const FString UVEditorAnalyticsPrefix(TEXT("Editor.Usage.UVEditor"));

FORCEINLINE FString UVEditorAnalyticsEventName(const FString& ToolEventName)
{
	return FString::Printf(TEXT("%s.%s"), *UVEditorAnalyticsPrefix, *ToolEventName);
}

struct UVEDITORTOOLS_API FTargetAnalytics
{
	int64 AllAssetsNumVertices = 0;
	int64 AllAssetsNumTriangles = 0;
	int64 AllAssetsNumUVLayers = 0;
	
	TArray<int32> PerAssetNumVertices;
	TArray<int32> PerAssetNumTriangles;
	TArray<int32> PerAssetNumUVLayers;
	TArray<int32> PerAssetActiveUVChannelIndex;
	TArray<int32> PerAssetActiveUVChannelNumVertices;
	TArray<int32> PerAssetActiveUVChannelNumTriangles;

	void AppendToAttributes(TArray<FAnalyticsEventAttribute>& Attributes, FString Prefix = "") const;
};

UVEDITORTOOLS_API FTargetAnalytics CollectTargetAnalytics(const TArray<TObjectPtr<UUVEditorToolMeshInput>>& Targets);
	
} // end namespace UVEditorAnalytics
	
} // end namespace Geometry
} // end namespace UE
