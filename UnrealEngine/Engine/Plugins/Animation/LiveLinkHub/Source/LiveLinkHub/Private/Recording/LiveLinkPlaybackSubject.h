// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "LiveLinkFrameTranslator.h"
#include "LiveLinkSubject.h"
#include "LiveLinkTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

class ILiveLinkClient;
class ULiveLinkRole;

// A Virtual subject is made up of one or more real subjects from a source
class FLiveLinkPlaybackSubject : public FLiveLinkSubject
{
	//~ Begin ILiveLinkSubject Interface
public:
	FLiveLinkPlaybackSubject(TSharedPtr<FLiveLinkTimedDataInput> InTimedDataGroup)
		: FLiveLinkSubject(MoveTemp(InTimedDataGroup))
	{
	}

	virtual ~FLiveLinkPlaybackSubject()
	{
	}

	virtual void Initialize(FLiveLinkSubjectKey InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, ILiveLinkClient* InLiveLinkClient) override
	{
		SubjectKey = InSubjectKey;
		Role = InRole;
	}
	
	virtual void Update() override
	{
	}
	
	virtual bool EvaluateFrame(TSubclassOf<ULiveLinkRole> InDesiredRole, FLiveLinkSubjectFrameData& OutFrame) override
	{
		return true;
	}
	
	virtual void ClearFrames() override
	{
	}
	
	virtual FLiveLinkSubjectKey GetSubjectKey() const override
	{
		return SubjectKey;
	}
	
	virtual TSubclassOf<ULiveLinkRole> GetRole() const override
	{
		return Role;
	}
	
	virtual bool HasValidFrameSnapshot() const override
	{
		return true;
	}
	
	virtual FLiveLinkStaticDataStruct& GetStaticData() override
	{
		return CurrentFrameSnapshot.StaticData;
	}
	
	virtual const FLiveLinkStaticDataStruct& GetStaticData() const override
	{
		return CurrentFrameSnapshot.StaticData;
	}
	
	virtual const TArray<ULiveLinkFrameTranslator::FWorkerSharedPtr> GetFrameTranslators() const override
	{
		return {};
	}
	
	virtual TArray<FLiveLinkTime> GetFrameTimes() const override
	{
		return {};
	}
protected:
	virtual const FLiveLinkSubjectFrameData& GetFrameSnapshot() const override
	{
		return CurrentFrameSnapshot;
	}
	//~ End ILiveLinkSubject Interface
	
private:
	/** Last evaluated frame for this subject. */
	FLiveLinkSubjectFrameData CurrentFrameSnapshot;

	/** Name of the subject */
	FLiveLinkSubjectKey SubjectKey;
};
