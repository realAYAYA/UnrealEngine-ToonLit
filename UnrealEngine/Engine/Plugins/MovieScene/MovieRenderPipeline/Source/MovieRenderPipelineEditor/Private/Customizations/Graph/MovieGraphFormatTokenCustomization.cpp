// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphFormatTokenCustomization.h"

#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/Nodes/MovieGraphDebugNode.h"
#include "Graph/Nodes/MovieGraphFileOutputNode.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SMoviePipelineFormatTokenAutoCompleteBox.h"

void FMovieGraphFormatTokenCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	CustomizedObject = DetailBuilder.GetSelectedObjects()[0];
	check(CustomizedObject.IsValid() && !CustomizedObject.IsStale());

	// This should work just fine for UMovieGraphFileOutputNode and UMovieGraphCommandLineEncoderNode as long as the property names stay in sync
	OutputFormatPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieGraphFileOutputNode, FileNameFormat));

	// If we can't find it, try the debug node
	if (!OutputFormatPropertyHandle->IsValidHandle())
	{
		OutputFormatPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UMovieGraphDebugSettingNode, UnrealInsightsTraceFileNameFormat));
	}

	// If we still can't find it, early out
	if (!OutputFormatPropertyHandle->IsValidHandle())
	{
		return;
	}

	FText StartingDisplayText;
	OutputFormatPropertyHandle->GetValueAsDisplayText(StartingDisplayText);

	// Update the text box when the handle value changes
	OutputFormatPropertyHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &FMovieGraphFormatTokenCustomization::OnPropertyChange));

	DetailBuilder.EditDefaultProperty(OutputFormatPropertyHandle)->CustomWidget()
		.NameContent()
		[
			OutputFormatPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
	[
			// We choose not to bind the text box text here because simultaneously setting and getting 
			// the handle value can cause the binding to stop working, so we manually update it when necessary 
			SAssignNew(AutoCompleteBox, SMoviePipelineFormatTokenAutoCompleteBox)
			.InitialText(StartingDisplayText)
			.Suggestions(this, &FMovieGraphFormatTokenCustomization::GetSuggestions)
			.OnTextChanged(this, &FMovieGraphFormatTokenCustomization::OnTextChanged)
		];
}

void FMovieGraphFormatTokenCustomization::OnPropertyChange()
{
	// Sync the text box back to the handle value
	FText DisplayText;
	OutputFormatPropertyHandle->GetValueAsDisplayText(DisplayText);

	AutoCompleteBox->SetText(DisplayText);
	AutoCompleteBox->CloseMenuAndReset();
}

TArray<FString> FMovieGraphFormatTokenCustomization::GetSuggestions() const
{
	TArray<FString> Suggestions;
	FMovieGraphResolveArgs FormatArgs;
	GetFormatArguments(FormatArgs);

	// Display the token names alphabetically
	TArray<FString> FilenameFormatTokens;
	FormatArgs.FilenameArguments.GetKeys(FilenameFormatTokens);
	FilenameFormatTokens.Sort();

	for (const FString& FilenameFormatToken : FilenameFormatTokens)
	{
		Suggestions.Add(FilenameFormatToken);
	}

	return Suggestions;
}

void FMovieGraphFormatTokenCustomization::OnTextChanged(const FText& InValue)
{
	OutputFormatPropertyHandle->SetValue(InValue.ToString());
}

void FMovieGraphFormatTokenCustomization::GetFormatArguments(FMovieGraphResolveArgs& InOutFormatArgs)
{
	// Just fetch the format arguments (by keeping the format string empty). The tokens themselves will not be resolved correctly here (no context is
	// provided in the resolve params), but all we care about here is the token list, not the resolved token values.
	const FString FormatString;
	const FMovieGraphFilenameResolveParams ResolveParams;
	UMovieGraphBlueprintLibrary::ResolveFilenameFormatArguments(FormatString, ResolveParams, InOutFormatArgs);
}
