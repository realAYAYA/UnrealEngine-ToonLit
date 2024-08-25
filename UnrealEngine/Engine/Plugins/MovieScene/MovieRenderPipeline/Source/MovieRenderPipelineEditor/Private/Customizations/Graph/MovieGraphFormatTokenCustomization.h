// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineQueue.h"
#include "IDetailCustomization.h"
#include "PropertyHandle.h"

class FMovieGraphFormatTokenCustomization : public IDetailCustomization
{
public:
	/** Creates a detail customization instance */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FMovieGraphFormatTokenCustomization>();
	}

	FMovieGraphFormatTokenCustomization(){}

protected:
	//~ Begin IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization interface

	void OnPropertyChange();

	TArray<FString> GetSuggestions() const;

	void OnTextChanged(const FText& InValue);

	static void GetFormatArguments(FMovieGraphResolveArgs& InOutFormatArgs);
	
	TWeakObjectPtr<UObject> CustomizedObject;
	TSharedPtr<IPropertyHandle> OutputFormatPropertyHandle;
	TSharedPtr<class SMoviePipelineFormatTokenAutoCompleteBox> AutoCompleteBox;
};
