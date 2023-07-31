// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"

class SWidget;
class FDragDropOperation;
class FDisplayClusterConfiguratorBlueprintEditor;
class UDisplayClusterConfigurationData_Base;
class UDisplayClusterConfigurationCluster;
class UDisplayClusterConfigurationHostDisplayData;
class UDisplayClusterConfigurationClusterNode;
class UDisplayClusterConfigurationViewport;
class SDisplayClusterConfiguratorNewClusterItemDialog;
struct FDisplayClusterConfigurationRectangle;

#define NDISPLAY_DEFAULT_CLUSTER_HOST "127.0.0.1"

class FDisplayClusterConfiguratorClusterUtils
{
public:
	/**
	 * Creates a new cluster node, presenting a dialog box to allow the user to customize its properties, and adds it to the cluster.
	 * @param Toolkit - The blueprint editor toolkit being used.
	 * @param Cluster - The initial cluster the user wants to add the cluster node to.
	 * @param PresetRect - The initial rectangle to configure the new cluster node with in the dialog box.
	 * @param PresetHost - Optional initial host string to configure the new cluster node with in the dialog box.
	 * @return The newly created cluster node, or null if the user cancelled out of the dialog box.
	 */
	static UDisplayClusterConfigurationClusterNode* CreateNewClusterNodeFromDialog(const TSharedRef<FDisplayClusterConfiguratorBlueprintEditor>& Toolkit, UDisplayClusterConfigurationCluster* Cluster, const FDisplayClusterConfigurationRectangle& PresetRect,
		FString PresetHost = NDISPLAY_DEFAULT_CLUSTER_HOST);

	/**
	 * Creates a new viewport, presenting a dialog box to allow the user to customize its properties, and adds it to the user chosen cluster node.
	 * @param Toolkit - The blueprint editor toolkit being used.
	 * @param ClusterNode - The initial cluster node the user wants to add the viewport to, or null to use the first cluster node in the cluster.
	 * @param PresetRect - The initial rectangle to configure the new viewport with in the dialog box.
	 * @return The newly created viewport, or null if the user cancelled out of the dialog box.
	 */
	static UDisplayClusterConfigurationViewport* CreateNewViewportFromDialog(const TSharedRef<FDisplayClusterConfiguratorBlueprintEditor>& Toolkit, UDisplayClusterConfigurationClusterNode* ClusterNode, const FDisplayClusterConfigurationRectangle& PresetRect);

	/**
	 * Displays a dialog box used to configure new cluster item properties before adding the new item to the cluster configuration.
	 * @param DialogContent - The widget to display in the dialog box's main section.
	 * @param ParentElement - The UI element to display the dialog box from. If null, will use the active window as a parent.
	 * @param WindowTitle - The title displayed at the top of the dialog box.
	 * @param WindowSize - The size of the dialog box.
	 */
	static void ShowNewClusterItemDialogWindow(TSharedRef<SDisplayClusterConfiguratorNewClusterItemDialog> DialogContent, TSharedPtr<SWidget> ParentElement, FText WindowTitle, FVector2D WindowSize);

	/**
	 * Attempts to create an appropriate drag and drop operation for the selected objects.
	 * @param SelectedObjects - The objects to create the drag and drop operation for.
	 * @return A valid pointer to the drag and drop operation if an operation could be made, and invalid pointer if not.
	 */
	static TSharedPtr<FDragDropOperation> MakeDragDropOperation(const TArray<UObject*>& SelectedObjects);

	/**
	 * Finds the next available position for a cluster node within a host so that the cluster node doesn't overlap any existing nodes.
	 * @param Cluster - The cluster to find the position within.
	 * @param DesiredHost - The host the cluster node wants to be positioned in.
	 * @param DesiredPosition - The desired position of the cluster node.
	 * @param DesiredSize - The desired size of the cluster node.
	 */
	static FVector2D FindNextAvailablePositionForClusterNode(UDisplayClusterConfigurationCluster* Cluster, const FString& DesiredHost, const FVector2D& DesiredPosition, const FVector2D& DesiredSize);

	/**
	 * Finds the next available position for a viewport within a cluster node so that the viewport doesn't overlap any existing viewports.
	 * @param ClusterNode - The cluster node to find the position within.
	 * @param DesiredPosition - The desired position of the viewport.
	 * @param DesiredSize - The desired size of the viewport.
	 */
	static FVector2D FindNextAvailablePositionForViewport(UDisplayClusterConfigurationClusterNode* ClusterNode, const FVector2D& DesiredPosition, const FVector2D& DesiredSize);

	/**
	 * Sorts a list of cluster nodes by their host.
	 * @param InClusterNodes - The list of nodes to sort
	 * @param OutSortedNodes - The sorted nodes, indexed by the Host property of the cluster nodes
	 */
	static void SortClusterNodesByHost(const TMap<FString, UDisplayClusterConfigurationClusterNode*>& InClusterNodes, TMap<FString, TMap<FString, UDisplayClusterConfigurationClusterNode*>>& OutSortedNodes);

