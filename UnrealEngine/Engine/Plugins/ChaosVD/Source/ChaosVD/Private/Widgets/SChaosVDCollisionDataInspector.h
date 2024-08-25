// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class AChaosVDParticleActor;
class FChaosVDScene;
class IStructureDetailsView;
class IChaosVDCollisionDataProviderInterface;
class SChaosVDNameListPicker;

struct FChaosVDCollisionDataFinder;
struct FChaosVDParticlePairMidPhase;

typedef TSharedPtr<FChaosVDParticlePairMidPhase> FChaosVDMidPhasePtr;

enum class EChaosVDCollisionParticleSelector
{
	Index_0,
	Index_1
};

class SChaosVDCollisionDataInspector : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SChaosVDCollisionDataInspector)
	{
	}

	SLATE_END_ARGS()

	virtual ~SChaosVDCollisionDataInspector() override;

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDScene>& InScenePtr);

	void SetCollisionDataProviderObjectToInspect(IChaosVDCollisionDataProviderInterface* CollisionDataProvider);
	void SetSingleContactDataToInspect(const FChaosVDCollisionDataFinder& InContactFinderData);

protected:

	void HandleSceneUpdated();

	void ClearInspector();

	FText GetObjectBeingInspectedName() const;
	TSharedPtr<SWidget> GenerateObjectNameRowWidget();

	TSharedPtr<FChaosVDCollisionDataFinder> GetCurrentDataBeingInspected();

	EVisibility GetOutOfDateWarningVisibility() const;

	void HandleCollisionDataEntryNameSelected(TSharedPtr<FName> SelectedName);

	TSharedPtr<FName> GenerateNameForCollisionDataItem(const FChaosVDCollisionDataFinder& InContactFinderData);

	TSharedPtr<IStructureDetailsView> CreateCollisionDataDetailsView();

	EVisibility GetDetailsSectionVisibility() const;

	FReply SelectParticleForCurrentCollisionData(EChaosVDCollisionParticleSelector ParticleSelector);

	TWeakPtr<FChaosVDScene> SceneWeakPtr;

	TSharedPtr<SChaosVDNameListPicker> CollisionDataAvailableList;
	
	TMap<FName, TSharedPtr<FChaosVDCollisionDataFinder>> CollisionDataByNameMap;

	TSharedPtr<FName> CurrentSelectedName;

	TSharedPtr<IStructureDetailsView> MainCollisionDataDetailsView;
	
	TSharedPtr<IStructureDetailsView> SecondaryCollisionDataDetailsPanel;

	FName CurrentObjectBeingInspectedName;

	bool bIsUpToDate = true;
};
