// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "MuR/Mesh.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateConstants.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class ITableRow;

/** Container with the information required to generate the tree of bones */
struct FBoneDefinition
{
	FText ParentBoneName = FText(INVTEXT(""));
	int32 ParentBoneIndex = -1;

	FText BoneName = FText(INVTEXT(""));
	int32 BoneIndex = -1;
};

/** Object responsible of showing with a tree view slate the bones present on the provided mutable skeleton */
class SMutableSkeletonViewer : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMutableSkeletonViewer) {}
		SLATE_ARGUMENT_DEFAULT(mu::SkeletonPtrConst, Skeleton) { nullptr };
	SLATE_END_ARGS()

public:
	/** Builds the widget */
	void Construct(const FArguments& InArgs);

	/** Sets the mutable skeleton object to be inspected */
	void SetSkeleton(mu::SkeletonPtrConst InSkeleton);
	
private:

	/** The mutable skeleton whose data is being inspected */
	mu::SkeletonPtrConst Skeleton = nullptr;
	
	/** Bones data: Bone definitions used to generate the bone tree structure */
	TArray<TSharedPtr<FBoneDefinition>> RootBones;
	TArray<TSharedPtr< FBoneDefinition>> ChildBones;

	/** UI Tree showing the bone data */
	TSharedPtr < STreeView<TSharedPtr<FBoneDefinition>>> BoneTree;

	/** Fills the bone TArrays that hold the root and child bone definitions */
	void FillBoneDefinitionArrays();
	
	/** 
	* Method responsible of generating each row for the tree of bones 
	* @param InBone - The mesh bone definition that is being used as blueprint for the actual UI row
	* @param OwnerTable - The parent UI element that is going to be expanded with the new row object
	* @return The new row object as the base interface used for all the table rows
	*/
	TSharedRef<ITableRow> OnGenerateBoneTreeRow(TSharedPtr<FBoneDefinition> InBone, const TSharedRef<STableViewBase>& OwnerTable) const;
	
	/** 
	* Fills the parameter OutChildrenBones with all the child bones from a selected Bone
	* @param InBone - The mesh bone definition that is being used as blueprint for the actual UI row
	* @param OutChildrenBones - An array that end up holding all the bone definitions that are children of the InBone. It can end up
	* being empty if no children bones are found for the InBone provided.
	*/
	void OnGetChildrenFromBoneDefinition(TSharedPtr<FBoneDefinition> InBone, TArray<TSharedPtr<FBoneDefinition>>& OutChildrenBones);
	
	/** Sets all rows of the tree as expanded */
	void ExpandFullBoneTree();
	
};