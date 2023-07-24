// Copyright Epic Games, Inc. All Rights Reserved.

#include "DiffControl.h"

#include "DiffResults.h"
#include "GraphDiffControl.h"
#include "SBlueprintDiff.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SBlueprintDif"

// well known algorithm for computing the Longest Common Subsequence table of two ordered lists
template <typename RangeType, typename ComparePredicate>
static TArray<TArray<int32>> CalculateLCSTable(const RangeType& Range1, const RangeType& Range2, ComparePredicate Comparison)
{
	TArray<TArray<int32>> LCS;
	LCS.SetNum(Range1.Num() + 1);
	for (int32 I = 0; I <= Range1.Num(); I++)
	{
		LCS[I].SetNum(Range2.Num() + 1);
		if (I == 0)
		{
			continue;
		}
	
		for (int32 J = 1; J <= Range2.Num(); J++)
		{
			if (Comparison(Range1[I - 1], Range2[J - 1]))
			{
				LCS[I][J] = LCS[I - 1][J - 1] + 1;
			}
			else
			{
				LCS[I][J] = FMath::Max(LCS[I - 1][J], LCS[I][J - 1]);
			}
		}
	}
	return LCS;
}

/////////////////////////////////////////////////////////////////////////////
/// IDiffControl

FText IDiffControl::RightRevision = LOCTEXT("OlderRevisionIdentifier", "Right Revision");

TSharedRef<SWidget> IDiffControl::GenerateSimpleDiffWidget(FText DiffText)
{
	return SNew(STextBlock)
		.Text(DiffText)
		.ToolTipText(DiffText)
		.ColorAndOpacity(DiffViewUtils::Differs());
}

TSharedRef<SWidget> IDiffControl::GenerateObjectDiffWidget(FSingleObjectDiffEntry DiffEntry, FText ObjectName)
{
	return SNew(STextBlock)
		.Text(DiffViewUtils::PropertyDiffMessage(DiffEntry, ObjectName))
		.ToolTipText(DiffViewUtils::PropertyDiffMessage(DiffEntry, ObjectName))
		.ColorAndOpacity(DiffViewUtils::Differs());
}


/////////////////////////////////////////////////////////////////////////////
/// FMyBlueprintDiffControl

FMyBlueprintDiffControl::FMyBlueprintDiffControl(const UBlueprint* InOldBlueprint, const UBlueprint* InNewBlueprint, FOnDiffEntryFocused InSelectionCallback)
	: SelectionCallback(MoveTemp(InSelectionCallback))
	, OldBlueprint(InOldBlueprint)
	, NewBlueprint(InNewBlueprint)
{}

