// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tree/CurveEditorTreeFilter.h"

#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTemplate.h"



ECurveEditorTreeFilterType FCurveEditorTreeFilter::RegisterFilterType()
{
	static ECurveEditorTreeFilterType NextFilterType = ECurveEditorTreeFilterType::CUSTOM_START;
	ensureMsgf(NextFilterType != ECurveEditorTreeFilterType::First, TEXT("Maximum limit for registered curve tree filters (64) reached."));
	if (NextFilterType == ECurveEditorTreeFilterType::First)
	{
		return NextFilterType;
	}

	ECurveEditorTreeFilterType ThisFilterType = NextFilterType;

	// When the custom view ID reaches 0x80000000 the left shift will result in well-defined unsigned integer wraparound, resulting in 0 (None)
	NextFilterType = ECurveEditorTreeFilterType( ((__underlying_type(ECurveEditorTreeFilterType))NextFilterType) + 1 );

	return NextFilterType;
}

void FCurveEditorTreeTextFilter::AssignFromText(const FString& FilterString)
{
	ChildToParentFilterTerms.Reset();

	static const bool bCullEmpty = true;

	TArray<FString> FilterTerms;
	FilterString.ParseIntoArray(FilterTerms, TEXT(" "), bCullEmpty);

	TArray<FString> ParentToChildTerms;
	for (const FString& Term : FilterTerms)
	{
		ParentToChildTerms.Reset();
		Term.ParseIntoArray(ParentToChildTerms, TEXT("."), bCullEmpty);

		// Move the results into a new term in reverse order (so they are child -> parent)
		FCurveEditorTreeTextFilterTerm NewTerm;
		for (int32 Index = ParentToChildTerms.Num()-1; Index >= 0; --Index)
		{
			NewTerm.ChildToParentTokens.Emplace(FCurveEditorTreeTextFilterToken{ MoveTemp(ParentToChildTerms[Index]) });
		}
		ChildToParentFilterTerms.Emplace(MoveTemp(NewTerm));
	}
}