// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGrid/RenderGridPropsSource.h"
#include "RenderGrid.generated.h"


class URenderGridQueue;
class UMoviePipelineOutputSetting;
class UMoviePipelineMasterConfig;
class ULevelSequence;
class URenderGrid;


/**
 * This struct contains the data for a certain remote control property.
 * 
 * It's currently simply a wrapper around a byte array.
 * This struct is needed so that that byte array can be used in another UPROPERTY container (TMap, TArray, etc).
 */
USTRUCT(BlueprintType)
struct RENDERGRID_API FRenderGridRemoteControlPropertyData
{
	GENERATED_BODY()

public:
	FRenderGridRemoteControlPropertyData() = default;
	FRenderGridRemoteControlPropertyData(const TArray<uint8>& InBytes)
		: Bytes(InBytes)
	{}

public:
	/** The property data, as bytes. */
	UPROPERTY()
	TArray<uint8> Bytes;
};


/**
 * This class represents a render grid job, in other words, an entry (a row) of a render grid.
 * It contains a level sequence and custom properties that will be applied while rendering.
 * 
 * Each RenderGridJob must belong to a RenderGrid.
 */
UCLASS(BlueprintType)
class RENDERGRID_API URenderGridJob : public UObject
{
	GENERATED_BODY()

public:
	URenderGridJob();

	/** Gets the calculated start frame, not taking the framerate of the render preset into account. */
	TOptional<int32> GetSequenceStartFrame() const;

	/** Gets the calculated end frame, not taking the framerate of the render preset into account. */
	TOptional<int32> GetSequenceEndFrame() const;

	/** Sets the custom start frame to match the given sequence start frame. */
	bool SetSequenceStartFrame(const int32 NewCustomStartFrame);

	/** Sets the custom end frame to match the given sequence end frame. */
	bool SetSequenceEndFrame(const int32 NewCustomEndFrame);

	/** Gets the calculated start frame. */
	TOptional<int32> GetStartFrame() const;

	/** Gets the calculated end frame. */
	TOptional<int32> GetEndFrame() const;

	/** Gets the calculated start time. */
	TOptional<double> GetStartTime() const;

	/** Gets the calculated end time. */
	TOptional<double> GetEndTime() const;

	/** Gets the calculated duration in seconds. */
	TOptional<double> GetDurationInSeconds() const;

	/** Gets the resolution that this job will be rendered in. */
	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	FIntPoint GetOutputResolution() const;

	/** Gets the aspect ratio that this job will be rendered in. */
	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	double GetOutputAspectRatio() const;

	/** Checks whether the job contains data that matches the search terms. */
	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	bool MatchesSearchTerm(const FString& SearchTerm) const;

public:
	/** Returns the GUID, which is randomly generated at creation. */
	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	FGuid GetGuid() const { return Guid; }

