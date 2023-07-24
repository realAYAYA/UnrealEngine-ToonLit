// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/PossessableModel.h"

#include "Containers/ArrayView.h"
#include "HAL/Platform.h"
#include "ISequencer.h"
#include "Internationalization/Internationalization.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "MovieScene.h"
#include "MovieSceneFwd.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSequence.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

class UClass;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "FPossessableModel"

namespace UE
{
namespace Sequencer
{

FPossessableModel::FPossessableModel(FSequenceModel* InOwnerModel, const FMovieSceneBinding& InBinding, const FMovieScenePossessable& InPossessable)
	: FObjectBindingModel(InOwnerModel, InBinding)
{
	ParentObjectBindingID = InPossessable.GetParent();
}

FPossessableModel::~FPossessableModel()
{
}

EObjectBindingType FPossessableModel::GetType() const
{
	return EObjectBindingType::Possessable;
}

bool FPossessableModel::SupportsRebinding() const
{
	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	UMovieSceneSequence* Sequence = OwnerModel->GetSequence();
	check(MovieScene && Sequence);

	const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingID);
	return Possessable && Sequence->CanRebindPossessable(*Possessable);
}

void FPossessableModel::OnConstruct()
{
	using namespace UE::MovieScene;

	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	check(MovieScene);

	const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingID);
	check(Possessable);

	FObjectBindingModel::OnConstruct();
}

FText FPossessableModel::GetIconToolTipText() const
{
	return LOCTEXT("PossessableToolTip", "This item is a possessable reference to an existing object.");
}

const FSlateBrush* FPossessableModel::GetIconOverlayBrush() const
{
	TSharedPtr<ISequencer> Sequencer = GetEditor()->GetSequencer();
	if (Sequencer)
	{
		const int32 NumBoundObjects = Sequencer->FindObjectsInCurrentSequence(ObjectBindingID).Num();
		if (NumBoundObjects > 1)
		{
			return FAppStyle::GetBrush("Sequencer.MultipleIconOverlay");
		}
	}
	return nullptr;
}

const UClass* FPossessableModel::FindObjectClass() const
{
	UMovieScene*            MovieScene  = OwnerModel ? OwnerModel->GetMovieScene() : nullptr;
	FMovieScenePossessable* Possessable = MovieScene ? MovieScene->FindPossessable(ObjectBindingID) : nullptr;

	const UClass* Class = UObject::StaticClass();
	if (Possessable && Possessable->GetPossessedObjectClass() != nullptr)
	{
		Class = Possessable->GetPossessedObjectClass();
	}

	return Class;
}

void FPossessableModel::Delete()
{
	FObjectBindingModel::Delete();

	UMovieSceneSequence* Sequence   = OwnerModel ? OwnerModel->GetSequence() : nullptr;
	UMovieScene*         MovieScene = Sequence   ? Sequence->GetMovieScene() : nullptr;

	if (MovieScene)
	{
		MovieScene->Modify();

		if (MovieScene->RemovePossessable(ObjectBindingID))
		{
			Sequence->Modify();
			Sequence->UnbindPossessableObjects(ObjectBindingID);
		}
	}
}

} // namespace Sequencer
} // namespace UE

#undef LOCTEXT_NAMESPACE