	/**
	 * Sorts a list of cluster nodes by their host.
	 * @param InClusterNodes - The list of nodes to sort
	 * @param OutSortedNodes - The sorted nodes, indexed by the Host property of the cluster nodes
	 */
	static void SortClusterNodesByHost(const TMap<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& InClusterNodes, TMap<FString, TMap<FString, UDisplayClusterConfigurationClusterNode*>>& OutSortedNodes);

	/**
	 * Attempts to find a preexisting display data object for the specified host, and if one can't be found, creates a new one.
	 * @param Cluster - The cluster to find the host display data object in
	 * @param HostIPAddress - The IP address of the host to find the display data for
	 * @return The display data that was found or created
	 */
	static UDisplayClusterConfigurationHostDisplayData* FindOrCreateHostDisplayData(UDisplayClusterConfigurationCluster* Cluster, FString HostIPAddress);

	/**
	 * Removes all host display data objects from the cluster that aren't being used by any of the cluster nodes.
	 * @param Cluster - The cluster to clean up the host display data for
	 * @return true if any host display data was removed; otherwise, false
	 */
	static bool RemoveUnusedHostDisplayData(UDisplayClusterConfigurationCluster* Cluster);

	/**
	 * Gets a unique name for a host owned by the specified cluster.
	 * @param InitialName - The initial name the host wants
	 * @param ParentCluster - The cluster the host belongs to
	 * @param bAddZero - Whether to add an "_0" to the initial name if it is unique
	 * @returns A unique name for the host
	 */
	static FString GetUniqueNameForHost(FString InitialName, UDisplayClusterConfigurationCluster* ParentCluster, bool bAddZero = false);

	/**
	 * Gets the address of a host using its host display data.
	 * @param HostDisplayData - The display data of the host to get the address for
	 * @return The address of the host, or an empty string if no host address was found
	 */
	static FString GetAddressForHost(UDisplayClusterConfigurationHostDisplayData* HostDisplayData);

	/**
	 * Removes all cluster nodes and host display data from the cluster for the specified host.
	 * @param Cluster - The cluster to remove the host from.
	 * @param Host - The host to remove.
	 * @return true if any cluster nodes or host display data was removed; otherwise, false
	 */
	static bool RemoveHost(UDisplayClusterConfigurationCluster* Cluster, FString Host);

	/**
	 * Attempts to get the host display data for the host of a cluster node.
	 * @param ClusterNode - The cluster node whose host to get the display data for.
	 * @return The display data for the cluster node's host, or null if no display data was found.
	 */
	static UDisplayClusterConfigurationHostDisplayData* GetHostDisplayDataForClusterNode(UDisplayClusterConfigurationClusterNode* ClusterNode);

	/**
	 * Gets the name that the cluster node is stored under in its parent cluster.
	 * @return The cluster node's storage name
	 */
	static FString GetClusterNodeName(UDisplayClusterConfigurationClusterNode* ClusterNode);

	/**
	 * Determines if a cluster node is the primary node in its cluster.
	 * @param ClusterNode - The cluster node to check.
	 * @return true if the node is the primary; otherwise, false.
	 */
	static bool IsClusterNodePrimary(UDisplayClusterConfigurationClusterNode* ClusterNode);

	/**
	 * Gets a unique name for a cluster node owned by the specified cluster.
	 * @param InitialName - The initial name the cluster node wants
	 * @param ParentCluster - The cluster the cluster node belongs to
	 * @param bAddZero - Whether to add an "_0" to the initial name if it is unique
	 * @returns A unique name for the cluster node
	 */
	static FString GetUniqueNameForClusterNode(FString InitialName, UDisplayClusterConfigurationCluster* ParentCluster, bool bAddZero = false);

	/**
	 * Adds a cluster node to a cluster, removing it from its current cluster if it belongs to one.
	 * @param ClusterNode - The cluster node to add
	 * @param Cluster - The cluster to add the cluster node to
	 * @param NewClusterNodeName - Optional name to add the new cluster node under in the cluster. If not supplied, the cluster node's current name is used, if possible.
	 * @return An updated pointer to the cluster node added to the cluster.
	 */
	static DISPLAYCLUSTERCONFIGURATOR_API UDisplayClusterConfigurationClusterNode* AddClusterNodeToCluster(UDisplayClusterConfigurationClusterNode* ClusterNode, UDisplayClusterConfigurationCluster* Cluster, FString NewClusterNodeName = "");

	/**
	 * Removes a cluster node from the cluster that owns it. The owning cluster is retrieved using the cluster node's Outer.
	 * @param ClusterNode - The cluster node to remove
	 * @returns true if the cluster node was successfully removed from its owning cluster
	 */
	static DISPLAYCLUSTERCONFIGURATOR_API bool RemoveClusterNodeFromCluster(UDisplayClusterConfigurationClusterNode* ClusterNode);

