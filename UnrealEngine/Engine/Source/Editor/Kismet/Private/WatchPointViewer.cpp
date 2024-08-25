// Copyright Epic Games, Inc. All Rights Reserved.

#include "WatchPointViewer.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "K2Node_Event.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "GraphEditorActions.h"
#include "EdGraphSchema_K2.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "Editor.h"
#include "ToolMenus.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SHyperlink.h"

#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Kismet2/DebuggerCommands.h"

#include "KismetNodes/KismetNodeInfoContext.h"
#include "Stats/Stats.h"

#define LOCTEXT_NAMESPACE "WatchPointViewer"

namespace
{
	struct FWatchRow
	{
		FWatchRow(
			TWeakObjectPtr<UBlueprint> InBP,
			const UEdGraphNode* InNode,
			const UEdGraphPin* InPin,
			UObject* InObjectBeingDebugged,
			FText InBlueprintName,
			FText InGraphName,
			FText InNodeName,
			FText InDisplayName,
			FText InValue,
			FText InType
		)
			: BP(InBP)
			, Node(InNode)
			, Pin(InPin)
			, ObjectBeingDebugged(InObjectBeingDebugged)
			, BlueprintName(MoveTemp(InBlueprintName))
			, GraphName(MoveTemp(InGraphName))
			, NodeName(MoveTemp(InNodeName))
			, DisplayName(MoveTemp(InDisplayName))
			, Value(MoveTemp(InValue))
			, Type(MoveTemp(InType))
		{
			SetObjectBeingDebuggedName();

			UPackage* Package = Cast<UPackage>(BP.IsValid() ? BP->GetOuter() : nullptr);
			BlueprintPackageName = Package ? Package->GetFName() : FName();
		}

		FWatchRow(
			TWeakObjectPtr<UBlueprint> InBP,
			const UEdGraphNode* InNode,
			const UEdGraphPin* InPin,
			UObject* InObjectBeingDebugged,
			FText InBlueprintName,
			FText InGraphName,
			FText InNodeName,
			FPropertyInstanceInfo& Info
		)
			: BP(InBP)
			, Node(InNode)
			, Pin(InPin)
			, ObjectBeingDebugged(InObjectBeingDebugged)
			, BlueprintName(MoveTemp(InBlueprintName))
			, GraphName(MoveTemp(InGraphName))
			, NodeName(MoveTemp(InNodeName))
			, DisplayName(Info.DisplayName)
			, Value(Info.Value)
			, Type(Info.Type)
		{
			SetObjectBeingDebuggedName();

			UPackage* Package = Cast<UPackage>(BP.IsValid() ? BP->GetOuter() : nullptr);
			BlueprintPackageName = Package ? Package->GetFName() : FName();

			for (const TSharedPtr<FPropertyInstanceInfo>& ChildInfo : Info.GetChildren())
			{
				Children.Add(MakeShared<FWatchRow>(InBP, InNode, InPin, InObjectBeingDebugged, BlueprintName, GraphName, NodeName, *ChildInfo));
			}
		}

		// this can't be const because we store watches in the blueprint
		TWeakObjectPtr<UBlueprint> BP;
		const UEdGraphNode* Node;
		const UEdGraphPin* Pin;
		// this can't be const because SelectActor takes a non-const actor
		UObject* ObjectBeingDebugged;

		FText BlueprintName;
		FText ObjectBeingDebuggedName;
		FText GraphName;
		FText NodeName;
		FText DisplayName;
		FText Value;
		FText Type;
		FName BlueprintPackageName;

		TArray<TSharedRef<FWatchRow>> Children;

		// used for copying entries in the watch viewer
		FText GetTextForEntry() const
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ObjectName"), FText::FromString(ObjectBeingDebugged ? ObjectBeingDebugged->GetName() : TEXT("")));
			Args.Add(TEXT("BlueprintName"), BlueprintName);
			Args.Add(TEXT("GraphName"), GraphName);
			Args.Add(TEXT("NodeName"), NodeName);
			Args.Add(TEXT("DisplayName"), DisplayName);
			Args.Add(TEXT("Type"), Type);
			Args.Add(TEXT("Value"), Value);
			return FText::Format(LOCTEXT("WatchEntry", "{ObjectName}({BlueprintName}) {GraphName} {NodeName} {DisplayName}({Type}): {Value}"), Args);
		}

