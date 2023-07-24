// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "IDetailCustomization.h"

namespace ESelectInfo { enum Type : int; }

class FString;
class IDetailLayoutBuilder;


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

	TArray< TSharedPtr<FString> > GridComboOptions;

	void OnGridComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnBlockChanged( int BlockIndex, FIntRect Block );

	FIntPoint GetGridSize() const;
	TArray<FIntRect> GetBlocks() const;

};
