// Copyright Epic Games, Inc. All Rights Reserved.
#include "SInterchangePipelineConfigurationDialog.h"

#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "IDetailsView.h"
#include "IDocumentation.h"
#include "InterchangeBlueprintPipelineBase.h"
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "InterchangeProjectSettings.h"
#include "InterchangePythonPipelineBase.h"
#include "InterchangeSourceData.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "SPrimaryButton.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "InterchangePipelineConfiguration"

extern INTERCHANGECORE_API bool GInterchangeEnableCustomPipelines;

const FName ReimportPipelineName = TEXT("ReimportPipeline");

/************************************************************************/
/* FInterchangePipelineStacksTreeNodeItem Implementation                    */
/************************************************************************/

void FInterchangePipelineStacksTreeNodeItem::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (Pipeline)
	{
		Collector.AddReferencedObject(Pipeline);
	}
}

/************************************************************************/
/* SInterchangePipelineStacksTreeView Implementation                    */
/************************************************************************/

SInterchangePipelineStacksTreeView::~SInterchangePipelineStacksTreeView()
{

}

void SInterchangePipelineStacksTreeView::Construct(const FArguments& InArgs)
{
	OnSelectionChangedDelegate = InArgs._OnSelectionChangedDelegate;
	SourceData = InArgs._SourceData;
	PipelineStack = InArgs._PipelineStack;
	bSceneImport = InArgs._bSceneImport;
	bReimport = InArgs._bReimport && PipelineStack.Num() > 0;

	//Build the FbxNodeInfoPtr tree data
	const FName& DefaultPipelineStackName = bReimport ? ReimportPipelineName : FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bSceneImport, *SourceData.Get());
	const TMap<FName, FInterchangePipelineStack>& PipelineStacks = FInterchangeProjectSettingsUtils::GetDefaultImportSettings(bSceneImport).PipelineStacks;

	//In reimport we modify directly the asset pipeline
	if (bReimport)
	{
		TSharedPtr<FInterchangePipelineStacksTreeNodeItem> StackNode = MakeShared<FInterchangePipelineStacksTreeNodeItem>();
		StackNode->StackName = ReimportPipelineName;
		StackNode->Pipeline = nullptr;
		for (int32 PipelineIndex = 0; PipelineIndex < PipelineStack.Num(); ++PipelineIndex)
		{
			if(UInterchangePipelineBase* GeneratedPipeline = PipelineStack[PipelineIndex])
			{
				//When we re-import we save the setting so ResetToDefault can simply just reload the settings
				GeneratedPipeline->SaveSettings(ReimportPipelineName);
				TSharedPtr<FInterchangePipelineStacksTreeNodeItem> PipelineNode = MakeShared<FInterchangePipelineStacksTreeNodeItem>();
				PipelineNode->StackName = ReimportPipelineName;
				//Currently the re-import never lock properties
				PipelineNode->Pipeline = GeneratedPipeline;
				StackNode->Childrens.Add(PipelineNode);
			}
		}
		RootNodeArray.Add(StackNode);
	}
	else
	{
		for (const TPair<FName, FInterchangePipelineStack>& NameAndPipelineStack : PipelineStacks)
		{
			TSharedPtr<FInterchangePipelineStacksTreeNodeItem> StackNode = MakeShared<FInterchangePipelineStacksTreeNodeItem>();
			StackNode->StackName = NameAndPipelineStack.Key;
			StackNode->Pipeline = nullptr;
			const FInterchangePipelineStack& InterchangePipelineStack = NameAndPipelineStack.Value;
			for (int32 PipelineIndex = 0; PipelineIndex < InterchangePipelineStack.Pipelines.Num(); ++PipelineIndex)
			{
				if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstance(InterchangePipelineStack.Pipelines[PipelineIndex]))
				{
					GeneratedPipeline->AdjustSettingsForContext((bSceneImport ? EInterchangePipelineContext::SceneImport : EInterchangePipelineContext::AssetImport), nullptr);
					//Load the settings for this pipeline
					GeneratedPipeline->LoadSettings(NameAndPipelineStack.Key);
					GeneratedPipeline->PreDialogCleanup(NameAndPipelineStack.Key);
					TSharedPtr<FInterchangePipelineStacksTreeNodeItem> PipelineNode = MakeShared<FInterchangePipelineStacksTreeNodeItem>();
					PipelineNode->StackName = NameAndPipelineStack.Key;
					PipelineNode->Pipeline = GeneratedPipeline;
					StackNode->Childrens.Add(PipelineNode);
				}
			}
			RootNodeArray.Add(StackNode);
		}
	}

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&RootNodeArray)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SInterchangePipelineStacksTreeView::OnGenerateRowPipelineConfigurationTreeView)
		.OnGetChildren(this, &SInterchangePipelineStacksTreeView::OnGetChildrenPipelineConfigurationTreeView)
		.OnContextMenuOpening(this, &SInterchangePipelineStacksTreeView::OnOpenContextMenu)
		.OnSelectionChanged(this, &SInterchangePipelineStacksTreeView::OnTreeViewSelectionChanged)
	);
}

