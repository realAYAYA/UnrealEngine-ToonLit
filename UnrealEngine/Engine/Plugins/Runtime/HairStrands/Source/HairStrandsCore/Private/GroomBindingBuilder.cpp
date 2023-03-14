// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomBindingBuilder.h"
#include "GeometryCache.h"
#include "GeometryCacheMeshData.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "HairStrandsMeshProjection.h"
#include "Async/ParallelFor.h"
#include "GlobalShader.h"
#include "Misc/ScopedSlowTask.h"

#if WITH_EDITORONLY_DATA

///////////////////////////////////////////////////////////////////////////////////////////////////
// Eigen for large matrix inversion
// Just to be sure, also added this in Eigen.Build.cs
#ifndef EIGEN_MPL2_ONLY
#define EIGEN_MPL2_ONLY
#endif

#if defined(_MSC_VER) && USING_CODE_ANALYSIS
#pragma warning(push)
#pragma warning(disable:6294) // Ill-defined for-loop:  initial condition does not satisfy test.  Loop body not executed.
#endif
PRAGMA_DEFAULT_VISIBILITY_START
THIRD_PARTY_INCLUDES_START
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseLU>
#include <Eigen/SVD>
THIRD_PARTY_INCLUDES_END
PRAGMA_DEFAULT_VISIBILITY_END
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
#pragma warning(pop)
#endif

#endif

// Run the binding asset building in parallel (faster)
#define BINDING_PARALLEL_BUILDING 1

///////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_LOG_CATEGORY_STATIC(LogGroomBindingBuilder, Log, All);

#define LOCTEXT_NAMESPACE "GroomBindingBuilder"

static int32 GHairStrandsBindingBuilderWarningEnable = 1;
static FAutoConsoleVariableRef CVarHairStrandsBindingBuilderWarningEnable(TEXT("r.HairStrands.Log.BindingBuilderWarning"), GHairStrandsBindingBuilderWarningEnable, TEXT("Enable/disable warning during groom binding builder"));

///////////////////////////////////////////////////////////////////////////////////////////////////

FString FGroomBindingBuilder::GetVersion()
{
	// Important to update the version when groom building changes
	return TEXT("2gc");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Common utils functions
// These utils function are a copy of function in HairStrandsMeshProjectionCommon.ush
uint32 FHairStrandsRootUtils::EncodeTriangleIndex(uint32 TriangleIndex, uint32 SectionIndex)
{
	return ((SectionIndex & 0xFF) << 24) | (TriangleIndex & 0xFFFFFF);
}

// This function is a copy of DecodeTriangleIndex in HairStrandsMeshProjectionCommon.ush
void FHairStrandsRootUtils::DecodeTriangleIndex(uint32 Encoded, uint32& OutTriangleIndex, uint32& OutSectionIndex)
{
	OutSectionIndex = (Encoded >> 24) & 0xFF;
	OutTriangleIndex = Encoded & 0xFFFFFF;
}

uint32 FHairStrandsRootUtils::EncodeBarycentrics(const FVector2f& B)
{
	return uint32(FFloat16(B.X).Encoded) | (uint32(FFloat16(B.Y).Encoded)<<16);
}

FVector2f FHairStrandsRootUtils::DecodeBarycentrics(uint32 B)
{
	FFloat16 BX;
	BX.Encoded = (B & 0xFFFF);

	FFloat16 BY;
	BY.Encoded = (B >> 16) & 0xFFFF;

	return FVector2f(BX, BY);
}

uint32 FHairStrandsRootUtils::PackUVs(const FVector2f& UV)
{
	return (FFloat16(UV.X).Encoded & 0xFFFF) | ((FFloat16(UV.Y).Encoded & 0xFFFF) << 16);
}

float FHairStrandsRootUtils::PackUVsToFloat(const FVector2f& UV)
{
	uint32 Encoded = PackUVs(UV);
	return *((float*)(&Encoded));
}

//////////////////////////////////////////////////////////////////////////
// Intermediate data struct

/** Binding data */
struct FHairRootGroupData
{
	FHairStrandsRootData			SimRootData;
	FHairStrandsRootData			RenRootData;
	TArray<FHairStrandsRootData>	CardsRootData;
};

namespace
{
//////////////////////////////////////////////////////////////////////////
// Interfaces to query mesh data from different sources
//////////////////////////////////////////////////////////////////////////

// Interface to gather info about a mesh section
class IMeshSectionData
{
public:
	virtual uint32 GetNumVertices() const = 0;
	virtual uint32 GetNumTriangles() const = 0;
	virtual uint32 GetBaseIndex() const = 0;
	virtual uint32 GetBaseVertexIndex() const = 0;
	virtual ~IMeshSectionData() {}
};

// Interface to query mesh data per LOD
class IMeshLODData
{
public:
	virtual const FVector3f* GetVerticesBuffer() const = 0;
	virtual uint32 GetNumVertices() const = 0;
	virtual const TArray<uint32>& GetIndexBuffer() const = 0;
	virtual const int32 GetNumSections() const = 0;
	virtual const IMeshSectionData& GetSection(uint32 SectionIndex) const = 0;
	virtual const FVector3f& GetVertexPosition(uint32 VertexIndex) const = 0;
	virtual FVector2D GetVertexUV(uint32 VertexIndex, uint32 ChannelIndex) const = 0;
	virtual void GetSectionFromVertexIndex(uint32 InVertIndex, int32& OutSectionIndex) const = 0;
	virtual ~IMeshLODData() {}
};

// Interface to wrap the mesh source and query its LOD data
class IMeshData
{
public:
	virtual bool IsValid() const = 0;
	virtual uint32 GetNumLODs() const = 0;
	virtual const IMeshLODData& GetMeshLODData(uint32 LODIndex) const = 0;
	virtual ~IMeshData() {}
};

//////////////////////////////////////////////////////////////////////////
// Implementation for SkeletalMesh as a mesh source

class FSkeletalMeshSection : public IMeshSectionData
{
public:
	FSkeletalMeshSection(FSkelMeshRenderSection& InSection)
		: Section(InSection)
	{
	}

	virtual uint32 GetNumVertices() const override
	{
		return Section.NumVertices;
	}

	virtual uint32 GetNumTriangles() const override
	{
		return Section.NumTriangles;
	}

	virtual uint32 GetBaseIndex() const override
	{
		return Section.BaseIndex;
	}

	virtual uint32 GetBaseVertexIndex() const override
	{
		return Section.BaseVertexIndex;
	}

private:
	FSkelMeshRenderSection& Section;
};

class FSkeletalMeshLODData : public IMeshLODData
{
public:
	FSkeletalMeshLODData(FSkeletalMeshLODRenderData& InMeshLODData) 
		: MeshLODData(InMeshLODData)
	{
		IndexBuffer.SetNum(MeshLODData.MultiSizeIndexContainer.GetIndexBuffer()->Num());
		MeshLODData.MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);

		for (FSkelMeshRenderSection& MeshSection : MeshLODData.RenderSections)
		{
			Sections.Add(FSkeletalMeshSection(MeshSection));
		}
	}

	virtual const FVector3f* GetVerticesBuffer() const override
	{
		return static_cast<FVector3f*>(MeshLODData.StaticVertexBuffers.PositionVertexBuffer.GetVertexData());
	}

	virtual uint32 GetNumVertices() const override
	{
		return MeshLODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
	}

	virtual const TArray<uint32>& GetIndexBuffer() const override
	{
		return IndexBuffer;
	}

	virtual const int32 GetNumSections() const override
	{
		return Sections.Num();
	}

	virtual const IMeshSectionData& GetSection(uint32 SectionIndex) const override
	{
		return Sections[SectionIndex];
	}

	virtual const FVector3f& GetVertexPosition(uint32 VertexIndex) const override
	{
		return MeshLODData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex);
	}

	virtual FVector2D GetVertexUV(uint32 VertexIndex, uint32 ChannelIndex) const override
	{
		return FVector2D(MeshLODData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertexIndex, ChannelIndex));
	}

	virtual void GetSectionFromVertexIndex(uint32 InVertIndex, int32& OutSectionIndex) const override
	{
		int32 OutVertIndex = 0;
		return MeshLODData.GetSectionFromVertexIndex(InVertIndex, OutSectionIndex, OutVertIndex);
	}

private:
	FSkeletalMeshLODRenderData& MeshLODData;
	TArray<uint32> IndexBuffer;
	TArray<FSkeletalMeshSection> Sections;
};

class FSkeletalMeshData : public IMeshData
{
public:
	FSkeletalMeshData(USkeletalMesh* InSkeletalMesh) 
		: SkeletalMesh(InSkeletalMesh)
	{
		if (SkeletalMesh)
		{
			MeshData = SkeletalMesh->GetResourceForRendering();
			if (MeshData)
			{
				for (FSkeletalMeshLODRenderData& MeshLODData : MeshData->LODRenderData)
				{
					MeshesLODData.Add(FSkeletalMeshLODData(MeshLODData));
				}
			}
			else
			{
				UE_LOG(LogHairStrands, Warning, TEXT("Could not retrieve mesh data for SkeletalMesh %s."), *SkeletalMesh->GetName());
			}
		}
	}

	virtual bool IsValid() const override
	{
		return SkeletalMesh != nullptr && MeshData != nullptr && MeshesLODData.Num() > 0;
	}

	virtual uint32 GetNumLODs() const override
	{
		return SkeletalMesh->GetLODNum();
	}

	virtual const IMeshLODData& GetMeshLODData(uint32 LODIndex) const override
	{
		return MeshesLODData[LODIndex];
	}

private:
	USkeletalMesh* SkeletalMesh;
	FSkeletalMeshRenderData* MeshData;
	TArray<FSkeletalMeshLODData> MeshesLODData;
};

//////////////////////////////////////////////////////////////////////////
// Implementation for GeometryCache as a mesh source

class FGeometryCacheSection : public IMeshSectionData
{
public:
	FGeometryCacheSection(FGeometryCacheMeshBatchInfo& InSection, uint32 InNumVertices, uint32 InBaseVertexIdex)
		: Section(InSection)
		, NumVertices(InNumVertices)
		, BaseVertexIndex(InBaseVertexIdex)
	{
	}

	virtual uint32 GetNumVertices() const override
	{
		return NumVertices;
	}

	virtual uint32 GetNumTriangles() const override
	{
		return Section.NumTriangles;
	}

	virtual uint32 GetBaseIndex() const override
	{
		return Section.StartIndex;
	}

	virtual uint32 GetBaseVertexIndex() const override
	{
		return BaseVertexIndex;
	}

private:
	FGeometryCacheMeshBatchInfo& Section;
	uint32 NumVertices;
	uint32 BaseVertexIndex;
};

