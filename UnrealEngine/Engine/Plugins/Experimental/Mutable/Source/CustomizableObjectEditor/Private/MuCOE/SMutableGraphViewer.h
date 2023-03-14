// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Input/Reply.h"
#include "Misc/Optional.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuT/Node.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Types/SlateConstants.h"
#include "Types/SlateEnums.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class FDragDropEvent;
class FReferenceCollector;
class FMutableGraphTreeElement;
class FTabManager;
class ITableRow;
class STextComboBox;
class SWidget;
struct FGeometry;


/** This widget shows the internal Mutable Graph for debugging purposes. 
 * This is not the Unreal source graph in the UCustomizableObject, but an intermediate step of the compilation process.
 */
class SMutableGraphViewer :
	public SCompoundWidget,
	public FGCObject
{
public:

	// SWidget interface
	SLATE_BEGIN_ARGS(SMutableGraphViewer) {}

	/** User-visible tag to indentify the source of the data shwon. */
	SLATE_ARGUMENT(FString, DataTag)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const mu::NodePtr& InRootNode, const FCompilationOptions& InCompileOptions,
		TWeakPtr<FTabManager> InParentTabManager, const FName& InParentNewTabId);

	// SWidget interface
	FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:

	/** Tag for the user to identify the data being shown. */
	FString DataTag;

	/** The Mutable Graph to show, represented by its root. */
	mu::NodePtr RootNode;

	/** Compilation options to use in the debugger operations. */
	FCompilationOptions CompileOptions;

	/** Object compiler. */
	FCustomizableObjectCompiler Compiler;

	/** UI references used to create new tabs. */
	TWeakPtr<FTabManager> ParentTabManager;
	FName ParentNewTabId;

	/** Root nodes of the tree widget. */
	TArray<TSharedPtr<FMutableGraphTreeElement>> RootNodes;

	/** Tree showing the graph. */
	TSharedPtr<STreeView<TSharedPtr<FMutableGraphTreeElement>>> TreeView;

	/** Cache of tree elements matching the graph nodes that have been generated so far. 
	* We store both the parent and the node in the key, because a single node may appear multiple times if it has different parents.
	*/
	struct FItemCacheKey
	{
		const mu::Node* Parent = nullptr;
		const mu::Node* Child = nullptr;
		uint32 ChildIndexInParent = 0;

		friend FORCEINLINE bool operator == (const FItemCacheKey& A, const FItemCacheKey& B)
		{
			return A.Parent == B.Parent && A.Child == B.Child && A.ChildIndexInParent == B.ChildIndexInParent;
		}

		friend FORCEINLINE uint32 GetTypeHash(const FItemCacheKey& Key)
		{
			return HashCombine( GetTypeHash(Key.Parent), HashCombine(GetTypeHash(Key.Child), Key.ChildIndexInParent));
		}
	};
	TMap< FItemCacheKey, TSharedPtr<FMutableGraphTreeElement>> ItemCache;

	/** Main tree item for each node. A graph node can be representes with multiple tree ndoes if it is reachable from different paths. */
	TMap< const mu::Node*, TSharedPtr<FMutableGraphTreeElement>> MainItemPerNode;

	/** */
	void RebuildTree();

	/** UI callbacks */
	void CompileMutableCodePressed();
	TSharedRef<SWidget> GenerateCompileOptionsMenuContent();
	TSharedPtr<STextComboBox> CompileOptimizationCombo;
	TArray< TSharedPtr<FString> > CompileOptimizationStrings;
	void OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Callbacks from the tree widget. */
 	TSharedRef<ITableRow> GenerateRowForNodeTree(TSharedPtr<FMutableGraphTreeElement> InTreeNode, const TSharedRef<STableViewBase>& InOwnerTable);
	void GetChildrenForInfo(TSharedPtr<FMutableGraphTreeElement> InInfo, TArray< TSharedPtr<FMutableGraphTreeElement> >& OutChildren);
	TSharedPtr<SWidget> OnTreeContextMenuOpening();
	void TreeExpandRecursive(TSharedPtr<FMutableGraphTreeElement> Info, bool bExpand);
	void TreeExpandUnique();

};


/** An row of the code tree in the SMutableGraphViewer. */
class FMutableGraphTreeElement : public TSharedFromThis<FMutableGraphTreeElement>
{
public:
	FMutableGraphTreeElement(const mu::NodePtr& InNode, TSharedPtr<FMutableGraphTreeElement>* InDuplicatedOf=nullptr )
	{
		MutableNode = InNode;
		if (InDuplicatedOf)
		{
			DuplicatedOf = *InDuplicatedOf;
		}
	}

public:

	/** Mutable Graph Node represented in this tree row. */
	mu::NodePtr MutableNode;

	/** If this tree element is a duplicated of another node, this is the node. */
	TSharedPtr<FMutableGraphTreeElement> DuplicatedOf;

};
