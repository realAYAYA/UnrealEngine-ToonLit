// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneObjectBindingIDPicker.h"
#include "MovieSceneSequenceID.h"
#include "Templates/SharedPointer.h"

class FDetailWidgetRow;
class FDragDropEvent;
class FDragDropOperation;
class FReply;
class IPropertyHandle;
class ISequencer;
class UMovieSceneSequence;
struct FGeometry;

namespace UE
{
namespace MovieScene
{
	struct FFixedObjectBindingID;
}
}



class MOVIESCENETOOLS_API FMovieSceneObjectBindingIDCustomization
	: public IPropertyTypeCustomization
	, FMovieSceneObjectBindingIDPicker
{
public:

	FMovieSceneObjectBindingIDCustomization()
	{}

	FMovieSceneObjectBindingIDCustomization(FMovieSceneSequenceID InLocalSequenceID, TWeakPtr<ISequencer> InSequencer)
		: FMovieSceneObjectBindingIDPicker(InLocalSequenceID, InSequencer)
	{}

	static void BindTo(TSharedRef<ISequencer> InSequencer);

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

private:

	virtual UMovieSceneSequence* GetSequence() const override;

	virtual void SetCurrentValue(const FMovieSceneObjectBindingID& InBindingId) override;

	virtual FMovieSceneObjectBindingID GetCurrentValue() const override;

	virtual bool HasMultipleValues() const override;

	FReply OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent);

	TSharedPtr<IPropertyHandle> StructProperty;
};