// GeometryCache have only one LOD so FGeometryCacheData provides both mesh source and mesh LOD data
class FGeometryCacheData : public IMeshData, public IMeshLODData
{
public:
	FGeometryCacheData(UGeometryCache* InGeometryCache) 
		: GeometryCache(InGeometryCache)
	{
		if (GeometryCache)
		{
			TArray<FGeometryCacheMeshData> MeshesData;
			GeometryCache->GetMeshDataAtTime(0.0f, MeshesData);
			if (MeshesData.Num() > 1)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("Cannot use non-flattened GeometryCache %s as input."), *GeometryCache->GetName());
			}
			else if (MeshesData.Num() == 0)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("Could not read mesh data from the GeometryCache %s."), *GeometryCache->GetName());
			}
			else
			{
				if (MeshesData[0].Positions.Num() > 0)
				{
					MeshData = MoveTemp(MeshesData[0]);
					for (FGeometryCacheMeshBatchInfo& BatchInfo : MeshData.BatchesInfo)
					{
						FRange SectionRange;
						for (uint32 VertexIndex = BatchInfo.StartIndex; VertexIndex < BatchInfo.StartIndex + BatchInfo.NumTriangles * 3; ++VertexIndex)
						{
							SectionRange.Add(MeshData.Indices[VertexIndex]);
						}
						SectionRanges.Add(SectionRange);

						Sections.Add(FGeometryCacheSection(BatchInfo, SectionRange.Num(), SectionRange.Min));
					}
				}
				else
				{
					UE_LOG(LogHairStrands, Warning, TEXT("GeometryCache %s has no valid mesh data."), *GeometryCache->GetName());
				}
			}
		}
	}

	virtual bool IsValid() const override
	{
		return GeometryCache != nullptr && Sections.Num() > 0;
	}

	virtual uint32 GetNumLODs() const override
	{
		return 1;
	}

	virtual const IMeshLODData& GetMeshLODData(uint32 LODIndex) const override
	{
		return *this;
	}

	virtual const FVector3f* GetVerticesBuffer() const override
	{
		return MeshData.Positions.GetData();
	}

	virtual uint32 GetNumVertices() const override
	{
		return MeshData.Positions.Num();
	}

	virtual const TArray<uint32>& GetIndexBuffer() const override
	{
		return MeshData.Indices;
	}

	virtual const int32 GetNumSections() const override
	{
		return Sections.Num();
	}

	virtual const IMeshSectionData& GetSection(uint32 SectionIndex) const override
	{
		return Sections[SectionIndex];
	}

	virtual const FVector3f& GetVertexPosition(uint32 VertexIndex) const override
	{
		return MeshData.Positions[VertexIndex];
	}

	virtual FVector2D GetVertexUV(uint32 VertexIndex, uint32 ChannelIndex) const override
	{
		return FVector2D(MeshData.TextureCoordinates[VertexIndex]);
	}

	virtual void GetSectionFromVertexIndex(uint32 InVertIndex, int32& OutSectionIndex) const override
	{
		for (int32 SectionIndex = 0; SectionIndex < SectionRanges.Num(); ++SectionIndex)
		{
			const FRange& SectionRange = SectionRanges[SectionIndex];
			if (InVertIndex >= SectionRange.Min && InVertIndex <= SectionRange.Max)
			{
				OutSectionIndex = SectionIndex;
				return;
			}
		}
	}

private:
	UGeometryCache* GeometryCache;
	FGeometryCacheMeshData MeshData;
	TArray<FGeometryCacheSection> Sections;

	struct FRange
	{
		uint32 Min = -1;
		uint32 Max = 0;
		void Add(uint32 Value)
		{
			Min = FMath::Min(Min, Value);
			Max = FMath::Max(Max, Value);
		}
		uint32 Num() { return Max - Min + 1; }
	};

	TArray<FRange> SectionRanges;
};

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// RBF weighting

namespace GroomBinding_RBFWeighting
{
	int32 FPointsSampler::StartingPoint(const TArray<bool>& ValidPoints, int32& NumPoints) const
	{
		int32 StartIndex = -1;
		NumPoints = 0;
		for (int32 i = 0; i < ValidPoints.Num(); ++i)
		{
			if (ValidPoints[i])
			{
				++NumPoints;
				if (StartIndex == -1)
				{
					StartIndex = i;
				}
			}
		}
		return StartIndex;
	}

	void FPointsSampler::BuildPositions(const FVector3f* PointPositions)
	{
		SamplePositions.SetNum(SampleIndices.Num());
		for (int32 i = 0; i < SampleIndices.Num(); ++i)
		{
			SamplePositions[i] = PointPositions[SampleIndices[i]];
		}
	}

	void FPointsSampler::FurthestPoint(const int32 NumPoints, const FVector3f* PointPositions, const uint32 SampleIndex, TArray<bool>& ValidPoints, TArray<float>& PointsDistance)
	{
		float FurthestDistance = 0.0;
		uint32 PointIndex = 0;
		for (int32 j = 0; j < NumPoints; ++j)
		{
			if (ValidPoints[j])
			{
				PointsDistance[j] = FMath::Min((PointPositions[SampleIndices[SampleIndex - 1]] - PointPositions[j]).Size(), PointsDistance[j]);
				if (PointsDistance[j] >= FurthestDistance)
				{
					PointIndex = j;
					FurthestDistance = PointsDistance[j];
				}
			}
		}
		ValidPoints[PointIndex] = false;
		SampleIndices[SampleIndex] = PointIndex;
	}

	FPointsSampler::FPointsSampler(TArray<bool>& ValidPoints, const FVector3f* PointPositions, const int32 NumSamples)
	{
		int32 NumPoints = 0;
		int32 StartIndex = StartingPoint(ValidPoints, NumPoints);

		const int32 SamplesCount = FMath::Min(NumPoints, NumSamples);
		if (SamplesCount != 0)
		{
			SampleIndices.SetNum(SamplesCount);
			SampleIndices[0] = StartIndex;
			ValidPoints[StartIndex] = false;

			TArray<float> PointsDistance;
			PointsDistance.Init(MAX_FLT, ValidPoints.Num());

			for (int32 i = 1; i < SamplesCount; ++i)
			{
				FurthestPoint(ValidPoints.Num(), PointPositions, i, ValidPoints, PointsDistance);
			}
			BuildPositions(PointPositions);
		}
	}
}
#if WITH_EDITORONLY_DATA
namespace GroomBinding_RBFWeighting
{
	struct FWeightsBuilder
	{
		FWeightsBuilder(const uint32 NumRows, const uint32 NumColumns,
			const FVector3f* SourcePositions, const FVector3f* TargetPositions);

		using EigenMatrix = Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;

		/** Compute the weights by inverting the matrix*/
		void ComputeWeights(const uint32 NumRows, const uint32 NumColumns);

		/** Entries in the dense structure */
		TArray<float> MatrixEntries;

		/** Entries of the matrix inverse */
		TArray<float> InverseEntries;
	};

	FWeightsBuilder::FWeightsBuilder(const uint32 NumRows, const uint32 NumColumns,
		const FVector3f* SourcePositions, const FVector3f* TargetPositions)
	{
		const uint32 PolyRows = NumRows + 4;
		const uint32 PolyColumns = NumColumns + 4;

		MatrixEntries.Init(0.0, PolyRows * PolyColumns);
		InverseEntries.Init(0.0, PolyRows * PolyColumns);
		TArray<float>& LocalEntries = MatrixEntries;
		ParallelFor(NumRows,
			[
				NumRows,
				NumColumns,
				PolyRows,
				PolyColumns,
				SourcePositions,
				TargetPositions,
				&LocalEntries
			] (uint32 RowIndex)
			{
				int32 EntryIndex = RowIndex * PolyColumns;
				for (uint32 j = 0; j < NumColumns; ++j)
				{
					const float FunctionScale = (SourcePositions[RowIndex] - TargetPositions[j]).Size();
					LocalEntries[EntryIndex++] = FMath::Sqrt(FunctionScale * FunctionScale + 1.0);
				}
				LocalEntries[EntryIndex++] = 1.0;
				LocalEntries[EntryIndex++] = SourcePositions[RowIndex].X;
				LocalEntries[EntryIndex++] = SourcePositions[RowIndex].Y;
				LocalEntries[EntryIndex++] = SourcePositions[RowIndex].Z;

				EntryIndex = NumRows * PolyColumns + RowIndex;
				LocalEntries[EntryIndex] = 1.0;

				EntryIndex += PolyColumns;
				LocalEntries[EntryIndex] = SourcePositions[RowIndex].X;

				EntryIndex += PolyColumns;
				LocalEntries[EntryIndex] = SourcePositions[RowIndex].Y;

				EntryIndex += PolyColumns;
				LocalEntries[EntryIndex] = SourcePositions[RowIndex].Z;
			});
		const float REGUL_VALUE = 1e-4;
		int32 EntryIndex = NumRows * PolyColumns + NumColumns;
		LocalEntries[EntryIndex] = REGUL_VALUE;

		EntryIndex += PolyColumns + 1;
		LocalEntries[EntryIndex] = REGUL_VALUE;

		EntryIndex += PolyColumns + 1;
		LocalEntries[EntryIndex] = REGUL_VALUE;

		EntryIndex += PolyColumns + 1;
		LocalEntries[EntryIndex] = REGUL_VALUE;

		ComputeWeights(PolyRows, PolyColumns);
	}

	void FWeightsBuilder::ComputeWeights(const uint32 NumRows, const uint32 NumColumns)
	{
		EigenMatrix WeightsMatrix(MatrixEntries.GetData(), NumRows, NumColumns);
		EigenMatrix WeightsInverse(InverseEntries.GetData(), NumColumns, NumRows);

		auto MatrixSvd = WeightsMatrix.jacobiSvd(Eigen::ComputeThinU | Eigen::ComputeThinV);
		const Eigen::VectorXf& SingularValues = MatrixSvd.singularValues();
		if (SingularValues.size() > 0)
		{
			const float Tolerance = FLT_EPSILON * SingularValues.array().abs()(0);
			WeightsInverse = MatrixSvd.matrixV() * (SingularValues.array().abs() > Tolerance).select(
				SingularValues.array().inverse(), 0).matrix().asDiagonal() * MatrixSvd.matrixU().adjoint();
		}
	}

	void UpdateInterpolationWeights(const FWeightsBuilder& InterpolationWeights, const FPointsSampler& PointsSampler, const uint32 LODIndex, FHairStrandsRootData& RootDatas)
	{
		FHairStrandsRootData::FMeshProjectionLOD& CPULOD = RootDatas.MeshProjectionLODs[LODIndex];
		CPULOD.MeshSampleIndicesBuffer.SetNum(PointsSampler.SampleIndices.Num());
		CPULOD.MeshInterpolationWeightsBuffer.SetNum(InterpolationWeights.InverseEntries.Num());
		CPULOD.RestSamplePositionsBuffer.SetNum(PointsSampler.SampleIndices.Num());

		CPULOD.SampleCount = PointsSampler.SampleIndices.Num();
		CPULOD.MeshSampleIndicesBuffer = PointsSampler.SampleIndices;
		CPULOD.MeshInterpolationWeightsBuffer = InterpolationWeights.InverseEntries;
		for (int32 i = 0; i < PointsSampler.SamplePositions.Num(); ++i)
		{
			CPULOD.RestSamplePositionsBuffer[i] = FVector4f(PointsSampler.SamplePositions[i], 1.0f);
		}
	}