void FMyBlueprintDiffControl::GenerateTreeEntries(TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutTreeEntries, TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutRealDifferences)
{
	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;

	if (OldBlueprint && OldBlueprint->SkeletonGeneratedClass && NewBlueprint && NewBlueprint->SkeletonGeneratedClass)
	{
		for (TFieldIterator<FProperty> PropertyIt(OldBlueprint->SkeletonGeneratedClass); PropertyIt; ++PropertyIt)
		{
			FProperty* OldProperty = *PropertyIt;
			FProperty* NewProperty = NewBlueprint->SkeletonGeneratedClass->FindPropertyByName(OldProperty->GetFName());

			FText PropertyText = FText::FromString(OldProperty->GetAuthoredName());

			if (NewProperty)
			{
				const int32 OldVarIndex = FBlueprintEditorUtils::FindNewVariableIndex(OldBlueprint, OldProperty->GetFName());
				const int32 NewVarIndex = FBlueprintEditorUtils::FindNewVariableIndex(NewBlueprint, OldProperty->GetFName());

				if (OldVarIndex != INDEX_NONE && NewVarIndex != INDEX_NONE)
				{
					TArray<FSingleObjectDiffEntry> DifferingProperties;
					DiffUtils::CompareUnrelatedStructs(FBPVariableDescription::StaticStruct(), &OldBlueprint->NewVariables[OldVarIndex], FBPVariableDescription::StaticStruct(), &NewBlueprint->NewVariables[NewVarIndex], DifferingProperties);
					for (const FSingleObjectDiffEntry& Difference : DifferingProperties)
					{
						TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
							SelectionCallback,
							FGenerateDiffEntryWidget::CreateStatic(&GenerateObjectDiffWidget, Difference, PropertyText));
						Children.Push(Entry);
						OutRealDifferences.Push(Entry);
					}
				}	
			}
			else
			{
				FText DiffText = FText::Format(LOCTEXT("VariableRemoved", "Removed Variable {0}"), PropertyText);

				TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
					SelectionCallback,
					FGenerateDiffEntryWidget::CreateStatic(&GenerateSimpleDiffWidget, DiffText));

				Children.Push(Entry);
				OutRealDifferences.Push(Entry);
			}
		}

		for (TFieldIterator<FProperty> PropertyIt(NewBlueprint->SkeletonGeneratedClass); PropertyIt; ++PropertyIt)
		{
			FProperty* NewProperty = *PropertyIt;
			FProperty* OldProperty = OldBlueprint->SkeletonGeneratedClass->FindPropertyByName(NewProperty->GetFName());

			if (!OldProperty)
			{
				FText DiffText = FText::Format(LOCTEXT("VariableAdded", "Added Variable {0}"), FText::FromString(NewProperty->GetAuthoredName()));

				TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
					SelectionCallback,
					FGenerateDiffEntryWidget::CreateStatic(&GenerateSimpleDiffWidget, DiffText));

				Children.Push(Entry);
				OutRealDifferences.Push(Entry);
			}
		}
	}
	const bool bHasDifferences = Children.Num() != 0;
	if (!bHasDifferences)
	{
		// make one child informing the user that there are no differences:
		Children.Push(FBlueprintDifferenceTreeEntry::NoDifferencesEntry());
	}

	static const FText MyBlueprintLabel = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "MyBlueprintLabel", "My Blueprint");
	static const FText MyBlueprintTooltip = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "MyBlueprintTooltip", "The list of changes made to blueprint structure in the My Blueprint panel");
	OutTreeEntries.Push(FBlueprintDifferenceTreeEntry::CreateCategoryEntry(
		MyBlueprintLabel,
		MyBlueprintTooltip,
		SelectionCallback,
		Children,
		bHasDifferences
	));
}


/////////////////////////////////////////////////////////////////////////////
/// FSCSDiffControl

FSCSDiffControl::FSCSDiffControl(const UBlueprint* InOldBlueprint, const UBlueprint* InNewBlueprint, FOnDiffEntryFocused InSelectionCallback)
	: SelectionCallback(InSelectionCallback)
	, OldSCS(InOldBlueprint)
	, NewSCS(InNewBlueprint)
{
}

void FSCSDiffControl::GenerateTreeEntries(TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutTreeEntries, TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutRealDifferences)
{
	TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>> Children;
	if (OldSCS.GetBlueprint() && NewSCS.GetBlueprint())
	{
		const TArray< FSCSResolvedIdentifier > OldHierarchy = OldSCS.GetDisplayedHierarchy();
		const TArray< FSCSResolvedIdentifier > NewHierarchy = NewSCS.GetDisplayedHierarchy();
		DiffUtils::CompareUnrelatedSCS(OldSCS.GetBlueprint(), OldHierarchy, NewSCS.GetBlueprint(), NewHierarchy, DifferingProperties);

		const auto FocusSCSDifferenceEntry = [](FSCSDiffEntry Entry, FOnDiffEntryFocused InSelectionCallback, FSCSDiffControl* Owner)
		{
			InSelectionCallback.ExecuteIfBound();
			if (Entry.TreeIdentifier.Name != NAME_None)
			{
				Owner->OldSCS.HighlightProperty(Entry.TreeIdentifier.Name, FPropertyPath());
				Owner->NewSCS.HighlightProperty(Entry.TreeIdentifier.Name, FPropertyPath());
			}
		};

		const auto CreateSCSDifferenceWidget = [](FSCSDiffEntry Entry, FText ObjectName) -> TSharedRef<SWidget>
		{
			return SNew(STextBlock)
					.Text(DiffViewUtils::SCSDiffMessage(Entry, ObjectName))
					.ColorAndOpacity(DiffViewUtils::Differs());
		};

		for (const FSCSDiffEntry& Difference : DifferingProperties.Entries)
		{
			TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
				FOnDiffEntryFocused::CreateStatic(FocusSCSDifferenceEntry, Difference, SelectionCallback, this),
				FGenerateDiffEntryWidget::CreateStatic(CreateSCSDifferenceWidget, Difference, RightRevision));
			Children.Push(Entry);
			OutRealDifferences.Push(Entry);
		}
	}
	const bool bHasDifferences = Children.Num() != 0;
	if (!bHasDifferences)
	{
		// make one child informing the user that there are no differences:
		Children.Push(FBlueprintDifferenceTreeEntry::NoDifferencesEntry());
	}

	static const FText SCSLabel = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSLabel", "Components");
	static const FText SCSTooltip = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SCSTooltip", "The list of changes made in the Components panel");
	OutTreeEntries.Push(FBlueprintDifferenceTreeEntry::CreateCategoryEntry(
		SCSLabel,
		SCSTooltip,
		SelectionCallback,
		Children,
		bHasDifferences
	));
}

