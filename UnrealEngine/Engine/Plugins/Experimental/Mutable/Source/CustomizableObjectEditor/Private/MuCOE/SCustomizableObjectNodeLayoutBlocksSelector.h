// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

class FReferenceCollector;
class FUICommandList;
class ICustomizableObjectInstanceEditor;
class SWidget;
struct FCustomizableObjectLayoutBlock;
struct FGuid;


/**
 * CustomizableObject Editor Preview viewport widget
 */
class SCustomizableObjectNodeLayoutBlocksSelector : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SCustomizableObjectNodeLayoutBlocksSelector ){}
	SLATE_END_ARGS()

	SCustomizableObjectNodeLayoutBlocksSelector();
	virtual ~SCustomizableObjectNodeLayoutBlocksSelector();

	void Construct(const FArguments& InArgs);
	
	// FSerializableObject interface
	void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SCustomizableObjectNodeLayoutBlocksSelector");
	}
	// End of FSerializableObject interface


	/** Binds commands associated with the viewport client. */
	void BindCommands();

	// These two types of nodes can use this widget
	void SetSelectedNode(class UCustomizableObjectNodeEditLayoutBlocks* Node);

private:

	// 
	TObjectPtr<class UCustomizableObjectNodeEditLayoutBlocks> CurrentNode = nullptr;

	/** */
	TSharedPtr<class SCustomizableObjectLayoutGrid> LayoutGridWidget;
	
	/** */
	TSharedPtr<class STextBlock> BlocksLabel;

	/** The list of UI Commands executable */
	TSharedRef<FUICommandList> UICommandList;

	/** */
	FIntPoint GetGridSize() const;
	void OnSelectionChanged( const TArray<FGuid>& selected );
	TArray<FCustomizableObjectLayoutBlock> GetBlocks() const;

	/** Callbacks from the layout block editor. */
	TSharedRef<SWidget> BuildLayoutToolBar();
	void OnSelectAll();
	void OnSelectNone();		
};
