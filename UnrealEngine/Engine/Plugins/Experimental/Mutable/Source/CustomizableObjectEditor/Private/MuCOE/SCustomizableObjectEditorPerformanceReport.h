// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Misc/NotifyHook.h"
#include "Misc/Optional.h"
#include "MuCOE/StatePerformanceTesting.h"
#include "MuCOE/StressTest.h"
#include "MuCOE/Widgets/SMutableExpandableTableRow.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/SCompoundWidget.h"

#include "SCustomizableObjectEditorPerformanceReport.generated.h"

class ITableRow;
class STableViewBase;
class SWidget;
class UCustomizableObject;
class UCustomizableObjectInstance;
struct FFrame;
struct FGeometry;
struct FSlateBrush;
struct FTableRowStyle;


// Instances performing worst in some aspect
UCLASS()
class UWorstCasePerformanceReportInstance : public UObject
{
public:
	GENERATED_BODY()

	enum EPerformanceReportWorstCaseType {
		LARGEST_TEXTURE_SIZE,
		MOST_TRIANGLES,
		SLOWEST_UPDATE_ENTER_STATE,
		SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE
	};

	// The instance that presented this worst case.
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Transient, NoClear, Category = "Stats|PerformanceReport", meta = (DisplayName = "Instance", ColumnWidth = "90")/*"PerformanceReport|BiggestTextureSize"*/)
	TObjectPtr<class UCustomizableObjectInstance> WorstCaseInstance = nullptr;

	// What worst case does this represent
	EPerformanceReportWorstCaseType WorstCaseType = LARGEST_TEXTURE_SIZE;

	// LOD number where this instance generated the worst case  (currently only relevant when LARGEST_TEXTURE_SIZE or MOST_TRIANGLES)
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Transient, NoClear, Category = "Stats|PerformanceReport", meta = (DisplayName = "LOD", ColumnWidth = "10", DisplayRight = "true")/*"PerformanceReport|BiggestTextureSize"*/)
	int32 LOD = INDEX_NONE;

	// Total texture memory size that this instance had when the worst case was recorded (currently only relevant when LARGEST_TEXTURE_SIZE)
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Transient, NoClear, Category = "Stats|PerformanceReport|Texture", meta = (DisplayName = "Textures Total Size", ColumnWidth = "60", DisplayRight = "true"))
	int32 TextureBytes = -1;

	// Amount of triangles this instance had when the worst case was recorded (currently only relevant when MOST_TRIANGLES)
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Transient, NoClear, Category = "Stats|PerformanceReport|Geometry", meta = (DisplayName = "Number of Triangles", ColumnWidth = "60", DisplayRight = "true"))
	int32 NumFaces = -1;

	// How long it took this instance to update when the worst case was recorded (currently only relevant when SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE or SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE)
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Transient, NoClear, Category = "Stats|PerformanceReport|Time", meta = (DisplayName = "Update Time", ColumnWidth = "60", DisplayRight = "true"))
	float UpdateTime = -1.0;

	// State CO index where this instance took most cycles to update (currently only relevant when SLOWEST_UPDATE_ENTER_STATE or SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE)
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Transient, NoClear, Category = "Stats|PerformanceReport|Time", meta = (DisplayName = "State ID", ColumnWidth = "60", DisplayRight = "true"))
	int32 LongestUpdateTimeStateIndex = INDEX_NONE;

	// Parameter CO index where this instance took most cycles to update (currently only relevant when SLOWEST_UPDATE_PARAM_IN_RELEVANT_STATE)
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Transient, NoClear, Category = "Stats|PerformanceReport|Time", meta = (DisplayName = "Parameter ID", ColumnWidth = "60", DisplayRight = "true"))
	int32 LongestUpdateTimeParameterIndexInCO = INDEX_NONE;

	// Measured data for the current worst case, the type depends on which is the current WorstCaseType
	MeasuredData MeasuredDataForThisWorstCase;

	EVisibility SubVisibility = EVisibility::Collapsed;

	// TODO: Obtain all data in all cases
	//MaterialInfoMap MapMaterialParam;

	void SetInstance(class UCustomizableObjectInstance* UpdatedInstance);
};


UCLASS()
class UPerformanceReportHelper : public UObject
{
public:
	GENERATED_BODY()
	// Standard constructor
	UPerformanceReportHelper() {}

	// Method to assign for the callback
	UFUNCTION()
	void DelegatedCallback(UCustomizableObjectInstance* Instance);

	// Properties to be updated once the instance has been updated
	class SCustomizableObjecEditorPerformanceReport* PerformanceReport = nullptr;
};


