// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshSamplingFunctions.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Sampling/MeshSurfacePointSampling.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshSamplingFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshSamplingFunctions"


UDynamicMesh* UGeometryScriptLibrary_MeshSamplingFunctions::ComputePointSampling(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshPointSamplingOptions Options,
	TArray<FTransform>& Samples,
	FGeometryScriptIndexList& TriangleIDs,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputePointSampling", "ComputePointSampling: TargetMesh is Null"));
		return TargetMesh;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(GS_ComputePointSampling);

	FMeshSurfacePointSampling Sampler;
	Sampler.SampleRadius = Options.SamplingRadius;
	Sampler.MaxSamples = Options.MaxNumSamples;
	Sampler.SubSampleDensity = Options.SubSampleDensity;
	Sampler.RandomSeed = Options.RandomSeed;

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		Sampler.ComputePoissonSampling(ReadMesh);
	});

	TriangleIDs.Reset(EGeometryScriptIndexType::Triangle);
	TArray<int>& IndexList = *TriangleIDs.List;
	int32 NumSamples = Sampler.Samples.Num();
	for ( int32 k = 0; k < NumSamples; ++k )
	{
		const FFrame3d& Frame = Sampler.Samples[k];
		Samples.Add( Frame.ToFTransform() );

		IndexList.Add( Sampler.TriangleIDs[k] );
	}

	return TargetMesh;
}


UDynamicMesh* UGeometryScriptLibrary_MeshSamplingFunctions::ComputeNonUniformPointSampling(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshPointSamplingOptions Options,
	FGeometryScriptNonUniformPointSamplingOptions NonUniformOptions,
	TArray<FTransform>& Samples,
	TArray<double>& SampleRadii,
	FGeometryScriptIndexList& TriangleIDs,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeNonUniformPointSampling", "ComputeNonUniformPointSampling: TargetMesh is Null"));
		return TargetMesh;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(GS_ComputeNonUniformPointSampling);

	FMeshSurfacePointSampling Sampler;
	Sampler.SampleRadius = Options.SamplingRadius;
	Sampler.MaxSamples = Options.MaxNumSamples;
	Sampler.SubSampleDensity = Options.SubSampleDensity;
	Sampler.RandomSeed = Options.RandomSeed;
	if (NonUniformOptions.MaxSamplingRadius > Options.SamplingRadius)
	{
		Sampler.MaxSampleRadius = NonUniformOptions.MaxSamplingRadius;
		Sampler.SizeDistribution = (FMeshSurfacePointSampling::ESizeDistribution)(int)(NonUniformOptions.SizeDistribution);
		Sampler.SizeDistributionPower = FMath::Clamp(NonUniformOptions.SizeDistributionPower, 1.0, 10.0);
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		Sampler.ComputePoissonSampling(ReadMesh);
	});

	TriangleIDs.Reset(EGeometryScriptIndexType::Triangle);
	TArray<int>& IndexList = *TriangleIDs.List;
	int32 NumSamples = Sampler.Samples.Num();
	for ( int32 k = 0; k < NumSamples; ++k )
	{
		Samples.Add( Sampler.Samples[k].ToFTransform() );
		SampleRadii.Add( Sampler.Radii[k]);

		IndexList.Add( Sampler.TriangleIDs[k] );
	}

	return TargetMesh;
}





UDynamicMesh* UGeometryScriptLibrary_MeshSamplingFunctions::ComputeVertexWeightedPointSampling(
	UDynamicMesh* TargetMesh,
	FGeometryScriptMeshPointSamplingOptions Options,
	FGeometryScriptNonUniformPointSamplingOptions NonUniformOptions,
	FGeometryScriptScalarList VertexWeights,
	TArray<FTransform>& Samples,
	TArray<double>& SampleRadii,
	FGeometryScriptIndexList& TriangleIDs,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ComputeVertexWeightedPointSampling_InvalidInput", "ComputeVertexWeightedPointSampling: TargetMesh is Null"));
		return TargetMesh;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(GS_ComputeVertexWeightedPointSampling);

	FMeshSurfacePointSampling Sampler;
	Sampler.SampleRadius = Options.SamplingRadius;
	Sampler.MaxSamples = Options.MaxNumSamples;
	Sampler.SubSampleDensity = Options.SubSampleDensity;
	Sampler.RandomSeed = Options.RandomSeed;
	bool bIsNonUniform = false;
	if (NonUniformOptions.MaxSamplingRadius > Options.SamplingRadius)
	{
		bIsNonUniform = true;
		Sampler.MaxSampleRadius = NonUniformOptions.MaxSamplingRadius;
		Sampler.SizeDistribution = (FMeshSurfacePointSampling::ESizeDistribution)(int)(NonUniformOptions.SizeDistribution);
		Sampler.SizeDistributionPower = FMath::Clamp(NonUniformOptions.SizeDistributionPower, 1.0, 10.0);
	}

	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		if ( bIsNonUniform && VertexWeights.List.IsValid() && VertexWeights.List->Num() == ReadMesh.MaxVertexID() )
		{
			Sampler.VertexWeights = *VertexWeights.List.Get();
			Sampler.bUseVertexWeights = true;
			Sampler.InterpretWeightMode = (FMeshSurfacePointSampling::EInterpretWeightMode)(int)(NonUniformOptions.WeightMode);
			Sampler.bInvertWeights = NonUniformOptions.bInvertWeights;
		}
		Sampler.ComputePoissonSampling(ReadMesh);
	});

	TriangleIDs.Reset(EGeometryScriptIndexType::Triangle);
	TArray<int>& IndexList = *TriangleIDs.List;
	int32 NumSamples = Sampler.Samples.Num();
	for ( int32 k = 0; k < NumSamples; ++k )
	{
		Samples.Add( Sampler.Samples[k].ToFTransform() );
		SampleRadii.Add( Sampler.Radii[k]);

		IndexList.Add( Sampler.TriangleIDs[k] );
	}

	return TargetMesh;
}



#undef LOCTEXT_NAMESPACE