// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/StaticSpatialIndex.h"
#include "Algo/Transform.h"

void FStaticSpatialIndex::FListImpl::Init(const TArray<TPair<FBox, uint32>>& InElements)
{
	Elements.Reserve(InElements.Num());
	Algo::Transform(InElements, Elements, [](const TPair<FBox, uint32>& Element) { return Element.Value; });
}

void FStaticSpatialIndex::FListImpl::ForEachElement(TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	for (uint32 ValueIndex : Elements)
	{
		Func(ValueIndex);
	}
}

void FStaticSpatialIndex::FListImpl::ForEachIntersectingElement(const FBox& InBox, TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	for (uint32 ValueIndex : Elements)
	{
		const FBox& Box = DataInterface.GetBox(ValueIndex);

		if (Box.Intersect(InBox))
		{
			Func(ValueIndex);
		}
	}
}

void FStaticSpatialIndex::FListImpl::ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	for (uint32 ValueIndex : Elements)
	{
		const FBox& Box = DataInterface.GetBox(ValueIndex);

		if (FMath::SphereAABBIntersection(InSphere, Box))
		{
			Func(ValueIndex);
		}
	}
}

void FStaticSpatialIndex::FRTreeImpl::Init(const TArray<TPair<FBox, uint32>>& InElements)
{
	if (InElements.Num())
	{
		const int32 MaxNumElementsPerNode = 16;
		const int32 MaxNumElementsPerLeaf = 64;

		TArray<TPair<FBox, uint32>> SortedElements = InElements;
		SortedElements.Sort([](const TPair<FBox, uint32>& A, const TPair<FBox, uint32>& B) { return A.Key.Min.X < B.Key.Min.X; });
	
		// build leaves
		FNode* CurrentNode = nullptr;
		TArray<FNode> Nodes;

		for (const TPair<FBox, uint32>& Element : SortedElements)
		{
			if (!CurrentNode || (CurrentNode->Content.Get<FNode::FLeafType>().Num() >= MaxNumElementsPerLeaf))
			{
				CurrentNode = &Nodes.Emplace_GetRef();
				CurrentNode->Content.Emplace<FNode::FLeafType>();
			}

			CurrentNode->Box += Element.Key;
			CurrentNode->Content.Get<FNode::FLeafType>().Add(Element.Value);
		}

		// build nodes
		while (Nodes.Num() > 1)
		{
			CurrentNode = nullptr;
			TArray<FNode> TopNodes;

			Nodes.Sort([](const FNode& A, const FNode& B) { return A.Box.Min.X < B.Box.Min.X; });

			for (FNode& Node : Nodes)
			{
				if (!CurrentNode || (CurrentNode->Content.Get<FNode::FNodeType>().Num() >= MaxNumElementsPerNode))
				{
					CurrentNode = &TopNodes.Emplace_GetRef();
					CurrentNode->Content.Emplace<FNode::FNodeType>();
				}

				CurrentNode->Box += Node.Box;
				CurrentNode->Content.Get<FNode::FNodeType>().Add(MoveTemp(Node));
			}

			Nodes = MoveTemp(TopNodes);
		}

		check(Nodes.Num() == 1);
		RootNode = MoveTemp(Nodes[0]);
	}
}

void FStaticSpatialIndex::FRTreeImpl::ForEachElement(TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	ForEachElementRecursive(&RootNode, Func);
}

void FStaticSpatialIndex::FRTreeImpl::ForEachElementRecursive(const FNode* Node, TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	if (Node->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
		{
			ForEachElementRecursive(&ChildNode, Func);
		}
	}
	else
	{
		for (uint32 ValueIndex : Node->Content.Get<FNode::FLeafType>())
		{
			Func(ValueIndex);
		}
	}
}

void FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElement(const FBox& InBox, TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	ForEachIntersectingElementRecursive(&RootNode, InBox, Func);
}

void FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElementRecursive(const FNode* Node, const FBox& InBox, TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	if (Node->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
		{
			if (ChildNode.Box.Intersect(InBox))
			{
				ForEachIntersectingElementRecursive(&ChildNode, InBox, Func);
			}
		}
	}
	else
	{
		for (uint32 ValueIndex : Node->Content.Get<FNode::FLeafType>())
		{
			const FBox& Box = DataInterface.GetBox(ValueIndex);

			if (Box.Intersect(InBox))
			{
				Func(ValueIndex);
			}
		}
	}
}

void FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	ForEachIntersectingElementRecursive(&RootNode, InSphere, Func);
}

void FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElementRecursive(const FNode* Node, const FSphere& InSphere, TFunctionRef<void(uint32 InValueIndex)> Func) const
{
	if (Node->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
		{
			if (FMath::SphereAABBIntersection(InSphere, ChildNode.Box))
			{
				ForEachIntersectingElementRecursive(&ChildNode, InSphere, Func);
			}
		}
	}
	else
	{
		for (uint32 ValueIndex : Node->Content.Get<FNode::FLeafType>())
		{
			const FBox& Box = DataInterface.GetBox(ValueIndex);

			if (FMath::SphereAABBIntersection(InSphere, Box))
			{
				Func(ValueIndex);
			}
		}
	}
}