void SInterchangePipelineStacksTreeView::SelectDefaultItem()
{
	FName DefaultStackName = bReimport ? ReimportPipelineName : FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bSceneImport, *SourceData.Get());

	for (const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& PipelineStacksTreeNodeItem : RootNodeArray)
	{
		if (PipelineStacksTreeNodeItem->StackName == DefaultStackName && !PipelineStacksTreeNodeItem->Childrens.IsEmpty())
		{
			const bool bShouldExpandItem = true;
			SetItemExpansion(PipelineStacksTreeNodeItem, bShouldExpandItem);
			SetSelection(PipelineStacksTreeNodeItem->Childrens[0]);

			break;
		}
	}
}

/** The item used for visualizing the class in the tree. */
class SInterchangePipelineStacksTreeViewItem : public STableRow< TSharedPtr<FInterchangePipelineStacksTreeNodeItem> >
{
public:

	SLATE_BEGIN_ARGS(SInterchangePipelineStacksTreeViewItem)
		: _InterchangeNode(nullptr)
	{}

		/** The item content. */
		SLATE_ARGUMENT(TSharedPtr<FInterchangePipelineStacksTreeNodeItem>, InterchangeNode)
		SLATE_ARGUMENT(bool, bSceneImport)
		SLATE_ARGUMENT(FName, DefaultPipelineStackName)
	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		InterchangeNode = InArgs._InterchangeNode;
		bSceneImport = InArgs._bSceneImport;
		DefaultPipelineStackName = InArgs._DefaultPipelineStackName;

		//This is suppose to always be valid
		check(InterchangeNode);
		const bool bIsPipelineStackNode = InterchangeNode->Pipeline == nullptr;

		//Prepare the tooltip
		FString Tooltip;
		//FString Tooltip = bIsPipelineStackNode ? InterchangeNode->GetDisplayLabel();
		FText NodeDisplayLabel = FText::FromName(bIsPipelineStackNode ? InterchangeNode->StackName : InterchangeNode->Pipeline->GetClass()->GetFName());
		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 6.0f, 2.0f)
			[
				SNew(SImage)
				.Image(this, &SInterchangePipelineStacksTreeViewItem::GetImageItemIcon)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f, 3.0f, 6.0f, 3.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NodeDisplayLabel)
				.ToolTipText(FText::FromString(Tooltip))
			]
		];

		STableRow< TSharedPtr<FInterchangePipelineStacksTreeNodeItem> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true),
			InOwnerTableView
		);
	}

private:
	const FSlateBrush* GetImageItemIcon() const
	{
		//This is suppose to always be valid
		check(InterchangeNode);
		const bool bIsPipelineStackNode = InterchangeNode->Pipeline == nullptr;
		const bool bIsDefaultStackNode = bIsPipelineStackNode && (DefaultPipelineStackName == InterchangeNode->StackName);
		const FSlateBrush * TypeIcon = nullptr;
		FName IconName = bIsDefaultStackNode ? "PipelineConfigurationIcon.PipelineStackDefault" : bIsPipelineStackNode ? "PipelineConfigurationIcon.PipelineStack" : "PipelineConfigurationIcon.Pipeline";
		if (IconName != NAME_None)
		{
			const FSlateIcon SlateIcon = FSlateIconFinder::FindIcon(IconName);
			TypeIcon = SlateIcon.GetOptionalIcon();
		}

		if (!TypeIcon)
		{
			TypeIcon = FSlateIconFinder::FindIconBrushForClass(AActor::StaticClass());
		}
		return TypeIcon;
	}

	/** The node to build the tree view row from. */
	TSharedPtr<FInterchangePipelineStacksTreeNodeItem> InterchangeNode = nullptr;
	bool bSceneImport = false;
	FName DefaultPipelineStackName;
};