FDetailsDiffControl::FDetailsDiffControl(const UObject* InOldObject, const UObject* InNewObject,
	FOnDiffEntryFocused InSelectionCallback, bool bPopulateOutTreeEntries): SelectionCallback(InSelectionCallback)
	                                                                        , OldDetails(InOldObject, FDetailsDiff::FOnDisplayedPropertiesChanged())
	                                                                        , NewDetails(InNewObject, FDetailsDiff::FOnDisplayedPropertiesChanged())
	                                                                        , bPopulateOutTreeEntries(bPopulateOutTreeEntries)
{
	OldDetails.DiffAgainst(NewDetails, DifferingProperties, true);

	TSet<FPropertyPath> PropertyPaths;
	Algo::Transform(DifferingProperties, PropertyPaths,
	                [&InOldObject](const FSingleObjectDiffEntry& DiffEntry)
	                {
		                return DiffEntry.Identifier.ResolvePath(InOldObject);
	                });

	OldDetails.DetailsWidget()->UpdatePropertyAllowList(PropertyPaths);

	PropertyPaths.Reset();
	Algo::Transform(DifferingProperties, PropertyPaths,
	                [&InNewObject](const FSingleObjectDiffEntry& DiffEntry)
	                {
		                return DiffEntry.Identifier.ResolvePath(InNewObject);
	                });

	NewDetails.DetailsWidget()->UpdatePropertyAllowList(PropertyPaths);
		
	// Sync the scrolling between the left and right panels
	const TSharedRef<IDetailsView> OldDetailsView = OldDetails.DetailsWidget();
	const TSharedRef<IDetailsView> NewDetailsView = NewDetails.DetailsWidget();
	FDetailsDiff::LinkScrolling(OldDetails, NewDetails, GetLinkedScrollRateAttribute(OldDetailsView, NewDetailsView));;
}

void FDetailsDiffControl::GenerateTreeEntries(TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutTreeEntries, TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutRealDifferences)
{
	for (const FSingleObjectDiffEntry& Difference : DifferingProperties)
	{
		TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
			FOnDiffEntryFocused::CreateSP(TSharedFromThis<FDetailsDiffControl>::AsShared(), &FDetailsDiffControl::OnSelectDiffEntry, Difference.Identifier),
			FGenerateDiffEntryWidget::CreateStatic(&GenerateObjectDiffWidget, Difference, RightRevision));
		Children.Push(Entry);
		OutRealDifferences.Push(Entry);
		if (bPopulateOutTreeEntries)
		{
			OutTreeEntries.Push(Entry);
		}
	}
}

void FDetailsDiffControl::OnSelectDiffEntry(FPropertySoftPath PropertyName)
{
	SelectionCallback.ExecuteIfBound();
	OldDetails.HighlightProperty(PropertyName);
	NewDetails.HighlightProperty(PropertyName);
}

TAttribute<TArray<FVector2f>> FDetailsDiffControl::GetLinkedScrollRateAttribute(const TSharedRef<IDetailsView>& OldDetailsView, const TSharedRef<IDetailsView>& NewDetailsView)
{
	return TAttribute<TArray<FVector2f>>::CreateRaw(this, &FDetailsDiffControl::GetLinkedScrollRate, OldDetailsView, NewDetailsView);
}

