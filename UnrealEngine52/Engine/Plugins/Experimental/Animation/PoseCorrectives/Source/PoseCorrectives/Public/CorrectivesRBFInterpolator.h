// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RBF/RBFInterpolator.h"

// An implementation detail for the RBF interpolator to hide the use of Eigen from components
// outside AnimGraphRuntime.
class FCorrectivesRBFInterpolatorBase
{
protected:
	POSECORRECTIVES_API bool SetFullKernel(const TArrayView<float>& FullKernel, int32 Size);

	// A square matrix of the solved coefficients.
public:
	TArray<float> Coeffs;
	bool bIsValid = false;
};


template<typename T>
class TCorrectivesRBFInterpolator
	: public FCorrectivesRBFInterpolatorBase
{
public:
	using WeightFuncT = TFunction<float(const T& A, const T& B)>;

	TCorrectivesRBFInterpolator() = default;

	/* Construct an RBF interpolator, taking in a set of sparse nodes and a symmetric weighing
	   function that computes the distance between two nodes, and, optionally, smooths 
	   the distance with a smoothing kernel.
	*/
	TCorrectivesRBFInterpolator(
		const TArrayView<T>& InNodes,
		WeightFuncT InWeightFunc)
		: Nodes(InNodes)
		, WeightFunc(InWeightFunc)
	{
		MakeFullKernel();
	}

	TCorrectivesRBFInterpolator(const TCorrectivesRBFInterpolator<T>&) = default;
	TCorrectivesRBFInterpolator(TCorrectivesRBFInterpolator<T>&&) = default;
	TCorrectivesRBFInterpolator<T>& operator=(const TCorrectivesRBFInterpolator<T>&) = default;
	TCorrectivesRBFInterpolator<T>& operator=(TCorrectivesRBFInterpolator<T>&&) = default;

	/* Given a value, compute the weight values to use to calculate each node's contribution
	   to that value's location.
	*/
	template<typename U, typename InAllocator>
	void Interpolate(
		TArray<float, InAllocator>& OutWeights,
		const U& Value,
		bool bClip = true,
		bool bNormalize = false) const
	{
		int NumNodes = Nodes.Num();

		if (!bIsValid)
		{
			OutWeights.Init(0.0f, NumNodes);
			return;
		}	

		if (NumNodes > 1)
		{
			TArray<float, TMemStackAllocator<> > ValueWeights;
			ValueWeights.SetNum(Nodes.Num());

			for (int32 i = 0; i < NumNodes; i++)
			{
				ValueWeights[i] = WeightFunc(Nodes[i], Value);
			}

			OutWeights.Reset(NumNodes);
			for (int32 i = 0; i < NumNodes; i++)
			{
				const float* C = &Coeffs[i * NumNodes];
				float W = 0.0f;

				for (int32 j = 0; j < NumNodes; j++)
				{
					W += C[j] * ValueWeights[j];
				}

				OutWeights.Add(W);
			}


			if (bNormalize)
			{
				// Clip here behaves differently than it does when no normalization
				// is taking place. Instead of clipping blindly, we rescale the values based
				// on the minimum value and then use the normalization to bring the values
				// within the 0-1 range.
				if (bClip)
				{
					float MaxNegative = 0.0f;
					for (int32 i = 0; i < NumNodes; i++)
					{
						if (OutWeights[i] < MaxNegative)
							MaxNegative = OutWeights[i];
					}
					for (int32 i = 0; i < NumNodes; i++)
					{
						OutWeights[i] += MaxNegative;
					}
				}

				float TotalWeight = 0.0f;
				for (int32 i = 0; i < NumNodes; i++)
				{
					TotalWeight += OutWeights[i];
				}
				for (int32 i = 0; i < NumNodes; i++)
				{
					// Clamp to clear up any precision issues. This may make the weights not
					// quite add up to 1.0, but that should be sufficient for our needs.
					OutWeights[i] = FMath::Clamp(OutWeights[i] / TotalWeight, 0.0f, 1.0f);
				}
			}
			else if (bClip)
			{
				// This can easily happen when the value being interpolated is outside of the
				// convex hull bounded by the nodes, resulting in an extrapolation.
				for (int32 i = 0; i < NumNodes; i++)
				{
					OutWeights[i] = FMath::Clamp(OutWeights[i], 0.0f, 1.0f);
				}
			}
		}
		else if (NumNodes == 1)
		{
			OutWeights.Reset(1);
			OutWeights.Add(1);
		}
		else
		{
			OutWeights.Reset(0);
		}
	}

	// Returns a list of integer pairs indicating which distinct pair of nodes have the same
	// weight as a pair of the same node. These result in an ill-formed coefficient matrix
	// which kills the interpolation. The user can then either simply remove one of the pairs
	// and retry, or warn the user that they have an invalid setup.
	static bool GetIdenticalNodePairs(
		const TArrayView<T>& InNodes,
		WeightFuncT InWeightFunc,
		TArray<TTuple<int, int>>& OutInvalidPairs
		) 
	{
		int NumNodes = InNodes.Num();
		if (NumNodes < 2)
		{
			return false;		
		}

		// One of the assumptions we make, is that the smoothing function is symmetric, 
		// hence we can use the weight between the same node as the functional equivalent
		// of the identity weight between any two nodes.
		float IdentityWeight = InWeightFunc(InNodes[0], InNodes[0]);

		OutInvalidPairs.Empty();
		for (int32 i = 0; i < (NumNodes - 1); i++)
		{
			for (int32 j = i + 1; j < NumNodes; j++)
			{
				float Weight = InWeightFunc(InNodes[i], InNodes[j]);

				// Don't use the default ULP, but be a little more cautious, since a matrix
				// inversion can lose a chunk of float precision.
				if (FMath::IsNearlyEqualByULP(Weight, IdentityWeight, 32))
				{
					OutInvalidPairs.Add(MakeTuple(i, j));
				}
			}
		}
		return OutInvalidPairs.Num() != 0;
	}

private:
	void MakeFullKernel()
	{
		// If there are less than two nodes, nothing to do, since the interpolated value
		// will be the same across the entire space. This is handled in Interpolate().
		int32 NumNodes = Nodes.Num();
		if (NumNodes < 2)
		{
			bIsValid = true;
			return;
		}

		// Need to construct a full matrix as the distances aren't symetrical anymore
		TArray<float, TMemStackAllocator<> > FullKernel;
		FullKernel.Reserve(NumNodes * NumNodes);

		for (int32 i = 0; i < NumNodes; i++)
		{
			for (int32 j = 0; j < NumNodes; j++) 
			{
				FullKernel.Add(WeightFunc(Nodes[i], Nodes[j]));
			}
		}

		bIsValid = SetFullKernel(FullKernel, NumNodes);
	}


	TArrayView<T> Nodes;
	WeightFuncT WeightFunc;
};
