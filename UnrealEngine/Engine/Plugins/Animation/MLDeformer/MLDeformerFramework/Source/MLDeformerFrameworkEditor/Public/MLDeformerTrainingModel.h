// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "MLDeformerTrainingModel.generated.h"

class UMLDeformerModel;

namespace UE::MLDeformer
{
	class FMLDeformerEditorModel;
}

/**
 * The training model base class.
 * This class is used to interface with Python by providing some methods you can call inside your python training code.
 * For example it allows you to get all the sampled data, such as the deltas, bones and curve values.
 * When you create a new model you need to create a class inherited from this base class, and define a Train method inside it as follows:
 *
 * @code{.cpp}
 * // Doesn't need an implementation inside cpp, just a declaration in the header file.
 * UFUNCTION(BlueprintImplementableEvent, Category = "Training Model")
 * int32 Train() const;
 * @endcode
 * 
 * Now inside your Python class you do something like:
 * 
 * @code{.py}
 * @unreal.uclass()
 * class YourModelPythonTrainingModel(unreal.YourTrainingModel):
 *     @unreal.ufunction(override=True)
 *     def train(self):
 *         # ...do training here...
 *         return 0   # A value of 0 is success, 1 means aborted, see ETrainingResult.
 * @endcode
 * 
 * The editor will execute the Train method, which will trigger the "train" method in your Python class to be executed.
 * Keep in mind that in Unreal Engine all python code is lower case. So a "Train" method inside c++ will need to be called "train" inside the python code.
 * Or if you have something called "PerformMyTraining" it will need to be called "perform_my_training" inside Python.
 */
UCLASS(Blueprintable)
class MLDEFORMERFRAMEWORKEDITOR_API UMLDeformerTrainingModel
	: public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Initialize the training model.
	 * This is automatically called by the editor.
	 * @param InEditorModel The pointer to the editor model that this is a training model for.
	 */
	virtual void Init(UE::MLDeformer::FMLDeformerEditorModel* InEditorModel);

	/** Get the runtime ML Deformer model object. */
	UFUNCTION(BlueprintPure, Category = "Training Data")
	UMLDeformerModel* GetModel() const;

	/** Get the number of input transforms. This is the number of bones. */
	UFUNCTION(BlueprintPure, Category = "Training Data")
	int32 GetNumberSampleTransforms() const;

	/** Get number of input curves. */
	UFUNCTION(BlueprintPure, Category = "Training Data")
	int32 GetNumberSampleCurves() const;

	/** Get the number of vertex deltas. */
	UFUNCTION(BlueprintPure, Category = "Training Data")
	int32 GetNumberSampleDeltas() const;

	/** Get the number of samples in this data set. This is the number of sample frames we want to train on. */
	UFUNCTION(BlueprintPure, Category = "Training Data")
    int32 NumSamples() const;

	/** 
	 * Set the current sample frame. This will internally call the SampleFrame method, which will update the deltas, curve values and bone rotations. 
	 * You call this before getting the input bone/curve and vertex delta values.
	 * @param Index The training data frame/sample number.
	 * @return Returns true when successful, or false when the specified sample index is out of range.
	 */
	UFUNCTION(BlueprintCallable, Category = "Training Data")
	bool SetCurrentSampleIndex(int32 Index);

	/** 
	 * Check whether we need to resample the inputs and outputs, or if we can use a cached version. 
	 * This will return true if any inputs changed, that indicate that we should regenerate any cached data.
	 * @return Returns true when we need to regenerate any cached data, otherwise false is returned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Training Data")
	bool GetNeedsResampling() const;

	/** Specify whether we need to resample any cached data or not, because our input assets, or any other relevant settings changed that would invalidate the cached data. */
	UFUNCTION(BlueprintCallable, Category = "Training Data")
	void SetNeedsResampling(bool bNeedsResampling);

protected:
	/** This updates the sample deltas, curves, and bone rotations. */
    virtual bool SampleFrame(int32 Index);

	/** Change the pointer to the editor model. */
	void SetEditorModel(UE::MLDeformer::FMLDeformerEditorModel* InModel);

	/** Get a pointer to the editor model. */ 
	UE::MLDeformer::FMLDeformerEditorModel* GetEditorModel() const;

public:
	// The delta values per vertex for this sample. This is updated after SetCurrentSampleIndex is called. Contains an xyz (3 floats) for each vertex.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Data")
	TArray<float> SampleDeltas;

	// The curve weights. This is updated after SetCurrentSampleIndex is called.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Data")
	TArray<float> SampleCurveValues;

	// The bone rotations in bone (local) space for this sample. This is updated after SetCurrentSampleIndex is called and is 6 floats per bone (2 columns of 3x3 rotation matrix).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Data")
	TArray<float> SampleBoneRotations;

protected:
	/** A pointer to the editor model from which we use the sampler. */
	UE::MLDeformer::FMLDeformerEditorModel* EditorModel = nullptr;
};
