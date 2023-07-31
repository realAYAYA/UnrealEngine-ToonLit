// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfiguratorClusterUtils.h"

#include "DisplayClusterConfiguratorBlueprintEditor.h"
#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfiguratorPropertyUtils.h"
#include "DisplayClusterConfiguratorStyle.h"
#include "DisplayClusterConfiguratorUtils.h"
#include "ClusterConfiguration/SDisplayClusterConfiguratorNewClusterItemDialog.h"
#include "ClusterConfiguration/ViewModels/DisplayClusterConfiguratorClusterNodeViewModel.h"
#include "Views/DragDrop/DisplayClusterConfiguratorClusterNodeDragDropOp.h"
#include "Views/DragDrop/DisplayClusterConfiguratorViewportDragDropOp.h"

#include "Factories.h"
#include "ISinglePropertyView.h"
#include "ObjectTools.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UnrealExporter.h"
#include "Exporters/Exporter.h"
#include "Input/DragAndDrop.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FDisplayClusterConfiguratorClusterUtils"

const FVector2D FDisplayClusterConfiguratorClusterUtils::NewClusterItemDialogSize = FVector2D(410, 512);
const FString FDisplayClusterConfiguratorClusterUtils::DefaultNewHostName = TEXT("Host");
const FString FDisplayClusterConfiguratorClusterUtils::DefaultNewClusterNodeName = TEXT("Node");
const FString FDisplayClusterConfiguratorClusterUtils::DefaultNewViewportName = TEXT("VP");

using namespace DisplayClusterConfiguratorPropertyUtils;

