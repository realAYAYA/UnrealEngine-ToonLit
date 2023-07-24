// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralMorphModelInstance.h"
#include "NeuralMorphModel.h"
#include "NeuralMorphNetwork.h"
#include "NeuralMorphInputInfo.h"
#include "MLDeformerComponent.h"
#include "MLDeformerAsset.h"
#include "Animation/AnimInstance.h"
#include "Components/ExternalMorphSet.h"
#include "Components/SkeletalMeshComponent.h"

CSV_DECLARE_CATEGORY_MODULE_EXTERN(MLDEFORMERFRAMEWORK_API, MLDeformer);

void UNeuralMorphModelInstance::Init(USkeletalMeshComponent* SkelMeshComponent)
{
	Super::Init(SkelMeshComponent);

	UNeuralMorphModel* MorphModel = Cast<UNeuralMorphModel>(Model);
	UNeuralMorphNetwork* MorphNetwork = MorphModel->GetNeuralMorphNetwork();
	NetworkInstance = MorphNetwork ? MorphNetwork->CreateInstance() : nullptr;

	UNeuralMorphInputInfo* InputInfo = Cast<UNeuralMorphInputInfo>(Model->GetInputInfo());
	if (InputInfo == nullptr)
	{
		return;
	}

	InputInfo->GenerateBoneGroupIndices(BoneGroupIndices);
	InputInfo->GenerateCurveGroupIndices(CurveGroupIndices);
}

void UNeuralMorphModelInstance::FillNetworkInputs()
{
	check(NetworkInstance);
	UNeuralMorphNetwork* MorphNetwork = Cast<UNeuralMorphNetwork>(NetworkInstance->GetOuter());

	// Write the input bone transforms.
	float* MainInputs = NetworkInstance->GetInputs().GetData();
	const UNeuralMorphMLP* MainMLP = MorphNetwork->GetMainMLP();
	const int32 NumMainMLPInputs = MainMLP->GetNumInputs();
	Super::SetBoneTransforms(MainInputs, NumMainMLPInputs, 0);

	// Write the curve input values, start writing after the bone inputs.
	const int32 CurveWriteOffset = Model->GetInputInfo()->GetNumBones() * 6;	// 6 Floats per bone.
	SetCurveValues(MainInputs, NumMainMLPInputs, CurveWriteOffset);

	// Normalize the inputs.
	const float* Means = MorphNetwork->GetInputMeans().GetData();
	const float* Stds = MorphNetwork->GetInputStds().GetData();
	const int32 NumInputs = MorphNetwork->GetInputMeans().Num();
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		MainInputs[Index] = (MainInputs[Index] - Means[Index]) / Stds[Index];
	}

	// Write the input transforms for the group MLP.
	// We reuse the same input values from the main network, and those values just got normalized.
	// So there is no need to normalize them again.
	const UNeuralMorphMLP* GroupMLP = MorphNetwork->GetGroupMLP();
	if (GroupMLP)
	{
		// Only the local mode should be using the group MLP.
		check(Cast<UNeuralMorphModel>(Model)->GetModelMode() == ENeuralMorphMode::Local);

		// Write the bone transforms.
		int32 Offset = 0;
		float* GroupNetworkInputs = NetworkInstance->GetGroupInputs().GetData();
		const int32 NumBoneGroups = BoneGroupIndices.Num();
		for (int32 Index = 0; Index < BoneGroupIndices.Num(); ++Index)
		{
			const int32 BoneIndex = BoneGroupIndices[Index];
			if (BoneIndex != INDEX_NONE)
			{
				const int32 BoneOffset = BoneIndex * 6;
				GroupNetworkInputs[Offset++] = MainInputs[BoneOffset];
				GroupNetworkInputs[Offset++] = MainInputs[BoneOffset + 1];
				GroupNetworkInputs[Offset++] = MainInputs[BoneOffset + 2];
				GroupNetworkInputs[Offset++] = MainInputs[BoneOffset + 3];
				GroupNetworkInputs[Offset++] = MainInputs[BoneOffset + 4];
				GroupNetworkInputs[Offset++] = MainInputs[BoneOffset + 5];
			}
			else
			{
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
			}
		}

		// Write the curve values.
		const int32 NumFloatsPerCurve = MorphNetwork->GetNumFloatsPerCurve();
		check(MorphNetwork->GetNumFloatsPerCurve() == 6);	// Local mode curve values are expected to use 6 floats.
		const int32 NumCurveGroups = CurveGroupIndices.Num();
		for (int32 Index = 0; Index < CurveGroupIndices.Num(); ++Index)
		{
			const int32 CurveIndex = CurveGroupIndices[Index];
			if (CurveIndex != INDEX_NONE)
			{
				const int32 CurveOffset = CurveWriteOffset + (CurveIndex * NumFloatsPerCurve);
				GroupNetworkInputs[Offset++] = MainInputs[CurveOffset];
				GroupNetworkInputs[Offset++] = MainInputs[CurveOffset + 1];
				GroupNetworkInputs[Offset++] = MainInputs[CurveOffset + 2];
				GroupNetworkInputs[Offset++] = MainInputs[CurveOffset + 3];
				GroupNetworkInputs[Offset++] = MainInputs[CurveOffset + 4];
				GroupNetworkInputs[Offset++] = MainInputs[CurveOffset + 5];
			}
			else
			{
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
				GroupNetworkInputs[Offset++] = 0.0f;
			}
		}
	}
}

