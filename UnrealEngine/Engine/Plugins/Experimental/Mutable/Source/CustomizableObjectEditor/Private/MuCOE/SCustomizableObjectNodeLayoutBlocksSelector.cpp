// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectNodeLayoutBlocksSelector.h"

#include "Delegates/Delegate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectInstanceEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditLayoutBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/SCustomizableObjectLayoutGrid.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class SWidget;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


/**
 * 
 */
class FLayoutBlockSelectorCommands : public TCommands<FLayoutBlockSelectorCommands>
{

public:
	FLayoutBlockSelectorCommands() : TCommands<FLayoutBlockSelectorCommands>
	(
		"LayoutBlockSelector", // Context name for fast lookup
		NSLOCTEXT( "CustomizableObjectEditor", "LayoutBlockSelector", "Layout Block Selector" ), // Localized context name for displaying
		NAME_None, // Parent
		FCustomizableObjectEditorStyle::GetStyleSetName()
	)
	{
	}	
	
	/**  */
	TSharedPtr< FUICommandInfo > SelectAll;
	TSharedPtr< FUICommandInfo > SelectNone;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override
	{
		UI_COMMAND( SelectAll, "Select All", "Select all the blocks in the layout.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( SelectNone, "Unselect All", "Unselect all the blocks in the layout.", EUserInterfaceActionType::Button, FInputChord() );
	}
};


void SCustomizableObjectNodeLayoutBlocksSelector::Construct(const FArguments& InArgs)
{
	CustomizableObjectEditorPtr = InArgs._CustomizableObjectEditor;
	CurrentNode = nullptr;
	
	BindCommands();
}


void SCustomizableObjectNodeLayoutBlocksSelector::SetSelectedNode(UCustomizableObjectNodeEditLayoutBlocks* Node )
{
	CurrentNode = Node;

	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = Node ? Node->GetParentMaterialNode() : nullptr;

	// Try to locate the source mesh
	TArray<FVector2f> UVs;
	if (CurrentNode && ParentMaterialNode)
	{
		const UEdGraphPin* SourceMeshPin = FindMeshBaseSource(*ParentMaterialNode->OutputPin(), false);
		if (SourceMeshPin)
		{
			const UCustomizableObjectNodeSkeletalMesh* MeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SourceMeshPin->GetOwningNode());
			if (MeshNode)
			{
				if (ParentMaterialNode->GetLayouts().Num() > 0 && ParentMaterialNode->GetLayouts().IsValidIndex(CurrentNode->ParentLayoutIndex))
				{
					MeshNode->GetUVChannelForPin(SourceMeshPin, UVs, CurrentNode->ParentLayoutIndex);
				}
			}

			const UCustomizableObjectNodeTable* TableNode = Cast<UCustomizableObjectNodeTable>(SourceMeshPin->GetOwningNode());
			if (TableNode)
			{
				if (ParentMaterialNode->GetLayouts().Num() > 0 && ParentMaterialNode->GetLayouts().IsValidIndex(CurrentNode->ParentLayoutIndex))
				{
					TableNode->GetUVChannelForPin(SourceMeshPin, UVs, CurrentNode->ParentLayoutIndex);
				}
			}
		}
	}

	this->ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
				.Padding(2.0f)
				.AutoHeight()
				[
					BuildLayoutToolBar()
				]

			+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SAssignNew(LayoutGridWidget, SCustomizableObjectLayoutGrid)
					.Mode(ELGM_Select)
					.GridSize(this, &SCustomizableObjectNodeLayoutBlocksSelector::GetGridSize)
					.Blocks(this, &SCustomizableObjectNodeLayoutBlocksSelector::GetBlocks)
					.UVLayout(UVs)
					.SelectionColor(FColor(178, 134, 73, 155))
					.OnSelectionChanged(this, &SCustomizableObjectNodeLayoutBlocksSelector::OnSelectionChanged)
				]
		];

	LayoutGridWidget->SetSelectedBlocks(CurrentNode->BlockIds);
	BlocksLabel->SetText(FText::FromString(FString::Printf(TEXT("%d blocks selected"), CurrentNode->BlockIds.Num())));
}


SCustomizableObjectNodeLayoutBlocksSelector::~SCustomizableObjectNodeLayoutBlocksSelector()
{
}


void SCustomizableObjectNodeLayoutBlocksSelector::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CurrentNode );
}