UDisplayClusterConfigurationClusterNode* FDisplayClusterConfiguratorClusterUtils::CreateNewClusterNodeFromDialog(const TSharedRef<FDisplayClusterConfiguratorBlueprintEditor>& Toolkit, UDisplayClusterConfigurationCluster* Cluster, const FDisplayClusterConfigurationRectangle& PresetRect, FString PresetHost)
{
	UDisplayClusterConfigurationClusterNode* NodeTemplate = NewObject<UDisplayClusterConfigurationClusterNode>(Toolkit->GetBlueprintObj());
	NodeTemplate->WindowRect = FDisplayClusterConfigurationRectangle(PresetRect);
	NodeTemplate->Host = PresetHost;

	TArray<FString> ParentItems;
	ParentItems.Add("Cluster");

	const FString InitialName = GetUniqueNameForClusterNode(DefaultNewClusterNodeName, Cluster, true);
	
	bool bAutoposition = true;
	bool bAddViewport = true;
	TSharedRef<SWidget> ClusterNodeFooter = SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([&bAutoposition]() { return bAutoposition ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([&bAutoposition](ECheckBoxState NewState) { bAutoposition = NewState == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("AddNewClusterNode_AutoPositionToolTip", "When checked, auto-positions the new cluster node to prevent overlap with other cluster nodes in the same host"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddNewClusterNode_AutoPositionLabel", "Adjust Cluster Node Position to Prevent Overlap"))
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([&bAddViewport]() { return bAddViewport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([&bAddViewport](ECheckBoxState NewState) { bAddViewport = NewState == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("AddNewClusterNode_AddViewportToolTip", "When checked, adds a new viewport to the cluster node when created that matches the size of the Node application window"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddNewClusterNode_AddViewportLabel", "Add Viewport to New Cluster Node"))
			]
		];

	TSharedRef<SDisplayClusterConfiguratorNewClusterItemDialog> DialogContent = SNew(SDisplayClusterConfiguratorNewClusterItemDialog, NodeTemplate)
		.ParentItemOptions(ParentItems)
		.InitiallySelectedParentItem("Cluster")
		.PresetItemOptions(FDisplayClusterConfiguratorPresetSize::CommonPresets)
		.InitiallySelectedPreset(FDisplayClusterConfiguratorPresetSize::CommonPresets[FDisplayClusterConfiguratorPresetSize::DefaultPreset])
		.InitialName(InitialName)
		.MaxWindowWidth(NewClusterItemDialogSize.X)
		.MaxWindowHeight(NewClusterItemDialogSize.Y)
		.FooterContent(ClusterNodeFooter)
		.OnPresetChanged_Lambda([=](FVector2D Size) { NodeTemplate->WindowRect.W = Size.X; NodeTemplate->WindowRect.H = Size.Y; });

	ShowNewClusterItemDialogWindow(DialogContent, nullptr, LOCTEXT("AddNewClusterNode_DialogTitle", "Add New Cluster Node"), NewClusterItemDialogSize);

	UDisplayClusterConfigurationClusterNode* NewNode = nullptr;

	if (DialogContent->WasAccepted())
	{
		const FString ItemName = DialogContent->GetItemName();
		NodeTemplate->SetFlags(RF_Transactional);

		if (bAutoposition)
		{
			FVector2D DesiredPosition = FVector2D(NodeTemplate->WindowRect.X, NodeTemplate->WindowRect.Y);
			FVector2D DesiredSize = FVector2D(NodeTemplate->WindowRect.W, NodeTemplate->WindowRect.H);
			FVector2D NewPosition = FindNextAvailablePositionForClusterNode(Cluster, NodeTemplate->Host, DesiredPosition, DesiredSize);

			NodeTemplate->WindowRect.X = NewPosition.X;
			NodeTemplate->WindowRect.Y = NewPosition.Y;
		}

		NewNode = AddClusterNodeToCluster(NodeTemplate, Cluster, ItemName);

		// If the newly added cluster node is the only node in the cluster, it should be the primary node, so set it as the primary
		if (Cluster->Nodes.Num() == 1)
		{
			SetClusterNodeAsPrimary(NewNode);
		}

		if (bAddViewport)
		{
			const FString ViewportName = GetUniqueNameForViewport(DefaultNewViewportName, NewNode, true);
			UDisplayClusterConfigurationViewport* NewViewport = NewObject<UDisplayClusterConfigurationViewport>(NewNode, NAME_None, RF_Transactional | RF_ArchetypeObject | RF_Public);
			NewViewport->Region.W = NewNode->WindowRect.W;
			NewViewport->Region.H = NewNode->WindowRect.H;

			NewViewport = AddViewportToClusterNode(NewViewport, NewNode, ViewportName);
		}
	}

	// Clean up the template object so that it gets GCed once this schema action has been destroyed
	NodeTemplate->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	NodeTemplate->SetFlags(RF_Transient);

	return NewNode;
}

UDisplayClusterConfigurationViewport* FDisplayClusterConfiguratorClusterUtils::CreateNewViewportFromDialog(const TSharedRef<FDisplayClusterConfiguratorBlueprintEditor>& Toolkit, UDisplayClusterConfigurationClusterNode* ClusterNode, const FDisplayClusterConfigurationRectangle& PresetRect)
{
	UDisplayClusterConfigurationViewport* ViewportTemplate = NewObject<UDisplayClusterConfigurationViewport>(Toolkit->GetBlueprintObj(), NAME_None, RF_Transactional | RF_ArchetypeObject | RF_Public);
	ViewportTemplate->Region = FDisplayClusterConfigurationRectangle(PresetRect);

	UDisplayClusterConfigurationCluster* Cluster = Toolkit->GetEditorData()->Cluster;

	TArray<FString> ParentItems;
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& Node : Cluster->Nodes)
	{
		ParentItems.Add(Node.Key);
	}

	FString ClusterNodeName = "";
	if (const FString* Key = Cluster->Nodes.FindKey(ClusterNode))
	{
		ClusterNodeName = *Key;
	}

	const FString InitialName = ClusterNode ? 
		FDisplayClusterConfiguratorClusterUtils::GetUniqueNameForViewport(DefaultNewViewportName, ClusterNode, true) : 
		FString::Printf(TEXT("%s_%d"), *DefaultNewViewportName, 0);

	bool bAutoposition = true;
	bool bExpandClusterNode = true;
	TSharedRef<SWidget> ViewportFooter = SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([&bAutoposition]() { return bAutoposition ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([&bAutoposition](ECheckBoxState NewState) { bAutoposition = NewState == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("AddNewViewport_AutoPositionToolTip", "When checked, auto-positions the new Viewport to prevent overlap with other Viewports in the same cluster Node"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddNewViewport_AutoPositionLabel", "Adjust Viewport Position to Prevent Overlap"))
			]
		]
	
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([&bExpandClusterNode]() { return bExpandClusterNode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([&bExpandClusterNode](ECheckBoxState NewState) { bExpandClusterNode = NewState == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("AddNewViewport_ExpandClusterNodeToolTip", "When checked, expands the parent cluster Node to contain the new Viewport"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddNewViewport_ExpandClusterNodeLabel", "Expand Cluster Node to Fit New Viewport"))
			]
		];

	TSharedRef<SDisplayClusterConfiguratorNewClusterItemDialog> DialogContent = SNew(SDisplayClusterConfiguratorNewClusterItemDialog, ViewportTemplate)
		.ParentItemOptions(ParentItems)
		.InitiallySelectedParentItem(ClusterNodeName)
		.PresetItemOptions(FDisplayClusterConfiguratorPresetSize::CommonPresets)
		.InitiallySelectedPreset(FDisplayClusterConfiguratorPresetSize::CommonPresets[FDisplayClusterConfiguratorPresetSize::DefaultPreset])
		.InitialName(InitialName)
		.MaxWindowWidth(NewClusterItemDialogSize.X)
		.MaxWindowHeight(NewClusterItemDialogSize.Y)
		.FooterContent(ViewportFooter)
		.OnPresetChanged_Lambda([=](FVector2D Size) { ViewportTemplate->Region.W = Size.X; ViewportTemplate->Region.H = Size.Y; });

	FDisplayClusterConfiguratorClusterUtils::ShowNewClusterItemDialogWindow(DialogContent, nullptr, LOCTEXT("AddNewViewport_DialogTitle", "Add New Viewport"), NewClusterItemDialogSize);

	UDisplayClusterConfigurationViewport* NewViewport = nullptr;

	if (DialogContent->WasAccepted())
	{
		const FString ParentItem = DialogContent->GetSelectedParentItem();
		const FString ItemName = DialogContent->GetItemName();

		check(Cluster->Nodes.Contains(ParentItem));

		UDisplayClusterConfigurationClusterNode* ParentNode = Cluster->Nodes[ParentItem];

		if (bAutoposition)
		{
			FVector2D DesiredPosition = FVector2D(ViewportTemplate->Region.X, ViewportTemplate->Region.Y);
			FVector2D DesiredSize = FVector2D(ViewportTemplate->Region.W, ViewportTemplate->Region.H);
			FVector2D NewPosition = FindNextAvailablePositionForViewport(ParentNode, DesiredPosition, DesiredSize);

			ViewportTemplate->Region.X = NewPosition.X;
			ViewportTemplate->Region.Y = NewPosition.Y;
		}

		NewViewport = DuplicateObject(ViewportTemplate, ParentNode);
		NewViewport->SetFlags(RF_Transactional);
		NewViewport = AddViewportToClusterNode(NewViewport, ParentNode, ItemName);

		if (bExpandClusterNode)
		{
			FIntRect ViewportRegion = NewViewport->Region.ToRect();
			FIntRect ClusterWindowRect = FIntRect(0, 0, ParentNode->WindowRect.W, ParentNode->WindowRect.H);

			ClusterWindowRect.Union(ViewportRegion);
			FIntPoint WindowSize = ClusterWindowRect.Max;
			FDisplayClusterConfigurationRectangle NewWindowRect(ParentNode->WindowRect.X, ParentNode->WindowRect.Y, WindowSize.X, WindowSize.Y);

			FDisplayClusterConfiguratorClusterNodeViewModel ClusterNodeVM(ParentNode);
			ClusterNodeVM.SetWindowRect(NewWindowRect);
		}
	}

	// Clean up the template object so that it gets GCed once this schema action has been destroyed
	ViewportTemplate->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	ViewportTemplate->SetFlags(RF_Transient);
	
	return NewViewport;
}

void FDisplayClusterConfiguratorClusterUtils::ShowNewClusterItemDialogWindow(TSharedRef<SDisplayClusterConfiguratorNewClusterItemDialog> DialogContent, TSharedPtr<SWidget> ParentElement, FText WindowTitle, FVector2D WindowSize)
{
	TSharedPtr<SWidget> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(ParentElement);

	const FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
	const FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
	const FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

	const float ScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayTopLeft.X, DisplayTopLeft.Y);
	const FVector2D DialogWindowSize = WindowSize * ScaleFactor;

	const FVector2D WindowPosition = (DisplayTopLeft + 0.5f * (DisplaySize - DialogWindowSize)) / ScaleFactor;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::None)
		.ClientSize(DialogWindowSize)
		.ScreenPosition(WindowPosition);

	DialogContent->SetParentWindow(Window);
	Window->SetContent(DialogContent);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

TSharedPtr<FDragDropOperation> FDisplayClusterConfiguratorClusterUtils::MakeDragDropOperation(const TArray<UObject*>& SelectedObjects)
{
	TArray<UDisplayClusterConfigurationViewport*> Viewports;
	TArray<UDisplayClusterConfigurationClusterNode*> ClusterNodes;

	for (UObject* SelectedObject : SelectedObjects)
	{
		if (UDisplayClusterConfigurationViewport* Viewport = Cast<UDisplayClusterConfigurationViewport>(SelectedObject))
		{
			Viewports.Add(Viewport);
		}
		else if (UDisplayClusterConfigurationClusterNode* ClusterNode = Cast<UDisplayClusterConfigurationClusterNode>(SelectedObject))
		{
			ClusterNodes.Add(ClusterNode);
		}
		else
		{
			// Only cluster nodes and viewports can be dragged and dropped. If anything else is selected, abort the drag/drop operation
			return TSharedPtr<FDragDropOperation>();
		}
	}

	if (Viewports.Num() && !ClusterNodes.Num())
	{
		// If only viewports are selected, return a viewport drag/drop operation
		return FDisplayClusterConfiguratorViewportDragDropOp::New(Viewports);
	}
	else if (ClusterNodes.Num() && !Viewports.Num())
	{
		// If only cluster nodes are selected, return a cluster node drag/drop operation
		return FDisplayClusterConfiguratorClusterNodeDragDropOp::New(ClusterNodes);
	}
	else
	{
		// If both cluster nodes and viewports are selected, only allow a cluster node drag/drop operation if all selected viewports are children of the selected cluster nodes
		bool bOnlyChildViewports = true;
		for (UDisplayClusterConfigurationViewport* Viewport : Viewports)
		{
			bool bIsChild = ClusterNodes.ContainsByPredicate([=](UDisplayClusterConfigurationClusterNode* ClusterNode)
			{
				return ClusterNode == Viewport->GetOuter();
			});

			if (!bIsChild)
			{
				bOnlyChildViewports = false;
				break;
			}
		}

		if (bOnlyChildViewports)
		{
			return FDisplayClusterConfiguratorClusterNodeDragDropOp::New(ClusterNodes);
		}
		else
		{
			return TSharedPtr<FDragDropOperation>();
		}
	}
}

namespace
{
	FVector2D FindAvailableSpace(const TArray<FBox2D>& OtherBounds, const FBox2D& DesiredBounds)
	{
		FBox2D CurrentBounds(DesiredBounds);

		// Similar to FBox2D::Intersects, but ignores the case where the box edges are touching.
		auto IntrudesFunc = [](const FBox2D& BoxA, const FBox2D& BoxB)
		{

			// Special case if both boxes are directly on top of each other, which is considered an intrusion.
			if (BoxA == BoxB)
			{
				return true;
			}

			if ((BoxA.Min.X >= BoxB.Max.X) || (BoxB.Min.X >= BoxA.Max.X))
			{
				return false;
			}

			if ((BoxA.Min.Y >= BoxB.Max.Y) || (BoxB.Min.Y >= BoxA.Max.Y))
			{
				return false;
			}

			return true;
		};

		bool bIntersectsBounds;
		do
		{
			bIntersectsBounds = false;

			for (const FBox2D& Bounds : OtherBounds)
			{
				if (IntrudesFunc(CurrentBounds, Bounds))
				{
					bIntersectsBounds = true;

					float Shift = Bounds.GetSize().X;
					CurrentBounds.Min.X += Shift;
					CurrentBounds.Max.X += Shift;

					break;
				}
			}

		} while (bIntersectsBounds);

		return CurrentBounds.Min;
	}
}

FVector2D FDisplayClusterConfiguratorClusterUtils::FindNextAvailablePositionForClusterNode(UDisplayClusterConfigurationCluster* Cluster, const FString& DesiredHost, const FVector2D& DesiredPosition, const FVector2D& DesiredSize)
{
	TMap<FString, TMap<FString, UDisplayClusterConfigurationClusterNode*>> ClusterNodesByHost;
	SortClusterNodesByHost(Cluster->Nodes, ClusterNodesByHost);

	// If the desired host doesn't exist yet, simply return the node's desired position
	if (!ClusterNodesByHost.Contains(DesiredHost))
	{
		return DesiredPosition;
	}

	const TMap<FString, UDisplayClusterConfigurationClusterNode*>& ExistingClusterNodes = ClusterNodesByHost[DesiredHost];
	if (!ExistingClusterNodes.Num())
	{
		return DesiredPosition;
	}

	FBox2D DesiredBounds = FBox2D(DesiredPosition, DesiredPosition + DesiredSize);
	TArray<FBox2D> ClusterNodeBounds;
	for (const TPair<FString, UDisplayClusterConfigurationClusterNode*>& ClusterNodeKeyPair : ExistingClusterNodes)
	{
		const FDisplayClusterConfigurationRectangle& WindowRect = ClusterNodeKeyPair.Value->WindowRect;
		ClusterNodeBounds.Add(FBox2D(FVector2D(WindowRect.X, WindowRect.Y), FVector2D(WindowRect.X + WindowRect.W, WindowRect.Y + WindowRect.H)));
	}

	return FindAvailableSpace(ClusterNodeBounds, DesiredBounds);
}

FVector2D FDisplayClusterConfiguratorClusterUtils::FindNextAvailablePositionForViewport(UDisplayClusterConfigurationClusterNode* ClusterNode, const FVector2D& DesiredPosition, const FVector2D& DesiredSize)
{
	if (!ClusterNode)
	{
		return DesiredPosition;
	}

	FBox2D DesiredBounds = FBox2D(DesiredPosition, DesiredPosition + DesiredSize);
	TArray<FBox2D> ViewportBounds;
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportKeyPair : ClusterNode->Viewports)
	{
		const FDisplayClusterConfigurationRectangle& Region = ViewportKeyPair.Value->Region;
		ViewportBounds.Add(FBox2D(FVector2D(Region.X, Region.Y), FVector2D(Region.X + Region.W, Region.Y + Region.H)));
	}

	return FindAvailableSpace(ViewportBounds, DesiredBounds);
}

void FDisplayClusterConfiguratorClusterUtils::SortClusterNodesByHost(const TMap<FString, UDisplayClusterConfigurationClusterNode*>& InClusterNodes, TMap<FString, TMap<FString, UDisplayClusterConfigurationClusterNode*>>& OutSortedNodes)
{
	for (const TPair<FString, UDisplayClusterConfigurationClusterNode*>& ClusterNodePair : InClusterNodes)
	{
		check(ClusterNodePair.Value)

		FString Host = ClusterNodePair.Value->Host;
		if (!OutSortedNodes.Contains(Host))
		{
			OutSortedNodes.Add(Host, TMap<FString, UDisplayClusterConfigurationClusterNode*>());
		}

		OutSortedNodes[Host].Add(ClusterNodePair);
	}

	// Sort the hosts by the host address
	OutSortedNodes.KeySort(TLess<FString>());
}

void FDisplayClusterConfiguratorClusterUtils::SortClusterNodesByHost(const TMap<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& InClusterNodes, TMap<FString, TMap<FString, UDisplayClusterConfigurationClusterNode*>>& OutSortedNodes)
{
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& ClusterNodePair : InClusterNodes)
	{
		check(ClusterNodePair.Value)

			FString Host = ClusterNodePair.Value->Host;
		if (!OutSortedNodes.Contains(Host))
		{
			OutSortedNodes.Add(Host, TMap<FString, UDisplayClusterConfigurationClusterNode*>());
		}

		OutSortedNodes[Host].Add(ClusterNodePair);
	}

	// Sort the hosts by the host address
	OutSortedNodes.KeySort(TLess<FString>());
}

UDisplayClusterConfigurationHostDisplayData* FDisplayClusterConfiguratorClusterUtils::FindOrCreateHostDisplayData(UDisplayClusterConfigurationCluster* Cluster, FString HostIPAddress)
{
	// In some cases, existing host display data may be pending kill, such as if the user recently performed an undo to a state
	// prior to the data's existence. In this case, simply remove existing host data that is pending kill and create a new one to use.
	TArray<FString> PendingKillHostData;
	for (TPair<FString, TObjectPtr<UDisplayClusterConfigurationHostDisplayData>>& HostPair : Cluster->HostDisplayData)
	{
		if (!IsValid(HostPair.Value))
		{
			PendingKillHostData.Add(HostPair.Key);
		}
	}

	for (const FString& Host : PendingKillHostData)
	{
		Cluster->HostDisplayData.Remove(Host);
	}

	if (!Cluster->HostDisplayData.Contains(HostIPAddress))
	{
		const FString HostName = GetUniqueNameForHost(DefaultNewHostName, Cluster, true);
		UDisplayClusterConfigurationHostDisplayData* NewData = NewObject<UDisplayClusterConfigurationHostDisplayData>(Cluster, NAME_None, RF_Transactional);
		NewData->HostName = FText::FromString(HostName);
		NewData->Color = FDisplayClusterConfiguratorStyle::Get().GetDefaultColor(Cluster->HostDisplayData.Num()).CopyWithNewOpacity(1.0f);

		Cluster->HostDisplayData.Add(HostIPAddress, NewData);
	}

	return Cluster->HostDisplayData[HostIPAddress];
}

bool FDisplayClusterConfiguratorClusterUtils::RemoveUnusedHostDisplayData(UDisplayClusterConfigurationCluster* Cluster)
{
	bool bHostDataRemoved = false;

	TArray<FString> UnusedHosts;
	Cluster->HostDisplayData.GetKeys(UnusedHosts);

	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodePair : Cluster->Nodes)
	{
		UnusedHosts.Remove(NodePair.Value->Host);
	}

	if (UnusedHosts.Num() > 0)
	{
		Cluster->Modify();

		for (const FString& UnusedHost : UnusedHosts)
		{
			Cluster->HostDisplayData[UnusedHost]->MarkAsGarbage();
			Cluster->HostDisplayData.Remove(UnusedHost);
		}

		bHostDataRemoved = true;
	}

	return bHostDataRemoved;
}

FString FDisplayClusterConfiguratorClusterUtils::GetUniqueNameForHost(FString InitialName, UDisplayClusterConfigurationCluster* ParentCluster, bool bAddZero)
{
	TArray<FString> UsedNames;
	for (TPair<FString, TObjectPtr<UDisplayClusterConfigurationHostDisplayData>>& HostPair : ParentCluster->HostDisplayData)
	{
		UsedNames.Add(HostPair.Value->HostName.ToString());
	}

	return GetUniqueName(InitialName, UsedNames, UDisplayClusterConfigurationClusterNode::StaticClass(), ParentCluster, bAddZero);
}

FString FDisplayClusterConfiguratorClusterUtils::GetAddressForHost(UDisplayClusterConfigurationHostDisplayData* HostDisplayData)
{
	FString HostAddress;
	if (UDisplayClusterConfigurationCluster* HostParent = Cast<UDisplayClusterConfigurationCluster>(HostDisplayData->GetOuter()))
	{
		if (const FString* HostKey = HostParent->HostDisplayData.FindKey(HostDisplayData))
		{
			HostAddress = *HostKey;
		}
	}

	return HostAddress;
}

bool FDisplayClusterConfiguratorClusterUtils::RemoveHost(UDisplayClusterConfigurationCluster* Cluster, FString Host)
{
	bool bDataRemoved = false;

	if (Cluster->HostDisplayData.Contains(Host))
	{
		Cluster->Modify();
		Cluster->HostDisplayData[Host]->Modify();

		Cluster->HostDisplayData.Remove(Host);

		bDataRemoved = true;
	}

	TArray<FString> ClusterNodesToRemove;
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& Pair : Cluster->Nodes)
	{
		if (Pair.Value->Host == Host)
		{
			ClusterNodesToRemove.Add(Pair.Key);
		}
	}

	for (FString ClusterNodeToRemove : ClusterNodesToRemove)
	{
		RemoveClusterNodeFromCluster(Cluster->Nodes[ClusterNodeToRemove]);
	}

	FDisplayClusterConfiguratorUtils::MarkDisplayClusterBlueprintAsModified(Cluster, true);
	
	return bDataRemoved;
}

UDisplayClusterConfigurationHostDisplayData* FDisplayClusterConfiguratorClusterUtils::GetHostDisplayDataForClusterNode(UDisplayClusterConfigurationClusterNode* ClusterNode)
{
	UDisplayClusterConfigurationHostDisplayData* HostDisplayData = nullptr;
	if (UDisplayClusterConfigurationCluster* ClusterNodeParent = Cast<UDisplayClusterConfigurationCluster>(ClusterNode->GetOuter()))
	{
		if (ClusterNodeParent->HostDisplayData.Contains(ClusterNode->Host))
		{
			HostDisplayData = ClusterNodeParent->HostDisplayData[ClusterNode->Host];
		}
	}

	return HostDisplayData;
}

FString FDisplayClusterConfiguratorClusterUtils::GetClusterNodeName(UDisplayClusterConfigurationClusterNode* ClusterNode)
{
	if (UDisplayClusterConfigurationCluster* ClusterNodeParent = Cast<UDisplayClusterConfigurationCluster>(ClusterNode->GetOuter()))
	{
		if (const FString* KeyPtr = ClusterNodeParent->Nodes.FindKey(ClusterNode))
		{
			return *KeyPtr;
		}
	}

	return "";
}

bool FDisplayClusterConfiguratorClusterUtils::IsClusterNodePrimary(UDisplayClusterConfigurationClusterNode* ClusterNode)
{
	if (const UDisplayClusterConfigurationCluster* Cluster = Cast<UDisplayClusterConfigurationCluster>(ClusterNode->GetOuter()))
	{
		const FString PrimaryNodeId = Cluster->PrimaryNode.Id;
		if (Cluster->Nodes.Contains(PrimaryNodeId) && Cluster->Nodes[PrimaryNodeId] == ClusterNode)
		{
			return true;
		}
	}

	return false;
}

FString FDisplayClusterConfiguratorClusterUtils::GetUniqueNameForClusterNode(FString InitialName, UDisplayClusterConfigurationCluster* ParentCluster, bool bAddZero)
{
	InitialName = ObjectTools::SanitizeObjectName(InitialName);
	
	TArray<FString> UsedNames;
	ParentCluster->Nodes.GenerateKeyArray(UsedNames);

	return GetUniqueName(InitialName, UsedNames, UDisplayClusterConfigurationClusterNode::StaticClass(), ParentCluster, bAddZero);
}

UDisplayClusterConfigurationClusterNode* FDisplayClusterConfiguratorClusterUtils::AddClusterNodeToCluster(UDisplayClusterConfigurationClusterNode* ClusterNode, UDisplayClusterConfigurationCluster* Cluster, FString NewClusterNodeName)
{
	FString ClusterNodeName = "";

	// First, remove the viewport from its current parent cluster node, if it has one.
	if (UDisplayClusterConfigurationCluster* ClusterNodeParent = Cast<UDisplayClusterConfigurationCluster>(ClusterNode->GetOuter()))
	{
		if (const FString* OldKeyPtr = ClusterNodeParent->Nodes.FindKey(ClusterNode))
		{
			ClusterNodeName = *OldKeyPtr;

			ClusterNodeParent->Modify();

			const FName FieldName = GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationCluster, Nodes);
			RemoveKeyFromMap(ClusterNodeParent, FieldName, *OldKeyPtr);
		}
	}

	if (!NewClusterNodeName.IsEmpty())
	{
		ClusterNodeName = NewClusterNodeName;
	}

	if (ClusterNodeName.IsEmpty())
	{
		ClusterNodeName = TEXT("ClusterNode");
	}

	ClusterNodeName = GetUniqueNameForClusterNode(ClusterNodeName, Cluster);

	Cluster->Modify();
	ClusterNode->Modify();
	ClusterNode->Rename(*ClusterNodeName, Cluster, REN_DontCreateRedirectors);

	const FName FieldName = GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationCluster, Nodes);
	UDisplayClusterConfigurationClusterNode* NewClusterNode = CastChecked<UDisplayClusterConfigurationClusterNode>(AddKeyWithInstancedValueToMap(Cluster, FieldName, ClusterNodeName, ClusterNode));

	check(ClusterNode != NewClusterNode);
	ClusterNode->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders);
	ClusterNode->SetFlags(RF_Transient);
	
	return NewClusterNode;
}

bool FDisplayClusterConfiguratorClusterUtils::RemoveClusterNodeFromCluster(UDisplayClusterConfigurationClusterNode* ClusterNode)
{
	if (UDisplayClusterConfigurationCluster* ClusterNodeParent = Cast<UDisplayClusterConfigurationCluster>(ClusterNode->GetOuter()))
	{
		if (const FString* KeyPtr = ClusterNodeParent->Nodes.FindKey(ClusterNode))
		{
			ClusterNode->Modify();

			ClusterNodeParent->Modify();

			const bool bRemovingPrimaryNode = IsClusterNodePrimary(ClusterNode);
			if (bRemovingPrimaryNode)
			{
				ClusterNodeParent->PrimaryNode.Id.Empty();
			}
			
			RemoveKeyFromMap(ClusterNodeParent, GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationCluster, Nodes), *KeyPtr);
			ensure (ClusterNode->GetPackage() == GetTransientPackage());
			
			ClusterNode->SetFlags(RF_Transient);

			if (bRemovingPrimaryNode)
			{
				for (const TTuple<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodesKeyVal : ClusterNodeParent->Nodes)
				{
					SetClusterNodeAsPrimary(NodesKeyVal.Value);
					break;
				}
			}
			
			FDisplayClusterConfiguratorUtils::MarkDisplayClusterBlueprintAsModified(ClusterNodeParent, true);
			
			return true;
		}
	}

	return false;
}

bool FDisplayClusterConfiguratorClusterUtils::RenameClusterNode(UDisplayClusterConfigurationClusterNode* ClusterNode, FString NewClusterNodeName)
{
	if (UDisplayClusterConfigurationCluster* ClusterNodeParent = Cast<UDisplayClusterConfigurationCluster>(ClusterNode->GetOuter()))
	{
		if (const FString* KeyPtr = ClusterNodeParent->Nodes.FindKey(ClusterNode))
		{
			if (KeyPtr->Equals(NewClusterNodeName))
			{
				return false;
			}

			const bool bIsPrimary = IsClusterNodePrimary(ClusterNode);

			ClusterNode->Modify();
			ClusterNodeParent->Modify();

			const FString UniqueName = GetUniqueNameForClusterNode(NewClusterNodeName, ClusterNodeParent);

			const FName FieldName = GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationCluster, Nodes);
			RemoveKeyFromMap(ClusterNodeParent, FieldName, *KeyPtr);

			// Rename after remove, before add.
			ClusterNode->Rename(*UniqueName, ClusterNodeParent, REN_DontCreateRedirectors);
			
			UDisplayClusterConfigurationClusterNode* NewClusterNode = CastChecked<UDisplayClusterConfigurationClusterNode>(AddKeyWithInstancedValueToMap(ClusterNodeParent, FieldName, UniqueName, ClusterNode));

			check(ClusterNode != NewClusterNode);
			ClusterNode->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders);
			ClusterNode->SetFlags(RF_Transient);
			
			// If the cluster node was a primary node before the rename, we need to update the primary reference in the cluster with the new name
			if (bIsPrimary)
			{
				SetClusterNodeAsPrimary(NewClusterNode);
			}

			return true;
		}
	}

	return false;
}

bool FDisplayClusterConfiguratorClusterUtils::SetClusterNodeAsPrimary(UDisplayClusterConfigurationClusterNode* ClusterNode)
{
	if (UDisplayClusterConfigurationCluster* ClusterNodeParent = Cast<UDisplayClusterConfigurationCluster>(ClusterNode->GetOuter()))
	{
		const FString* NodeIdPtr = ClusterNodeParent->Nodes.FindKey(ClusterNode);

		if (NodeIdPtr != nullptr)
		{
			ClusterNodeParent->Modify();
			ClusterNodeParent->PrimaryNode.Id = *NodeIdPtr;
			return true;
		}
	}

	return false;
}

FString FDisplayClusterConfiguratorClusterUtils::GetViewportName(UDisplayClusterConfigurationViewport* Viewport)
{
	if (UDisplayClusterConfigurationClusterNode* ViewportParent = Cast<UDisplayClusterConfigurationClusterNode>(Viewport->GetOuter()))
	{
		if (const FString* KeyPtr = ViewportParent->Viewports.FindKey(Viewport))
		{
			return *KeyPtr;
		}
	}

	return "";
}

FString FDisplayClusterConfiguratorClusterUtils::GetUniqueNameForViewport(FString InitialName, UDisplayClusterConfigurationClusterNode* ParentClusterNode, bool bAddZero)
{
	InitialName = ObjectTools::SanitizeObjectName(InitialName);
	
	// Viewport names must be unique across the entire cluster, not just within its parent cluster nodes. Gather all of the viewport names
	// in the cluster to check for uniqueness. Add the parent cluster node's viewports first, in case we can't get to the root cluster through
	// the cluster node's Outer (i.e. the cluster node has not been added to the cluster yet)
	TSet<FString> UsedNames;
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportKeyPair : ParentClusterNode->Viewports)
	{
		UsedNames.Add(ViewportKeyPair.Key);
	}

	if (UDisplayClusterConfigurationCluster* Cluster = Cast<UDisplayClusterConfigurationCluster>(ParentClusterNode->GetOuter()))
	{
		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& ClusterNodeKeyPair : Cluster->Nodes)
		{
			UDisplayClusterConfigurationClusterNode* ClusterNode = ClusterNodeKeyPair.Value;
			if (ClusterNode != ParentClusterNode)
			{
				for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportKeyPair : ClusterNode->Viewports)
				{
					UsedNames.Add(ViewportKeyPair.Key);
				}
			}
		}
	}

	TArray<FString> UsedNamesArray = UsedNames.Array();
	
	return GetUniqueName(InitialName, UsedNamesArray, UDisplayClusterConfigurationViewport::StaticClass(), ParentClusterNode, bAddZero);
}

