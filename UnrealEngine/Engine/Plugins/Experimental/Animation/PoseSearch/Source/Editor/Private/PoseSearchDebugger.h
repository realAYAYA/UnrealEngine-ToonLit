// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "CustomBoneIndexArray.h"
#include "RewindDebuggerInterface/Public/IRewindDebuggerExtension.h"
#include "RewindDebuggerInterface/Public/IRewindDebuggerView.h"
#include "RewindDebuggerInterface/Public/IRewindDebuggerViewCreator.h"
#include "Widgets/Views/SHeaderRow.h"
#include "PoseSearch/PoseSearch.h"
#include "PoseSearchDebugger.generated.h"

struct FPoseSearchDatabaseSequence;
struct FPoseSearchDatabaseBlendSpace;
class UAnimSequence;
class UBlendSpace;

namespace TraceServices { class IAnalysisSession; }
enum class EPoseSearchFeatureDomain;
class UPoseSearchDatabase;
class IUnrealInsightsModule;
class ITableRow;
class IDetailsView;
class STableViewBase;
class SVerticalBox;
class SHorizontalBox;
class SScrollBox;
class SWidgetSwitcher;
class SSearchBox;
template <typename ItemType> class SListView;

namespace UE::PoseSearch
{
	namespace DebuggerDatabaseColumns { struct IColumn; }
	struct FDebugDrawParams;
	struct FTraceMotionMatchingStateMessage;
	class FDebuggerDatabaseRowData;
} // namespace UE::PoseSearch


UCLASS()
class UPoseSearchMeshComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()
public:

	struct FUpdateContext
	{
		ESearchIndexAssetType Type = ESearchIndexAssetType::Invalid;
		const UAnimSequence* Sequence = nullptr;
		const UBlendSpace* BlendSpace = nullptr;
		float StartTime = 0.0f;
		float Time = 0.0f;
		bool bLoop = false;
		bool bMirrored = false;
		FVector BlendParameters = FVector::Zero();
		const UMirrorDataTable* MirrorDataTable = nullptr;
		TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex>* CompactPoseMirrorBones = nullptr;
		TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex>* ComponentSpaceRefRotations = nullptr;
	};

	void Refresh();
	void ResetToStart();
	void UpdatePose(const FUpdateContext& UpdateContext);
	void Initialize(const FTransform& InComponentToWorld);
	FTransform StartingTransform = FTransform::Identity;
	FTransform LastRootMotionDelta = FTransform::Identity;
};

/**
 * Used by the reflection UObject to encompass draw options for the query and database selections
 */
USTRUCT()
struct FPoseSearchDebuggerFeatureDrawOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Draw Options")
    bool bDisable = false;

	UPROPERTY(EditAnywhere, Category = "Draw Options", Meta = (EditCondition = "!bDisable"))
	bool bDrawBoneNames = false;

	UPROPERTY(EditAnywhere, Category = "Draw Options", Meta = (EditCondition = "!bDisable"))
	bool bDrawSampleLabels = false;
};

/**
 * Reflection UObject being observed in the details view panel of the debugger
 */
UCLASS()
class POSESEARCHEDITOR_API UPoseSearchDebuggerReflection : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category="Motion Matching State", Meta=(DisplayName="Current Database"))
	FString CurrentDatabaseName = "";

	/** Time since last PoseSearch jump */
	UPROPERTY(VisibleAnywhere, Category="Motion Matching State")
	float ElapsedPoseJumpTime = 0.0f;

	/** Whether it is playing the loop following the expended animation runway */
	UPROPERTY(VisibleAnywhere, Category="Motion Matching State")
	bool bFollowUpAnimation = false;
	
	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	FString AssetPlayerAssetName = "";

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float AssetPlayerTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float LastDeltaTime = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float SimLinearVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float SimAngularVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float AnimLinearVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Motion Matching State")
	float AnimAngularVelocity;

	UPROPERTY(EditAnywhere, Category="Draw Options", Meta=(DisplayName="Query"))
	FPoseSearchDebuggerFeatureDrawOptions QueryDrawOptions;

	UPROPERTY(EditAnywhere, Category="Draw Options", Meta=(DisplayName="Selected Pose"))
	FPoseSearchDebuggerFeatureDrawOptions SelectedPoseDrawOptions;

	UPROPERTY(EditAnywhere, Category = "Draw Options")
	bool bDrawActiveSkeleton = false;
	
	UPROPERTY(EditAnywhere, Category = "Draw Options")
	bool bDrawSelectedSkeleton = false;

    UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> QueryPoseVector;
    	
    UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> ActivePoseVector;

	UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> SelectedPoseVector;

	UPROPERTY(VisibleAnywhere, Category="Pose Vectors")
	TArray<float> CostVector;
};


