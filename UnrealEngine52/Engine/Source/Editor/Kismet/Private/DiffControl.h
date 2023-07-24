// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SCSDiff.h"
#include "CoreMinimal.h"
#include "DetailsDiff.h"
#include "DiffResults.h"
#include "IAssetTypeActions.h"
#include "IDetailsView.h"
#include "Algo/Transform.h"
#include "Widgets/SWidget.h"

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
class KISMET_API FDetailsDiffControl : public TSharedFromThis<FDetailsDiffControl>, public IDiffControl
{
public:
	FDetailsDiffControl(const UObject* InOldObject, const UObject* InNewObject, FOnDiffEntryFocused InSelectionCallback, bool bPopulateOutTreeEntries);

	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;

	TSharedRef<SWidget> OldDetailsWidget() { return OldDetails.DetailsWidget(); }
	TSharedRef<SWidget> NewDetailsWidget() { return NewDetails.DetailsWidget(); }

protected:
	virtual void OnSelectDiffEntry(FPropertySoftPath PropertyName);
	TAttribute<TArray<FVector2f>> GetLinkedScrollRateAttribute(const TSharedRef<IDetailsView>& OldDetailsView, const TSharedRef<IDetailsView>& NewDetailsView);
	

	// helper function that analyzes two details views and determines the rate they should scroll relative to one another to be in sync
	TArray<FVector2f> GetLinkedScrollRate(TSharedRef<IDetailsView> OldDetailsView, TSharedRef<IDetailsView> NewDetailsView) const;

	FOnDiffEntryFocused SelectionCallback;
	FDetailsDiff OldDetails;
	FDetailsDiff NewDetails;

	TArray<FSingleObjectDiffEntry> DifferingProperties;
	TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> > Children;
	const bool bPopulateOutTreeEntries;

	mutable struct
	{
		TArray<TPair<int32, FPropertyPath>> OldProperties;
		TArray<TPair<int32, FPropertyPath>> NewProperties;
		TArray<FVector2f> ScrollRate;
	} LinkedScrollRateCache;
};

/** Override for CDO special case */
class FCDODiffControl : public FDetailsDiffControl
{
public:
	FCDODiffControl(const UObject* InOldObject, const UObject* InNewObject, FOnDiffEntryFocused InSelectionCallback);

	virtual void GenerateTreeEntries(TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutTreeEntries, TArray< TSharedPtr<FBlueprintDifferenceTreeEntry> >& OutRealDifferences) override;
};

/** Override for class class settings */
class FClassSettingsDiffControl : public FDetailsDiffControl
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