	/** Randomly generates a new GUID. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void GenerateNewGuid() { Guid = FGuid::NewGuid(); }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	int32 GetWaitFramesBeforeRendering() const { return WaitFramesBeforeRendering; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetWaitFramesBeforeRendering(const int32 NewWaitFramesBeforeRendering) { WaitFramesBeforeRendering = FMath::Max<int32>(0, NewWaitFramesBeforeRendering); }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	ULevelSequence* GetSequence() const { return Sequence; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetSequence(ULevelSequence* NewSequence) { Sequence = NewSequence; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	bool GetIsUsingCustomStartFrame() const { return bOverrideStartFrame; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetIsUsingCustomStartFrame(const bool bNewOverrideStartFrame) { bOverrideStartFrame = bNewOverrideStartFrame; }

	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	int32 GetCustomStartFrame() const { return CustomStartFrame; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetCustomStartFrame(const int32 NewCustomStartFrame) { CustomStartFrame = NewCustomStartFrame; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	bool GetIsUsingCustomEndFrame() const { return bOverrideEndFrame; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetIsUsingCustomEndFrame(const bool bNewOverrideEndFrame) { bOverrideEndFrame = bNewOverrideEndFrame; }

	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	int32 GetCustomEndFrame() const { return CustomEndFrame; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetCustomEndFrame(const int32 NewCustomEndFrame) { CustomEndFrame = NewCustomEndFrame; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	bool GetIsUsingCustomResolution() const { return bOverrideResolution; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetIsUsingCustomResolution(const bool bNewOverrideResolution) { bOverrideResolution = bNewOverrideResolution; }

	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	FIntPoint GetCustomResolution() const { return CustomResolution; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetCustomResolution(const FIntPoint NewCustomResolution) { CustomResolution = NewCustomResolution; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	FString GetJobId() const { return JobId; }

	static FString PurgeJobIdOrReturnEmptyString(const FString& NewJobId);
	static FString PurgeJobId(const FString& NewJobId);
	static FString PurgeJobIdOrGenerateUniqueId(URenderGrid* Grid, const FString& NewJobId);

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetJobId(const FString& NewJobId) { JobId = PurgeJobId(NewJobId); }
	void SetJobIdRaw(const FString& NewJobId) { JobId = NewJobId; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	FString GetJobName() const { return JobName; }

	static FString PurgeJobName(const FString& NewJobName);

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetJobName(const FString& NewJobName) { JobName = NewJobName; }
	void SetJobNameRaw(const FString& NewJobName) { JobName = NewJobName; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	bool GetIsEnabled() const { return bIsEnabled; }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetIsEnabled(const bool bEnabled) { bIsEnabled = bEnabled; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	FString GetOutputDirectory() const;
	FString GetOutputDirectoryRaw() const { return OutputDirectory; }

	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	FString GetOutputDirectoryForDisplay() const { return OutputDirectory; }

	static FString PurgeOutputDirectory(const FString& NewOutputDirectory);

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetOutputDirectory(const FString& NewOutputDirectory) { OutputDirectory = PurgeOutputDirectory(NewOutputDirectory); }
	void SetOutputDirectoryRaw(const FString& NewOutputDirectory) { OutputDirectory = NewOutputDirectory; }


	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	UMoviePipelineMasterConfig* GetRenderPreset() const { return RenderPreset; }

	UFUNCTION(BlueprintPure, Category="Render Grid|Job")
	UMoviePipelineOutputSetting* GetRenderPresetOutputSettings() const;

	UFUNCTION(BlueprintCallable, Category="Render Grid|Job")
	void SetRenderPreset(UMoviePipelineMasterConfig* NewRenderPreset) { RenderPreset = NewRenderPreset; }


	bool HasRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity) const;
	bool ConstGetRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, TArray<uint8>& OutBinaryArray) const;
	bool GetRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, TArray<uint8>& OutBinaryArray);
	bool SetRemoteControlValue(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, const TArray<uint8>& BinaryArray);
	TMap<FString, FRenderGridRemoteControlPropertyData>& GetRemoteControlValuesRef() { return RemoteControlValues; }

private:
	/** The unique ID of this job. */
	UPROPERTY()
	FGuid Guid;

	/** Waits the given number of frames before it will render this job. This can be set to a higher amount when the renderer has to wait for your code to complete (such as construction scripts etc). Try increasing this value when rendering doesn't produce the output you expect it to. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true", ClampMin="0"))
	int32 WaitFramesBeforeRendering;

	/** The level sequence, this is what will be rendered during rendering. A job without a level sequence can't be rendered. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true"))
	TObjectPtr<ULevelSequence> Sequence;

	/** If this is true, the CustomStartFrame property will override the start frame of the level sequence. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true", InlineEditConditionToggle))
	bool bOverrideStartFrame;

	/** If bOverrideStartFrame is true, this property will override the start frame of the level sequence. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true", EditCondition="bOverrideStartFrame"))
	int32 CustomStartFrame;

	/** If this is true, the CustomEndFrame property will override the end frame of the level sequence. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true", InlineEditConditionToggle))
	bool bOverrideEndFrame;

	/** If bOverrideEndFrame is true, this property will override the end frame of the level sequence. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true", EditCondition="bOverrideEndFrame"))
	int32 CustomEndFrame;

	/** If this is true, the CustomResolution property will override the resolution of the render. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true", InlineEditConditionToggle))
	bool bOverrideResolution;

	/** If bOverrideResolution is true, this property will override the resolution of the render. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Job", Meta=(AllowPrivateAccess="true", EditCondition="bOverrideResolution", ClampMin="1", UIMin="1"))
	FIntPoint CustomResolution;

	/** If this is true, this job will be rendered during a batch rendering, otherwise it will be skipped. */
	UPROPERTY()
	bool bIsEnabled;

