// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"

class FLiveSessionTracker;
class FMenuBuilder;
class FUICommandList;
class STraceServerControl;

TSharedRef<SWidget> CreateInsightsStatusBarWidget();

struct FTraceFileInfo
{
	FString FilePath;
	FDateTime ModifiedTime;
	bool bIsFromTraceStore;

	bool operator <(const FTraceFileInfo& rhs)
	{
		return this->ModifiedTime > rhs.ModifiedTime;
	}
};

/**
 *  Status bar widget for Unreal Insights.
 *  Shows buttons to start tracing either to a file or to the trace store and allows saving a snapshot to file.
 */
class SInsightsStatusBarWidget : public SCompoundWidget
{
private:
	enum class ETraceDestination : uint32
	{
		TraceStore = 0,
		File = 1
	};

	struct FChannelData
	{
		FString Name;
		bool bIsEnabled = false;
		bool bIsReadOnly = false;
	};

	enum class ESelectLatestTraceCriteria : uint32
	{
		None,
		CreatedTime,
		ModifiedTime,
	};

public:
	SLATE_BEGIN_ARGS(SInsightsStatusBarWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FText	GetTitleToolTipText() const;
	
	FSlateColor GetRecordingButtonColor() const;
	FSlateColor GetRecordingButtonOutlineColor() const;
	FText GetRecordingButtonTooltipText() const;

	void LaunchUnrealInsights_OnClicked();
	
	void OpenLiveSession_OnClicked();
	void OpenLiveSession(const FString& InTraceDestination);

	void OpenProfilingDirectory_OnClicked();
	void OpenProfilingDirectory();

	void OpenTraceStoreDirectory_OnClicked();
	void OpenTraceStoreDirectory(ESelectLatestTraceCriteria Criteria);

	void OpenLatestTraceFromFolder(const FString& InFolder, ESelectLatestTraceCriteria InCriteria);
	FString GetLatestTraceFileFromFolder(const FString& InFolder, ESelectLatestTraceCriteria InCriteria);

	void SetTraceDestination_Execute(ETraceDestination InDestination);
	bool SetTraceDestination_CanExecute();
	bool SetTraceDestination_IsChecked(ETraceDestination InDestination);

	void SaveSnapshot();
	bool SaveSnapshot_CanExecute();

	FText GetTraceMenuItemText() const;
	FText GetTraceMenuItemTooltipText() const;
	void ToggleTrace_OnClicked();

	bool PauseTrace_CanExecute();
	FText GetPauseTraceMenuItemTooltipText() const;
	void TogglePauseTrace_OnClicked();

	EVisibility GetStartTraceIconVisibility() const;
	EVisibility GetStopTraceIconVisibility() const;

	bool StartTracing();

	TSharedRef<SWidget> MakeTraceMenu();
	void Channels_BuildMenu(FMenuBuilder& MenuBuilder);
	void Traces_BuildMenu(FMenuBuilder& MenuBuilder);

	void LogMessage(const FText& Text);
	void ShowNotification(const FText& Text, const FText& SubText);

	void SetTraceChannels(const TCHAR* InChannels);
	bool IsPresetSet(const TCHAR* InChannels) const;

	bool GetBooleanSettingValue(const TCHAR* InSettingName);
	void ToggleBooleanSettingValue(const TCHAR* InSettingName);

	void OnTraceStarted(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);
	void OnTraceStopped(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);
	void OnSnapshotSaved(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);

	void CacheTraceStorePath();

	void ToggleChannel_Execute(int32 Index);
	bool ToggleChannel_IsChecked(int32 Index);

	void CreateChannelsInfo();
	void UpdateChannelsInfo();

	void InitCommandList();

	bool TraceScreenshot_CanExecute();
	void TraceScreenshot_Execute();

	bool TraceBookmark_CanExecute();
	void TraceBookmark_Execute();

	void PopulateRecentTracesList();

	void OpenTrace(int32 Index);

private:
	static const TCHAR* DefaultPreset;
	static const TCHAR* MemoryPreset;
	static const TCHAR* TaskGraphPreset;
	static const TCHAR* ContextSwitchesPreset;

	static const TCHAR* SettingsCategory;
	static const TCHAR* OpenLiveSessionOnTraceStartSettingName;
	static const TCHAR* OpenInsightsAfterTraceSettingName;
	static const TCHAR* ShowInExplorerAfterTraceSettingName;

	ETraceDestination TraceDestination = ETraceDestination::TraceStore;
	bool bIsTraceRecordButtonHovered = false;
	mutable double ConnectionStartTime = 0.0f;

	FString TraceStorePath;

	TArray<FChannelData> ChannelsInfo;
	bool bShouldUpdateChannels = false;

	TSharedPtr<FLiveSessionTracker> LiveSessionTracker;

	TSharedPtr<FUICommandList> CommandList;
	
	TArray<STraceServerControl> ServerControls;

	TArray<TSharedPtr<FTraceFileInfo>> Traces;
	FName LogListingName;
};
