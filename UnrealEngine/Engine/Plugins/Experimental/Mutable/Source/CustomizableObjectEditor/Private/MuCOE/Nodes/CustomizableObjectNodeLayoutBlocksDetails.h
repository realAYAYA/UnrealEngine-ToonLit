// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "IDetailCustomization.h"

enum class ECheckBoxState : uint8;
namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;
class STextBlock;
class SWidget;

class FCustomizableObjectNodeLayoutBlocksDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;


private:

	class UCustomizableObjectNodeLayoutBlocks* Node;

	// Layout block editor widget
	TSharedPtr<class SCustomizableObjectNodeLayoutBlocksEditor> LayoutBlocksEditor;

	// Widget to select at which LOD layout vertex warnings will start to be ignored
	TSharedPtr<SWidget> LODSelectorWidget;
	TSharedPtr<STextBlock> LODSelectorTextWidget;

	TArray< TSharedPtr<FString> > GridComboOptions;

	void OnGridComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnBlockChanged( int BlockIndex, FIntRect Block );
	void OnIgnoreErrorsCheckStateChanged(ECheckBoxState State);
	void OnLODBoxValueChanged(int32 Value);

	FIntPoint GetGridSize() const;
	TArray<FIntRect> GetBlocks() const;

};
