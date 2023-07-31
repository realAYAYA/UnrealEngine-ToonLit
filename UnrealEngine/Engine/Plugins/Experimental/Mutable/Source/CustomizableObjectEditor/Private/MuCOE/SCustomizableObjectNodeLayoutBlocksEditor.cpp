// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectNodeLayoutBlocksEditor.h"

#include "Delegates/Delegate.h"
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
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/IntRect.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/ICustomizableObjectInstanceEditor.h"
#include "MuCOE/SCustomizableObjectLayoutGrid.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SToolTip.h"

class ISlateStyle;
class SWidget;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

/**
 * 
 */
class FLayoutEditorCommands : public TCommands<FLayoutEditorCommands>
{

public:
	FLayoutEditorCommands() : TCommands<FLayoutEditorCommands>
	(
		"LayoutEditorCommands", // Context name for fast lookup
		NSLOCTEXT( "CustomizableObjectEditor", "LayoutEditorCommands", "Layout Editor" ), // Localized context name for displaying
		NAME_None, // Parent
		FCustomizableObjectEditorStyle::GetStyleSetName()
	)
	{
	}	
	
	/**  */
	TSharedPtr< FUICommandInfo > AddBlock;
	TSharedPtr< FUICommandInfo > RemoveBlock;
	TSharedPtr< FUICommandInfo > GenerateBlocks;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override
	{
		UI_COMMAND( AddBlock, "Add Block", "Add a new block to the layout.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( RemoveBlock, "Remove Block", "Remove a block from the layout.", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( GenerateBlocks, "Generate Blocks", "Generate Blocks automatically from UVs", EUserInterfaceActionType::Button, FInputChord() );
	}
};


SCustomizableObjectNodeLayoutBlocksEditor::SCustomizableObjectNodeLayoutBlocksEditor() : UICommandList(new FUICommandList())
{
}


void SCustomizableObjectNodeLayoutBlocksEditor::Construct(const FArguments& InArgs)
{
	CurrentLayout = 0;
	
	BindCommands();
}


void SCustomizableObjectNodeLayoutBlocksEditor::SetCurrentLayout(UCustomizableObjectLayout* Layout )
{
	CurrentLayout = Layout;

	// Try to locate the source mesh
	TArray<FVector2f> UVs;
	TArray<FVector2f> UnassignedUVs;

	if (CurrentLayout)
	{
		CurrentLayout->GetUVChannel(UVs);

		UnassignedUVs = TArray<FVector2f>();
		
		if (CurrentLayout->UnassignedUVs.Num())
		{
			UnassignedUVs = CurrentLayout->UnassignedUVs[0];
		}
	}

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(0.0f,2.0f,0.0f,0.0f )
		.AutoHeight()
		[
			BuildLayoutToolBar()
		]

		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.AutoHeight()
		.Padding(3.0f,5.0f,0.0f,5.0f)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush(TEXT("Icons.Info")))
			.ToolTip(GenerateInfoToolTip())
		]

		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SAssignNew(LayoutGridWidget, SCustomizableObjectLayoutGrid)
			.Mode(ELGM_Edit)
			.GridSize(this, &SCustomizableObjectNodeLayoutBlocksEditor::GetGridSize)
			.Blocks(this, &SCustomizableObjectNodeLayoutBlocksEditor::GetBlocks)
			.UVLayout(UVs)
			.UnassignedUVLayoutVertices(UnassignedUVs)
			.SelectionColor(FColor(75, 106, 230, 155))
			.OnBlockChanged(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnBlockChanged)
			.OnDeleteBlocks(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnRemoveBlock)
			.OnAddBlockAt(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlockAt)
			.OnSetBlockPriority(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockPriority)
		]
	];

	if (CurrentLayout && LayoutGridWidget.IsValid())
	{
		LayoutGridWidget->SetLayoutStrategy(CurrentLayout->GetPackingStrategy());
	}	
}