	/**
	 * Renames a cluster node, which involves changing the key that the cluster node is stored under in its parent cluster.
	 * @param ClusterNode - The cluster node to rename
	 * @param NewClusterNodeName - The desired name to change to. The final name will be a unique-ified version of this name.
	 * @return true if the cluster node was successfully renamed
	 */
	static bool RenameClusterNode(UDisplayClusterConfigurationClusterNode* ClusterNode, FString NewClusterNodeName);

	/**
	 * Sets the cluster node as the primary node.
	 * @param ClusterNode - The cluster node to set as the primary node
	 * @return true if the cluster node was successfully set as the primary node
	 */
	static bool DISPLAYCLUSTERCONFIGURATOR_API SetClusterNodeAsPrimary(UDisplayClusterConfigurationClusterNode* ClusterNode);

	/**
	 * Gets the name that the viewport is stored under in its parent cluster node.
	 * @return The viewport's storage name
	 */
	static FString GetViewportName(UDisplayClusterConfigurationViewport* Viewport);

	/**
	 * Gets a unique name for a viewport owned by the specified cluster node.
	 * @param InitialName - The initial name the viewport wants
	 * @param ParentClusterNode - The cluster node the viewport belongs to
	 * @param bAddZero - Whether to add an "_0" to the initial name if it is unique
	 * @returns A unique name for the viewport
	 */
	static FString GetUniqueNameForViewport(FString InitialName, UDisplayClusterConfigurationClusterNode* ParentClusterNode, bool bAddZero = false);

	/**
	 * Adds a viewport to a cluster node, removing it from its current cluster node if it belongs to one.
	 * @param Viewport - The viewport to add
	 * @param ClusterNode - The cluster node to add the viewport to
	 * @param NewViewportName - Optional name to add the new viewport under in the cluster node. If not supplied, the viewport's current name is used, if possible.
	 * @return An updated pointer to the viewport added to the cluster.
	 */
	static DISPLAYCLUSTERCONFIGURATOR_API UDisplayClusterConfigurationViewport* AddViewportToClusterNode(UDisplayClusterConfigurationViewport* Viewport, UDisplayClusterConfigurationClusterNode* ClusterNode, FString NewViewportName = "");

	/**
	 * Removes a viewport from the cluster node that owns it. The owning cluster node is retrieved using the viewport's Outer.
	 * @param Viewport - The viewport to remove
	 * @returns true if the viewport was successfully removed from its owning cluster
	 */
	static bool RemoveViewportFromClusterNode(UDisplayClusterConfigurationViewport* Viewport);

	/**
	 * Renames a viewport, which involves changing the key that the viewport is stored under in its parent cluster node.
	 * @param Viewport - The viewport to rename
	 * @param NewViewportName - The desired name to change to. The final name will be a unique-ified version of this name.
	 * @return true if the viewport was successfully renamed
	 */
	static bool RenameViewport(UDisplayClusterConfigurationViewport* Viewport, FString NewViewportName);

	/**
	 * Copies the specified list of cluster items to the clipboard.
	 * @param ClusterItemsToCopy - The list of cluster items to copy.
	 */
	static void CopyClusterItemsToClipboard(const TArray<UObject*>& ClusterItemsToCopy);

	/**
	 * Checks to see if the clipboard contains cluster items that can be pasted into the target cluster items.
	 * @param TargetClusterItems - The cluster items to paste the copied cluster items into.
	 * @param OutNumItems - The number of cluster items that can be pasted from the clipboard.
	 */
	static bool CanPasteClusterItemsFromClipboard(const TArray<UObject*>& TargetClusterItems, int32& OutNumItems);

	/**
	 * Pastes any cluster items in the clipboard into the specified target items, if the target items can accept the pasted items.
	 * @param TargetClusterItems - The cluster items to paste the copied cluster items into, if compatible.
	 * @param PasteLocation - Optional location to paste the cluster items at.
	 * @return The pasted cluster items.
	 */
	static TArray<UObject*> PasteClusterItemsFromClipboard(const TArray<UObject*>& TargetClusterItems, TOptional<FVector2D> PasteLocation = TOptional<FVector2D>());

private:
	/**
	 * Gets a unique name for something given a list of existing names.
	 * @param InitialName - The initial name the object wants
	 * @param UsedNames - The list of names that have already been used
	 * @param Class - The class to generate a name for
	 * @param Parent - The parent to check for in-memory objects that collide with the name
	 * @param bAddZero - Whether to add an "_0" to the initial name if it is unique
	 * @returns A unique name for the object
	 */
	static FString GetUniqueName(const FString& InitialName, const TArray<FString>& UsedNames, const UClass* Class, UObject* Parent, bool bAddZero = false);
	
	static const FVector2D NewClusterItemDialogSize;
	static const FString DefaultNewHostName;
	static const FString DefaultNewClusterNodeName;
	static const FString DefaultNewViewportName;
};