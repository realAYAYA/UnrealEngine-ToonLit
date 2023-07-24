// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerModelInstance.h"
#include "VertexDeltaModelInstance.generated.h"

class UVertexDeltaModel;
class UNeuralNetwork;

UCLASS()
class VERTEXDELTAMODEL_API UVertexDeltaModelInstance
	: public UMLDeformerModelInstance
{
	GENERATED_BODY()

public:
	// UMLDeformerModelInstance overrides.
	virtual void Release() override;
	virtual FString CheckCompatibility(USkeletalMeshComponent* InSkelMeshComponent, bool bLogIssues) override;
	virtual bool IsValidForDataProvider() const override;
	virtual void Execute(float ModelWeight) override;
	virtual bool SetupInputs() override;
	// ~END UMLDeformerModelInstance overrides.

	UVertexDeltaModel* GetVertexDeltaModel() const;
	UNeuralNetwork* GetNNINetwork() const;
};
