// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimeSynchronizationSource.h"
#include "LiveLinkClient.h"
#include "Misc/Guid.h"
#include "LiveLinkTimeSynchronizationSource.generated.h"

UCLASS(EditInlineNew)
class LIVELINK_API ULiveLinkTimeSynchronizationSource : public UTimeSynchronizationSource
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="LiveLink")
	FLiveLinkSubjectName SubjectName;

private:
	FLiveLinkClient* LiveLinkClient;

	enum class ESyncState
	{
		NotSynced,
		Opened,
	};

	mutable ESyncState State = ESyncState::NotSynced;
	mutable FLiveLinkSubjectTimeSyncData CachedData;
	mutable int64 LastUpdateFrame;
	FLiveLinkSubjectKey SubjectKey;

public:

	ULiveLinkTimeSynchronizationSource();

	//~ Begin TimeSynchronizationSource API
	virtual FFrameTime GetNewestSampleTime() const override;
	virtual FFrameTime GetOldestSampleTime() const override;
	virtual FFrameRate GetFrameRate() const override;
	virtual bool IsReady() const override;
	virtual bool Open(const FTimeSynchronizationOpenData& OpenData) override;
	virtual void Start(const FTimeSynchronizationStartData& StartData) override;
	virtual void Close() override;
	virtual FString GetDisplayName() const override;
	//~ End TimeSynchronizationSource API

private:

	bool IsCurrentStateValid() const;
	void OnModularFeatureRegistered(const FName& FeatureName, class IModularFeature* Feature);
	void OnModularFeatureUnregistered(const FName& FeatureName, class IModularFeature* Feature);
	void UpdateCachedState() const;
};