UDisplayClusterConfigurationViewport* FDisplayClusterConfiguratorClusterUtils::AddViewportToClusterNode(UDisplayClusterConfigurationViewport* Viewport, UDisplayClusterConfigurationClusterNode* ClusterNode, FString NewViewportName)
{
	FString ViewportName = "";

	// First, remove the viewport from its current parent cluster node, if it has one.
	if (UDisplayClusterConfigurationClusterNode* ViewportParent = Cast<UDisplayClusterConfigurationClusterNode>(Viewport->GetOuter()))
	{
		if (const FString* OldKeyPtr = ViewportParent->Viewports.FindKey(Viewport))
		{
			ViewportName = *OldKeyPtr;

			ViewportParent->Modify();
			
			RemoveKeyFromMap(ViewportParent, GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationClusterNode, Viewports), *OldKeyPtr);
		}
	}

	if (!NewViewportName.IsEmpty())
	{
		ViewportName = NewViewportName;
	}

	if (ViewportName.IsEmpty())
	{
		ViewportName = TEXT("Viewport");
	}

	ViewportName = GetUniqueNameForViewport(ViewportName, ClusterNode);

	ClusterNode->Modify();
	Viewport->Modify();
	Viewport->Rename(*ViewportName, ClusterNode, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders);

	const FName FieldName = GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationClusterNode, Viewports);
	UDisplayClusterConfigurationViewport* NewViewport = CastChecked<UDisplayClusterConfigurationViewport>(AddKeyWithInstancedValueToMap(ClusterNode, FieldName, ViewportName, Viewport));

	check(Viewport != NewViewport);
	Viewport->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders);
	Viewport->SetFlags(RF_Transient);
	
	FDisplayClusterConfiguratorUtils::MarkDisplayClusterBlueprintAsModified(NewViewport, true);

	return NewViewport;
}