namespace UE::PoseSearch
{

/** Draw flags for the view's debug draw */
enum class ESkeletonDrawFlags : uint32
{
	None			= 0,
	ActivePose		= 1 << 0,
	SelectedPose	= 1 << 1,
	Asset			= 1 << 2,
};
ENUM_CLASS_FLAGS(ESkeletonDrawFlags)

struct FSkeletonDrawParams
{
	ESkeletonDrawFlags Flags = ESkeletonDrawFlags::None;
};

/** Sets model selection data on row selection */
DECLARE_DELEGATE_ThreeParams(FOnPoseSelectionChanged, const UPoseSearchDatabase*, int32, float)
class FDebuggerViewModel;

/**
 * Database panel view widget of the PoseSearch debugger
 */

class SDebuggerDatabaseView : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDebuggerDatabaseView) {}
		SLATE_ARGUMENT(TWeakPtr<class SDebuggerView>, Parent)
		SLATE_EVENT(FOnPoseSelectionChanged, OnPoseSelectionChanged)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	void Update(const FTraceMotionMatchingStateMessage& State);

	const TSharedPtr<SListView<TSharedRef<FDebuggerDatabaseRowData>>>& GetActiveRow() const { return ActiveView.ListView; }
	const TSharedPtr<SListView<TSharedRef<FDebuggerDatabaseRowData>>>& GetContinuingPoseRow() const { return ContinuingPoseView.ListView; }
	const TSharedPtr<SListView<TSharedRef<FDebuggerDatabaseRowData>>>& GetDatabaseRows() const { return FilteredDatabaseView.ListView; }
	const TSharedRef<FDebuggerDatabaseRowData>& GetPoseIdxDatabaseRow(int32 PoseIdx) const;

	/** Used by database rows to acquire column-specific information */
	using FColumnMap = TMap<FName, TSharedRef<DebuggerDatabaseColumns::IColumn>>;

