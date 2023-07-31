// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SCSDiff.h"
#include "CoreMinimal.h"
#include "DetailsDiff.h"
#include "DiffResults.h"
#include "IAssetTypeActions.h"
#include "Algo/Transform.h"

struct FDiffResultItem;
class SBlueprintDiff;
class UEdGraph;
struct FEdGraphEditAction;

/** Interface responsible for generating FBlueprintDifferenceTreeEntry's for visual diff tools */
class KISMET_API IDiffControl
{
public:
	virtual ~IDiffControl() {}

	/** Adds widgets to the tree of differences to show */
	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) = 0;
	
	static FText RightRevision;
	static TSharedRef<SWidget> GenerateSimpleDiffWidget(FText DiffText);
	static TSharedRef<SWidget> GenerateObjectDiffWidget(FSingleObjectDiffEntry DiffEntry, FText ObjectName);
	
};

/** Shows all differences for the blueprint structure itself that aren't picked up elsewhere */
class FMyBlueprintDiffControl : public TSharedFromThis<FMyBlueprintDiffControl>, public IDiffControl
{
public:
	FMyBlueprintDiffControl(const UBlueprint* InOldBlueprint, const UBlueprint* InNewBlueprint, FOnDiffEntryFocused InSelectionCallback);

	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;

private:
	FOnDiffEntryFocused SelectionCallback;
	const UBlueprint* OldBlueprint;
	const UBlueprint* NewBlueprint;
};

/** 
 * Each difference in the tree will either be a tree node that is added in one Blueprint 
 * or a tree node and an FName of a property that has been added or edited in one Blueprint
 */
class FSCSDiffControl : public TSharedFromThis<FSCSDiffControl>, public IDiffControl
{
public:
	FSCSDiffControl(const UBlueprint* InOldBlueprint, const UBlueprint* InNewBlueprint, FOnDiffEntryFocused InSelectionCallback);

	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;

	TSharedRef<SWidget> OldTreeWidget() { return OldSCS.TreeWidget(); }
	TSharedRef<SWidget> NewTreeWidget() { return NewSCS.TreeWidget(); }

private:
	FOnDiffEntryFocused SelectionCallback;
	FSCSDiffRoot DifferingProperties;

	FSCSDiff OldSCS;
	FSCSDiff NewSCS;
};

/** Generic wrapper around a details view template parameter determines whether TreeEntries are populated */
template<bool bPopulateOutTreeEntries>
class KISMET_API TDetailsDiffControl : public TSharedFromThis<TDetailsDiffControl<bPopulateOutTreeEntries>>, public IDiffControl
{
public:
	TDetailsDiffControl(const UObject* InOldObject, const UObject* InNewObject, FOnDiffEntryFocused InSelectionCallback)
		: SelectionCallback(InSelectionCallback)
		, OldDetails(InOldObject, FDetailsDiff::FOnDisplayedPropertiesChanged())
		, NewDetails(InNewObject, FDetailsDiff::FOnDisplayedPropertiesChanged())
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
	}

	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override
	{
		for (const FSingleObjectDiffEntry& Difference : DifferingProperties)
		{
			
			TSharedPtr<FBlueprintDifferenceTreeEntry> Entry = MakeShared<FBlueprintDifferenceTreeEntry>(
				FOnDiffEntryFocused::CreateSP(TSharedFromThis<TDetailsDiffControl<bPopulateOutTreeEntries>>::AsShared(), &TDetailsDiffControl::OnSelectDiffEntry, Difference.Identifier),
				FGenerateDiffEntryWidget::CreateStatic(&GenerateObjectDiffWidget, Difference, RightRevision));
			Children.Push(Entry);
			OutRealDifferences.Push(Entry);
			if constexpr (bPopulateOutTreeEntries)
			{
				OutTreeEntries.Push(Entry);
			}
		}
	}

	TSharedRef<SWidget> OldDetailsWidget() { return OldDetails.DetailsWidget(); }
	TSharedRef<SWidget> NewDetailsWidget() { return NewDetails.DetailsWidget(); }

protected:
	virtual void OnSelectDiffEntry(FPropertySoftPath PropertyName)
	{
		SelectionCallback.ExecuteIfBound();
		OldDetails.HighlightProperty(PropertyName);
		NewDetails.HighlightProperty(PropertyName);
	}

	FOnDiffEntryFocused SelectionCallback;
	FDetailsDiff OldDetails;
	FDetailsDiff NewDetails;

	TArray<FSingleObjectDiffEntry> DifferingProperties;
	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
};