int64 UNeuralMorphModelInstance::SetCurveValues(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex)
{
	UNeuralMorphModel* MorphModel = Cast<UNeuralMorphModel>(Model);
	UNeuralMorphNetwork* MorphNetwork = MorphModel->GetNeuralMorphNetwork();

	const UMLDeformerInputInfo* InputInfo = Model->GetInputInfo();

	int64 Index = StartIndex;
	const int32 AssetNumCurves = InputInfo->GetNumCurves();
	const int32 NumFloatsPerCurve = MorphNetwork ? MorphNetwork->GetNumFloatsPerCurve() : 1;
	const int32 NumCurveFloats = AssetNumCurves * NumFloatsPerCurve;
	checkf((Index + NumCurveFloats) <= OutputBufferSize, TEXT("Writing curves past the end of the input buffer"));

	if (NumFloatsPerCurve > 1)
	{
		// First write all zeros.
		for (int32 CurveIndex = 0; CurveIndex < NumCurveFloats; ++CurveIndex)
		{
			OutputBuffer[Index++] = 0.0f;
		}

		// Write the actual curve values.
		UAnimInstance* AnimInstance = SkeletalMeshComponent->GetAnimInstance();
		if (AnimInstance)
		{
			for (int32 CurveIndex = 0; CurveIndex < AssetNumCurves; ++CurveIndex)
			{
				const FName CurveName = InputInfo->GetCurveName(CurveIndex);
				const float CurveValue = AnimInstance->GetCurveValue(CurveName);	// Outputs 0.0 when not found.
				OutputBuffer[StartIndex + CurveIndex * NumFloatsPerCurve] = CurveValue;
			}
		}
	}
	else
	{
		checkSlow(NumFloatsPerCurve == 1);
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
			// Just write zeros.
			for (int32 CurveIndex = 0; CurveIndex < NumCurveFloats; ++CurveIndex)
			{
				OutputBuffer[Index++] = 0.0f;
			}
		}
	}

	return Index;
}


bool UNeuralMorphModelInstance::SetupInputs()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNeuralMorphModelInstance::SetupInputs)

	// Make sure we have a valid network.
	UNeuralMorphModel* MorphModel = Cast<UNeuralMorphModel>(Model);
	UNeuralMorphNetwork* MorphNetwork = MorphModel->GetNeuralMorphNetwork();
	if (MorphNetwork == nullptr || MorphNetwork->GetMainMLP() == nullptr)
	{		
		return false;
	}

	// Some additional safety checks.
	if (SkeletalMeshComponent == nullptr ||
		SkeletalMeshComponent->GetSkeletalMeshAsset() == nullptr ||
		!bIsCompatible)
	{
		return false;
	}

	// If the neural network expects a different number of inputs, do nothing.
	const int64 NumNeuralNetInputs = MorphNetwork->GetNumInputs();
	const int32 NumFloatsPerBone = 6;
	const int32 NumFloatsPerCurve = MorphNetwork->GetNumFloatsPerCurve();
	const int64 NumDeformerAssetInputs = Model->GetInputInfo()->CalcNumNeuralNetInputs(NumFloatsPerBone, NumFloatsPerCurve);
	if (NumNeuralNetInputs != NumDeformerAssetInputs)
	{
		return false;
	}

	// Set the actual network input values.
	FillNetworkInputs();

	return true;
}