private:
	/** Adds a column to the existing list */
	void AddColumn(TSharedRef<DebuggerDatabaseColumns::IColumn>&& Column);

	/** Retrieves current column map, used as an attribute by rows */
	const FColumnMap* GetColumnMap() const { return &Columns; }

	/** Sorts the database by the current sort predicate, updating the view order */
	void SortDatabaseRows();

	void PopulateViewRows();

	/** Acquires sort predicate for the given column */
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;

	/** Gets active column width, used to align active and database view */
	float GetColumnWidth(const FName ColumnId) const;
	
	/** Updates the active sort predicate, setting the sorting order of all other columns to none
	 * (to be dependent on active column */
	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName & ColumnId, const EColumnSortMode::Type InSortMode);

	/** Aligns the active and database views */
	void OnColumnWidthChanged(const float NewWidth, FName ColumnId) const;

	/** Called when the text in the filter box is modified to update the filtering */
	void OnFilterTextChanged(const FText& SearchText);

	/** Row selection to update model view */
	void OnDatabaseRowSelectionChanged(TSharedPtr<FDebuggerDatabaseRowData> Row, ESelectInfo::Type SelectInfo);

	/** Generates a database row widget for the given data */
	TSharedRef<ITableRow> HandleGenerateDatabaseRow(TSharedRef<FDebuggerDatabaseRowData> Item, const TSharedRef<STableViewBase>& OwnerTable) const;
	
	/** Generates the active row widget for the given data */
	TSharedRef<ITableRow> HandleGenerateActiveRow(TSharedRef<FDebuggerDatabaseRowData> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Generates the continuing pose row widget for the given data */
	TSharedRef<ITableRow> HandleGenerateContinuingPoseRow(TSharedRef<FDebuggerDatabaseRowData> Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	TWeakPtr<SDebuggerView> ParentDebuggerViewPtr;

	FOnPoseSelectionChanged OnPoseSelectionChanged;

	/** Current column to sort by */
	FName SortColumn = "";

	/** Current sorting mode */
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;
	
	/** Column data container, used to emplace defined column structures of various types */
    FColumnMap Columns;

	TArray<FText> OldLabels;

	struct FTable
	{
		/** Header row*/
		TSharedPtr<SHeaderRow> HeaderRow;

		/** Widget for displaying the list of row objects */
		TSharedPtr<SListView<TSharedRef<FDebuggerDatabaseRowData>>> ListView;

		// @TODO: Explore options for active row other than displaying array of 1 element
		/** List of row objects */
		TArray<TSharedRef<FDebuggerDatabaseRowData>> Rows;

		/** Background style for the list view */
		FTableRowStyle RowStyle;

		/** Row color */
		FSlateBrush RowBrush;

		/** Scroll bar for the data table */
		TSharedPtr<SScrollBar> ScrollBar;
	};

	/** Active row at the top of the view */
	FTable ActiveView;

	/** Continuing pose row below Active row */
	FTable ContinuingPoseView;

	/** All database poses */
	TArray<TSharedRef<FDebuggerDatabaseRowData>> UnfilteredDatabaseRows;

	/** Database listing for filtered poses */
	FTable FilteredDatabaseView;

	/** Search box widget */
	TSharedPtr<SSearchBox> FilterBox;

	/** Text used to filter DatabaseView */
	FText FilterText;
};

/**
 * Details panel view widget of the PoseSearch debugger
 */
class SDebuggerDetailsView : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDebuggerDetailsView) {}
		SLATE_ARGUMENT(TWeakPtr<class SDebuggerView>, Parent)
	SLATE_END_ARGS()

	SDebuggerDetailsView() = default;
	virtual ~SDebuggerDetailsView() override;
	
	void Construct(const FArguments& InArgs);
	void Update(const FTraceMotionMatchingStateMessage& State) const;

	/** Get a const version of our reflection object */
	const TObjectPtr<UPoseSearchDebuggerReflection> GetReflection() const { return Reflection; }
	
private:
	/** Update our details view object with new state information */
	void UpdateReflection(const FTraceMotionMatchingStateMessage& State) const;
	
	TWeakPtr<SDebuggerView> ParentDebuggerViewPtr;

	/** Details widget constructed for the MM node */
	TSharedPtr<IDetailsView> Details;

	/** Last updated reflection data relative to MM state */
	TObjectPtr<UPoseSearchDebuggerReflection> Reflection = nullptr;
};
/** Callback to relay closing of the view to destroy the debugger instance */
DECLARE_DELEGATE_OneParam(FOnViewClosed, uint64 AnimInstanceId);

/**
 * Entire view of the PoseSearch debugger, containing all sub-widgets
 */
class SDebuggerView : public IRewindDebuggerView
{
public:
	SLATE_BEGIN_ARGS(SDebuggerView){}
		SLATE_ATTRIBUTE(TSharedPtr<FDebuggerViewModel>, ViewModel)
		SLATE_EVENT(FOnViewClosed, OnViewClosed)
	SLATE_END_ARGS()

	SDebuggerView() = default;
    virtual ~SDebuggerView() override;

	void Construct(const FArguments& InArgs, uint64 InAnimInstanceId);
	virtual void SetTimeMarker(double InTimeMarker) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FName GetName() const override;
	virtual uint64 GetObjectId() const override;