	private:
		void SetObjectBeingDebuggedName()
		{
			if (ObjectBeingDebugged != nullptr)
			{
				AActor* ActorBeingDebugged = Cast<AActor>(ObjectBeingDebugged);
				if (ActorBeingDebugged)
				{
					ObjectBeingDebuggedName = FText::AsCultureInvariant(ActorBeingDebugged->GetActorLabel());
				}
				else
				{
					ObjectBeingDebuggedName = FText::FromName(ObjectBeingDebugged->GetFName());
				}
			}
			else
			{
				ObjectBeingDebuggedName = BlueprintName;
			}
		}
	};

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisplayedWatchWindowChanged, TArray<TSharedRef<FWatchRow>>*);
	FOnDisplayedWatchWindowChanged WatchListSubscribers;

	// Proxy array of the watches. This allows us to manually refresh UI state when changes are made:
	TArray<TSharedRef<FWatchRow>> Private_WatchSource;
	TArray<TSharedRef<FWatchRow>> Private_InstanceWatchSource;

	TArray<TWeakObjectPtr<UBlueprint>> WatchedBlueprints;

	// Returns true if the blueprint execution is currently paused; false otherwise
	bool IsPaused()
	{
		return GUnrealEd && GUnrealEd->PlayWorld && GUnrealEd->PlayWorld->bDebugPauseExecution;
	}

	void UpdateNonInstancedWatchDisplay()
	{
		Private_WatchSource.Reset();

		for (TWeakObjectPtr<UBlueprint> BlueprintObj : WatchedBlueprints)
		{
			if (!BlueprintObj.IsValid())
			{
				continue;
			}
			FText BlueprintName = FText::FromString(BlueprintObj->GetName());

			FKismetDebugUtilities::ForeachPinWatch(
				BlueprintObj.Get(),
				[BlueprintObj, BlueprintName](UEdGraphPin* Pin)
				{
					FText GraphName = FText::FromString(Pin->GetOwningNode()->GetGraph()->GetName());
					FText NodeName = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);

					const UEdGraphSchema* Schema = Pin->GetOwningNode()->GetSchema();

					TSharedPtr<FPropertyInstanceInfo> DebugInfo;
					DebugInfo->DisplayName = Schema->GetPinDisplayName(Pin);
					DebugInfo->Type = UEdGraphSchema_K2::TypeToText(Pin->PinType);
					DebugInfo->Value = LOCTEXT("ExecutionNotPaused", "(execution not paused)");

					Private_WatchSource.Add(
						MakeShared<FWatchRow>(
							BlueprintObj,
							Pin->GetOwningNode(),
							Pin,
							nullptr,
							BlueprintName,
							MoveTemp(GraphName),
							MoveTemp(NodeName),
							*DebugInfo
						)
					);
				}
			);
		}
	}

	void UpdateWatchListFromBlueprintImpl(TWeakObjectPtr<UBlueprint> BlueprintObj, const bool bShouldWatch)
	{
		if (bShouldWatch)
		{
			// make sure the blueprint is in our list
			WatchedBlueprints.AddUnique(BlueprintObj);
		}
		else
		{
			// if this blueprint shouldn't be watched and we aren't watching it already then there is nothing to do
			int32 FoundIdx = WatchedBlueprints.Find(BlueprintObj);
			if (FoundIdx == INDEX_NONE)
			{
				// if we didn't find it, it could be because BlueprintObj is no longer valid
				// in this case the pointer in WatchedBlueprints would also be invalid
				bool bRemovedBP = false;
				for (int32 Idx = 0; Idx < WatchedBlueprints.Num(); ++Idx)
				{
					if (!WatchedBlueprints[Idx].IsValid())
					{
						bRemovedBP = true;
						WatchedBlueprints.RemoveAt(Idx);
						--Idx;
					}
				}

				if (!bRemovedBP)
				{
					return;
				}
			}
			else
			{
				// since we're not watching the blueprint anymore we should remove it from the watched list
				WatchedBlueprints.RemoveAt(FoundIdx);
			}
		}

		// something changed so we need to update the lists shown in the UI
		UpdateNonInstancedWatchDisplay();

		if (IsPaused())
		{
#ifndef WATCH_VIEWER_DEPRECATED
			WatchViewer::UpdateInstancedWatchDisplay();
#endif
		}

		// Notify subscribers:
		WatchListSubscribers.Broadcast(&Private_WatchSource);
	}

	// Updates all of the watches from the currently watched blueprints
	void UpdateAllBlueprintWatches()
	{
		for (TWeakObjectPtr<UBlueprint> Blueprint : WatchedBlueprints)
		{
			UpdateWatchListFromBlueprintImpl(Blueprint, true);
		}
	}
};

