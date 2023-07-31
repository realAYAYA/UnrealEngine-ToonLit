// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PrimitiveMaterialTrackEditor.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"
#include "ISequencerModule.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Algo/Find.h"


#define LOCTEXT_NAMESPACE "PrimitiveMaterialTrackEditor"


FPrimitiveMaterialTrackEditor::FPrimitiveMaterialTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FKeyframeTrackEditor(InSequencer)
{}

TSharedRef<ISequencerTrackEditor> FPrimitiveMaterialTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShared<FPrimitiveMaterialTrackEditor>(OwningSequencer);
}

void FPrimitiveMaterialTrackEditor::ExtendObjectBindingTrackMenu(TSharedRef<FExtender> Extender, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(UPrimitiveComponent::StaticClass()))
	{
		Extender->AddMenuExtension(SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, EExtensionHook::Before, nullptr, FMenuExtensionDelegate::CreateSP(this, &FPrimitiveMaterialTrackEditor::ConstructObjectBindingTrackMenu, ObjectBindings));
	}
}

void FPrimitiveMaterialTrackEditor::ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings)
{
	auto GetMaterialIndexForTrack = [](UMovieSceneTrack* InTrack)
	{
		UMovieScenePrimitiveMaterialTrack* MaterialTrack = Cast<UMovieScenePrimitiveMaterialTrack>(InTrack);
		return MaterialTrack ? MaterialTrack->GetMaterialIndex() : INDEX_NONE;
	};

	int32 MinNumMaterials = TNumericLimits<int32>::Max();

	for (TWeakObjectPtr<> WeakObject : GetSequencer()->FindObjectsInCurrentSequence(ObjectBindings[0]))
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(WeakObject.Get());
		if (PrimitiveComponent)
		{
			MinNumMaterials = FMath::Min(MinNumMaterials, PrimitiveComponent->GetNumMaterials());
		}
	}

	if (MinNumMaterials > 0 && MinNumMaterials < TNumericLimits<int32>::Max())
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("MaterialSwitcherTitle", "Material Switchers"));

		const UMovieScene*        MovieScene = GetFocusedMovieScene();
		const FMovieSceneBinding* Binding    = Algo::FindBy(MovieScene->GetBindings(), ObjectBindings[0], &FMovieSceneBinding::GetObjectGuid);

		check(Binding);

		for (int32 Index = 0; Index < MinNumMaterials; ++Index)
		{
			const bool bAlreadyExists = Algo::FindBy(Binding->GetTracks(), Index, GetMaterialIndexForTrack) != nullptr;
			if (!bAlreadyExists)
			{
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("MaterialID_Format", "Material Element {0} Switcher"), FText::AsNumber(Index)),
					FText(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FPrimitiveMaterialTrackEditor::CreateTrackForElement, ObjectBindings, Index)
					)
				);
			}
		}

		MenuBuilder.EndSection();
	}
}

void FPrimitiveMaterialTrackEditor::CreateTrackForElement(TArray<FGuid> ObjectBindingIDs, int32 MaterialIndex)
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	FScopedTransaction Transaction(LOCTEXT("CreateTrack", "Create Material Track"));
	MovieScene->Modify();

	for (FGuid ObjectBindingID : ObjectBindingIDs)
	{
		UMovieScenePrimitiveMaterialTrack* NewTrack = MovieScene->AddTrack<UMovieScenePrimitiveMaterialTrack>(ObjectBindingID);
		NewTrack->SetMaterialIndex(MaterialIndex);
		NewTrack->SetDisplayName(FText::Format(LOCTEXT("MaterialTrackName_Format", "Material Element {0}"), FText::AsNumber(MaterialIndex)));

		NewTrack->AddSection(*NewTrack->CreateNewSection());
	}

	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

#undef LOCTEXT_NAMESPACE