	TSharedPtr<FDebuggerViewModel> GetViewModel() const { return ViewModel.Get(); }
	TArray<TSharedRef<FDebuggerDatabaseRowData>> GetSelectedDatabaseRows() const;

private:
	/** Called each frame to draw features of the query vector & database selections */
	void DrawFeatures(const UWorld& DebuggerWorld, const FTraceMotionMatchingStateMessage& State, const FTransform& Transform, const USkinnedMeshComponent* Mesh) const;
	
	/** Check if a node selection was made, true if a node is selected */
	bool UpdateNodeSelection();

	/** Update the database and details views */
	void UpdateViews() const;

	void DrawVisualization() const;
	
	/** Returns an int32 appropriate to the index of our widget selector */
	int32 SelectView() const;

	/** Callback when a button in the selection view is clicked */
	FReply OnUpdateNodeSelection(int32 InSelectedNodeId);

	void OnPoseSelectionChanged(const UPoseSearchDatabase* Database, int32 PoseIdx, float Time);

	/** Button interaction to toggle play / stop of the asset */
	FReply TogglePlaySelectedAssets() const;

	/** Generates the message view relaying that there is no data */
	TSharedRef<SWidget> GenerateNoDataMessageView();

	/** Generates the return button to go back to the selection mode */
	TSharedRef<SHorizontalBox> GenerateReturnButtonView();

	/** Generates the entire node debugger widget, including database and details view */
	TSharedRef<SWidget> GenerateNodeDebuggerView();

	/** Pointer to the debugger instance / model for this view */
	TAttribute<TSharedPtr<FDebuggerViewModel>> ViewModel;
	
	/** Destroy the debugger instanced when closed */
	FOnViewClosed OnViewClosed;

	/** Active node being debugged */
	int32 SelectedNodeId = -1;

	/** Database view of the motion matching node */
	TSharedPtr<SDebuggerDatabaseView> DatabaseView;
	
	/** Details panel for introspecting the motion matching node */
	TSharedPtr<SDebuggerDetailsView> DetailsView;
	
	/** Node debugger view hosts the above two views */
	TSharedPtr<SSplitter> NodeDebuggerView;

	/** Selection view before node is selected */
	TSharedPtr<SVerticalBox> SelectionView;
	
	/** Gray box occluding the debugger view when simulating */
	TSharedPtr<SVerticalBox> SimulatingView;

	/** Used to switch between views in the switcher, int32 maps to index in the SWidgetSwitcher */
	enum ESwitcherViewType : int32
	{
		Selection = 0,
		Debugger = 1,
		StoppedMsg = 2,
		RecordingMsg = 3,
		NoDataMsg = 4
	} SwitcherViewType = StoppedMsg;
	
	/** Contains all the above, switches between them depending on context */
	TSharedPtr<SWidgetSwitcher> Switcher;

	/** Contains the switcher, the entire debugger view */
	TSharedPtr<SVerticalBox> DebuggerView;

	/** AnimInstance this view was created for */
	uint64 AnimInstanceId = 0;

	/** Current position of the time marker */
	double TimeMarker = -1.0;

	/** Previous position of the time marker */
	double PreviousTimeMarker = -1.0;

	/** Tracks if the current time has been updated yet (delayed) */
	bool bUpdated = false;

	/** Tracks number of consecutive frames, once it reaches threshold it will update the view */
	int32 CurrentConsecutiveFrames = 0;

	/** Once the frame count has reached this value, an update will trigger for the view */
	static constexpr int32 ConsecutiveFramesUpdateThreshold = 10;
};

class FDebuggerViewModel : public TSharedFromThis<FDebuggerViewModel>
{
public:
	explicit FDebuggerViewModel(uint64 InAnimInstanceId);
	virtual ~FDebuggerViewModel();