bool FDisplayClusterConfiguratorClusterUtils::RemoveViewportFromClusterNode(UDisplayClusterConfigurationViewport* Viewport)
{
	if (UDisplayClusterConfigurationClusterNode* ViewportParent = Cast<UDisplayClusterConfigurationClusterNode>(Viewport->GetOuter()))
	{
		if (const FString* KeyPtr = ViewportParent->Viewports.FindKey(Viewport))
		{
			Viewport->Modify();

			ViewportParent->Modify();

			RemoveKeyFromMap(ViewportParent, GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationClusterNode, Viewports), *KeyPtr);
			ensure (Viewport->GetPackage() == GetTransientPackage());
			
			Viewport->SetFlags(RF_Transient);
			
			FDisplayClusterConfiguratorUtils::MarkDisplayClusterBlueprintAsModified(ViewportParent, true);
			
			return true;
		}
	}

	return false;
}

bool FDisplayClusterConfiguratorClusterUtils::RenameViewport(UDisplayClusterConfigurationViewport* Viewport, FString NewViewportName)
{
	if (UDisplayClusterConfigurationClusterNode* ViewportParent = Cast<UDisplayClusterConfigurationClusterNode>(Viewport->GetOuter()))
	{
		if (const FString* KeyPtr = ViewportParent->Viewports.FindKey(Viewport))
		{
			if (KeyPtr->Equals(NewViewportName))
			{
				return false;
			}

			const FString UniqueName = GetUniqueNameForViewport(NewViewportName, ViewportParent);
			
			Viewport->Modify();
			ViewportParent->Modify();
			
			const FName FieldName = GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationClusterNode, Viewports);

			// Remove and re-add. We can't just call RenameKeyInMap because when we rename the viewport after
			// it will lose sync with instances.
			
			RemoveKeyFromMap(ViewportParent, FieldName, *KeyPtr);

			// Rename after removing. If this is done after adding instances will lose sync with the CDO.
			Viewport->Rename(*UniqueName, ViewportParent, REN_DontCreateRedirectors);
			
			UDisplayClusterConfigurationViewport* NewViewport =
				CastChecked<UDisplayClusterConfigurationViewport>(AddKeyWithInstancedValueToMap(ViewportParent, FieldName, UniqueName, Viewport));
			check(Viewport != NewViewport);
			Viewport->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders);
			Viewport->SetFlags(RF_Transient);
			
			FDisplayClusterConfiguratorUtils::MarkDisplayClusterBlueprintAsModified(NewViewport, true);
			
			return true;
		}
	}

	return false;
}

