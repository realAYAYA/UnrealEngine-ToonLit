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
	template<typename DynamicParticlesType>
	static TArray<TArray<int32>> ComputeGraphColoringParticlesOrRange(const TArray<TVector<int32, 2>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);

	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 2>>& Graph, const TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
	{
		return ComputeGraphColoringParticlesOrRange(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd);
	}
	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 2>>& Graph, const TDynamicParticles<T, 3>& InParticles)
	{
		return ComputeGraphColoringParticlesOrRange(Graph, InParticles, 0, InParticles.Size());
	}
	template<typename DynamicParticlesType>
	static TArray<TArray<int32>> ComputeGraphColoringParticlesOrRange(const TArray<TVector<int32, 3>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 3>>& Graph, const TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
	{
		return ComputeGraphColoringParticlesOrRange(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd);
	}
	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 3>>& Graph, const TDynamicParticles<T, 3>& InParticles)
	{
		return ComputeGraphColoringParticlesOrRange(Graph, InParticles, 0, InParticles.Size());
	}
	template<typename DynamicParticlesType>
	static TArray<TArray<int32>> ComputeGraphColoringParticlesOrRange(const TArray<TVector<int32, 4>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 4>>& Graph, const TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
	{
		return ComputeGraphColoringParticlesOrRange(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd);
	}
	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoring(const TArray<TVector<int32, 4>>& Graph, const TDynamicParticles<T, 3>& InParticles)
	{
		return ComputeGraphColoringParticlesOrRange(Graph, InParticles, 0, InParticles.Size());
	}
	template<typename DynamicParticlesType>
	static TArray<TArray<int32>> ComputeGraphColoringAllDynamicParticlesOrRange(const TArray<TVec4<int32>>& Graph, const DynamicParticlesType& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd);
	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoringAllDynamic(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd)
	{
		return ComputeGraphColoringAllDynamicParticlesOrRange(Graph, InParticles, GraphParticlesStart, GraphParticlesEnd);
	}
	template<typename T>
	inline static TArray<TArray<int32>> ComputeGraphColoringAllDynamic(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles)
	{
		return ComputeGraphColoringAllDynamicParticlesOrRange(Graph, InParticles, 0, InParticles.Size());
	}
};

template<typename T> 
CHAOS_API void ComputeGridBasedGraphSubColoringPointer(const TArray<TArray<int32>>& ElementsPerColor, const TMPMGrid<T>& Grid, const int32 GridSize, TArray<TArray<int32>>*& PreviousColoring, const TArray<TArray<int32>>& ConstraintsNodesSet, TArray<TArray<TArray<int32>>>& ElementsPerSubColors);


template<typename T>
CHAOS_API void ComputeWeakConstraintsColoring(const TArray<TArray<int32>>& Indices, const TArray<TArray<int32>>& SecondIndices, const Chaos::TDynamicParticles<T, 3>& InParticles, TArray<TArray<int32>>& ConstraintsPerColor);

template<typename T>
CHAOS_API TArray<TArray<int32>> ComputeNodalColoring(const TArray<TVec4<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex);

template<typename T>
CHAOS_API TArray<TArray<int32>> ComputeNodalColoring(const TArray<TArray<int32>>& Graph, const Chaos::TDynamicParticles<T, 3>& InParticles, const int32 GraphParticlesStart, const int32 GraphParticlesEnd, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& IncidentElementsLocalIndex, TArray<int32>* ParticleColorsOut = nullptr);

template<typename T>
CHAOS_API void ComputeExtraNodalColoring(const TArray<TArray<int32>>& Graph, const TArray<TArray<int32>>& ExtraGraph, const Chaos::TDynamicParticles<T, 3>& InParticles, const TArray<TArray<int32>>& IncidentElements, const TArray<TArray<int32>>& ExtraIncidentElements, TArray<int32>& ParticleColors, TArray<TArray<int32>>& ParticlesPerColor);

}
