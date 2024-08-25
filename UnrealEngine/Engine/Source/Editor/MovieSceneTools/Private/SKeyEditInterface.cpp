// Copyright Epic Games, Inc. All Rights Reserved.

#include "SKeyEditInterface.h"
#include "MovieSceneObjectBindingIDCustomization.h"
#include "UObject/StructOnScope.h"
#include "MovieSceneSection.h"
#include "ISequencer.h"
#include "Editor.h"
#include "MovieSceneKeyStruct.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "FrameNumberDetailsCustomization.h"
#include "MovieSceneSectionDetailsCustomization.h"
#include "MovieSceneEventCustomization.h"
#include "Misc/FrameNumber.h"
#include "Modules/ModuleManager.h"
#include "IDetailCustomization.h"
#include "MovieSceneToolsModule.h"

SKeyEditInterface::~SKeyEditInterface()
{
	GEditor->UnregisterForUndo(this);
}

void SKeyEditInterface::Construct(const FArguments& InArgs, TSharedRef<ISequencer> InSequencer)
{
	GEditor->RegisterForUndo(this);

	EditDataAttribute = InArgs._EditData;

	WeakSequencer = InSequencer;
	Initialize();
}

void SKeyEditInterface::Initialize()
{
	// Reset the section and widget content
	FKeyEditData NewEditData = EditDataAttribute.Get();
	WeakSection = NewEditData.OwningSection;

	ChildSlot
	[
		SNullWidget::NullWidget
	];

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid() || !NewEditData.KeyStruct.IsValid())
	{
		return;
	}

	// Set up the details panel
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bShowScrollBar = false;
		DetailsViewArgs.NotifyHook = this;
	}

	FStructureDetailsViewArgs StructureViewArgs;
	{
		StructureViewArgs.bShowObjects = false;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = false;
	}

	TSharedRef<IStructureDetailsView> StructureDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")
		.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);

	// register details customizations for this instance
	StructureDetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout("MovieSceneObjectBindingID", FOnGetPropertyTypeCustomizationInstance::CreateSP(this, &SKeyEditInterface::CreateBindingIDCustomization));
	StructureDetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout("MovieSceneEvent", FOnGetPropertyTypeCustomizationInstance::CreateSP(this, &SKeyEditInterface::CreateEventCustomization));
	StructureDetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout("FrameNumber", FOnGetPropertyTypeCustomizationInstance::CreateSP(this, &SKeyEditInterface::CreateFrameNumberCustomization));

	FMovieSceneToolsModule::Get().CustomizeKeyStructInstancedPropertyTypes(StructureDetailsView, WeakSection);

	StructureDetailsView->SetStructureData(NewEditData.KeyStruct);
	StructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SKeyEditInterface::OnFinishedChangingProperties, NewEditData.KeyStruct);

	ChildSlot
	[
		StructureDetailsView->GetWidget().ToSharedRef()
	];
}

FVector2D SKeyEditInterface::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	FVector2D ChildSize = SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
	return FVector2D::Max(ChildSize, FVector2D(350.f, 0.f));
}

TSharedRef<IPropertyTypeCustomization> SKeyEditInterface::CreateBindingIDCustomization()
{
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	FMovieSceneSequenceID SequenceID = SequencerPtr.IsValid() ? SequencerPtr->GetFocusedTemplateID() : MovieSceneSequenceID::Root;
	return MakeShared<FMovieSceneObjectBindingIDCustomization>(SequenceID, WeakSequencer);
}

TSharedRef<IPropertyTypeCustomization> SKeyEditInterface::CreateFrameNumberCustomization()
{
	TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
	return MakeShared<FFrameNumberDetailsCustomization>(SequencerPtr->GetNumericTypeInterface());
}

TSharedRef<IPropertyTypeCustomization> SKeyEditInterface::CreateEventCustomization()
{
	return FMovieSceneEventCustomization::MakeInstance(WeakSection.Get());
}

void SKeyEditInterface::OnFinishedChangingProperties(const FPropertyChangedEvent& ChangeEvent, TSharedPtr<FStructOnScope> KeyStruct)
{
	if (KeyStruct->GetStruct()->IsChildOf(FMovieSceneKeyStruct::StaticStruct()))
	{
		if (UMovieSceneSection* Section = WeakSection.Get())
		{
			Section->Modify();
		}

		((FMovieSceneKeyStruct*)KeyStruct->GetStructMemory())->PropagateChanges(ChangeEvent);
	}
	else if (KeyStruct->GetStruct()->IsChildOf(FGeneratedMovieSceneKeyStruct::StaticStruct()))
	{
		if (UMovieSceneSection* Section = WeakSection.Get())
		{
			Section->Modify();
		}

		FGeneratedMovieSceneKeyStruct* GeneratedStruct = reinterpret_cast<FGeneratedMovieSceneKeyStruct*>(KeyStruct->GetStructMemory());
		if (GeneratedStruct->OnPropertyChangedEvent)
		{
			GeneratedStruct->OnPropertyChangedEvent(ChangeEvent);
		}
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

void SKeyEditInterface::NotifyPreChange( FProperty* PropertyAboutToChange )
{
	if (UMovieSceneSection* Section = WeakSection.Get())
	{
		Section->Modify();
	}
}

void SKeyEditInterface::PostUndo(bool bSuccess)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	if (Sequencer.IsValid())
	{
		Initialize();
	}
}

void SKeyEditInterface::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}