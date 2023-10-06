// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "FrameTypes.h"
#include "Util/ProgressCancel.h"

class FProgressCancel;

namespace UE
{
namespace Geometry
{

class FDynamicMesh3;


/**
 * FMeshSurfacePointSampling computes oriented point samples on the surface of
 * a Mesh using various sampling strategies.
 */
class FMeshSurfacePointSampling
{
public:
	// 
	// Basic sampling parameters
	//

	/** Desired "radius" of sample points. Spacing between samples is at least 2x this value. */
	double SampleRadius = 10.0;

	/** Maximum number of samples requested. If 0 or default value, mesh will be maximally sampled */
	uint32 MaxSamples = TNumericLimits<uint32>::Max();
	
	/** Density of subsampling used in Poisson strategy. Larger numbers mean "more accurate" (but slower) results. */
	double SubSampleDensity = 10.0;

	/** Random Seed used to initialize sampling strategies */
	int32 RandomSeed = 0;

	/** Safety threshold for maximum number of points in subsampling. */
	int32 MaxSubSamplePoints = 50000000;

	//
	// Parameters for non-uniform / variable-radius sampling. 
	// Disabled if MaxSampleRadius <= SampleRadius
	//

	/** If MaxSampleRadius > SampleRadius, then output point radii will be in range [SampleRadius, MaxSampleRadius] */
	double MaxSampleRadius = 0.0;

	/** Controls the distribution of sample radii */
	enum class ESizeDistribution
	{
		/** Uniform distribution of sizes, ie all equally likely */
		Uniform = 0,
		/** Distribution is weighted towards smaller points (T^Power) */
		Smaller = 1,
		/** Distribution is weighted towards larger points (T^1/Power) */
		Larger = 2
	};
	/** Active Size Distribution mode */
	ESizeDistribution SizeDistribution = ESizeDistribution::Uniform;

	/** Used to define how extreme the Size Distribution shift is. Valid range is [1,10] */
	double SizeDistributionPower = 2.0;


	/** Control whether VertexWeights (if valid) will be interpolated to modulate sampling */
	bool bUseVertexWeights = false;

	/** Per-vertex weights, size must be == Max Vertex Index of input Mesh */
	TArray<double> VertexWeights;

	/** Control how active Weights are used to affect point radius */
	enum class EInterpretWeightMode
	{
		/** Weights are clamped to [0,1] and used to interpolate Min/Max Radius */
		RadiusInterp = 0,
		/** Weights are clamped to [0,1] and used to interpolate Min/Max Radius, with decay, so that smaller-radius samples will infill between large ones */
		RadiusInterpWithFill = 1,
		/** Weight is used to create nonuniform random sampling, ie it nudges the random point radius distribution but does not directly control it */
		WeightedRandom = 2
	};
	/** Active weight interpretation mode */
	EInterpretWeightMode InterpretWeightMode = EInterpretWeightMode::RadiusInterpWithFill;

	/** If true, weights are inverted */
	bool bInvertWeights = false;

	/** If true, barycentric coordinates output array will be populated */
	bool bComputeBarycentrics = false;

	//
	// TODO: when MaxSamples is set, it would be useful to be able to use Weight to modulate
	// positional distribution instead of radius, (or both!)
	//

public:
	//
	// Outputs
	//

	/** Result of last computation */
	FGeometryResult Result;

	/** Oriented Sample Points on the mesh surface. Z axis of frame points along mesh normal, X and Y are arbitrary */
	TArray<FFrame3d> Samples;

	/** Radius of each Sample Point, length is the same as Samples array */
	TArray<double> Radii;

	/** Triangle that contains each Sample Point, length is the same as Samples array */
	TArray<int32> TriangleIDs;

	/** Barycentric Coordinates of each Sample Point in it's respective Triangle. Only computed if bComputeBarycentrics = true */
	TArray<FVector3d> BarycentricCoords;

public:

	/**
	 * Compute an approximate Poisson sampling of the mesh, either uniform or non-uniform depending on the settings above.
	 * The sampling will attempt to fully cover the mesh unless .MaxSamples is provided, in which case exactly
	 * that many samples will be generated and they will be randomly distributed (so not at all Poisson...!)
	 * 
	 * By default the sampling will be uniform - all points will be spaced at least .SampleRadius*2, ie no "collisions"
	 * between their bounding spheres.
	 * 
	 * If .MaxSampleRadius is larger than .SampleRadius, the sampling will be non-uniform, ie samples will be emitted
	 * with radii within this range. By default a random uniform distribution of radii will be attempted, .SizeDistribution 
	 * and related parameters can be used to make this distribution non-uniform. 
	 * The spacing between points will always be greater than the sum of their two sample radii, so again no collisions. 
	 * However the "density" of the sampling will vary depending on how well the algorithm can find gaps to fill.
	 * 
	 * If .bUseVertexWeights is defined and valid VertexWeights are provided, they will be used to modulate the sampling radii.
	 * The .InterpretWeightMode setting controls how the weights are used to influence the sample radii.
	 * 
	 * The strategy used is to compute a much higher density sampling than needed (based on .SubSampleDensity),
	 * then iteratively select from that point set and decimate it within the radius of selected samples. The implementation
	 * also introduces various biases to increase performance. Generally increasing SubSampleDensity will result in 
	 * more tightly-packed results, but at increasingly expensive computation time.
	 */
	GEOMETRYCORE_API void ComputePoissonSampling(const FDynamicMesh3& Mesh, FProgressCancel* Progress = nullptr);


	// ability to incrementally increase existing sample sizes to more tightly pack them?
	// ability to iteratively redistribute samples to improve uniformity (ie potential field / mass-spring type method, point smoothing, ?)
};






} // end namespace UE::Geometry
} // end namespace UE