	// Used for view callbacks
    const FTraceMotionMatchingStateMessage* GetMotionMatchingState() const;
	const UPoseSearchDatabase* GetCurrentDatabase() const;
	const UPoseSearchSearchableAsset* GetSearchableAsset() const;
	const TArray<int32>* GetNodeIds() const;
	int32 GetNodesNum() const;
	const FTransform* GetRootTransform() const;

	/** Checks if Update must be called */
	bool HasSearchableAssetChanged() const;

	/** Update motion matching states for frame */
	void OnUpdate();
	
	/** Updates active motion matching state based on node selection */
	void OnUpdateNodeSelection(int32 InNodeId);

	/** Updates internal Skeletal Mesh Component depending on input */
	void OnDraw(FSkeletonDrawParams& DrawParams);
	
	/** Get an animation sequence from the sequence ID of the active database */
	const FPoseSearchDatabaseSequence* GetAnimSequence(int32 SequenceIdx) const;
	
	/** Get a blendspace from the blend space ID of the active database */
	const FPoseSearchDatabaseBlendSpace* GetBlendSpace(int32 BlendSpaceIdx) const;

	/** Sets the selected pose skeleton*/
	void ShowSelectedSkeleton(const UPoseSearchDatabase* Database, int32 DbPoseIdx, float Time);
	
	/** Clears the selected pose skeleton */
	void ClearSelectedSkeleton();
	
	/** Plays the selected row upon button press */
	void PlaySelection(int32 PoseIdx, float Time);

	/** Stops the playing selection upon button press */
	void StopSelection();
	
	/** If there is an asset selection playing */
	bool IsPlayingSelections() const { return AssetData.bActive; }
	
	/** Play Rate of the asset selection player */
	void ChangePlayRate(float PlayRate) { AssetPlayRate = PlayRate; }
	
	/** Current play rate of the asset selection player */
	float GetPlayRate() const { return AssetPlayRate; }
	
	void SetVerbose(bool bVerbose) { bIsVerbose = bVerbose; }

	bool IsVerbose() const { return bIsVerbose; }

	/** Callback to reset debug skeletons for the active world */
	void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);

	/** Updates the current playing asset */
	void UpdateAsset();

	const USkinnedMeshComponent* GetMeshComponent() const;

private:
	/** Update the list of states for this frame */
	void UpdateFromTimeline();

	/** Populates arrays used for mirroring the animation pose */
	void FillCompactPoseAndComponentRefRotations();
	
	/** List of all Node IDs associated with motion matching states */
	TArray<int32> NodeIds;
	
	/** List of all updated motion matching states per node */
	TArray<const FTraceMotionMatchingStateMessage*> MotionMatchingStates;
	
	/** Currently active motion matching state based on node selection in the view */
	const FTraceMotionMatchingStateMessage* ActiveMotionMatchingState = nullptr;

	/** Active motion matching state's searchable asset */
	uint64 SearchableAssetId = 0;

	/** Current Skeletal Mesh Component Id for the AnimInstance */
	uint64 SkeletalMeshComponentId = 0;

	/** Currently active root transform on the skeletal mesh */
	const FTransform* RootTransform = nullptr;

	/** Pointer to the active rewind debugger in the scene */
	TAttribute<const IRewindDebugger*> RewindDebugger;

	/** Anim Instance associated with this debugger instance */
	uint64 AnimInstanceId = 0;

	/** Compact pose format of Mirror Bone Map */
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;

	/** Pre-calculated component space rotations of reference pose */
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;

	/** Debug visualization skeleton actor */
	struct FSkeleton
	{
		/** Actor object for the skeleton */
		TWeakObjectPtr<AActor> Actor = nullptr;

		/** Derived skeletal mesh for setting the skeleton in the scene */
		TWeakObjectPtr<UPoseSearchMeshComponent> Component = nullptr;
		
		/** Type of the asset being played */
		ESearchIndexAssetType Type = ESearchIndexAssetType::Invalid;

		/** Source database for this skeleton  */
		TWeakObjectPtr<const UPoseSearchDatabase> SourceDatabase = nullptr;

		/** Source asset for this skeleton */
		int32 AssetIdx = 0;
		
		/** Time in the sequence this skeleton is accessing */
		float Time = 0.0f;

		/** If this asset should be mirrored */
		bool bMirrored = false;

		/** Blend Parameters if asset is a BlendSpace */
		FVector BlendParameters = FVector::Zero();

		const FPoseSearchDatabaseSequence* GetAnimSequence() const;
		const FPoseSearchDatabaseBlendSpace* GetBlendSpace() const;
	};
	
	/** Index for each type of skeleton we store for debug visualization */
	enum ESkeletonIndex
	{
		ActivePose = 0,
		SelectedPose,
		Asset,

		Num
	};

	/** Skeleton container for each type */
	TArray<FSkeleton, TFixedAllocator<ESkeletonIndex::Num>> Skeletons;

	/** Whether the skeletons have been initialized for this world */
	bool bSkeletonsInitialized = false;
	
	/** If we currently have a selection active in the view */
	bool bSelecting = false;
	
	/** Data of the active playing asset */
	struct FAsset
	{
		/** How long to stop upon reaching target */
		static constexpr float StopDuration = 2.0f;

		/** Time since the start of play */
		float AccumulatedTime = 0.0f;

		/** Start time of the asset */
		float StartTime = 0.0f;

		/** If we are currently playing a asset */
		bool bActive = false;

	} AssetData;
	
	/** Current play rate of the asset selection player */
	float AssetPlayRate = 1.0f;

	bool bIsVerbose = false;

	/** Limits some public API */
	friend class FDebugger;
};