void FDisplayClusterConfiguratorClusterUtils::CopyClusterItemsToClipboard(const TArray<UObject*>& ClusterItemsToCopy)
{
	TSet<UObject*> UniqueItemsToCopy;

	for (UObject* ClusterItem : ClusterItemsToCopy)
	{
		// Only allow viewports and cluster nodes to be copied at this point.
		if (ClusterItem->IsA<UDisplayClusterConfigurationViewport>())
		{
			// If a viewport is being copied, make sure its parent cluster node is not also being copied; if its parent is being copied,
			// the viewport doesn't need to be copied directly as it will be copied with its parent.
			if (!ClusterItemsToCopy.Contains(ClusterItem->GetOuter()))
			{
				UniqueItemsToCopy.Add(ClusterItem);
			}
		}
		else if (ClusterItem->IsA<UDisplayClusterConfigurationClusterNode>())
		{
			UniqueItemsToCopy.Add(ClusterItem);
		}
	}

	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	for (UObject* ClusterItem : UniqueItemsToCopy)
	{
		UExporter::ExportToOutputDevice(&Context, ClusterItem, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, nullptr);
	}

	FString CopiedText = Archive;
	FPlatformApplicationMisc::ClipboardCopy(*CopiedText);
}

class FClusterNodeTextFactory : public FCustomizableTextObjectFactory
{
public:
	FClusterNodeTextFactory()
		: FCustomizableTextObjectFactory(GWarn)
	{
	}

