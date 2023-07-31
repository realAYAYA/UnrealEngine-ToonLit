// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"

class IDetailsView;
class SCompoundWidget;
class UObject;
class USequenceRecordingBase;

class SEQUENCERECORDER_API ISequenceRecorderExtender : public TSharedFromThis<ISequenceRecorderExtender>
{
public:
	virtual ~ISequenceRecorderExtender() {}

	virtual TSharedPtr<IDetailsView> MakeSettingDetailsView() = 0;

	DECLARE_DELEGATE_OneParam(FListViewSelectionChanged, USequenceRecordingBase*);
	virtual TSharedPtr<SWidget> MakeListWidget(TSharedPtr<SListView<USequenceRecordingBase*>>& OutCreatedListView, FListViewSelectionChanged OnListViewSelectionChanged) = 0;

	virtual void SetListViewSelection(USequenceRecordingBase* InSelectedBase) = 0;

	virtual USequenceRecordingBase* AddNewQueueRecording(UObject* SequenceRecordingObjectToRecord) = 0;

	virtual void BuildQueuedRecordings(const TArray<USequenceRecordingBase*>& InQueuedRecordings) = 0;
};