TSharedRef< ITableRow > SInterchangePipelineStacksTreeView::OnGenerateRowPipelineConfigurationTreeView(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Item, const TSharedRef< STableViewBase >& OwnerTable)
{
	FName DefaultStackName = bReimport ? ReimportPipelineName : FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bSceneImport, *SourceData.Get());

	TSharedRef< SInterchangePipelineStacksTreeViewItem > ReturnRow = SNew(SInterchangePipelineStacksTreeViewItem, OwnerTable)
		.InterchangeNode(Item)
		.bSceneImport(bSceneImport)
		.DefaultPipelineStackName(DefaultStackName);
	return ReturnRow;
}
void SInterchangePipelineStacksTreeView::OnGetChildrenPipelineConfigurationTreeView(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> InParent, TArray< TSharedPtr<FInterchangePipelineStacksTreeNodeItem> >& OutChildren)
{
	for (int32 ChildIndex = 0; ChildIndex < InParent->Childrens.Num(); ++ChildIndex)
	{
		TSharedPtr<FInterchangePipelineStacksTreeNodeItem> ChildNode = InParent->Childrens[ChildIndex];
		if (!ChildNode.IsValid())
			continue;
		OutChildren.Add(ChildNode);
	}
}

void SInterchangePipelineStacksTreeView::RecursiveSetExpand(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Node, bool ExpandState)
{
	SetItemExpansion(Node, ExpandState);
	for (int32 ChildIndex = 0; ChildIndex < Node->Childrens.Num(); ++ChildIndex)
	{
		TSharedPtr<FInterchangePipelineStacksTreeNodeItem> ChildNode = Node->Childrens[ChildIndex];
		if (!ChildNode.IsValid())
			continue;
		RecursiveSetExpand(ChildNode, ExpandState);
	}
}

FReply SInterchangePipelineStacksTreeView::OnExpandAll()
{
	for (TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Node : RootNodeArray)
	{
		if (!ensure(Node))
		{
			continue;
		}
		RecursiveSetExpand(Node, true);
	}
	return FReply::Handled();
}

FReply SInterchangePipelineStacksTreeView::OnCollapseAll()
{
	for (TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Node : RootNodeArray)
	{
		if (!ensure(Node))
		{
			continue;
		}
		RecursiveSetExpand(Node, false);
	}
	return FReply::Handled();
}

TSharedPtr<SWidget> SInterchangePipelineStacksTreeView::OnOpenContextMenu()
{
	if (bReimport)
	{
		return SNullWidget::NullWidget;
	}

	// Build up the menu for a selection
	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, TSharedPtr<FUICommandList>());

	TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>> SelectedNodes;
	const auto NumSelectedItems = GetSelectedItems(SelectedNodes);

	if (SelectedNodes.Num() == 1)
	{
		TSharedPtr<FInterchangePipelineStacksTreeNodeItem> SelectNode = SelectedNodes[0];
		if (SelectNode->Pipeline == nullptr)
		{
			// We always create a section here, even if there is no parent so that clients can still extend the menu
			MenuBuilder.BeginSection("TreeViewContextMenuStackNodeSection");
			{
				const FSlateIcon DefaultIcon(FAppStyle::GetAppStyleSetName(), "Icons.Default");
				MenuBuilder.AddMenuEntry(LOCTEXT("SetAsDefaultMenuAction", "Set As Default Stack"), FText(), DefaultIcon, FUIAction(FExecuteAction::CreateSP(this, &SInterchangePipelineStacksTreeView::SetAsDefaultStack, SelectNode->StackName)));
			}
			MenuBuilder.EndSection();
		}
	}
	

	return MenuBuilder.MakeWidget();
}