	void FillLocalValidPoints(const IMeshLODData& MeshLODData, const int32 TargetSection,
		const FHairStrandsRootData::FMeshProjectionLOD& ProjectionLOD, TArray<bool>& ValidPoints)
	{
		const TArray<uint32>& TriangleIndices = MeshLODData.GetIndexBuffer();

		ValidPoints.Init(false, MeshLODData.GetNumVertices());

		const bool ValidSection = (TargetSection >= 0 && TargetSection < MeshLODData.GetNumSections());

		for (uint32 EncodedTriangleId : ProjectionLOD.UniqueTriangleIndexBuffer)
		{
			uint32 SectionIndex = 0;
			uint32 TriangleIndex = 0;
			FHairStrandsRootUtils::DecodeTriangleIndex(EncodedTriangleId, TriangleIndex, SectionIndex);
			if (!ValidSection || (ValidSection && (SectionIndex == TargetSection)))
			{
				const IMeshSectionData& Section = MeshLODData.GetSection(SectionIndex);
				for (uint32 VertexIt = 0; VertexIt < 3; ++VertexIt)
				{
					const uint32 VertexIndex = TriangleIndices[Section.GetBaseIndex() + 3 * TriangleIndex + VertexIt];
					ValidPoints[VertexIndex] = (VertexIndex >= Section.GetBaseVertexIndex()) && (VertexIndex < Section.GetBaseVertexIndex() + Section.GetNumVertices());
				}
			}
		}
	}

	void FillGlobalValidPoints(const IMeshLODData& MeshLODData, const int32 TargetSection, TArray<bool>& ValidPoints)
	{
		const TArray<uint32>& TriangleIndices = MeshLODData.GetIndexBuffer(); 
		if (TargetSection >= 0 && TargetSection < MeshLODData.GetNumSections())
		{
			ValidPoints.Init(false, MeshLODData.GetNumVertices());

			const IMeshSectionData& Section = MeshLODData.GetSection(TargetSection);
			for (uint32 TriangleIt = 0; TriangleIt < Section.GetNumTriangles(); ++TriangleIt)
			{
				for (uint32 VertexIt = 0; VertexIt < 3; ++VertexIt)
				{
					const uint32 VertexIndex = TriangleIndices[Section.GetBaseIndex() + 3 * TriangleIt + VertexIt];
					ValidPoints[VertexIndex] = true;
				}
			}
		}
		else
		{
			ValidPoints.Init(true, MeshLODData.GetNumVertices());
		}
	}

	void ComputeInterpolationWeights(
		TArray<FHairRootGroupData>& Out, 
		const uint32 NumInterpolationPoints, 
		const int32 MatchingSection, 
		const IMeshData* MeshData, 
		const TArray<TArray<FVector3f>>& TransferedPositions)
	{
		const uint32 GroupCount  = Out.Num();
		const uint32 MeshLODCount= MeshData->GetNumLODs();
		const uint32 MaxSamples  = NumInterpolationPoints;

		for (uint32 LODIndex = 0; LODIndex < MeshLODCount; ++LODIndex)
		{
			const IMeshLODData& MeshLODData = MeshData->GetMeshLODData(LODIndex);

			int32 TargetSection = -1;
			bool GlobalSamples = false;
			const FVector3f* PositionsPointer = nullptr;
			if (TransferedPositions.Num() == MeshLODCount)
			{
				PositionsPointer = TransferedPositions[LODIndex].GetData();
				GlobalSamples = true;
				TargetSection = MatchingSection;
			}
			else
			{
				PositionsPointer = MeshLODData.GetVerticesBuffer();
			}

			if (!GlobalSamples)
			{
				TArray<bool> ValidPoints;
				for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
				{
					FillLocalValidPoints(MeshLODData, TargetSection, Out[GroupIt].SimRootData.MeshProjectionLODs[LODIndex], ValidPoints);

					FPointsSampler PointsSampler(ValidPoints, PositionsPointer, MaxSamples);
					const uint32 SampleCount = PointsSampler.SamplePositions.Num();

					FWeightsBuilder InterpolationWeights(SampleCount, SampleCount,
						PointsSampler.SamplePositions.GetData(), PointsSampler.SamplePositions.GetData());

					UpdateInterpolationWeights(InterpolationWeights, PointsSampler, LODIndex, Out[GroupIt].SimRootData);
					UpdateInterpolationWeights(InterpolationWeights, PointsSampler, LODIndex, Out[GroupIt].RenRootData);
				}
			}
			else
			{
				TArray<bool> ValidPoints;

				FillGlobalValidPoints(MeshLODData, TargetSection, ValidPoints);

				FPointsSampler PointsSampler(ValidPoints, PositionsPointer, MaxSamples);
				const uint32 SampleCount = PointsSampler.SamplePositions.Num();

				FWeightsBuilder InterpolationWeights(SampleCount, SampleCount,
					PointsSampler.SamplePositions.GetData(), PointsSampler.SamplePositions.GetData());

				for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
				{
					UpdateInterpolationWeights(InterpolationWeights, PointsSampler, LODIndex, Out[GroupIt].SimRootData);
					UpdateInterpolationWeights(InterpolationWeights, PointsSampler, LODIndex, Out[GroupIt].RenRootData);
				}
			}
		}
	}
}// namespace GroomBinding_RBFWeighting

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// Root projection

namespace GroomBinding_RootProjection
{
	struct FTriangleGrid
	{
		struct FTriangle
		{
			uint32  TriangleIndex;
			uint32  SectionIndex;
			uint32  SectionBaseIndex;

			uint32  I0;
			uint32  I1;
			uint32  I2;

			FVector P0;
			FVector P1;
			FVector P2;

			FVector2D UV0;
			FVector2D UV1;
			FVector2D UV2;
		};

		struct FCell
		{
			TArray<FTriangle> Triangles;
		};
		typedef TArray<const FCell*> FCells;

		FTriangleGrid(const FVector& InMinBound, const FVector& InMaxBound, float InVoxelWorldSize)
		{
			MinBound = InMinBound;
			MaxBound = InMaxBound;

			// Compute the voxel volume resolution, and snap the max bound to the voxel grid
			GridResolution = FIntVector::ZeroValue;
			FVector VoxelResolutionF = (MaxBound - MinBound) / InVoxelWorldSize;
			GridResolution = FIntVector(FMath::CeilToInt(VoxelResolutionF.X), FMath::CeilToInt(VoxelResolutionF.Y), FMath::CeilToInt(VoxelResolutionF.Z));
			MaxBound = MinBound + FVector(GridResolution) * InVoxelWorldSize;

			Cells.SetNum(GridResolution.X * GridResolution.Y * GridResolution.Z);
		}

		FORCEINLINE bool IsValid(const FIntVector& P) const
		{
			return
				0 <= P.X && P.X < GridResolution.X &&
				0 <= P.Y && P.Y < GridResolution.Y &&
				0 <= P.Z && P.Z < GridResolution.Z;
		}

		FORCEINLINE bool IsOutside(const FVector& MinP, const FVector& MaxP) const
		{
			return
				(MaxP.X <= MinBound.X || MaxP.Y <= MinBound.Y || MaxP.Z <= MinBound.Z) ||
				(MinP.X >= MaxBound.X || MinP.Y >= MaxBound.Y || MinP.Z >= MaxBound.Z);
		}

		FORCEINLINE FIntVector ClampToVolume(const FIntVector& CellCoord, bool& bIsValid) const
		{
			bIsValid = IsValid(CellCoord);
			return FIntVector(
				FMath::Clamp(CellCoord.X, 0, GridResolution.X - 1),
				FMath::Clamp(CellCoord.Y, 0, GridResolution.Y - 1),
				FMath::Clamp(CellCoord.Z, 0, GridResolution.Z - 1));
		}

		FORCEINLINE FIntVector ToCellCoord(const FVector& P) const
		{
			bool bIsValid = false;
			const FVector F = ((P - MinBound) / (MaxBound - MinBound));
			const FIntVector CellCoord = FIntVector(FMath::FloorToInt(F.X * GridResolution.X), FMath::FloorToInt(F.Y * GridResolution.Y), FMath::FloorToInt(F.Z * GridResolution.Z));
			return ClampToVolume(CellCoord, bIsValid);
		}

		uint32 ToIndex(const FIntVector& CellCoord) const
		{
			uint32 CellIndex = CellCoord.X + CellCoord.Y * GridResolution.X + CellCoord.Z * GridResolution.X * GridResolution.Y;
			check(CellIndex < uint32(Cells.Num()));
			return CellIndex;
		}

		FCells ToCells(const FVector& P)
		{
			FCells Out;

			bool bIsValid = false;
			const FIntVector Coord = ToCellCoord(P);
			{
				const uint32 LinearIndex = ToIndex(Coord);
				if (Cells[LinearIndex].Triangles.Num() > 0)
				{
					Out.Add(&Cells[LinearIndex]);
					bIsValid = true;
				}
			}
			
			int32 Kernel = 1;
			do
			{
				for (int32 Z = -Kernel; Z <= Kernel; ++Z)
				for (int32 Y = -Kernel; Y <= Kernel; ++Y)
				for (int32 X = -Kernel; X <= Kernel; ++X)
				{
					// Do kernel box filtering layer, by layer
					if (FMath::Abs(X) != Kernel && FMath::Abs(Y) != Kernel && FMath::Abs(Z) != Kernel)
						continue;

					const FIntVector Offset(X, Y, Z);
					FIntVector C = Coord + Offset;
					C.X = FMath::Clamp(C.X, 0, GridResolution.X - 1);
					C.Y = FMath::Clamp(C.Y, 0, GridResolution.Y - 1);
					C.Z = FMath::Clamp(C.Z, 0, GridResolution.Z - 1);

					const uint32 LinearIndex = ToIndex(C);
					if (Cells[LinearIndex].Triangles.Num() > 0)
					{
						Out.Add(&Cells[LinearIndex]);
						bIsValid = true;
					}
				}
				++Kernel;

				// If no cells have been found in the entire grid, return
				if (Kernel >= FMath::Max3(GridResolution.X, GridResolution.Y, GridResolution.Z))
				{
					break;
				}
			} while (!bIsValid);

			return Out;
		}

		bool IsTriangleValid(const FTriangle& T) const
		{
			const FVector A = T.P0;
			const FVector B = T.P1;
			const FVector C = T.P2;

			const FVector AB = B - A;
			const FVector AC = C - A;
			const FVector BC = B - C;
			return FVector::DotProduct(AB, AB) > 0 && FVector::DotProduct(AC, AC) > 0 && FVector::DotProduct(BC, BC) > 0;
		}

		bool Insert(const FTriangle& T)
		{
			if (!IsTriangleValid(T))
			{
				return false;
			}

			FVector TriMinBound;
			TriMinBound.X = FMath::Min(T.P0.X, FMath::Min(T.P1.X, T.P2.X));
			TriMinBound.Y = FMath::Min(T.P0.Y, FMath::Min(T.P1.Y, T.P2.Y));
			TriMinBound.Z = FMath::Min(T.P0.Z, FMath::Min(T.P1.Z, T.P2.Z));

			FVector TriMaxBound;
			TriMaxBound.X = FMath::Max(T.P0.X, FMath::Max(T.P1.X, T.P2.X));
			TriMaxBound.Y = FMath::Max(T.P0.Y, FMath::Max(T.P1.Y, T.P2.Y));
			TriMaxBound.Z = FMath::Max(T.P0.Z, FMath::Max(T.P1.Z, T.P2.Z));

			if (IsOutside(TriMinBound, TriMaxBound))
			{
				return false;
			}

			const FIntVector MinCoord = ToCellCoord(TriMinBound);
			const FIntVector MaxCoord = ToCellCoord(TriMaxBound);

			// Insert triangle in all cell covered by the AABB of the triangle
			bool bInserted = false;
			for (int32 Z = MinCoord.Z; Z <= MaxCoord.Z; ++Z)
			{
				for (int32 Y = MinCoord.Y; Y <= MaxCoord.Y; ++Y)
				{
					for (int32 X = MinCoord.X; X <= MaxCoord.X; ++X)
					{
						const FIntVector CellIndex(X, Y, Z);
						if (IsValid(CellIndex))
						{
							const uint32 CellLinearIndex = ToIndex(CellIndex);
							Cells[CellLinearIndex].Triangles.Add(T);
							bInserted = true;
						}
					}
				}
			}
			return bInserted;
		}

