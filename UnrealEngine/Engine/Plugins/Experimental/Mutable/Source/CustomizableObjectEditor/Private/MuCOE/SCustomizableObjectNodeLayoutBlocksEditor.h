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

	TSharedPtr< SVerticalBox > LayoutGridSizeWidget;

	/** Widget for displaying the available layout block grid sizes. */
	TSharedPtr< STextComboBox > LayoutGridSizeCombo;
	TSharedPtr< STextComboBox > MaxLayoutGridSizeCombo;

	/** List of available layout grid sizes. */
	TArray< TSharedPtr< FString > > LayoutGridSizes;

	/** List of available layout max grid sizes. */
	TArray< TSharedPtr< FString > > MaxLayoutGridSizes;

	/** List of available layout packing strategies. */
	TArray< TSharedPtr< FString > > LayoutPackingStrategies;

	/** Widget to select the available layout packing strategies. */
	TSharedPtr< STextComboBox > LayoutPackingStrategyCombo;

	/** List of available block reduction methods. */
	TArray< TSharedPtr< FString > > BlockReductionMethods;

	/** Widget to select the available layout block reduction methods. */
	TSharedPtr< STextComboBox > BlockReductionMethodsCombo;

	/** Widget to select the layout packing strategy */
	TSharedPtr< SHorizontalBox> LayoutStrategyWidget;

	/** Widget to select the fixed layout properties */
	TSharedPtr< SVerticalBox> FixedLayoutWidget;

	TSharedPtr<SWidget> StrategyWidget;

	/** The list of UI Commands executable */
	TSharedRef<FUICommandList> UICommandList;

private:

	/** */
	FIntPoint GetGridSize() const;
	void OnBlockChanged(FGuid BlockId, FIntRect Block );
	TArray<FCustomizableObjectLayoutBlock> GetBlocks() const;

	/** Callbacks from the layout block editor. */
	TSharedRef<SWidget> BuildLayoutToolBar();
	TSharedRef<SWidget> BuildLayoutStrategyWidgets(const ISlateStyle* Style, const FName& StyleName);

	void OnAddBlock();
	void OnAddBlockAt(const FIntPoint Min, const FIntPoint Max);
	void OnRemoveBlock();

	/** Generate layout blocks using UVs*/
	void OnGenerateBlocks();
		
	/** . */
	void OnGridSizeChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo );
	void OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnReductionMethodChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	/** Sets the block priority from the input text. */
	void OnSetBlockPriority(int32 InValue);

	/** Sets the block reduction symmetry method. */
	void OnSetBlockReductionSymmetry(bool bInValue);

	/** Called when the packing strategy has changed. */
	void OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	TSharedPtr<IToolTip> GenerateInfoToolTip() const;

};
