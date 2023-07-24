// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Misc/FilterCollection.h"
#include "Misc/TextFilter.h"
#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Views/SListView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FActiveTimerHandle;
class ITableRow;
class SEditableTextBox;
class SNotificationList;
class SSearchBox;
class STableViewBase;
class SVerticalBox;

namespace Insights
{
	class FStoreBrowser;
	struct FStoreBrowserTraceInfo;
}

namespace TraceStoreColumns
{
	static const FName Date(TEXT("Date"));
	static const FName Name(TEXT("Name"));
	static const FName Uri(TEXT("Uri"));
	static const FName Platform(TEXT("Platform"));
	static const FName AppName(TEXT("AppName"));
	static const FName BuildConfig(TEXT("BuildConfig"));
	static const FName BuildTarget(TEXT("BuildTarget"));
	static const FName Size(TEXT("Size"));
	static const FName Status(TEXT("Status"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Type definition for shared pointers to instances of SNotificationItem. */
typedef TSharedPtr<class SNotificationItem> SNotificationItemPtr;

/** Type definition for shared references to instances of SNotificationItem. */
typedef TSharedRef<class SNotificationItem> SNotificationItemRef;

/** Type definition for weak references to instances of SNotificationItem. */
typedef TWeakPtr<class SNotificationItem> SNotificationItemWeak;

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FTraceViewModel
{
	uint32 TraceId = 0;
	int32 TraceIndex = -1; // debug

	uint64 ChangeSerial = 0;

	FText Name;
	FText Uri;

	FDateTime Timestamp = 0;
	uint64 Size = 0;

	FText Platform;
	FText AppName;
	FText CommandLine;
	FText Branch;
	FText BuildVersion;
	uint32 Changelist = 0;
	EBuildConfiguration ConfigurationType = EBuildConfiguration::Unknown;
	EBuildTargetType TargetType = EBuildTargetType::Unknown;

	bool bIsMetadataUpdated = false;
	bool bIsRenaming = false;
	bool bIsLive = false;
	uint32 IpAddress = 0;

	TWeakPtr<SEditableTextBox> RenameTextBox;

	FTraceViewModel() = default;

	static FDateTime ConvertTimestamp(uint64 InTimestamp)
	{
		return FDateTime(static_cast<int64>(InTimestamp));
	}

	static FText AnsiStringViewToText(const FAnsiStringView& AnsiStringView)
	{
		FString FatString(AnsiStringView.Len(), AnsiStringView.GetData());
		return FText::FromString(FatString);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Filters

/** The filter collection - used for updating the list of trace sessions. */
typedef TFilterCollection<const FTraceViewModel&> FTraceViewModelFilterCollection;

/** The text based filter - used for updating the list of trace sessions. */
typedef TTextFilter<const FTraceViewModel&> FTraceTextFilter;

template<typename TSetType>
class TTraceSetFilter : public IFilter<const FTraceViewModel&>, public TSharedFromThis< TTraceSetFilter<TSetType> >
{
public:
	TTraceSetFilter();
	virtual ~TTraceSetFilter() {};

	typedef const FTraceViewModel& ItemType;

	/** Broadcasts anytime the restrictions of the Filter changes. */
	DECLARE_DERIVED_EVENT(TTraceSetFilter, IFilter<ItemType>::FChangedEvent, FChangedEvent);
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }

	/** Returns whether the specified Trace passes the Filter's restrictions. */
	virtual bool PassesFilter(const FTraceViewModel& InTrace) const override
	{
		return FilterSet.IsEmpty() || !FilterSet.Contains(GetFilterValueForTrace(InTrace));
	}

	bool IsEmpty() const { return FilterSet.IsEmpty(); }

	void Reset() { FilterSet.Reset(); }

	virtual void BuildMenu(FMenuBuilder& InMenuBuilder, class STraceStoreWindow& InWindow);

protected:
	virtual TSetType GetFilterValueForTrace(const FTraceViewModel& InTrace) const = 0;
	virtual FText ValueToText(const TSetType Value) const = 0;

protected:
	/**	The event that fires whenever new search terms are provided */
	FChangedEvent ChangedEvent;

	/** The set of values used to filter */
	TSet<TSetType> FilterSet;

	FText ToggleAllActionLabel;
	FText ToggleAllActionTooltip;
	FText UndefinedValueLabel;
};

class FTraceFilterByStringSet : public TTraceSetFilter<FString>
{
protected:
	virtual FText ValueToText(const FString InValue) const override
	{
		return InValue.IsEmpty() ? UndefinedValueLabel : FText::FromString(InValue);
	}
};

class FTraceFilterByPlatform : public FTraceFilterByStringSet
{
public:
	FTraceFilterByPlatform();

protected:
	virtual FString GetFilterValueForTrace(const FTraceViewModel& InTrace) const override
	{
		return InTrace.Platform.ToString();
	}
};

class FTraceFilterByAppName : public FTraceFilterByStringSet
{
public:
	FTraceFilterByAppName();

protected:
	virtual FString GetFilterValueForTrace(const FTraceViewModel& InTrace) const override
	{
		return InTrace.AppName.ToString();
	}
};

class FTraceFilterByBuildConfig : public TTraceSetFilter<uint8>
{
public:
	FTraceFilterByBuildConfig();

protected:
	virtual uint8 GetFilterValueForTrace(const FTraceViewModel& InTrace) const override
	{
		return (uint8)InTrace.ConfigurationType;
	}

	virtual FText ValueToText(const uint8 InValue) const override
	{
		const TCHAR* Str = LexToString((EBuildConfiguration)InValue);
		return Str ? FText::FromString(Str) : UndefinedValueLabel;
	}
};

class FTraceFilterByBuildTarget : public TTraceSetFilter<uint8>
{
public:
	FTraceFilterByBuildTarget();

protected:
	virtual uint8 GetFilterValueForTrace(const FTraceViewModel& InTrace) const override
	{
		return (uint8)InTrace.TargetType;
	}

	virtual FText ValueToText(const uint8 InValue) const override
	{
		const TCHAR* Str = LexToString((EBuildTargetType)InValue);
		return Str ? FText::FromString(Str) : UndefinedValueLabel;
	}
};

class FTraceFilterByBranch : public FTraceFilterByStringSet
{
public:
	FTraceFilterByBranch();

protected:
	virtual FString GetFilterValueForTrace(const FTraceViewModel& InTrace) const override
	{
		return InTrace.Branch.ToString();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Implements the Trace Store window. */
class STraceStoreWindow : public SCompoundWidget
{
	friend class STraceListRow;

public:
	/** Default constructor. */
	STraceStoreWindow();

	/** Virtual destructor. */
	virtual ~STraceStoreWindow();

	SLATE_BEGIN_ARGS(STraceStoreWindow) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);

	void OpenSettings();
	void CloseSettings();

	void GetExtraCommandLineParams(FString& OutParams) const;

	void SetEnableAutomaticTesting(bool InValue) { bEnableAutomaticTesting = InValue; };
	bool GetEnableAutomaticTesting() const { return bEnableAutomaticTesting; };

	void SetEnableDebugTools(bool InValue) { bEnableDebugTools = InValue; };
	bool GetEnableDebugTools() const { return bEnableDebugTools; };

	void SetStartProcessWithStompMalloc(bool InValue) { bStartProcessWithStompMalloc = InValue; };
	bool GetStartProcessWithStompMalloc() const { return bStartProcessWithStompMalloc; };

	void OnFilterChanged();
	const TArray<TSharedPtr<FTraceViewModel>>& GetAllAvailableTraces() const;

private:
	TSharedRef<SWidget> ConstructFiltersToolbar();
	TSharedRef<SWidget> ConstructSessionsPanel();
	TSharedRef<SWidget> ConstructLoadPanel();
	TSharedRef<SWidget> ConstructTraceStoreDirectoryPanel();
	TSharedRef<SWidget> ConstructAutoStartPanel();

	/** Generate a new row for the Traces list view. */
	TSharedRef<ITableRow> TraceList_OnGenerateRow(TSharedPtr<FTraceViewModel> InTrace, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<SWidget> TraceList_GetMenuContent();

	bool CanEditTraceFile() const;
	void RenameTraceFile();
	void DeleteTraceFile();

	//////////////////////////////////////////////////
	// "Starting Analysis" Splash Screen

	void ShowSplashScreenOverlay();
	void TickSplashScreenOverlay(const float InDeltaTime);
	float SplashScreenOverlayOpacity() const;

	EVisibility SplashScreenOverlay_Visibility() const;
	FSlateColor SplashScreenOverlay_ColorAndOpacity() const;
	FSlateColor SplashScreenOverlay_TextColorAndOpacity() const;
	FText GetSplashScreenOverlayText() const;

	//////////////////////////////////////////////////

	bool Open_IsEnabled() const;
	FReply Open_OnClicked();

	void OpenTraceFile();
	void OpenTraceFile(const FString& InTraceFile);
	void OpenTraceSession(TSharedPtr<FTraceViewModel> InTrace);
	void OpenTraceSession(uint32 InTraceId);

	//////////////////////////////////////////////////
	// Traces

	TSharedRef<SWidget> MakeTraceListMenu();

	TSharedRef<SWidget> MakePlatformColumnHeaderMenu();
	TSharedRef<SWidget> MakePlatformFilterMenu();
	void BuildPlatformFilterSubMenu(FMenuBuilder& InMenuBuilder);

	TSharedRef<SWidget> MakeAppNameColumnHeaderMenu();
	TSharedRef<SWidget> MakeAppNameFilterMenu();
	void BuildAppNameFilterSubMenu(FMenuBuilder& InMenuBuilder);

	TSharedRef<SWidget> MakeBuildConfigColumnHeaderMenu();
	TSharedRef<SWidget> MakeBuildConfigFilterMenu();
	void BuildBuildConfigFilterSubMenu(FMenuBuilder& InMenuBuilder);

	TSharedRef<SWidget> MakeBuildTargetColumnHeaderMenu();
	TSharedRef<SWidget> MakeBuildTargetFilterMenu();
	void BuildBuildTargetFilterSubMenu(FMenuBuilder& InMenuBuilder);

	TSharedRef<SWidget> MakeBranchFilterMenu();
	void BuildBranchFilterSubMenu(FMenuBuilder& InMenuBuilder);

	FReply RefreshTraces_OnClicked();

	void RefreshTraceList();
	void UpdateTrace(FTraceViewModel& InOutTrace, const Insights::FStoreBrowserTraceInfo& InSourceTrace);
	void OnTraceListChanged();

	void TraceList_OnSelectionChanged(TSharedPtr<FTraceViewModel> InTrace, ESelectInfo::Type SelectInfo);
	void TraceList_OnMouseButtonDoubleClick(TSharedPtr<FTraceViewModel> InTrace);

	//////////////////////////////////////////////////
	// Auto Start Analysis

	ECheckBoxState AutoStart_IsChecked() const;
	void AutoStart_OnCheckStateChanged(ECheckBoxState NewState);

	//////////////////////////////////////////////////
	// Trace Store Directory

	FText GetTraceStoreDirectory() const;
	FReply ExploreTraceStoreDirectory_OnClicked();

	//////////////////////////////////////////////////

	/** Updates the amount of time the profiler has been active. */
	EActiveTimerReturnType UpdateActiveDuration(double InCurrentTime, float InDeltaTime);

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/**
	 * The system will use this event to notify a widget that the cursor has entered it. This event is NOT bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/**
	 * The system will use this event to notify a widget that the cursor has left it. This event is NOT bubbled.
	 *
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	/**
	 * Called after a key is pressed when this widget has focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)  override;

	//////////////////////////////////////////////////
	// Filtering

	void FilterByNameSearchBox_OnTextChanged(const FText& InFilterText);

	FText GetFilterStatsText() const { return FilterStatsText; }

	void CreateFilters();

	/**
	 * Populates OutSearchStrings with the strings that should be used in searching.
	 *
	 * @param GroupOrStatNodePtr - the group and stat node to get a text description from.
	 * @param OutSearchStrings   - an array of strings to use in searching.
	 *
	 */
	void HandleItemToStringArray(const FTraceViewModel& InTrace, TArray<FString>& OutSearchStrings) const;

	void UpdateFiltering();

	void UpdateFilterStatsText();

	//////////////////////////////////////////////////
	// Sorting

	EColumnSortMode::Type GetSortModeForColumn(const FName ColumnId) const;
	void OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

	void UpdateSorting();

	//////////////////////////////////////////////////

	void UpdateTraceListView();

	void ShowSuccessMessage(FText& InMessage);
	void ShowFailMessage(FText& InMessage);

private:
	/** Widget for the non-intrusive notifications. */
	TSharedPtr<SNotificationList> NotificationList;

	/** Overlay slot which contains the profiler settings widget. */
	SOverlay::FOverlaySlot* OverlaySettingsSlot;

	/** The number of seconds the profiler has been active */
	float DurationActive;

	/** The handle to the active update duration tick */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	TSharedPtr<SVerticalBox> MainContentPanel;

	//////////////////////////////////////////////////

	TUniquePtr<Insights::FStoreBrowser> StoreBrowser;
	uint64 TracesChangeSerial;

	TArray<TSharedPtr<FTraceViewModel>> TraceViewModels; // all available trace view models
	TArray<TSharedPtr<FTraceViewModel>> FilteredTraceViewModels; // the filtered list of trace view models
	TMap<uint32, TSharedPtr<FTraceViewModel>> TraceViewModelMap;

	TSharedPtr<SListView<TSharedPtr<FTraceViewModel>>> TraceListView;
	TSharedPtr<FTraceViewModel> SelectedTrace;
	bool bIsUserSelectedTrace;

	//////////////////////////////////////////////////
	// Filtering

	TSharedPtr<FTraceViewModelFilterCollection> Filters;

	bool bSearchByCommandLine;
	TSharedPtr<SSearchBox> FilterByNameSearchBox;
	TSharedPtr<FTraceTextFilter> FilterByName;

	TSharedPtr<FTraceFilterByPlatform> FilterByPlatform;
	TSharedPtr<FTraceFilterByAppName> FilterByAppName;
	TSharedPtr<FTraceFilterByBuildConfig> FilterByBuildConfig;
	TSharedPtr<FTraceFilterByBuildTarget> FilterByBuildTarget;
	TSharedPtr<FTraceFilterByBranch> FilterByBranch;

	bool bFilterStatsTextIsDirty;
	FText FilterStatsText;

	//////////////////////////////////////////////////
	// Sorting

	FName SortColumn;
	EColumnSortMode::Type SortMode;

	//////////////////////////////////////////////////
	// Auto-start functionality

	bool bAutoStartAnalysisForLiveSessions;
	TArray<uint32> AutoStartedSessions; // tracks sessions that were auto started (in order to not start them again)

	TSharedPtr<SSearchBox> AutoStartPlatformFilter;
	TSharedPtr<SSearchBox> AutoStartAppNameFilter;
	EBuildConfiguration AutoStartConfigurationTypeFilter;
	EBuildTargetType AutoStartTargetTypeFilter;

	//////////////////////////////////////////////////

	FString SplashScreenOverlayTraceFile;
	float SplashScreenOverlayFadeTime;

	bool bEnableAutomaticTesting = false;
	bool bEnableDebugTools = false;
	bool bStartProcessWithStompMalloc = false;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Implements the Connection window. */
class SConnectionWindow : public SCompoundWidget
{
public:
	/** Default constructor. */
	SConnectionWindow();

	/** Virtual destructor. */
	virtual ~SConnectionWindow();

	SLATE_BEGIN_ARGS(SConnectionWindow) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> ConstructConnectPanel();
	FReply Connect_OnClicked();

private:
	TSharedPtr<SVerticalBox> MainContentPanel;
	TSharedPtr<SEditableTextBox> TraceRecorderAddressTextBox;
	TSharedPtr<SEditableTextBox> RunningInstanceAddressTextBox;
	TSharedPtr<SEditableTextBox> ChannelsTextBox;

	/** Widget for the non-intrusive notifications. */
	TSharedPtr<SNotificationList> NotificationList;

	FGraphEventRef ConnectTask;

	std::atomic<bool> bIsConnecting;
	std::atomic<bool> bIsConnectedSuccessfully;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Implements the Launcher window. */
class SLauncherWindow : public SCompoundWidget
{
public:
	/** Default constructor. */
	SLauncherWindow();

	/** Virtual destructor. */
	virtual ~SLauncherWindow();

	SLATE_BEGIN_ARGS(SLauncherWindow) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
