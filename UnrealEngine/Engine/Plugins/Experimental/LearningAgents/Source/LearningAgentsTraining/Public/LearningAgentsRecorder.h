// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsManagerComponent.h"

#include "LearningArray.h"
#include "Containers/Map.h"
#include "UObject/ObjectPtr.h"

#include "LearningAgentsRecorder.generated.h"

struct FLearningAgentsRecord;
class ULearningAgentsRecording;

/** The path settings for the recorder. */
USTRUCT(BlueprintType, Category = "LearningAgents")
struct LEARNINGAGENTSTRAINING_API FLearningAgentsRecorderPathSettings
{
	GENERATED_BODY()

public:

	FLearningAgentsRecorderPathSettings();

	/** The relative path to the Intermediate directory. Defaults to FPaths::ProjectIntermediateDir. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LearningAgents", meta = (RelativePath))
	FDirectoryPath IntermediateRelativePath;

	/** The name of the sub-directory to use in the intermediate directory */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "LearningAgents", meta = (RelativePath))
	FString RecordingsSubdirectory = TEXT("Recordings");
};

/** A component that can be used to create recordings of training data for imitation learning. */
UCLASS(BlueprintType, Blueprintable)
class LEARNINGAGENTSTRAINING_API ULearningAgentsRecorder : public ULearningAgentsManagerComponent
{
	GENERATED_BODY()

// ----- Setup -----
public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsRecorder();
	ULearningAgentsRecorder(FVTableHelper& Helper);
	virtual ~ULearningAgentsRecorder();

	/** Will automatically call EndRecording if recording is still in-progress when play is ending. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Initializes this object and runs the setup functions for the underlying data storage.
	 * @param InInteractor The agent interactor we are recording with.
	 * @param RecorderPathSettings The path settings used by the recorder.
	 * @param RecordingAsset Optional recording asset to use. If not provided then a new recording object will be 
	 * created.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void SetupRecorder(
		ULearningAgentsInteractor* InInteractor,
		const FLearningAgentsRecorderPathSettings& RecorderPathSettings = FLearningAgentsRecorderPathSettings(),
		ULearningAgentsRecording* RecordingAsset = nullptr);

public:

	//~ Begin ULearningAgentsManagerComponent Interface
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds) override;
	virtual void OnAgentsReset(const TArray<int32>& AgentIds) override;
	//~ End ULearningAgentsManagerComponent Interface

// ----- Recording Process -----
public:

	/**
	 * Begins the recording of the observations and actions of each added agent.
	 * @param bReinitializeRecording If to clear all records from the recording object in use at the start of recording.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void BeginRecording(bool bReinitializeRecording = true);

	/**
	 * Ends the recording of the observations and actions of each agent and stores them in the current recording object.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EndRecording();

	/** Returns true if recording is active; Otherwise, false. */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	bool IsRecording() const;

	/**
	 * While recording, adds the current observations and actions of the added agents to the internal buffer. Call this 
	 * after ULearningAgentsInteractor::EncodeObservations and either ULearningAgentsController::EncodeActions (if 
	 * recording a human/AI demonstration) or ULearningAgentsInteractor::DecodeActions (if recording another policy).
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void AddExperience();

	/** Gets the current recording object. Note: this may be empty until EndRecording has been called. */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	const ULearningAgentsRecording* GetCurrentRecording() const;

// ----- Load / Save -----
public:

	/** Loads the current recording object from a file */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (RelativePath))
	void LoadRecordingFromFile(const FFilePath& File);

	/** Saves the current recording object to a file */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "LearningAgents", meta = (RelativePath))
	void SaveRecordingToFile(const FFilePath& File) const;

	/** Append to the current recording object from a file. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (RelativePath))
	void AppendRecordingFromFile(const FFilePath& File);

	/** Uses the given recording asset. New recordings will be appended to this. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void UseRecordingAsset(ULearningAgentsRecording* RecordingAsset);

	/** Loads the current recording object from the given recording asset */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void LoadRecordingFromAsset(ULearningAgentsRecording* RecordingAsset);

	/** Saves the current recording object to the given recording asset */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DevelopmentOnly))
	void SaveRecordingToAsset(ULearningAgentsRecording* RecordingAsset);

	/** Appends the current recording object to the given recording asset */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents", meta = (DevelopmentOnly))
	void AppendRecordingToAsset(ULearningAgentsRecording* RecordingAsset);


// ----- Private Data ----- 
private:

	/** The agent interactor this recorder is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;

	/** The current recording object. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsRecording> Recording;

	/** True if recording is currently in-progress. Otherwise, false. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	bool bIsRecording = false;

// ----- Private Data ----- 
private:

	/** Directory to store recordings in */
	FString RecordingDirectory;

	/** Basic structure used to buffer the observations and actions of each agent. */
	struct FAgentRecordBuffer
	{
		const int32 ChunkSize = 1024;
		int32 SampleNum = 0;
		TArray<TLearningArray<2, float>, TInlineAllocator<16>> Observations;
		TArray<TLearningArray<2, float>, TInlineAllocator<16>> Actions;

		TLearningArrayView<1, float> GetObservation(const int32 SampleIdx);
		TLearningArrayView<1, float> GetAction(const int32 SampleIdx);
		TLearningArrayView<1, const float> GetObservation(const int32 SampleIdx) const;
		TLearningArrayView<1, const float> GetAction(const int32 SampleIdx) const;

		bool IsEmpty() const;

		void Empty();

		void Push(
			const TLearningArrayView<1, const float> Observation,
			const TLearningArrayView<1, const float> Action);

		void CopyToRecord(FLearningAgentsRecord& Record) const;
	};

	/** Array of recording buffers for each agent */
	TArray<FAgentRecordBuffer, TInlineAllocator<32>> RecordBuffers;
};
