// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableObjectViewer.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "MuCOE/CustomizableObjectCompileRunnable.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SMutableCodeViewer.h"
#include "MuCOE/SMutableGraphViewer.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Views/STreeView.h"

class FExtender;
class FUICommandList;
class ITableRow;
class SWidget;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "SMutableDebugger"

// \todo: multi-column tree
namespace MutableObjectTreeViewColumns
{
	static const FName Name("Property");
	static const FName Value("Value");
};


class SMutableObjectTreeRow : public STableRow<TSharedPtr<FMutableObjectTreeElement>>
{
public:

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableObjectTreeElement>& InRowItem, const UCustomizableObject* InObject )
	{
		RowItem = InRowItem;

		FText MainLabel;;
		switch (RowItem->Type)
		{
		case FMutableObjectTreeElement::EType::SectionCaption:
		{
			switch (RowItem->Section)
			{
			case FMutableObjectTreeElement::ESection::General:
				MainLabel = FText::FromString(TEXT("General"));
				break;
			case FMutableObjectTreeElement::ESection::ChildObjects:
				MainLabel = FText::FromString(TEXT("Children"));
				break;
			default:
				MainLabel = FText::FromString(TEXT("Unknown"));
				break;
			}
			break;
		}

		case FMutableObjectTreeElement::EType::Name:
			MainLabel = FText::FromString(InObject->GetName() );

		default:
			MainLabel = FText::FromString(TEXT("Unknown"));
			break;
		}


		// TODO
		const FSlateBrush* IconBrush = nullptr;

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]

			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5.f, 0.f, 5.f, 0.f))
				.AutoWidth()
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(IconBrush ? IconBrush : FCoreStyle::Get().GetDefaultBrush())
						.ColorAndOpacity(IconBrush ? FLinearColor::White : FLinearColor::Transparent)
					]
				]

			+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(MainLabel)
				]
		];

		STableRow< TSharedPtr<FMutableObjectTreeElement> >::ConstructInternal(
			STableRow::FArguments()
			//.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(true)
			, InOwnerTableView
		);

	}


private:

	TSharedPtr<FMutableObjectTreeElement> RowItem;
};


void SMutableObjectViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CustomizableObject);
}


FString SMutableObjectViewer::GetReferencerName() const
{
	return TEXT("SMutableObjectViewer");
}


void SMutableObjectViewer::Construct(const FArguments& InArgs, UCustomizableObject* InObject, 
	TWeakPtr<FTabManager> InParentTabManager, const FName& InParentNewTabId )
{
	CustomizableObject = InObject;
	ParentTabManager = InParentTabManager;
	ParentNewTabId = InParentNewTabId;

	// Initialize the debugger compile options
	CompileOptions.bTextureCompression = true;
	CompileOptions.OptimizationLevel = 3;
	{
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();
		CompileOptions.TargetPlatform = Platforms.IsEmpty() ? nullptr : Platforms[0];
	}

	FToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None, TSharedPtr<FExtender>(), true);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Visible);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "SlimToolBar");

	ToolbarBuilder.BeginSection("Compilation");

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SMutableObjectViewer::GenerateMutableGraphPressed)),
		NAME_None,
		LOCTEXT("GenerateMutableGraph", "Unreal to Mutable Graph"),
		LOCTEXT("GenerateMutableGraphTooltip", "Generate a mutable graph from the customizable object source graph."),
		FSlateIcon(FCustomizableObjectEditorStyle::Get().GetStyleSetName(), "CustomizableObjectDebugger.GenerateMutableGraph", "CustomizableObjectDebugger.GenerateMutableGraph.Small"),
		EUserInterfaceActionType::Button
	);

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SMutableObjectViewer::CompileMutableCodePressed)),
		NAME_None,
		LOCTEXT("GenerateMutableCode", "Unreal to Mutable Code"),
		LOCTEXT("GenerateMutableCodeTooltipFromGraph", "Generate a mutable code from the customizable object source graph."),
		FSlateIcon(FCustomizableObjectEditorStyle::Get().GetStyleSetName(), "CustomizableObjectDebugger.CompileMutableCode", "CustomizableObjectDebugger.CompileMutableCode.Small"),
		EUserInterfaceActionType::Button
	);

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SMutableObjectViewer::GenerateCompileOptionsMenuContent),
		LOCTEXT("Compile_Options_Label", "Compile Options"),
		LOCTEXT("Compile_Options_Tooltip", "Change Compile Options"),
		TAttribute<FSlateIcon>(),
		true);

	ToolbarBuilder.EndSection();

	// Initialize the root tree nodes
	RootTreeNodes.Add(MakeShareable(new FMutableObjectTreeElement(FMutableObjectTreeElement::EType::SectionCaption, FMutableObjectTreeElement::ESection::General, InObject)));
	RootTreeNodes.Add(MakeShareable(new FMutableObjectTreeElement(FMutableObjectTreeElement::EType::SectionCaption, FMutableObjectTreeElement::ESection::ChildObjects, InObject)));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			ToolbarBuilder.MakeWidget()
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(UE_MUTABLE_GET_BRUSH("ToolPanel.GroupBorder"))
			.Padding(FMargin(4.0f, 4.0f))
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FMutableObjectTreeElement>>)
				.TreeItemsSource(&RootTreeNodes)
				.OnGenerateRow(this,&SMutableObjectViewer::GenerateRowForNodeTree)
				.OnGetChildren(this, &SMutableObjectViewer::GetChildrenForInfo)
				.SelectionMode(ESelectionMode::None)					
			]
		]
	];	
}