/**
* Widget that visualizes the contents of a FWatchRow.
*/
class SWatchTreeWidgetItem : public SMultiColumnTableRow<TSharedRef<FWatchRow>>
{
public:

	SLATE_BEGIN_ARGS(SWatchTreeWidgetItem)
		: _WatchToVisualize()
	{ }

	SLATE_ARGUMENT(TSharedPtr<FWatchRow>, WatchToVisualize)

	SLATE_END_ARGS()

public:

	/**
	* Construct child widgets that comprise this widget.
	*
	* @param InArgs Declaration from which to construct this widget.
	*/
	void Construct(const FArguments& InArgs, class SWatchViewer* InOwner, const TSharedRef<STableViewBase>& InOwnerTableView);

public:

	// SMultiColumnTableRow overrides
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

protected:
	FText GetDebuggedObjectName() const
	{
		return WatchRow->ObjectBeingDebuggedName;
	}

	FText GetBlueprintName() const
	{
		return WatchRow->BlueprintName;
	}

	FText GetGraphName() const
	{
		return WatchRow->GraphName;
	}

	FText GetNodeName() const
	{
		return WatchRow->NodeName;
	}

	FText GetVariableName() const
	{
		return WatchRow->DisplayName;
	}

	FText GetValue() const
	{
		return WatchRow->Value;
	}

	FText GetType() const
	{
		return WatchRow->Type;
	}

	void HandleHyperlinkDebuggedObjectNavigate() const
	{
		if (AActor* Actor = Cast<AActor>(WatchRow.IsValid() ? WatchRow->ObjectBeingDebugged : nullptr))
		{
			// unselect whatever was selected
			GEditor->SelectNone(false, false, false);

			// select the actor we care about
			GEditor->SelectActor(Actor, true, true, true);
		}
	}

	EVisibility DisplayDebuggedObjectAsHyperlink() const
	{
		if ( AActor* Actor = Cast<AActor>(WatchRow.IsValid() ? WatchRow->ObjectBeingDebugged : nullptr))
		{
			return EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	}

	EVisibility DisplayDebuggedObjectAsText() const
	{
		if (AActor* Actor = Cast<AActor>(WatchRow.IsValid() ? WatchRow->ObjectBeingDebugged : nullptr))
		{
			return EVisibility::Collapsed;
		}

		return EVisibility::Visible;
	}

	void HandleHyperlinkNodeNavigate() const
	{
		if (WatchRow.IsValid() && WatchRow->Node)
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(WatchRow->Node);
		}
	}

private:

	/** The info about the widget that we are visualizing. */
	TSharedPtr<FWatchRow> WatchRow;

	SWatchViewer* Owner;
};

typedef STreeView<TSharedRef<FWatchRow>> SWatchTree;

class SWatchViewer : public SCompoundWidget
{
	friend class FBlueprintEditor;
public:
	SLATE_BEGIN_ARGS(SWatchViewer){}
	SLATE_END_ARGS()

