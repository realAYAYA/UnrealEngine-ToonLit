// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Misc/Optional.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateConstants.h"
#include "Types/SlateEnums.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

// Forward declarations
class FMutableObjectTreeElement;
class FReferenceCollector;
class FTabManager;
class ITableRow;
class STextComboBox;
class SWidget;


/** This widget shows the internals of a CustomizableObject for debugging purposes. 
 * This is not the Unreal source graph in the UCustomizableObject, but an intermediate step of the compilation process.
 */
class SMutableObjectViewer :
	public SCompoundWidget,
	public FGCObject
{
public:

	// SWidget interface
	SLATE_BEGIN_ARGS(SMutableObjectViewer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UCustomizableObject*,
		TWeakPtr<FTabManager> InParentTabManager, const FName& InParentNewTabId );

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:

	/** The Mutable Graph to show, represented by its root. */
	UCustomizableObject* CustomizableObject;

	/** Compilation options to use in the debugger operations. */
	FCompilationOptions CompileOptions;
	TArray< TSharedPtr<FString> > DebugPlatformStrings;
	TSharedPtr<STextComboBox> DebugPlatformCombo;

	/** Object compiler. */
	FCustomizableObjectCompiler Compiler;

	/** UI references used to create new tabs. */
	TWeakPtr<FTabManager> ParentTabManager;
	FName ParentNewTabId;

	/** Tree showing the object properties. */
	TSharedPtr<STreeView<TSharedPtr<FMutableObjectTreeElement>>> TreeView;

	/** Root nodes of the tree widget. */
	TArray<TSharedPtr<FMutableObjectTreeElement>> RootTreeNodes;

	/** UI callbacks */
	void GenerateMutableGraphPressed();
	void CompileMutableCodePressed();
	TSharedRef<SWidget> GenerateCompileOptionsMenuContent();
	TSharedPtr<STextComboBox> CompileOptimizationCombo;
	TArray< TSharedPtr<FString> > CompileOptimizationStrings;
	void OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnChangeDebugPlatform(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Tree callbacks. */
	TSharedRef<ITableRow> GenerateRowForNodeTree(TSharedPtr<FMutableObjectTreeElement> InTreeNode, const TSharedRef<STableViewBase>& InOwnerTable);
	void GetChildrenForInfo(TSharedPtr<FMutableObjectTreeElement> InInfo, TArray< TSharedPtr<FMutableObjectTreeElement> >& OutChildren);

};


/** An row of the code tree in . */
class FMutableObjectTreeElement : public TSharedFromThis<FMutableObjectTreeElement>
{
public:

	enum class EType
	{
		None,
		SectionCaption,
		Name,
		ChildObject
	};

	enum class ESection
	{
		None,
		General,
		ChildObjects
	};

	/** Constructor for section rows */
	FMutableObjectTreeElement(ESection InSection, UCustomizableObject* InObject)
	{
		Type = EType::SectionCaption;
		Section = InSection;
		CustomizableObject = InObject;
	}

	/** Constructor for generic property rows. */
	FMutableObjectTreeElement(EType InType, ESection InSection, UCustomizableObject* InObject)
	{
		Type = InType;
		Section = InSection;
		CustomizableObject = InObject;
	}

public:

	/** Row type. */
	EType Type = EType::None;

	/** If the row is a section row. */
	ESection Section = ESection::None;

	/** */
	UCustomizableObject* CustomizableObject = nullptr;

};
