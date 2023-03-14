// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"

#include "EntitySystem/MovieSceneEntitySystemTypes.h"

struct MOVIESCENE_API FMovieSceneEntitySystemDirectedGraph
{
	struct FDirectionalEdge
	{
		explicit FDirectionalEdge(uint16 InFromNode, uint16 InToNode)
			: FromNode(InFromNode)
			, ToNode(InToNode)
		{}

		friend bool operator==(const FDirectionalEdge& A, const FDirectionalEdge& B)
		{
			return A.FromNode == B.FromNode && A.ToNode == B.ToNode;
		}

		friend bool operator!=(const FDirectionalEdge& A, const FDirectionalEdge& B)
		{
			return !(A == B);
		}

		friend bool operator<(const FDirectionalEdge& A, const FDirectionalEdge& B)
		{
			if (A.FromNode == B.FromNode)
			{
				return A.ToNode < B.ToNode;
			}
			return A.FromNode < B.FromNode;
		}

		uint16 FromNode;

		uint16 ToNode;
	};

public:

	struct MOVIESCENE_API FDepthFirstSearch
	{
		TArray<uint16> PostNodes;

		explicit FDepthFirstSearch(const FMovieSceneEntitySystemDirectedGraph* InGraph);

		void Search(uint16 Node);

		const TBitArray<>& GetVisited() const
		{
			return Visited;
		}

	private:

		TBitArray<> Visited;
		TBitArray<> IsVisiting;

		const FMovieSceneEntitySystemDirectedGraph* Graph;
	};

	struct MOVIESCENE_API FBreadthFirstSearch
	{
		TArray<uint16> Nodes;

		explicit FBreadthFirstSearch(const FMovieSceneEntitySystemDirectedGraph* InGraph);

		void Search(uint16 Node);

		const TBitArray<>& GetVisited() const
		{
			return Visited;
		}

	private:

		TBitArray<> Visited;
		const FMovieSceneEntitySystemDirectedGraph* Graph;
		int32 StackIndex;
	};

	struct MOVIESCENE_API FDiscoverCyclicEdges
	{
		explicit FDiscoverCyclicEdges(const FMovieSceneEntitySystemDirectedGraph* InGraph);

		void Search(uint16 Node);

		bool IsCyclic(const uint16 EdgeIndex) const
		{
			return CyclicEdges.IsValidIndex(EdgeIndex) && CyclicEdges[EdgeIndex] == true;
		}

		const TBitArray<>& GetCyclicEdges() const
		{
			return CyclicEdges;
		}

		void Search();
		void SearchFrom(uint16 NodeID);

	private:

		void DiscoverCycles(uint16 NodeID, TBitArray<>& VisitedNodes);
		void TagCyclicChain(uint16 CyclicNodeID);

		TBitArray<> CyclicEdges;
		TBitArray<> VisitedEdges;
		TArray<uint16, TInlineAllocator<16>> EdgeChain;
		const FMovieSceneEntitySystemDirectedGraph* Graph;
	};

public:

	FMovieSceneEntitySystemDirectedGraph()
		: bHasDanglingEdges(false)
	{}

	void AllocateNode(uint16 NodeID);

	bool IsNodeAllocated(uint16 NodeID) const;

	void CleanUpDanglingEdges();

	void RemoveNode(uint16 NodeID);

	const TBitArray<>& GetNodeMask() const
	{
		return Nodes;
	}

	bool IsCyclic() const;

	void MakeEdge(uint16 FromNode, uint16 ToNode);

	void DestroyEdge(uint16 FromNode, uint16 ToNode);

	TBitArray<> FindEdgeUpstreamNodes() const;

	TArrayView<const FDirectionalEdge> GetEdges() const;

	TArrayView<const FDirectionalEdge> GetEdgesFrom(uint16 InNode) const;

	bool HasEdgeFrom(uint16 InNode) const;

	bool HasEdgeTo(uint16 InNode) const;

private:

	int32 FindEdgeStart(uint16 FromNode) const;

	int32 FindEdgeIndex(const FDirectionalEdge& Edge) const;

	bool EdgeExists(const FDirectionalEdge& Edge) const;

	bool IsCyclicImpl(uint16 NodeID, TBitArray<>& Visiting) const;

private:

	friend FDepthFirstSearch;
	friend FBreadthFirstSearch;
	friend FDiscoverCyclicEdges;

	TBitArray<> Nodes;
	TArray<FDirectionalEdge> SortedEdges;
	bool bHasDanglingEdges;
};