	SWatchViewer()
	{
		// make sure we have the latest information about the watches on loaded blueprints
		UpdateAllBlueprintWatches();

		FKismetDebugUtilities::WatchedPinsListChangedEvent.AddRaw(this, &SWatchViewer::HandleWatchedPinsChanged);
		FEditorDelegates::ResumePIE.AddRaw(this, &SWatchViewer::HandleResumePIE);
		FEditorDelegates::EndPIE.AddRaw(this, &SWatchViewer::HandleEndPIE);

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &SWatchViewer::HandleAssetRemoved);
		AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &SWatchViewer::HandleAssetRenamed);
	}
	~SWatchViewer()
	{
		FKismetDebugUtilities::WatchedPinsListChangedEvent.RemoveAll(this);
		FEditorDelegates::ResumePIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);

		if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
		{
			IAssetRegistry* AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).TryGet();
			if (AssetRegistry)
			{
				AssetRegistry->OnAssetRemoved().RemoveAll(this);
				AssetRegistry->OnAssetRenamed().RemoveAll(this);
			}
		}
	}

	void Construct(const FArguments& InArgs, TArray<TSharedRef<FWatchRow>>* InWatchSource);
	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FWatchRow> InWatchRow, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleGetChildren(TSharedRef<FWatchRow> InWatchRow, TArray<TSharedRef<FWatchRow>>& OutChildren);
	void HandleWatchedPinsChanged(UBlueprint* BlueprintObj);
	void HandleResumePIE(bool);
	void HandleEndPIE(bool);
	void HandleAssetRemoved(const FAssetData& InAssetData);
	void HandleAssetRenamed(const FAssetData& InAssetData, const FString& InOldName);
	void UpdateWatches(TArray<TSharedRef<FWatchRow>>* WatchValues);
	void CopySelectedRows() const;
	void StopWatchingPin() const;

	TSharedPtr<SWatchTree> WatchTreeWidget;
	TArray<TSharedRef<FWatchRow>>* WatchSource;

	TSharedPtr< FUICommandList > CommandList;

private:
	void CopySelectedRowsHelper(const TArray<TSharedRef<FWatchRow>>& RowSource, FString& StringToCopy) const;
};

void SWatchViewer::Construct(const FArguments& InArgs, TArray<TSharedRef<FWatchRow>>* InWatchSource)
{
	CommandList = MakeShareable( new FUICommandList );
	CommandList->MapAction( 
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP( this, &SWatchViewer::CopySelectedRows ),
		// we need to override the default 'can execute' because we want to be available during debugging:
		FCanExecuteAction::CreateStatic( [](){ return true; } )
	);

	CommandList->MapAction(
		FGraphEditorCommands::Get().StopWatchingPin,
		FExecuteAction::CreateSP(this, &SWatchViewer::StopWatchingPin),
		FCanExecuteAction::CreateStatic([]() { return true; })
	);

	WatchSource = InWatchSource;

	const auto ContextMenuOpened = [](TWeakPtr<FUICommandList> InCommandList, TWeakPtr<SWatchViewer> ControlOwnerWeak) -> TSharedPtr<SWidget>
	{
		const bool CloseAfterSelection = true;
		FMenuBuilder MenuBuilder( CloseAfterSelection, InCommandList.Pin() );
		MenuBuilder.AddMenuEntry(FGraphEditorCommands::Get().StopWatchingPin);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Copy);
		return MenuBuilder.MakeWidget();
	};

	const auto EmptyWarningVisibility = [](TWeakPtr<SWatchViewer> ControlOwnerWeak) -> EVisibility
	{
		TSharedPtr<SWatchViewer> ControlOwner = ControlOwnerWeak.Pin();
		if (ControlOwner.IsValid() &&
			ControlOwner->WatchSource &&
			ControlOwner->WatchSource->Num() > 0)
		{
			return EVisibility::Hidden;
		}
		return EVisibility::Visible;
	};

	const auto WatchViewIsEnabled = [](TWeakPtr<SWatchViewer> ControlOwnerWeak) -> bool
	{
		TSharedPtr<SWatchViewer> ControlOwner = ControlOwnerWeak.Pin();
		if (ControlOwner.IsValid() &&
			ControlOwner->WatchSource &&
			ControlOwner->WatchSource->Num() > 0)
		{
			return true;
		}
		return false;
	};

	// Cast due to TSharedFromThis inheritance issues:
	TSharedRef<SWatchViewer> SelfTyped = StaticCastSharedRef<SWatchViewer>(AsShared());
	TWeakPtr<SWatchViewer> SelfWeak = SelfTyped;
	TWeakPtr<FUICommandList> CommandListWeak = CommandList;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(4.0f)
		.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SAssignNew(WatchTreeWidget, SWatchTree)
				.ItemHeight(25.0f)
				.TreeItemsSource(WatchSource)
				.OnGenerateRow(this, &SWatchViewer::HandleGenerateRow)
				.OnGetChildren(this, &SWatchViewer::HandleGetChildren)
				.OnContextMenuOpening(FOnContextMenuOpening::CreateStatic(ContextMenuOpened, CommandListWeak, SelfWeak))
				.IsEnabled(
					TAttribute<bool>::Create(
						TAttribute<bool>::FGetter::CreateStatic(WatchViewIsEnabled, SelfWeak)
					)
				)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+SHeaderRow::Column(TEXT("ObjectName"))
					.FillWidth(.2f)
					.VAlignHeader(VAlign_Center)
					.DefaultLabel(LOCTEXT("ObjectName", "Object Name"))
					.DefaultTooltip(LOCTEXT("ObjectNameTooltip", "Name of the object instance being debugged or the blueprint if there is no object being debugged"))
					+ SHeaderRow::Column(TEXT("GraphName"))
					.FillWidth(.2f)
					.VAlignHeader(VAlign_Center)
					.DefaultLabel(LOCTEXT("GraphName", "Graph Name"))
					.DefaultTooltip(LOCTEXT("GraphNameTooltip", "Name of the source blueprint graph for this variable"))
					+ SHeaderRow::Column(TEXT("NodeName"))
					.FillWidth(.3f)
					.VAlignHeader(VAlign_Center)
					.DefaultLabel(LOCTEXT("NodeName", "Node Name"))
					.DefaultTooltip(LOCTEXT("NodeNameTooltip", "Name of the source blueprint graph node for this variable"))
					+ SHeaderRow::Column(TEXT("VariableName"))
					.FillWidth(.3f)
					.VAlignHeader(VAlign_Center)
					.DefaultLabel(LOCTEXT("VariableName", "Variable Name"))
					.DefaultTooltip(LOCTEXT("VariabelNameTooltip", "Name of the variable"))
					+ SHeaderRow::Column(TEXT("Value"))
					.FillWidth(.8f)
					.VAlignHeader(VAlign_Center)
					.DefaultLabel(LOCTEXT("Value", "Value"))
					.DefaultTooltip(LOCTEXT("ValueTooltip", "Current value of this variable"))
				)
			]
			+SOverlay::Slot()
			.Padding(32.f)
			[
				SNew(STextBlock)
				.Text(
					LOCTEXT("NoWatches", "No watches to display")
				)
				.Justification(ETextJustify::Center)
				.Visibility(
					TAttribute<EVisibility>::Create(
						TAttribute<EVisibility>::FGetter::CreateStatic(EmptyWarningVisibility, SelfWeak)
					)
				)
			]
		]
	];

	WatchListSubscribers.AddSP(StaticCastSharedRef<SWatchViewer>(AsShared()), &SWatchViewer::UpdateWatches);
}

