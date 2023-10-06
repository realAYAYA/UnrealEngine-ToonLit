// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "IDetailCustomization.h"

class FString;
class IDetailLayoutBuilder;
class UCustomizableObjectNodeRemoveMeshBlocks;
class SCustomizableObjectNodeLayoutBlocksSelector;


class FCustomizableObjectNodeRemoveMeshBlocksDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** ILayoutDetails interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;


private:

	UCustomizableObjectNodeRemoveMeshBlocks* Node;

	// Layout block editor widget
	TSharedPtr<SCustomizableObjectNodeLayoutBlocksSelector> LayoutBlocksSelector;

};
