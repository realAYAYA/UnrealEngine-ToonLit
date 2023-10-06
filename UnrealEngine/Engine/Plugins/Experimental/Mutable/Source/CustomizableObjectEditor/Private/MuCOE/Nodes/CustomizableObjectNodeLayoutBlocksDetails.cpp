// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocksDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

class FString;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeLayoutBlocksDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeLayoutBlocksDetails );
}


void FCustomizableObjectNodeLayoutBlocksDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	Node = nullptr;
	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeLayoutBlocks>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& CustomizableObjectCategory = DetailBuilder.EditCategory( "LayoutOptions" );
	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "LayoutEditor" );

	if (Node)
	{
		LayoutBlocksEditor = SNew(SCustomizableObjectNodeLayoutBlocksEditor);

		CustomizableObjectCategory.AddCustomRow(LOCTEXT("BlocksDetails_LayoutOptions", "LayoutOptions"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(0.0f,5.0f,0.0f,2.5f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.FillWidth(0.75f)
				[
					SNew(STextBlock)
					.Text( LOCTEXT("BlocksDetails_LayoutOptions_IgnoreLodsCheckBox", "Ignore Unassigned Vertices Warning:"))
					.ToolTipText(LOCTEXT("BlocksDetails_LayoutOptions_IgnoreLodsCheckBox_Tooltip",
						"If true, warning message \"Source mesh has vertices not assigned to any layout block\" will be ignored."
						"\n Note:"
						"\n This warning can appear when a CO has more than one LOD using the same Layout Block node and these LODs have been generated using the automatic LOD generation."
						"\n (At high LODs, some vertices may have been displaced from their original position which means they could have been displaced outside their layout blocks.)"
						"\n Ignoring these warnings can cause some visual artifacts that may or may not be visually important at higher LODs."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(0.25f)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					.IsChecked(Node->Layout ? (Node->Layout->GetIgnoreVertexLayoutWarnings() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked) : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnIgnoreErrorsCheckStateChanged)
				]
			]
			
			+ SVerticalBox::Slot()
			.Padding(0.0f, 2.5f, 0.0f, 5.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.FillWidth(0.75f)
				[
					SAssignNew(LODSelectorTextWidget, STextBlock)
					.Text( LOCTEXT("BlocksDetails_LayoutOptions_IgnoreLod", "First LOD to ignore:"))
					.ToolTipText(LOCTEXT("BlocksDetails_LayoutOptions_IgnoreLod_Tooltip", "LOD from which vertex warning messages will be ignored."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsEnabled(Node->Layout ? (Node->Layout->GetIgnoreVertexLayoutWarnings() ? true : false) : false)
				]
				
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(0.25f)
				.Padding(5.0f,0.0f,0.0f,0.0f)
				[
					SAssignNew(LODSelectorWidget, SSpinBox<int32>)
					.Value_Lambda([this]()->int32
					{
						if (Node->Layout)
						{
							return Node->Layout->GetFirstLODToIgnoreWarnings();
						}

						return 0;
					})
					.IsEnabled(Node->Layout ? (Node->Layout->GetIgnoreVertexLayoutWarnings() ? true : false ) : false)
					.OnValueChanged(this, &FCustomizableObjectNodeLayoutBlocksDetails::OnLODBoxValueChanged)
					.MinValue(0)
					.Delta(1)
					.AlwaysUsesDeltaSnap(true)
					.MinDesiredWidth(40.0f)
				]
			]
		];

		BlocksCategory.AddCustomRow( LOCTEXT("BlocksDetails_BlockInstructions", "BlockInstructions") )
		[
			SNew(SBox)
			.HeightOverride(700.0f)
			.WidthOverride(700.0f)
			[
				LayoutBlocksEditor.ToSharedRef()
			]
		];
		
		LayoutBlocksEditor->SetCurrentLayout(Node->Layout);
	}
	else
	{
		BlocksCategory.AddCustomRow( LOCTEXT("BlocksDetails_NodeNotFound", "NodeNotFound") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "Node not found", "Node not found" ) )
		];
	}

}


FIntPoint FCustomizableObjectNodeLayoutBlocksDetails::GetGridSize() const
{
	if (Node->Layout)
	{
		return Node->Layout->GetGridSize();
	}

	return FIntPoint(0);
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnBlockChanged( int BlockIndex, FIntRect Block )
{
	if (Node->Layout)
	{
		Node->Layout->Blocks[BlockIndex].Min = Block.Min;
		Node->Layout->Blocks[BlockIndex].Max = Block.Max;
		Node->PostEditChange();
	}
}


TArray<FIntRect> FCustomizableObjectNodeLayoutBlocksDetails::GetBlocks() const
{
	TArray<FIntRect> Blocks;

	if (Node->Layout)
	{
		Blocks.SetNum(Node->Layout->Blocks.Num());

		for (int BlockIndex = 0; BlockIndex < Node->Layout->Blocks.Num(); ++BlockIndex)
		{
			Blocks[BlockIndex] = FIntRect(Node->Layout->Blocks[BlockIndex].Min, Node->Layout->Blocks[BlockIndex].Max);
		}
	}

	return Blocks;
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnGridComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	/*
	if (CustomInstance)
	{
		mu::Model* Model = CustomInstance->CustomizableObject->GetModel();

		CustomInstance->PreEditChange(NULL);
		CustomInstance->State = Model->FindState( StringCast<ANSICHAR>(**Selection).Get() );
		CustomInstance->PostEditChange();

		ResetParamBox();
	}
	*/
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnIgnoreErrorsCheckStateChanged(ECheckBoxState State)
{
	if (Node->Layout)
	{
		bool bStateBool = State == ECheckBoxState::Checked;
		Node->Layout->SetIgnoreVertexLayoutWarnings(bStateBool);

		LODSelectorWidget->SetEnabled(bStateBool);
		LODSelectorTextWidget->SetEnabled(bStateBool);
	}
}


void FCustomizableObjectNodeLayoutBlocksDetails::OnLODBoxValueChanged(int32 Value)
{
	if (Node->Layout)
	{
		Node->Layout->SetIgnoreWarningsLOD(Value);
	}
}


#undef LOCTEXT_NAMESPACE