TArray<FVector2f> FDetailsDiffControl::GetLinkedScrollRate(TSharedRef<IDetailsView> OldDetailsView, TSharedRef<IDetailsView> NewDetailsView) const
{
	TArray<TPair<int32, FPropertyPath>> OldProperties = OldDetailsView->GetPropertyRowNumbers();
	TArray<TPair<int32, FPropertyPath>> NewProperties = NewDetailsView->GetPropertyRowNumbers();

	const int32 OldRowCount = OldDetailsView->CountRows();
	const int32 NewRowCount = NewDetailsView->CountRows();

	// use caching to avoid O(n^2) LCS calculation every frame
	if(LinkedScrollRateCache.OldProperties != OldProperties || LinkedScrollRateCache.NewProperties != NewProperties)
	{
		const auto PropertyPathsEqual = [](const TPair<int32, FPropertyPath>& A, const TPair<int32, FPropertyPath>& B)
		{
			return A.Value == B.Value;
		};
		
		// Find the Longest Common Subsequence between both sets of properties
		const TArray<TArray<int32>> LCS = CalculateLCSTable(OldProperties, NewProperties, PropertyPathsEqual);
	
		// Using the LCS Table, we can determine which lines should match one another while scrolling. For example, an element
		// of FixedPoints may be {12.f, 5.f} meaning that line 12 of the left panel is the same as line 5 of the right panel
		TArray<FVector2f> FixedPoints;

		int32 I = OldProperties.Num();
		int32 J = NewProperties.Num();
		while (I > 0 && J > 0)
		{
			if (OldProperties[I - 1].Value == NewProperties[J - 1].Value)
			{
				const int32 OldLineNum = OldProperties[I - 1].Key;
				const int32 NewLineNum = NewProperties[J - 1].Key;
				// add two endpoints for every matched property because the elements have a width of 1 in the panels
				FixedPoints.Add(FVector2f((OldLineNum + 1.f) / OldRowCount, (NewLineNum + 1.f) / NewRowCount));
				FixedPoints.Add(FVector2f((float)OldLineNum / OldRowCount, (float)NewLineNum / NewRowCount));
				--I;
				--J;
			}
			else if (LCS[I - 1][J] <= LCS[I][J	 - 1])
			{
				--J;
			}
			else
			{
				--I;
			}
		}
		FixedPoints.Add({0.f, 0.f});
		Algo::Reverse(FixedPoints);
		FixedPoints.Add({1.f, 1.f});
        	
		LinkedScrollRateCache.OldProperties = OldProperties;
		LinkedScrollRateCache.NewProperties = NewProperties;
		LinkedScrollRateCache.ScrollRate = FixedPoints;
	}
	
	return LinkedScrollRateCache.ScrollRate;
}


/////////////////////////////////////////////////////////////////////////////
/// FCDODiffControl

FCDODiffControl::FCDODiffControl(const UObject* InOldObject, const UObject* InNewObject, FOnDiffEntryFocused InSelectionCallback)
	: FDetailsDiffControl(InOldObject, InNewObject, InSelectionCallback, false)
{
}

void FCDODiffControl::GenerateTreeEntries(TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutTreeEntries, TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutRealDifferences)
{
	FDetailsDiffControl::GenerateTreeEntries(OutTreeEntries, OutRealDifferences);

	const bool bHasDifferences = Children.Num() != 0;
	if (!bHasDifferences)
	{
		// make one child informing the user that there are no differences:
		Children.Push(FBlueprintDifferenceTreeEntry::NoDifferencesEntry());
	}

	static const FText DefaultsLabel = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsLabel", "Defaults");
	static const FText DefaultsTooltip = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "DefaultsTooltip", "The list of changes made in the Defaults panel");
	OutTreeEntries.Push(FBlueprintDifferenceTreeEntry::CreateCategoryEntry(
		DefaultsLabel,
		DefaultsTooltip,
		SelectionCallback,
		Children,
		bHasDifferences
	));
}

/////////////////////////////////////////////////////////////////////////////
/// FClassSettingsDiffControl

FClassSettingsDiffControl::FClassSettingsDiffControl(const UObject* InOldObject, const UObject* InNewObject, FOnDiffEntryFocused InSelectionCallback)
	: FDetailsDiffControl(InOldObject, InNewObject, InSelectionCallback, false)
{
}