TSharedRef<SWidget> SCustomizableObjectNodeLayoutBlocksSelector::BuildLayoutToolBar()
{
	TSharedPtr<FUICommandList> CommandList = CustomizableObjectEditorPtr.Pin()->GetToolkitCommands();
	FToolBarBuilder LayoutToolbarBuilder(CommandList, FMultiBoxCustomization::None);

	// Build toolbar widgets

	LayoutToolbarBuilder.BeginSection("Blocks");
	{
		LayoutToolbarBuilder.AddToolBarButton(FLayoutBlockSelectorCommands::Get().SelectAll);
		LayoutToolbarBuilder.AddToolBarButton(FLayoutBlockSelectorCommands::Get().SelectNone);
	}
	LayoutToolbarBuilder.EndSection();
		
	BlocksLabel = SNew(STextBlock)
		.Text( LOCTEXT("0 blocks selected","0 blocks selected") );

	LayoutToolbarBuilder.BeginSection("Label");
	{
		LayoutToolbarBuilder.AddWidget
		(
			SNew(SBox)
			.WidthOverride(125.0f)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				BlocksLabel.ToSharedRef()
			]
		);
	}
	LayoutToolbarBuilder.EndSection();

	return
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.Padding(4,0)
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
			[
				LayoutToolbarBuilder.MakeWidget()
			]
		]
		;
}


void SCustomizableObjectNodeLayoutBlocksSelector::OnSelectAll()
{
	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = CurrentNode ? CurrentNode->GetParentMaterialNode() : nullptr;

	if ( CurrentNode && ParentMaterialNode)
	{
		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (Layouts.Num() && Layouts.IsValidIndex(CurrentNode->ParentLayoutIndex))
		{
			UCustomizableObjectLayout* Layout = Layouts[CurrentNode->ParentLayoutIndex];

			for( const FCustomizableObjectLayoutBlock& Block : Layout->Blocks )
			{
				CurrentNode->BlockIds.Add(Block.Id);
			}

			BlocksLabel->SetText(FText::FromString(FString::Printf(TEXT("%d blocks selected"), CurrentNode->BlockIds.Num())));

			LayoutGridWidget->SetSelectedBlocks( CurrentNode->BlockIds );
			CurrentNode->GetGraph()->MarkPackageDirty();
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksSelector::OnSelectNone()
{
	if ( CurrentNode )
	{
		CurrentNode->BlockIds.Reset();
		LayoutGridWidget->SetSelectedBlocks( CurrentNode->BlockIds );

		BlocksLabel->SetText(LOCTEXT("0 blocks selected", "0 blocks selected"));

		CurrentNode->GetGraph()->MarkPackageDirty();
	}
}


FIntPoint SCustomizableObjectNodeLayoutBlocksSelector::GetGridSize() const
{
	FIntPoint Result(1, 1);
	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = CurrentNode ? CurrentNode->GetParentMaterialNode() : nullptr;

	if ( CurrentNode && ParentMaterialNode)
	{
		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (Layouts.Num() && Layouts.IsValidIndex(CurrentNode->ParentLayoutIndex))
		{
			UCustomizableObjectLayout* Layout = Layouts[CurrentNode->ParentLayoutIndex];
			Result = Layout->GetGridSize();
		}
	}

	return Result;
}


void SCustomizableObjectNodeLayoutBlocksSelector::OnSelectionChanged( const TArray<FGuid>& selected )
{
	if ( CurrentNode )
	{
		CurrentNode->BlockIds = selected;

		BlocksLabel->SetText( FText::FromString( FString::Printf( TEXT("%d blocks selected"),selected.Num() ) ) );

		CurrentNode->GetGraph()->MarkPackageDirty();
	}
}


TArray<FCustomizableObjectLayoutBlock> SCustomizableObjectNodeLayoutBlocksSelector::GetBlocks() const
{
	TArray<FCustomizableObjectLayoutBlock> Blocks;

	UCustomizableObjectNodeMaterialBase* ParentMaterialNode = CurrentNode ? CurrentNode->GetParentMaterialNode() : nullptr;

	if (ParentMaterialNode)
	{
		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (Layouts.IsValidIndex(CurrentNode->ParentLayoutIndex))
		{
			UCustomizableObjectLayout* Layout = Layouts[CurrentNode->ParentLayoutIndex];
			Blocks = Layout->Blocks;
		}
	}

	return Blocks;
}


void SCustomizableObjectNodeLayoutBlocksSelector::BindCommands()
{
	// Register our commands. This will only register them if not previously registered
	FLayoutBlockSelectorCommands::Register();

	const FLayoutBlockSelectorCommands& Commands = FLayoutBlockSelectorCommands::Get();

	const TSharedRef<FUICommandList>& UICommandList = CustomizableObjectEditorPtr.Pin()->GetToolkitCommands();

	UICommandList->MapAction(
		Commands.SelectAll,
		FExecuteAction::CreateSP( this, &SCustomizableObjectNodeLayoutBlocksSelector::OnSelectAll ),
		FCanExecuteAction(),
		FIsActionChecked() );

	UICommandList->MapAction(
		Commands.SelectNone,
		FExecuteAction::CreateSP( this, &SCustomizableObjectNodeLayoutBlocksSelector::OnSelectNone ),
		FCanExecuteAction(),
		FIsActionChecked() );
}


#undef LOCTEXT_NAMESPACE