void SInterchangePipelineStacksTreeView::SetAsDefaultStack(FName NewDefaultStackValue)
{
	UInterchangeProjectSettings* InterchangeProjectSettingsCDO = GetMutableDefault<UInterchangeProjectSettings>();
	FName& DefaultPipelineStackName = [this, InterchangeProjectSettingsCDO]() -> FName&
	{
		if (bSceneImport)
		{
			return InterchangeProjectSettingsCDO->SceneImportSettings.DefaultPipelineStack;
		}
		else
		{
			return InterchangeProjectSettingsCDO->ContentImportSettings.DefaultPipelineStack;
		}
	}();

	const TMap<FName, FInterchangePipelineStack>& PipelineStacks =
		[this, InterchangeProjectSettingsCDO]() -> const TMap<FName, FInterchangePipelineStack>&
		{
			if (bSceneImport)
			{
				return InterchangeProjectSettingsCDO->SceneImportSettings.PipelineStacks;
			}
			else
			{
				return InterchangeProjectSettingsCDO->ContentImportSettings.PipelineStacks;
			}
		}();

	if (PipelineStacks.Contains(NewDefaultStackValue))
	{
		DefaultPipelineStackName = NewDefaultStackValue;
		InterchangeProjectSettingsCDO->SaveConfig(); //This ensure the default pipeline stack name is save into the local config
	}
}

void SInterchangePipelineStacksTreeView::OnTreeViewSelectionChanged(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Item, ESelectInfo::Type SelectionType)
{
	if (SelectionMode.Get() == ESelectionMode::None)
	{
		return;
	}

	if (OnSelectionChangedDelegate.IsBound())
	{
		OnSelectionChangedDelegate.ExecuteIfBound(Item, SelectionType);
	}
}


/************************************************************************/
/* SInterchangePipelineConfigurationDialog Implementation                      */
/************************************************************************/

SInterchangePipelineConfigurationDialog::SInterchangePipelineConfigurationDialog()
{
	PipelineConfigurationTreeView = nullptr;
	PipelineConfigurationDetailsView = nullptr;
	OwnerWindow = nullptr;
}

SInterchangePipelineConfigurationDialog::~SInterchangePipelineConfigurationDialog()
{
	if (TSharedPtr<SWindow> OwnerWindowPinned = OwnerWindow.Pin())
	{
		OwnerWindowPinned->GetOnWindowClosedEvent().RemoveAll(this);
	}
}

TSharedRef<SBox> SInterchangePipelineConfigurationDialog::SpawnPipelineConfiguration()
{
	//Create the treeview
	PipelineConfigurationTreeView = SNew(SInterchangePipelineStacksTreeView)
		.OnSelectionChangedDelegate(this, &SInterchangePipelineConfigurationDialog::OnSelectionChanged)
		.SourceData(SourceData)
		.bSceneImport(bSceneImport)
		.bReimport(bReimport)
		.PipelineStack(PipelineStack);

	const float PipelineStackTreeViewWidth = GInterchangeEnableCustomPipelines ? 0.4f : 0.f;

	TSharedPtr<SBox> InspectorBox;
	TSharedRef<SBox> PipelineConfigurationPanelBox = SNew(SBox)
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		+ SSplitter::Slot()
		.Value(PipelineStackTreeViewWidth)
		.Resizable(GInterchangeEnableCustomPipelines)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoHeight()
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("FbxOptionWindow_Scene_ExpandAll", "Expand All"))
					.OnClicked(PipelineConfigurationTreeView.Get(), &SInterchangePipelineStacksTreeView::OnExpandAll)
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("FbxOptionWindow_Scene_CollapseAll", "Collapse All"))
					.OnClicked(PipelineConfigurationTreeView.Get(), &SInterchangePipelineStacksTreeView::OnCollapseAll)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBox)
				[
					PipelineConfigurationTreeView.ToSharedRef()
				]
			]
		]
		+ SSplitter::Slot()
		.Value(1.f - PipelineStackTreeViewWidth)
		[
			SAssignNew(InspectorBox, SBox)
		]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bShowSectionSelector = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	PipelineConfigurationDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBox->SetContent(PipelineConfigurationDetailsView->AsShared());
	PipelineConfigurationDetailsView->SetObject(nullptr);
	PipelineConfigurationDetailsView->GetIsPropertyVisibleDelegate().BindLambda([this](const FPropertyAndParent& PropertyAndParent)
		{
			return IsPropertyVisible(PropertyAndParent);
		});
	return PipelineConfigurationPanelBox;
}


