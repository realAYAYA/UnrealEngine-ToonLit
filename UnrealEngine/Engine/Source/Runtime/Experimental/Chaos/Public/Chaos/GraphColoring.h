// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/DynamicParticles.h"
#include "Chaos/Core.h"
#include "Chaos/Vector.h"
#include "Chaos/UniformGrid.h"

namespace Chaos
{

class FGraphColoring
{
	typedef TArray<int32, TInlineAllocator<8>> FColorSet;
	
	struct FGraphNode
	{
		FGraphNode()
			: NextColor(0)
		{
		}

		TArray<int32, TInlineAllocator<8>> Edges;
		int32 NextColor;
		FColorSet UsedColors;
	};

	struct FGraphEdge
	{
		FGraphEdge()
			: FirstNode(INDEX_NONE)
			, SecondNode(INDEX_NONE)
			, Color(INDEX_NONE)
		{
		}

		int32 FirstNode;
		int32 SecondNode;
		int32 Color;
	};

	struct FGraph3dEdge : FGraphEdge
	{
		FGraph3dEdge()
			: ThirdNode(INDEX_NONE)
		{
		}

		int32 ThirdNode;
	};

	struct FGraphTetEdge : FGraph3dEdge
	{
		FGraphTetEdge()
			: FourthNode(INDEX_NONE)
		{
		}

		int32 FourthNode;
	};

  public:
	template<typename T>
	CHAOS_API static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 2>>& Graph, const TDynamicParticles<T, 3>& InParticles);
	template<typename T>
	CHAOS_API static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 3>>& Graph, const TDynamicParticles<T, 3>& InParticles);
	template<typename T>
	CHAOS_API static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 4>>& Graph, const TDynamicParticles<T, 3>& InParticles);
	template<typename T>
	CHAOS_API static TArray<TArray<int32>> ComputeGraphColoringAllDynamic(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles);
};

template<typename T> 
CHAOS_API void ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<T>& Grid, const int32 GridSize, TArray<TArray<int32>>*& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors);

}