SCustomizableObjectNodeLayoutBlocksEditor::~SCustomizableObjectNodeLayoutBlocksEditor()
{
}


void SCustomizableObjectNodeLayoutBlocksEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CurrentLayout );
}


TSharedRef<SWidget> SCustomizableObjectNodeLayoutBlocksEditor::BuildLayoutToolBar()
{
	FToolBarBuilder LayoutToolbarBuilder(UICommandList, FMultiBoxCustomization::None);

	//Getting toolbar style
	const ISlateStyle* const StyleSet = &FCoreStyle::Get();
	const FName& StyleName = "ToolBar";
	
	// Build toolbar widgets
	LayoutGridSizes.Empty();
	int32 MaxGridSize = 32;
	for(int32 Size = 1; Size <= MaxGridSize; Size*=2 )
	{
		LayoutGridSizes.Add( MakeShareable( new FString( FString::Printf(TEXT("%d x %d"), Size, Size ) ) ) );
	}
	
	LayoutGridSizeWidget = SNew(SVerticalBox);

	LayoutGridSizeWidget->AddSlot()
	.AutoHeight()
	.Padding(0.0f,0.0f,0.0f,10.0f)
	[
		SAssignNew(LayoutGridSizeCombo, STextComboBox)
		.OptionsSource(&LayoutGridSizes)
		.OnSelectionChanged(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnGridSizeChanged)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
	];

	LayoutGridSizeWidget->AddSlot()
	.AutoHeight()
	.HAlign(EHorizontalAlignment::HAlign_Center)
	.VAlign(EVerticalAlignment::VAlign_Bottom)
	[
		SNew(STextBlock).Text(LOCTEXT("LayoutGridSizeText", "Grid Size"))
		.ShadowOffset(FVector2D::UnitVector)
		.ColorAndOpacity(FLinearColor::Gray)
	];

	if (CurrentLayout)
	{
		int grid = CurrentLayout->GetGridSize().X;
		int option = -1;
		while ( grid )
		{
			option++;
			grid >>= 1;
		}

		if ( LayoutGridSizes.IsValidIndex(option) )
		{
			LayoutGridSizeCombo->SetSelectedItem( LayoutGridSizes[option] );
		}
	}

	LayoutToolbarBuilder.BeginSection("Blocks");
	{
		LayoutToolbarBuilder.AddToolBarButton(FLayoutEditorCommands::Get().AddBlock);
		LayoutToolbarBuilder.AddToolBarButton(FLayoutEditorCommands::Get().RemoveBlock);
		LayoutToolbarBuilder.AddToolBarButton(FLayoutEditorCommands::Get().GenerateBlocks);
	}
	LayoutToolbarBuilder.EndSection();

	LayoutToolbarBuilder.BeginSection("Grid");
	{		
		//LayoutToolbarBuilder.AddComboButton(FUIAction(),FOnGetContent::CreateSP(
		//		this,&SCustomizableObjectNodeLayoutBlocksEditor::GenerateLayoutGridOptionsMenuContent),
		//	TAttribute<FText>(),TAttribute<FText>(),FSlateIcon());

		LayoutToolbarBuilder.AddWidget(LayoutGridSizeWidget.ToSharedRef());
	}
	LayoutToolbarBuilder.EndSection();

	
	StrategyWidget = BuildLayoutStrategyWidgets(StyleSet, StyleName);
	
	LayoutToolbarBuilder.BeginSection("Packing Strategy");
	{
		LayoutToolbarBuilder.AddWidget(StrategyWidget.ToSharedRef());
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


TSharedRef<SWidget> SCustomizableObjectNodeLayoutBlocksEditor::BuildLayoutStrategyWidgets(const ISlateStyle* Style, const FName& StyleName)
{
	MaxLayoutGridSizes.Empty();
	int32 MaxGridSize = 32;
	for (int32 Size = 1; Size <= MaxGridSize; Size *= 2)
	{
		MaxLayoutGridSizes.Add(MakeShareable(new FString(FString::Printf(TEXT("%d x %d"), Size, Size))));
	}

	MaxLayoutGridSizeCombo = SNew(STextComboBox)
		.OptionsSource(&MaxLayoutGridSizes)
		.OnSelectionChanged(this,&SCustomizableObjectNodeLayoutBlocksEditor::OnMaxGridSizeChanged)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());

	LayoutPackingStrategies.Empty();
	LayoutPackingStrategies.Add(MakeShareable(new FString("Resizable")));
	LayoutPackingStrategies.Add(MakeShareable(new FString("Fixed")));

	LayoutPackingStrategyCombo = SNew(STextComboBox)
		.OptionsSource(&LayoutPackingStrategies)
		.OnSelectionChanged(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnLayoutPackingStrategyChanged)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());


	LayoutStrategyWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(2.0f).AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Layout Strategy:", "Layout Strategy:"))
			.ToolTipText(LOCTEXT("LayoutStrategyTooltup","Selects the packing strategy: Resizable Layout or Fixed Layout"))
			.Margin(FMargin(0, 4, 0, 0))
			.ShadowOffset(FVector2D::UnitVector)
			.ColorAndOpacity(FLinearColor::Gray)
		]

		+ SHorizontalBox::Slot().Padding(2.0f).AutoWidth()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				LayoutPackingStrategyCombo.ToSharedRef()
			]
		];

	FixedLayoutWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(2.0f).AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Max Layout Size:", "Max Layout Size:"))
			.Margin(FMargin(0, 4, 0, 0))
			.ShadowOffset(FVector2D::UnitVector)
			.ColorAndOpacity(FLinearColor::Gray)
		]
		+ SHorizontalBox::Slot().Padding(2.0f).AutoWidth()
		[
			MaxLayoutGridSizeCombo.ToSharedRef()
		];

	//Modify after the creation of all the widgets
	if (CurrentLayout)
	{
		int grid = CurrentLayout->GetMaxGridSize().X;
		int option = -1;
		while (grid)
		{
			option++;
			grid >>= 1;
		}

		if (MaxLayoutGridSizes.IsValidIndex(option))
		{
			MaxLayoutGridSizeCombo->SetSelectedItem(MaxLayoutGridSizes[option]);
		}

		if (LayoutPackingStrategies.IsValidIndex((uint32)CurrentLayout->GetPackingStrategy()))
		{
			LayoutPackingStrategyCombo->SetSelectedItem(LayoutPackingStrategies[(uint32)CurrentLayout->GetPackingStrategy()]);
		}
	}

	return SNew(SVerticalBox)
		//.IsEnabled(CurrentLayout ? !CurrentLayout->GetNode()->GetGraphEditor()->GetCustomizableObject()->bDisableTextureLayoutManagement : false)
		+ SVerticalBox::Slot().VAlign(EVerticalAlignment::VAlign_Center)
		[
			LayoutStrategyWidget.ToSharedRef()
		]
		+SVerticalBox::Slot()
		[
			FixedLayoutWidget.ToSharedRef()
		]
	;
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnGridSizeChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo )
{
	if (CurrentLayout)
	{
		int Size = 1 << LayoutGridSizes.Find(NewSelection);

		if (CurrentLayout->GetGridSize().X!=Size || CurrentLayout->GetGridSize().Y!=Size )
		{
			CurrentLayout->SetGridSize(FIntPoint(Size));

			// Adjust all the blocks sizes
			for ( int b=0; b< CurrentLayout->Blocks.Num(); ++b )
			{
				CurrentLayout->Blocks[b].Min.X = FMath::Min( CurrentLayout->Blocks[b].Min.X, Size-1 );
				CurrentLayout->Blocks[b].Min.Y = FMath::Min( CurrentLayout->Blocks[b].Min.Y, Size-1 );
				CurrentLayout->Blocks[b].Max.X = FMath::Min( CurrentLayout->Blocks[b].Max.X, Size );
				CurrentLayout->Blocks[b].Max.Y = FMath::Min( CurrentLayout->Blocks[b].Max.Y, Size );
			}

			CurrentLayout->MarkPackageDirty();
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlock()
{
	if (CurrentLayout)
	{
		FCustomizableObjectLayoutBlock block;
		block.Min = FIntPoint( 0, 0 );
		block.Max = FIntPoint(1, 1);
		block.Id = FGuid::NewGuid();
		block.Priority = 0;
		CurrentLayout->Blocks.Add( block );
		CurrentLayout->MarkPackageDirty();

		if (LayoutGridWidget.IsValid())
		{
			LayoutGridWidget->SetSelectedBlock(block.Id);
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlockAt(const FIntPoint Min, const FIntPoint Max)
{
	if (CurrentLayout)
	{
		FCustomizableObjectLayoutBlock block;
		block.Min = Min;
		block.Max = Max;
		block.Id = FGuid::NewGuid();
		block.Priority = 0;

		CurrentLayout->Blocks.Add(block);
		CurrentLayout->MarkPackageDirty();
	}
}



void SCustomizableObjectNodeLayoutBlocksEditor::OnRemoveBlock()
{
	if (CurrentLayout)
	{
		if (LayoutGridWidget.IsValid())
		{
			bool Change = false;

			TArray<FGuid> selected = LayoutGridWidget->GetSelectedBlocks();
			for (int i=0; i<CurrentLayout->Blocks.Num();)
			{
				if (selected.Contains(CurrentLayout->Blocks[i].Id))
				{
					Change = true;
					CurrentLayout->Blocks.RemoveAt(i);
				}
				else
				{
					++i;
				}
			}

			if (Change)
			{
				CurrentLayout->MarkPackageDirty();
			}
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnGenerateBlocks()
{
	if (CurrentLayout)
	{
		CurrentLayout->GenerateBlocksFromUVs();
	}
}


FIntPoint SCustomizableObjectNodeLayoutBlocksEditor::GetGridSize() const
{
	if ( CurrentLayout )
	{
		return CurrentLayout->GetGridSize();
	}
	return FIntPoint(1);
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnBlockChanged( FGuid BlockId, FIntRect Block )
{
	if (CurrentLayout)
	{
		for (FCustomizableObjectLayoutBlock& B : CurrentLayout->Blocks)
		{
			if (B.Id == BlockId)
			{
				B.Min = Block.Min;
				B.Max = Block.Max;
			}
		}
	}
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnSetBlockPriority(int32 InValue)
{
	if (CurrentLayout)
	{
		if (LayoutGridWidget.IsValid())
		{
			TArray<FGuid> SelectedBlocks = LayoutGridWidget->GetSelectedBlocks();

			for (int i = 0; i < CurrentLayout->Blocks.Num(); ++i)
			{
				if (SelectedBlocks.Contains(CurrentLayout->Blocks[i].Id))
				{
					CurrentLayout->Blocks[i].Priority = InValue;
					CurrentLayout->MarkPackageDirty();
				}
			}
		}
	}	
}


void SCustomizableObjectNodeLayoutBlocksEditor::OnLayoutPackingStrategyChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (CurrentLayout && FixedLayoutWidget)
	{
		uint32 selection = LayoutPackingStrategies.IndexOfByKey(NewSelection);

		if (!selection)
		{
			FixedLayoutWidget->SetVisibility(EVisibility::Collapsed);
		}
		else
		{
			FixedLayoutWidget->SetVisibility(EVisibility::Visible);
		}

		if (CurrentLayout->GetPackingStrategy() != (ECustomizableObjectTextureLayoutPackingStrategy)selection)
		{
			CurrentLayout->SetPackingStrategy((ECustomizableObjectTextureLayoutPackingStrategy)selection);

			if (LayoutGridWidget.IsValid())
			{
				LayoutGridWidget->SetLayoutStrategy(CurrentLayout->GetPackingStrategy());
			}

			CurrentLayout->MarkPackageDirty();
		}
	}
}

void SCustomizableObjectNodeLayoutBlocksEditor::OnMaxGridSizeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (CurrentLayout)
	{
		int Size = 1 << MaxLayoutGridSizes.Find(NewSelection);

		if (CurrentLayout->GetMaxGridSize().X != Size || CurrentLayout->GetMaxGridSize().Y != Size)
		{
			CurrentLayout->SetMaxGridSize(FIntPoint(Size));
			
			CurrentLayout->MarkPackageDirty();
		}
	}
}


TArray<FCustomizableObjectLayoutBlock> SCustomizableObjectNodeLayoutBlocksEditor::GetBlocks() const
{
	TArray<FCustomizableObjectLayoutBlock> Blocks;

	if (CurrentLayout)
	{
		Blocks = CurrentLayout->Blocks;
	}

	return Blocks;
}


void SCustomizableObjectNodeLayoutBlocksEditor::BindCommands()
{
	// Register our commands. This will only register them if not previously registered
	FLayoutEditorCommands::Register();

	const FLayoutEditorCommands& Commands = FLayoutEditorCommands::Get();

	UICommandList->MapAction(
		Commands.AddBlock,
		FExecuteAction::CreateSP( this, &SCustomizableObjectNodeLayoutBlocksEditor::OnAddBlock ),
		FCanExecuteAction(),
		FIsActionChecked() );

	UICommandList->MapAction(
		Commands.RemoveBlock,
		FExecuteAction::CreateSP( this, &SCustomizableObjectNodeLayoutBlocksEditor::OnRemoveBlock ),
		FCanExecuteAction(),
		FIsActionChecked() );

	UICommandList->MapAction(
		Commands.GenerateBlocks,
		FExecuteAction::CreateSP(this, &SCustomizableObjectNodeLayoutBlocksEditor::OnGenerateBlocks),
		FCanExecuteAction(),
		FIsActionChecked());
}


TSharedPtr<IToolTip> SCustomizableObjectNodeLayoutBlocksEditor::GenerateInfoToolTip() const
{
	TSharedPtr<SGridPanel> ToolTipWidget = SNew(SGridPanel);
	int32 SlotCount = 0;

	auto BuildShortcutAndTooltip = [ToolTipWidget, &SlotCount](const FText& Shortcut, const FText& Tooltip)
	{
		// Command Shortcut
		ToolTipWidget->AddSlot(0, SlotCount)
		[
			SNew(STextBlock)
			.Text(Shortcut)
		];

		// Command Explanation
		ToolTipWidget->AddSlot(1, SlotCount)
		.Padding(15.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(Tooltip)
		];

		++SlotCount;
	};

	// Duplicate command
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_DuplicateBlocks", "CTRL + D"), LOCTEXT("Tooltip_DuplicateBlocks", "Duplicate selected block/s"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_CreateNewBlock", "CTRL + N"), LOCTEXT("Tooltip_CreateNewBlock", "Create new block"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_FillGridSize", "CTRL + F"), LOCTEXT("Tooltip_FillGridSize", "Resize selected block/s to grid size"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_DeleteSelectedBlock","DEL"), LOCTEXT("Tooltip_DeleteSelectedBlock","Delete selected block/s"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_SelectMultipleBlocksOneByOne","SHIFT + L Click"), LOCTEXT("Tooltip_SelectMultipleBlocksOneByOne","Select multiple blocks one by one"));
	BuildShortcutAndTooltip(LOCTEXT("ShortCut_SelectMultipleBlocks","L Click + Drag"), LOCTEXT("Tooltip_SelectMultipleBlocks","Select blocks that intersect with the yellow rectangle"));

	return SNew(SToolTip)
	[
		ToolTipWidget.ToSharedRef()
	];
}


#undef LOCTEXT_NAMESPACE

