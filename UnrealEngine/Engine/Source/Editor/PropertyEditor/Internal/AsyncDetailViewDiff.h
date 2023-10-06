// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "AsyncTreeDifferences.h"
#include "DiffUtils.h"
#include "IDetailsView.h"


namespace TreeDiffSpecification
{
	template<>
	bool PROPERTYEDITOR_API AreValuesEqual(const TWeakPtr<FDetailTreeNode>& TreeNodeA, const TWeakPtr<FDetailTreeNode>& TreeNodeB);
	
	template<>
	bool PROPERTYEDITOR_API AreMatching(const TWeakPtr<FDetailTreeNode>& TreeNodeA, const TWeakPtr<FDetailTreeNode>& TreeNodeB);
	
	template<>
	void PROPERTYEDITOR_API GetChildren(const TWeakPtr<FDetailTreeNode>& InParent, TArray<TWeakPtr<FDetailTreeNode>>& OutChildren);

	template<>
	bool PROPERTYEDITOR_API ShouldMatchByValue(const TWeakPtr<FDetailTreeNode>& TreeNodeA);
};

// asynchronously update a difference tree with changes to details views
// note: users are expected to call TAsyncTreeDifferences::Tick or TAsyncTreeDifferences::FlushQueue to get accurate data
class PROPERTYEDITOR_API FAsyncDetailViewDiff : public TAsyncTreeDifferences<TWeakPtr<FDetailTreeNode>>
{
public:
	FAsyncDetailViewDiff(TSharedRef<IDetailsView> LeftView, TSharedRef<IDetailsView> RightView);

	// Note: by default this list is not necessarily accurate or exhaustive.
	// if you need a perfectly accurate list, call FlushQueue() first
	void GetPropertyDifferences(TArray<FSingleObjectDiffEntry>& OutDiffEntries) const;

	// execute a method for each node associated with a row in at least one details view
	// method provides current left and right row number
	// returns the number of rows in each details view
	TPair<int32, int32> ForEachRow(const TFunction<ETreeTraverseControl(const TUniquePtr<DiffNodeType>&, int32, int32)>& Method) const;

private:
	static TAttribute<TArray<TWeakPtr<FDetailTreeNode>>> RootNodesAttribute(TWeakPtr<IDetailsView> DetailsView);
	TWeakPtr<IDetailsView> LeftView;
	TWeakPtr<IDetailsView> RightView;
};
