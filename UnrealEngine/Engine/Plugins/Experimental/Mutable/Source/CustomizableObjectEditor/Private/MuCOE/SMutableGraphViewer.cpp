// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableGraphViewer.h"

#include "Containers/StringConv.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "IDesktopPlatform.h"
#include "Input/DragAndDrop.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/Paths.h"
#include "MuCOE/CustomizableObjectCompileRunnable.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SMutableCodeViewer.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuT/Streams.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateColor.h"
#include "Textures/SlateIcon.h"
#include "UObject/UnrealNames.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class FExtender;
class FReferenceCollector;
class FUICommandList;
class ITableRow;
class SWidget;
struct FGeometry;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "SMutableDebugger"


// \todo: multi-column tree
namespace MutableGraphTreeViewColumns
{
	static const FName Name("Name");
};


class SMutableGraphTreeRow : public STableRow<TSharedPtr<FMutableGraphTreeElement>>
{
public:

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableGraphTreeElement>& InRowItem)
	{
		RowItem = InRowItem;

		const char* TypeName = RowItem->MutableNode->GetType()->m_strName;

		FText MainLabel = FText::FromString(ANSI_TO_TCHAR(TypeName));
		if (RowItem->DuplicatedOf)
		{
			MainLabel = FText::FromString( FString::Printf(TEXT("%s (Duplicated)"), ANSI_TO_TCHAR(TypeName)));
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

		STableRow< TSharedPtr<FMutableGraphTreeElement> >::ConstructInternal(
			STableRow::FArguments()
			//.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(true)
			, InOwnerTableView
		);

	}


private:

	TSharedPtr<FMutableGraphTreeElement> RowItem;
};


void SMutableGraphViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Add UObjects here if we own any at some point
	//Collector.AddReferencedObject(CustomizableObject);
}


FString SMutableGraphViewer::GetReferencerName() const
{
	return TEXT("SMutableGraphViewer");
}


void SMutableGraphViewer::Construct(const FArguments& InArgs, const mu::NodePtr& InRootNode, 
	const FCompilationOptions& InCompileOptions,
	TWeakPtr<FTabManager> InParentTabManager, const FName& InParentNewTabId)
{
	DataTag = InArgs._DataTag;
	RootNode = InRootNode;
	CompileOptions = InCompileOptions;	
	ParentTabManager = InParentTabManager;
	ParentNewTabId = InParentNewTabId;

	FToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None, TSharedPtr<FExtender>(), true);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Visible);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "SlimToolBar");

	// Export
	ToolbarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateLambda([InRootNode]()
				{
					TArray<FString> SaveFilenames;
					IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
					bool bSave = false;
					if (!DesktopPlatform) return;

					FString LastExportPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT);
					FString FileTypes = TEXT("Mutable source data files|*.mutable_source|All files|*.*");
					bSave = DesktopPlatform->SaveFileDialog(
						FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
						TEXT("Export Mutable object"),
						*LastExportPath,
						TEXT("exported.mutable_source"),
						*FileTypes,
						EFileDialogFlags::None,
						SaveFilenames
					);

					if (!bSave) return;

					// Dump source model to a file.
					FString SaveFileName = FString(SaveFilenames[0]);
					mu::OutputFileStream stream(TCHAR_TO_ANSI(*SaveFileName));
					stream.Write(MUTABLE_SOURCE_MODEL_FILETAG, 4);
					mu::OutputArchive arch(&stream);
					mu::Node::Serialise(InRootNode.get(), arch);
					stream.Flush();

					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, SaveFileName);
				})
			),
			NAME_None,
			LOCTEXT("ExportMutableGraph", "Export"),
			LOCTEXT("ExportMutableGraphTooltip", "Export a debug mutable graph file."),
			FSlateIcon(),
			EUserInterfaceActionType::Button
		);
		
	ToolbarBuilder.EndSection();


	ToolbarBuilder.BeginSection("Compilation");

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SMutableGraphViewer::CompileMutableCodePressed)),
		NAME_None,
		LOCTEXT("GenerateMutableCode", "Unreal to Mutable Code"),
		LOCTEXT("GenerateMutableCodeTooltip", "Generate a mutable code from the mutable graph."),
		FSlateIcon(FCustomizableObjectEditorStyle::Get().GetStyleSetName(), "CustomizableObjectDebugger.CompileMutableCode", "CustomizableObjectDebugger.CompileMutableCode.Small"),
		EUserInterfaceActionType::Button
	);

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SMutableGraphViewer::GenerateCompileOptionsMenuContent),
		LOCTEXT("Compile_Options_Label", "Compile Options"),
		LOCTEXT("Compile_Options_Tooltip", "Change Compile Options"),
		TAttribute<FSlateIcon>(),
		true);

	ToolbarBuilder.EndSection();

	ToolbarBuilder.AddWidget(SNew(STextBlock).Text(MakeAttributeLambda([this]() { return FText::FromString(DataTag); })));

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
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<FMutableGraphTreeElement>>)
					.TreeItemsSource(&RootNodes)
					.OnGenerateRow(this,&SMutableGraphViewer::GenerateRowForNodeTree)
					.OnGetChildren(this, &SMutableGraphViewer::GetChildrenForInfo)
					.OnSetExpansionRecursive(this, &SMutableGraphViewer::TreeExpandRecursive)
					.OnContextMenuOpening(this, &SMutableGraphViewer::OnTreeContextMenuOpening)
					.SelectionMode(ESelectionMode::Single)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(MutableGraphTreeViewColumns::Name)
						.FillWidth(25.f)
						.DefaultLabel(LOCTEXT("Node Name", "Node Name"))
					)
				]
			]
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				//[
				//	SubjectsTreeView->AsShared()
				//]
			]
		]
	];
	
	RebuildTree();
}


