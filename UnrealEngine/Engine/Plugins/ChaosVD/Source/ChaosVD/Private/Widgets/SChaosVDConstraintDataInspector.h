// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"
#include "Widgets/SCompoundWidget.h"

struct FChaosVDJointConstraintSelectionHandle;
class FChaosVDScene;
class IStructureDetailsView;
class SChaosVDNameListPicker;

enum class EChaosVDParticleSelector : int32
{
	Index_0,
	Index_1
};

class SChaosVDConstraintDataInspector : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SChaosVDConstraintDataInspector)
	{
	}

	SLATE_END_ARGS()

	virtual ~SChaosVDConstraintDataInspector() override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDScene>& InScenePtr);
	/** Sets a new query data to be inspected */
	void SetConstraintDataToInspect(const FChaosVDJointConstraintSelectionHandle& InDataSelectionHandle);

protected:

	FText GetParticleName(EChaosVDParticleSelector PairIndex) const;

	TSharedPtr<IStructureDetailsView> CreateDataDetailsView();

	TSharedRef<SWidget> GenerateParticleSelectorButtons();
	
	FReply SelectParticleForCurrentSelectedData(EChaosVDParticleSelector ParticlePairIndex);

	EVisibility GetOutOfDateWarningVisibility() const;
	EVisibility GetDetailsSectionVisibility() const;
	EVisibility GetNothingSelectedMessageVisibility() const;

	void RegisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo);
	void UnregisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo);

	void RegisterSceneEvents();
	void UnregisterSceneEvents();

	void HandleSceneUpdated();

	void ClearInspector();

	TSharedPtr<IStructureDetailsView> ConstraintDataDetailsView;
	
	TWeakPtr<FChaosVDScene> SceneWeakPtr;

	FChaosVDJointConstraintSelectionHandle CurrentDataSelectionHandle;

	bool bIsUpToDate = true;
};