	/** The 'ID' of this job, this 'ID' is set by users. During rendering it will place all the output files of this job into a folder called after this 'ID', this means that this string can only contain file-safe characters. */
	UPROPERTY()
	FString JobId;

	/** The name of this job, this can be anything, it's set by the user, it serves as a way for the user to understand what job this is. */
	UPROPERTY()
	FString JobName;

	/** This is the folder in which the output files (of rendering) are placed into. To be more specific, the output files are placed in: {OutputDirectory}/{JobId}/, this folder will be created if it doesn't exist at the time of rendering. */
	UPROPERTY()
	FString OutputDirectory;

	/** The movie pipeline render preset. Render grid jobs are rendered using the movie pipeline plugin. This 'preset' contains the configuration of that plugin. */
	UPROPERTY()
	TObjectPtr<UMoviePipelineMasterConfig> RenderPreset;

	/** The remote control plugin can be used to customize and modify the way a job is rendered. If remote control is being used, the property values of this job will be stored in this map (remote control entity id -> value as bytes). */
	UPROPERTY()
	TMap<FString, FRenderGridRemoteControlPropertyData> RemoteControlValues;
};


/**
 * This class represents a render grid, which is basically just a collection of render grid jobs.
 * A render grid is the asset that is shown in the content browser, it's the asset that can be opened and edited using the editor.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, Meta=(DontUseGenericSpawnObject="true"))
class RENDERGRID_API URenderGrid : public UObject
{
	GENERATED_BODY()

public:
	URenderGrid();

	//UObject interface
	virtual UWorld* GetWorld() const override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostLoad() override;
	//~ End UObject Interface

	/** Should be called when the editor closes this asset. */
	void OnClose() { SaveValuesToCDO(); }

public:
	static TArray<FString> GetBlueprintImplementableEvents()
	{
		return {
			TEXT("ReceiveBeginEditor"),
			TEXT("ReceiveEndEditor"),
			TEXT("ReceiveBeginBatchRender"),
			TEXT("ReceiveEndBatchRender"),
			TEXT("ReceiveBeginJobRender"),
			TEXT("ReceiveEndJobRender"),
			TEXT("ReceiveBeginViewportRender"),
			TEXT("ReceiveEndViewportRender"),
		};
	}

protected:
	/**
	 * Event for when this asset is opened in the editor. The asset will also be closed and reopened during a blueprint compile.
	 * 
	 * In here, you could for example obtain jobs from an external source and replace the current jobs of this render grid asset with those.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="BeginEditor"))
	void ReceiveBeginEditor();

	/**
	 * Event for when this asset is closed in the editor. The asset will also be closed and reopened during a blueprint compile.
	 * 
	 * In here, you could do any cleanup required at the end of editing this render grid asset.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="EndEditor"))
	void ReceiveEndEditor();

	/**
	 * Event for when batch rendering begins.
	 * 
	 * In here, you could for example obtain jobs from an external source and add them to the queue.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="BeginBatchRender"))
	void ReceiveBeginBatchRender(URenderGridQueue* Queue);

	/**
	 * Event for when batch rendering ends.
	 * 
	 * In here, you could do any cleanup required at the end of a batch render.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="EndBatchRender"))
	void ReceiveEndBatchRender(URenderGridQueue* Queue);

	/**
	 * Event for when job rendering begins.
	 * 
	 * In here, you could for example change elements in the world according to what job this is.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="BeginJobRender"))
	void ReceiveBeginJobRender(URenderGridQueue* Queue, URenderGridJob* Job);

	/**
	 * Event for when job rendering ends.
	 * 
	 * In here, you could do any cleanup required at the end of rendering out a job,
	 * like for example undoing the changes you've made to the world for this job.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="EndJobRender"))
	void ReceiveEndJobRender(URenderGridQueue* Queue, URenderGridJob* Job);

	/**
	 * Event for when job rendering for the viewport viewer-mode begins.
	 * 
	 * This event will fire every frame, as long as the viewport viewer-mode is open.
	 * 
	 * In here, you could for example change elements in the world according to what job this is.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="BeginViewportRender"))
	void ReceiveBeginViewportRender(URenderGridJob* Job);

	/**
	 * Event for when job rendering for the viewport viewer-mode ends.
	 * 
	 * This event will fire every frame, as long as the viewport viewer-mode is open.
	 * 
	 * In here, you could do any cleanup required at the end of rendering out a job,
	 * like for example undoing the changes you've made to the world for this job.
	 */
	UFUNCTION(BlueprintImplementableEvent, Meta=(DisplayName="EndViewportRender"))
	void ReceiveEndViewportRender(URenderGridJob* Job);