/**
 * PoseSearch debugger, containing the data to be acquired and relayed to the view
 */
class FDebugger : public TSharedFromThis<FDebugger>, public IRewindDebuggerExtension
{
public:
	virtual void Update(float DeltaTime, IRewindDebugger* InRewindDebugger) override;
	virtual ~FDebugger() = default;

	virtual void RecordingStarted(IRewindDebugger* RewindDebugger) override;
	virtual void RecordingStopped(IRewindDebugger* RewindDebugger) override;

	static FDebugger* Get() { return Debugger; }
	static void Initialize();
	static void Shutdown();
	static const FName ModularFeatureName;

	// Shared data from the Rewind Debugger singleton
	static bool IsPIESimulating();
	static bool IsRecording();
	static double GetRecordingDuration();
	static UWorld* GetWorld();
	static const IRewindDebugger* GetRewindDebugger();

	/** Generates the slate debugger view widget */
	TSharedPtr<SDebuggerView> GenerateInstance(uint64 InAnimInstanceId);

private:
	/** Removes the reference from the model array when closed, destroying the model */
	static void OnViewClosed(uint64 InAnimInstanceId);

	/** Acquire view model from the array */
	static TSharedPtr<FDebuggerViewModel> GetViewModel(uint64 InAnimInstanceId);

	/** Last stored Rewind Debugger */
	const IRewindDebugger* RewindDebugger = nullptr;

	/** List of all active debugger instances */
	TArray<TSharedRef<FDebuggerViewModel>> ViewModels;
	
	/** Internal instance */
	static FDebugger* Debugger;
};

/**
 * Creates the slate widgets associated with the PoseSearch debugger
 * when prompted by the Rewind Debugger
 */
class FDebuggerViewCreator : public IRewindDebuggerViewCreator
{
public:
	virtual ~FDebuggerViewCreator() = default;
	virtual FName GetName() const override;
	virtual FText GetTitle() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual FName GetTargetTypeName() const override;
	
	/** Creates the PoseSearch Slate view for the provided AnimInstance */
	virtual TSharedPtr<IRewindDebuggerView> CreateDebugView(uint64 ObjectId, double CurrentTime, const TraceServices::IAnalysisSession& InAnalysisSession) const override;
	virtual bool HasDebugInfo(uint64 ObjectId) const override;
};


} // namespace UE::PoseSearch
