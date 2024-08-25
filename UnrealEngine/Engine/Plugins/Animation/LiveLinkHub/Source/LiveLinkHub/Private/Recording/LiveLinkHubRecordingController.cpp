// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubRecordingController.h"

#include "Implementations/LiveLinkUAssetRecorder.h"
#include "LiveLinkRecorder.h"
#include "SLiveLinkHubRecordingView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"

FLiveLinkHubRecordingController::FLiveLinkHubRecordingController()
{
	RecorderImplementation = MakeShared<FLiveLinkUAssetRecorder>();
}

TSharedRef<SWidget> FLiveLinkHubRecordingController::MakeRecordToolbarEntry()
{
	return SNew(SLiveLinkHubRecordingView)
		.IsRecording_Raw(this, &FLiveLinkHubRecordingController::IsRecording)
		.OnStartRecording_Raw(this, &FLiveLinkHubRecordingController::StartRecording)
		.OnStopRecording_Raw(this, &FLiveLinkHubRecordingController::StopRecording);
}

void FLiveLinkHubRecordingController::StartRecording()
{
	RecorderImplementation->StartRecording();
}
	
void FLiveLinkHubRecordingController::StopRecording()
{
	RecorderImplementation->StopRecording();
}

bool FLiveLinkHubRecordingController::IsRecording() const
{
	return RecorderImplementation->IsRecording();
}

void FLiveLinkHubRecordingController::RecordStaticData(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, const FLiveLinkStaticDataStruct& StaticData)
{
	RecorderImplementation->RecordStaticData(SubjectKey, Role, StaticData);
}
	
void FLiveLinkHubRecordingController::RecordFrameData(const FLiveLinkSubjectKey& SubjectKey, const FLiveLinkFrameDataStruct& FrameData)
{
	RecorderImplementation->RecordFrameData(SubjectKey, FrameData);
}