// Editor GUI that executes all relevant testing of a CO and presents and prints results
class SCustomizableObjecEditorPerformanceReport : public SCompoundWidget, public FNotifyHook, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SCustomizableObjecEditorPerformanceReport){}
		SLATE_ARGUMENT(UCustomizableObject *,CustomizableObject)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCustomizableObjecEditorPerformanceReport();

	// FSerializableObject interface
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SCustomizableObjecEditorPerformanceReport");
	}

	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void StopTests();

	// Force the re-start of the performance report
	FReply RunPerformanceReport();
	// Force the re-start of the performance report and provides the instance to be tested
	FReply RunPerformanceReport(UCustomizableObject* ObjectToTest);
	// Select and view the details of the slowest instance found up to this moment
	FReply SelectSlowestInstance();

	bool IsGetWorstInstanceButtonEnabled() const;

	FReply ResetPerformanceReportOptions();

private:

	// Customizable object whose performance is being analyzed
	UCustomizableObject* ReportSubject;

	// Number of LODs that the Customizable Object has
	uint32 NumLOD;

	// Number of States that the Customizable Object has
	uint32 NumStates;

	// Currently focused report case and instance
	UWorstCasePerformanceReportInstance* CurrentReportWorstCase = nullptr;
	class UCustomizableObjectInstance* CurrentReportInstance = nullptr;

	// Stress tester ================================================================================================================================================================
	TSharedPtr<FRunningStressTest> StressTest;

	int StressTestOptionInstanceCount;
	TSharedPtr<class SNumericEntryBox<int32>> StressTestOptionInstanceCountEntry;
	TOptional<int32> GetStressTestOptionInstanceCount() const;
	void OnStressTestOptionInstanceCountChanged(int32 Value);

	int StressTestOptionCreateInstanceTime;
	TSharedPtr<class SNumericEntryBox<int32>> StressTestOptionCreateInstanceTimeEntry;
	TOptional<int32> GetStressTestOptionCreateInstanceTime() const;
	void OnStressTestOptionCreateInstanceTimeChanged(int32 Value);

	int StressTestOptionCreateInstanceTimeVar;
	TSharedPtr<class SNumericEntryBox<int32>> StressTestOptionCreateInstanceTimeVarEntry;
	TOptional<int32> GetStressTestOptionCreateInstanceTimeVar() const;
	void OnStressTestOptionCreateInstanceTimeVarChanged(int32 Value);

	int StressTestOptionInstanceUpdateTime;
	TSharedPtr<class SNumericEntryBox<int32>> StressTestOptionInstanceUpdateTimeEntry;
	TOptional<int32> GetStressTestOptionInstanceUpdateTime() const;
	void OnStressTestOptionInstanceUpdateTimeChanged(int32 Value);

	int StressTestOptionInstanceUpdateTimeVar;
	TSharedPtr<class SNumericEntryBox<int32>> StressTestOptionInstanceUpdateTimeVarEntry;
	TOptional<int32> GetStressTestOptionInstanceUpdateTimeVar() const;
	void OnStressTestOptionInstanceUpdateTimeVarChanged(int32 Value);

	int StressTestOptionInstanceLifeTime;
	TSharedPtr<class SNumericEntryBox<int32>> StressTestOptionInstanceLifeTimeEntry;
	TOptional<int32> GetStressTestOptionInstanceLifeTime() const;
	void OnStressTestOptionInstanceLifeTimeChanged(int32 Value);

	int StressTestOptionInstanceLifeTimeVar;
	TSharedPtr<class SNumericEntryBox<int32>> StressTestOptionInstanceLifeTimeVarEntry;
	TOptional<int32> GetStressTestOptionInstanceLifeTimeVar() const;
	void OnStressTestOptionInstanceLifeTimeVarChanged(int32 Value);

	// State tester =================================================================================================================================================================
	TSharedPtr<class FTestingCustomizableObject> StateTest;

	// Texture Analyzer =============================================================================================================================================================
	TSharedPtr<class SCustomizableObjecEditorTextureAnalyzer> PerformanceReportTextureAnalyzer;

	// Worst cases ==================================================================================================================================================================
	TArray<UWorstCasePerformanceReportInstance*> WorstTextureInstances;	// By LOD
	TArray<UWorstCasePerformanceReportInstance*> WorstGeometryInstances;	// By LOD
	const UWorstCasePerformanceReportInstance* WorstTimeInstance;	// Slowest instance in any test that checks for time for this report
	TArray<TArray<UWorstCasePerformanceReportInstance*>> WorstTimeStateInstances;	// By LOD then by state index
	TArray<TArray<TArray<UWorstCasePerformanceReportInstance*>>> WorstTimeParameterInstances;	// By LOD then by state index then all parameters relevant for that state (not ordered)
	void ResetSavedData();
	void WorstFacesFound(uint32 CurrentLODNumFaces, uint32 LOD, class UCustomizableObjectInstance* Instance, MeasuredData* data);
	void WorstTextureFound(MaterialInfoMap MapMaterialParam, uint32 CurrentLODTextureSize, uint32 LOD, class UCustomizableObjectInstance* Instance, MeasuredData* data);
	void WorstTimeFound(float CurrentInstanceGenerationTime, int32 LongesTimeStateIndex, int32 LongesTimeParameterIndexInCO, uint32 LOD, class UCustomizableObjectInstance* Instance, MeasuredData* data);
	void UpdateWorstTimeInstance(const UWorstCasePerformanceReportInstance* EvenWorseTime);
	void StressTestEnded();
	void StateTestEnded();
	UWorstCasePerformanceReportInstance* GetWorstCaseStructureFromIndexes(UWorstCasePerformanceReportInstance::EPerformanceReportWorstCaseType WorstCaseType, uint32 LOD, int32 StateID = INDEX_NONE, int32 ParamID = INDEX_NONE) const;

	// UI ==========================================================================================================================================================================
	TSharedRef<SWidget> GenerateOptionsMenuContent();
	TSharedRef<SWidget> BuildAnalyzer();
	void UpdateSelectedInstanceData();
	void BuildTextureWorstCasesView();
	void BuildGeometryWorstCasesView();
	void BuildTimeWorstCasesDetailedView();
	void OnOpenAssetEditor(UObject* ObjectToEdit);
	void ChangeFocusedReportInstance(UWorstCasePerformanceReportInstance* InReportInstance);
	FString OnGetInstancePath() const;
	bool IsSelectedInstanceOutdated() const;
	FReply RefreshSelectedInstance();
	TSharedRef<ITableRow> GenerateRow(UWorstCasePerformanceReportInstance* ReportedInstance, const TSharedRef<STableViewBase>& OwnerTable);
	// This array pointer is only valid until the array of arrays WorstTimeParameterInstances is modified. Use this only to access the actual instance pointers, but don't keep the array pointer for later use.
	const TArray<UWorstCasePerformanceReportInstance*>* GetWorstTimeParameters(uint32 LOD, uint32 StateIdx) const;
	void SelectInstance(UWorstCasePerformanceReportInstance* ClickedInstance);
	void SelectInstanceFinishedUpdate(class UCustomizableObjectInstance* ClickedInstance);

	TSharedPtr<class SHeaderRow> TimeWorstCasesViewHeader;
	TSharedPtr<class SVerticalBox> TextureWorstCasesView;
	TSharedPtr<class SVerticalBox> GeometryWorstCasesView;
	TSharedPtr<class SVerticalBox> TimeWorstCasesView;
	TSharedPtr<class SScrollBox> TimeWorstCasesDetailedView;
	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class STextBlock> SelectedTextureAnalyzerText;
	TSharedPtr<class SButton> PerformanceReportResetOptionsEntry;
	TSharedPtr<class SObjectPropertyEntryBox> InstanceThumbnail;
	TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool;
	TSharedPtr<class FAssetThumbnail> AssetThumbnail;
	TSharedPtr<class SButton> PerformanceReportRefreshSelectedInstance;

	/** UObject class to be able to use the UCustomizableObjectInstance::UpdatedDelegate update callback */
	UPROPERTY()
	UPerformanceReportHelper* HelperCallback;

	friend class UPerformanceReportHelper;
	friend class SPerformanceReportWorstCaseRow;
};


