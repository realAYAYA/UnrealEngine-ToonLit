// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerModelInstance.h"
#include "MLDeformerModel.h"
#include "MLDeformerModule.h"
#include "MLDeformerAsset.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerModelInstance)

CSV_DEFINE_CATEGORY_MODULE(MLDEFORMERFRAMEWORK_API, MLDeformer, false);

#if STATS
DEFINE_STAT(STAT_MLDeformerInference);
#endif

void UMLDeformerModelInstance::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}

void UMLDeformerModelInstance::Release()
{
	// Wait for the render commands to finish, because some neural network might still be executing, using the inference handle
	// that we are about to delete.
	RenderCommandFence.BeginFence();
	RenderCommandFence.Wait();
}

USkeletalMeshComponent* UMLDeformerModelInstance::GetSkeletalMeshComponent() const
{ 
	return SkeletalMeshComponent;
}

UMLDeformerModel* UMLDeformerModelInstance::GetModel() const
{ 
	return Model.Get();
}

void UMLDeformerModelInstance::SetModel(UMLDeformerModel* InModel)
{ 
	Model = InModel;
}

UMLDeformerComponent* UMLDeformerModelInstance::GetMLDeformerComponent() const
{ 
	return Cast<UMLDeformerComponent>(GetOuter());
}

int32 UMLDeformerModelInstance::GetNeuralNetworkInferenceHandle() const
{ 
	return NeuralNetworkInferenceHandle;
}

void UMLDeformerModelInstance::SetHasPostInitialized(bool bHasInitialized)
{ 
	bHasPostInitialized = bHasInitialized;
}

bool UMLDeformerModelInstance::HasPostInitialized() const
{ 
	return bHasPostInitialized;
}

const TArray<FTransform>& UMLDeformerModelInstance::GetBoneTransforms() const
{ 
	return BoneTransforms;
}

bool UMLDeformerModelInstance::IsCompatible() const
{ 
	return bIsCompatible;
}

const FString& UMLDeformerModelInstance::GetCompatibilityErrorText() const
{ 
	return ErrorText;
}

void UMLDeformerModelInstance::Init(USkeletalMeshComponent* SkelMeshComponent)
{
	SkeletalMeshComponent = SkelMeshComponent;

	if (SkelMeshComponent == nullptr)
	{
		AssetBonesToSkelMeshMappings.Empty();
		return;
	}

	if (Model->DoesSupportBones())
	{
		USkeletalMesh* SkelMesh = SkelMeshComponent->GetSkeletalMeshAsset();
		if (SkelMesh)
		{
			// Init the bone mapping table.
			const UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();
			const int32 NumAssetBones = InputInfo->GetNumBones();
			AssetBonesToSkelMeshMappings.Reset();
			AssetBonesToSkelMeshMappings.AddUninitialized(NumAssetBones);
			TrainingBoneTransforms.SetNumUninitialized(NumAssetBones);

			// For each bone in the deformer asset, find the matching bone index inside the skeletal mesh component.
			for (int32 Index = 0; Index < NumAssetBones; ++Index)
			{
				const FName BoneName = InputInfo->GetBoneName(Index);
				const int32 SkelMeshBoneIndex = SkeletalMeshComponent->GetBaseComponent()->GetBoneIndex(BoneName);
				AssetBonesToSkelMeshMappings[Index] = SkelMeshBoneIndex;
			}
		}
	}

	// Perform a compatibility check.
	UpdateCompatibilityStatus();
}

void UMLDeformerModelInstance::UpdateCompatibilityStatus()
{
	bIsCompatible = SkeletalMeshComponent->GetSkeletalMeshAsset() && CheckCompatibility(SkeletalMeshComponent, true).IsEmpty();
}

FString UMLDeformerModelInstance::CheckCompatibility(USkeletalMeshComponent* InSkelMeshComponent, bool bLogIssues)
{
	ErrorText = FString();

	// If we're not compatible, generate a compatibility string.
	USkeletalMesh* SkelMesh = InSkelMeshComponent ? InSkelMeshComponent->GetSkeletalMeshAsset() : nullptr;
	UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();
	if (SkelMesh && !InputInfo->IsCompatible(SkelMesh) && Model->GetDeformerAsset())
	{
		ErrorText += InputInfo->GenerateCompatibilityErrorString(SkelMesh);
		ErrorText += "\n";
		if (bLogIssues)
		{
			UE_LOG(LogMLDeformer, Error, TEXT("ML Deformer '%s' isn't compatible with Skeletal Mesh '%s'.\nReason(s):\n%s"), 
				*Model->GetDeformerAsset()->GetName(), 
				*SkelMesh->GetName(), 
				*ErrorText);
		}
	}

	return ErrorText;
}

