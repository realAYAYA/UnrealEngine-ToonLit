// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Recorder/TakeRecorderPanel.h"

class UTakePreset;
class UTakeMetaData;
class ULevelSequence;
class STakeRecorderPanel;
class FTakePresetToolkit;
class UTakeRecorderSources;
class STakePresetAssetEditor;

class STakeRecorderTabContent : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STakeRecorderTabContent){}
	SLATE_END_ARGS()

	~STakeRecorderTabContent();

	void Construct(const FArguments& InArgs);

	FText GetTitle() const;

	const FSlateBrush* GetIcon() const;

	TOptional<ETakeRecorderPanelMode> GetMode() const;

	void SetupForRecording(ULevelSequence* LevelSequenceAsset);

	void SetupForRecording(UTakePreset* BasePreset);

	void SetupForRecordingInto(ULevelSequence* LevelSequenceAsset);

	void SetupForEditing(TSharedPtr<FTakePresetToolkit> InToolkit);

	void SetupForViewing(ULevelSequence* LevelSequence);

	/*~ UTakeRecorderPanel exposure */

	ULevelSequence* GetLevelSequence() const;

	ULevelSequence* GetLastRecordedLevelSequence() const;

	UTakeMetaData* GetTakeMetaData() const;

	FFrameRate GetFrameRate() const;

	void SetFrameRate(FFrameRate InFrameRate);

	void SetFrameRateFromTimecode(bool  bInFromTimecode);

	UTakeRecorderSources* GetSources() const;

	void StartRecording() const;

	void StopRecording() const;

	void ClearPendingTake();

	bool CanStartRecording(FText& ErrorText) const;

private:

	EActiveTimerReturnType OnActiveTimer(double InCurrentTime, float InDeltaTime);

private:

	TAttribute<FText> TitleAttribute;
	TAttribute<const FSlateBrush*> IconAttribute;
	TWeakPtr<STakeRecorderPanel> WeakPanel;
	TWeakPtr<STakePresetAssetEditor> WeakAssetEditor;
};