void FClassSettingsDiffControl::GenerateTreeEntries(TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutTreeEntries, TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>>& OutRealDifferences)
{
	FDetailsDiffControl::GenerateTreeEntries(OutTreeEntries, OutRealDifferences);

	const bool bHasDifferences = Children.Num() != 0;
	if (!bHasDifferences)
	{
		// make one child informing the user that there are no differences:
		Children.Push(FBlueprintDifferenceTreeEntry::NoDifferencesEntry());
	}

	static const FText SettingsLabel = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SettingsLabel", "Class Settings");
	static const FText SettingsTooltip = NSLOCTEXT("FBlueprintDifferenceTreeEntry", "SettingsTooltip", "The list of changes made in the Class Settings panel");
	OutTreeEntries.Push(FBlueprintDifferenceTreeEntry::CreateCategoryEntry(
		SettingsLabel,
		SettingsTooltip,
		SelectionCallback,
		Children,
		bHasDifferences
	));
}


/////////////////////////////////////////////////////////////////////////////
/// FBlueprintTypeDiffControl

FBlueprintTypeDiffControl::FBlueprintTypeDiffControl(const UBlueprint* InBlueprintOld, const UBlueprint* InBlueprintNew, FOnDiffEntryFocused InSelectionCallback)
	: BlueprintOld(InBlueprintOld)
	, BlueprintNew(InBlueprintNew)
	, SelectionCallback(InSelectionCallback)
	, bDiffSucceeded(false)
{
	check(InBlueprintNew || InBlueprintOld);
}

void FBlueprintTypeDiffControl::GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences)
{
	BuildDiffSourceArray();

	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
	
	bool bHasRealChange = false;

	// First add manual diffs in main category
	for (const TSharedPtr<FDiffResultItem>& Difference : DiffListSource)
	{
		TSharedPtr<FBlueprintDifferenceTreeEntry> ChildEntry = MakeShared<FBlueprintDifferenceTreeEntry>(
			SelectionCallback,
			FGenerateDiffEntryWidget::CreateSP(Difference.ToSharedRef(), &FDiffResultItem::GenerateWidget));
		Children.Push(ChildEntry);
		OutRealDifferences.Push(ChildEntry);

		if (Difference->Result.IsRealDifference())
		{
			bHasRealChange = true;
		}
	}

	if (Children.Num() == 0)
	{
		// Make one child informing the user that there are no differences, or that it is unknown
		if (bDiffSucceeded)
		{
			Children.Push(FBlueprintDifferenceTreeEntry::NoDifferencesEntry());
		}
		else
		{
			Children.Push(FBlueprintDifferenceTreeEntry::UnknownDifferencesEntry());
		}
	}

	TSharedPtr<FBlueprintDifferenceTreeEntry> CategoryEntry = MakeShared<FBlueprintDifferenceTreeEntry>(
			SelectionCallback,
			FGenerateDiffEntryWidget::CreateSP(AsShared(), &FBlueprintTypeDiffControl::GenerateCategoryWidget, bHasRealChange),
			Children);
	OutTreeEntries.Push(CategoryEntry);

	// Now add subobject diffs, one category per object
	for (const TSharedPtr<FSubObjectDiff>& SubObjectDiff : SubObjectDiffs)
	{
		Children.Reset();

		Children.Append(SubObjectDiff->Diffs);
		OutRealDifferences.Append(SubObjectDiff->Diffs);

		TSharedPtr<FBlueprintDifferenceTreeEntry> SubObjectEntry = FBlueprintDifferenceTreeEntry::CreateCategoryEntry(
			SubObjectDiff->SourceResult.DisplayString,
			SubObjectDiff->SourceResult.ToolTip,
			FOnDiffEntryFocused::CreateSP(AsShared(), &FBlueprintTypeDiffControl::OnSelectSubobjectDiff, FPropertySoftPath(), SubObjectDiff),
			Children,
			true);

		OutTreeEntries.Push(SubObjectEntry);
	}
}

TSharedRef<SWidget> FBlueprintTypeDiffControl::GenerateCategoryWidget(bool bHasRealDiffs)
{
	FLinearColor Color = FLinearColor::White;

	if (bHasRealDiffs)
	{
		Color = DiffViewUtils::Differs();
	}

	const FText Label = (BlueprintNew ? BlueprintNew : BlueprintOld)->GetClass()->GetDisplayNameText();

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(STextBlock)
			.ColorAndOpacity(Color)
		.Text(Label)
		];
}