	// FCustomizableTextObjectFactory implementation

	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override
	{
		const bool isClusterNode = ObjectClass->IsChildOf(UDisplayClusterConfigurationClusterNode::StaticClass());
		const bool isViewportNode = ObjectClass->IsChildOf(UDisplayClusterConfigurationViewport::StaticClass());

		return isClusterNode || isViewportNode;
	}

	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		if (UDisplayClusterConfigurationClusterNode* ClusterNode = Cast<UDisplayClusterConfigurationClusterNode>(NewObject))
		{
			ClusterNodes.Add(ClusterNode);
		}
		else if (UDisplayClusterConfigurationViewport* ViewportNode = Cast<UDisplayClusterConfigurationViewport>(NewObject))
		{
			Viewports.Add(ViewportNode);
		}
	}

	// FCustomizableTextObjectFactory (end)

public:
	TArray<UDisplayClusterConfigurationClusterNode*> ClusterNodes;
	TArray<UDisplayClusterConfigurationViewport*> Viewports;
};

bool FDisplayClusterConfiguratorClusterUtils::CanPasteClusterItemsFromClipboard(const TArray<UObject*>& TargetClusterItems, int32& OutNumItems)
{
	// Can't paste unless the clipboard has a string in it
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	if (!ClipboardContent.IsEmpty())
	{
		FClusterNodeTextFactory Factory;
		Factory.ProcessBuffer(GetTransientPackage(), RF_Transactional, ClipboardContent);

		OutNumItems = Factory.ClusterNodes.Num() + Factory.Viewports.Num();

		bool bCanPaste = false;
		if (Factory.ClusterNodes.Num() > 0)
		{
			// If we are trying to paste cluster nodes, the target should be a cluster root, a host display data, or a cluster node.
			bCanPaste = TargetClusterItems.ContainsByPredicate([=](const UObject* Item)
			{
				return
					Item->IsA<UDisplayClusterConfigurationCluster>() ||
					Item->IsA<UDisplayClusterConfigurationClusterNode>() ||
					Item->IsA<UDisplayClusterConfigurationHostDisplayData>();
			});
		}

		if (Factory.Viewports.Num() > 0)
		{
			// If we are trying to paste viewports, the target should either be a cluster node or a viewport.
			bCanPaste = TargetClusterItems.ContainsByPredicate([=](const UObject* Item)
			{
				return Item->IsA<UDisplayClusterConfigurationClusterNode>() || Item->IsA<UDisplayClusterConfigurationViewport>();
			});
		}

		return bCanPaste;
	}

	return false;
}

