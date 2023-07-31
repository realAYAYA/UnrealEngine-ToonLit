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
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class FReferenceCollector;
class IDetailLayoutBuilder;
class ITableRow;
class STableViewBase;
class UCustomizableObjectNode;
class UEdGraphPin;
struct EVisibility;
struct FEdGraphPinReference;
struct FGuid;

/**
 * Pin Viewer widget. Shows a list of pins a node contains.
 * In addition, a pin can provide a custom details widget which will be shown as an expandable menu.
 * 
 * This widget will work as long as pins are only created and destroyed during the node reconstruction.
 */
class SPinViewer : public SCompoundWidget, public FGCObject
{
	friend class SPinViewerListRow;

public:
	SLATE_BEGIN_ARGS(SPinViewer) {}
	SLATE_ARGUMENT(UCustomizableObjectNode*, Node)
	SLATE_END_ARGS()

	static const FName COLUMN_NAME;
	static const FName COLUMN_TYPE;
	static const FName COLUMN_VISIBILITY;
	
	void Construct(const FArguments& InArgs);

	// FGCObject interface
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SCustomizableObjectNodePinViewer");
	}

	/** Regenerate the list contents and update the widget. */
	void UpdateWidget();

	/** Callback to Generate a Nodfe Material Pin row */
	TSharedRef<ITableRow> GenerateNodePinRow(TSharedPtr<FEdGraphPinReference> PinReference, const TSharedRef<STableViewBase>& OwnerTable);

	/** SearchBox callback. */
	void OnFilterTextChanged(const FText& SearchText);

	/** Buttons callbacks. */
	FReply OnShowAllPressed() const;
	FReply OnHideAllPressed() const;

	// Columns sorting methods
	/** Sets the sorting method to be applied */
	void SortListView(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode);

	/** Get the pin name which is actually displayed. */
	static FText GetPinName(const UEdGraphPin& Pin);

private:
	/** Regenerate the list contents. */
	void GeneratePinInfoList();

	void PinsRemapped(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap);

	/** Pointer to the Node Material */
	UCustomizableObjectNode* Node = nullptr;

	/** ListView elements. */
	TArray<TSharedPtr<FEdGraphPinReference>> PinReferences;

	/** Data structure used to save SPinViewerMultiColumnTableRow additional widget visibility state between reconstructs.
	 * Key is a pin id. */
	TMap<FGuid, EVisibility> AdditionalWidgetVisibility;
	
	/** Current column. */
	FName CurrentSortColumn = COLUMN_TYPE;

	FString CurrentFilter;

	/** Widget List of the Node Material Pins */
	TSharedPtr<SListView<TSharedPtr<FEdGraphPinReference>>> ListView;

	/** Scrollbox widget needed to fix some scrolling problems */
	TSharedPtr<class SScrollBox> Scrollbox;
};


/** Create and attach a PinViewer Widget to the given custom details DetailBuilder.
 * The pin viewer will be shown as a new category with the lowest priority. */
void PinViewerAttachToDetailCustomization(IDetailLayoutBuilder& DetailBuilder);