public:
	/** Overridable native event for when this asset is opened in the editor. The asset will also be closed and reopened during a blueprint compile. */
	virtual void BeginEditor();

	/** Overridable native event for when this asset is closed in the editor. The asset will also be closed and reopened during a blueprint compile. */
	virtual void EndEditor();

	/** Overridable native event for when batch rendering begins. */
	virtual void BeginBatchRender(URenderGridQueue* Queue);

	/** Overridable native event for when batch rendering ends. */
	virtual void EndBatchRender(URenderGridQueue* Queue);

	/** Overridable native event for when job rendering begins. */
	virtual void BeginJobRender(URenderGridQueue* Queue, URenderGridJob* Job);

	/** Overridable native event for when job rendering ends. */
	virtual void EndJobRender(URenderGridQueue* Queue, URenderGridJob* Job);

	/** Overridable native event for when job rendering for the viewport viewer-mode begins. */
	virtual void BeginViewportRender(URenderGridJob* Job);

	/** Overridable native event for when job rendering for the viewport viewer-mode ends. */
	virtual void EndViewportRender(URenderGridJob* Job);

private:
	/**
	 * Because render grid assets are blueprints (assets that also have a blueprint graph), the render grid data is not stored directly in the asset data that you see in the content browser.
	 * Instead, the data that is stored (and load) is the CDO (class default object).
	 * Because of that, any data that needs to persist needs to be copied over to the CDO during a save, and data you'd like to load from it needs to be copied from the CDO during a load.
	 */

	/** Obtains the CDO, could return itself if this is called on the CDO instance. */
	URenderGrid* GetCDO() { return (HasAnyFlags(RF_ClassDefaultObject) ? this : GetClass()->GetDefaultObject<URenderGrid>()); }

	/** Copied values over into the CDO. */
	void SaveValuesToCDO() { CopyValuesToOrFromCDO(true); }

	/** Copied values over from the CDO. */
	void LoadValuesFromCDO() { CopyValuesToOrFromCDO(false); }

	/** Copied values to or from the CDO, based on whether bToCDO is true or false. */
	void CopyValuesToOrFromCDO(const bool bToCDO);

public:
	/** Returns the GUID, which is randomly generated at creation. */
	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	FGuid GetGuid() const { return Guid; }

	/** Randomly generates a new GUID. */
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	void GenerateNewGuid() { Guid = FGuid::NewGuid(); }

	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	void SetPropsSource(ERenderGridPropsSourceType InPropsSourceType, UObject* InPropsSourceOrigin = nullptr);

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	URenderGridPropsSourceBase* GetPropsSource() const;

	template<typename Type>
	Type* GetPropsSource() const
	{
		static_assert(TIsDerivedFrom<Type, URenderGridPropsSourceBase>::IsDerived, "Type needs to derive from URenderGridPropsSourceBase.");
		return Cast<Type>(GetPropsSource());
	}

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	ERenderGridPropsSourceType GetPropsSourceType() const { return PropsSourceType; }

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	UObject* GetPropsSourceOrigin() const;

