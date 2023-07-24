// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "MVVM/Extensions/DynamicExtensionContainer.h"
#include "MVVM/ViewModelTypeID.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectKey.h"

class ISequencerSection;
class UMovieSceneSection;

namespace UE
{
namespace Sequencer
{

class FSectionModel;
class FSequenceModel;

class SEQUENCER_API FSectionModelStorageExtension
	: public IDynamicExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FSectionModelStorageExtension);

	FSectionModelStorageExtension();

	TSharedPtr<FSectionModel> CreateModelForSection(UMovieSceneSection* InSection, TSharedRef<ISequencerSection> SectionInterface);

	TSharedPtr<FSectionModel> FindModelForSection(const UMovieSceneSection* InSection) const;

	virtual void OnReinitialize() override;

private:

	TMap<FObjectKey, TWeakPtr<FSectionModel>> SectionToModel;
};

} // namespace Sequencer
} // namespace UE