TSharedRef<ITableRow> SWatchViewer::HandleGenerateRow(TSharedRef<FWatchRow> InWatchRow, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SWatchTreeWidgetItem, this, OwnerTable)
		.WatchToVisualize(InWatchRow);
}

void SWatchViewer::HandleGetChildren(TSharedRef<FWatchRow> InWatchRow, TArray<TSharedRef<FWatchRow>>& OutChildren)
{
	OutChildren = InWatchRow->Children;
}

void SWatchViewer::HandleWatchedPinsChanged(UBlueprint* BlueprintObj)
{
#ifndef WATCH_VIEWER_DEPRECATED
	WatchViewer::UpdateWatchListFromBlueprint(BlueprintObj);
#endif
}

void SWatchViewer::HandleResumePIE(bool)
{
#ifndef WATCH_VIEWER_DEPRECATED
	// swap to displaying the unpaused watches
	WatchViewer::ContinueExecution();
#endif
}

void SWatchViewer::HandleEndPIE(bool)
{
#ifndef WATCH_VIEWER_DEPRECATED
	// show the unpaused watches in case we stopped PIE while at a breakpoint
	WatchViewer::ContinueExecution();
#endif
}

void SWatchViewer::HandleAssetRemoved(const FAssetData& InAssetData)
{
#ifndef WATCH_VIEWER_DEPRECATED
	WatchViewer::RemoveWatchesForAsset(InAssetData);
#endif
}

void SWatchViewer::HandleAssetRenamed(const FAssetData& InAssetData, const FString& InOldName)
{
#ifndef WATCH_VIEWER_DEPRECATED
	WatchViewer::OnRenameAsset(InAssetData, InOldName);
#endif
}

void SWatchViewer::UpdateWatches(TArray<TSharedRef<FWatchRow>>* Watches)
{
	WatchSource = Watches;
	WatchTreeWidget->SetTreeItemsSource(Watches);
}

