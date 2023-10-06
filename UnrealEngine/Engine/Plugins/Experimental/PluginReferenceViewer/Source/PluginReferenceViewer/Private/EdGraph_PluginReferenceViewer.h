// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "PluginReferencePinCategory.h"

#include "EdGraph_PluginReferenceViewer.generated.h"

class IPlugin;
class UEdGraphNode_PluginReference;
class SPluginReferenceViewer;

struct FPluginIdentifier
{
	FName Name;

	FPluginIdentifier()
		: Name(NAME_None)
	{
	}

	FPluginIdentifier(FName InName)
		: Name(InName)
	{
	}

	friend inline bool operator==(const FPluginIdentifier& A, const FPluginIdentifier& B)
	{
		return A.Name == B.Name;
	}

	friend inline uint32 GetTypeHash(const FPluginIdentifier& Key)
	{
		return GetTypeHash(Key.Name);
	}

	bool IsValid() const
	{
		return Name != NAME_None;
	}

	FString ToString() const
	{
		return Name.ToString();
	}

	static FPluginIdentifier FromString(const FString& String)
	{
		return FPluginIdentifier(FName(String));
	}
};

struct FPluginDependsNode
{
	explicit FPluginDependsNode(const FPluginIdentifier& InIdentifier)
		: Identifier(InIdentifier)
	{
	}

	FPluginIdentifier Identifier;
	TArray<FPluginDependsNode*> Dependencies;
	TArray<FPluginDependsNode*> Referencers;
};

struct FPluginReferenceNodeInfo
{
	FPluginIdentifier Identifier;

	TSharedPtr<IPlugin> Plugin;

	TArray<TPair<FPluginIdentifier, EPluginReferencePinCategory>> Children;

	TArray<FPluginIdentifier> Parents;

	bool bReferencers;

	FPluginReferenceNodeInfo(const FPluginIdentifier& InIdentifier, bool bInReferencers);

	bool IsFirstParent(const FPluginIdentifier& InParentId) const;

	// The Provision Size, or vertical spacing required for layout, for a given parent.  
	int32 ProvisionSize(const FPluginIdentifier& InParentId) const;

	// how many nodes worth of children require vertical spacing 
	int32 ChildProvisionSize;
};

UCLASS()
class UEdGraph_PluginReferenceViewer : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:
	void CachePluginDependencies(const TArray<TSharedRef<IPlugin>>& Plugins);

	/** Set reference viewer to focus on these assets */
	void SetGraphRoot(const TArray<FPluginIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin = FIntPoint(ForceInitToZero));

	const TArray<FPluginIdentifier>& GetCurrentGraphRootIdentifiers() const;

	UEdGraphNode_PluginReference* ConstructNodes(const TArray<FPluginIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin);

	UEdGraphNode_PluginReference* RecursivelyCreateNodes(bool bInReferencers, const FPluginIdentifier& InPluginId, const FIntPoint& InNodeLoc, const FPluginIdentifier& InParentId, UEdGraphNode_PluginReference* InParentNode, TMap<FPluginIdentifier, FPluginReferenceNodeInfo>& InNodeInfos, int32 InCurrentDepth, int32 InMaxDepth, bool bIsRoot = false);

	void RecursivelyPopulateNodeInfos(bool bInReferencers, const TArray<FPluginIdentifier>& Identifiers, TMap<FPluginIdentifier, FPluginReferenceNodeInfo>& InNodeInfos, int32 InCurrentDepth, int32 InMaxDepth);

	void RecursivelyFilterNodeInfos(const FPluginIdentifier& InPluginId, TMap<FPluginIdentifier, FPluginReferenceNodeInfo>& InNodeInfos, int32 InCurrentDepth, int32 InMaxDepth);

	void GetSortedLinks(const TArray<FPluginIdentifier>& Identifiers, bool bReferencers, TMap<FPluginIdentifier, EPluginReferencePinCategory>& OutLinks) const;

	/** Force the graph to rebuild */
	UEdGraphNode_PluginReference* RebuildGraph();

	UEdGraphNode_PluginReference* CreatePluginReferenceNode();
	void RemoveAllNodes();

private:
	struct FPluginDependency
	{
		FPluginIdentifier Identifier;
		bool bIsEnabled;
		bool bIsOptional;

		FPluginDependency(FPluginIdentifier InIdentifier, bool bInIsEnabled, bool bInIsOptional)
			: Identifier(InIdentifier)
			, bIsEnabled(bInIsEnabled)
			, bIsOptional(bInIsOptional)
		{
		}
	};

	void SetPluginReferenceViewer(TSharedPtr<SPluginReferenceViewer> InViewer);

	void GetPluginDependencies(const FPluginIdentifier& PluginIdentifier, TArray<FPluginDependency>& OutDependencies) const;
	void GetPluginReferencers(const FPluginIdentifier& PluginIdentifier, TArray<FPluginDependency>& OutReferencers) const;

	UEdGraphNode_PluginReference* RefilterGraph();

private:
	/** Editor for this pool */
	TWeakPtr<SPluginReferenceViewer> PluginReferenceViewer;

	TArray<FPluginIdentifier> CurrentGraphRootIdentifiers;
	FIntPoint CurrentGraphRootOrigin;

	TMap<FPluginIdentifier, TUniquePtr<FPluginDependsNode>> CachedDependsNodes;
	TMap<FPluginIdentifier, const TSharedRef<IPlugin>> PluginMap;

	TMap<FPluginIdentifier, FPluginReferenceNodeInfo> ReferencerNodeInfos;
	TMap<FPluginIdentifier, FPluginReferenceNodeInfo> DependencyNodeInfos;

	friend SPluginReferenceViewer;
};