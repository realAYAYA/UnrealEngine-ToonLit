// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ChaosVDSceneQueryDataComponent.h"
#include "Widgets/SCompoundWidget.h"

struct FChaosVDSceneQuerySelectionHandle;
class SChaosVDNameListPicker;
class FEditorModeTools;
class IToolkitHost;
class SChaosVDTimelineWidget;
struct FChaosVDQueryDataWrapper;
class IStructureDetailsView;
class FChaosVDScene;

/**
 * Widget for the Chaos Visual Debugger Scene Queries data inspector
 */
class SChaosVDSceneQueryDataInspector : public SCompoundWidget
{
public:
	SChaosVDSceneQueryDataInspector();

	SLATE_BEGIN_ARGS(SChaosVDSceneQueryDataInspector)
		{
		}
	SLATE_END_ARGS()

	virtual ~SChaosVDSceneQueryDataInspector() override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDScene>& InScenePtr, const TWeakPtr<FEditorModeTools>& InEditorModeTools);

	/** Sets a new query data to be inspected */
	void SetQueryDataToInspect(const FChaosVDSceneQuerySelectionHandle& InQueryDataSelectionHandle);

protected:

	TSharedRef<SWidget> GenerateQueryTagInfoRow();
	TSharedRef<SWidget> GenerateQueryNavigationBoxWidget(float TagTitleBoxHorizontalPadding, float TagTitleBoxVerticalPadding);
	TSharedRef<SWidget> GenerateQueryDetailsPanelSection(float InnerDetailsPanelsHorizontalPadding, float InnerDetailsPanelsVerticalPadding);
	TSharedRef<SWidget> GenerateVisitStepControls();

	void HandleQueryStepSelectionUpdated(int32 NewStepIndex);
	
	FText GetQueryBeingInspectedTag() const;
	FText GetSelectParticleText() const;
	FText GetSQVisitsStepsText() const;
	
	FReply SelectParticleForCurrentQueryData() const;
	FReply SelectQueryToInspectByID(int32 QueryID);
	FReply SelectParentQuery();

	static TSharedPtr<IStructureDetailsView> CreateDataDetailsView();
	
	void HandleSceneUpdated();
	void HandleSubQueryNameSelected(TSharedPtr<FName> Name);

	void ClearInspector();
	
	EVisibility GetOutOfDateWarningVisibility() const;
	EVisibility GetQueryDetailsSectionVisibility() const;
	EVisibility GetQueryStepPlaybackControlsVisibility() const;
	EVisibility GetSQVisitDetailsSectionVisibility() const;
	EVisibility GetNothingSelectedMessageVisibility() const;
	EVisibility GetSubQuerySelectorVisibility() const;
	EVisibility GetParentQuerySelectorVisibility() const;

	bool GetSelectParticleHitStateEnable() const;
	bool GetSQVisitStepsEnabled() const;

	TSharedPtr<FChaosVDQueryDataWrapper> GetCurrentDataBeingInspected();

	TSharedPtr<SChaosVDTimelineWidget> QueryStepsTimelineWidget;
	
	TWeakPtr<FChaosVDScene> SceneWeakPtr;

	TSharedPtr<IStructureDetailsView> SceneQueryDataDetailsView;
	
	TSharedPtr<IStructureDetailsView> SceneQueryHitDataDetailsView;

	TSharedPtr<SChaosVDNameListPicker> SubQueryNamePickerWidget;

	TWeakPtr<FEditorModeTools> EditorModeToolsWeakPtr;

	TMap<TSharedPtr<FName>, int32> CurrentSubQueriesByName;
	
	FChaosVDSceneQuerySelectionHandle CurrentSceneQueryBeingInspectedHandle;

	bool bIsUpToDate = true;

	bool bListenToSelectionEvents = true;

	friend struct FScopedSQInspectorSilencedSelectionEvents;
};

/** Structure that makes a SQ Inspector ignore selection events withing a scope */
struct FScopedSQInspectorSilencedSelectionEvents
{
	FScopedSQInspectorSilencedSelectionEvents(SChaosVDSceneQueryDataInspector& InInspectorIgnoringEvents) : InspectorIgnoringSelectionEvents(InInspectorIgnoringEvents)
	{
		InspectorIgnoringSelectionEvents.bListenToSelectionEvents = false;
	}
	
	~FScopedSQInspectorSilencedSelectionEvents()
	{
		InspectorIgnoringSelectionEvents.bListenToSelectionEvents = true;
	}
	
private:
	SChaosVDSceneQueryDataInspector& InspectorIgnoringSelectionEvents;
};
