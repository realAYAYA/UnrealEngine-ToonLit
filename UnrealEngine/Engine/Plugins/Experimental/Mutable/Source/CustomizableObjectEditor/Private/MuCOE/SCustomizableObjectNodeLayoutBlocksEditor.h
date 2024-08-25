// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

namespace ESelectInfo { enum Type : int; }

class FReferenceCollector;
class ICustomizableObjectInstanceEditor;
class ISlateStyle;
class SWidget;
class STextComboBox;
class SVerticalBox;
class SHorizontalBox;
class FUICommandList;
class SCustomizableObjectLayoutGrid;

struct FCustomizableObjectLayoutBlock;
struct FGuid;


/**
 * CustomizableObject Editor Preview viewport widget
 */
class SCustomizableObjectNodeLayoutBlocksEditor : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SCustomizableObjectNodeLayoutBlocksEditor ){}
	SLATE_END_ARGS()

	SCustomizableObjectNodeLayoutBlocksEditor();
	virtual ~SCustomizableObjectNodeLayoutBlocksEditor();

	void Construct(const FArguments& InArgs);
	
	// FSerializableObject interface
	void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SCustomizableObjectNodeLayoutBlocksEditor");
	}
	// End of FSerializableObject interface


	/** Binds commands associated with the viewport client. */
	void BindCommands();

	/**  */
	void SetCurrentLayout( class UCustomizableObjectLayout* Layout );

private:

	/** */
	TObjectPtr<class UCustomizableObjectLayout> CurrentLayout;

	/** */
	TSharedPtr<SCustomizableObjectLayoutGrid> LayoutGridWidget;

	/** The list of UI Commands executable */
	TSharedRef<FUICommandList> UICommandList;

private:

	/** */
	FIntPoint GetGridSize() const;
	void OnBlockChanged(FGuid BlockId, FIntRect Block );
	TArray<FCustomizableObjectLayoutBlock> GetBlocks() const;

	/** Callbacks from the layout block editor. */
	TSharedRef<SWidget> BuildLayoutToolBar();

	void OnAddBlock();
	void OnAddBlockAt(const FIntPoint Min, const FIntPoint Max);
	void OnRemoveBlock();

	/** Generate layout blocks using UVs*/
	void OnGenerateBlocks();

	/** Sets the block priority from the input text. */
	void OnSetBlockPriority(int32 InValue);

	/** Sets the block reduction symmetry option. */
	void OnSetBlockReductionSymmetry(bool bInValue);

	/** Sets the block reduction ReduceByTwo option. */
	void OnSetBlockReductionByTwo(bool bInValue);

	TSharedPtr<IToolTip> GenerateInfoToolTip() const;

};
