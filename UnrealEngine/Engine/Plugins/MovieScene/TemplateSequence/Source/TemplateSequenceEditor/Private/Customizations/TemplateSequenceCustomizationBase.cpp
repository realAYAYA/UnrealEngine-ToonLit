// Copyright Epic Games, Inc. All Rights Reserved.

#include "TemplateSequenceCustomizationBase.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "Misc/TemplateSequenceEditorUtil.h"
#include "ScopedTransaction.h"
#include "TemplateSequence.h"

#define LOCTEXT_NAMESPACE "TemplateSequenceCustomizationBase"

void FTemplateSequenceCustomizationBase::RegisterSequencerCustomization(FSequencerCustomizationBuilder& Builder)
{
	Sequencer = &Builder.GetSequencer();
	TemplateSequence = Cast<UTemplateSequence>(&Builder.GetFocusedSequence());

	Sequencer->OnMovieSceneDataChanged().AddRaw(this, &FTemplateSequenceCustomizationBase::OnMovieSceneDataChanged);

	FSequencerCustomizationInfo BaseInfo;
	BaseInfo.OnPaste.BindRaw(this, &FTemplateSequenceCustomizationBase::OnPaste);
	Builder.AddCustomization(BaseInfo);
}

void FTemplateSequenceCustomizationBase::UnregisterSequencerCustomization()
{
	Sequencer->OnMovieSceneDataChanged().RemoveAll(this);

	Sequencer = nullptr;
	TemplateSequence = nullptr;
}

UClass* FTemplateSequenceCustomizationBase::GetBoundActorClass() const
{
	return TemplateSequence ? TemplateSequence->BoundActorClass.Get() : nullptr;
}

FText FTemplateSequenceCustomizationBase::GetBoundActorClassName() const
{
	const UClass* BoundActorClass = GetBoundActorClass();
	return BoundActorClass ? BoundActorClass->GetDisplayNameText() : FText::FromName(NAME_None);
}

void FTemplateSequenceCustomizationBase::OnBoundActorClassPicked(UClass* ChosenClass)
{
	FSlateApplication::Get().DismissAllMenus();

	if (TemplateSequence != nullptr)
	{
		ChangeActorBinding(ChosenClass);

		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

void FTemplateSequenceCustomizationBase::ChangeActorBinding(UObject* Object, UActorFactory* ActorFactory, bool bSetupDefaults)
{
	const FScopedTransaction Transaction(LOCTEXT("ChangeActorBinding", "Change Template Sequence Actor Binding"));

	FTemplateSequenceEditorUtil Util(TemplateSequence, *Sequencer);
	Util.ChangeActorBinding(Object, ActorFactory, bSetupDefaults);
}

ESequencerPasteSupport FTemplateSequenceCustomizationBase::OnPaste()
{
	// We don't support pasting folders or new object bindings.
	return ESequencerPasteSupport::Tracks | ESequencerPasteSupport::Sections;
}

void FTemplateSequenceCustomizationBase::OnMovieSceneDataChanged(EMovieSceneDataChangeType ChangeType)
{
	if (ChangeType == EMovieSceneDataChangeType::TrackValueChanged || ChangeType == EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately)
	{
		return;
	}

	// Ensure the BoundActorClass is up to date with whatever change might have occured.
	// This is mostly only needed when the user deletes or cuts the root object binding and we are suddenly left
	// with an empty template sequence (in which case we need to set BoundActorClass to null).
	const UObject* RootSpawnableTemplate = TemplateSequence->GetRootObjectSpawnableTemplate();	
	const UClass* RootObjectClass = Cast<const UClass>(RootSpawnableTemplate);
	if (RootSpawnableTemplate && !RootObjectClass)
	{
		RootObjectClass = RootSpawnableTemplate->GetClass();
	}

	if (TemplateSequence->BoundActorClass != RootObjectClass)
	{
		FProperty* BoundActorClassProperty = FindFProperty<FProperty>(UTemplateSequence::StaticClass(), GET_MEMBER_NAME_CHECKED(UTemplateSequence, BoundActorClass));
		TemplateSequence->PreEditChange(BoundActorClassProperty);

		TemplateSequence->BoundActorClass = RootObjectClass;

		FPropertyChangedEvent PropertyEvent(BoundActorClassProperty, EPropertyChangeType::ValueSet);
		TemplateSequence->PostEditChangeProperty(PropertyEvent);
	}
}

#undef LOCTEXT_NAMESPACE

