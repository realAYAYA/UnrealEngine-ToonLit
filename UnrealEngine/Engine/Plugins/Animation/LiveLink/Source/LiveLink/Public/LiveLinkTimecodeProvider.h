// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TimecodeProvider.h"

#include "HAL/CriticalSection.h"
#include "LiveLinkTypes.h"
#include "Templates/Atomic.h"
#include "LiveLinkTimecodeProvider.generated.h"

class ILiveLinkClient;
class IModularFeature;

UENUM()
enum class ELiveLinkTimecodeProviderEvaluationType
{
	/** Interpolate, or extrapolate, between the 2 frames that are the closest to evaluation. */
	Lerp,
	/** Use the frame that is closest to evaluation. */
	Nearest,
	/** Use the newest frame that was received. */
	Latest,
};

/**
 * Fetch the latest frames from the LiveLink subject and create a Timecode from it.
 */
UCLASS(config=Engine, Blueprintable, editinlinenew)
class LIVELINK_API ULiveLinkTimecodeProvider : public UTimecodeProvider
{
	GENERATED_BODY()

public:
	ULiveLinkTimecodeProvider();

	//~ Begin UTimecodeProvider Interface
	virtual FQualifiedFrameTime GetQualifiedFrameTime() const override;
	
	virtual ETimecodeProviderSynchronizationState GetSynchronizationState() const override
	{
		return State;
	}

	virtual bool Initialize(class UEngine* InEngine) override;
	virtual void Shutdown(class UEngine* InEngine) override;
	//~ End UTimecodeProvider Interface

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject Interface

private:
	FQualifiedFrameTime ConvertTo(FQualifiedFrameTime Value) const;
	FQualifiedFrameTime LerpBetweenFrames(double Seconds, int32 IndexA, int32 IndexB) const;

	void InitClient();
	void UninitClient();
	void RegisterSubject();
	void UnregisterSubject();
	void OnLiveLinkClientRegistered(const FName& Type, class IModularFeature* ModularFeature);
	void OnLiveLinkClientUnregistered(const FName& Type, class IModularFeature* ModularFeature);
	void OnLiveLinkSubjectAdded(FLiveLinkSubjectKey SubjectKey);
	void OnLiveLinkSubjectRemoved(FLiveLinkSubjectKey SubjectKey);
	void OnLiveLinkFrameDataReceived_AnyThread(const FLiveLinkFrameDataStruct& FrameData);

private:
	/** The specific subject that we listen to. */
	UPROPERTY(EditAnywhere, Category = Timecode)
	FLiveLinkSubjectKey SubjectKey;

	/** How to evaluate the timecode. */
	UPROPERTY(EditAnywhere, Category = Timecode)
	ELiveLinkTimecodeProviderEvaluationType Evaluation;

	UPROPERTY(EditAnywhere, Category = Timecode, meta=(InlineEditConditionToggle))
	bool bOverrideFrameRate;

	/**
	 * Override the frame rate at which this timecode provider will create its timecode value.
	 * By default, we use the subject frame rate.
	 */
	UPROPERTY(EditAnywhere, Category = Timecode, meta=(EditCondition="bOverrideFrameRate"))
	FFrameRate OverrideFrameRate;

	/** The number of frame to keep in memory. The provider will not be synchronized until the buffer is full at least once. */
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = Timecode, meta=(ClampMin = "2", UIMin = "2", ClampMax = "10", UIMax = "10"))
	int32 BufferSize;

private:
	TAtomic<ETimecodeProviderSynchronizationState> State;
	ILiveLinkClient* LiveLinkClient;
	FLiveLinkSubjectKey RegisteredSubjectKey;
	TArray<FLiveLinkTime> SubjectFrameTimes;
	mutable FCriticalSection SubjectFrameLock; // Only lock SubjectFrameTimes
	FDelegateHandle RegisterForFrameDataReceivedHandle;
};