void SMutableGraphViewer::CompileMutableCodePressed()
{
	// Do the compilation to Mutable Code synchronously.
	TSharedPtr<FCustomizableObjectCompileRunnable> CompileTask = MakeShareable(new FCustomizableObjectCompileRunnable(RootNode, false));
	CompileTask->Options = CompileOptions;
	CompileTask->Init();
	CompileTask->Run();

	FString NewDataTag = FString::Printf(TEXT("%s graph, opt %d "), *DataTag, CompileOptions.OptimizationLevel);

	TSharedPtr<SDockTab> NewMutableCodeTab = SNew(SDockTab)
		.Label(LOCTEXT("MutableCode", "Mutable Code"))
		[
			SNew(SMutableCodeViewer, CompileTask->Model)
			.DataTag(NewDataTag)
		];

	TSharedPtr<FTabManager> TabManager = ParentTabManager.Pin();
	check(TabManager);
	TabManager->InsertNewDocumentTab(ParentNewTabId, FTabManager::ESearchPreference::PreferLiveTab, NewMutableCodeTab.ToSharedRef());
}


TSharedRef<SWidget> SMutableGraphViewer::GenerateCompileOptionsMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = false;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	// settings
	MenuBuilder.BeginSection("Optimization", LOCTEXT("MutableCompileOptimizationHeading", "Optimization"));
	{
		// Compilation options
		//-----------------------------------

		// Optimisation level
		CompileOptimizationStrings.Empty();
		CompileOptimizationStrings.Add(MakeShareable(new FString(NSLOCTEXT("CustomizableObjectEditor", "OptimizationNone", "None").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(NSLOCTEXT("CustomizableObjectEditor", "OptimizationMin", "Minimal").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(NSLOCTEXT("CustomizableObjectEditor", "OptimizationMed", "Medium").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(NSLOCTEXT("CustomizableObjectEditor", "OptimizationMax", "Maximum").ToString())));

		CompileOptions.OptimizationLevel = FMath::Min(CompileOptions.OptimizationLevel, CompileOptimizationStrings.Num() - 1);

		CompileOptimizationCombo =
			SNew(STextComboBox)
			.OptionsSource(&CompileOptimizationStrings)
			.InitiallySelectedItem(CompileOptimizationStrings[CompileOptions.OptimizationLevel])
			.OnSelectionChanged(this, &SMutableGraphViewer::OnChangeCompileOptimizationLevel)
			;
		MenuBuilder.AddWidget(CompileOptimizationCombo.ToSharedRef(), LOCTEXT("MutableCompileOptimizationLevel", "Optimization Level"));

		// Parallel compilation
		MenuBuilder.AddMenuEntry(
			LOCTEXT("MutableEnableParallelCompilation", "Enable compiling in multiple threads."),
			LOCTEXT("MutableEnableParallelCompilationTooltip", "This is faster but use more memory."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]() { CompileOptions.bUseParallelCompilation = !CompileOptions.bUseParallelCompilation; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return CompileOptions.bUseParallelCompilation; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		// Disk as cache
		MenuBuilder.AddMenuEntry(
			LOCTEXT("MutableDiskAsMemory", "Enable compiling using the disk as memory."),
			LOCTEXT("MutableDiskAsMemoryTooltip", "This is very slow but supports compiling huge objects. It requires a lot of free space in the OS disk."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]() { CompileOptions.bUseDiskCompilation = !CompileOptions.bUseDiskCompilation; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return CompileOptions.bUseDiskCompilation; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void SMutableGraphViewer::OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	CompileOptions.OptimizationLevel = CompileOptimizationStrings.Find(NewSelection);
}


void SMutableGraphViewer::RebuildTree()
{
	RootNodes.Reset();
	ItemCache.Reset();
	MainItemPerNode.Reset();

	RootNodes.Add(MakeShareable(new FMutableGraphTreeElement(RootNode)));
	TreeView->RequestTreeRefresh();
	TreeExpandUnique();
}


TSharedRef<ITableRow> SMutableGraphViewer::GenerateRowForNodeTree(TSharedPtr<FMutableGraphTreeElement> InTreeNode, const TSharedRef<STableViewBase>& InOwnerTable)
{
	TSharedRef<SMutableGraphTreeRow> Row = SNew(SMutableGraphTreeRow, InOwnerTable, InTreeNode);
	return Row;
}

void SMutableGraphViewer::GetChildrenForInfo(TSharedPtr<FMutableGraphTreeElement> InInfo, TArray<TSharedPtr<FMutableGraphTreeElement>>& OutChildren)
{
	if (!InInfo->MutableNode)
	{
		return;
	}

	// If this is a duplicated of another row, don't provide its children.
	if (InInfo->DuplicatedOf)
	{
		return;
	}

	// Generic node case
	int32 InputCount = InInfo->MutableNode->GetInputCount();
	for (int32 InputIndex=0; InputIndex<InputCount; ++InputIndex)
	{
		mu::NodePtr ChildNode = InInfo->MutableNode->GetInputNode( InputIndex );
		if (ChildNode)
		{
			FItemCacheKey Key = { InInfo->MutableNode.get(),ChildNode.get(), uint32(InputIndex) };
			TSharedPtr<FMutableGraphTreeElement>* CachedItem = ItemCache.Find(Key);

			if (CachedItem)
			{
				OutChildren.Add(*CachedItem);
			}
			else
			{
				TSharedPtr<FMutableGraphTreeElement>* MainItemPtr = MainItemPerNode.Find(ChildNode.get());
				TSharedPtr<FMutableGraphTreeElement> Item = MakeShareable(new FMutableGraphTreeElement(ChildNode, MainItemPtr));
				OutChildren.Add(Item);
				ItemCache.Add(Key, Item);

				if (!MainItemPtr)
				{
					MainItemPerNode.Add(ChildNode.get(),Item);
				}
			}
		}
	}
}


TSharedPtr<SWidget> SMutableGraphViewer::OnTreeContextMenuOpening()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Graph_Expand_Instance", "Expand Instance-Level Operations"),
		LOCTEXT("Graph_Expand_Instance_Tooltip", "Expands all the operations in the tree that are instance operations (not images, meshes, booleans, etc.)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMutableGraphViewer::TreeExpandUnique)
			//, FCanExecuteAction::CreateSP(this, &SMutableCodeViewer::HasAnyItemInPalette)
		)
	);

	return MenuBuilder.MakeWidget();
}


void SMutableGraphViewer::TreeExpandRecursive(TSharedPtr<FMutableGraphTreeElement> InInfo, bool bExpand)
{
	if (bExpand)
	{
		TreeExpandUnique();
	}
}


void SMutableGraphViewer::TreeExpandUnique()
{
	TArray<TSharedPtr<FMutableGraphTreeElement>> Pending = RootNodes;

	TSet<TSharedPtr<FMutableGraphTreeElement>> Processed;

	TArray<TSharedPtr<FMutableGraphTreeElement>> Children;

	while (!Pending.IsEmpty())
	{
		TSharedPtr<FMutableGraphTreeElement> Item = Pending.Pop();
		TreeView->SetItemExpansion(Item, true);

		Children.SetNum(0);
		GetChildrenForInfo(Item, Children);
		Pending.Append(Children);
	}
}



FReply SMutableGraphViewer::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".mutable_source"))
				{
					// Dump source model to a file.
					mu::InputFileStream stream(TCHAR_TO_ANSI(*Files[0]));

					char MutableSourceTag[4] = {};
					stream.Read(MutableSourceTag, 4);

					if (!FMemory::Memcmp(MutableSourceTag, MUTABLE_SOURCE_MODEL_FILETAG, 4))
					{
						return FReply::Handled();
					}

					return FReply::Unhandled();
				}
			}
		}
	}

	return FReply::Unhandled();
}


FReply SMutableGraphViewer::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);

	if (TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".mutable_source"))
				{
					// Dump source model to a file.
					mu::InputFileStream stream(TCHAR_TO_ANSI(*Files[0]));

					char MutableSourceTag[4] = {};
					stream.Read(MutableSourceTag, 4);

					if (!FMemory::Memcmp(MutableSourceTag, MUTABLE_SOURCE_MODEL_FILETAG, 4))
					{
						mu::InputArchive arch(&stream);
						RootNode = mu::Node::StaticUnserialise( arch );
						DataTag = FString("dropped-file ") + FPaths::GetCleanFilename(Files[0]);
						RebuildTree();

						return FReply::Handled();
					}

					return FReply::Unhandled();
				}
			}
		}

		return FReply::Unhandled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE 
