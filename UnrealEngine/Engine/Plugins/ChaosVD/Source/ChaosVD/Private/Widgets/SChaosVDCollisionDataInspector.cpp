// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDCollisionDataInspector.h"

#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Visualizers/ChaosVDSolverCollisionDataComponentVisualizer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SChaosVDNameListPicker.h"
#include "Widgets/SChaosVDWarningMessageBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

SChaosVDCollisionDataInspector::~SChaosVDCollisionDataInspector()
{
	if (TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		ScenePtr->OnSceneUpdated().RemoveAll(this);
	}
}

void SChaosVDCollisionDataInspector::Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDScene>& InScenePtr)
{
	SceneWeakPtr = InScenePtr;

	if (TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		ScenePtr->OnSceneUpdated().AddRaw(this, &SChaosVDCollisionDataInspector::HandleSceneUpdated);
	}

	MainCollisionDataDetailsView = CreateCollisionDataDetailsView();
	SecondaryCollisionDataDetailsPanel = CreateCollisionDataDetailsView();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10.0f, 10.0f, 10.0f, 0.0f))
		[
			GenerateObjectNameRowWidget().ToSharedRef()
		]
		+SVerticalBox::Slot()
		.Padding(FMargin(10.0f, 5.0f, 10.0f, 1.0f))
		.AutoHeight()
		[
			SAssignNew(CollisionDataAvailableList, SChaosVDNameListPicker)
			.OnNameSleceted_Raw(this, &SChaosVDCollisionDataInspector::HandleCollisionDataEntryNameSelected)
		]
		+SVerticalBox::Slot()
		.Padding(10)
		[
			SNew(SScrollBox)
			.Visibility_Raw(this, &SChaosVDCollisionDataInspector::GetDetailsSectionVisibility)
			+SScrollBox::Slot()
			[
				SNew(SBox)
				.Padding(4.0f,2.0f,2.0f,10.0f)
				[
					SNew(SChaosVDWarningMessageBox)
					.WarningText(LOCTEXT("CollisionDataOutOfData", "Scene change detected!. Selected collision data is out of date..."))
					.Visibility_Raw(this, &SChaosVDCollisionDataInspector::GetOutOfDateWarningVisibility)
				]
			]
			+SScrollBox::Slot()
			.Padding(0.0f,5.0f,0.0f,15.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("SelectParticle0", "Select Particle 0"))
					.OnClicked(this, &SChaosVDCollisionDataInspector::SelectParticleForCurrentCollisionData, EChaosVDCollisionParticleSelector::Index_0)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("SelectParticle1", "Select Particle 1"))
					.OnClicked(this, &SChaosVDCollisionDataInspector::SelectParticleForCurrentCollisionData, EChaosVDCollisionParticleSelector::Index_1)
				]				
			]
			+SScrollBox::Slot()
			.Padding(15.0f,0.0f,15.0f,0.0f)
			[
				MainCollisionDataDetailsView->GetWidget().ToSharedRef()
			]
			+SScrollBox::Slot()
			.Padding(15.0f,10.0f,15.0f,5.0f)
			[
				SecondaryCollisionDataDetailsPanel->GetWidget().ToSharedRef()
			]
		]
	];
}

void SChaosVDCollisionDataInspector::SetCollisionDataProviderObjectToInspect(IChaosVDCollisionDataProviderInterface* CollisionDataProvider)
{
	ClearInspector();

	if (CollisionDataProvider == nullptr)
	{
		return;
	}

	CurrentObjectBeingInspectedName = CollisionDataProvider->GetProviderName();

	TArray<TSharedPtr<FName>> NewCollisionDataEntriesNameList;
	TArray<TSharedPtr<FChaosVDCollisionDataFinder>> FoundCollisionData;

	CollisionDataProvider->GetCollisionData(FoundCollisionData);

	for (const TSharedPtr<FChaosVDCollisionDataFinder>& CollisionData : FoundCollisionData)
	{
		if (TSharedPtr<FName> CollisionDataEntryName = GenerateNameForCollisionDataItem(*CollisionData))
		{
			CollisionDataByNameMap.Add(*CollisionDataEntryName, CollisionData);
			NewCollisionDataEntriesNameList.Add(CollisionDataEntryName);
		}
	}

	CollisionDataAvailableList->UpdateNameList(MoveTemp(NewCollisionDataEntriesNameList));
	bIsUpToDate = true;
}

