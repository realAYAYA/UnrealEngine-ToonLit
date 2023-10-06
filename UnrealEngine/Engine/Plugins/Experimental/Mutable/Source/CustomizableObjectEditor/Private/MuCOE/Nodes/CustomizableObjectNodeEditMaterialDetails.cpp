// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterial.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksSelector.h"
#include "MuCOE/PinViewer/SPinViewer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

class FString;

#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeEditMaterialDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeEditMaterialDetails);
}


void FCustomizableObjectNodeEditMaterialDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	Node = nullptr;
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeEditMaterial>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "Blocks" );

	if (Node)
	{
		UCustomizableObjectNodeEditLayoutBlocks* NodeEditLayout = Cast<UCustomizableObjectNodeEditLayoutBlocks>(Node);
		if (NodeEditLayout)
		{
			// Add blocks selector
			LayoutBlocksSelector = SNew(SCustomizableObjectNodeLayoutBlocksSelector);

			BlocksCategory.AddCustomRow(LOCTEXT("BlocksDetails_BlockInstructions", "BlockInstructions"))
			[
				SNew(SBox)
				.HeightOverride(700.0f)
				.WidthOverride(700.0f)
				[
					LayoutBlocksSelector.ToSharedRef()
				]
			];

			LayoutBlocksSelector->SetSelectedNode(NodeEditLayout);
		}
	}
	else
	{
		BlocksCategory.AddCustomRow( LOCTEXT("BlocksDetails_NodeNotFound", "NodeNotFound") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "Node not found", "Node not found" ) )
		];
	}

	PinViewerAttachToDetailCustomization(DetailBuilder);
}

#undef LOCTEXT_NAMESPACE
