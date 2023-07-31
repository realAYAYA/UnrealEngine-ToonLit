// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerModelInstance.h"
#include "MLDeformerModel.h"
#include "MLDeformerModule.h"
#include "MLDeformerAsset.h"
#include "MLDeformerInputInfo.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "NeuralNetwork.h"

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

	// Destroy the neural network instance.
	UNeuralNetwork* NeuralNetwork = Model.Get() ? Model->GetNeuralNetwork() : nullptr;
	if (NeuralNetwork && NeuralNetworkInferenceHandle != -1)
	{
		NeuralNetwork->DestroyInferenceContext(NeuralNetworkInferenceHandle);
		NeuralNetworkInferenceHandle = -1;
	}
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

FString UMLDeformerModelInstance::CheckCompatibility(USkeletalMeshComponent* InSkelMeshComponent, bool LogIssues)
{
	ErrorText = FString();

	// If we're not compatible, generate a compatibility string.
	USkeletalMesh* SkelMesh = InSkelMeshComponent ? InSkelMeshComponent->GetSkeletalMeshAsset() : nullptr;
	UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();
	if (SkelMesh && !InputInfo->IsCompatible(SkelMesh) && Model->GetDeformerAsset())
	{
		ErrorText += InputInfo->GenerateCompatibilityErrorString(SkelMesh);
		ErrorText += "\n";
		if (LogIssues)
		{
			UE_LOG(LogMLDeformer, Error, TEXT("ML Deformer '%s' isn't compatible with Skeletal Mesh '%s'.\nReason(s):\n%s"), 
				*Model->GetDeformerAsset()->GetName(), 
				*SkelMesh->GetName(), 
				*ErrorText);
		}
	}

	// Verify the number of inputs versus the expected number of inputs.
	UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
	if (NeuralNetwork && NeuralNetwork->IsLoaded() && Model->GetDeformerAsset())
	{
		const int64 NumNeuralNetInputs = NeuralNetwork->GetInputTensor().Num();
		const int64 NumDeformerAssetInputs = static_cast<int64>(Model->GetInputInfo()->CalcNumNeuralNetInputs());
		if (NumNeuralNetInputs != NumDeformerAssetInputs)
		{
			const FString InputErrorString = "The number of network inputs doesn't match the asset. Please retrain the asset."; 
			ErrorText += InputErrorString + "\n";
			if (LogIssues)
			{
				UE_LOG(LogMLDeformer, Error, TEXT("Deformer '%s': %s"), *(Model->GetDeformerAsset()->GetName()), *InputErrorString);
			}
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
		USkinnedAsset* SkinnedAsset = LeaderPoseComponent->GetSkinnedAsset();
		const int32 NumTrainingBones = AssetBonesToSkelMeshMappings.Num();
		for (int32 Index = 0; Index < NumTrainingBones; ++Index)
		{
			const int32 ComponentBoneIndex = AssetBonesToSkelMeshMappings[Index];
			const FTransform& ComponentSpaceTransform = LeaderTransforms[ComponentBoneIndex];
			const int32 ParentIndex = SkinnedAsset->GetRefSkeleton().GetParentIndex(ComponentBoneIndex);
			if (ParentIndex != INDEX_NONE)
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
	else
	{
		// Grab the transforms from our own skeletal mesh component.
		// These are local space transforms, relative to the parent bone.
		BoneTransforms = SkeletalMeshComponent->GetBoneSpaceTransforms();
		const int32 NumTrainingBones = AssetBonesToSkelMeshMappings.Num();
		for (int32 Index = 0; Index < NumTrainingBones; ++Index)
		{
			const int32 ComponentBoneIndex = AssetBonesToSkelMeshMappings[Index];
			TrainingBoneTransforms[Index] = BoneTransforms[ComponentBoneIndex];
		}
	}
}

int64 UMLDeformerModelInstance::SetBoneTransforms(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex)
{
	// Get the transforms for the bones we used during training.
	// These are in the space relative to their parent.
	UpdateBoneTransforms();

	// Write the transforms into the output buffer.
	const UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();
	const int32 AssetNumBones = InputInfo->GetNumBones();
	int64 Index = StartIndex;

	// Make sure we don't write past the OutputBuffer. (6 because of two columns of the 3x3 rotation matrix)
	checkf((Index + AssetNumBones * 6) <= OutputBufferSize, TEXT("Writing bones past the end of the input buffer."));

	for (int32 BoneIndex = 0; BoneIndex < AssetNumBones; ++BoneIndex)
	{
		const FMatrix RotationMatrix = TrainingBoneTransforms[BoneIndex].GetRotation().ToMatrix();
		const FVector X = RotationMatrix.GetColumn(0);
		const FVector Y = RotationMatrix.GetColumn(1);	
		OutputBuffer[Index++] = X.X;
		OutputBuffer[Index++] = X.Y;
		OutputBuffer[Index++] = X.Z;
		OutputBuffer[Index++] = Y.X;
		OutputBuffer[Index++] = Y.Y;
		OutputBuffer[Index++] = Y.Z;
	}

	return Index;
}

int64 UMLDeformerModelInstance::SetCurveValues(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex)
{
	const UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();

	// Write the weights into the output buffer.
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
	check(SkeletalMeshComponent);

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
	const UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
	if (NeuralNetwork == nullptr || !NeuralNetwork->IsLoaded())
	{
		return false;
	}

	// We expect to run on the GPU when using a data provider for the deformer graph system (Optimus).
	if (Model->IsNeuralNetworkOnGPU())
	{
		// Make sure we're actually running the network on the GPU.
		// Inputs are expected to come from the CPU though.
		if (NeuralNetwork->GetDeviceType() != ENeuralDeviceType::GPU || NeuralNetwork->GetOutputDeviceType() != ENeuralDeviceType::GPU || NeuralNetwork->GetInputDeviceType() != ENeuralDeviceType::CPU)
		{
			return false;
		}
	}

	return (Model->GetVertexMapBuffer().ShaderResourceViewRHI != nullptr) && (GetNeuralNetworkInferenceHandle() != -1);
}

void UMLDeformerModelInstance::Execute(float ModelWeight)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerModelInstance::Execute)

	UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
	if (NeuralNetwork == nullptr)
	{
		return;
	}

	if (Model->IsNeuralNetworkOnGPU())
	{
		// Even if the model needs the GPU it is possible that the hardware does not support GPU evaluation
		if (NeuralNetwork->GetDeviceType() == ENeuralDeviceType::GPU)
		{
			// NOTE: Inputs still come from the CPU.
			check(NeuralNetwork->GetDeviceType() == ENeuralDeviceType::GPU && NeuralNetwork->GetInputDeviceType() == ENeuralDeviceType::CPU && NeuralNetwork->GetOutputDeviceType() == ENeuralDeviceType::GPU);
			ENQUEUE_RENDER_COMMAND(RunNeuralNetwork)
				(
					[NeuralNetwork, Handle = NeuralNetworkInferenceHandle](FRHICommandListImmediate& RHICmdList)
					{
						// Output deltas will be available on GPU for DeformerGraph via UMLDeformerDataProvider.
						FRDGBuilder GraphBuilder(RHICmdList);
						NeuralNetwork->Run(GraphBuilder, Handle);
						GraphBuilder.Execute();
					}
			);
		}
	}
	else
	{
		// Run on the CPU.
		check(NeuralNetwork->GetDeviceType() == ENeuralDeviceType::CPU && NeuralNetwork->GetInputDeviceType() == ENeuralDeviceType::CPU && NeuralNetwork->GetOutputDeviceType() == ENeuralDeviceType::CPU);
		NeuralNetwork->Run(NeuralNetworkInferenceHandle);
	}
}

bool UMLDeformerModelInstance::SetupInputs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerModelInstance::SetupInputs)

	// Some safety checks.
	if (Model == nullptr ||
		SkeletalMeshComponent == nullptr ||
		SkeletalMeshComponent->GetSkeletalMeshAsset() == nullptr ||
		!bIsCompatible)
	{
		return false;
	}

	// Get the network and make sure it's loaded.
	UNeuralNetwork* NeuralNetwork = Model->GetNeuralNetwork();
	if (NeuralNetwork == nullptr || !NeuralNetwork->IsLoaded())
	{
		return false;
	}

	// Allocate an inference context if none has already been allocated.
	if (NeuralNetworkInferenceHandle == -1)
	{
		NeuralNetworkInferenceHandle = NeuralNetwork->CreateInferenceContext();
		if (NeuralNetworkInferenceHandle == -1)
		{
			return false;
		}
	}

	// If the neural network expects a different number of inputs, do nothing.
	const int64 NumNeuralNetInputs = NeuralNetwork->GetInputTensorForContext(NeuralNetworkInferenceHandle).Num();
	const int64 NumDeformerAssetInputs = Model->GetInputInfo()->CalcNumNeuralNetInputs();
	if (NumNeuralNetInputs != NumDeformerAssetInputs)
	{
		return false;
	}

	// Update and write the input values directly into the input tensor.
	float* InputDataPointer = static_cast<float*>(NeuralNetwork->GetInputDataPointerMutableForContext(NeuralNetworkInferenceHandle));
	const int64 NumFloatsWritten = SetNeuralNetworkInputValues(InputDataPointer, NumNeuralNetInputs);
	check(NumFloatsWritten == NumNeuralNetInputs);

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

	if (ModelWeight > 0.0001f && SetupInputs())
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
