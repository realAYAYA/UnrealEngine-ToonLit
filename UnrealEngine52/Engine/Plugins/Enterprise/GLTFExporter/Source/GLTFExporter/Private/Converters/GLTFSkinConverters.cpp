// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFSkinConverters.h"
#include "Utilities/GLTFCoreUtilities.h"
#include "Converters/GLTFBoneUtilities.h"
#include "Builders/GLTFConvertBuilder.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

FGLTFJsonSkin* FGLTFSkinConverter::Convert(FGLTFJsonNode* RootNode, const USkeletalMesh* SkeletalMesh)
{
	const USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();

	const int32 BoneCount = RefSkeleton.GetNum();
	if (BoneCount == 0)
	{
		// TODO: report warning
		return nullptr;
	}

	FGLTFJsonSkin* JsonSkin = Builder.AddSkin();
	JsonSkin->Name = Skeleton != nullptr ? Skeleton->GetName() : SkeletalMesh->GetName();
	JsonSkin->Skeleton = RootNode;
	JsonSkin->Joints.AddUninitialized(BoneCount);

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		JsonSkin->Joints[BoneIndex] = Builder.AddUniqueNode(RootNode, SkeletalMesh, BoneIndex);
	}

	TArray<FGLTFMatrix4> InverseBindMatrices;
	InverseBindMatrices.AddUninitialized(BoneCount);

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const FTransform3f InverseBindTransform = FTransform3f(FGLTFBoneUtilities::GetBindTransform(RefSkeleton, BoneIndex).Inverse());
		InverseBindMatrices[BoneIndex] = FGLTFCoreUtilities::ConvertTransform(InverseBindTransform, Builder.ExportOptions->ExportUniformScale);
	}

	FGLTFJsonAccessor* JsonBindMatricesAccessor = Builder.AddAccessor();
	JsonBindMatricesAccessor->BufferView = Builder.AddBufferView(InverseBindMatrices);
	JsonBindMatricesAccessor->ComponentType = EGLTFJsonComponentType::Float;
	JsonBindMatricesAccessor->Count = BoneCount;
	JsonBindMatricesAccessor->Type = EGLTFJsonAccessorType::Mat4;

	JsonSkin->InverseBindMatrices = JsonBindMatricesAccessor;
	return JsonSkin;
}