void SInterchangePipelineConfigurationDialog::Construct(const FArguments& InArgs)
{
	//Make sure there is a valid default value

	OwnerWindow = InArgs._OwnerWindow;
	SourceData = InArgs._SourceData;
	bSceneImport = InArgs._bSceneImport;
	bReimport = InArgs._bReimport;
	PipelineStack = InArgs._PipelineStack;

	check(OwnerWindow.IsValid());
	if (TSharedPtr<SWindow> OwnerWindowPinned = OwnerWindow.Pin())
	{
		OwnerWindowPinned->GetOnWindowClosedEvent().AddRaw(this, &SInterchangePipelineConfigurationDialog::OnWindowClosed);
	}

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(10.0f, 3.0f))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2)
				[
					SNew(STextBlock)
					.Text(this, &SInterchangePipelineConfigurationDialog::GetSourceDescription)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(10, 2, 2, 2)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("InspectorGraphWindow_ResetDefault", "Reset to Default"))
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnResetToDefault)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2)
			[
				SpawnPipelineConfiguration()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0.f)
				.AutoWidth()
				[
					IDocumentation::Get()->CreateAnchor(FString("Engine/Content/Interchange/PipelineConfiguration"))
				]
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0.f)
				.AutoWidth()
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOCTEXT("InspectorGraphWindow_ReuseSettingsToolTip", "When importing multiple files, keep the same import settings for every file or open the settings dialog for each file."))
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(4.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InspectorGraphWindow_ReuseSettings", "Use the same settings for subsequent files"))
					]
					+ SHorizontalBox::Slot()
					.Padding(4.f, 0.f)
					[
						SAssignNew(UseSameSettingsForAllCheckBox, SCheckBox)
						.IsChecked(true)
						.IsEnabled(this, &SInterchangePipelineConfigurationDialog::IsImportButtonEnabled)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f) 
				[
					SNew(SPrimaryButton)
					.Text(LOCTEXT("InspectorGraphWindow_Import", "Import"))
					.ToolTipText(this, &SInterchangePipelineConfigurationDialog::GetImportButtonTooltip)
					.IsEnabled(this, &SInterchangePipelineConfigurationDialog::IsImportButtonEnabled)
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnCloseDialog, ECloseEventType::Import)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f, 0.f) 
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("InspectorGraphWindow_Cancel", "Cancel"))
					.OnClicked(this, &SInterchangePipelineConfigurationDialog::OnCloseDialog, ECloseEventType::Cancel)
				]
			]
		]
	];

	PipelineConfigurationTreeView->SelectDefaultItem();
}

void SInterchangePipelineConfigurationDialog::OnSelectionChanged(TSharedPtr<FInterchangePipelineStacksTreeNodeItem> Item, ESelectInfo::Type SelectionType)
{
	CurrentSelectedPipeline = nullptr;
	FName StackName = NAME_None;
	if (Item.IsValid())
	{
		CurrentSelectedPipeline = Item->Pipeline;
		StackName = Item->StackName;
	}
	PipelineConfigurationDetailsView->SetObject(CurrentSelectedPipeline.Get());
	CurrentStackName = StackName;
}

bool SInterchangePipelineConfigurationDialog::IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	if (bReimport)
	{
		const FName ReimportRestrictKey(TEXT("ReimportRestrict"));
		return !(PropertyAndParent.Property.GetBoolMetaData(ReimportRestrictKey));
	}
	return true;
}

FText SInterchangePipelineConfigurationDialog::GetSourceDescription() const
{
	FText ActionDescription;
	if (bReimport)
	{
		ActionDescription = LOCTEXT("GetSourceDescription_Reimport", "Reimport");
	}
	else
	{
		ActionDescription = LOCTEXT("GetSourceDescription_Import", "Import");
	}
	if (SourceData.IsValid())
	{
		ActionDescription = FText::Format(LOCTEXT("GetSourceDescription", "{0} source {1}"), ActionDescription, FText::FromString(SourceData->GetFilename()));
	}
	return ActionDescription;
}

void SInterchangePipelineConfigurationDialog::RecursiveIterateNode(TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, TFunctionRef<void(TSharedPtr<FInterchangePipelineStacksTreeNodeItem>&)> IterationLambda)
{
	IterationLambda(ParentNode);

	for (int32 ChildIndex = 0; ChildIndex < ParentNode->Childrens.Num(); ++ChildIndex)
	{
		RecursiveIterateNode(ParentNode->Childrens[ChildIndex], IterationLambda);
	}
}

