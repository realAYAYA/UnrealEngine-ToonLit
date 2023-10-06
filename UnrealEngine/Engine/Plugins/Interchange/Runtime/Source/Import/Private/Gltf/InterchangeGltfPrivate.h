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
	struct FAnimationPayloadData;
	struct FMeshPayloadData;

	namespace Gltf::Private
	{
		static float GltfUnitConversionMultiplier = 100.f;

		// Animation related functions
		bool GetTransformAnimationPayloadData(const FString& PayLoadKey, const GLTF::FAsset& GltfAsset, FAnimationPayloadData& OutPayloadData);
		bool GetMorphTargetAnimationPayloadData(const FString& PayLoadKey, const GLTF::FAsset& GltfAsset, FAnimationPayloadData& OutPayloadData);
		bool GetBakedAnimationTransformPayloadData(const FString& PayLoadKey, const GLTF::FAsset& GltfAsset, FAnimationPayloadData& PayloadData);
		// 

		// Mesh related functions
		int32 GetRootNodeIndex(const GLTF::FAsset& GltfAsset, const TArray<int32>& NodeIndices);

		bool GetSkeletalMeshDescriptionForPayLoadKey(const GLTF::FAsset& GltfAsset, const FString& PayLoadKey, const FTransform& MeshGlobalTransform,
			FMeshDescription& MeshDescription, TArray<FString>* OutJointUniqueNames);

		bool GetStaticMeshPayloadDataForPayLoadKey(const GLTF::FAsset& GltfAsset, const FString& PayLoadKey, const FTransform& MeshGlobalTransform
		, FMeshDescription& MeshDescription);
		//
	}
}