TArray<UObject*> FDisplayClusterConfiguratorClusterUtils::PasteClusterItemsFromClipboard(const TArray<UObject*>& TargetClusterItems, TOptional<FVector2D> PasteLocation)
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	FClusterNodeTextFactory Factory;
	Factory.ProcessBuffer(GetTransientPackage(), RF_Transactional, ClipboardContent);

	// Filter out the target items and find all unique cluster roots and cluster nodes. For each target cluster node, also add its parent root,
	// and for each target viewport, add its parent cluster node. This allows users to paste cluster nodes and viewports by selecting their sibling nodes,
	// which will then add the pasted node to the parent of the target node.

	// Pair up the cluster roots with a host string, which indicates which host the cluster nodes should be pasted into (the cluster nodes' Host strings will be set when duplicated).
	// If the paired host string is empty, the pasted cluster nodes' original host values will be preserved.
	TSet<TTuple<UDisplayClusterConfigurationCluster*, FString>> SelectedRootHosts;
	TSet<UDisplayClusterConfigurationClusterNode*> SelectedNodes;

	for (UObject* TargetItem : TargetClusterItems)
	{
		if (UDisplayClusterConfigurationCluster* ClusterRoot = Cast<UDisplayClusterConfigurationCluster>(TargetItem))
		{
			SelectedRootHosts.Add(TTuple<UDisplayClusterConfigurationCluster*, FString>(ClusterRoot, ""));
		}
		else if (UDisplayClusterConfigurationHostDisplayData* Host = Cast<UDisplayClusterConfigurationHostDisplayData>(TargetItem))
		{
			// If the target is a host display data, that indicates the user wants to paste the cluster node into the host the display data represents,
			// so add an entry to the cluster root list with the parent cluster object and the host string of the host display data.
			if (UDisplayClusterConfigurationCluster* ParentRoot = Cast<UDisplayClusterConfigurationCluster>(TargetItem->GetOuter()))
			{
				FString HostStr = FString();
				if (const FString* Key = ParentRoot->HostDisplayData.FindKey(Host))
				{
					HostStr = *Key;
				}

				SelectedRootHosts.Add(TTuple<UDisplayClusterConfigurationCluster*, FString>(ParentRoot, HostStr));
			}
		}
		else if (UDisplayClusterConfigurationClusterNode* ClusterNode = Cast<UDisplayClusterConfigurationClusterNode>(TargetItem))
		{
			SelectedNodes.Add(ClusterNode);

			if (UDisplayClusterConfigurationCluster* ParentRoot = Cast<UDisplayClusterConfigurationCluster>(TargetItem->GetOuter()))
			{
				// If pasting as a sibling of a target cluster node, set the pasted node's host to be the same as the sibling node's host.
				SelectedRootHosts.Add(TTuple<UDisplayClusterConfigurationCluster*, FString>(ParentRoot, ClusterNode->Host));
			}
		}
		else if (UDisplayClusterConfigurationViewport* Viewport = Cast<UDisplayClusterConfigurationViewport>(TargetItem))
		{
			if (UDisplayClusterConfigurationClusterNode* ParentNode = Cast<UDisplayClusterConfigurationClusterNode>(TargetItem->GetOuter()))
			{
				SelectedNodes.Add(ParentNode);
			}
		}
	}

	TArray<UObject*> CopiedObjects;

	// Keep track of a couple reference positions for pasting the items to a specfic location to correctly offset pasted location in the case where
	// more than one item is being pasted or an item is being pasted into more than one parent. The ReferencePosition variable keeps track of the 
	// relative position of the item to be pasted, while the ReferenceTargetPosition keeps track of the relative position of the target item being pasted
	// into. The way these work is that, if they aren't set when in the pasting loop below, then they are set to the current iteration item's position. Every
	// other iteration will then subtract the reference position from the pasted position. This allows each item being pasted to be correctly offset to maintain
	// both its relative position in the group of items being pasted and its relative position to the item it is being pasted into.
	TOptional<FVector2D> ReferencePosition = TOptional<FVector2D>();
	TOptional<FVector2D> ReferenceTargetPosition = TOptional<FVector2D>();

	for (const TTuple<UDisplayClusterConfigurationCluster*, FString>& ClusterRootHost : SelectedRootHosts)
	{
		UDisplayClusterConfigurationCluster* ClusterRoot = ClusterRootHost.Key;
		FString HostStr = ClusterRootHost.Value;

		for (UDisplayClusterConfigurationClusterNode* ClusterNode : Factory.ClusterNodes)
		{
			UDisplayClusterConfigurationClusterNode* ClusterNodeCopy = DuplicateObject(ClusterNode, ClusterRoot);
			ClusterNodeCopy->SetFlags(RF_Transactional);
			ClusterNodeCopy = AddClusterNodeToCluster(ClusterNodeCopy, ClusterRoot, ClusterNode->GetName());

			// If the host string isn't empty, it means the user is pasting the cluster node into a particular host, so set the cluster node's host string.
			if (!HostStr.IsEmpty())
			{
				ClusterNodeCopy->Host = HostStr;
			}

			if (PasteLocation.IsSet())
			{
				FVector2D NewLocation = *PasteLocation;

				// If we are pasting into an existing host, offset the reference target position so that the position of the node inside the host
				// is consistent for every host we paste into.
				if (UDisplayClusterConfigurationHostDisplayData* HostDisplayData = GetHostDisplayDataForClusterNode(ClusterNodeCopy))
				{
					// If the reference target position isn't set, set it to the current host's position.
					if (!ReferenceTargetPosition.IsSet())
					{
						ReferenceTargetPosition = HostDisplayData->Position;
					}

					// Offset the location to keep this cluster node's position relative to its parent host consistent.
					NewLocation -= *ReferenceTargetPosition;
				}

				// If the reference position isn't set, set it to the current pasted cluster node's position.
				if (!ReferencePosition.IsSet())
				{
					ReferencePosition = FVector2D(ClusterNode->WindowRect.X, ClusterNode->WindowRect.Y);
				}

				// Offset the location to keep this cluster node's relative position with the group of pasted nodes consistent.
				NewLocation -= FVector2D(ClusterNode->WindowRect.X, ClusterNode->WindowRect.Y) - *ReferencePosition;

				ClusterNodeCopy->WindowRect.X = NewLocation.X;
				ClusterNodeCopy->WindowRect.Y = NewLocation.Y;
			}

			// Rename viewports in the new cluster node to ensure they're unique. Copy the viewport pointers into an array
			// first since renaming would cause the dictionary to change while we're iterating it.
			TArray<typename decltype(ClusterNodeCopy->Viewports)::ValueType> CopiedViewports;
			ClusterNodeCopy->Viewports.GenerateValueArray(CopiedViewports);
			
			for (UDisplayClusterConfigurationViewport* Viewport : CopiedViewports)
			{
				RenameViewport(Viewport, *GetUniqueNameForViewport(*Viewport->GetName(), ClusterNodeCopy));
			}

			CopiedObjects.Add(ClusterNodeCopy);
		}
	}

	// Reset the reference positions for the viewports pasting loop
	ReferencePosition = TOptional<FVector2D>();
	ReferenceTargetPosition = TOptional<FVector2D>();

	for (UDisplayClusterConfigurationClusterNode* ClusterNode : SelectedNodes)
	{
		for (UDisplayClusterConfigurationViewport* Viewport : Factory.Viewports)
		{
			UDisplayClusterConfigurationViewport* ViewportCopy = DuplicateObject(Viewport, ClusterNode);
			ViewportCopy->SetFlags(RF_Transactional);
			ViewportCopy = AddViewportToClusterNode(ViewportCopy, ClusterNode, Viewport->GetName());

			if (PasteLocation.IsSet())
			{
				// If the reference target position isn't set, set it to the current cluster node's position.
				if (!ReferenceTargetPosition.IsSet())
				{
					FVector2D ClusterNodeLocation = FVector2D(ClusterNode->WindowRect.X, ClusterNode->WindowRect.Y);

					// Cluster node window rectangles are local to the node's host, so factor in its host position, if it has one.
					if (UDisplayClusterConfigurationHostDisplayData* HostDisplayData = GetHostDisplayDataForClusterNode(ClusterNode))
					{
						ClusterNodeLocation += HostDisplayData->Position;
					}

					ReferenceTargetPosition = ClusterNodeLocation;
				}

				// Offset the location to keep this viewport's position relative to its parent cluster node consistent.
				FVector2D NewLocation = *PasteLocation - *ReferenceTargetPosition;

				// If the reference position isn't set, set it to the current pasted viewport's position.
				if (!ReferencePosition.IsSet())
				{
					ReferencePosition = FVector2D(Viewport->Region.X, Viewport->Region.Y);
				}

				// Offset the location to keep this viewport's relative position with the group of pasted nodes consistent.
				NewLocation -= FVector2D(Viewport->Region.X, Viewport->Region.Y) - *ReferencePosition;

				// Viewports cannot have negative position
				ViewportCopy->Region.X = FMath::Max(NewLocation.X, 0.0f);
				ViewportCopy->Region.Y = FMath::Max(NewLocation.Y, 0.0f);
			}

			CopiedObjects.Add(ViewportCopy);
		}
	}

	return CopiedObjects;
}