public:
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	void AddRenderGridJob(URenderGridJob* Job);

	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	void RemoveRenderGridJob(URenderGridJob* Job);

	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	void InsertRenderGridJob(URenderGridJob* Job, int32 Index);

	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	bool HasAnyRenderGridJobs() const { return !RenderGridJobs.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	bool HasRenderGridJob(URenderGridJob* Job) const;

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	int32 GetIndexOfRenderGridJob(URenderGridJob* Job) const;

	TArray<TObjectPtr<URenderGridJob>>& GetRenderGridJobsRef() { return RenderGridJobs; }

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	TArray<URenderGridJob*> GetRenderGridJobs() const;

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	TArray<URenderGridJob*> GetEnabledRenderGridJobs() const;

	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	TArray<URenderGridJob*> GetDisabledRenderGridJobs() const;

	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	void InsertRenderGridJobBefore(URenderGridJob* Job, URenderGridJob* BeforeJob);

	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	void InsertRenderGridJobAfter(URenderGridJob* Job, URenderGridJob* AfterJob);

public:
	/** Generates a unique job ID by grabbing the current time, as well as 16 random bytes, and converting that to a base64 string. **/
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	FString GenerateUniqueRandomJobId();

	/** Generates a unique job ID by finding the currently highest job ID and increasing it by one. **/
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	FString GenerateNextJobId();

	/** Finds whether given job ID already exists in this grid. **/
	UFUNCTION(BlueprintPure, Category="Render Grid|Grid")
	bool DoesJobIdExist(const FString& JobId);

	/** Creates a new job. This job won't be added to the grid, so it will eventually be garbage collected. **/
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	URenderGridJob* CreateTempRenderGridJob();

	/** Creates a new job and adds it to this grid. **/
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	URenderGridJob* CreateAndAddNewRenderGridJob();

	/** Copy the given job in this grid. **/
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	URenderGridJob* DuplicateAndAddRenderGridJob(URenderGridJob* Job);

	/** Relocates the given job in this grid to the position of the given dropped-on job. **/
	UFUNCTION(BlueprintCallable, Category="Render Grid|Grid")
	bool ReorderRenderGridJob(URenderGridJob* Job, URenderGridJob* DroppedOnJob, const bool bAfter = true);

public:
	/** Returns true when it's currently executing a blueprint implementable event, returns false otherwise. */
	bool IsCurrentlyExecutingUserCode() const { return bExecutingBlueprintEvent; }

private:
	DECLARE_MULTICAST_DELEGATE(FOnRenderGridPreSave);
	FOnRenderGridPreSave& OnPreSaveCDO() { return GetCDO()->OnRenderGridPreSaveDelegate; }

private:
	/** The delegate for when this grid is about to save. */
	FOnRenderGridPreSave OnRenderGridPreSaveDelegate;

private:
	/** The unique ID of this render grid. */
	UPROPERTY()
	FGuid Guid;


	/** The type of the properties that a job in this grid can have. */
	UPROPERTY(/*EditInstanceOnly, Category="Render Grid|Grid", Meta=(DisplayName="Properties Type", AllowPrivateAccess="true")*/)
	ERenderGridPropsSourceType PropsSourceType;

	/** The remote control properties that a job in this grid can have, only use this if PropsSourceType is ERenderGridPropsSourceType::RemoteControl. */
	UPROPERTY(EditInstanceOnly, Category="Render Grid|Grid", Meta=(DisplayName="Remote Control Preset", AllowPrivateAccess="true", EditCondition="PropsSourceType == ERenderGridPropsSourceType::RemoteControl", EditConditionHides))
	TObjectPtr<URemoteControlPreset> PropsSourceOrigin_RemoteControl;


	/** The jobs of this grid. */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<URenderGridJob>> RenderGridJobs;


	/** True when it's currently executing a blueprint event, false otherwise. */
	UPROPERTY(Transient)
	bool bExecutingBlueprintEvent;


	/** GetPropsSource calls are somewhat expensive, we speed that up by caching the result (the PropsSource) that has been last outputted by that function. */
	UPROPERTY(Transient)
	mutable TObjectPtr<URenderGridPropsSourceBase> CachedPropsSource;

	/** GetPropsSource calls are somewhat expensive, we speed that up by caching the PropsSourceType last used in that function. */
	UPROPERTY(Transient)
	mutable ERenderGridPropsSourceType CachedPropsSourceType;

	/** GetPropsSource calls are somewhat expensive, we speed that up by caching the PropsSourceOrigin last used in that function. */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<UObject> CachedPropsSourceOriginWeakPtr;


	/** GetWorld calls can be expensive, we speed them up by caching the last found world until it goes away. */
	mutable TWeakObjectPtr<UWorld> CachedWorldWeakPtr;
};