FReply SInterchangePipelineConfigurationDialog::OnResetToDefault()
{
	FReply Result = FReply::Handled();
	TArray<TWeakObjectPtr<UObject>> SelectedPipelines = PipelineConfigurationDetailsView->GetSelectedObjects();
	if (CurrentStackName == NAME_None)
	{
		return Result;
	}
	//Multi selection is not allowed
	ensure(SelectedPipelines.Num() <= 1);
	for(TWeakObjectPtr<UObject> WeakObject : SelectedPipelines)
	{
		if (UInterchangePipelineBase* Pipeline = Cast<UInterchangePipelineBase>(WeakObject.Get()))
		{
			if (bReimport)
			{
				//When we re-import we save the settings before showing the dialog, so the reset to default simply reload
				//the original settings when the dialog was loaded.
				for (int32 PipelineIndex = 0; PipelineIndex < PipelineStack.Num(); ++PipelineIndex)
				{
					if (Pipeline == PipelineStack[PipelineIndex])
					{
						Pipeline->LoadSettings(CurrentStackName);
						PipelineConfigurationDetailsView->SetObject(Pipeline, true);
						break;
					}
				}
			}
			else
			{
				//The reset to default when doing a import is to duplicate again the pipeline stack reference asset.
				const TMap<FName, FInterchangePipelineStack>& DefaultPipelineStacks = FInterchangeProjectSettingsUtils::GetDefaultImportSettings(bSceneImport).PipelineStacks;
				//Find the stack node, so we can update the pipeline
				TSharedPtr<FInterchangePipelineStacksTreeNodeItem> StackNode;
				TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>& RootNodeArray = PipelineConfigurationTreeView->GetMutableRootNodeArray();
				for (TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& RootNode : RootNodeArray)
				{
					RecursiveIterateNode(RootNode, [this, &Pipeline, &DefaultPipelineStacks](TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& NodeItem)
					{
						if (NodeItem->StackName == CurrentStackName && NodeItem->Pipeline == Pipeline)
						{
							//We assume the pipelines inside one stack are all different classes, we use the class to know which default asset we need to duplicate
							const UClass* PipelineClass = NodeItem->Pipeline->GetClass();
							for (const TPair<FName, FInterchangePipelineStack>& NameAndPipelineStack : DefaultPipelineStacks)
							{
								if(CurrentStackName != NameAndPipelineStack.Key)
								{
									continue;
								}
								const FInterchangePipelineStack& InterchangePipelineStack = NameAndPipelineStack.Value;
								for (int32 PipelineIndex = 0; PipelineIndex < InterchangePipelineStack.Pipelines.Num(); ++PipelineIndex)
								{
									if (UInterchangePipelineBase* GeneratedPipeline = UE::Interchange::GeneratePipelineInstance(InterchangePipelineStack.Pipelines[PipelineIndex]))
									{
										if (GeneratedPipeline->GetClass() == PipelineClass)
										{
											NodeItem->Pipeline = GeneratedPipeline;
											GeneratedPipeline->AdjustSettingsForContext((bSceneImport ? EInterchangePipelineContext::SceneImport : EInterchangePipelineContext::AssetImport), nullptr);
											PipelineConfigurationDetailsView->SetObject(NodeItem->Pipeline, true);
											//Exit the lambda
											return;
										}
									}
								}
							}
						}
					});
				}
			}
		}
	}
	return Result;
}

bool SInterchangePipelineConfigurationDialog::RecursiveValidatePipelineSettings(const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, TOptional<FText>& OutInvalidReason) const
{
	if (ParentNode->Pipeline)
	{
		if (!ParentNode->Pipeline->IsSettingsAreValid(OutInvalidReason))
		{
			return false;
		}
	}
	for (int32 ChildIndex = 0; ChildIndex < ParentNode->Childrens.Num(); ++ChildIndex)
	{
		if (!RecursiveValidatePipelineSettings(ParentNode->Childrens[ChildIndex], OutInvalidReason))
		{
			return false;
		}
	}
	return true;
}

