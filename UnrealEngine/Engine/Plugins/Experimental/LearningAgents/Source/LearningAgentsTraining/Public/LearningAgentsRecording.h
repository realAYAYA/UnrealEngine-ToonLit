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
	int32 SampleNum = 0;

	/** The number of dimensions in the observation vector for this record */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "LearningAgents")
	int32 ObservationDimNum = 0;

	/** The number of dimensions in the action vector for this record */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "LearningAgents")
	int32 ActionDimNum = 0;

	bool Serialize(FArchive& Ar);

	TLearningArray<2, float> Observations;
	TLearningArray<2, float> Actions;
};

template<>
struct TStructOpsTypeTraits<FLearningAgentsRecord> : public TStructOpsTypeTraitsBase2<FLearningAgentsRecord>
{
	enum
	{
		WithSerializer = true,
	};
};

/** Data asset representing an array of records. */
UCLASS(BlueprintType)
class LEARNINGAGENTSTRAINING_API ULearningAgentsRecording : public UDataAsset
{
	GENERATED_BODY()

public:
	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsRecording();
	ULearningAgentsRecording(FVTableHelper& Helper);
	virtual ~ULearningAgentsRecording();

public:

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
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DevelopmentOnly))
	void SaveRecordingToAsset(ULearningAgentsRecording* RecordingAsset);

	/** Appends this recording to the given recording asset */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DevelopmentOnly))
	void AppendRecordingToAsset(ULearningAgentsRecording* RecordingAsset);

public:

	/** Marks this asset as modified even during PIE */
	void ForceMarkDirty();

public:

	/** Set of records. */
	UPROPERTY(EditInstanceOnly, Category = "LearningAgents")
	TArray<FLearningAgentsRecord> Records;
};