using FDetailsDiffControl = TDetailsDiffControl<true>;

/** Override for CDO special case */
class FCDODiffControl : public TDetailsDiffControl<false>
{
public:
	FCDODiffControl(const UObject* InOldObject, const UObject* InNewObject, FOnDiffEntryFocused InSelectionCallback);

	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;
};

/** Override for class class settings */
class FClassSettingsDiffControl : public TDetailsDiffControl<false>
{
public:
	FClassSettingsDiffControl(const UObject* InOldObject, const UObject* InNewObject, FOnDiffEntryFocused InSelectionCallback);

	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;
};

/** Diff control to handle finding type-specific differences */
struct FBlueprintTypeDiffControl : public TSharedFromThis<FBlueprintTypeDiffControl>, public IDiffControl
{
	struct FSubObjectDiff
	{
		FDiffSingleResult SourceResult;
		FDetailsDiff OldDetails;
		FDetailsDiff NewDetails;
		TArray<TSharedPtr<FBlueprintDifferenceTreeEntry>> Diffs;

		FSubObjectDiff(const FDiffSingleResult& InSourceResult, const UObject* OldObject, const UObject* NewObject)
			: SourceResult(InSourceResult)
			, OldDetails(OldObject, FDetailsDiff::FOnDisplayedPropertiesChanged())
			, NewDetails(NewObject, FDetailsDiff::FOnDisplayedPropertiesChanged())
		{}
	};

	FBlueprintTypeDiffControl(const UBlueprint* InBlueprintOld, const UBlueprint* InBlueprintNew, FOnDiffEntryFocused InSelectionCallback);

	/** Generate difference tree widgets */
	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;

	/** The old blueprint (left) */
	const UBlueprint* BlueprintOld;

	/** The new blueprint(right) */
	const UBlueprint* BlueprintNew;

	/** Boxes that will display the details diffs */
	TSharedPtr<SBox> OldDetailsBox;
	TSharedPtr<SBox> NewDetailsBox;

private:
	/** Generate Widget for top category */
	TSharedRef<SWidget> GenerateCategoryWidget(bool bHasRealDiffs);

	/** Build up the Diff Source Array*/
	void BuildDiffSourceArray();

	/** Handle selecting a diff */
	void OnSelectSubobjectDiff(FPropertySoftPath Identifier, TSharedPtr<FSubObjectDiff> SubObjectDiff);

	/** List of objects with differences */
	TArray<TSharedPtr<FSubObjectDiff>> SubObjectDiffs;

	/** Source for list view */
	TArray<TSharedPtr<FDiffResultItem>> DiffListSource;

	/** Selection callback */
	FOnDiffEntryFocused SelectionCallback;

	/** Did diff generation succeed? */
	bool bDiffSucceeded;
};


/** Category list item for a graph*/
struct FGraphToDiff	: public TSharedFromThis<FGraphToDiff>, IDiffControl
{
	FGraphToDiff(SBlueprintDiff* DiffWidget, UEdGraph* GraphOld, UEdGraph* GraphNew, const FRevisionInfo& RevisionOld, const FRevisionInfo& RevisionNew);
	virtual ~FGraphToDiff() override;

	/** Add widgets to the differences tree */
	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;

	/** Get old(left) graph*/
	UEdGraph* GetGraphOld() const;

	/** Get new(right) graph*/
	UEdGraph* GetGraphNew() const;

	/** Source for list view */
	TArray<TSharedPtr<FDiffResultItem>> DiffListSource;
	TSharedPtr<TArray<FDiffSingleResult>> FoundDiffs;

	/** Index of the first item in RealDifferences that was generated by this graph */
	int32 RealDifferencesStartIndex = INDEX_NONE;

private:
	/** Get tooltip for category */
	FText GetToolTip();

	/** Generate Widget for category list */
	TSharedRef<SWidget> GenerateCategoryWidget();

	/** Called when the Newer Graph is modified*/
	void OnGraphChanged(const FEdGraphEditAction& Action);

	/** Build up the Diff Source Array*/
	void BuildDiffSourceArray();

	/** Diff widget */
	class SBlueprintDiff* DiffWidget;

	/** The old graph(left)*/
	UEdGraph* GraphOld;

	/** The new graph(right)*/
	UEdGraph* GraphNew;

	/** Description of Old and new graph*/
	FRevisionInfo	RevisionOld, RevisionNew;

	/** Handle to the registered OnGraphChanged delegate. */
	FDelegateHandle OnGraphChangedDelegateHandle;
};