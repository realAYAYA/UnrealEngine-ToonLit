// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDConstraintDataInspector.h"

#include "ChaosVDScene.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Widgets/SChaosVDWarningMessageBox.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"
#include "DataWrappers/ChaosVDJointDataWrappers.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

SChaosVDConstraintDataInspector::~SChaosVDConstraintDataInspector()
{
	UnregisterSceneEvents();
}

void SChaosVDConstraintDataInspector::Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDScene>& InScenePtr)
{
	SceneWeakPtr = InScenePtr;

	RegisterSceneEvents();

	ConstraintDataDetailsView = CreateDataDetailsView();

	constexpr float NoPadding = 0.0f;
	constexpr float OuterBoxPadding = 2.0f;
	constexpr float OuterInnerPadding = 5.0f;
	constexpr float TagTitleBoxHorizontalPadding = 10.0f;
	constexpr float TagTitleBoxVerticalPadding = 5.0f;
	constexpr float InnerDetailsPanelsHorizontalPadding = 15.0f;
	constexpr float InnerDetailsPanelsVerticalPadding = 15.0f;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(OuterInnerPadding)
		[
			SNew(SBox)
			.Visibility_Raw(this, &SChaosVDConstraintDataInspector::GetOutOfDateWarningVisibility)
			.Padding(OuterBoxPadding, OuterBoxPadding,OuterBoxPadding,NoPadding)
			[
				SNew(SChaosVDWarningMessageBox)
				.WarningText(LOCTEXT("ConstraintDataOutOfDate", "Scene change detected!. Selected constraint data is out of date..."))
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(TagTitleBoxHorizontalPadding, TagTitleBoxVerticalPadding, TagTitleBoxHorizontalPadding, NoPadding)
		[
			GenerateParticleSelectorButtons()
		]
		+SVerticalBox::Slot()
		.Padding(TagTitleBoxHorizontalPadding, TagTitleBoxVerticalPadding, TagTitleBoxHorizontalPadding, NoPadding)
		.AutoHeight()
		[
			SNew(STextBlock)
			.Visibility_Raw(this, &SChaosVDConstraintDataInspector::GetNothingSelectedMessageVisibility)
			.Justification(ETextJustify::Center)
			.TextStyle(FAppStyle::Get(), "DetailsView.BPMessageTextStyle")
			.Text(LOCTEXT("ConstraintDataNoSelectedMessage", "Select a Joint Constraint in the viewport to see its details..."))
			.AutoWrapText(true)
		]
		+SVerticalBox::Slot()
		.Padding(OuterInnerPadding)
		[
			SNew(SScrollBox)
			.Visibility_Raw(this, &SChaosVDConstraintDataInspector::GetDetailsSectionVisibility)
			+SScrollBox::Slot()
			.Padding(InnerDetailsPanelsHorizontalPadding,NoPadding,InnerDetailsPanelsHorizontalPadding,InnerDetailsPanelsVerticalPadding)
			[
				ConstraintDataDetailsView->GetWidget().ToSharedRef()
			]
		]
	];
}

void SChaosVDConstraintDataInspector::RegisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo)
{
	if (UChaosVDSolverJointConstraintDataComponent* JointDataComponent = SolverInfo ? SolverInfo->GetJointsDataComponent() : nullptr)
	{
		JointDataComponent->OnSelectionChanged().AddRaw(this, &SChaosVDConstraintDataInspector::SetConstraintDataToInspect);
	}
}

void SChaosVDConstraintDataInspector::UnregisterSelectionEventsForSolver(AChaosVDSolverInfoActor* SolverInfo)
{
	if (UChaosVDSolverJointConstraintDataComponent* JointDataComponent = SolverInfo ? SolverInfo->GetJointsDataComponent() : nullptr)
	{
		JointDataComponent->OnSelectionChanged().RemoveAll(this);
	}
}

void SChaosVDConstraintDataInspector::RegisterSceneEvents()
{
	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		ScenePtr->OnSceneUpdated().AddRaw(this, &SChaosVDConstraintDataInspector::HandleSceneUpdated);
		ScenePtr->OnSolverInfoActorCreated().AddRaw(this, &SChaosVDConstraintDataInspector::RegisterSelectionEventsForSolver);

		for (const TPair<int32, AChaosVDSolverInfoActor*> SolverInfos : ScenePtr->GetSolverInfoActorsMap())
		{
			RegisterSelectionEventsForSolver(SolverInfos.Value);
		}
	}
}

void SChaosVDConstraintDataInspector::UnregisterSceneEvents()
{
	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		ScenePtr->OnSceneUpdated().RemoveAll(this);
		ScenePtr->OnSolverInfoActorCreated().RemoveAll(this);

		for (const TPair<int32, AChaosVDSolverInfoActor*> SolverInfos : ScenePtr->GetSolverInfoActorsMap())
		{
			UnregisterSelectionEventsForSolver(SolverInfos.Value);
		}
	}
}