void SMutableObjectViewer::GenerateMutableGraphPressed()
{
	// Convert from Unreal graph to Mutable graph.
	mu::Ptr<mu::Node> RootNode = Compiler.Export(CustomizableObject, CompileOptions);
	if (!RootNode)
	{
		// TODO: Show errors
		return;
	}

	FString DataTag = FString::Printf(TEXT("%s for %s"), *CustomizableObject->GetName(), *CompileOptions.TargetPlatform->PlatformName());

	TSharedPtr<SDockTab> NewMutableGraphTab = SNew(SDockTab)
		.Label(LOCTEXT("MutableGraph", "Mutable Graph"))
		[
			SNew(SMutableGraphViewer, RootNode, CompileOptions, ParentTabManager, ParentNewTabId)
			.DataTag(DataTag)
		];

	TSharedPtr<FTabManager> TabManager = ParentTabManager.Pin();
	check(TabManager);
	TabManager->InsertNewDocumentTab(ParentNewTabId, FTabManager::ESearchPreference::PreferLiveTab, NewMutableGraphTab.ToSharedRef());
}


void SMutableObjectViewer::CompileMutableCodePressed()
{
	if (CompileOptions.bForceLargeLODBias)
	{
		// Debug compile with many different biasses
		const int32 MaxBias = 15;
		for (int32 Bias = 0; Bias < MaxBias; ++Bias)
		{
			CompileOptions.DebugBias = Bias;

			mu::NodePtr RootNode = Compiler.Export(CustomizableObject, CompileOptions);
			if (!RootNode)
			{
				// TODO: Show errors
				ensure(false);
				return;
			}

			// Do the compilation to Mutable Code synchronously.
			TSharedPtr<FCustomizableObjectCompileRunnable> CompileTask = MakeShareable(new FCustomizableObjectCompileRunnable(RootNode));
			CompileTask->Options = CompileOptions;
			CompileTask->Init();
			CompileTask->Run();
		}

	}

	// Convert from Unreal graph to Mutable graph.
	mu::NodePtr RootNode = Compiler.Export(CustomizableObject, CompileOptions);
	if (!RootNode)
	{
		// TODO: Show errors
		ensure(false);
		return;
	}

	// Do the compilation to Mutable Code synchronously.
	TSharedPtr<FCustomizableObjectCompileRunnable> CompileTask = MakeShareable(new FCustomizableObjectCompileRunnable(RootNode));
	CompileTask->Options = CompileOptions;
	CompileTask->Init();
	CompileTask->Run();

	FString DataTag = FString::Printf( TEXT("%s for %s opt %d "), *CustomizableObject->GetName(), *CompileOptions.TargetPlatform->PlatformName(), CompileOptions.OptimizationLevel );

	TSharedPtr<SDockTab> NewMutableCodeTab = SNew(SDockTab)
		.Label(LOCTEXT("MutableCode", "Mutable Code"))
		[
			SNew(SMutableCodeViewer, CompileTask->Model)
			.DataTag(DataTag)
		];

	TSharedPtr<FTabManager> TabManager = ParentTabManager.Pin();
	check(TabManager);
	TabManager->InsertNewDocumentTab(ParentNewTabId, FTabManager::ESearchPreference::PreferLiveTab, NewMutableCodeTab.ToSharedRef());
}