		FVector MinBound;
		FVector MaxBound;
		FIntVector GridResolution;
		TArray<FCell> Cells;
	};

	// Closest point on A triangle from another point
	// Code from the book "Real-Time Collision Detection" by Christer Ericson
	struct FTrianglePoint
	{
		FVector P;
		FVector Barycentric;
	};

	static FTrianglePoint ComputeClosestPoint(const FTriangleGrid::FTriangle& Tri, const FVector& P)
	{
		const FVector A = Tri.P0;
		const FVector B = Tri.P1;
		const FVector C = Tri.P2;

		// Check if P is in vertex region outside A.
		FVector AB = B - A;
		FVector AC = C - A;
		FVector AP = P - A;
		float D1 = FVector::DotProduct(AB, AP);
		float D2 = FVector::DotProduct(AC, AP);
		if (D1 <= 0.f && D2 <= 0.f)
		{
			FTrianglePoint Out;
			Out.P = A;
			Out.Barycentric = FVector(1, 0, 0);
			return Out;
		}

		// Check if P is in vertex region outside B.
		FVector BP = P - B;
		float D3 = FVector::DotProduct(AB, BP);
		float D4 = FVector::DotProduct(AC, BP);
		if (D3 >= 0.f && D4 <= D3)
		{
			FTrianglePoint Out;
			Out.P = B;
			Out.Barycentric = FVector(0, 1, 0);
			return Out;
		}

		// Check if P is in edge region of AB, and if so, return the projection of P onto AB.
		float VC = D1 * D4 - D3 * D2;
		if (VC <= 0.f && D1 >= 0.f && D3 <= 0.f)
		{
			float V = D1 / (D1 - D3);

			FTrianglePoint Out;
			Out.P = A + V * AB;
			Out.Barycentric = FVector(1 - V, V, 0);
			return Out;
		}

		// Check if P is in vertex region outside C.
		FVector CP = P - C;
		float D5 = FVector::DotProduct(AB, CP);
		float D6 = FVector::DotProduct(AC, CP);
		if (D6 >= 0.f && D5 <= D6)
		{
			FTrianglePoint Out;
			Out.P = C;
			Out.Barycentric = FVector(0, 0, 1);
			return Out;
		}

		// Check if P is in edge region of AC, and if so, return the projection of P onto AC.
		float VB = D5 * D2 - D1 * D6;
		if (VB <= 0.f && D2 >= 0.f && D6 <= 0.f)
		{
			float W = D2 / (D2 - D6);
			FTrianglePoint Out;
			Out.P = A + W * AC;
			Out.Barycentric = FVector(1 - W, 0, W);
			return Out;
		}

		// Check if P is in edge region of BC, and if so, return the projection of P onto BC.
		float VA = D3 * D6 - D5 * D4;
		if (VA <= 0.f && D4 - D3 >= 0.f && D5 - D6 >= 0.f)
		{
			float W = (D4 - D3) / (D4 - D3 + D5 - D6);
			FTrianglePoint Out;
			Out.P = B + W * (C - B);
			Out.Barycentric = FVector(0, 1 - W, W);
			return Out;
		}

		// P must be inside the face region. Compute the closest point through its barycentric coordinates (u,V,W).
		float Denom = 1.f / (VA + VB + VC);
		float V = VB * Denom;
		float W = VC * Denom;

		FTrianglePoint Out;
		Out.P = A + AB * V + AC * W;
		Out.Barycentric = FVector(1 - V - W, V, W);
		return Out;
	}

	static bool Project(
		const FHairStrandsDatas& InStrandsData,
		const IMeshData* InMeshData,
		const TArray<TArray<FVector3f>>& InTransferredPositions,
		FHairStrandsRootData& OutRootData)
	{
		// 2. Project root for each mesh LOD
		const uint32 CurveCount = InStrandsData.GetNumCurves();
		const uint32 ChannelIndex = 0;
		const float VoxelWorldSize = 2; //cm
		const uint32 MeshLODCount = InMeshData->GetNumLODs();
		check(MeshLODCount == OutRootData.MeshProjectionLODs.Num());

		const bool bHasTransferredPosition = InTransferredPositions.Num() > 0;
		if (bHasTransferredPosition)
		{
			check(InTransferredPositions.Num() == MeshLODCount);
		}

		for (uint32 LODIt = 0; LODIt < MeshLODCount; ++LODIt)
		{
			check(LODIt == OutRootData.MeshProjectionLODs[LODIt].LODIndex);

			// 2.1. Build a grid around the hair AABB
			const IMeshLODData& MeshLODData = InMeshData->GetMeshLODData(LODIt);
			const TArray<uint32>& IndexBuffer = MeshLODData.GetIndexBuffer();

			const uint32 MaxSectionCount = GetHairStrandsMaxSectionCount();
			const uint32 MaxTriangleCount = GetHairStrandsMaxTriangleCount();

			FBox MeshBound;
			MeshBound.Init();
			const uint32 SectionCount = MeshLODData.GetNumSections();
			
			if ( SectionCount == 0 )
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. MeshLODData has 0 sections."));
				return false;
			}
			
			check(SectionCount > 0);
			for (uint32 SectionIt = 0; SectionIt < SectionCount; ++SectionIt)
			{
				// 2.2.1 Compute the bounding box of the skeletal mesh
				const IMeshSectionData& Section = MeshLODData.GetSection(SectionIt);
				const uint32 TriangleCount = Section.GetNumTriangles();
				const uint32 SectionBaseIndex = Section.GetBaseIndex();

				check(TriangleCount < MaxTriangleCount);
				check(SectionCount < MaxSectionCount);
				check(TriangleCount > 0);

				for (uint32 TriangleIt = 0; TriangleIt < TriangleCount; ++TriangleIt)
				{
					FTriangleGrid::FTriangle T;
					T.TriangleIndex = TriangleIt;
					T.SectionIndex = SectionIt;
					T.SectionBaseIndex = SectionBaseIndex;

					T.I0 = IndexBuffer[T.SectionBaseIndex + T.TriangleIndex * 3 + 0];
					T.I1 = IndexBuffer[T.SectionBaseIndex + T.TriangleIndex * 3 + 1];
					T.I2 = IndexBuffer[T.SectionBaseIndex + T.TriangleIndex * 3 + 2];

					if (bHasTransferredPosition)
					{
						T.P0 = (FVector)InTransferredPositions[LODIt][T.I0];
						T.P1 = (FVector)InTransferredPositions[LODIt][T.I1];
						T.P2 = (FVector)InTransferredPositions[LODIt][T.I2];
					}
					else
					{
						T.P0 = (FVector)MeshLODData.GetVertexPosition(T.I0);
						T.P1 = (FVector)MeshLODData.GetVertexPosition(T.I1);
						T.P2 = (FVector)MeshLODData.GetVertexPosition(T.I2);
					}

					T.UV0 = MeshLODData.GetVertexUV(T.I0, ChannelIndex);
					T.UV1 = MeshLODData.GetVertexUV(T.I1, ChannelIndex);
					T.UV2 = MeshLODData.GetVertexUV(T.I2, ChannelIndex);

					MeshBound += T.P0;
					MeshBound += T.P1;
					MeshBound += T.P2;
				}
			}

			// Take the smallest bounding box between the groom and the skeletal mesh
			const FVector MeshExtent = MeshBound.Max - MeshBound.Min;
			const FVector HairExtent = InStrandsData.BoundingBox.Max - InStrandsData.BoundingBox.Min;
			FVector GridMin;
			FVector GridMax;
			if (MeshExtent.Size() < HairExtent.Size())
			{
				GridMin = MeshBound.Min;
				GridMax = MeshBound.Max;
			}
			else
			{
				GridMin = InStrandsData.BoundingBox.Min;
				GridMax = InStrandsData.BoundingBox.Max;
			}

			FTriangleGrid Grid(GridMin, GridMax, VoxelWorldSize);
			bool bIsGridPopulated = false;
			for (uint32 SectionIt = 0; SectionIt < SectionCount; ++SectionIt)
			{
				// 2.2.2 Insert all triangle within the grid
				const IMeshSectionData& Section = MeshLODData.GetSection(SectionIt);
				const uint32 TriangleCount = Section.GetNumTriangles();
				const uint32 SectionBaseIndex = Section.GetBaseIndex();

				check(TriangleCount < MaxTriangleCount);
				check(SectionCount < MaxSectionCount);
				check(TriangleCount > 0);

				for (uint32 TriangleIt = 0; TriangleIt < TriangleCount; ++TriangleIt)
				{
					FTriangleGrid::FTriangle T;
					T.TriangleIndex = TriangleIt;
					T.SectionIndex = SectionIt;
					T.SectionBaseIndex = SectionBaseIndex;

					T.I0 = IndexBuffer[T.SectionBaseIndex + T.TriangleIndex * 3 + 0];
					T.I1 = IndexBuffer[T.SectionBaseIndex + T.TriangleIndex * 3 + 1];
					T.I2 = IndexBuffer[T.SectionBaseIndex + T.TriangleIndex * 3 + 2];

					if (bHasTransferredPosition)
					{
						T.P0 = (FVector)InTransferredPositions[LODIt][T.I0];
						T.P1 = (FVector)InTransferredPositions[LODIt][T.I1];
						T.P2 = (FVector)InTransferredPositions[LODIt][T.I2];
					}
					else
					{
						T.P0 = (FVector)MeshLODData.GetVertexPosition(T.I0);
						T.P1 = (FVector)MeshLODData.GetVertexPosition(T.I1);
						T.P2 = (FVector)MeshLODData.GetVertexPosition(T.I2);
					}

					T.UV0 = MeshLODData.GetVertexUV(T.I0, ChannelIndex);
					T.UV1 = MeshLODData.GetVertexUV(T.I1, ChannelIndex);
					T.UV2 = MeshLODData.GetVertexUV(T.I2, ChannelIndex);

					bIsGridPopulated = Grid.Insert(T) || bIsGridPopulated;
				}
			}

			if (!bIsGridPopulated)
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. The target skeletal mesh could be missing UVs."));
				return false;
			}

			OutRootData.MeshProjectionLODs[LODIt].RootBarycentricBuffer.SetNum(CurveCount);
			OutRootData.MeshProjectionLODs[LODIt].RootToUniqueTriangleIndexBuffer.SetNum(CurveCount);

			// 2.3. Compute the closest triangle for each root
			//InMeshRenderData->LODRenderData[LODIt].GetNumVertices();

			TArray<FHairStrandsUniqueTriangleIndexFormat::Type> RootTriangleIndexBuffer;
			RootTriangleIndexBuffer.SetNum(CurveCount);

			TArray<FHairStrandsMeshTrianglePositionFormat::Type> RestRootTrianglePosition0Buffer;
			TArray<FHairStrandsMeshTrianglePositionFormat::Type> RestRootTrianglePosition1Buffer;
			TArray<FHairStrandsMeshTrianglePositionFormat::Type> RestRootTrianglePosition2Buffer;
			RestRootTrianglePosition0Buffer.SetNum(CurveCount);
			RestRootTrianglePosition1Buffer.SetNum(CurveCount);
			RestRootTrianglePosition2Buffer.SetNum(CurveCount);