void SChaosVDConstraintDataInspector::SetConstraintDataToInspect(const FChaosVDJointConstraintSelectionHandle& InDataSelectionHandle)
{
	ClearInspector();

	if (const TSharedPtr<FChaosVDJointConstraint> QueryDataToInspect = InDataSelectionHandle.GetData().Pin())
	{
		CurrentDataSelectionHandle = InDataSelectionHandle;
		const TSharedPtr<FStructOnScope> QueryDataView = MakeShared<FStructOnScope>(FChaosVDJointConstraint::StaticStruct(), reinterpret_cast<uint8*>(QueryDataToInspect.Get()));
		ConstraintDataDetailsView->SetStructureData(QueryDataView);
	}

	bIsUpToDate = true;
}

FText SChaosVDConstraintDataInspector::GetParticleName(EChaosVDParticleSelector PairIndex) const
{
	const TSharedPtr<FChaosVDJointConstraint> JointConstraintPtr = CurrentDataSelectionHandle.GetData().Pin();
	if (!JointConstraintPtr)
	{
		return FText::AsCultureInvariant("UnnamedParticle");
	}

	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		if (AChaosVDParticleActor* ParticleActor = ScenePtr->GetParticleActor(JointConstraintPtr->SolverID, PairIndex == EChaosVDParticleSelector::Index_0 ? JointConstraintPtr->ParticleParIndexes[0] : JointConstraintPtr->ParticleParIndexes[1]))
		{
			if (const FChaosVDParticleDataWrapper* ParticleData = ParticleActor->GetParticleData())
			{
				return FText::AsCultureInvariant(ParticleData->DebugName);
			}
		}
	}

	return FText::AsCultureInvariant("UnnamedParticle");
}

TSharedRef<SWidget> SChaosVDConstraintDataInspector::GenerateParticleSelectorButtons()
{
	return SNew(SUniformGridPanel)
			.Visibility_Raw(this, &SChaosVDConstraintDataInspector::GetDetailsSectionVisibility)
			.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("SelectParticle0", "Select Particle 0"))
				.ToolTipText_Raw(this, &SChaosVDConstraintDataInspector::GetParticleName, EChaosVDParticleSelector::Index_0)
				.OnClicked(this, &SChaosVDConstraintDataInspector::SelectParticleForCurrentSelectedData, EChaosVDParticleSelector::Index_0)
			]
			+SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("SelectParticle1", "Select Particle 1"))
				.ToolTipText_Raw(this, &SChaosVDConstraintDataInspector::GetParticleName, EChaosVDParticleSelector::Index_1)
				.OnClicked(this, &SChaosVDConstraintDataInspector::SelectParticleForCurrentSelectedData, EChaosVDParticleSelector::Index_1)
			];
}

FReply SChaosVDConstraintDataInspector::SelectParticleForCurrentSelectedData(EChaosVDParticleSelector ParticlePairIndex)
{
	const TSharedPtr<FChaosVDJointConstraint> JointConstraintPtr = CurrentDataSelectionHandle.GetData().Pin();
	if (!JointConstraintPtr)
	{
		return FReply::Handled();
	}

	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		if (AChaosVDParticleActor* ParticleActor = ScenePtr->GetParticleActor(JointConstraintPtr->SolverID, ParticlePairIndex == EChaosVDParticleSelector::Index_0 ? JointConstraintPtr->ParticleParIndexes[0] : JointConstraintPtr->ParticleParIndexes[1]))
		{
			ScenePtr->SetSelectedObject(ParticleActor);
		}
	}
	
	return FReply::Handled();
}

EVisibility SChaosVDConstraintDataInspector::GetOutOfDateWarningVisibility() const
{
	return !bIsUpToDate && CurrentDataSelectionHandle.GetData().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SChaosVDConstraintDataInspector::GetDetailsSectionVisibility() const
{
	return CurrentDataSelectionHandle.GetData().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SChaosVDConstraintDataInspector::GetNothingSelectedMessageVisibility() const
{
	return !CurrentDataSelectionHandle.GetData().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SChaosVDConstraintDataInspector::HandleSceneUpdated()
{
	// TODO: Disabling the "out of date" message system, it is a little bit confusing with joint constraints as the debug draw position will be updated between frames (because the constraint only has particles IDs)
	// but the data itself is not updated.
	// We are clearing out the selection altogether instead for now
	//bIsUpToDate = false;
	
	CurrentDataSelectionHandle.SetIsSelected(false);
	CurrentDataSelectionHandle = FChaosVDJointConstraintSelectionHandle();

	// TODO: To Keep a selection up to date we need a persistent ID for the constraint
	// We could hash the pointer for that or add an ID to the Constraint Handle only compiled in when CVD is enabled
}

void SChaosVDConstraintDataInspector::ClearInspector()
{
	ConstraintDataDetailsView->SetStructureData(nullptr);
	CurrentDataSelectionHandle = FChaosVDJointConstraintSelectionHandle();
}

TSharedPtr<IStructureDetailsView> SChaosVDConstraintDataInspector::CreateDataDetailsView()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const FStructureDetailsViewArgs StructDetailsViewArgs;
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowFavoriteSystem = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowScrollBar = false;

	return PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructDetailsViewArgs, nullptr);
}

#undef LOCTEXT_NAMESPACE
