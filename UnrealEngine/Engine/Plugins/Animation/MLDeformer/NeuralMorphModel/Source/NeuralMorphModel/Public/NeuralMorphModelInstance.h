// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerMorphModelInstance.h"
#include "NeuralMorphModelInstance.generated.h"

class UNeuralMorphNetworkInstance;
class UNeuralMorphNetwork;

UCLASS()
class NEURALMORPHMODEL_API UNeuralMorphModelInstance
	: public UMLDeformerMorphModelInstance
{
	GENERATED_BODY()

public:
	// UMLDeformerModelInstance overrides.
	virtual void Init(USkeletalMeshComponent* SkelMeshComponent) override;
	virtual int64 SetCurveValues(float* OutputBuffer, int64 OutputBufferSize, int64 StartIndex) override;
	virtual bool SetupInputs() override;
	virtual void Execute(float ModelWeight) override;
	// ~END UMLDeformerModelInstance overrides.

protected:
	/**
	 * Set the network inputs.
	 */
	void FillNetworkInputs();

protected:
	UPROPERTY(Transient)
	TObjectPtr<UNeuralMorphNetworkInstance> NetworkInstance;

	TArray<int32> BoneGroupIndices;
	TArray<int32> CurveGroupIndices;
};
