// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

enum class ECheckBoxState : uint8;
namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;
class STextBlock;
class SWidget;
class UCustomizableObjectNodeLayoutBlocks;
struct EVisibility;

class FCustomizableObjectNodeLayoutBlocksDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;


private:

	/** Returns the visibility of the Fixed layout widgets */
	EVisibility FixedStrategyOptionsVisibility() const;

	/** Fills the combo box arrays sources */
	void FillComboBoxOptionsArrays(TSharedPtr<FString>& CurrGridSize, TSharedPtr<FString>& CurrStrategy, TSharedPtr<FString>& CurrMaxSize, TSharedPtr<FString>& CurrRedMethod);

	/** Layout Options Callbacks */
	void OnGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnReductionMethodChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnIgnoreErrorsCheckStateChanged(ECheckBoxState State);
	void OnLODBoxValueChanged(int32 Value);

private:

	/** Weak pointer to the node */
	TWeakObjectPtr<UCustomizableObjectNodeLayoutBlocks> Node;

	// Layout block editor widget
	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;

	// Widget to select at which LOD layout vertex warnings will start to be ignored
	TSharedPtr<SWidget> LODSelectorWidget;
	TSharedPtr<STextBlock> LODSelectorTextWidget;

	/** List of available layout grid sizes. */
	TArray< TSharedPtr< FString > > LayoutGridSizes;

	/** List of available layout packing strategies. */
	TArray< TSharedPtr< FString > > LayoutPackingStrategies;

	/** List of available block reduction methods. */
	TArray< TSharedPtr< FString > > BlockReductionMethods;

};
