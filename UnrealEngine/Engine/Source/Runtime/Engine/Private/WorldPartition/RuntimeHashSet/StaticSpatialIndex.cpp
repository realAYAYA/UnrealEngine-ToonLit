// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/StaticSpatialIndex.h"
#include "Algo/Transform.h"

void FStaticSpatialIndex::FListImpl::Init(const TArray<TPair<FBox, uint32>>& InElements)
{
	Elements.Reserve(InElements.Num());
	Algo::Transform(InElements, Elements, [](const TPair<FBox, uint32>& Element) { return Element.Value; });
}

bool FStaticSpatialIndex::FListImpl::ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	for (uint32 ValueIndex : Elements)
	{
		if (!Func(ValueIndex))
		{
			return false;
		}
	}
	return true;
}

bool  FStaticSpatialIndex::FListImpl::ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	for (uint32 ValueIndex : Elements)
	{
		const FBox& Box = DataInterface.GetBox(ValueIndex);

		if (Box.Intersect(InBox))
		{
			if (!Func(ValueIndex))
			{
				return false;
			}
		}
	}
	return true;
}

bool FStaticSpatialIndex::FListImpl::ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	for (uint32 ValueIndex : Elements)
	{
		const FBox& Box = DataInterface.GetBox(ValueIndex);

		if (FMath::SphereAABBIntersection(InSphere, Box))
		{
			if (!Func(ValueIndex))
			{
				return false;
			}
		}
	}
	return true;
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

bool FStaticSpatialIndex::FRTreeImpl::ForEachElement(TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	return ForEachElementRecursive(&RootNode, Func);
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachElementRecursive(const FNode* Node, TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	if (Node->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
		{
			if (!ForEachElementRecursive(&ChildNode, Func))
			{
				return false;
			}
		}
	}
	else
	{
		for (uint32 ValueIndex : Node->Content.Get<FNode::FLeafType>())
		{
			if (!Func(ValueIndex))
			{
				return false;
			}
		}
	}
	return true;
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElement(const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	return ForEachIntersectingElementRecursive(&RootNode, InBox, Func);
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElementRecursive(const FNode* Node, const FBox& InBox, TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	if (Node->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
		{
			if (ChildNode.Box.Intersect(InBox))
			{
				if (!ForEachIntersectingElementRecursive(&ChildNode, InBox, Func))
				{
					return false;
				}
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
				if (!Func(ValueIndex))
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElement(const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	return ForEachIntersectingElementRecursive(&RootNode, InSphere, Func);
}

bool FStaticSpatialIndex::FRTreeImpl::ForEachIntersectingElementRecursive(const FNode* Node, const FSphere& InSphere, TFunctionRef<bool(uint32 InValueIndex)> Func) const
{
	if (Node->Content.IsType<FNode::FNodeType>())
	{
		for (auto& ChildNode : Node->Content.Get<FNode::FNodeType>())
		{
			if (FMath::SphereAABBIntersection(InSphere, ChildNode.Box))
			{
				if (!ForEachIntersectingElementRecursive(&ChildNode, InSphere, Func))
				{
					return false;
				}
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
				if (!Func(ValueIndex))
				{
					return false;
				}
			}
		}
	}
	return true;
}