		#if BINDING_PARALLEL_BUILDING
			TAtomic<uint32> bIsValid(1);
			ParallelFor(CurveCount,
				[
					LODIt,
					&InStrandsData,
					&Grid,
					&RootTriangleIndexBuffer,
					&RestRootTrianglePosition0Buffer,
					&RestRootTrianglePosition1Buffer,
					&RestRootTrianglePosition2Buffer,
					&OutRootData,
					&bIsValid
				] (uint32 CurveIndex)
		#else
			for (uint32 CurveIndex = 0; CurveIndex < CurveCount; ++CurveIndex)
		#endif
			{
				const uint32 Offset = InStrandsData.StrandsCurves.CurvesOffset[CurveIndex];
				const FVector& RootP = (FVector)InStrandsData.StrandsPoints.PointsPosition[Offset];
				const FTriangleGrid::FCells Cells = Grid.ToCells(RootP);

				if (Cells.Num() == 0)
				{
				#if BINDING_PARALLEL_BUILDING
					bIsValid = 0; return;
				#else
					return false;
				#endif
				}

				float ClosestDistance = FLT_MAX;
				FTriangleGrid::FTriangle ClosestTriangle;
				FVector2D ClosestBarycentrics;
				for (const FTriangleGrid::FCell* Cell : Cells)
				{
					for (const FTriangleGrid::FTriangle& CellTriangle : Cell->Triangles)
					{
						const FTrianglePoint Tri = ComputeClosestPoint(CellTriangle, RootP);
						const float Distance = FVector::Distance(Tri.P, RootP);
						if (Distance < ClosestDistance)
						{
							ClosestDistance = Distance;
							ClosestTriangle = CellTriangle;
							ClosestBarycentrics = FVector2D(Tri.Barycentric.X, Tri.Barycentric.Y);
						}
					}
				}
				check(ClosestDistance < FLT_MAX);

				// Record closest triangle and the root's barycentrics
				const uint32 EncodedBarycentrics = FHairStrandsRootUtils::EncodeBarycentrics(FVector2f(ClosestBarycentrics));	// LWC_TODO: Precision loss
				const uint32 EncodedTriangleIndex = FHairStrandsRootUtils::EncodeTriangleIndex(ClosestTriangle.TriangleIndex, ClosestTriangle.SectionIndex);
				OutRootData.MeshProjectionLODs[LODIt].RootBarycentricBuffer[CurveIndex] = EncodedBarycentrics;

				RootTriangleIndexBuffer[CurveIndex] = EncodedTriangleIndex;
				RestRootTrianglePosition0Buffer[CurveIndex] = FVector4f((FVector3f)ClosestTriangle.P0, FHairStrandsRootUtils::PackUVsToFloat(FVector2f(ClosestTriangle.UV0)));	// LWC_TODO: Precision loss
				RestRootTrianglePosition1Buffer[CurveIndex] = FVector4f((FVector3f)ClosestTriangle.P1, FHairStrandsRootUtils::PackUVsToFloat(FVector2f(ClosestTriangle.UV1)));	// LWC_TODO: Precision loss
				RestRootTrianglePosition2Buffer[CurveIndex] = FVector4f((FVector3f)ClosestTriangle.P2, FHairStrandsRootUtils::PackUVsToFloat(FVector2f(ClosestTriangle.UV2)));	// LWC_TODO: Precision loss
			}
		#if BINDING_PARALLEL_BUILDING
			);
			if (bIsValid == 0)
			{
				return false;
			}
		#endif

			// Build list of unique triangles
			TArray<uint32> UniqueSectionId;
			TArray<uint32> UniqueTriangleToRootList;
			TMap<uint32, TArray<uint32>> UniqueTriangleToRootMap;
			for (uint32 CurveIndex = 0; CurveIndex < CurveCount; ++CurveIndex)
			{
				const uint32 EncodedTriangleId = RootTriangleIndexBuffer[CurveIndex];
				if (TArray<uint32>* CurvesList = UniqueTriangleToRootMap.Find(EncodedTriangleId))
				{
					CurvesList->Add(CurveIndex);
				}
				else
				{
					// Add unique section
					uint32 TriangleIndex;
					uint32 SectionIndex;
					FHairStrandsRootUtils::DecodeTriangleIndex(EncodedTriangleId, TriangleIndex, SectionIndex);
					UniqueSectionId.AddUnique(SectionIndex);

					// Add unique triangle
					UniqueTriangleToRootList.Add(EncodedTriangleId);
					TArray<uint32>& NewCurvesList = UniqueTriangleToRootMap.Add(EncodedTriangleId);
					NewCurvesList.Add(CurveIndex);
				}
			}

			// Sort unique triangle per section and triangle ID (encoded triangle ID stores section ID in high bits)
			UniqueTriangleToRootList.Sort();
			UniqueSectionId.Sort();

			// Build final unique triangle list and the root-to-unique-triangle mapping
			const uint32 UniqueTriangleCount = UniqueTriangleToRootList.Num();
			OutRootData.MeshProjectionLODs[LODIt].UniqueTriangleIndexBuffer.Reserve(UniqueTriangleCount);
			OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition0Buffer.Reserve(UniqueTriangleCount);
			OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition1Buffer.Reserve(UniqueTriangleCount);
			OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition2Buffer.Reserve(UniqueTriangleCount);
			for (uint32 EncodedTriangleId : UniqueTriangleToRootList)
			{
				auto It = UniqueTriangleToRootMap.Find(EncodedTriangleId);
				check(It);

				OutRootData.MeshProjectionLODs[LODIt].UniqueTriangleIndexBuffer.Add(EncodedTriangleId);

				const uint32 FirstCurveIndex = (*It)[0];
				OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition0Buffer.Add(RestRootTrianglePosition0Buffer[FirstCurveIndex]);
				OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition1Buffer.Add(RestRootTrianglePosition1Buffer[FirstCurveIndex]);
				OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition2Buffer.Add(RestRootTrianglePosition2Buffer[FirstCurveIndex]);

				// Write for each root, the index of the triangle
				const uint32 UniqueTriangleIndex = OutRootData.MeshProjectionLODs[LODIt].UniqueTriangleIndexBuffer.Num()-1;
				for (uint32 CurveIndex : *It)
				{
					OutRootData.MeshProjectionLODs[LODIt].RootToUniqueTriangleIndexBuffer[CurveIndex] = UniqueTriangleIndex;
				}
			}

			// Sanity check
			check(OutRootData.MeshProjectionLODs[LODIt].RootToUniqueTriangleIndexBuffer.Num() == CurveCount);
			check(OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition0Buffer.Num() == UniqueTriangleCount);
			check(OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition1Buffer.Num() == UniqueTriangleCount);
			check(OutRootData.MeshProjectionLODs[LODIt].RestUniqueTrianglePosition2Buffer.Num() == UniqueTriangleCount);
			check(OutRootData.MeshProjectionLODs[LODIt].UniqueTriangleIndexBuffer.Num() == UniqueTriangleCount);

			// Update the root mesh projection data with unique valid mesh section IDs, based on the projection data
			OutRootData.MeshProjectionLODs[LODIt].UniqueSectionIds = UniqueSectionId;
		}

		return true;
	}
}// namespace GroomBinding_Project

///////////////////////////////////////////////////////////////////////////////////////////////////
// Mesh transfer

namespace GroomBinding_Transfer
{

	struct FTriangleGrid2D
	{
		struct FTriangle
		{
			uint32  TriangleIndex;
			uint32  SectionIndex;
			uint32  SectionBaseIndex;

			uint32  I0;
			uint32  I1;
			uint32  I2;

			FVector P0;
			FVector P1;
			FVector P2;

			FVector2D UV0;
			FVector2D UV1;
			FVector2D UV2;
		};

		struct FCell
		{
			TArray<FTriangle> Triangles;
		};
		typedef TArray<const FCell*> FCells;

		FTriangleGrid2D(uint32 Resolution)
		{
			GridResolution.X = Resolution;
			GridResolution.Y = Resolution;
			MinBound = FVector2D(0,0);
			MaxBound = FVector2D(1,1);

			Cells.SetNum(GridResolution.X * GridResolution.Y);
		}

		void Reset()
		{
			Cells.Empty();
			Cells.SetNum(GridResolution.X * GridResolution.Y);
		}

		FORCEINLINE bool IsValid(const FIntPoint& P) const
		{
			return
				0 <= P.X && P.X < GridResolution.X &&
				0 <= P.Y && P.Y < GridResolution.Y;
		}

		FORCEINLINE bool IsOutside(const FVector2D& MinP, const FVector2D& MaxP) const
		{
			return
				(MaxP.X <= MinBound.X || MaxP.Y <= MinBound.Y) ||
				(MinP.X >= MaxBound.X || MinP.Y >= MaxBound.Y);
		}

		FORCEINLINE FIntPoint ClampToVolume(const FIntPoint& CellCoord, bool& bIsValid) const
		{
			bIsValid = IsValid(CellCoord);
			return FIntPoint(
				FMath::Clamp(CellCoord.X, 0, GridResolution.X - 1),
				FMath::Clamp(CellCoord.Y, 0, GridResolution.Y - 1));
		}

		FORCEINLINE FIntPoint ToCellCoord(const FVector2D& P) const
		{
			bool bIsValid = false;
			FVector2D PP;
			PP.X = FMath::Clamp(P.X, 0.f, 1.f);
			PP.Y = FMath::Clamp(P.Y, 0.f, 1.f);
			const FIntPoint CellCoord = FIntPoint(FMath::FloorToInt(PP.X * GridResolution.X), FMath::FloorToInt(PP.Y * GridResolution.Y));
			return ClampToVolume(CellCoord, bIsValid);
		}

		uint32 ToIndex(const FIntPoint& CellCoord) const
		{
			uint32 CellIndex = CellCoord.X + CellCoord.Y * GridResolution.X;
			check(CellIndex < uint32(Cells.Num()));
			return CellIndex;
		}

		FCells ToCells(const FVector2D& P)
		{
			FCells Out;

			bool bIsValid = false;
			const FIntPoint Coord = ToCellCoord(P);
			{
				const uint32 LinearIndex = ToIndex(Coord);
				if (Cells[LinearIndex].Triangles.Num() > 0)
				{
					Out.Add(&Cells[LinearIndex]);
					bIsValid = true;
				}
			}

			int32 Kernel = 1;
			while (!bIsValid)
			{
				for (int32 Y = -Kernel; Y <= Kernel; ++Y)
				for (int32 X = -Kernel; X <= Kernel; ++X)
				{
					if (FMath::Abs(X) != Kernel && FMath::Abs(Y) != Kernel)
						continue;

					const FIntPoint Offset(X, Y);
					FIntPoint C = Coord + Offset;
					C.X = FMath::Clamp(C.X, 0, GridResolution.X - 1);
					C.Y = FMath::Clamp(C.Y, 0, GridResolution.Y - 1);

					const uint32 LinearIndex = ToIndex(C);
					if (Cells[LinearIndex].Triangles.Num() > 0)
					{
						Out.Add(&Cells[LinearIndex]);
						bIsValid = true;
					}
				}
				++Kernel;
			}

			return Out;
		}