void UNeuralMorphModelInstance::Execute(float ModelWeight)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNeuralMorphModelInstance::Execute)
	CSV_SCOPED_TIMING_STAT(MLDeformer, NeuralMorphExecute);

	UNeuralMorphModel* MorphModel = Cast<UNeuralMorphModel>(Model);
	UNeuralMorphNetwork* MorphNetwork = MorphModel->GetNeuralMorphNetwork();
	if (MorphNetwork == nullptr)
	{
		Super::Execute(ModelWeight);
		return;
	}

	// Grab the weight data for this morph set.
	// This could potentially fail if we are applying this deformer to the wrong skeletal mesh component.
	const int LOD = 0;	// For now we only support LOD 0, as we can't setup an ML Deformer per LOD yet.
	FExternalMorphSetWeights* WeightData = FindWeightData(LOD);
	if (WeightData == nullptr)
	{
		return;
	}

	// Make sure we have a valid network instance.
	if (NetworkInstance == nullptr)
	{
		WeightData->ZeroWeights();
		return;
	}

	// If our model is active, we want to run the neural network and update the morph weights
	// with the values that the neural net calculated for us.
	// Get the network output values, read the values and use them as morph target weights inside the skeletal mesh component.
	const TArrayView<const float> NetworkOutputs = NetworkInstance->GetOutputs();
	const int32 NumNetworkWeights = NetworkOutputs.Num();
	const int32 NumMorphTargets = WeightData->Weights.Num();

	// Set the weights to zero if we have no valid network.
	if (MorphNetwork->IsEmpty() || NumMorphTargets != NumNetworkWeights + 1)	// Plus 1 because of the extra means morph target.
	{
		WeightData->ZeroWeights();
		return;
	}

	// Perform inference on the neural network, this updates its output values.
	NetworkInstance->Run();

	// Set the first morph target, which represents the means, to a weight of 1.0, as it always needs to be fully active.
	WeightData->Weights[0] = ModelWeight;

	// Update all generated morph target weights with the values calculated by our neural network.
	const TArrayView<const float> ErrorValues = MorphModel->GetMorphTargetErrorValues();
	const TArrayView<const int32> ErrorOrder = MorphModel->GetMorphTargetErrorOrder();
	if (!ErrorValues.IsEmpty())
	{
		const int32 QualityLevel = GetMLDeformerComponent()->GetQualityLevel();
		const int32 NumActiveMorphs = MorphModel->GetNumActiveMorphs(QualityLevel);
		for (int32 Index = 0; Index < NumActiveMorphs; ++Index)
		{
			const int32 MorphIndex = ErrorOrder[Index];
			const float TargetWeight = NetworkOutputs[MorphIndex] * ModelWeight;
			WeightData->Weights[MorphIndex + 1] = FMath::Lerp<float, float>(StartMorphWeights[MorphIndex + 1], TargetWeight, MorphLerpAlpha);
		}

		// Disable all inactive morphs.
		for (int32 Index = NumActiveMorphs; Index < NumNetworkWeights; ++Index)
		{
			const int32 MorphIndex = ErrorOrder[Index];
			WeightData->Weights[MorphIndex + 1] = FMath::Lerp<float, float>(StartMorphWeights[MorphIndex + 1], 0.0f, MorphLerpAlpha);
		}
	}
	else // Old models might not have the error values yet, and thus not support the quality levels yet.
	{
		for (int32 MorphIndex = 0; MorphIndex < NumNetworkWeights; ++MorphIndex)
		{
			WeightData->Weights[MorphIndex + 1] = NetworkOutputs[MorphIndex] * ModelWeight;
		}
	}
}
