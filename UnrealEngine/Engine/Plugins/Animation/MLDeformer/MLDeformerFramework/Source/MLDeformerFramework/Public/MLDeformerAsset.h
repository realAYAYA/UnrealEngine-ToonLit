// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MLDeformerAsset.generated.h"

class UMLDeformerModel;

/**
 * The machine learning deformer asset class.
 * This class contains a Model property, through which most functionality happens.
 */
UCLASS(BlueprintType, hidecategories=Object)
class MLDEFORMERFRAMEWORK_API UMLDeformerAsset 
	: public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get the ML Deformer model that is being applied by this asset.
	 * @return A pointer to the model.
	 */
	UMLDeformerModel* GetModel() const			{ return Model.Get(); }

	/**
	 * Set the ML Deformer model that is used by this deformer asset.
	 * @param InModel A pointer to the model object.
	 */
	void SetModel(UMLDeformerModel* InModel)	{ Model = InModel; }

public:
	/** The ML Deformer model, used to deform the mesh. */
	UPROPERTY()
	TObjectPtr<UMLDeformerModel> Model = nullptr;
};