		bool Insert(const FTriangle& T)
		{
			FVector2D TriMinBound;
			TriMinBound.X = FMath::Min(T.UV0.X, FMath::Min(T.UV1.X, T.UV2.X));
			TriMinBound.Y = FMath::Min(T.UV0.Y, FMath::Min(T.UV1.Y, T.UV2.Y));

			FVector2D TriMaxBound;
			TriMaxBound.X = FMath::Max(T.UV0.X, FMath::Max(T.UV1.X, T.UV2.X));
			TriMaxBound.Y = FMath::Max(T.UV0.Y, FMath::Max(T.UV1.Y, T.UV2.Y));

			if (IsOutside(TriMinBound, TriMaxBound))
			{
				return false;
			}

			const FIntPoint MinCoord = ToCellCoord(TriMinBound);
			const FIntPoint MaxCoord = ToCellCoord(TriMaxBound);

			// Insert triangle in all cell covered by the AABB of the triangle
			bool bInserted = false;
			for (int32 Y = MinCoord.Y; Y <= MaxCoord.Y; ++Y)
			{
				for (int32 X = MinCoord.X; X <= MaxCoord.X; ++X)
				{
					const FIntPoint CellIndex(X, Y);
					if (IsValid(CellIndex))
					{
						const uint32 CellLinearIndex = ToIndex(CellIndex);
						Cells[CellLinearIndex].Triangles.Add(T);
						bInserted = true;
					}
				}
			}
			return bInserted;
		}

		FVector2D MinBound;
		FVector2D MaxBound;
		FIntPoint GridResolution;
		TArray<FCell> Cells;
	};


	// Closest point on A triangle from another point in UV space
	// Code from the book "Real-Time Collision Detection" by Christer Ericson
	struct FTrianglePoint
	{
		FVector P;
		FVector Barycentric;
	};

	FTrianglePoint ComputeClosestPoint(const FVector2D& TriUV0, const FVector2D& TriUV1, const FVector2D& TriUV2, const FVector2D& UVs)
	{
		const FVector A = FVector(TriUV0, 0);
		const FVector B = FVector(TriUV1, 0);
		const FVector C = FVector(TriUV2, 0);
		const FVector P = FVector(UVs, 0);

		// Check if P is in vertex region outside A.
		FVector AB = B - A;
		FVector AC = C - A;
		FVector AP = P - A;
		float D1 = FVector::DotProduct(AB, AP);
		float D2 = FVector::DotProduct(AC, AP);
		if (D1 <= 0.f && D2 <= 0.f)
		{
			FTrianglePoint Out;
			Out.P = A;
			Out.Barycentric = FVector(1, 0, 0);
			return Out;
		}

		// Check if P is in vertex region outside B.
		FVector BP = P - B;
		float D3 = FVector::DotProduct(AB, BP);
		float D4 = FVector::DotProduct(AC, BP);
		if (D3 >= 0.f && D4 <= D3)
		{
			FTrianglePoint Out;
			Out.P = B;
			Out.Barycentric = FVector(0, 1, 0);
			return Out;
		}

		// Check if P is in edge region of AB, and if so, return the projection of P onto AB.
		float VC = D1 * D4 - D3 * D2;
		if (VC <= 0.f && D1 >= 0.f && D3 <= 0.f)
		{
			float V = D1 / (D1 - D3);

			FTrianglePoint Out;
			Out.P = A + V * AB;
			Out.Barycentric = FVector(1 - V, V, 0);
			return Out;
		}

		// Check if P is in vertex region outside C.
		FVector CP = P - C;
		float D5 = FVector::DotProduct(AB, CP);
		float D6 = FVector::DotProduct(AC, CP);
		if (D6 >= 0.f && D5 <= D6)
		{
			FTrianglePoint Out;
			Out.P = C;
			Out.Barycentric = FVector(0, 0, 1);
			return Out;
		}

		// Check if P is in edge region of AC, and if so, return the projection of P onto AC.
		float VB = D5 * D2 - D1 * D6;
		if (VB <= 0.f && D2 >= 0.f && D6 <= 0.f)
		{
			float W = D2 / (D2 - D6);
			FTrianglePoint Out;
			Out.P = A + W * AC;
			Out.Barycentric = FVector(1 - W, 0, W);
			return Out;
		}

		// Check if P is in edge region of BC, and if so, return the projection of P onto BC.
		float VA = D3 * D6 - D5 * D4;
		if (VA <= 0.f && D4 - D3 >= 0.f && D5 - D6 >= 0.f)
		{
			float W = (D4 - D3) / (D4 - D3 + D5 - D6);
			FTrianglePoint Out;
			Out.P = B + W * (C - B);
			Out.Barycentric = FVector(0, 1 - W, W);
			return Out;
		}

		// P must be inside the face region. Compute the closest point through its barycentric coordinates (u,V,W).
		float Denom = 1.f / (VA + VB + VC);
		float V = VB * Denom;
		float W = VC * Denom;

		FTrianglePoint Out;
		Out.P = A + AB * V + AC * W;
		Out.Barycentric = FVector(1 - V - W, V, W);
		return Out;
	}

	bool Transfer(
		const IMeshData* InSourceMeshData,
		const IMeshData* InTargetMeshData,
		TArray<TArray<FVector3f>>& OutTransferredPositions, const int32 MatchingSection)
	{

		// 1. Insert triangles into a 2D UV grid
		auto BuildGrid = [InSourceMeshData](
			int32 InSourceLODIndex,
			int32 InSourceSectionId,
			int32 InTargetSectionId,
			int32 InChannelIndex,
			FTriangleGrid2D& OutGrid)
		{
			// Notes:
			// LODs are transfered using the LOD0 of the source mesh, as the LOD count can mismatch between source and target meshes.
			// Assume that the section 0 contains the head section, which is where the hair/facial hair should be projected on
			const IMeshLODData& MeshLODData = InSourceMeshData->GetMeshLODData(InSourceLODIndex);
			const uint32 SourceTriangleCount = MeshLODData.GetSection(InSourceSectionId).GetNumTriangles();
			const uint32 SourceSectionBaseIndex = MeshLODData.GetSection(InSourceSectionId).GetBaseIndex();

			const TArray<uint32>& SourceIndexBuffer = MeshLODData.GetIndexBuffer();

			OutGrid.Reset();

			bool bIsGridPopulated = false;
			for (uint32 SourceTriangleIt = 0; SourceTriangleIt < SourceTriangleCount; ++SourceTriangleIt)
			{
				FTriangleGrid2D::FTriangle T;
				T.SectionIndex = InSourceSectionId;
				T.SectionBaseIndex = SourceSectionBaseIndex;
				T.TriangleIndex = SourceTriangleIt;

				T.I0 = SourceIndexBuffer[T.SectionBaseIndex + SourceTriangleIt * 3 + 0];
				T.I1 = SourceIndexBuffer[T.SectionBaseIndex + SourceTriangleIt * 3 + 1];
				T.I2 = SourceIndexBuffer[T.SectionBaseIndex + SourceTriangleIt * 3 + 2];

				T.P0 = (FVector)MeshLODData.GetVertexPosition(T.I0);
				T.P1 = (FVector)MeshLODData.GetVertexPosition(T.I1);
				T.P2 = (FVector)MeshLODData.GetVertexPosition(T.I2);

				T.UV0 = MeshLODData.GetVertexUV(T.I0, InChannelIndex);
				T.UV1 = MeshLODData.GetVertexUV(T.I1, InChannelIndex);
				T.UV2 = MeshLODData.GetVertexUV(T.I2, InChannelIndex);

				bIsGridPopulated = OutGrid.Insert(T) || bIsGridPopulated;
			}

			return bIsGridPopulated;
		};

		// 1. Insert triangles into a 2D UV grid
		const uint32 ChannelIndex = 0;
		const uint32 SourceLODIndex = 0;
		const IMeshLODData& SourceMeshLODData = InSourceMeshData->GetMeshLODData(SourceLODIndex);
		const bool bIsMatchingSectionValid = MatchingSection < SourceMeshLODData.GetNumSections();
		const int32 SourceSectionId = bIsMatchingSectionValid ? MatchingSection : 0;
		if (!bIsMatchingSectionValid && GHairStrandsBindingBuilderWarningEnable > 0)
		{
			UE_LOG(LogHairStrands, Warning, TEXT("[Groom] Binding asset will not respect the requested 'Matching section' %d. The source skeletal mesh does not have such a section. Instead 'Matching Section' 0 will be used."), MatchingSection);
		}
		const int32 TargetSectionId = SourceSectionId;
		FTriangleGrid2D Grid(256);
		{
			const bool bIsGridPopulated = BuildGrid(SourceLODIndex, SourceSectionId, TargetSectionId, ChannelIndex, Grid);
			if (!bIsGridPopulated)
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. The source skeletal mesh is missing or has invalid UVs."));
				return false;
			}
		}

		// 2. Look for closest triangle point in UV space
		// Make this run in parallel
		const uint32 TargetLODCount = InTargetMeshData->GetNumLODs();
		OutTransferredPositions.SetNum(TargetLODCount);
		for (uint32 TargetLODIndex = 0; TargetLODIndex < TargetLODCount; ++TargetLODIndex)
		{
			// Check that the target SectionId is valid for the current LOD. 
			// If this is not the case, then fall back to section 0 and rebuild the source triangle grid to match the same section ID (1.)
			int32 LocalSourceSectionId = SourceSectionId;
			int32 LocalTargetSectionId = TargetSectionId;
			const IMeshLODData& TargetMeshLODData = InTargetMeshData->GetMeshLODData(TargetLODIndex);

			if (LocalTargetSectionId >= TargetMeshLODData.GetNumSections())
			{
				if ( TargetMeshLODData.GetNumSections() == 0 )
				{
					UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built for LOD %d. TargetMeshLODData.GetNumSections() == 0."));
					return false;
				}

				if (GHairStrandsBindingBuilderWarningEnable > 0)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] Binding asset will not respect the requested 'Matching section' %d for LOD %d. The target skeletal mesh does not have such a section for this LOD. Instead section 0 will be used for this given LOD."), TargetSectionId, TargetLODIndex);
				}

				LocalTargetSectionId = 0;
				LocalSourceSectionId = 0;
				const bool bIsGridPopulated = BuildGrid(SourceLODIndex, LocalSourceSectionId, LocalTargetSectionId, ChannelIndex, Grid);
				if (!bIsGridPopulated)
				{
					UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built for LOD %d. The source skeletal mesh is missing or has invalid UVs."));
					return false;
				}
			}

			const uint32 TargetTriangleCount = TargetMeshLODData.GetSection(LocalTargetSectionId).GetNumTriangles();
			const uint32 TargetVertexCount = TargetMeshLODData.GetNumVertices();

			TSet<FVector2D> UVs;
			for (uint32 TargetVertexIt = 0; TargetVertexIt < TargetVertexCount; ++TargetVertexIt)
			{
				const FVector2D Target_UV = TargetMeshLODData.GetVertexUV(TargetVertexIt, ChannelIndex);
				UVs.Add(Target_UV);
			}

			// Simple check to see if the target UVs are meaningful before doing the heavy work
			const int32 NumUVLowerLimit = FMath::Max(1, int32(TargetVertexCount * 0.01f));
			if (UVs.Num() < NumUVLowerLimit)
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. The target skeletal mesh is missing or has invalid UVs."));
				return false;
			}

			OutTransferredPositions[TargetLODIndex].SetNum(TargetVertexCount);