bool SInterchangePipelineConfigurationDialog::IsImportButtonEnabled() const
{
	const FName DefaultPipelineStackName = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bSceneImport, *SourceData.Get());
	const TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>& RootNodeArray = PipelineConfigurationTreeView->GetRootNodeArray();
	for (const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& RootNode : RootNodeArray)
	{
		//Reimport has only one stack and the name wont match DefaultPipelineStackName
		if (bReimport || RootNode->StackName == DefaultPipelineStackName)
		{
			TOptional<FText> InvalidReason;
			return RecursiveValidatePipelineSettings(RootNode, InvalidReason);
		}
	}
	return true;
}

FText SInterchangePipelineConfigurationDialog::GetImportButtonTooltip() const
{
	const FName DefaultPipelineStackName = FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(bSceneImport, *SourceData.Get());
	const TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>& RootNodeArray = PipelineConfigurationTreeView->GetRootNodeArray();
	for (const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& RootNode : RootNodeArray)
	{
		//Reimport have only one stack and the name wont match DefaultPipelineStackName
		if (bReimport || RootNode->StackName == DefaultPipelineStackName)
		{
			TOptional<FText> InvalidReason;
			if (!RecursiveValidatePipelineSettings(RootNode, InvalidReason) && InvalidReason.IsSet())
			{
				return InvalidReason.GetValue();
			}
		}
	}
	return FText();
}

void SInterchangePipelineConfigurationDialog::RecursiveSavePipelineSettings(const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, const int32 PipelineIndex) const
{
	if (ParentNode->Pipeline)
	{
		ParentNode->Pipeline->SaveSettings(ParentNode->StackName);
	}
	for (int32 ChildIndex = 0; ChildIndex < ParentNode->Childrens.Num(); ++ChildIndex)
	{
		RecursiveSavePipelineSettings(ParentNode->Childrens[ChildIndex], ChildIndex);
	}
}

void SInterchangePipelineConfigurationDialog::RecursiveLoadPipelineSettings(const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& ParentNode, const int32 PipelineIndex) const
{
	if (ParentNode->Pipeline)
	{
		ParentNode->Pipeline->LoadSettings(ParentNode->StackName);
	}
	for (int32 ChildIndex = 0; ChildIndex < ParentNode->Childrens.Num(); ++ChildIndex)
	{
		RecursiveLoadPipelineSettings(ParentNode->Childrens[ChildIndex], ChildIndex);
	}
}

void SInterchangePipelineConfigurationDialog::ClosePipelineConfiguration(const ECloseEventType CloseEventType)
{
	if (CloseEventType == ECloseEventType::Cancel || CloseEventType == ECloseEventType::WindowClosing)
	{
		bCanceled = true;
		bImportAll = false;
		if (bReimport && PipelineConfigurationTreeView)
		{
			//If user cancel the reimport we have to put back the pipeline settings
			const TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>& RootNodeArray = PipelineConfigurationTreeView->GetRootNodeArray();
			for (const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& RootNode : RootNodeArray)
			{
				RecursiveLoadPipelineSettings(RootNode, 0);
			}
		}
	}
	else //ECloseEventType::Import
	{
		bCanceled = false;
		bImportAll = UseSameSettingsForAllCheckBox->IsChecked();
	}

	if (!bReimport && PipelineConfigurationTreeView)
	{
		const TArray<TSharedPtr<FInterchangePipelineStacksTreeNodeItem>>& RootNodeArray = PipelineConfigurationTreeView->GetRootNodeArray();
		for (const TSharedPtr<FInterchangePipelineStacksTreeNodeItem>& RootNode : RootNodeArray)
		{
			RecursiveSavePipelineSettings(RootNode, 0);
		}
	}

	PipelineConfigurationTreeView = nullptr;
	PipelineConfigurationDetailsView = nullptr;

	if (CloseEventType != ECloseEventType::WindowClosing)
	{
		if (TSharedPtr<SWindow> OwnerWindowPin = OwnerWindow.Pin())
		{
			OwnerWindowPin->GetOnWindowClosedEvent().RemoveAll(this);
			OwnerWindowPin->RequestDestroyWindow();
		}
	}
	OwnerWindow = nullptr;
}

FReply SInterchangePipelineConfigurationDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!FApp::IsUnattended())
		{
			FString Message = FText(LOCTEXT("InterchangePipelineCancelEscKey", "Are you sure you want to cancel the import?")).ToString();
			if (FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *Message, TEXT("Cancel Import")) == EAppReturnType::Type::Yes)
			{
				return OnCloseDialog(ECloseEventType::Cancel);
			}
		}
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