void SChaosVDCollisionDataInspector::SetSingleContactDataToInspect(const FChaosVDCollisionDataFinder& InContactFinderData)
{
	ClearInspector();

	TSharedPtr<FChaosVDCollisionDataFinder> ContactFinderDataPtr = MakeShared<FChaosVDCollisionDataFinder>();
	*ContactFinderDataPtr = InContactFinderData;

	CurrentSelectedName = GenerateNameForCollisionDataItem(*ContactFinderDataPtr);
	
	// TODO: We need a way to provide a name when we receive collision data directly.
	// Currently this is only possible when clicking contacts in the viewport
	CurrentObjectBeingInspectedName = TEXT("Contact");

	CollisionDataByNameMap.Add(*CurrentSelectedName, ContactFinderDataPtr);

	CollisionDataAvailableList->UpdateNameList({ CurrentSelectedName });
	CollisionDataAvailableList->SelectName(CurrentSelectedName, ESelectInfo::OnMouseClick);
}

void SChaosVDCollisionDataInspector::HandleSceneUpdated()
{
	if (TSharedPtr<FChaosVDCollisionDataFinder> DataFinderPtr = GetCurrentDataBeingInspected())
	{
		if (DataFinderPtr.IsValid() && DataFinderPtr->OwningMidPhase.Pin())
		{
			bIsUpToDate = false;
		}
		else
		{
			ClearInspector();
		}
	}
}

void SChaosVDCollisionDataInspector::ClearInspector()
{
	MainCollisionDataDetailsView->SetStructureData(nullptr);
	SecondaryCollisionDataDetailsPanel->SetStructureData(nullptr);

	CollisionDataByNameMap.Reset();
	CollisionDataAvailableList->UpdateNameList({});

	CurrentSelectedName = nullptr;

	CurrentObjectBeingInspectedName = FName();

	bIsUpToDate = true;
}

FText SChaosVDCollisionDataInspector::GetObjectBeingInspectedName() const
{
	return FText::AsCultureInvariant(CurrentObjectBeingInspectedName.ToString());
}

TSharedPtr<SWidget> SChaosVDCollisionDataInspector::GenerateObjectNameRowWidget()
{
	TSharedPtr<SWidget> RowWidget = SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
		.BorderBackgroundColor(FAppStyle::Get().GetSlateColor("Colors.Panel"))
		.Padding(0)
		[
			SNew(SBox)
			.Padding(2.0f)
			[
				SNew(SSplitter)
				.Style(FAppStyle::Get(), "DetailsView.Splitter")
				.PhysicalSplitterHandleSize(1.5f)
				.HitDetectionSplitterHandleSize(5.0f)
				+SSplitter::Slot()
				.Value(0.2)
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					.Padding(1.0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CollisionDataInspectorObject","Object"))
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					]	
				]
				+SSplitter::Slot()
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					.Padding(1.0)
					[
						SNew(STextBlock)
						.Text_Raw(this, &SChaosVDCollisionDataInspector::GetObjectBeingInspectedName)
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					]
				]
			]
		];

	return RowWidget;
}


EVisibility SChaosVDCollisionDataInspector::GetOutOfDateWarningVisibility() const
{
	return bIsUpToDate ? EVisibility::Collapsed : EVisibility::Visible;
}

void SChaosVDCollisionDataInspector::HandleCollisionDataEntryNameSelected(TSharedPtr<FName> SelectedName)
{
	CurrentSelectedName = SelectedName;

	if (!CurrentSelectedName.IsValid())
	{
		return;
	}

	if (TSharedPtr<FChaosVDCollisionDataFinder> DataFinderPtr = GetCurrentDataBeingInspected())
	{
		if (DataFinderPtr->OwningConstraint)
		{
			FChaosVDConstraint* MutableConstraint = const_cast<FChaosVDConstraint*>(DataFinderPtr->OwningConstraint);
			TSharedPtr<FStructOnScope> ConstraintView = MakeShared<FStructOnScope>(FChaosVDConstraint::StaticStruct(), reinterpret_cast<uint8*>(MutableConstraint));
			SecondaryCollisionDataDetailsPanel->SetStructureData(ConstraintView); 

			if (DataFinderPtr->OwningConstraint->ManifoldPoints.IsValidIndex(DataFinderPtr->ContactIndex))
			{
				const FChaosVDManifoldPoint* ContactPointData = &DataFinderPtr->OwningConstraint->ManifoldPoints[DataFinderPtr->ContactIndex];
				TSharedPtr<FStructOnScope> ContactViewView = MakeShared<FStructOnScope>(FChaosVDManifoldPoint::StaticStruct(), reinterpret_cast<uint8*>(const_cast<FChaosVDManifoldPoint*>(ContactPointData)));
				MainCollisionDataDetailsView->SetStructureData(ContactViewView);
			}
			else
			{
				MainCollisionDataDetailsView->SetStructureData(nullptr); 
			}
		}
		else if (TSharedPtr<FChaosVDParticlePairMidPhase> MidPhaseData = DataFinderPtr->OwningMidPhase.Pin())
		{
			// If we have a recorded midphase with not contact data, show that instead
			const TSharedPtr<FStructOnScope> MidPhaseView = MakeShared<FStructOnScope>(FChaosVDParticlePairMidPhase::StaticStruct(), reinterpret_cast<uint8*>(MidPhaseData.Get()));
			MainCollisionDataDetailsView->SetStructureData(MidPhaseView); 
		}
	}
}