void FBlueprintTypeDiffControl::BuildDiffSourceArray()
{
	if (!BlueprintNew || !BlueprintOld)
	{
		return;
	}
	
	TArray<FDiffSingleResult> BlueprintDiffResults;
	FDiffResults BlueprintDiffs(&BlueprintDiffResults);
	if (BlueprintNew->FindDiffs(BlueprintOld, BlueprintDiffs))
	{
		bDiffSucceeded = true;

		// Add manual diffs
		for (const FDiffSingleResult& CurrentDiff : BlueprintDiffResults)
		{
			if (CurrentDiff.Diff == EDiffType::OBJECT_REQUEST_DIFF)
			{
				// Turn into a subobject diff

				// Invert order, we want old then new
				TSharedPtr<FSubObjectDiff> SubObjectDiff = MakeShared<FSubObjectDiff>(CurrentDiff, CurrentDiff.Object2, CurrentDiff.Object1);

				TArray<FSingleObjectDiffEntry> DifferingProperties;
				SubObjectDiff->OldDetails.DiffAgainst(SubObjectDiff->NewDetails, DifferingProperties, true);

				if (DifferingProperties.Num() > 0)
				{
					// Actual differences, so add to tree
					SubObjectDiffs.Add(SubObjectDiff);

					for (const FSingleObjectDiffEntry& Difference : DifferingProperties)
					{
						TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
							FOnDiffEntryFocused::CreateSP(AsShared(), &FBlueprintTypeDiffControl::OnSelectSubobjectDiff, Difference.Identifier, SubObjectDiff),
							FGenerateDiffEntryWidget::CreateStatic(&GenerateObjectDiffWidget, Difference, RightRevision));
						SubObjectDiff->Diffs.Push(Entry);
					}
				}
			}
			else
			{
				DiffListSource.Add(MakeShared<FDiffResultItem>(CurrentDiff));
			}
		}

		struct SortDiff
		{
			bool operator () (const TSharedPtr<FDiffResultItem>& A, const TSharedPtr<FDiffResultItem>& B) const
			{
				return A->Result.Diff < B->Result.Diff;
			}
		};

		Sort(DiffListSource.GetData(), DiffListSource.Num(), SortDiff());
	}
}

void FBlueprintTypeDiffControl::OnSelectSubobjectDiff(FPropertySoftPath Identifier, TSharedPtr<FSubObjectDiff> SubObjectDiff)
{
	// This allows the owning control to focus the correct tab (or do whatever else it likes):
	SelectionCallback.ExecuteIfBound();

	if (SubObjectDiff.IsValid())
	{
		SubObjectDiff->OldDetails.HighlightProperty(Identifier);
		SubObjectDiff->NewDetails.HighlightProperty(Identifier);

		OldDetailsBox->SetContent(SubObjectDiff->OldDetails.DetailsWidget());
		NewDetailsBox->SetContent(SubObjectDiff->NewDetails.DetailsWidget());
	}
}


/////////////////////////////////////////////////////////////////////////////
/// FGraphToDiff

FGraphToDiff::FGraphToDiff(SBlueprintDiff* InDiffWidget, UEdGraph* InGraphOld, UEdGraph* InGraphNew, const FRevisionInfo& InRevisionOld, const FRevisionInfo& InRevisionNew)
	: FoundDiffs(MakeShared<TArray<FDiffSingleResult>>()), DiffWidget(InDiffWidget), GraphOld(InGraphOld), GraphNew(InGraphNew), RevisionOld(InRevisionOld), RevisionNew(InRevisionNew)
{
	check(InGraphOld || InGraphNew); //one of them needs to exist

	//need to know when it is modified
	if (InGraphNew)
	{
		OnGraphChangedDelegateHandle = InGraphNew->AddOnGraphChangedHandler( FOnGraphChanged::FDelegate::CreateRaw(this, &FGraphToDiff::OnGraphChanged));
	}

	BuildDiffSourceArray();
}

FGraphToDiff::~FGraphToDiff()
{
	if (GraphNew)
	{
		GraphNew->RemoveOnGraphChangedHandler( OnGraphChangedDelegateHandle);
	}
}