TSharedRef<SWidget> SMutableObjectViewer::GenerateCompileOptionsMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = false;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	// settings
	MenuBuilder.BeginSection("Optimization", LOCTEXT("MutableCompileOptimizationHeading", "Optimization"));
	{
		// Unreal Graph to Mutable Graph options
		//-----------------------------------

		// Platform
		DebugPlatformStrings.Empty();

		TSharedPtr<FString> SelectedPlatform;
		{
			ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
			check(TPM);

			ITargetPlatform* CurrentPlatform = NULL;
			const TArray<ITargetPlatform*>& Platforms = TPM->GetTargetPlatforms();
			for (const ITargetPlatform* Platform : Platforms)
			{
				TSharedPtr<FString> ThisPlatform = MakeShareable(new FString(Platform->PlatformName()));
				DebugPlatformStrings.Add(ThisPlatform);
				if (Platform == CompileOptions.TargetPlatform)
				{
					SelectedPlatform = ThisPlatform;
				}
			}
		}

		if (!SelectedPlatform.IsValid() && DebugPlatformStrings.Num())
		{
			SelectedPlatform = DebugPlatformStrings[0];
		}

		DebugPlatformCombo =
			SNew(STextComboBox)
			.OptionsSource(&DebugPlatformStrings)
			.InitiallySelectedItem(SelectedPlatform)
			.OnSelectionChanged(this, &SMutableObjectViewer::OnChangeDebugPlatform)
			;

		MenuBuilder.AddWidget(DebugPlatformCombo.ToSharedRef(), LOCTEXT("MutableDebugPlatform", "Target Platform"));

		// Texture compresssion
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Generate_MutableEnableTextureCompression", "Enable texture compression."),
			LOCTEXT("Generate_MutableEnableTextureCompressionTooltip", "Enable or disable the compression of textures in the mutable code compilation."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]() { CompileOptions.bTextureCompression = !CompileOptions.bTextureCompression; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return CompileOptions.bTextureCompression; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		// Compilation options
		//-----------------------------------

		// Optimisation level
		CompileOptimizationStrings.Empty();
		CompileOptimizationStrings.Add(MakeShareable(new FString(LOCTEXT("OptimizationNone", "None").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(LOCTEXT("OptimizationMin", "Minimal").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(LOCTEXT("OptimizationMed", "Medium").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(LOCTEXT("OptimizationMax", "Maximum").ToString())));

		CompileOptions.OptimizationLevel = FMath::Min(CompileOptions.OptimizationLevel, CompileOptimizationStrings.Num() - 1);

		CompileOptimizationCombo =
			SNew(STextComboBox)
			.OptionsSource(&CompileOptimizationStrings)
			.InitiallySelectedItem(CompileOptimizationStrings[CompileOptions.OptimizationLevel])
			.OnSelectionChanged(this, &SMutableObjectViewer::OnChangeCompileOptimizationLevel)
			;
		MenuBuilder.AddWidget(CompileOptimizationCombo.ToSharedRef(), LOCTEXT("MutableCompileOptimizationLevel", "Optimization Level"));

		// Image tiling
		// Unfortunately SNumericDropDown doesn't work with integers at the time of writing.
		TArray<SNumericDropDown<float>::FNamedValue> TilingOptions;
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(0, FText::FromString(TEXT("0")), FText::FromString(TEXT("Disabled"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(64, FText::FromString(TEXT("64")), FText::FromString(TEXT("64"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(128, FText::FromString(TEXT("128")), FText::FromString(TEXT("128"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(256, FText::FromString(TEXT("256")), FText::FromString(TEXT("256"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(512, FText::FromString(TEXT("512")), FText::FromString(TEXT("512"))));

		CompileTilingCombo = SNew(SNumericDropDown<float>)
			.DropDownValues(TilingOptions)
			.Value_Lambda( [&]() { return float(CompileOptions.ImageTiling); })
			.OnValueChanged_Lambda( [&](float Value) { CompileOptions.ImageTiling = int32(Value); } )
			;
		MenuBuilder.AddWidget(CompileTilingCombo.ToSharedRef(), LOCTEXT("MutableCompileImageTiling", "Image Tiling"));

		// Disk as cache
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Generate_MutableUseDisk", "Enable compiling using the disk as memory."),
			LOCTEXT("Generate_MutableUseDiskTooltip", "This is very slow but supports compiling huge objects. It requires a lot of free space in the OS disk."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]() { CompileOptions.bUseDiskCompilation = !CompileOptions.bUseDiskCompilation; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return CompileOptions.bUseDiskCompilation; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		// Debug LODBias
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ForceLargeLODBias", "Force a large texture LODBias."),
			LOCTEXT("ForceLargeLODBiasTooltip", "This is useful to test compilation of special cook modes."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]() { CompileOptions.bForceLargeLODBias = !CompileOptions.bForceLargeLODBias; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return CompileOptions.bForceLargeLODBias; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void SMutableObjectViewer::OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	CompileOptions.OptimizationLevel = CompileOptimizationStrings.Find(NewSelection);
}


void SMutableObjectViewer::OnChangeDebugPlatform(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!CustomizableObject) return;

	CompileOptions.TargetPlatform = nullptr;

	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	check(TPM);
	const TArray<ITargetPlatform*>& Platforms = TPM->GetTargetPlatforms();
	check(Platforms.Num());

	CompileOptions.TargetPlatform = Platforms[0];

	for (int32 Index = 1; Index < Platforms.Num(); Index++)
	{
		if (Platforms[Index]->PlatformName() == *NewSelection)
		{
			CompileOptions.TargetPlatform = Platforms[Index];
			break;
		}
	}
}


TSharedRef<ITableRow> SMutableObjectViewer::GenerateRowForNodeTree(TSharedPtr<FMutableObjectTreeElement> InTreeNode, const TSharedRef<STableViewBase>& InOwnerTable)
{
	TSharedRef<SMutableObjectTreeRow> Row = SNew(SMutableObjectTreeRow, InOwnerTable, InTreeNode, CustomizableObject);
	return Row;
}


void SMutableObjectViewer::GetChildrenForInfo(TSharedPtr<FMutableObjectTreeElement> InInfo, TArray<TSharedPtr<FMutableObjectTreeElement>>& OutChildren)
{
	switch (InInfo->Type)
	{
	case FMutableObjectTreeElement::EType::SectionCaption:
		switch (InInfo->Section)
		{

		case FMutableObjectTreeElement::ESection::General:
		{
			OutChildren.Add(MakeShareable(new FMutableObjectTreeElement(FMutableObjectTreeElement::EType::Name, InInfo->Section, CustomizableObject )));
			break;
		}

		case FMutableObjectTreeElement::ESection::ChildObjects:
		{
			// TODO: based on what is done in FCustomizableObjectCompiler::ProcessChildObjectsRecursively
			//OutChildren.Add(MakeShareable(new FMutableObjectTreeElement(FMutableObjectTreeElement::EType::Name, InInfo->Section)));
			break;
		}

		default:
			break;
		}
		break;

	default:
		break;
	}
}


#undef LOCTEXT_NAMESPACE 
