// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeAnimationPose.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/PoseAsset.h"
#include "Animation/SmartName.h"
#include "BoneContainer.h"
#include "BoneIndices.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Math/TransformVectorized.h"
#include "Misc/AssertionMacros.h"
#include "Misc/MemStack.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "ReferenceSkeleton.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeAnimationPose::UCustomizableObjectNodeAnimationPose() : Super()
	, PoseAsset(nullptr)
{
}


void UCustomizableObjectNodeAnimationPose::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeAnimationPose::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Input Mesh");
	UEdGraphPin* PinImagePin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(*PinName));
	PinImagePin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Output Mesh");
	PinImagePin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*PinName));
	PinImagePin->bDefaultValueIsIgnored = true;
}


UEdGraphPin* UCustomizableObjectNodeAnimationPose::GetInputMeshPin() const
{
	FString PinName = FString(TEXT("Input Mesh"));

	UEdGraphPin* Pin = FindPin(PinName);

	return Pin;
}


FText UCustomizableObjectNodeAnimationPose::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (PoseAsset)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("SkeletamMeshName"), FText::FromString(PoseAsset->GetName()));

		return FText::Format(LOCTEXT("AnimationPose_Title", "{SkeletamMeshName}\nAnimation Pose"), Args);
	}
	else
	{
		return LOCTEXT("PoseMesh", "Pose Mesh");
	}
}


FLinearColor UCustomizableObjectNodeAnimationPose::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


void UCustomizableObjectNodeAnimationPose::StaticRetrievePoseInformation(UPoseAsset* PoseAsset, USkeletalMesh* RefSkeletalMesh, TArray<FString>& OutArrayBoneName, TArray<FTransform>& OutArrayTransform)
{
	if (PoseAsset && RefSkeletalMesh)
	{
		// Need this for the FCompactPose below
		FMemMark Mark(FMemStack::Get());

		UDebugSkelMeshComponent* SkeletalMeshComponent = NewObject<UDebugSkelMeshComponent>();
		SkeletalMeshComponent->SetSkeletalMesh(RefSkeletalMesh);
		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		SkeletalMeshComponent->AllocateTransformData();
		SkeletalMeshComponent->SetAnimation(PoseAsset);
		SkeletalMeshComponent->RefreshBoneTransforms();
		SkeletalMeshComponent->InitAnim(false);

		UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
		FBoneContainer& BoneContainerCopy = AnimInstance->GetRequiredBones();

		// Needs a FMemMark declared before in the stack context so that the memory allocated by the FCompactPose is freed correctly
		FCompactPose OutPose;
		OutPose.SetBoneContainer(&BoneContainerCopy);

		FBlendedCurve OutCurve;
		UE::Anim::FStackAttributeContainer OutAttributes;
		FAnimationPoseData OutAnimData(OutPose, OutCurve, OutAttributes);
		PoseAsset->GetBaseAnimationPose(OutAnimData);
		OutPose = OutAnimData.GetPose();
		OutCurve = OutAnimData.GetCurve();

		FBlendedHeapCurve AnimCurves = SkeletalMeshComponent->AnimCurves;
		const int32 NumElement = AnimCurves.CurveWeights.Num();

		for (int32 i = 0; i < NumElement; ++i)
		{
			OutCurve.CurveWeights.Add(AnimCurves.CurveWeights[i]);
		}

		OutCurve.bInitialized = AnimCurves.bInitialized;

		// Assuming one single pose, with a weigth set to 1.0
		FAnimExtractContext ExtractionContext;
		ExtractionContext.bExtractRootMotion = false;
		ExtractionContext.CurrentTime = 0.0f;

		OutCurve.UIDToArrayIndexLUT = AnimCurves.UIDToArrayIndexLUT;

		const TArray<FSmartName>& PoseNames = PoseAsset->GetPoseNames();
		ExtractionContext.PoseCurves.Add(FPoseCurve(0, PoseNames[0].UID, 1.0f));
		FAnimationPoseData SecondOutAnimData(OutPose, OutCurve, OutAttributes);
		PoseAsset->GetAnimationPose(SecondOutAnimData, ExtractionContext);
		OutPose = OutAnimData.GetPose();
		OutCurve = OutAnimData.GetCurve();

		const TArray<FTransform, FAnimStackAllocator>& ArrayPoseBoneTransform = OutPose.GetBones();
		const TArray<FBoneIndexType>& ArrayPoseBoneIndices = OutPose.GetBoneContainer().GetBoneIndicesArray();

		for (int32 ArrayIndex = 0; ArrayIndex < ArrayPoseBoneIndices.Num(); ArrayIndex++)
		{
			FBoneIndexType const& PoseBoneIndex = ArrayPoseBoneIndices[ArrayIndex];

			checkSlow(PoseBoneIndex != INDEX_NONE);

			FTransform CumulativePoseTransform;
			FBoneIndexType ParentIndex = PoseBoneIndex;

			while (ParentIndex > 0)
			{
				for (int32 IndicesIndex = 0; IndicesIndex < ArrayPoseBoneIndices.Num(); ++IndicesIndex)
				{
					const FBoneIndexType BoneIndex = ArrayPoseBoneIndices[IndicesIndex];

					if (BoneIndex == ParentIndex)
					{
						CumulativePoseTransform = CumulativePoseTransform * ArrayPoseBoneTransform[IndicesIndex];
						break;
					}
					else if (BoneIndex > ParentIndex)
					{
						break; // Break for, ArrayPoseBoneIndices is sorted.
					}
				}

				ParentIndex = RefSkeletalMesh->GetRefSkeleton().GetParentIndex(ParentIndex);
			}

			FString Name = RefSkeletalMesh->GetRefSkeleton().GetBoneName(PoseBoneIndex).ToString();
			int32 SMBoneIndex = SkeletalMeshComponent->GetBoneIndex(FName(*Name));
			FTransform BoneToComponentTranform = SkeletalMeshComponent->GetEditableComponentSpaceTransforms()[SMBoneIndex];
			FTransform TransformToAdd = BoneToComponentTranform.Inverse() * CumulativePoseTransform;

			OutArrayBoneName.Add(Name);
			OutArrayTransform.Add(TransformToAdd);
		}
	}
}


#undef LOCTEXT_NAMESPACE
