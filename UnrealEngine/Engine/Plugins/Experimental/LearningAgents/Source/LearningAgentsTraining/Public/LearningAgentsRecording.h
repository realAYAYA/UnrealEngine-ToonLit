// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

#include "Containers/Array.h"
#include "UObject/NameTypes.h"
#include "Engine/DataAsset.h"

#include "LearningAgentsRecording.generated.h"

/** A single recording of a series of observations and actions. */
USTRUCT(BlueprintType)
struct LEARNINGAGENTSTRAINING_API FLearningAgentsRecord
{
	GENERATED_BODY()

public:

	/** The number of observations and actions recorded. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "LearningAgents")
	int32 StepNum = 0;

	/** The number of dimensions in the observation vector for this record */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "LearningAgents")
	int32 ObservationDimNum = 0;

	/** The number of dimensions in the action vector for this record */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "LearningAgents")
	int32 ActionDimNum = 0;

	/** The compatibility hash for the recorded observation vectors */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "LearningAgents")
	int32 ObservationCompatibilityHash = 0;

	/** The compatibility hash for the recorded action vectors */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "LearningAgents")
	int32 ActionCompatibilityHash = 0;

	/** Observation data */
	UPROPERTY(BlueprintReadOnly, Category = "LearningAgents")
	TArray<float> ObservationData;

	/** Action data */
	UPROPERTY(BlueprintReadOnly, Category = "LearningAgents")
	TArray<float> ActionData;

};

/** Data asset representing an array of records. */
UCLASS(BlueprintType, Blueprintable)
class LEARNINGAGENTSTRAINING_API ULearningAgentsRecording : public UDataAsset
{
	GENERATED_BODY()

public:
	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsRecording();
	ULearningAgentsRecording(FVTableHelper& Helper);
	virtual ~ULearningAgentsRecording();

public:

	/** Resets this recording asset to be empty. */
	UFUNCTION(CallInEditor, Category = "LearningAgents")
	void ResetRecording();

	/** Load this recording from a file. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (RelativePath))
	void LoadRecordingFromFile(const FFilePath& File);

	/** Save this recording to a file. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "LearningAgents", meta = (RelativePath))
	void SaveRecordingToFile(const FFilePath& File) const;

	/** Append to this recording from a file. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (RelativePath))
	void AppendRecordingFromFile(const FFilePath& File);

	/** Loads this recording from the given recording asset */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void LoadRecordingFromAsset(ULearningAgentsRecording* RecordingAsset);

	/** Saves this recording to the given recording asset */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void SaveRecordingToAsset(ULearningAgentsRecording* RecordingAsset);

	/** Appends this recording to the given recording asset */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void AppendRecordingToAsset(ULearningAgentsRecording* RecordingAsset);

	/** Get the number of records */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	int32 GetRecordNum() const;

	/** Get the number of steps in a given record */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	int32 GetRecordStepNum(const int32 Record) const;

	/**
	 * Get the Observation Vector associated with a particular step of a given recording
	 * 
	 * @param OutObservationVector				Output Observation Vector
	 * @param OutObservationCompatibilityHash	Output Compatibility Hash for the given Observation Vector
	 * @param Record							Index of the record in the array of records.
	 * @param Step								Step of the recording
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void GetObservationVector(TArray<float>& OutObservationVector, int32& OutObservationCompatibilityHash, const int32 Record, const int32 Step);

	/**
	 * Get the Action Vector associated with a particular step of a given recording
	 *
	 * @param OutActionVector					Output Action Vector
	 * @param OutActionCompatibilityHash		Output Compatibility Hash for the given Action Vector
	 * @param Record							Index of the record in the array of records.
	 * @param Step								Step of the recording
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void GetActionVector(TArray<float>& OutActionVector, int32& OutActionCompatibilityHash, const int32 Record, const int32 Step);

public:

	/** Marks this asset as modified even during PIE */
	void ForceMarkDirty();

public:

	/** Set of records. */
	UPROPERTY(EditInstanceOnly, Category = "LearningAgents")
	TArray<FLearningAgentsRecord> Records;
};