void FGraphToDiff::GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences)
{
	if (!DiffListSource.IsEmpty())
	{
		RealDifferencesStartIndex = OutRealDifferences.Num();
	}
	
	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
	for (const TSharedPtr<FDiffResultItem>& Difference : DiffListSource)
	{
		TSharedPtr<FBlueprintDifferenceTreeEntry> ChildEntry = MakeShared<FBlueprintDifferenceTreeEntry>(
				FOnDiffEntryFocused::CreateRaw(DiffWidget, &SBlueprintDiff::OnDiffListSelectionChanged, Difference),
				FGenerateDiffEntryWidget::CreateSP(Difference.ToSharedRef(), &FDiffResultItem::GenerateWidget));
		Children.Push(ChildEntry);
		OutRealDifferences.Push(ChildEntry);
	}

	if (Children.Num() == 0)
	{
		// make one child informing the user that there are no differences:
		Children.Push(FBlueprintDifferenceTreeEntry::NoDifferencesEntry());
	}

	TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
			FOnDiffEntryFocused::CreateRaw(DiffWidget, &SBlueprintDiff::OnGraphSelectionChanged, TSharedPtr<FGraphToDiff>(AsShared()), ESelectInfo::Direct),
			FGenerateDiffEntryWidget::CreateSP(AsShared(), &FGraphToDiff::GenerateCategoryWidget),
			Children);
	OutTreeEntries.Push(Entry);
}

UEdGraph* FGraphToDiff::GetGraphOld() const
{
	return GraphOld;
}

UEdGraph* FGraphToDiff::GetGraphNew() const
{
	return GraphNew;
}

FText FGraphToDiff::GetToolTip()
{
	if (GraphOld && GraphNew)
	{
		if (DiffListSource.Num() > 0)
		{
			return LOCTEXT("ContainsDifferences", "Revisions are different");
		}
		else
		{
			return LOCTEXT("GraphsIdentical", "Revisions appear to be identical");
		}
	}
	else
	{
		UEdGraph* GoodGraph = GraphOld ? GraphOld : GraphNew;
		check(GoodGraph);
		const FRevisionInfo& Revision = GraphNew ? RevisionOld : RevisionNew;
		FText RevisionText = LOCTEXT("CurrentRevision", "Current Revision");

		if (!Revision.Revision.IsEmpty())
		{
			RevisionText = FText::Format(LOCTEXT("Revision Number", "Revision {0}"), FText::FromString(Revision.Revision));
		}

		return FText::Format(LOCTEXT("MissingGraph", "Graph '{0}' missing from {1}"), FText::FromString(GoodGraph->GetName()), RevisionText);
	}
}

TSharedRef<SWidget> FGraphToDiff::GenerateCategoryWidget()
{
	const UEdGraph* Graph = GraphOld ? GraphOld : GraphNew;
	check(Graph);
	
	FLinearColor Color = (GraphOld && GraphNew) ? DiffViewUtils::Identical() : FLinearColor(0.3f,0.3f,1.f);

	const bool bHasDiffs = DiffListSource.Num() > 0;

	if (bHasDiffs)
	{
		Color = DiffViewUtils::Differs();
	}

	FText GraphName;
	if (const UEdGraphSchema* Schema = Graph->GetSchema())
	{
		FGraphDisplayInfo DisplayInfo;
		Schema->GetGraphDisplayInformation(*Graph, DisplayInfo);

		GraphName = DisplayInfo.DisplayName;
	}
	else
	{
		GraphName = FText::FromName(Graph->GetFName());
	}

	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	[
		SNew(STextBlock)
		.ColorAndOpacity(Color)
		.Text(GraphName)
		.ToolTipText(GetToolTip())
	]
	+ DiffViewUtils::Box( GraphOld != nullptr, Color )
	+ DiffViewUtils::Box( GraphNew != nullptr, Color );
}

void FGraphToDiff::BuildDiffSourceArray()
{
	FoundDiffs->Empty();
	FGraphDiffControl::DiffGraphs(GraphOld, GraphNew, *FoundDiffs);

	struct SortDiff
	{
		bool operator () (const FDiffSingleResult& A, const FDiffSingleResult& B) const
		{
			return A.Diff < B.Diff;
		}
	};

	Sort(FoundDiffs->GetData(), FoundDiffs->Num(), SortDiff());

	DiffListSource.Empty();
	for (const FDiffSingleResult& Diff : *FoundDiffs)
	{
		DiffListSource.Add(MakeShared<FDiffResultItem>(Diff));
	}
}

void FGraphToDiff::OnGraphChanged( const FEdGraphEditAction& Action )
{
	DiffWidget->OnGraphChanged(this);
}

#undef LOCTEXT_NAMESPACE