void SWatchViewer::CopySelectedRowsHelper(const TArray<TSharedRef<FWatchRow>>& RowSource, FString& StringToCopy) const
{
	for (const TSharedRef<FWatchRow>& Item : RowSource)
	{
		if (WatchTreeWidget->IsItemSelected(Item))
		{
			StringToCopy.Append(Item->GetTextForEntry().ToString());
			StringToCopy.Append(LINE_TERMINATOR);
		}

		CopySelectedRowsHelper(Item->Children, StringToCopy);
	}
}

void SWatchViewer::CopySelectedRows() const
{
	FString StringToCopy;

	// We want to copy in the order displayed, not the order selected, so iterate the list and build up the string:
	if (WatchSource)
	{
		CopySelectedRowsHelper(*WatchSource, StringToCopy);
	}

	if (!StringToCopy.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*StringToCopy);
	}
}

void SWatchViewer::StopWatchingPin() const
{
	TArray<TSharedRef<FWatchRow>> SelectedRows = WatchTreeWidget->GetSelectedItems();
	for (TSharedRef<FWatchRow>& Row : SelectedRows)
	{
		FKismetDebugUtilities::TogglePinWatch(Row->BP.Get(), Row->Pin);
	}
}

void SWatchTreeWidgetItem::Construct(const FArguments& InArgs, SWatchViewer* InOwner, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	this->WatchRow = InArgs._WatchToVisualize;
	Owner = InOwner;

	SMultiColumnTableRow<TSharedRef<FWatchRow>>::Construct(SMultiColumnTableRow<TSharedRef<FWatchRow>>::FArguments().Padding(1.0f), InOwnerTableView);
}

TSharedRef<SWidget> SWatchTreeWidgetItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	const static FName NAME_ObjectName(TEXT("ObjectName"));
	const static FName NAME_GraphName(TEXT("GraphName"));
	const static FName NAME_NodeName(TEXT("NodeName"));
	const static FName NAME_VariableName(TEXT("VariableName"));
	const static FName NAME_Value(TEXT("Value"));

	if (ColumnName == NAME_ObjectName)
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 1.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SHyperlink)
					.Text(this, &SWatchTreeWidgetItem::GetDebuggedObjectName)
					.ToolTipText(this, &SWatchTreeWidgetItem::GetBlueprintName)
					.OnNavigate(this, &SWatchTreeWidgetItem::HandleHyperlinkDebuggedObjectNavigate)
					.Visibility(this, &SWatchTreeWidgetItem::DisplayDebuggedObjectAsHyperlink)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(this, &SWatchTreeWidgetItem::GetDebuggedObjectName)
					.ToolTipText(this, &SWatchTreeWidgetItem::GetBlueprintName)
					.Visibility(this, &SWatchTreeWidgetItem::DisplayDebuggedObjectAsText)
				]
			];
	}
	else if (ColumnName == NAME_GraphName)
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 1.0f))
			[
				SNew(STextBlock)
				.Text(this, &SWatchTreeWidgetItem::GetGraphName)
			];
	}
	else if (ColumnName == NAME_NodeName)
	{
		FString Comment; 
		if (WatchRow->Node->NodeComment.Len() > 0)
		{
			Comment = TEXT("\n\n");
			Comment.Append(WatchRow->Node->NodeComment);
		}
		FText TooltipText = FText::Format(LOCTEXT("NodeTooltip", "Find the {0} node in the blueprint graph.{1}"), GetNodeName(), FText::FromString(Comment));
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 1.0f))
			[
				SNew(SHyperlink)
				.Text(this, &SWatchTreeWidgetItem::GetNodeName)
				.ToolTipText(TooltipText)
				.OnNavigate(this, &SWatchTreeWidgetItem::HandleHyperlinkNodeNavigate)
			];
	}
	else if (ColumnName == NAME_VariableName)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SExpanderArrow, SharedThis(this))
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SWatchTreeWidgetItem::GetVariableName)
				.ToolTipText(this, &SWatchTreeWidgetItem::GetType)
			];
	}
	else if (ColumnName == NAME_Value)
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 1.0f))
			[
				SNew(STextBlock)
				.Text(this, &SWatchTreeWidgetItem::GetValue)
			];
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

