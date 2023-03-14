// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamPixelStreamingLiveLink.h"
#include "ILiveLinkClient.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"

#define LOCTEXT_NAMESPACE "PixelStreamingLiveLinkSource"

UPixelStreamingLiveLinkSourceSettings::UPixelStreamingLiveLinkSourceSettings()
	: ULiveLinkSourceSettings()
{
	// Override the default evaluation mode to latest
	Mode = ELiveLinkSourceMode::Latest;
}

FPixelStreamingLiveLinkSource::FPixelStreamingLiveLinkSource()
	: LiveLinkClient(nullptr)
{
}

void FPixelStreamingLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	LiveLinkClient = InClient;
	SourceGuid = InSourceGuid;
}

void FPixelStreamingLiveLinkSource::Update()
{
	ILiveLinkSource::Update();
}

bool FPixelStreamingLiveLinkSource::CanBeDisplayedInUI() const
{
	return true;
}

bool FPixelStreamingLiveLinkSource::IsSourceStillValid() const
{
	return true;
}

bool FPixelStreamingLiveLinkSource::RequestSourceShutdown()
{
	return true;
}

FText FPixelStreamingLiveLinkSource::GetSourceType() const
{
	return LOCTEXT("SourceType", "Pixel Streaming");
}

FText FPixelStreamingLiveLinkSource::GetSourceMachineName() const
{
	return FText::FromString(FPlatformProcess::ComputerName());
}

FText FPixelStreamingLiveLinkSource::GetSourceStatus() const
{
	return LOCTEXT("ActiveStatus", "Active");
}

void FPixelStreamingLiveLinkSource::CreateSubject(FName SubjectName) const
{
	if (LiveLinkClient)
	{
		const FLiveLinkSubjectKey SubjectKey(SourceGuid, SubjectName);
		FLiveLinkStaticDataStruct StaticDataStruct(FLiveLinkTransformStaticData::StaticStruct());
		LiveLinkClient->PushSubjectStaticData_AnyThread(SubjectKey, ULiveLinkTransformRole::StaticClass(), MoveTemp(StaticDataStruct));
	}
}

void FPixelStreamingLiveLinkSource::RemoveSubject(FName SubjectName) const
{
	if (LiveLinkClient)
	{
		const FLiveLinkSubjectKey SubjectKey(SourceGuid, SubjectName);
		LiveLinkClient->RemoveSubject_AnyThread(SubjectKey);
	}
}

void FPixelStreamingLiveLinkSource::PushTransformForSubject(FName SubjectName, FTransform Transform) const
{
	if (LiveLinkClient)
	{
		const FLiveLinkSubjectKey SubjectKey(SourceGuid, SubjectName);
		FLiveLinkFrameDataStruct FrameDataStruct(FLiveLinkTransformFrameData::StaticStruct());
		FLiveLinkTransformFrameData* TransformFrameData = FrameDataStruct.Cast<FLiveLinkTransformFrameData>();
		TransformFrameData->Transform = Transform;
		LiveLinkClient->PushSubjectFrameData_AnyThread(SubjectKey, MoveTemp(FrameDataStruct));
	}
}

void FPixelStreamingLiveLinkSource::PushTransformForSubject(FName SubjectName, FTransform Transform,
	double Timestamp) const
{
	if (LiveLinkClient)
	{
		const FLiveLinkSubjectKey SubjectKey(SourceGuid, SubjectName);
		FLiveLinkFrameDataStruct FrameDataStruct(FLiveLinkTransformFrameData::StaticStruct());
		FLiveLinkTransformFrameData* TransformFrameData = FrameDataStruct.Cast<FLiveLinkTransformFrameData>();
		TransformFrameData->Transform = Transform;

		// Timestamp is currently assumed to be elapsed seconds at a fixed rate of 60 frames per second
		// this will be adjusted as actual rate information is supported
		const int32 NumberOfFrames = FMath::FloorToInt32(Timestamp * 60.0);
		TransformFrameData->MetaData.SceneTime = FQualifiedFrameTime(NumberOfFrames, FFrameRate(60,1));
		
		LiveLinkClient->PushSubjectFrameData_AnyThread(SubjectKey, MoveTemp(FrameDataStruct));
	}
}

#undef LOCTEXT_NAMESPACE
