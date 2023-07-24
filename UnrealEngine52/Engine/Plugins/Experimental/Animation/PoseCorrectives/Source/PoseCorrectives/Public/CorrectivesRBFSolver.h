// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RBF/RBFSolver.h"
#include "CorrectivesRBFSolver.generated.h"


/** Struct storing a particular entry within the RBF */
USTRUCT()
struct POSECORRECTIVES_API FCorrectivesRBFEntry
{
	GENERATED_BODY()

	/** Set of values for this target, size must be TargetDimensions  */
	UPROPERTY(EditAnywhere, Category = RBFData)
	TArray<float> Values;

	/** Set of indicies that reference the first element in a vector contained in Values. These indicies form the subset of Vectors in Values that are considered when calculating its distance to other entries.*/
	TArray<int32> VectorDistanceBaseIndices;

	/** Set of indicies that reference an element in Values. These indicies form the subset of scalars in Values that are considered when calculating its distance to other entries.*/
	TArray<int32> ScalarDistanceIndices;


	/** Return a target as an rotator, assuming Values is a sequence of Euler entries. Index is which Euler to convert.*/
	FRotator AsRotator(int32 Index) const;

	/** Return a target as a quaternion, assuming Values is a sequence of Euler entries. Index is which Euler to convert. */
	FQuat AsQuat(int32 Index) const;

	FVector AsVector(int32 Index) const;


	/** Adds 3 floats to the entry from supplied rotator */
	void AddFromRotator(const FRotator& InRot, bool bUseForDistance = true);
	/** Adds 3 floats to the entry from supplied vector */
	void AddFromVector(const FVector& InVector, bool bUseForDistance = true);
	/** Adds float to the entry */
	void AddFromScalar(float InScalar, bool bUseForDistance = true);

	/** Return dimensionality of this target */
	int32 GetDimensions() const
	{
		return Values.Num();
	}

	void Reset();
};

/** Data about a particular target in the RBF, including scaling factor */
USTRUCT()
struct POSECORRECTIVES_API FCorrectivesRBFTarget : public FCorrectivesRBFEntry
{
	GENERATED_BODY()

	/** How large the influence of this target. */
	UPROPERTY(EditAnywhere, Category = RBFData)
	float ScaleFactor;

	/** Whether we want to apply an additional custom curve when activating this target. 
	    Ignored if the solver type is Interpolative. 
	*/
	UPROPERTY(EditAnywhere, Category = RBFData)
	bool bApplyCustomCurve;

	/** Custom curve to apply to activation of this target, if bApplyCustomCurve is true.
		Ignored if the solver type is Interpolative. */
	UPROPERTY(EditAnywhere, Category = RBFData)
	FRichCurve CustomCurve;

	/** Override the distance method used to calculate the distance from this target to
		the input. Ignored if the solver type is Interpolative. */
	UPROPERTY(EditAnywhere, Category = RBFData)
	ERBFDistanceMethod DistanceMethod;

	/** Override the falloff function used to smooth the distance from this target to
		the input. Ignored if the solver type is Interpolative. */
	UPROPERTY(EditAnywhere, Category = RBFData)
	ERBFFunctionType FunctionType;

	FCorrectivesRBFTarget()
		: ScaleFactor(1.f)
		, bApplyCustomCurve(false)
		, DistanceMethod(ERBFDistanceMethod::DefaultMethod)
		, FunctionType(ERBFFunctionType::DefaultFunction)
	{}
};

struct POSECORRECTIVES_API FCorrectivesRBFSolverData;

/** Library of Radial Basis Function solver functions */
struct POSECORRECTIVES_API FCorrectivesRBFSolver
{
	/** Given a list of targets, verify which ones are valid for solving the RBF setup. This is mostly about removing identical targets
		which invalidates the interpolative solver. Returns true if all targets are valid. */
	static bool ValidateTargets(const FRBFParams& Params, const TArray<FCorrectivesRBFTarget>& Targets, TArray<int>& InvalidTargets);

	/** Given a set of targets and new input entry, give list of activated targets with weights */
	static TSharedPtr<const FCorrectivesRBFSolverData> InitSolver(const FRBFParams& Params, const TArray<FCorrectivesRBFTarget>& Targets);

	static bool IsSolverDataValid(const FCorrectivesRBFSolverData& SolverData, const FRBFParams& Params, const TArray<FCorrectivesRBFTarget>& Targets);

	/** Given a set of targets and new input entry, give list of activated targets with weights */
	static void Solve(const FCorrectivesRBFSolverData& SolverData, const FRBFParams& Params, const TArray<FCorrectivesRBFTarget>& Targets, const FCorrectivesRBFEntry& Input, TArray<FRBFOutputWeight>& OutputWeights);

	/** Util to find distance to nearest neighbour target for each target */
	static bool FindTargetNeighbourDistances(const FRBFParams& Params, const TArray<FCorrectivesRBFTarget>& Targets, TArray<float>& NeighbourDists);

	/** Util to find distance from entry A to entry B, using provided method for rotations */
	static float FindDistanceBetweenEntries(const FCorrectivesRBFEntry& A, const FCorrectivesRBFEntry& B, const FRBFParams& Params, ERBFDistanceMethod OverrideMethod = ERBFDistanceMethod::DefaultMethod);

	/** Returns the radius for a given target */
	static float GetRadiusForTarget(const FCorrectivesRBFTarget& Target, const FRBFParams& Params);

	/** Compute the optimal radius for the given targets. Returns the radius */
	static float GetOptimalRadiusForTargets(const FRBFParams& Params, const TArray<FCorrectivesRBFTarget>& Targets);
};
