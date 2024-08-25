// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/SectionModelStorageExtension.h"

#include "MVVM/ViewModels/SectionModel.h"
#include "MovieSceneSection.h"

namespace UE
{
namespace Sequencer
{

FSectionModelStorageExtension::FSectionModelStorageExtension()
{
}

void FSectionModelStorageExtension::OnReinitialize()
{
	for (auto It = SectionToModel.CreateIterator(); It; ++It)
	{
		if (It.Key().ResolveObjectPtr() == nullptr || It.Value().Pin().Get() == nullptr)
		{
			It.RemoveCurrent();
		}
	}
	SectionToModel.Compact();
}

TSharedPtr<FSectionModel> FSectionModelStorageExtension::CreateModelForSection(UMovieSceneSection* InSection, TSharedRef<ISequencerSection> SectionInterface)
{
	FObjectKey SectionAsKey(InSection);
	if (TSharedPtr<FSectionModel> Existing = SectionToModel.FindRef(SectionAsKey).Pin())
	{
		return Existing;
	}

	TSharedPtr<FSectionModel> NewModel = MakeShared<FSectionModel>(InSection, SectionInterface);
	SectionToModel.Add(SectionAsKey, NewModel);

	return NewModel;
}

TSharedPtr<FSectionModel> FSectionModelStorageExtension::FindModelForSection(const UMovieSceneSection* InSection) const
{
	FObjectKey SectionAsKey(InSection);
	return SectionToModel.FindRef(SectionAsKey).Pin();
}

} // namespace Sequencer
} // namespace UE