#if BINDING_PARALLEL_BUILDING
			ParallelFor(TargetVertexCount,
				[
					LocalTargetSectionId,
					ChannelIndex,
					&TargetMeshLODData,
					TargetLODIndex,
					&Grid,
					&OutTransferredPositions
				] (uint32 TargetVertexIt)
#else
			for (uint32 TargetVertexIt = 0; TargetVertexIt < TargetVertexCount; ++TargetVertexIt)
#endif
			{
				int32 SectionIt = 0;
				TargetMeshLODData.GetSectionFromVertexIndex(TargetVertexIt, SectionIt);
				if (SectionIt != LocalTargetSectionId)
				{
					OutTransferredPositions[TargetLODIndex][TargetVertexIt] = FVector3f(0,0,0);
#if BINDING_PARALLEL_BUILDING
					return;
#else
					continue;
#endif
				}

				const FVector3f Target_P    = TargetMeshLODData.GetVertexPosition(TargetVertexIt);
				const FVector2D Target_UV = TargetMeshLODData.GetVertexUV(TargetVertexIt, ChannelIndex);

				// 2.1 Query closest triangles
				FVector3f RetargetedVertexPosition = Target_P;
				FTriangleGrid2D::FCells Cells = Grid.ToCells(Target_UV);

				// 2.2 Compute the closest triangle and comput the retarget position 
				float ClosestUVDistance = FLT_MAX;
				for (const FTriangleGrid2D::FCell* Cell : Cells)
				{
					for (const FTriangleGrid2D::FTriangle& CellTriangle : Cell->Triangles)
					{
						const FTrianglePoint ClosestPoint = ComputeClosestPoint(CellTriangle.UV0, CellTriangle.UV1, CellTriangle.UV2, Target_UV);
						const float UVDistanceToTriangle = FVector2D::Distance(FVector2D(ClosestPoint.P.X, ClosestPoint.P.Y), Target_UV);
						if (UVDistanceToTriangle < ClosestUVDistance)
						{
							RetargetedVertexPosition = FVector3f(
								ClosestPoint.Barycentric.X * CellTriangle.P0 +
								ClosestPoint.Barycentric.Y * CellTriangle.P1 +
								ClosestPoint.Barycentric.Z * CellTriangle.P2);
							ClosestUVDistance = UVDistanceToTriangle;
						}
					}
				}
				check(ClosestUVDistance < FLT_MAX);
				OutTransferredPositions[TargetLODIndex][TargetVertexIt] = RetargetedVertexPosition;
			}
#if BINDING_PARALLEL_BUILDING
			);
#endif
		}
		return true;
	}
}
// namespace GroomBinding_Transfer

static void InitHairStrandsRootData(FHairStrandsRootData& Out, const FHairStrandsDatas* HairStrandsDatas, uint32 LODCount, uint32 NumSamples)
{
	check(HairStrandsDatas);

	Out.RootCount = HairStrandsDatas->GetNumCurves();
	Out.PointCount = HairStrandsDatas->GetNumPoints();

	const uint32 CurveCount = HairStrandsDatas->GetNumCurves();
	Out.VertexToCurveIndexBuffer.SetNum(HairStrandsDatas->GetNumPoints());
	
	for (uint32 CurveIndex = 0; CurveIndex < CurveCount; ++CurveIndex)
	{
		const uint32 RootIndex = HairStrandsDatas->StrandsCurves.CurvesOffset[CurveIndex];
		const uint32 PointCount = HairStrandsDatas->StrandsCurves.CurvesCount[CurveIndex];
		for (uint32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			Out.VertexToCurveIndexBuffer[RootIndex + PointIndex] = CurveIndex; // RootIndex;
		}
	}

	Out.MeshProjectionLODs.SetNum(LODCount);
	uint32 LODIndex = 0;
	for (FHairStrandsRootData::FMeshProjectionLOD& MeshProjectionLOD : Out.MeshProjectionLODs)
	{
		MeshProjectionLOD.SampleCount = NumSamples;
		MeshProjectionLOD.LODIndex = LODIndex++;
		MeshProjectionLOD.MeshInterpolationWeightsBuffer.Empty();
		MeshProjectionLOD.MeshSampleIndicesBuffer.Empty();
		MeshProjectionLOD.RestSamplePositionsBuffer.Empty();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Convert data into bulk data

namespace GroomBinding_BulkCopy
{

template<typename TFormatType>
void CopyToBulkData(FByteBulkData& Out, const TArray<typename TFormatType::Type>& Data)
{
	const uint32 DataSizeInByte = Data.Num() * sizeof(typename TFormatType::BulkType);

	// The buffer is then stored into bulk data
	Out.Lock(LOCK_READ_WRITE);
	void* BulkBuffer = Out.Realloc(DataSizeInByte);
	FMemory::Memcpy(BulkBuffer, Data.GetData(), DataSizeInByte);
	Out.Unlock();
}

template<typename TFormatType>
void CopyFromBulkData(TArray<typename TFormatType::Type>& Out, const FByteBulkData& In)
{
	const uint32 InDataSize = In.GetBulkDataSize();
	const uint32 ElementCount = InDataSize / sizeof(typename TFormatType::BulkType);
	Out.SetNum(ElementCount);

	// The buffer is then stored into bulk data
	const uint8* InData = (const uint8*)In.LockReadOnly();
	FMemory::Memcpy(Out.GetData(), InData, InDataSize);
	In.Unlock();
}

// Convert "root data" -> "root bulk data"
static void BuildRootBulkData(
	FHairStrandsRootBulkData& Out,
	const FHairStrandsRootData& In)
{
	Out.RootCount = In.RootCount;
	Out.PointCount = In.PointCount;

	CopyToBulkData<FHairStrandsIndexFormat>(Out.VertexToCurveIndexBuffer, In.VertexToCurveIndexBuffer);

	const uint32 MeshLODCount = In.MeshProjectionLODs.Num();
	Out.MeshProjectionLODs.SetNum(MeshLODCount);
	for (uint32 MeshLODIt = 0; MeshLODIt < MeshLODCount; ++MeshLODIt)
	{
		const bool bHasValidSamples =
			In.MeshProjectionLODs[MeshLODIt].MeshInterpolationWeightsBuffer.Num() > 0 &&
			In.MeshProjectionLODs[MeshLODIt].MeshSampleIndicesBuffer.Num() > 0 &&
			In.MeshProjectionLODs[MeshLODIt].RestSamplePositionsBuffer.Num() > 0;

		Out.MeshProjectionLODs[MeshLODIt].LODIndex = In.MeshProjectionLODs[MeshLODIt].LODIndex;
		Out.MeshProjectionLODs[MeshLODIt].SampleCount = bHasValidSamples ? In.MeshProjectionLODs[MeshLODIt].SampleCount : 0u;
		Out.MeshProjectionLODs[MeshLODIt].UniqueTriangleCount = In.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition0Buffer.Num();

		CopyToBulkData<FHairStrandsUniqueTriangleIndexFormat>(Out.MeshProjectionLODs[MeshLODIt].UniqueTriangleIndexBuffer, In.MeshProjectionLODs[MeshLODIt].UniqueTriangleIndexBuffer);
		CopyToBulkData<FHairStrandsRootBarycentricFormat>(Out.MeshProjectionLODs[MeshLODIt].RootBarycentricBuffer, In.MeshProjectionLODs[MeshLODIt].RootBarycentricBuffer);
		CopyToBulkData<FHairStrandsRootToUniqueTriangleIndexFormat>(Out.MeshProjectionLODs[MeshLODIt].RootToUniqueTriangleIndexBuffer, In.MeshProjectionLODs[MeshLODIt].RootToUniqueTriangleIndexBuffer);

		CopyToBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition0Buffer, In.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition0Buffer);
		CopyToBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition1Buffer, In.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition1Buffer);
		CopyToBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition2Buffer, In.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition2Buffer);

		Out.MeshProjectionLODs[MeshLODIt].UniqueSectionIndices = In.MeshProjectionLODs[MeshLODIt].UniqueSectionIds;

		if (bHasValidSamples)
		{
			CopyToBulkData<FHairStrandsWeightFormat>(Out.MeshProjectionLODs[MeshLODIt].MeshInterpolationWeightsBuffer, In.MeshProjectionLODs[MeshLODIt].MeshInterpolationWeightsBuffer);
			CopyToBulkData<FHairStrandsIndexFormat>(Out.MeshProjectionLODs[MeshLODIt].MeshSampleIndicesBuffer, In.MeshProjectionLODs[MeshLODIt].MeshSampleIndicesBuffer);
			CopyToBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestSamplePositionsBuffer, In.MeshProjectionLODs[MeshLODIt].RestSamplePositionsBuffer);
		}
		else
		{
			Out.MeshProjectionLODs[MeshLODIt].MeshInterpolationWeightsBuffer.RemoveBulkData();
			Out.MeshProjectionLODs[MeshLODIt].MeshSampleIndicesBuffer.RemoveBulkData();
			Out.MeshProjectionLODs[MeshLODIt].RestSamplePositionsBuffer.RemoveBulkData();
		}
	}
}

// Convert "root data" <- "root bulk data"
static void BuildRootData(
	FHairStrandsRootData& Out,
	const FHairStrandsRootBulkData& In)
{
	Out.RootCount = In.RootCount;
	Out.PointCount = In.PointCount;

	CopyFromBulkData<FHairStrandsIndexFormat>(Out.VertexToCurveIndexBuffer, In.VertexToCurveIndexBuffer);

	const uint32 MeshLODCount = In.MeshProjectionLODs.Num();
	Out.MeshProjectionLODs.SetNum(MeshLODCount);
	for (uint32 MeshLODIt = 0; MeshLODIt < MeshLODCount; ++MeshLODIt)
	{
		const bool bHasValidSamples = In.MeshProjectionLODs[MeshLODIt].SampleCount > 0;

		Out.MeshProjectionLODs[MeshLODIt].LODIndex = In.MeshProjectionLODs[MeshLODIt].LODIndex;
		Out.MeshProjectionLODs[MeshLODIt].SampleCount = bHasValidSamples ? In.MeshProjectionLODs[MeshLODIt].SampleCount : 0u;

		CopyFromBulkData<FHairStrandsUniqueTriangleIndexFormat>(Out.MeshProjectionLODs[MeshLODIt].UniqueTriangleIndexBuffer, In.MeshProjectionLODs[MeshLODIt].UniqueTriangleIndexBuffer);
		CopyFromBulkData<FHairStrandsRootToUniqueTriangleIndexFormat>(Out.MeshProjectionLODs[MeshLODIt].RootToUniqueTriangleIndexBuffer, In.MeshProjectionLODs[MeshLODIt].RootToUniqueTriangleIndexBuffer);
		CopyFromBulkData<FHairStrandsRootBarycentricFormat>(Out.MeshProjectionLODs[MeshLODIt].RootBarycentricBuffer, In.MeshProjectionLODs[MeshLODIt].RootBarycentricBuffer);

		CopyFromBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition0Buffer, In.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition0Buffer);
		CopyFromBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition1Buffer, In.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition1Buffer);
		CopyFromBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition2Buffer, In.MeshProjectionLODs[MeshLODIt].RestUniqueTrianglePosition2Buffer);

		Out.MeshProjectionLODs[MeshLODIt].UniqueSectionIds = In.MeshProjectionLODs[MeshLODIt].UniqueSectionIndices;

		if (bHasValidSamples)
		{
			CopyFromBulkData<FHairStrandsWeightFormat>(Out.MeshProjectionLODs[MeshLODIt].MeshInterpolationWeightsBuffer, In.MeshProjectionLODs[MeshLODIt].MeshInterpolationWeightsBuffer);
			CopyFromBulkData<FHairStrandsIndexFormat>(Out.MeshProjectionLODs[MeshLODIt].MeshSampleIndicesBuffer, In.MeshProjectionLODs[MeshLODIt].MeshSampleIndicesBuffer);
			CopyFromBulkData<FHairStrandsMeshTrianglePositionFormat>(Out.MeshProjectionLODs[MeshLODIt].RestSamplePositionsBuffer, In.MeshProjectionLODs[MeshLODIt].RestSamplePositionsBuffer);
		}
		else
		{
			Out.MeshProjectionLODs[MeshLODIt].MeshInterpolationWeightsBuffer.Empty();
			Out.MeshProjectionLODs[MeshLODIt].MeshSampleIndicesBuffer.Empty();
			Out.MeshProjectionLODs[MeshLODIt].RestSamplePositionsBuffer.Empty();
		}
	}
}

