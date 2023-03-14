// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

struct FMeshDescription;

namespace GLTF
{
	struct FAsset;
}

namespace UE::Interchange
{
	struct FAnimationCurvePayloadData;
	struct FAnimationBakeTransformPayloadData;
	struct FStaticMeshPayloadData;

	namespace Gltf::Private
	{
		static float GltfUnitConversionMultiplier = 100.f;

		// Animation related functions
		bool GetAnimationTransformPayloadData(const FString& PayLoadKey, const GLTF::FAsset& GltfAsset, FAnimationCurvePayloadData& OutPayloadData);
		bool GetBakedAnimationTransformPayloadData(const FString& PayLoadKey, const GLTF::FAsset& GltfAsset, FAnimationBakeTransformPayloadData& PayloadData);
		// 

		// Mesh related functions
		int32 GetRootNodeIndex(const GLTF::FAsset& GltfAsset, const TArray<int32>& NodeIndices);

		bool GetSkeletalMeshDescriptionForPayLoadKey(const GLTF::FAsset& GltfAsset, const FString& PayLoadKey,
			FMeshDescription& MeshDescription, TArray<FString>* OutJointUniqueNames);

		bool GetStaticMeshPayloadDataForPayLoadKey(const GLTF::FAsset& GltfAsset, const FString& PayLoadKey, FStaticMeshPayloadData& StaticMeshPayloadData);
		//
	}
}