void WatchViewer::UpdateInstancedWatchDisplay()
{
#if DO_BLUEPRINT_GUARD
	{
		Private_InstanceWatchSource.Reset();
		TArrayView<const FFrame* const> ScriptStack = FBlueprintContextTracker::Get().GetCurrentScriptStack();

		TSet<const UBlueprint*> SeenBlueprints;

		for (const FFrame* ScriptFrame : ScriptStack)
		{
			UObject* BlueprintInstance = ScriptFrame ? ScriptFrame->Object : nullptr;
			UClass* Class = BlueprintInstance ? BlueprintInstance->GetClass() : nullptr;
			UBlueprint* BlueprintObj = (Class ? Cast<UBlueprint>(Class->ClassGeneratedBy) : nullptr);
			if (BlueprintObj == nullptr)
			{
				continue;
			}

			// Only add watchpoints from each blueprint once
			if (SeenBlueprints.Contains(BlueprintObj))
			{
				continue;
			}
			SeenBlueprints.Add(BlueprintObj);

			FText BlueprintName = FText::FromString(BlueprintObj->GetName());

			// Don't show info for the CDO
			if (BlueprintInstance->IsTemplate())
			{
				continue;
			}

			// Don't show info if this instance is pending kill
			if (!IsValid(BlueprintInstance))
			{
				continue;
			}

			// Don't show info if this instance isn't in the current world
			UObject* ObjOuter = BlueprintInstance;
			UWorld* ObjWorld = nullptr;
			static bool bUseNewWorldCode = false;
			do		// Run through at least once in case the TestObject is a UGameInstance
			{
				UGameInstance* ObjGameInstance = Cast<UGameInstance>(ObjOuter);

				ObjOuter = ObjOuter->GetOuter();
				ObjWorld = ObjGameInstance ? ObjGameInstance->GetWorld() : Cast<UWorld>(ObjOuter);
			} while (ObjWorld == nullptr && ObjOuter != nullptr);

			if (ObjWorld)
			{
				// Make check on owning level (not streaming level)
				if (ObjWorld->PersistentLevel && ObjWorld->PersistentLevel->OwningWorld)
				{
					ObjWorld = ObjWorld->PersistentLevel->OwningWorld;
				}

				if (ObjWorld->WorldType != EWorldType::PIE && !((ObjWorld->WorldType == EWorldType::Editor) && (GUnrealEd->GetPIEViewport() == nullptr)))
				{
					continue;
				}
			}

			// We have a valid instance, iterate over all the watched pins and create rows for them
			FKismetDebugUtilities::ForeachPinWatch(
				BlueprintObj,
				[BlueprintObj, BlueprintInstance, BlueprintName](UEdGraphPin* Pin)
				{
					FText GraphName = FText::FromString(Pin->GetOwningNode()->GetGraph()->GetName());
					FText NodeName = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);

					TSharedPtr<FPropertyInstanceInfo> DebugInfo;
					const FKismetDebugUtilities::EWatchTextResult WatchStatus = FKismetDebugUtilities::GetDebugInfo(DebugInfo, BlueprintObj, BlueprintInstance, Pin);

					if (WatchStatus != FKismetDebugUtilities::EWTR_Valid)
					{
						const UEdGraphSchema* Schema = Pin->GetOwningNode()->GetSchema();
						DebugInfo->DisplayName = Schema->GetPinDisplayName(Pin);
						DebugInfo->Type = UEdGraphSchema_K2::TypeToText(Pin->PinType);

						switch (WatchStatus)
						{
						case FKismetDebugUtilities::EWTR_NotInScope:
							DebugInfo->Value = LOCTEXT("NotInScope", "(not in scope)");
							break;

						case FKismetDebugUtilities::EWTR_NoProperty:
							DebugInfo->Value = LOCTEXT("NoDebugData", "(no debug data)");
							break;

						case FKismetDebugUtilities::EWTR_NoDebugObject:
							DebugInfo->Value = LOCTEXT("NoDebugObject", "(no debug object)");
							break;

						default:
							// do nothing
							break;
						}
					}

					Private_InstanceWatchSource.Add(
						MakeShared<FWatchRow>(
							BlueprintObj,
							Pin->GetOwningNode(),
							Pin,
							BlueprintInstance,
							BlueprintName,
							GraphName,
							NodeName,
							*DebugInfo
							)
					);
				}
			);
		}

		// Notify subscribers:
		WatchListSubscribers.Broadcast(&Private_InstanceWatchSource);
	}
