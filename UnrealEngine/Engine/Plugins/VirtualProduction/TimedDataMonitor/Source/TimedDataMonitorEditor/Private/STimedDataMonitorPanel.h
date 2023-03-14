// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SlateOptMacros.h"

#include "Framework/Commands/UIAction.h"
#include "Input/Reply.h"
#include "Textures/SlateIcon.h"
#include "TimedDataMonitorEditorSettings.h"


struct FSlateBrush;
struct FTimedDataMonitorCalibrationResult;
class FTimedDataMonitorCalibration;
class FWorkspaceItem;
class IMessageLogListing;
class STimedDataGenlock;
class STimedDataInputListView;
class STimedDataTimecodeProvider;
class SWidget;

enum class ETimedDataMonitorEvaluationState : uint8;

class STimedDataMonitorPanel : public SCompoundWidget
{
public:
	static void RegisterNomadTabSpawner(TSharedRef<FWorkspaceItem> InWorkspaceItem);
	static void UnregisterNomadTabSpawner();
	static TSharedPtr<STimedDataMonitorPanel> GetPanelInstance();

private:
	using Super = SCompoundWidget;
	static TWeakPtr<STimedDataMonitorPanel> WidgetInstance;

public:
	SLATE_BEGIN_ARGS(STimedDataMonitorPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void RequestRefresh() { bRefreshRequested = true; }

private:
	FReply OnCalibrateClicked();
	TSharedRef<SWidget> OnCalibrateBuildMenu();
	FText GetCalibrateButtonTooltip() const;
	const FSlateBrush* GetCalibrateButtonImage() const;
	FText GetCalibrateButtonText() const;
	FReply OnResetErrorsClicked();
	TSharedRef<SWidget> OnResetBuildMenu();
	void OnResetBufferStatClicked();
	bool IsResetBufferStatChecked() const;
	void OnClearMessageClicked();
	bool IsClearMessageChecked() const;
	void OnResetAllEvaluationTimeClicked();
	bool IsResetEvaluationChecked() const;
	FReply OnShowBuffersClicked();
	FReply OnGeneralUserSettingsClicked();
	FSlateColor GetEvaluationStateColorAndOpacity() const;
	FText GetEvaluationStateText() const;
	TSharedPtr<SWidget> OnDataListConstructContextMenu();
	bool IsSourceListSectionValid() const;
	void ApplyTimeCorrectionOnSelection();
	void ResetTimeCorrectionOnSelection();

	EVisibility ShowMessageLog() const;
	EVisibility ShowEditorPerformanceThrottlingWarning() const;
	FReply DisableEditorPerformanceThrottling();

	EVisibility GetThrobberVisibility() const;

	void BuildCalibrationArray();
	void CalibrateWithTimecode();
	void CalibrateWithTimecodeCompleted(FTimedDataMonitorCalibrationResult);
	void ApplyTimeCorrectionAll();
	ETimedDataMonitorTimeCorrectionReturnCode ApplyTimeCorrection(const FTimedDataMonitorInputIdentifier& InputIndentifier);
	FReply OnCancelCalibration();

private:
	TSharedPtr<STimedDataGenlock> TimedDataGenlockWidget;
	TSharedPtr<STimedDataTimecodeProvider> TimedDataTimecodeWidget;
	TSharedPtr<STimedDataInputListView> TimedDataSourceList;
	TSharedPtr<IMessageLogListing> MessageLogListing;

	static const int32 CalibrationArrayCount = (int32)ETimedDataMonitorEditorCalibrationType::Max;
	FUIAction CalibrationUIAction[CalibrationArrayCount];
	FSlateIcon CalibrationSlateIcon[CalibrationArrayCount];
	FText CalibrationName[CalibrationArrayCount];
	FText CalibrationTooltip[CalibrationArrayCount];

	ETimedDataMonitorEvaluationState CachedGlobalEvaluationState;

	TUniquePtr<FTimedDataMonitorCalibration> MonitorCalibration;

	bool bRefreshRequested = true;
	double LastCachedValueUpdateTime = 0.0;

	static FDelegateHandle LevelEditorTabManagerChangedHandle;
};