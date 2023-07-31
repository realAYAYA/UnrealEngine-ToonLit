// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Math/UnrealMathSSE.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FReferenceCollector;
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
		SLATE_ARGUMENT(TWeakPtr<ICustomizableObjectInstanceEditor>, CustomizableObjectEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SCustomizableObjectNodeLayoutBlocksSelector();
	
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

	/** Pointer back to the editor tool that owns us */
	TWeakPtr<ICustomizableObjectInstanceEditor> CustomizableObjectEditorPtr;

	// 
	class UCustomizableObjectNodeEditLayoutBlocks* CurrentNode = nullptr;

	/** */
	TSharedPtr<class SCustomizableObjectLayoutGrid> LayoutGridWidget;
	
	/** */
	TSharedPtr<class STextBlock> BlocksLabel;

	/** */
	FIntPoint GetGridSize() const;
	void OnSelectionChanged( const TArray<FGuid>& selected );
	TArray<FCustomizableObjectLayoutBlock> GetBlocks() const;

	/** Callbacks from the layout block editor. */
	TSharedRef<SWidget> BuildLayoutToolBar();
	void OnSelectAll();
	void OnSelectNone();		
};
