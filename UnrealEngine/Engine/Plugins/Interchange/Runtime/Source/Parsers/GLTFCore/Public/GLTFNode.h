// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GLTFMaterial.h"
#include "GLTFMesh.h"
#include "GLTFTexture.h"

namespace GLTF { struct FAccessor; }

namespace GLTF
{
	struct GLTFCORE_API FNode
	{
		enum class EType
		{
			None,
			Transform,
			Joint,
			Mesh,
			MeshSkinned,
			Camera,
			Light
		};

		FString       Name;
		FTransform    Transform;
		TArray<int32> Children;  // each is an index into FAsset::Nodes
		EType         Type;

		int32 MeshIndex;  // index into FAsset::Meshes

		// Skindex is the skin used by the mesh at this node.
		// It's not the joints belonging *to* a skin
		int32 Skindex;      // index into FAsset::Skins
		int32 CameraIndex;  // index into FAsset::Cameras
		int32 LightIndex;   // index into FAsset::Lights

		int32 Index;		// index of FNode in GLTFAsset.Nodes array.
		int32 ParentIndex;
		int32 RootJointIndex; //only valid if node is of Joint Type

		TArray<float> MorphTargetWeights; //for the instantiated mesh with morph targets.

		FString UniqueId; //will be generated in FAsset::GenerateNames

		TMap<int, FTransform> SkinIndexToGlobalInverseBindTransform;
		TMap<int, FTransform> SkinIndexToLocalBindPose; //bind pose would be CurrentNode.GlobalInverseBindTransform.Inverse() * ParentNode.GlobalInverseBindTransform
		bool bHasLocalBindPose;
		FTransform LocalBindPose;	// First Skin that's using the joint will fill the LocalBindPose.
									//	Edge case Scenario which is currently not supported:
									//	Where multiple skins use the same Joint. Currently expected bad outcome if the different skins have different inversebindmatrices on the joint.

		FNode()
		    : Type(EType::None)
		    , MeshIndex(INDEX_NONE)
		    , Skindex(INDEX_NONE)
		    , CameraIndex(INDEX_NONE)
		    , LightIndex(INDEX_NONE)
			, Index(INDEX_NONE)
			, ParentIndex(INDEX_NONE)
			, RootJointIndex(INDEX_NONE)
			, bHasLocalBindPose(false)
		{
		}
	};

	struct GLTFCORE_API FCamera
	{
		struct FPerspective
		{
			// vertical field of view in radians
			float Fov;
			// aspect ratio of the field of view
			float AspectRatio;
		};
		struct FOrthographic
		{
			// horizontal magnification of the view
			float XMagnification;
			// vertical magnification of the view
			float YMagnification;
		};

		const FNode& Node;
		FString      Name;
		union {
			FOrthographic Orthographic;
			FPerspective  Perspective;
		};
		float ZNear;
		float ZFar;
		bool  bIsPerspective;
		FString UniqueId; //will be generated in FAsset::GenerateNames

		FCamera(const FNode& Node)
		    : Node(Node)
		    , ZNear(0.f)
		    , bIsPerspective(true)
		{
			Perspective.Fov         = 0.f;
			Perspective.AspectRatio = 1.f;
		}
	};

	struct GLTFCORE_API FLight
	{
		enum class EType
		{
			Directional,
			Point,
			Spot
		};

		struct FSpot
		{
			float InnerConeAngle;
			float OuterConeAngle;

			FSpot()
			    : InnerConeAngle(0.f)
			    , OuterConeAngle(PI / 4.f)
			{
			}
		};

		const FNode* Node;
		FString      Name;
		EType        Type;
		FVector      Color;
		float        Intensity;
		// Must be > 0. When undefined, range is assumed to be infinite.
		float Range;
		FSpot Spot;

		FString UniqueId; //will be generated in FAsset::GenerateNames

		FLight(const FNode* Node)
		    : Node(Node)
		    , Color(1.f)
		    , Intensity(1.f)
		    , Range(1e+20)
		{
		}
	};

	struct GLTFCORE_API FSkinInfo
	{
		const FAccessor& InverseBindMatrices;
		FString          Name;
		TArray<int32>    Joints;    // each is an index into FAsset::Nodes
		int32            Skeleton;  // root node, index into FAsset::Nodes

		FString          UniqueId; //will be generated in FAsset::GenerateNames

		FSkinInfo(const FAccessor& InverseBindMatrices)
		    : InverseBindMatrices(InverseBindMatrices)
		    , Skeleton(INDEX_NONE)
		{
		}
	};

}  // namespace GLTF