void UMLDeformerModelInstance::UpdateBoneTransforms()
{
	// If we use a leader component, we have to get the transforms from that component.
	const USkinnedMeshComponent* LeaderPoseComponent = SkeletalMeshComponent->LeaderPoseComponent.Get();
	if (LeaderPoseComponent)
	{
		const TArray<FTransform>& LeaderTransforms = LeaderPoseComponent->GetComponentSpaceTransforms();
		if (!LeaderTransforms.IsEmpty())
		{
			USkinnedAsset* SkinnedAsset = LeaderPoseComponent->GetSkinnedAsset();
			const int32 NumTrainingBones = AssetBonesToSkelMeshMappings.Num();
			for (int32 Index = 0; Index < NumTrainingBones; ++Index)
			{
				const int32 ComponentBoneIndex = AssetBonesToSkelMeshMappings[Index];
				checkSlow(LeaderTransforms.IsValidIndex(ComponentBoneIndex));			
				const FTransform& ComponentSpaceTransform = LeaderTransforms[ComponentBoneIndex];
				const int32 ParentIndex = SkinnedAsset->GetRefSkeleton().GetParentIndex(ComponentBoneIndex);
				if (LeaderTransforms.IsValidIndex(ParentIndex))
				{
					TrainingBoneTransforms[Index] = ComponentSpaceTransform.GetRelativeTransform(LeaderTransforms[ParentIndex]);
				}
				else
				{
					TrainingBoneTransforms[Index] = ComponentSpaceTransform;
				}
				TrainingBoneTransforms[Index].NormalizeRotation();
			}
		}
	}
	else
	{
		// Grab the transforms from our own skeletal mesh component.
		// These are local space transforms, relative to the parent bone.
		const TArrayView<const FTransform> Transforms = SkeletalMeshComponent->GetBoneSpaceTransformsView();
		if (!Transforms.IsEmpty())
		{
			const int32 NumTrainingBones = AssetBonesToSkelMeshMappings.Num();
			for (int32 Index = 0; Index < NumTrainingBones; ++Index)
			{
				const int32 ComponentBoneIndex = AssetBonesToSkelMeshMappings[Index];
				TrainingBoneTransforms[Index] = Transforms[ComponentBoneIndex];
			}
		}
	}
}

int64 UMLDeformerModelInstance::SetBoneTransforms(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex)
{
	// Get the transforms for the bones we used during training.
	// These are in the space relative to their parent.
	UpdateBoneTransforms();

	// Make sure we don't write past the OutputBuffer. Six, because of two columns of the 3x3 rotation matrix.
	const int32 AssetNumBones = Model->GetInputInfo()->GetNumBones();
	checkfSlow((StartIndex + AssetNumBones * 6) <= OutputBufferSize, TEXT("Writing bones past the end of the input buffer."));

	// Write 6 floats to the buffer, for each bone.
	UMLDeformerInputInfo::RotationToTwoVectorsAsSixFloats(TrainingBoneTransforms, OutputBuffer + StartIndex);

	// Return the new buffer offset, where we stopped writing.
	return StartIndex + AssetNumBones * 6;
}

int64 UMLDeformerModelInstance::SetCurveValues(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex)
{
	const UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();

	int64 Index = StartIndex;
	const int32 AssetNumCurves = InputInfo->GetNumCurves();
	checkf((Index + AssetNumCurves) <= OutputBufferSize, TEXT("Writing curves past the end of the input buffer"));

	// Write the curve weights to the output buffer.
	UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
	if (AnimInstance)
	{
		for (int32 CurveIndex = 0; CurveIndex < AssetNumCurves; ++CurveIndex)
		{
			const FName CurveName = InputInfo->GetCurveName(CurveIndex);
			const float CurveValue = AnimInstance->GetCurveValue(CurveName);	// Outputs 0.0 when not found.
			OutputBuffer[Index++] = CurveValue;
		}
	}
	else
	{
		for (int32 CurveIndex = 0; CurveIndex < AssetNumCurves; ++CurveIndex)
		{
			OutputBuffer[Index++] = 0.0f;
		}
	}

	return Index;
}

int64 UMLDeformerModelInstance::SetNeuralNetworkInputValues(float* InputData, int64 NumInputFloats)
{
	// Feed data to the network inputs.
	int64 BufferOffset = 0;
	if (Model->DoesSupportBones())
	{
		BufferOffset = SetBoneTransforms(InputData, NumInputFloats, BufferOffset);
	}

	if (Model->DoesSupportCurves())
	{
		BufferOffset = SetCurveValues(InputData, NumInputFloats, BufferOffset);
	}

	return BufferOffset;
}

bool UMLDeformerModelInstance::IsValidForDataProvider() const
{
	return true;
}

bool UMLDeformerModelInstance::HasValidTransforms() const
{
	if (!SkeletalMeshComponent)
	{
		return false;
	}

	const USkinnedMeshComponent* LeaderPoseComponent = SkeletalMeshComponent->LeaderPoseComponent.Get();
	if (LeaderPoseComponent)
	{
		if (LeaderPoseComponent->GetComponentSpaceTransforms().IsEmpty())
		{
			return false;
		}
	}
	else if (SkeletalMeshComponent->GetBoneSpaceTransformsView().IsEmpty())
	{
		return false;
	}

	return true;
}

void UMLDeformerModelInstance::Tick(float DeltaTime, float ModelWeight)
{
	// Post init hasn't yet succeeded, try again.
	// This could for example happen when you add an ML Deformer component, but your SkeletalMesh isn't setup yet, but later becomes valid.
	if (Model && !HasPostInitialized())
	{
		PostMLDeformerComponentInit();
	}

	if (ModelWeight > 0.0001f && HasValidTransforms() && SetupInputs())
	{
		// Execute the model instance.
		// For models using neural networks this will perform the inference, 
		// calculate the network outputs and possibly use them, depending on how the model works.
		Execute(ModelWeight);
	}
	else
	{
		HandleZeroModelWeight();
	}
}