#endif
}

void WatchViewer::ContinueExecution()
{
	// Notify subscribers:
	WatchListSubscribers.Broadcast(&Private_WatchSource);
}

FName WatchViewer::GetTabName()
{
	const FName TabName = TEXT("WatchViewer");
	return TabName;
}

void WatchViewer::RemoveWatchesForBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj)
{
	if (!ensure(BlueprintObj.IsValid()))
	{
		return;
	}

	int32 FoundIdx = WatchedBlueprints.Find(BlueprintObj);
	if (FoundIdx == INDEX_NONE)
	{
		return;
	}

	// since we're not watching any pins anymore we should remove it from the watched list
	WatchedBlueprints.RemoveAt(FoundIdx);

	// something changed so we need to update the lists shown in the UI
	UpdateNonInstancedWatchDisplay();

	if (IsPaused())
	{
		WatchViewer::UpdateInstancedWatchDisplay();
	}

	// Notify subscribers
	WatchListSubscribers.Broadcast(&Private_WatchSource);
}

void WatchViewer::RemoveWatchesForAsset(const struct FAssetData& AssetData)
{
	for (TSharedRef<FWatchRow> WatchRow : Private_WatchSource)
	{
		if (AssetData.PackageName == WatchRow->BlueprintPackageName && FText::FromName(AssetData.AssetName).EqualTo(WatchRow->BlueprintName))
		{
			RemoveWatchesForBlueprint(WatchRow->BP);
			break;
		}
	}
}

void WatchViewer::OnRenameAsset(const struct FAssetData& AssetData, const FString& OldAssetName)
{
	FString OldPackageName;
	FString OldBPName;

	if (OldAssetName.Split(".", &OldPackageName, &OldBPName))
	{
		bool bUpdated = false;

		for (TSharedRef<FWatchRow> WatchRow : Private_WatchSource)
		{
			if (OldPackageName == WatchRow->BlueprintPackageName.ToString() && FText::FromString(OldBPName).EqualTo(WatchRow->BlueprintName))
			{
				WatchRow->BlueprintName = FText::FromName(AssetData.AssetName);
				bUpdated = true;
			}
		}
	
		if (bUpdated)
		{
			// something changed so we need to update the lists shown in the UI
			UpdateNonInstancedWatchDisplay();

			if (IsPaused())
			{
				WatchViewer::UpdateInstancedWatchDisplay();
			}

			// Notify subscribers if necessary
			WatchListSubscribers.Broadcast(&Private_WatchSource);
		}
	}
}

void WatchViewer::UpdateWatchListFromBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj)
{
	UpdateWatchListFromBlueprintImpl(BlueprintObj, true);
}

void WatchViewer::ClearWatchListFromBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj)
{
	UpdateWatchListFromBlueprintImpl(BlueprintObj, false);
}

void WatchViewer::RegisterTabSpawner(FTabManager& TabManager)
{
	const auto SpawnWatchViewTab = []( const FSpawnTabArgs& Args )
	{
		TArray<TSharedRef<FWatchRow>>* Source = &Private_WatchSource;
		if (IsPaused())
		{
			Source = &Private_InstanceWatchSource;
		}
		
		static const FName ToolbarName = TEXT("Kismet.DebuggingViewToolBar");
		FToolMenuContext MenuContext(FPlayWorldCommands::GlobalPlayWorldActions);
		TSharedRef<SWidget> ToolbarWidget = UToolMenus::Get()->GenerateWidget(ToolbarName, MenuContext);

		return SNew(SDockTab)
			.TabRole( ETabRole::PanelTab )
			.Label( LOCTEXT("TabTitle", "Watches") )
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage( FAppStyle::GetBrush( TEXT("NoBorder") ) )
					[
						ToolbarWidget
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage( FAppStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
					[
						SNew(SWatchViewer, Source)
					]
				]
			];
	};
	
	TabManager.RegisterTabSpawner( WatchViewer::GetTabName(), FOnSpawnTab::CreateStatic(SpawnWatchViewTab) )
		.SetDisplayName( LOCTEXT("SpawnerTitle", "Watch Window") )
		.SetTooltipText( LOCTEXT("SpawnerTooltipText", "Open the watch window tab") );
}

#undef LOCTEXT_NAMESPACE