FString FDisplayClusterConfiguratorClusterUtils::GetUniqueName(const FString& InitialName, const TArray<FString>& UsedNames, const UClass* Class, UObject* Parent, bool bAddZero)
{
	FString NewName;
	if (!bAddZero && !UsedNames.Contains(InitialName))
	{
		// Name doesn't need to be modified
		NewName = InitialName;
	}
	else
	{
		int32 Counter = bAddZero ? 0 : 1;

		// Find the start of the existing numeric suffix
		int32 Index = InitialName.Len();
		while (Index > 0 && InitialName[Index-1] >= '0' && InitialName[Index-1] <= '9')
		{
			--Index;
		}

		FString BaseName = InitialName;
		if (Index < BaseName.Len())
		{
			// Strip away the suffix and store the value in the counter so we can count up from there
			FString NumericSuffix = BaseName.RightChop(Index);
			Counter = FCString::Atoi(*NumericSuffix);
			NumericSuffix = FString::FromInt(Counter); // Restringify the counter to account for leading 0s that we don't want to remove
			BaseName.RemoveAt(BaseName.Len() - NumericSuffix.Len(), NumericSuffix.Len(), false);
		}
		else
		{
			// No existing suffix, so add our underscore separator
			BaseName += "_";
		}

		do
		{
			NewName = FString::Printf(TEXT("%s%d"), *BaseName, Counter);
			++Counter;
		}
		while (UsedNames.Contains(NewName));
	}
	
	// If there is already an in-memory object connected to the parent cluster with our generated name, we need to use a globally unique object name.
	if (StaticFindObject(nullptr, Parent, *NewName, true))
	{
		NewName = MakeUniqueObjectName(Parent, Class, *NewName).ToString();
	}

	return NewName;
}

#undef LOCTEXT_NAMESPACE