class SPerformanceReportWorstCaseRow : public SMutableExpandableTableRow<UWorstCasePerformanceReportInstance*>, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SPerformanceReportWorstCaseRow) {}
		SLATE_ARGUMENT(UWorstCasePerformanceReportInstance*, ReportInstance)
		SLATE_ARGUMENT(SCustomizableObjecEditorPerformanceReport*, PerformanceReport)
		SLATE_ARGUMENT(const FTableRowStyle*, Style)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	// FSerializableObject interface
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SPerformanceReportWorstCaseRow");
	}

private:
	// Report instance we represent
	UWorstCasePerformanceReportInstance* ReportInstance;

	// Performance report we are from. Necessary to generate the parameter worst time case widgets when the row is a state worst case
	SCustomizableObjecEditorPerformanceReport* PerformanceReport;

	// Local copy of additional widget data
	TArray<UWorstCasePerformanceReportInstance*> SubWidgetData;

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	virtual TSharedPtr<SWidget> GenerateAdditionalWidgetForRow() override;
	virtual EVisibility GetAdditionalWidgetDefaultVisibility() const override;
	virtual void SetAdditionalWidgetVisibility(const EVisibility Visibility) override;
	const FSlateBrush* GetSelectedRowHighlightColor() const;

	TSharedRef<ITableRow> GenerateStyledRow(UWorstCasePerformanceReportInstance* ReportedInstance, const TSharedRef<STableViewBase>& OwnerTable);
};