// Convert the root data into root bulk data
static void BuildRootBulkData(
	TArray<FHairGroupBulkData>& Out,
	const TArray<FHairRootGroupData>& In)
{
	const uint32 GroupCount = In.Num();
	Out.SetNum(GroupCount);
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		BuildRootBulkData(Out[GroupIt].SimRootBulkData, In[GroupIt].SimRootData);
		BuildRootBulkData(Out[GroupIt].RenRootBulkData, In[GroupIt].RenRootData);

		const uint32 LODCount = In[GroupIt].CardsRootData.Num();
		Out[GroupIt].CardsRootBulkData.SetNum(LODCount);
		for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			BuildRootBulkData(Out[GroupIt].CardsRootBulkData[LODIt], In[GroupIt].CardsRootData[LODIt]);
		}
	}
}

} // namespace GroomBinding_BulkCopy

  ///////////////////////////////////////////////////////////////////////////////////////////////////
// Main entry (CPU path)
static bool InternalBuildBinding_CPU(UGroomBindingAsset* BindingAsset, bool bInitResources)
{
#if WITH_EDITORONLY_DATA
	if (!BindingAsset ||
		!BindingAsset->Groom ||
		!BindingAsset->HasValidTarget() ||
		BindingAsset->Groom->GetNumHairGroups() == 0)
	{
		UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset cannot be created/rebuilt."));
		return false;
	}

	// 1. Build groom root data
	TArray<FHairRootGroupData> OutHairGroupDatas;
	{

		BindingAsset->Groom->ConditionalPostLoad();

		const int32 NumInterpolationPoints = BindingAsset->NumInterpolationPoints;
		UGroomAsset* GroomAsset = BindingAsset->Groom;

		TUniquePtr<IMeshData> SourceMeshData;
		TUniquePtr<IMeshData> TargetMeshData;
		if (BindingAsset->GroomBindingType == EGroomBindingMeshType::SkeletalMesh)
		{
			BindingAsset->TargetSkeletalMesh->ConditionalPostLoad();
			if (BindingAsset->SourceSkeletalMesh)
			{
				BindingAsset->SourceSkeletalMesh->ConditionalPostLoad();
			}

			SourceMeshData = TUniquePtr<FSkeletalMeshData, TDefaultDelete<IMeshData>>(new FSkeletalMeshData(BindingAsset->SourceSkeletalMesh));
			TargetMeshData = TUniquePtr<FSkeletalMeshData, TDefaultDelete<IMeshData>>(new FSkeletalMeshData(BindingAsset->TargetSkeletalMesh));
		}
		else
		{
			BindingAsset->TargetGeometryCache->ConditionalPostLoad();
			if (BindingAsset->SourceGeometryCache)
			{
				BindingAsset->SourceGeometryCache->ConditionalPostLoad();
			}

			SourceMeshData = TUniquePtr<FGeometryCacheData, TDefaultDelete<IMeshData>>(new FGeometryCacheData(BindingAsset->SourceGeometryCache));
			TargetMeshData = TUniquePtr<FGeometryCacheData, TDefaultDelete<IMeshData>>(new FGeometryCacheData(BindingAsset->TargetGeometryCache));
		}

		if (!TargetMeshData->IsValid())
		{
			UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. Target mesh is not valid."));
			return false;
		}
		const uint32 GroupCount = GroomAsset->GetNumHairGroups();
		const uint32 MeshLODCount = TargetMeshData->GetNumLODs();

		uint32 GroupIndex = 0;
		for (const FHairGroupData& GroupData : GroomAsset->HairGroupsData)
		{
			FHairStrandsDatas StrandsData;
			FHairStrandsDatas GuidesData;
			GroomAsset->GetHairStrandsDatas(GroupIndex, StrandsData, GuidesData);

			FHairRootGroupData& OutData = OutHairGroupDatas.AddDefaulted_GetRef();
			InitHairStrandsRootData(OutData.RenRootData, &StrandsData, MeshLODCount, NumInterpolationPoints);
			InitHairStrandsRootData(OutData.SimRootData, &GuidesData, MeshLODCount, NumInterpolationPoints);

			const uint32 CardsLODCount = GroupData.Cards.LODs.Num();
			OutData.CardsRootData.SetNum(GroupData.Cards.LODs.Num());
			for (uint32 CardsLODIt = 0; CardsLODIt < CardsLODCount; ++CardsLODIt)
			{
				if (GroupData.Cards.IsValid(CardsLODIt))
				{
					FHairStrandsDatas LODGuidesData;
					const bool bIsValid = GroomAsset->GetHairCardsGuidesDatas(GroupIndex, CardsLODIt, LODGuidesData);
					if (bIsValid)
					{
						InitHairStrandsRootData(OutData.CardsRootData[CardsLODIt], &LODGuidesData, MeshLODCount, NumInterpolationPoints);
					}
				}
			}
			++GroupIndex;
		}

		const bool bNeedTransferPosition = SourceMeshData->IsValid();

		// Create mapping between the source & target using their UV
		uint32 WorkItemCount = 1 + (bNeedTransferPosition ? 1 : 0); //RBF + optional position transfer
		for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
		{
			WorkItemCount += 2; // Sim & Render
			const uint32 CardsLODCount = OutHairGroupDatas[GroupIt].CardsRootData.Num();
			WorkItemCount += CardsLODCount;
		}
	
		uint32 WorkItemIndex = 0;
		FScopedSlowTask SlowTask(WorkItemCount, LOCTEXT("BuildBindingData", "Building groom binding data"));
		SlowTask.MakeDialog();

		TArray<TArray<FVector3f>> TransferredPositions;
		if (bNeedTransferPosition)
		{
			bool bSucceed = GroomBinding_Transfer::Transfer( 
				SourceMeshData.Get(),
				TargetMeshData.Get(),
				TransferredPositions, BindingAsset->MatchingSection);

			if (!bSucceed)
			{
				return false;
			}

			SlowTask.EnterProgressFrame();
		}

		bool bSucceed = false;
		for (uint32 GroupIt=0; GroupIt < GroupCount; ++GroupIt)
		{
			FHairStrandsDatas StrandsData;
			FHairStrandsDatas GuidesData;
			GroomAsset->GetHairStrandsDatas(GroupIt, StrandsData, GuidesData);

			bSucceed = GroomBinding_RootProjection::Project(
				StrandsData,
				TargetMeshData.Get(),
				TransferredPositions,
				OutHairGroupDatas[GroupIt].RenRootData);
			if (!bSucceed)
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. Some strand roots are not close enough to the target mesh to be projected onto it."));
				return false;
			}

			SlowTask.EnterProgressFrame();

			bSucceed = GroomBinding_RootProjection::Project(
				GuidesData,
				TargetMeshData.Get(),
				TransferredPositions,
				OutHairGroupDatas[GroupIt].SimRootData);
			if (!bSucceed) 
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. Some guide roots are not close enough to the target mesh to be projected onto it."));
				return false; 
			}

			SlowTask.EnterProgressFrame();

			const uint32 CardsLODCount = OutHairGroupDatas[GroupIt].CardsRootData.Num();
			for (uint32 CardsLODIt = 0; CardsLODIt < CardsLODCount; ++CardsLODIt)
			{
				if (BindingAsset->Groom->HairGroupsData[GroupIt].Cards.IsValid(CardsLODIt))
				{
					FHairStrandsDatas LODGuidesData;
					const bool bIsValid = GroomAsset->GetHairCardsGuidesDatas(GroupIt, CardsLODIt, LODGuidesData);
					if (bIsValid)
					{
						bSucceed = GroomBinding_RootProjection::Project(
							LODGuidesData,
							TargetMeshData.Get(),
							TransferredPositions,
							OutHairGroupDatas[GroupIt].CardsRootData[CardsLODIt]);

						if (!bSucceed) 
						{
							UE_LOG(LogHairStrands, Error, TEXT("[Groom] Binding asset could not be built. Some cards guide roots are not close enough to the target mesh to be projected onto it."));
							return false; 
						}
					}
				}

				SlowTask.EnterProgressFrame();
			}
		}
		
		GroomBinding_RBFWeighting::ComputeInterpolationWeights(OutHairGroupDatas, BindingAsset->NumInterpolationPoints, BindingAsset->MatchingSection, TargetMeshData.Get(), TransferredPositions);
		SlowTask.EnterProgressFrame();
	}

	// 2. Release existing resources data
	UGroomBindingAsset::FHairGroupResources& OutHairGroupResources = BindingAsset->HairGroupResources;
	if (BindingAsset->HairGroupResources.Num() > 0)
	{
		for (UGroomBindingAsset::FHairGroupResource& GroupResrouces : OutHairGroupResources)
		{
			BindingAsset->HairGroupResourcesToDelete.Enqueue(GroupResrouces);
		}
		OutHairGroupResources.Empty();
	}
	check(OutHairGroupResources.Num() == 0);

	// 2. Convert data to bulk data
	GroomBinding_BulkCopy::BuildRootBulkData(BindingAsset->HairGroupBulkDatas, OutHairGroupDatas);

	// 3. Update GroomBindingAsset infos
	{
		TArray<FGoomBindingGroupInfo>& OutGroupInfos = BindingAsset->GroupInfos;
		OutGroupInfos.Empty();
		for (const FHairRootGroupData& Data : OutHairGroupDatas)
		{
			FGoomBindingGroupInfo& Info = OutGroupInfos.AddDefaulted_GetRef();
			Info.SimRootCount = Data.SimRootData.RootCount;
			Info.SimLODCount  = Data.SimRootData.MeshProjectionLODs.Num();
			Info.RenRootCount = Data.RenRootData.RootCount;
			Info.RenLODCount  = Data.RenRootData.MeshProjectionLODs.Num();
		}
	}

	BindingAsset->QueryStatus = UGroomBindingAsset::EQueryStatus::Completed;

	// 4. Optionnally update resources
	if (bInitResources)
	{
		BindingAsset->InitResource();
	}
#endif
	return true;
}

bool FGroomBindingBuilder::BuildBinding(UGroomBindingAsset* BindingAsset, bool bInitResources)
{
	return InternalBuildBinding_CPU(BindingAsset, bInitResources);
}

void FGroomBindingBuilder::GetRootData(
	FHairStrandsRootData& Out,
	const FHairStrandsRootBulkData& In)
{
	GroomBinding_BulkCopy::BuildRootData(Out, In);
}

#undef LOCTEXT_NAMESPACE