TSharedPtr<FName> SChaosVDCollisionDataInspector::GenerateNameForCollisionDataItem(const FChaosVDCollisionDataFinder& InContactFinderData)
{
	int32 Particle0Index = INDEX_NONE;
	int32 Particle1Index = INDEX_NONE;

	bool bHasConstraintData = false;

	if (const TSharedPtr<FChaosVDParticlePairMidPhase> MidPhaseData = InContactFinderData.OwningMidPhase.Pin())
	{
		if (InContactFinderData.OwningConstraint)
		{
			Particle0Index = InContactFinderData.OwningConstraint->Particle0Index;
			Particle1Index = InContactFinderData.OwningConstraint->Particle1Index;
			bHasConstraintData = true;
		}
		else
		{
			Particle0Index = MidPhaseData->Particle0Idx;
			Particle1Index = MidPhaseData->Particle1Idx;	
		}
	}

	if (Particle0Index != INDEX_NONE || Particle1Index != INDEX_NONE)
	{
		static FText MidPhaseOnlyText = LOCTEXT("CollisionItemDataTitleMidPhaseOnly", "[MidPhase Only]");
		static FText ConstraintText = LOCTEXT("CollisionItemDataTitleConstaint", "[Constraint]");
		const FText GeneratedName = FText::Format(LOCTEXT("CollisionItemDataTitle", "Particle Pair | Index0 [ID {0}] <-> Index1 [{1}] | Type {2}"), Particle0Index, Particle1Index, bHasConstraintData ? ConstraintText: MidPhaseOnlyText);
		return MakeShared<FName>(GeneratedName.ToString());
	}

	return nullptr;
}

TSharedPtr<IStructureDetailsView> SChaosVDCollisionDataInspector::CreateCollisionDataDetailsView()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FStructureDetailsViewArgs StructDetailsViewArgs;
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowFavoriteSystem = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bShowScrollBar = false;

	return PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs,StructDetailsViewArgs, nullptr);
}

EVisibility SChaosVDCollisionDataInspector::GetDetailsSectionVisibility() const
{
	return CurrentSelectedName ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SChaosVDCollisionDataInspector::SelectParticleForCurrentCollisionData(EChaosVDCollisionParticleSelector ParticleSelector)
{
	TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin();
	if (!ScenePtr.IsValid())
	{
		return FReply::Handled();
	}
	
	if (const TSharedPtr<FChaosVDCollisionDataFinder> DataFinderPtr = GetCurrentDataBeingInspected())
	{
		if (const FChaosVDMidPhasePtr MidPhasePtr = DataFinderPtr->OwningMidPhase.Pin())
		{
			int32 ParticleIndex = INDEX_NONE;
			if (DataFinderPtr->OwningConstraint)
			{
				ParticleIndex = ParticleSelector == EChaosVDCollisionParticleSelector::Index_0 ? DataFinderPtr->OwningConstraint->Particle0Index : DataFinderPtr->OwningConstraint->Particle1Index;
			}
			else
			{
				ParticleIndex = ParticleSelector == EChaosVDCollisionParticleSelector::Index_0 ? MidPhasePtr->Particle0Idx : MidPhasePtr->Particle1Idx;
			}

			if (AChaosVDParticleActor* ParticleActor = ScenePtr->GetParticleActor(MidPhasePtr->SolverID, ParticleIndex))
			{
				ScenePtr->SetSelectedObject(ParticleActor);
			}
		}
	}

	return FReply::Handled();
}

TSharedPtr<FChaosVDCollisionDataFinder> SChaosVDCollisionDataInspector::GetCurrentDataBeingInspected()
{
	if (!CurrentSelectedName.IsValid())
	{
		return nullptr;
	}

	if (TSharedPtr<FChaosVDCollisionDataFinder>* DataFinderPtrPtr = CollisionDataByNameMap.Find(*CurrentSelectedName))
	{
		if (TSharedPtr<FChaosVDCollisionDataFinder> DataFinderPtr = *DataFinderPtrPtr)
		{
			return DataFinderPtr;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
