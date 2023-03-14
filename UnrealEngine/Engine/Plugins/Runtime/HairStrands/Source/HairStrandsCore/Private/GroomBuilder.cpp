// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomBuilder.h"
#include "GroomAsset.h"
#include "GroomComponent.h"
#include "GroomSettings.h"
#include "HairDescription.h"
#include "GroomSettings.h"
#include "GroomBindingBuilder.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

#include "Async/ParallelFor.h"
#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/ScopedSlowTask.h"

DEFINE_LOG_CATEGORY_STATIC(LogGroomBuilder, Log, All);

#define LOCTEXT_NAMESPACE "GroomBuilder"

// For debug purpose
static float GHairInterpolationMetric_Distance = 1;
static float GHairInterpolationMetric_Angle = 0;
static float GHairInterpolationMetric_Length = 0;
static float GHairInterpolationMetric_AngleAttenuation = 5;
static FAutoConsoleVariableRef CVarHairInterpolationMetric_Distance(TEXT("r.HairStrands.InterpolationMetric.Distance"), GHairInterpolationMetric_Distance, TEXT("Hair strands interpolation metric weights for distance"));
static FAutoConsoleVariableRef CVarHairInterpolationMetric_Angle(TEXT("r.HairStrands.InterpolationMetric.Angle"), GHairInterpolationMetric_Angle, TEXT("Hair strands interpolation metric weights for angle"));
static FAutoConsoleVariableRef CVarHairInterpolationMetric_Length(TEXT("r.HairStrands.InterpolationMetric.Length"), GHairInterpolationMetric_Length, TEXT("Hair strands interpolation metric weights for length"));
static FAutoConsoleVariableRef CVarHairInterpolationMetric_AngleAttenuation(TEXT("r.HairStrands.InterpolationMetric.AngleAttenuation"), GHairInterpolationMetric_AngleAttenuation, TEXT("Hair strands interpolation angle attenuation"));

static int32 GHairClusterBuilder_MaxVoxelResolution = 256;
static FAutoConsoleVariableRef CVarHairClusterBuilder_MaxVoxelResolution(TEXT("r.HairStrands.ClusterBuilder.MaxVoxelResolution"), GHairClusterBuilder_MaxVoxelResolution, TEXT("Max voxel resolution used when building hair strands cluster data to avoid too long building time (default:128).  "));

static int32 GHairGroupIndexBuilder_MaxVoxelResolution = 64;
static FAutoConsoleVariableRef CVarHairGroupIndexBuilder_MaxVoxelResolution(TEXT("r.HairStrands.HairGroupBuilder.MaxVoxelResolution"), GHairGroupIndexBuilder_MaxVoxelResolution, TEXT("Max voxel resolution used when voxelizing hair strands to transfer group index grom strands to cards. This avoids too long building time (default:64).  "));

FString FGroomBuilder::GetVersion()
{
	// Important to update the version when groom building changes
	if (IsHairStrandContinuousDecimationReorderingEnabled())
		return TEXT("3r0");
	else
		return TEXT("2r2");
}

namespace FHairStrandsDecimation
{
	void Decimate(
		const FHairStrandsDatas& InData,
		float CurveDecimationPercentage,
		float VertexDecimationPercentage,
		bool bContinuousDecimationReordering,
		FHairStrandsDatas& OutData);

	void Decimate(
		const FHairStrandsDatas& InData, 
		const uint32 NumCurves,
		const int32 NumVertices,
		FHairStrandsDatas& OutData);
}

namespace HairStrandsBuilder
{
	FVector2D SignNotZero(const FVector2D& v)
	{
		return FVector2D((v.X >= 0.0) ? +1.0 : -1.0, (v.Y >= 0.0) ? +1.0 : -1.0);
	}

	// A Survey of Efficient Representations for Independent Unit Vectors
	// Reference: http://jcgt.org/published/0003/02/01/paper.pdf
	// Assume normalized input. Output is on [-1, 1] for each component.
	FVector2D SphericalToOctahedron(const FVector& v)
	{
		// Project the sphere onto the octahedron, and then onto the xy plane
		FVector2D p = FVector2D(v.X, v.Y) * (1.0 / (abs(v.X) + abs(v.Y) + abs(v.Z)));
		// Reflect the folds of the lower hemisphere over the diagonals
		return (v.Z <= 0.0) ? ((FVector2D(1, 1) - FVector2D(abs(p.Y), abs(p.X))) * SignNotZero(p)) : p;
	}

	// Auto-generate Root UV data if not loaded
	void ComputeRootUV(FHairStrandsCurves& Curves, FHairStrandsPoints& Points)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HairStrandsBuilder::ComputeRootUV);

		TArray<FVector> RootPoints;
		const uint32 CurveCount = Curves.Num();
		RootPoints.Reserve(Curves.Num());
		FVector MinAABB(MAX_FLT, MAX_FLT, MAX_FLT);
		FVector MaxAABB(-MAX_FLT, -MAX_FLT, -MAX_FLT);
		FMatrix Rotation = FRotationMatrix::Make(FRotator(0, 0, -90));
		for (uint32 CurveIndex=0; CurveIndex< CurveCount; ++CurveIndex)
		{
			const uint32 Offset = Curves.CurvesOffset[CurveIndex];
			check(Offset < uint32(Points.PointsPosition.Num()));
			const FVector P = Rotation.TransformPosition((FVector)Points.PointsPosition[Offset]);

			RootPoints.Add(P);
			MinAABB.X = FMath::Min(P.X, MinAABB.X);
			MinAABB.Y = FMath::Min(P.Y, MinAABB.Y);
			MinAABB.Z = FMath::Min(P.Z, MinAABB.Z);

			MaxAABB.X = FMath::Max(P.X, MaxAABB.X);
			MaxAABB.Y = FMath::Max(P.Y, MaxAABB.Y);
			MaxAABB.Z = FMath::Max(P.Z, MaxAABB.Z);
		}

		// Compute sphere bound
		const FVector Extent = MaxAABB - MinAABB;
		FSphere SBound;
		SBound.Center = (MaxAABB + MinAABB) * 0.5f;
		SBound.W = FMath::Max(Extent.X, FMath::Max(Extent.Y, Extent.Z));

		// Project root point onto the bounding sphere and map it onto 
		// an octahedron, which is unfold onto the unit space [0,1]^2
		TArray<FVector2D> RootUVs;
		RootUVs.Reserve(Curves.Num());
		FVector2D MinUV(MAX_FLT, MAX_FLT);
		FVector2D MaxUV(-MAX_FLT, -MAX_FLT);
		for (const FVector& RootP : RootPoints)
		{
			FVector D = RootP - SBound.Center;
			D.Normalize();
			FVector2D UV = SphericalToOctahedron(D);
			UV += FVector2D(1, 1);
			UV *= 0.5f;
			RootUVs.Add(UV);

			MinUV.X = FMath::Min(UV.X, MinUV.X);
			MinUV.Y = FMath::Min(UV.Y, MinUV.Y);

			MaxUV.X = FMath::Max(UV.X, MaxUV.X);
			MaxUV.Y = FMath::Max(UV.Y, MaxUV.Y);
		}

		// Find the minimal UV space cover by root point, and 
		// offsets/scales it to maximize UV space
		const FVector2f UVScale(1 / (MaxUV.X - MinUV.X), 1 / (MaxUV.Y - MinUV.Y));
		const FVector2f UVOffset(-MinUV.X, -MinUV.Y);
		uint32 Index = 0;
		for (FVector2f& RootUV : Curves.CurvesRootUV)
		{
			RootUV = (FVector2f(RootUVs[Index++]) + UVOffset) * UVScale;	// LWC_TODO: Precision loss
		}
	}

	/** Build the internal points and curves data */
	void BuildInternalData(FHairStrandsDatas& HairStrands, bool bComputeRootUV)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HairStrandsBuilder::BuildInternalData);

		FHairStrandsCurves& Curves = HairStrands.StrandsCurves;
		FHairStrandsPoints& Points = HairStrands.StrandsPoints;

		HairStrands.BoundingBox.Min = {  FLT_MAX,  FLT_MAX ,  FLT_MAX };
		HairStrands.BoundingBox.Max = { -FLT_MAX, -FLT_MAX , -FLT_MAX };

		if (HairStrands.GetNumCurves() > 0 && HairStrands.GetNumPoints() > 0)
		{
			TArray<FVector3f>::TIterator PositionIterator = Points.PointsPosition.CreateIterator();
			TArray<float>::TIterator RadiusIterator = Points.PointsRadius.CreateIterator();
			TArray<float>::TIterator CoordUIterator = Points.PointsCoordU.CreateIterator();

			TArray<uint16>::TIterator CountIterator = Curves.CurvesCount.CreateIterator();
			TArray<uint32>::TIterator OffsetIterator = Curves.CurvesOffset.CreateIterator();
			TArray<float>::TIterator LengthIterator = Curves.CurvesLength.CreateIterator();

			Curves.MaxRadius = 0.0;
			Curves.MaxLength = 0.0;

			uint32 StrandOffset = 0;
			*OffsetIterator = StrandOffset; ++OffsetIterator;

			for (uint32 CurveIndex = 0; CurveIndex < HairStrands.GetNumCurves(); ++CurveIndex, ++OffsetIterator, ++LengthIterator, ++CountIterator)
			{
				const uint16& StrandCount = *CountIterator;

				StrandOffset += StrandCount;
				*OffsetIterator = StrandOffset;

				float StrandLength = 0.0;
				FVector PreviousPosition(0.0, 0.0, 0.0);
				for (uint32 PointIndex = 0; PointIndex < StrandCount; ++PointIndex, ++PositionIterator, ++RadiusIterator, ++CoordUIterator)
				{
					HairStrands.BoundingBox += (FVector)*PositionIterator;

					if (PointIndex > 0)
					{
						StrandLength += ((FVector)*PositionIterator - (FVector)PreviousPosition).Size();
					}
					*CoordUIterator = StrandLength;
					PreviousPosition = (FVector)*PositionIterator;

					Curves.MaxRadius = FMath::Max(Curves.MaxRadius, *RadiusIterator);
				}
				*LengthIterator = StrandLength;
				Curves.MaxLength = FMath::Max(Curves.MaxLength, StrandLength);
			}

			CountIterator.Reset();
			LengthIterator.Reset();
			RadiusIterator.Reset();
			CoordUIterator.Reset();

			for (uint32 CurveIndex = 0; CurveIndex < HairStrands.GetNumCurves(); ++CurveIndex, ++LengthIterator, ++CountIterator)
			{
				const uint16& StrandCount = *CountIterator;

				for (uint32 PointIndex = 0; PointIndex < StrandCount; ++PointIndex, ++RadiusIterator, ++CoordUIterator)
				{
					if (*LengthIterator > 0.0f)
					{
						*CoordUIterator /= *LengthIterator;
					}
					if (Curves.MaxRadius > 0.0f)
					{
						*RadiusIterator /= Curves.MaxRadius;
					}
				}
				if (Curves.MaxLength > 0.0f)
				{
					*LengthIterator /= Curves.MaxLength;
				}
			}

			if (bComputeRootUV)
			{
				ComputeRootUV(Curves, Points);
			}
		}
	}

	inline void CopyVectorToPosition(const FVector& InVector, FHairStrandsPositionFormat::Type& OutPosition)
	{
		OutPosition.X = InVector.X;
		OutPosition.Y = InVector.Y;
		OutPosition.Z = InVector.Z;
	}

	template<typename TFormatType>
	void CopyToBulkData(FByteBulkData& Out, const TArray<typename TFormatType::Type>& Data)
	{
		static_assert(TFormatType::SizeInByte == sizeof(typename TFormatType::BulkType));
		const uint32 ElementSizeInByte = sizeof(typename TFormatType::BulkType);
		const uint32 DataSizeInByte = Data.Num() * ElementSizeInByte;

		// The buffer is then stored into bulk data
		Out.Lock(LOCK_READ_WRITE);
		void* BulkBuffer = Out.Realloc(DataSizeInByte);
		FMemory::Memcpy(BulkBuffer, Data.GetData(), DataSizeInByte);
		Out.Unlock();
	}

	/** Build the packed datas for gpu rendering/simulation */
	void BuildRenderData(const FHairStrandsDatas& HairStrands, const TArray<uint8>& RandomSeeds, FHairStrandsBulkData& OutBulkData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HairStrandsBuilder::BuildRenderData);

		const uint32 NumCurves = HairStrands.GetNumCurves();
		const uint32 NumPoints = HairStrands.GetNumPoints();
		if (!(NumCurves > 0 && NumPoints > 0))
			return;

		TArray<FHairStrandsPositionFormat::Type> OutPackedPositions;
		TArray<FHairStrandsAttribute0Format::Type> OutPackedAttributes0;
		TArray<FHairStrandsAttribute1Format::Type> OutPackedAttributes1;
		TArray<FHairStrandsMaterialFormat::Type> OutPackedMaterials;
		TArray<FHairStrandsRootIndexFormat::Type> OutCurveOffsets;

		OutPackedPositions.SetNum(NumPoints * FHairStrandsPositionFormat::ComponentCount);
		OutPackedAttributes0.SetNum(NumPoints * FHairStrandsAttribute0Format::ComponentCount);
		OutPackedAttributes1.SetNum(NumPoints * FHairStrandsAttribute1Format::ComponentCount);
		OutPackedMaterials.SetNum(NumPoints * FHairStrandsMaterialFormat::ComponentCount);
		OutCurveOffsets = HairStrands.StrandsCurves.CurvesOffset;

		const FVector HairBoxCenter = HairStrands.BoundingBox.GetCenter();

		const FHairStrandsCurves& Curves = HairStrands.StrandsCurves;
		const FHairStrandsPoints& Points = HairStrands.StrandsPoints;

		// Track what features/data the bulk data contains
		bool bHasMaterialData = false;

		struct FPackedRadiusAndType
		{
			union
			{
				struct
				{
					uint8 ControlPointType : 2;
					uint8 NormalizedRadius : 6;
				} Data;
				uint8 Packed;
			};
		};
		static_assert(sizeof(FPackedRadiusAndType) == sizeof(uint8));
		const bool bSeedValid = RandomSeeds.Num() > 0;
		for (uint32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
		{
			const uint8 CurveSeed = bSeedValid ? RandomSeeds[CurveIndex] : 0;
			const int32 IndexOffset = Curves.CurvesOffset[CurveIndex];
			const uint16& PointCount = Curves.CurvesCount[CurveIndex];
			for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				const uint32 PrevIndex = FMath::Max(0, PointIndex - 1);
				const uint32 NextIndex = FMath::Min(PointCount + 1, PointCount - 1);
				const FVector& PointPosition = (FVector)Points.PointsPosition[PointIndex + IndexOffset];

				const float CoordU = Points.PointsCoordU[PointIndex + IndexOffset];
				const float NormalizedRadius = Points.PointsRadius[PointIndex + IndexOffset];
				const float NormalizedLength = CoordU * Curves.CurvesLength[CurveIndex];

				FHairStrandsPositionFormat::Type& PackedPosition = OutPackedPositions[PointIndex + IndexOffset];
				CopyVectorToPosition(PointPosition - HairBoxCenter, PackedPosition);
				FPackedRadiusAndType PackedRadiusAndType;
				PackedRadiusAndType.Data.ControlPointType = (PointIndex == 0) ? 1u : (PointIndex == (PointCount - 1) ? 2u : 0u);
				PackedRadiusAndType.Data.NormalizedRadius = uint8(FMath::Clamp(NormalizedRadius * 63.f, 0.f, 63.f));
				PackedPosition.PackedRadiusAndType = PackedRadiusAndType.Packed;
				PackedPosition.UCoord = uint8(FMath::Clamp(CoordU * 255.f, 0.f, 255.f));

				FVector2D RootUV = FVector2D(Curves.CurvesRootUV[CurveIndex]); 
				FHairStrandsAttribute0Format::Type& PackedAttributes0 = OutPackedAttributes0[PointIndex + IndexOffset];
				PackedAttributes0.NormalizedLength = uint8(FMath::Clamp(NormalizedLength * 255.f, 0.f, 255.f));
				PackedAttributes0.Seed = CurveSeed;

				// Root UV support UDIM texture coordinate but limit the spans of the UDIM to be in 256x256 instead of 9999x9999.
				// The internal UV coords are also limited to 8bits, which means if sampling need to be super precise, this is no enough.
				// Specal case for UV == 1.0f, as we don't need UDIM data in this case, so force the value to be in [0..1)
				const float SmallEpsilon = 1e-05f;
				RootUV.X = RootUV.X == 1.0f ? RootUV.X - SmallEpsilon : RootUV.X;
				RootUV.Y = RootUV.Y == 1.0f ? RootUV.Y - SmallEpsilon : RootUV.Y;
				const FVector2D TextureRootUV(FMath::Fractional(RootUV.X), FMath::Fractional(RootUV.Y));
				const FVector2D TextureIndexUV = RootUV - TextureRootUV;

				// UDIM
				FHairStrandsAttribute1Format::Type& PackedAttributes1 = OutPackedAttributes1[PointIndex + IndexOffset];
				PackedAttributes1.Packed = 0;
				PackedAttributes1.Packed |= (uint32(FMath::Clamp(TextureRootUV.X * 2047.f, 0.f, 2047.f)) & 0x7FFu);
				PackedAttributes1.Packed |= (uint32(FMath::Clamp(TextureRootUV.Y * 2047.f, 0.f, 2047.f)) & 0x7FFu) << 11u;
				PackedAttributes1.Packed |= (uint32(FMath::Clamp(TextureIndexUV.X, 0.f, 31.f)) & 0x1F) << 22u;
				PackedAttributes1.Packed |= (uint32(FMath::Clamp(TextureIndexUV.Y, 0.f, 31.f)) & 0x1F) << 27u;

				// Material
				{
					FHairStrandsMaterialFormat::Type& Material = OutPackedMaterials[PointIndex + IndexOffset];
					// Cheap sRGB encoding instead of PointsBaseColor.ToFColor(true), as this makes the decompression 
					// cheaper on GPU (since R8G8B8A8 sRGB format used/exposed not exposed)
					Material.BaseColorR = FMath::Clamp(uint32(FMath::Sqrt(Points.PointsBaseColor[PointIndex + IndexOffset].R) * 0xFF), 0u, 0xFFu);
					Material.BaseColorG = FMath::Clamp(uint32(FMath::Sqrt(Points.PointsBaseColor[PointIndex + IndexOffset].G) * 0xFF), 0u, 0xFFu);
					Material.BaseColorB = FMath::Clamp(uint32(FMath::Sqrt(Points.PointsBaseColor[PointIndex + IndexOffset].B) * 0xFF), 0u, 0xFFu);
					Material.Roughness  = FMath::Clamp(uint32(Points.PointsRoughness[PointIndex + IndexOffset] * 0xFF), 0u, 0xFFu);

					if (Material.BaseColorR != 0 || Material.BaseColorG != 0 || Material.BaseColorB != 0 || Material.Roughness != 0)
					{
						bHasMaterialData = true;
					}
				}
			}
		}

		OutBulkData.BoundingBox = HairStrands.BoundingBox;
		OutBulkData.CurveCount = HairStrands.GetNumCurves();
		OutBulkData.PointCount = HairStrands.GetNumPoints();
		OutBulkData.MaxLength = HairStrands.StrandsCurves.MaxLength;
		OutBulkData.MaxRadius = HairStrands.StrandsCurves.MaxRadius;
		OutBulkData.Flags = FHairStrandsBulkData::DataFlags_HasData;

		if (bHasMaterialData)	{ OutBulkData.Flags |= FHairStrandsBulkData::DataFlags_HasMaterialData; }	else { OutPackedMaterials.Empty(); }

		CopyToBulkData<FHairStrandsPositionFormat>(OutBulkData.Positions, OutPackedPositions);
		CopyToBulkData<FHairStrandsAttribute0Format>(OutBulkData.Attributes0, OutPackedAttributes0);
		CopyToBulkData<FHairStrandsAttribute1Format>(OutBulkData.Attributes1, OutPackedAttributes1);

		if (bHasMaterialData)
		{
			CopyToBulkData<FHairStrandsMaterialFormat>(OutBulkData.Materials, OutPackedMaterials);
		}
		CopyToBulkData<FHairStrandsRootIndexFormat>(OutBulkData.CurveOffsets, OutCurveOffsets);
	}

	void BuildRenderData(const FHairStrandsDatas& HairStrands, FHairStrandsBulkData& OutBulkData)
	{
		TArray<uint8> RandomSeeds;
		BuildRenderData(HairStrands, RandomSeeds, OutBulkData);
	}

} // namespace HairStrandsBuilder

namespace HairInterpolationBuilder
{
	struct FHairRoot
	{
		FVector Position;
		uint32  VertexCount;
		FVector Normal;
		uint32  Index;
		float   Length;
	};

	struct FHairInterpolationMetric
	{	
		// Total/combined metrics
		float Metric;

		// Debug info
		float DistanceMetric;
		float AngularMetric;
		float LengthMetric;

		float CosAngle;
		float Distance;
		float GuideLength;
		float RenderLength;

	};

	inline FHairInterpolationMetric ComputeInterpolationMetric(const FHairRoot& RenderRoot, const FHairRoot& GuideRoot)
	{
		FHairInterpolationMetric Out;
		Out.Distance = FVector::Dist(RenderRoot.Position, GuideRoot.Position);
		Out.CosAngle = FVector::DotProduct(RenderRoot.Normal, GuideRoot.Normal);
		Out.GuideLength = GuideRoot.Length;
		Out.RenderLength = RenderRoot.Length;

		// Metric takes into account the following properties to find guides which are close, share similar orientation, and 
		// have similar length for better interpolation
		// * distance
		// * orientation 
		// * length 
		const float AngularAttenuation = GHairInterpolationMetric_AngleAttenuation > 1 ? GHairInterpolationMetric_AngleAttenuation : 0;
		Out.DistanceMetric	= Out.Distance * GHairInterpolationMetric_Distance;
		Out.AngularMetric	= AngularAttenuation == 0 ? 0 : (FMath::Clamp((1 - FMath::Pow(Out.CosAngle, AngularAttenuation)), 0.f, 1.f) * GHairInterpolationMetric_Angle);
		Out.LengthMetric	= FMath::Abs(FMath::Max(Out.GuideLength / float(Out.RenderLength), Out.RenderLength / float(Out.GuideLength)) - 1) * GHairInterpolationMetric_Length; // Ratio
		Out.Metric			= Out.DistanceMetric + Out.AngularMetric + Out.LengthMetric;

		return Out;
	}

	template<uint32 NumSamples>
	inline FVector GetCurvePosition(const FHairStrandsDatas& CurvesDatas, const uint32 CurveIndex, const uint32 SampleIndex)
	{
		const float PointCount = CurvesDatas.StrandsCurves.CurvesCount[CurveIndex]-1.0;
		const uint32 PointOffset = CurvesDatas.StrandsCurves.CurvesOffset[CurveIndex];

		const float CurvePoint = static_cast<float>(SampleIndex) * PointCount / (static_cast<float>(NumSamples)-1.0f);
		const uint32 PointPrev = (SampleIndex == 0) ? 0 : (SampleIndex == (NumSamples-1)) ? PointCount - 1 : floor(CurvePoint);
		const uint32 PointNext = PointPrev + 1;

		const float PointAlpha = CurvePoint - static_cast<float>(PointPrev);
		return FVector(CurvesDatas.StrandsPoints.PointsPosition[PointOffset+PointPrev]*(1.0f-PointAlpha) +
			CurvesDatas.StrandsPoints.PointsPosition[PointOffset+PointNext]*PointAlpha);
	}

	template<uint32 NumSamples>
	inline float ComputeCurvesMetric(const FHairStrandsDatas& RenderCurvesDatas, const uint32 RenderCurveIndex, 
		const FHairStrandsDatas& GuideCurvesDatas, const uint32 GuideCurveIndex, const float RootImportance, 
		const float ShapeImportance, const float ProximityImportance)
	{
		const float AverageLength = FMath::Max(0.5f * (RenderCurvesDatas.StrandsCurves.CurvesLength[RenderCurveIndex] * RenderCurvesDatas.StrandsCurves.MaxLength +
			GuideCurvesDatas.StrandsCurves.CurvesLength[GuideCurveIndex] * GuideCurvesDatas.StrandsCurves.MaxLength), SMALL_NUMBER);

		static const float DeltaCoord = 1.0f / static_cast<float>(NumSamples-1);

		const FVector& RenderRoot = (FVector)RenderCurvesDatas.StrandsPoints.PointsPosition[RenderCurvesDatas.StrandsCurves.CurvesOffset[RenderCurveIndex]];
		const FVector& GuideRoot = (FVector)GuideCurvesDatas.StrandsPoints.PointsPosition[GuideCurvesDatas.StrandsCurves.CurvesOffset[GuideCurveIndex]];

		float CurveProximityMetric = 0.0;
		float CurveShapeMetric = 0.0;
		for (uint32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			const FVector GuidePosition = GetCurvePosition<NumSamples>(GuideCurvesDatas, GuideCurveIndex, SampleIndex);
			const FVector RenderPosition = GetCurvePosition<NumSamples>(RenderCurvesDatas, RenderCurveIndex, SampleIndex);
			const float RootWeight = FMath::Exp(-RootImportance*SampleIndex*DeltaCoord);

			CurveProximityMetric += (GuidePosition - RenderPosition).Size() * RootWeight;
			CurveShapeMetric += (GuidePosition - GuideRoot - RenderPosition + RenderRoot).Size() * RootWeight;
		}
		CurveShapeMetric *= DeltaCoord / AverageLength;
		CurveProximityMetric *= DeltaCoord / AverageLength;

		return FMath::Exp(-ShapeImportance*CurveShapeMetric) * FMath::Exp(-ProximityImportance * CurveProximityMetric);
	}

	inline void PrintInterpolationMetric(const FHairInterpolationMetric& In)
	{
		UE_LOG(LogGroomBuilder, Log, TEXT("------------------------------------------------------------------------------------------"));
		UE_LOG(LogGroomBuilder, Log, TEXT("Total Metric = %f"), In.Metric);
		UE_LOG(LogGroomBuilder, Log, TEXT("Distance     = %f (%f)"), In.Distance, In.DistanceMetric);
		UE_LOG(LogGroomBuilder, Log, TEXT("Angle        = %f (%f)"), FMath::RadiansToDegrees(FMath::Acos(In.CosAngle)), In.AngularMetric);
		UE_LOG(LogGroomBuilder, Log, TEXT("Length       = %f/%f (%f)"), In.RenderLength, In.GuideLength, In.LengthMetric);
	}

	template<typename T>
	void SwapValue(T& A, T& B)
	{
		T Temp = A;
		A = B;
		B = Temp;
	}

	struct FMetrics
	{
		static const uint32 Count = 3;
		float KMinMetrics[Count];
		int32 KClosestGuideIndices[Count];
	};

	struct FClosestGuides
	{
		static const uint32 Count = 3;
		int32 Indices[Count];
	};

	// Randomize influence guide to break interpolation coherence, and create a more random/natural pattern
	static void SelectFinalGuides(
		FClosestGuides& ClosestGuides, 
		const FIntVector& RandomIndices,
		const FMetrics& InMetric, 
		const bool bRandomizeInterpolation, 
		const bool bUseUniqueGuide)
	{
		FMetrics Metric = InMetric;
		check(Metric.KClosestGuideIndices[0] >= 0);

		// If some indices are invalid (for instance, found a valid single guide, fill in the rest with the valid ones)
		if (Metric.KClosestGuideIndices[1] < 0)
		{
			Metric.KClosestGuideIndices[1] = Metric.KClosestGuideIndices[0];
			Metric.KMinMetrics[1] = Metric.KMinMetrics[0];
		}
		if (Metric.KClosestGuideIndices[2] < 0)
		{
			Metric.KClosestGuideIndices[2] = Metric.KClosestGuideIndices[1];
			Metric.KMinMetrics[2] = Metric.KMinMetrics[1];
		}

		uint32 RandIndex0 = 0;
		uint32 RandIndex1 = 1;
		uint32 RandIndex2 = 2;
		if (bRandomizeInterpolation)
		{
			// This randomization makes certain strands being affected by 1, 2, or 3 guides
			RandIndex0 = RandomIndices[0];
			RandIndex1 = RandomIndices[1];
			RandIndex2 = RandomIndices[2];
		}

		ClosestGuides.Indices[0] = Metric.KClosestGuideIndices[RandIndex0];
		ClosestGuides.Indices[1] = Metric.KClosestGuideIndices[RandIndex1];
		ClosestGuides.Indices[2] = Metric.KClosestGuideIndices[RandIndex2];

		if (bUseUniqueGuide)
		{
			ClosestGuides.Indices[1] = Metric.KClosestGuideIndices[RandIndex0];
			ClosestGuides.Indices[2] = Metric.KClosestGuideIndices[RandIndex0];
			RandIndex1 = RandIndex0;
			RandIndex2 = RandIndex0;
		}


		float MinMetrics[FMetrics::Count];
		MinMetrics[0] = Metric.KMinMetrics[RandIndex0];
		MinMetrics[1] = Metric.KMinMetrics[RandIndex1];
		MinMetrics[2] = Metric.KMinMetrics[RandIndex2];


		while (!(MinMetrics[0] <= MinMetrics[1] && MinMetrics[1] <= MinMetrics[2]))
		{
			if (MinMetrics[0] > MinMetrics[1])
			{
				SwapValue(MinMetrics[0], MinMetrics[1]);
				SwapValue(ClosestGuides.Indices[0], ClosestGuides.Indices[1]);
			}

			if (MinMetrics[1] > MinMetrics[2])
			{
				SwapValue(MinMetrics[1], MinMetrics[2]);
				SwapValue(ClosestGuides.Indices[1], ClosestGuides.Indices[2]);
			}
		}

		// If there less than 3 valid guides, fill the rest with existing valid guides
		// This can happen due to the normal-orientation based rejection above
		if (ClosestGuides.Indices[1] < 0)
		{
			ClosestGuides.Indices[1] = ClosestGuides.Indices[0];
			MinMetrics[1] = MinMetrics[0];
		}
		if (ClosestGuides.Indices[2] < 0)
		{
			ClosestGuides.Indices[2] = ClosestGuides.Indices[1];
			MinMetrics[2] = MinMetrics[1];
		}

		check(MinMetrics[0] <= MinMetrics[1]);
		check(MinMetrics[1] <= MinMetrics[2]);
	}

	// Simple closest distance metric
	static void ComputeSimpleMetric(
		FMetrics& Metrics1, 
		const FHairRoot& RenRoot, 
		const FHairRoot& GuideRoot, 
		const int32 RenCurveIndex,
		const int32 SimCurveIndex)
	{
		const float Metric = FVector::Dist(GuideRoot.Position, RenRoot.Position);
		if (Metric < Metrics1.KMinMetrics[FMetrics::Count - 1])
		{
			int32 LastGuideIndex = SimCurveIndex;
			float LastMetric = Metric;
			for (uint32 Index = 0; Index < FMetrics::Count; ++Index)
			{
				if (Metric < Metrics1.KMinMetrics[Index])
				{
					SwapValue(Metrics1.KClosestGuideIndices[Index], LastGuideIndex);
					SwapValue(Metrics1.KMinMetrics[Index], LastMetric);
				}
			}
		}
	}

	// Complex pairing metric
	static void ComputeAdvandedMetric(FMetrics& Metrics0,
		const FHairStrandsDatas& RenStrandsData,
		const FHairStrandsDatas& SimStrandsData,
		const int32 RenCurveIndex,
		const int32 SimCurveIndex)
	{
		const float Metric = 1.0 - ComputeCurvesMetric<16>(RenStrandsData, RenCurveIndex, SimStrandsData, SimCurveIndex, 0.0f, 1.0f, 1.0f);
		if (Metric < Metrics0.KMinMetrics[FMetrics::Count - 1])
		{
			int32 LastGuideIndex = SimCurveIndex;
			float LastMetric = Metric;
			for (uint32 Index = 0; Index < FMetrics::Count; ++Index)
			{
				if (Metric < Metrics0.KMinMetrics[Index])
				{
					SwapValue(Metrics0.KClosestGuideIndices[Index], LastGuideIndex);
					SwapValue(Metrics0.KMinMetrics[Index], LastMetric);
				}
			}
		}
	}

	struct FRootsGrid
	{
		FVector MinBound = FVector::ZeroVector;
		FVector MaxBound = FVector::ZeroVector;
		
		const uint32 MaxLookupDistance = 31;
		const FIntVector GridResolution = FIntVector(32, 32, 32);

		TArray<int32> GridIndirection;
		TArray<TArray<int32>> RootIndices;
		
		FORCEINLINE bool IsValid(const FIntVector& P) const
		{
			return	0 <= P.X && P.X < GridResolution.X &&
					0 <= P.Y && P.Y < GridResolution.Y &&
					0 <= P.Z && P.Z < GridResolution.Z;
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
			check(CellIndex < uint32(GridIndirection.Num()));
			return CellIndex;
		}

		void InsertRoots(TArray<FHairRoot>& Roots, const FVector& InMinBound, const FVector& InMaxBound)
		{
			MinBound = InMinBound;
			MaxBound = InMaxBound;
			GridIndirection.SetNumZeroed(GridResolution.X*GridResolution.Y*GridResolution.Z);
			RootIndices.Empty();
			RootIndices.AddDefaulted(); // Add a default empty list for the null element

			const uint32 RootCount = Roots.Num();
			for (uint32 RootIt = 0; RootIt < RootCount; ++RootIt)
			{
				const FHairRoot& Root = Roots[RootIt];
				const FIntVector CellCoord = ToCellCoord(Root.Position);
				const uint32 Index = ToIndex(CellCoord);
				if (GridIndirection[Index] == 0)
				{
					GridIndirection[Index] = RootIndices.Num();
					RootIndices.AddDefaulted();
				}
				
				TArray<int32>& CellGuideIndices = RootIndices[GridIndirection[Index]];
				CellGuideIndices.Add(RootIt);
			}
		}

		FORCEINLINE void SearchCell(
			const FIntVector& CellCoord,
			const uint32 RenCurveIndex,
			const FHairRoot& RenRoot,
			const TArray<FHairRoot>& SimRoots,
			FMetrics& Metrics) const 
		{
			const uint32 CellIndex = ToIndex(CellCoord);

			if (GridIndirection[CellIndex] == 0)
				return;

			const TArray<int32>& Elements = RootIndices[GridIndirection[CellIndex]];

			for (int32 SimCurveIndex : Elements)
			{
				const FHairRoot& GuideRoot = SimRoots[SimCurveIndex];
				{
					ComputeSimpleMetric(Metrics, RenRoot, GuideRoot, RenCurveIndex, SimCurveIndex);
				}
			}
		}

		FClosestGuides FindClosestRoots(
			const uint32 RenCurveIndex,
			const TArray<FHairRoot>& RenRoots,
			const TArray<FHairRoot>& SimRoots,
			const FHairStrandsDatas& RenStrandsData,
			const FHairStrandsDatas& SimStrandsData,
			const bool bRandomized,
			const bool bUnique,
			const FIntVector& RandomIndices) const
		{
			const FHairRoot& RenRoot = RenRoots[RenCurveIndex];
			const FIntVector PointCoord = ToCellCoord(RenRoot.Position);

			FMetrics Metrics;
			for (uint32 ClearIt = 0; ClearIt < FMetrics::Count; ++ClearIt)
			{
				Metrics.KMinMetrics[ClearIt] = FLT_MAX;
				Metrics.KClosestGuideIndices[ClearIt] = -1;
			}

			for (int32 Offset = 1; Offset <= int32(MaxLookupDistance); ++Offset)
			{
				// Center
				{
					bool bIsValid = false;
					const FIntVector CellCoord = ClampToVolume(PointCoord, bIsValid);
					if (bIsValid) SearchCell(PointCoord, RenCurveIndex, RenRoot, SimRoots, Metrics);
				}

				// Top & Bottom
				for (int32 X = -Offset; X <= Offset; ++X)
				for (int32 Y = -Offset; Y <= Offset; ++Y)
				{
					bool bIsValid0 = false, bIsValid1 = false;
					const FIntVector CellCoord0 = ClampToVolume(PointCoord + FIntVector(X, Y, Offset), bIsValid0);
					const FIntVector CellCoord1 = ClampToVolume(PointCoord + FIntVector(X, Y,-Offset), bIsValid1);
					if (bIsValid0) SearchCell(CellCoord0, RenCurveIndex, RenRoot, SimRoots, Metrics);
					if (bIsValid1) SearchCell(CellCoord1, RenCurveIndex, RenRoot, SimRoots, Metrics);
				}

				const int32 OffsetMinusOne = Offset - 1;
				// Front & Back
				for (int32 X = -Offset; X <= Offset; ++X)
				for (int32 Z = -OffsetMinusOne; Z <= OffsetMinusOne; ++Z)
				{
					bool bIsValid0 = false, bIsValid1 = false;
					const FIntVector CellCoord0 = ClampToVolume(PointCoord + FIntVector(X,  Offset, Z), bIsValid0);
					const FIntVector CellCoord1 = ClampToVolume(PointCoord + FIntVector(X, -Offset, Z), bIsValid1);
					if (bIsValid0) SearchCell(CellCoord0, RenCurveIndex, RenRoot, SimRoots, Metrics);
					if (bIsValid1) SearchCell(CellCoord1, RenCurveIndex, RenRoot, SimRoots, Metrics);
				}
				
				// Left & Right
				for (int32 Y = -OffsetMinusOne; Y <= OffsetMinusOne; ++Y)
				for (int32 Z = -OffsetMinusOne; Z <= OffsetMinusOne; ++Z)
				{
					bool bIsValid0 = false, bIsValid1 = false;
					const FIntVector CellCoord0 = ClampToVolume(PointCoord + FIntVector( Offset, Y, Z), bIsValid0);
					const FIntVector CellCoord1 = ClampToVolume(PointCoord + FIntVector(-Offset, Y, Z), bIsValid1);
					if (bIsValid0) SearchCell(CellCoord0, RenCurveIndex, RenRoot, SimRoots, Metrics);
					if (bIsValid1) SearchCell(CellCoord1, RenCurveIndex, RenRoot, SimRoots, Metrics);
				}

				// Early out if we have found all closest guide during a ring/layer step.
				// This early out is not conservative, as the complex metric might find better guides one or multiple step further.
				if (Metrics.KClosestGuideIndices[FMetrics::Count-1] != -1 && Offset >= 2)
				{
					break;
				}
			}

			// If no valid guide have been found, switch to a simpler metric
			FClosestGuides ClosestGuides;
			SelectFinalGuides(ClosestGuides, RandomIndices, Metrics, bRandomized, bUnique);

			check(ClosestGuides.Indices[0] >= 0);
			check(ClosestGuides.Indices[1] >= 0);
			check(ClosestGuides.Indices[2] >= 0);

			return ClosestGuides;
		}


		FORCEINLINE void SearchCell(
			const FIntVector& CellCoord,
			const uint32 RenCurveIndex,
			const FHairRoot& RenRoot,
			const TArray<FHairRoot>& SimRoots,
			const FHairStrandsDatas& RenStrandsData,
			const FHairStrandsDatas& SimStrandsData,
			FMetrics& Metrics0,
			FMetrics& Metrics1) const
		{
			const uint32 CellIndex = ToIndex(CellCoord);

			if (GridIndirection[CellIndex] == 0)
				return;

			const TArray<int32>& Elements = RootIndices[GridIndirection[CellIndex]];

			for (int32 SimCurveIndex : Elements)
			{
				const FHairRoot& GuideRoot = SimRoots[SimCurveIndex];
				{
					ComputeSimpleMetric(Metrics1, RenRoot, GuideRoot, RenCurveIndex, SimCurveIndex);
					ComputeAdvandedMetric(Metrics0, RenStrandsData, SimStrandsData, RenCurveIndex, SimCurveIndex);
				}
			}
		}

		FClosestGuides FindBestClosestRoots(
			const uint32 RenCurveIndex,
			const TArray<FHairRoot>& RenRoots,
			const TArray<FHairRoot>& SimRoots,
			const FHairStrandsDatas& RenStrandsData,
			const FHairStrandsDatas& SimStrandsData,
			const bool bRandomized,
			const bool bUnique,
			const FIntVector& RandomIndices) const
		{
			const FHairRoot& RenRoot = RenRoots[RenCurveIndex];
			const FIntVector PointCoord = ToCellCoord(RenRoot.Position);

			FMetrics Metrics0;
			FMetrics Metrics1;
			for (uint32 ClearIt = 0; ClearIt < FMetrics::Count; ++ClearIt)
			{
				Metrics0.KMinMetrics[ClearIt] = FLT_MAX;
				Metrics0.KClosestGuideIndices[ClearIt] = -1;
				Metrics1.KMinMetrics[ClearIt] = FLT_MAX;
				Metrics1.KClosestGuideIndices[ClearIt] = -1;
			}

			for (int32 Offset = 1; Offset <= int32(MaxLookupDistance); ++Offset)
			{
				// Center
				{
					bool bIsValid = false;
					const FIntVector CellCoord = ClampToVolume(PointCoord, bIsValid);
					if (bIsValid) SearchCell(CellCoord, RenCurveIndex, RenRoot, SimRoots, RenStrandsData, SimStrandsData, Metrics0, Metrics1);
				}

				// Top & Bottom
				for (int32 X = -Offset; X <= Offset; ++X)
				for (int32 Y = -Offset; Y <= Offset; ++Y)
				{
					bool bIsValid0 = false, bIsValid1 = false;
					const FIntVector CellCoord0 = ClampToVolume(PointCoord + FIntVector(X, Y, Offset), bIsValid0);
					const FIntVector CellCoord1 = ClampToVolume(PointCoord + FIntVector(X, Y,-Offset), bIsValid1);
					if (bIsValid0) SearchCell(CellCoord0, RenCurveIndex, RenRoot, SimRoots, RenStrandsData, SimStrandsData, Metrics0, Metrics1);
					if (bIsValid1) SearchCell(CellCoord1, RenCurveIndex, RenRoot, SimRoots, RenStrandsData, SimStrandsData, Metrics0, Metrics1);
				}

				const int32 OffsetMinusOne = Offset - 1;
				// Front & Back
				for (int32 X = -Offset; X <= Offset; ++X)
				for (int32 Z = -OffsetMinusOne; Z <= OffsetMinusOne; ++Z)
				{
					bool bIsValid0 = false, bIsValid1 = false;
					const FIntVector CellCoord0 = ClampToVolume(PointCoord + FIntVector(X, Offset, Z), bIsValid0);
					const FIntVector CellCoord1 = ClampToVolume(PointCoord + FIntVector(X, -Offset, Z), bIsValid1);
					if (bIsValid0) SearchCell(CellCoord0, RenCurveIndex, RenRoot, SimRoots, RenStrandsData, SimStrandsData, Metrics0, Metrics1);
					if (bIsValid1) SearchCell(CellCoord1, RenCurveIndex, RenRoot, SimRoots, RenStrandsData, SimStrandsData, Metrics0, Metrics1);
				}
				
				// Left & Right
				for (int32 Y = -OffsetMinusOne; Y <= OffsetMinusOne; ++Y)
				for (int32 Z = -OffsetMinusOne; Z <= OffsetMinusOne; ++Z)
				{
					bool bIsValid0 = false, bIsValid1 = false;
					const FIntVector CellCoord0 = ClampToVolume(PointCoord + FIntVector( Offset, Y, Z), bIsValid0);
					const FIntVector CellCoord1 = ClampToVolume(PointCoord + FIntVector(-Offset, Y, Z), bIsValid1);
					if (bIsValid0) SearchCell(CellCoord0, RenCurveIndex, RenRoot, SimRoots, RenStrandsData, SimStrandsData, Metrics0, Metrics1);
					if (bIsValid1) SearchCell(CellCoord1, RenCurveIndex, RenRoot, SimRoots, RenStrandsData, SimStrandsData, Metrics0, Metrics1);
				}

				// Early out if we have found all closest guide during a ring/layer step.
				// This early out is not conservative, as the complex metric might find better guides one or multiple step further.
				if ((Metrics0.KClosestGuideIndices[FMetrics::Count-1] != -1 || Metrics1.KClosestGuideIndices[FMetrics::Count - 1] != -1) && Offset >= 2)
				{
					break;
				}
			}

			// If no valid guide have been found, switch to a simpler metric
			FClosestGuides ClosestGuides;
			if (Metrics0.KClosestGuideIndices[0] != -1)
			{
				SelectFinalGuides(ClosestGuides, RandomIndices, Metrics0, bRandomized, bUnique);
			}
			else
			{
				SelectFinalGuides(ClosestGuides, RandomIndices, Metrics1, bRandomized, bUnique);
			}

			check(ClosestGuides.Indices[0] >= 0);
			check(ClosestGuides.Indices[1] >= 0);
			check(ClosestGuides.Indices[2] >= 0);

			return ClosestGuides;
		}
	};

	static FClosestGuides FindBestRoots(
		const uint32 RenCurveIndex,
		const TArray<FHairRoot>& RenRoots,
		const TArray<FHairRoot>& SimRoots,
		const FHairStrandsDatas& RenStrandsData,
		const FHairStrandsDatas& SimStrandsData,
		const bool bRandomized,
		const bool bUnique,
		const FIntVector& RandomIndices)
	{
		FMetrics Metrics;
		for (uint32 ClearIt = 0; ClearIt < FMetrics::Count; ++ClearIt)
		{
			Metrics.KMinMetrics[ClearIt] = FLT_MAX;
			Metrics.KClosestGuideIndices[ClearIt] = -1;
		}

		const uint32 SimRootsCount = SimRoots.Num();
		for (uint32 SimCurveIndex =0; SimCurveIndex<SimRootsCount; ++SimCurveIndex)
		{
			ComputeAdvandedMetric(Metrics, RenStrandsData, SimStrandsData, RenCurveIndex, SimCurveIndex);
		}
			
		FClosestGuides ClosestGuides;
		SelectFinalGuides(ClosestGuides, RandomIndices, Metrics, bRandomized, bUnique);

		check(ClosestGuides.Indices[0] >= 0);
		check(ClosestGuides.Indices[1] >= 0);
		check(ClosestGuides.Indices[2] >= 0);

		return ClosestGuides;
	}

	// Extract strand roots
	static void ExtractRoots(const FHairStrandsDatas& InData, TArray<FHairRoot>& OutRoots, FVector& MinBound, FVector& MaxBound)
	{
		MinBound = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
		MaxBound = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		const uint32 CurveCount = InData.StrandsCurves.Num();
		OutRoots.Reserve(CurveCount);
		for (uint32 CurveIndex = 0; CurveIndex < CurveCount; ++CurveIndex)
		{
			const uint32 PointOffset = InData.StrandsCurves.CurvesOffset[CurveIndex];
			const uint32 PointCount = InData.StrandsCurves.CurvesCount[CurveIndex];
			const float  CurveLength = InData.StrandsCurves.CurvesLength[CurveIndex] * InData.StrandsCurves.MaxLength;
			check(PointCount > 1);
			const FVector& P0 = (FVector)InData.StrandsPoints.PointsPosition[PointOffset];
			const FVector& P1 = (FVector)InData.StrandsPoints.PointsPosition[PointOffset + 1];
			FVector N = (P1 - P0).GetSafeNormal();

			// Fallback in case the initial points are too close (this happens on certain assets)
			if (FVector::DotProduct(N, N) == 0)
			{
				N = FVector(0, 0, 1);
			}
			OutRoots.Add({ P0, PointCount, N, PointOffset, CurveLength });

			MinBound = MinBound.ComponentMin(P0);
			MaxBound = MaxBound.ComponentMax(P0);
		}
	}

	struct FVertexInterpolationDesc
	{
		uint32 Index0 = 0;
		uint32 Index1 = 0;
		float T = 0;
	};

	// Find the vertex along a sim curve 'SimCurveIndex', which has the same parametric distance than the render distance 'RenPointDistance'
	static FVertexInterpolationDesc FindMatchingVertex(const float RenPointDistance, const FHairStrandsDatas& SimStrandsData, const uint32 SimCurveIndex)
	{
		const uint32 SimOffset = SimStrandsData.StrandsCurves.CurvesOffset[SimCurveIndex];

		const float CurveLength = SimStrandsData.StrandsCurves.CurvesLength[SimCurveIndex] * SimStrandsData.StrandsCurves.MaxLength;

		// Find with with vertex the vertex should be paired
		const uint32 SimPointCount = SimStrandsData.StrandsCurves.CurvesCount[SimCurveIndex];
		for (uint32 SimPointIndex = 0; SimPointIndex < SimPointCount-1; ++SimPointIndex)
		{
			const float SimPointDistance0 = SimStrandsData.StrandsPoints.PointsCoordU[SimPointIndex + SimOffset] * CurveLength;
			const float SimPointDistance1 = SimStrandsData.StrandsPoints.PointsCoordU[SimPointIndex + SimOffset + 1] * CurveLength;
			if (SimPointDistance0 <= RenPointDistance && RenPointDistance <= SimPointDistance1)
			{
				const float SegmentLength = SimPointDistance1 - SimPointDistance0;
				FVertexInterpolationDesc Out;
				Out.Index0 = SimPointIndex;
				Out.Index1 = SimPointIndex+1;
				Out.T = (RenPointDistance - SimPointDistance0) / (SegmentLength>0? SegmentLength : 1);
				Out.T = FMath::Clamp(Out.T, 0.f, 1.f);
				return Out;
			}
		}
		FVertexInterpolationDesc Desc;
		Desc.Index0 = SimPointCount-2;
		Desc.Index1 = SimPointCount-1;
		Desc.T = 1;
		return Desc;
	}

	// Find the vertex along a sim curve 'SimCurveIndex', which has the smallest euclidean distance than the render vertex
	static FVertexInterpolationDesc FindMatchingVertex(const FVector3f& RenPointPosition, const FHairStrandsDatas& SimStrandsData, const uint32 SimCurveIndex)
	{
		const uint32 SimOffset = SimStrandsData.StrandsCurves.CurvesOffset[SimCurveIndex];
		float MinPointDistance = FLT_MAX;
		int32 MinPointIndex = 0;

		// Find with with vertex the vertex should be paired
		const uint32 SimPointCount = SimStrandsData.StrandsCurves.CurvesCount[SimCurveIndex];
		for (uint32 SimPointIndex = 0; SimPointIndex < SimPointCount-1; ++SimPointIndex)
		{
			
			const float SimRenDistance = (SimStrandsData.StrandsPoints.PointsPosition[SimPointIndex + SimOffset] - RenPointPosition).Size();
			if (SimRenDistance <= MinPointDistance)
			{
				MinPointDistance = SimRenDistance;
				MinPointIndex = SimPointIndex;
			}
		}
		FVertexInterpolationDesc Desc;
		const FVector3f EdgeDirection = (SimStrandsData.StrandsPoints.PointsPosition[MinPointIndex +1+ SimOffset] - SimStrandsData.StrandsPoints.PointsPosition[MinPointIndex + SimOffset]).GetSafeNormal();
		if(((RenPointPosition- SimStrandsData.StrandsPoints.PointsPosition[MinPointIndex + SimOffset]).Dot(EdgeDirection) > 0.0) || (MinPointIndex == 0))
		{
			Desc.Index0 = MinPointIndex;
			Desc.Index1 = MinPointIndex + 1;
		}
		else
		{
			Desc.Index0 = MinPointIndex-1;
			Desc.Index1 = MinPointIndex;
		}
		
		const float SimRenDistance0 = (SimStrandsData.StrandsPoints.PointsPosition[Desc.Index0 + SimOffset] - RenPointPosition).Size();
		const float SimRenDistance1 = (SimStrandsData.StrandsPoints.PointsPosition[Desc.Index1 + SimOffset] - RenPointPosition).Size();
		
		Desc.T = SimRenDistance0 / (SimRenDistance0+SimRenDistance1);
		return Desc;
	}

	static void BuildInterpolationData(
		FHairStrandsInterpolationDatas& InterpolationData,
		const FHairStrandsDatas& SimStrandsData,
		const FHairStrandsDatas& RenStrandsData,
		const FHairInterpolationSettings& Settings,
		const TArray<FIntVector>& RandomGuideIndices)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HairInterpolationBuilder::BuildInterpolationData);

		InterpolationData.SetNum(RenStrandsData.GetNumPoints());
		InterpolationData.bUseUniqueGuide = Settings.bUseUniqueGuide;

		typedef TArray<FHairRoot> FRoots;

		// Build acceleration structure for fast nearest-neighbors lookup.
		// This is used only for low quality interpolation as high quality 
		// interpolation require broader search
		FRoots RenRoots, SimRoots;
		FRootsGrid RootsGrid;
		{
			FVector RenMinBound, RenMaxBound;
			FVector SimMinBound, SimMaxBound;
			ExtractRoots(RenStrandsData, RenRoots, RenMinBound, RenMaxBound);
			ExtractRoots(SimStrandsData, SimRoots, SimMinBound, SimMaxBound);

			if (Settings.InterpolationQuality == EHairInterpolationQuality::Low || Settings.InterpolationQuality == EHairInterpolationQuality::Medium)
			{
				// Build a conservative bound, to insure all queries will fall 
				// into the grid volume.
				const FVector MinBound = RenMinBound.ComponentMin(SimMinBound);
				const FVector MaxBound = RenMaxBound.ComponentMax(SimMaxBound);
				RootsGrid.InsertRoots(SimRoots, MinBound, MaxBound);
			}
		}

		// Find k-closest guide:
		uint32 TotalInvalidInterpolationCount = 0;
		const static float MinWeightDistance = 0.0001f;

		const uint32 RenCurveCount = RenStrandsData.GetNumCurves();
		const uint32 SimCurveCount = SimStrandsData.GetNumCurves();

		TAtomic<uint32> CompletedTasks(0);
		FScopedSlowTask SlowTask(RenCurveCount, LOCTEXT("BuildInterpolationData", "Building groom simulation data"));
		SlowTask.MakeDialog();

		const FDateTime StartTime = FDateTime::UtcNow();
		ParallelFor(RenCurveCount, 
		[
			StartTime,
			Settings,
			RenCurveCount, &RenRoots, &RenStrandsData,
			SimCurveCount, &SimRoots, &SimStrandsData, 
			&RootsGrid,
			&TotalInvalidInterpolationCount,  
			&InterpolationData, 
			&RandomGuideIndices,
			&CompletedTasks,
			&SlowTask
		] (uint32 RenCurveIndex) 
		//for (uint32 RenCurveIndex = 0; RenCurveIndex < RenCurveCount; ++RenCurveIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(HairInterpolationBuilder::ComputingClosestGuidesAndWeights);

			++CompletedTasks;

			if (IsInGameThread())
			{
				const uint32 CurrentCompletedTasks = CompletedTasks.Exchange(0);
				const float RemainingTasks = FMath::Clamp(SlowTask.TotalAmountOfWork - SlowTask.CompletedWork, 1.f, SlowTask.TotalAmountOfWork);
				const FTimespan ElaspedTime = FDateTime::UtcNow() - StartTime;
				const double RemainingTimeInSeconds = RemainingTasks * double(ElaspedTime.GetSeconds() / SlowTask.CompletedWork);

				FTextBuilder TextBuilder;
				TextBuilder.AppendLineFormat(LOCTEXT("ComputeGuidesAndWeights", "Computing closest guides and weights ({0})"), FText::AsTimespan(FTimespan::FromSeconds(RemainingTimeInSeconds)));
				SlowTask.EnterProgressFrame(CurrentCompletedTasks, TextBuilder.ToText());
			}

			const FHairRoot& StrandRoot = RenRoots[RenCurveIndex];

			FClosestGuides ClosestGuides;
			if (Settings.InterpolationQuality == EHairInterpolationQuality::Low)
			{
				ClosestGuides = RootsGrid.FindClosestRoots(RenCurveIndex, RenRoots, SimRoots, RenStrandsData, SimStrandsData, Settings.bRandomizeGuide, Settings.bUseUniqueGuide, RandomGuideIndices[RenCurveIndex]);
			}
			else if (Settings.InterpolationQuality == EHairInterpolationQuality::Medium)
			{
				ClosestGuides = RootsGrid.FindBestClosestRoots(RenCurveIndex, RenRoots, SimRoots, RenStrandsData, SimStrandsData, Settings.bRandomizeGuide, Settings.bUseUniqueGuide, RandomGuideIndices[RenCurveIndex]);
			}
			else // (Settings.Quality == EHairInterpolationQuality::High)
			{
				ClosestGuides = FindBestRoots(RenCurveIndex, RenRoots, SimRoots, RenStrandsData, SimStrandsData, Settings.bRandomizeGuide, Settings.bUseUniqueGuide, RandomGuideIndices[RenCurveIndex]);
			}

			const uint32 RendPointCount	= RenStrandsData.StrandsCurves.CurvesCount[RenCurveIndex];
			const uint32 RenOffset		= RenStrandsData.StrandsCurves.CurvesOffset[RenCurveIndex];
			const FVector& RenPointPositionRoot = (FVector)RenStrandsData.StrandsPoints.PointsPosition[RenOffset];
			for (uint32 RenPointIndex = 0; RenPointIndex < RendPointCount; ++RenPointIndex)
			{
				const uint32 PointGlobalIndex = RenPointIndex + RenOffset;
				const FVector& RenPointPosition = (FVector)RenStrandsData.StrandsPoints.PointsPosition[PointGlobalIndex];
				const float RenPointDistance = RenStrandsData.StrandsPoints.PointsCoordU[PointGlobalIndex] * RenStrandsData.StrandsCurves.CurvesLength[RenCurveIndex] * RenStrandsData.StrandsCurves.MaxLength;

				float TotalWeight = 0;
				for (uint32 KIndex = 0; KIndex < FClosestGuides::Count; ++KIndex)
				{
					// Find the closest vertex on the guide which matches the strand vertex distance along its curve
					if (Settings.InterpolationDistance == EHairInterpolationWeight::Parametric || Settings.InterpolationDistance == EHairInterpolationWeight::Distance)
					{
						const uint32 SimCurveIndex = ClosestGuides.Indices[KIndex];
						const uint32 SimOffset = SimStrandsData.StrandsCurves.CurvesOffset[SimCurveIndex];
						const FVertexInterpolationDesc Desc =  (Settings.InterpolationDistance == EHairInterpolationWeight::Parametric) ?
							FindMatchingVertex(RenPointDistance, SimStrandsData, SimCurveIndex) :
							FindMatchingVertex(RenStrandsData.StrandsPoints.PointsPosition[PointGlobalIndex], SimStrandsData, SimCurveIndex);
						
						const FVector& SimPointPosition0 = (FVector)SimStrandsData.StrandsPoints.PointsPosition[Desc.Index0 + SimOffset];
						const FVector& SimPointPosition1 = (FVector)SimStrandsData.StrandsPoints.PointsPosition[Desc.Index1 + SimOffset];
						const float Weight = 1.0f / FMath::Max(MinWeightDistance, FVector::Distance(RenPointPosition, FMath::Lerp(SimPointPosition0, SimPointPosition1, Desc.T)));

						InterpolationData.PointsSimCurvesIndex[PointGlobalIndex][KIndex] = SimCurveIndex;
						InterpolationData.PointsSimCurvesVertexIndex[PointGlobalIndex][KIndex] = Desc.Index0 + SimOffset;
						InterpolationData.PointsSimCurvesVertexLerp[PointGlobalIndex][KIndex] = Desc.T;
						InterpolationData.PointsSimCurvesVertexWeights[PointGlobalIndex][KIndex] = Weight;
					}

					// Use only the root as a *constant* weight for deformation along each vertex
					// Still compute the closest vertex (in parametric distance) to know on which vertex the offset/delta should be computed
					if (Settings.InterpolationDistance == EHairInterpolationWeight::Root)
					{
						const uint32 SimCurveIndex = ClosestGuides.Indices[KIndex];
						const uint32 SimOffset = SimStrandsData.StrandsCurves.CurvesOffset[SimCurveIndex];
						const FVector& SimRootPointPosition = (FVector)SimStrandsData.StrandsPoints.PointsPosition[SimOffset];
						const FVector& RenRootPointPosition = (FVector)RenStrandsData.StrandsPoints.PointsPosition[RenOffset];
						const float Weight = 1.0f / FMath::Max(MinWeightDistance, FVector::Distance(RenRootPointPosition, SimRootPointPosition));
						const FVertexInterpolationDesc Desc = FindMatchingVertex(RenPointDistance, SimStrandsData, SimCurveIndex);

						InterpolationData.PointsSimCurvesIndex[PointGlobalIndex][KIndex] = SimCurveIndex;
						InterpolationData.PointsSimCurvesVertexIndex[PointGlobalIndex][KIndex] = Desc.Index0 + SimOffset;
						InterpolationData.PointsSimCurvesVertexLerp[PointGlobalIndex][KIndex] = Desc.T;
						InterpolationData.PointsSimCurvesVertexWeights[PointGlobalIndex][KIndex] = Weight;
					}

					// Use the *same vertex index* to match guide vertex with strand vertex
					if (Settings.InterpolationDistance == EHairInterpolationWeight::Index)
					{
						const uint32 SimCurveIndex = ClosestGuides.Indices[KIndex];
						const uint32 SimOffset = SimStrandsData.StrandsCurves.CurvesOffset[SimCurveIndex];
						const uint32 SimPointCount = SimStrandsData.StrandsCurves.CurvesCount[SimCurveIndex];
						const uint32 SimPointIndex = FMath::Clamp(RenPointIndex, 0u, SimPointCount - 1);
						const FVector& SimPointPosition = (FVector)SimStrandsData.StrandsPoints.PointsPosition[SimPointIndex + SimOffset];
						const float Weight = 1.0f / FMath::Max(MinWeightDistance, FVector::Distance(RenPointPosition, SimPointPosition));

						InterpolationData.PointsSimCurvesIndex[PointGlobalIndex][KIndex] = SimCurveIndex;
						InterpolationData.PointsSimCurvesVertexIndex[PointGlobalIndex][KIndex] = SimPointIndex + SimOffset;
						InterpolationData.PointsSimCurvesVertexLerp[PointGlobalIndex][KIndex] = 1;
						InterpolationData.PointsSimCurvesVertexWeights[PointGlobalIndex][KIndex] = Weight;
					}

					TotalWeight += InterpolationData.PointsSimCurvesVertexWeights[PointGlobalIndex][KIndex];
				}

				for (int32 KIndex = 0; KIndex < FClosestGuides::Count; ++KIndex)
				{
					InterpolationData.PointsSimCurvesVertexWeights[PointGlobalIndex][KIndex] /= TotalWeight;
				}
			}
		});
	}

	/** Build data for interpolation between simulation and rendering */
	void BuildRenderData(
		const FHairStrandsDatas& SimDatas, 
		const FHairStrandsInterpolationDatas& HairInterpolation, 
		FHairStrandsInterpolationBulkData& OutBulkData)
	{
		OutBulkData.Reset();

		TRACE_CPUPROFILER_EVENT_SCOPE(HairInterpolationBuilder::BuildRenderData);

		const uint32 PointCount = HairInterpolation.Num();
		if (PointCount == 0)
			return;

		auto LowerPart = [](uint32 Index) -> uint16 { return uint16(Index & 0xFFFF); };
		auto UpperPart = [](uint32 Index) -> uint8  { return uint8((Index >> 16) & 0xFF); };


		OutBulkData.Flags = FHairStrandsInterpolationBulkData::DataFlags_HasData;
		OutBulkData.PointCount = PointCount;

		if (HairInterpolation.bUseUniqueGuide)
		{
			OutBulkData.Flags |= FHairStrandsInterpolationBulkData::DataFlags_HasSingleGuideData;

			TArray<FHairStrandsInterpolationFormat::Type> OutPointsInterpolation;
			OutPointsInterpolation.SetNum(PointCount * FHairStrandsInterpolationFormat::ComponentCount);

			for (uint32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				const FIntVector& Indices = HairInterpolation.PointsSimCurvesVertexIndex[PointIndex];
				const FVector& Weights = (FVector)HairInterpolation.PointsSimCurvesVertexWeights[PointIndex];
				const FVector& S = (FVector)HairInterpolation.PointsSimCurvesVertexLerp[PointIndex];

				FHairStrandsInterpolationFormat::Type& OutInterp = OutPointsInterpolation[PointIndex];
				OutInterp.VertexGuideIndex0 = LowerPart(Indices[0]);
				OutInterp.VertexGuideIndex1 = UpperPart(Indices[0]);
				OutInterp.VertexLerp		= S[0] * 255.f;
			}

			HairStrandsBuilder::CopyToBulkData<FHairStrandsInterpolationFormat>(OutBulkData.Interpolation, OutPointsInterpolation);
		}
		else
		{
			TArray<FHairStrandsInterpolation0Format::Type> OutPointsInterpolation0;
			TArray<FHairStrandsInterpolation1Format::Type> OutPointsInterpolation1;

			OutPointsInterpolation0.SetNum(PointCount * FHairStrandsInterpolation0Format::ComponentCount);
			OutPointsInterpolation1.SetNum(PointCount * FHairStrandsInterpolation1Format::ComponentCount);

			for (uint32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
			{
				const FIntVector& Indices = HairInterpolation.PointsSimCurvesVertexIndex[PointIndex];
				const FVector& Weights = (FVector)HairInterpolation.PointsSimCurvesVertexWeights[PointIndex];
				const FVector& S = (FVector)HairInterpolation.PointsSimCurvesVertexLerp[PointIndex];

				FHairStrandsInterpolation0Format::Type& OutInterp0 = OutPointsInterpolation0[PointIndex];
				OutInterp0.Index0 = LowerPart(Indices[0]);
				OutInterp0.Index1 = LowerPart(Indices[1]);
				OutInterp0.Index2 = LowerPart(Indices[2]);
				OutInterp0.VertexWeight0 = Weights[0] * 255.f;
				OutInterp0.VertexWeight1 = Weights[1] * 255.f;

				FHairStrandsInterpolation1Format::Type& OutInterp1 = OutPointsInterpolation1[PointIndex];
				OutInterp1.VertexIndex0 = UpperPart(Indices[0]);
				OutInterp1.VertexIndex1 = UpperPart(Indices[1]);
				OutInterp1.VertexIndex2 = UpperPart(Indices[2]);
				OutInterp1.VertexLerp0  = S[0] * 255.f;
				OutInterp1.VertexLerp1  = S[1] * 255.f;
				OutInterp1.VertexLerp2  = S[2] * 255.f;
				OutInterp1.Pad0			= 0;
				OutInterp1.Pad1			= 0;
			}

			HairStrandsBuilder::CopyToBulkData<FHairStrandsInterpolation0Format>(OutBulkData.Interpolation0, OutPointsInterpolation0);
			HairStrandsBuilder::CopyToBulkData<FHairStrandsInterpolation1Format>(OutBulkData.Interpolation1, OutPointsInterpolation1);
		}

		{
			OutBulkData.SimPointCount = SimDatas.GetNumPoints();
			const uint32 SimCurveCount = SimDatas.GetNumCurves();

			TArray<FHairStrandsRootIndexFormat::Type> SimRootPointIndex;
			SimRootPointIndex.SetNum(OutBulkData.SimPointCount);
			for (uint32 CurveIndex = 0; CurveIndex < SimCurveCount; ++CurveIndex)
			{
				const uint16 SimPointCount = SimDatas.StrandsCurves.CurvesCount[CurveIndex];
				const uint32 SimPointOffset = SimDatas.StrandsCurves.CurvesOffset[CurveIndex];
				for (uint32 PointIndex = 0; PointIndex < SimPointCount; ++PointIndex)
				{
					SimRootPointIndex[PointIndex + SimPointOffset] = SimPointOffset;
				}
			}

			HairStrandsBuilder::CopyToBulkData<FHairStrandsRootIndexFormat>(OutBulkData.SimRootPointIndex, SimRootPointIndex);
		}
	}

	/** Fill the GroomAsset with the interpolation data that exists in the HairDescription */
	void FillInterpolationData(
		const FHairStrandsDatas& RenData,
		const FHairStrandsDatas& SimData,
		FHairStrandsInterpolationDatas& OutInterpolation)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HairInterpolationBuilder::FillInterpolationData);

		check(RenData.StrandsCurves.CurvesClosestGuideIDs.Num() != 0);
		check(RenData.StrandsCurves.CurvesClosestGuideWeights.Num() != 0);
		check(SimData.GetNumCurves() > 0);
		int32 SimCurveCount = SimData.GetNumCurves();

		OutInterpolation.SetNum(RenData.GetNumPoints());
		for (uint32 CurveIndex = 0; CurveIndex < RenData.GetNumCurves(); ++CurveIndex)
		{
			const uint32 CurveOffset = RenData.StrandsCurves.CurvesOffset[CurveIndex];
			const uint16 CurveNumVertices = RenData.StrandsCurves.CurvesCount[CurveIndex];

			const FIntVector StrandClosestGuides = RenData.StrandsCurves.CurvesClosestGuideIDs[CurveIndex];
			const FVector StrandGuideWeights  = RenData.StrandsCurves.CurvesClosestGuideWeights[CurveIndex];
			for (uint16 VertexIndex = 0; VertexIndex < CurveNumVertices; ++VertexIndex)
			{
				const uint32 PointGlobalIndex = VertexIndex + CurveOffset;
				const FVector& RenPointPosition = (FVector)RenData.StrandsPoints.PointsPosition[PointGlobalIndex];
				const float RenPointDistance = RenData.StrandsPoints.PointsCoordU[PointGlobalIndex] * RenData.StrandsCurves.CurvesLength[CurveIndex] * RenData.StrandsCurves.MaxLength;

				float TotalWeight = 0;
				for (uint32 GuideIndex = 0; GuideIndex < FClosestGuides::Count; ++GuideIndex)
				{
					int32 ImportedGroomID = StrandClosestGuides[GuideIndex];
					const int* SimCurveIndex = SimData.StrandsCurves.GroomIDToIndex.Find(ImportedGroomID);

					if (SimCurveIndex && *SimCurveIndex >= 0 && *SimCurveIndex < SimCurveCount)
					{
						// Fill the interpolation data using the ParametricDistance algorithm with a constant weight for all vertices along the strand
						const uint32 SimOffset = SimData.StrandsCurves.CurvesOffset[*SimCurveIndex];
						const FVertexInterpolationDesc Desc = FindMatchingVertex(RenPointDistance, SimData, *SimCurveIndex);

						OutInterpolation.PointsSimCurvesIndex[PointGlobalIndex][GuideIndex] = *SimCurveIndex;
						OutInterpolation.PointsSimCurvesVertexIndex[PointGlobalIndex][GuideIndex] = Desc.Index0 + SimOffset;
						OutInterpolation.PointsSimCurvesVertexLerp[PointGlobalIndex][GuideIndex] = Desc.T;
						OutInterpolation.PointsSimCurvesVertexWeights[PointGlobalIndex][GuideIndex] = StrandGuideWeights[GuideIndex];
						TotalWeight += OutInterpolation.PointsSimCurvesVertexWeights[PointGlobalIndex][GuideIndex];
					}
				}

				// Normalize the weights
				if (TotalWeight > 0.f)
				{
					for (int32 GuideIndex = 0; GuideIndex < FClosestGuides::Count; ++GuideIndex)
					{
						OutInterpolation.PointsSimCurvesVertexWeights[PointGlobalIndex][GuideIndex] /= TotalWeight;
					}
				}
			}
		}
	}
}

class FGroomDataRandomizer
{
public:
	FGroomDataRandomizer(int Seed, int NumRenderCurves, int NumSimCurves)
	{
		Random.Initialize(Seed);

		RenderCurveSeeds.SetNumUninitialized(NumRenderCurves);
		for (int Index = 0; Index < NumRenderCurves; ++Index)
		{
			RenderCurveSeeds[Index] = Random.RandHelper(255);
		}

		SimCurveSeeds.SetNumUninitialized(NumSimCurves);
		for (int Index = 0; Index < NumSimCurves; ++Index)
		{
			SimCurveSeeds[Index] = Random.RandHelper(255);
		}

		// This randomization makes certain strands being affected by 1, 2, or 3 guides
		GuideIndices.SetNumUninitialized(NumRenderCurves);
		for (int Index = 0; Index < NumRenderCurves; ++Index)
		{
			FIntVector& RandomIndices = GuideIndices[Index];
			for (int GuideIndex = 0; GuideIndex < HairInterpolationBuilder::FMetrics::Count; ++GuideIndex)
			{
				RandomIndices[GuideIndex] = Random.RandRange(0, HairInterpolationBuilder::FMetrics::Count - 1);
			}
		}
	}

	const TArray<uint8>& GetRenderCurveSeeds() const { return RenderCurveSeeds; }
	const TArray<uint8>& GetSimCurveSeeds() const { return SimCurveSeeds; }
	const TArray<FIntVector>& GetRandomGuideIndices() const { return GuideIndices; }

private:
	FRandomStream Random;
	TArray<uint8> RenderCurveSeeds;
	TArray<uint8> SimCurveSeeds;
	TArray<FIntVector> GuideIndices;
};

FORCEINLINE FIntVector VectorToIntVector(const FVector& Index)
{
	return FIntVector(Index.X, Index.Y, Index.Z);
}

// These accessors are defined here to ease mirror logic with BuildHairDescriptionGroups, if any type/attributes changes
bool FHairDescription::HasRootUV() const
{
	return StrandAttributes().GetAttributesRef<FVector2f>(HairAttribute::Strand::RootUV).IsValid();
}

bool FHairDescription::HasGuideWeights() const
{
	return
		// Single
		(StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::ClosestGuides).IsValid() && 
		 StrandAttributes().GetAttributesRef<float>(HairAttribute::Strand::GuideWeights).IsValid())
		||
		// Triplet
		(StrandAttributes().GetAttributesRef<FVector3f>(HairAttribute::Strand::ClosestGuides).IsValid() &&
		 StrandAttributes().GetAttributesRef<FVector3f>(HairAttribute::Strand::GuideWeights).IsValid());
}

bool FHairDescription::HasColorAttributes() const
{
	return
		VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Color).IsValid() ||
		StrandAttributes().GetAttributesRef<FVector3f>(HairAttribute::Strand::Color).IsValid() ||
		GroomAttributes().GetAttributesRef<FVector3f>(HairAttribute::Groom::Color).IsValid();
}

bool FHairDescription::HasRoughnessAttributes() const
{
	return
		VertexAttributes().GetAttributesRef<float>(HairAttribute::Vertex::Roughness).IsValid() ||
		StrandAttributes().GetAttributesRef<float>(HairAttribute::Strand::Roughness).IsValid() ||
		GroomAttributes().GetAttributesRef<float>(HairAttribute::Groom::Roughness).IsValid();
}

bool FGroomBuilder::BuildHairDescriptionGroups(const FHairDescription& HairDescription, FHairDescriptionGroups& Out)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FGroomBuilder::BuildHairDescriptionGroups);

	// Convert HairDescription to HairStrandsDatas
	// For now, just convert HairDescription to HairStrandsDatas
	int32 NumCurves = HairDescription.GetNumStrands();
	int32 NumVertices = HairDescription.GetNumVertices();

	// Check for required attributes
	TGroomAttributesConstRef<int> MajorVersion = HairDescription.GroomAttributes().GetAttributesRef<int>(HairAttribute::Groom::MajorVersion);
	TGroomAttributesConstRef<int> MinorVersion = HairDescription.GroomAttributes().GetAttributesRef<int>(HairAttribute::Groom::MinorVersion);

	// Major/Minor version check is currently disabled as this is not used at the moment and create false positive
	#if 0
	if (!MajorVersion.IsValid() || !MinorVersion.IsValid())
	{
		UE_LOG(LogGroomBuilder, Warning, TEXT("No version number attributes found. The groom may be missing attributes even if it imports."));
	}
	#endif

	FGroomID GroomID(0);

	TGroomAttributesConstRef<float> GroomHairWidthAttribute = HairDescription.GroomAttributes().GetAttributesRef<float>(HairAttribute::Groom::Width);
	TOptional<float> GroomHairWidth;
	if (GroomHairWidthAttribute.IsValid())
	{
		GroomHairWidth = GroomHairWidthAttribute[GroomID];
	}

	TGroomAttributesConstRef<FVector3f> GroomHairColorAttribute = HairDescription.GroomAttributes().GetAttributesRef<FVector3f>(HairAttribute::Groom::Color);
	TOptional<FVector> GroomHairColor;
	if (GroomHairColorAttribute.IsValid())
	{
		GroomHairColor = (FVector)GroomHairColorAttribute[GroomID];
	}

	TGroomAttributesConstRef<float> GroomHairRoughnessAttribute = HairDescription.GroomAttributes().GetAttributesRef<float>(HairAttribute::Groom::Roughness);
	TOptional<float> GroomHairRoughness;
	if (GroomHairRoughnessAttribute.IsValid())
	{
		GroomHairRoughness = GroomHairRoughnessAttribute[GroomID];
	}

	TVertexAttributesConstRef<FVector3f> VertexPositions	= HairDescription.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Position);
	TVertexAttributesConstRef<FVector3f> VertexBaseColor	= HairDescription.VertexAttributes().GetAttributesRef<FVector3f>(HairAttribute::Vertex::Color);
	TVertexAttributesConstRef<float> VertexRoughness	= HairDescription.VertexAttributes().GetAttributesRef<float>(HairAttribute::Vertex::Roughness);
	TStrandAttributesConstRef<int> StrandNumVertices	= HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::VertexCount);

	if (!VertexPositions.IsValid() || !StrandNumVertices.IsValid())
	{
		UE_LOG(LogGroomBuilder, Warning, TEXT("Failed to import hair: No vertices or curves data found."));
		return false;
	}

	const bool bHasBaseColorAttribute = VertexBaseColor.IsValid();
	const bool bHasRoughnessAttribute = VertexRoughness.IsValid();

	TVertexAttributesConstRef<float> VertexWidths = HairDescription.VertexAttributes().GetAttributesRef<float>(HairAttribute::Vertex::Width);
	TStrandAttributesConstRef<float> StrandWidths = HairDescription.StrandAttributes().GetAttributesRef<float>(HairAttribute::Strand::Width);

	TStrandAttributesConstRef<FVector2f> StrandRootUV = HairDescription.StrandAttributes().GetAttributesRef<FVector2f>(HairAttribute::Strand::RootUV);
	const bool bHasUVData = StrandRootUV.IsValid();

	TStrandAttributesConstRef<int> StrandGuides = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::Guide);
	TStrandAttributesConstRef<int> GroupIDs = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::GroupID);
	TStrandAttributesConstRef<int> StrandIDs = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::ID);
	TStrandAttributesConstRef<FName> GroupNames = HairDescription.StrandAttributes().GetAttributesRef<FName>(HairAttribute::Strand::GroupName);

	bool bImportGuides = true;

	// For precomputed weights, there is two variants with single or triplet IDs/weights
	// Single
	TStrandAttributesConstRef<int> ClosestGuide		  = HairDescription.StrandAttributes().GetAttributesRef<int>(HairAttribute::Strand::ClosestGuides);
	TStrandAttributesConstRef<float> GuideWeight	  = HairDescription.StrandAttributes().GetAttributesRef<float>(HairAttribute::Strand::GuideWeights);
	// Triplet
	TStrandAttributesConstRef<FVector3f> ClosestGuides  = HairDescription.StrandAttributes().GetAttributesRef<FVector3f>(HairAttribute::Strand::ClosestGuides);
	TStrandAttributesConstRef<FVector3f> GuideWeights	  = HairDescription.StrandAttributes().GetAttributesRef<FVector3f>(HairAttribute::Strand::GuideWeights);

	// To use ClosestGuides and GuideWeights attributes, guides must be imported from HairDescription and
	// must include StrandID attribute since ClosestGuides references those IDs
	const bool bPrecomputedWeight1 = ClosestGuide.IsValid()  && GuideWeight.IsValid();
	const bool bPrecomputedWeight3 = ClosestGuides.IsValid() && GuideWeights.IsValid();
	const bool bCanUseClosestGuidesAndWeights = bImportGuides && StrandIDs.IsValid() && (bPrecomputedWeight1 || bPrecomputedWeight3);

	auto FindOrAdd = [&Out](int32 GroupID, FName GroupName) -> FHairDescriptionGroup&
	{
		for (FHairDescriptionGroup& Group : Out.HairGroups)
		{
			if (Group.Info.GroupID == GroupID)
			{
				return Group;
			}
		}
		FHairDescriptionGroup& Group = Out.HairGroups.AddDefaulted_GetRef();
		Group.Info.GroupID = GroupID;
		Group.Info.GroupName = GroupName != NAME_None ? GroupName : FName(FString::Printf(TEXT("Group_%d"), GroupID));
		Group.Info.MaxImportedWidth = -1.0f; // Invalid width
		return Group;
	};

	// Track the imported max hair width among all imported CVs. This information is displayed to artsits to set/tune groom asset later.
	float DDCMaxHairWidth = -1.f;

	int32 GlobalVertexIndex = 0;
	int32 NumHairPoints = 0;
	int32 NumGuidePoints = 0;
	for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
	{
		FStrandID StrandID(CurveIndex);

		bool bIsGuide = false;
		if (StrandGuides.IsValid())
		{
			// Version 0.1 defines 1 as being guide
			bIsGuide = StrandGuides[StrandID] == 1;
		}

		int32 CurveNumVertices = StrandNumVertices[StrandID];

		if (CurveNumVertices <= 0)
		{
			continue;
		}

		int32 GroupID = 0;
		FName GroupName = NAME_None;
		if (GroupIDs.IsValid())
		{
			GroupID = GroupIDs[StrandID];
			GroupName = GroupNames.IsValid() && StrandID < GroupNames.GetNumElements() ? GroupNames[StrandID] : NAME_None;
		}

		FHairStrandsDatas* CurrentHairStrandsDatas = nullptr;
		FHairDescriptionGroup& Group = FindOrAdd(GroupID, GroupName);
		check(Group.Info.GroupID == GroupID);
		if (!bIsGuide)
		{
			NumHairPoints += CurveNumVertices;
			CurrentHairStrandsDatas = &Group.Strands;

			++Group.Info.NumCurves;
		}
		else if (bImportGuides)
		{
			NumGuidePoints += CurveNumVertices;
			CurrentHairStrandsDatas = &Group.Guides;

			++Group.Info.NumGuides;
		}
		else
		{
			// A guide but don't want to import it, so skip it
			GlobalVertexIndex += CurveNumVertices;
			continue;
		}

		if (!CurrentHairStrandsDatas)
			continue;

		CurrentHairStrandsDatas->StrandsCurves.CurvesCount.Add(CurveNumVertices);

		if (bCanUseClosestGuidesAndWeights)
		{
			// ClosesGuides needs mapping of StrandID (the strand index in the HairDescription)
			CurrentHairStrandsDatas->StrandsCurves.StrandIDs.Add(CurveIndex);

			// and ImportedGroomID (the imported ID associated with a strand) to index in StrandCurves
			const int ImportedGroomID = StrandIDs[StrandID];
			const int StrandCurveIndex = CurrentHairStrandsDatas->StrandsCurves.CurvesCount.Num() - 1;
			CurrentHairStrandsDatas->StrandsCurves.GroomIDToIndex.Add(ImportedGroomID, StrandCurveIndex);

			if (!bIsGuide)
			{
				if (bPrecomputedWeight3)
				{
					CurrentHairStrandsDatas->StrandsCurves.CurvesClosestGuideIDs.Add(VectorToIntVector((FVector)ClosestGuides[StrandID]));
					CurrentHairStrandsDatas->StrandsCurves.CurvesClosestGuideWeights.Add((FVector)GuideWeights[StrandID]);
				}
				else
				{
					CurrentHairStrandsDatas->StrandsCurves.CurvesClosestGuideIDs.Add(FIntVector(ClosestGuide[StrandID]));
					CurrentHairStrandsDatas->StrandsCurves.CurvesClosestGuideWeights.Add(FVector(GuideWeight[StrandID]));
				}
			}
		}

		if (bHasUVData)
		{
			CurrentHairStrandsDatas->StrandsCurves.CurvesRootUV.Add(StrandRootUV[StrandID]);
		}

		// Groom 
		float StrandWidth = 0.01f;
		if (GroomHairWidth) 
		{
			StrandWidth = GroomHairWidth.GetValue();
			if (!bIsGuide)
			{
				Group.Info.MaxImportedWidth = FMath::Max(Group.Info.MaxImportedWidth, StrandWidth);
			}
		}

		// Curve
		if (StrandWidths.IsValid())
		{
			StrandWidth = StrandWidths[StrandID];
			if (!bIsGuide)
			{
				Group.Info.MaxImportedWidth = FMath::Max(Group.Info.MaxImportedWidth, StrandWidth);
			}
		}

		for (int32 VertexIndex = 0; VertexIndex < CurveNumVertices; ++VertexIndex, ++GlobalVertexIndex)
		{
			FVertexID VertexID(GlobalVertexIndex);

			CurrentHairStrandsDatas->StrandsPoints.PointsPosition.Add(VertexPositions[VertexID]);
			CurrentHairStrandsDatas->StrandsPoints.PointsBaseColor.Add(bHasBaseColorAttribute ? FLinearColor(VertexBaseColor[VertexID]) : (GroomHairColor     ? FLinearColor(GroomHairColor.GetValue()) : FLinearColor::Black));
			CurrentHairStrandsDatas->StrandsPoints.PointsRoughness.Add(bHasRoughnessAttribute ? VertexRoughness[VertexID] : (GroomHairRoughness ? GroomHairRoughness.GetValue() : 0.f));

			// Vertex
			float VertexWidth = 0.f;
			if (VertexWidths.IsValid())
			{
				VertexWidth = VertexWidths[VertexID];
				if (!bIsGuide)
				{
					Group.Info.MaxImportedWidth = FMath::Max(Group.Info.MaxImportedWidth, VertexWidth);
				}
			}
			else if (StrandWidth != 0.f)
			{
				// Fall back to strand width if there was no vertex width
				VertexWidth = StrandWidth;
			}

			// If the curve is a guide, and its width is 0, force to a constant width to insure the guides can be correctly rendered/displayed for debug purpose
			if (bIsGuide && VertexWidth == 0.f)
			{
				VertexWidth = 0.01f;
			}

			CurrentHairStrandsDatas->StrandsPoints.PointsRadius.Add(VertexWidth * 0.5f);
		}
	}

	// Sparse->Dense Groups
	// Change GroupID to be contiguous
	{
		int32 GroupIndex = 0;
		for (FHairDescriptionGroup& Group : Out.HairGroups)
		{
			Group.Info.GroupID = GroupIndex++;
			Group.bCanUseClosestGuidesAndWeights = bCanUseClosestGuidesAndWeights;
			Group.bHasUVData = bHasUVData;
		}
	}

	// Compute bound radius
	{
		FVector3f GroomBoundMin(FLT_MAX);
		FVector3f GroomBoundMax(-FLT_MAX);
		for (const FHairDescriptionGroup& Group : Out.HairGroups)
		{
			for (const FVector3f& P : Group.Strands.StrandsPoints.PointsPosition)
			{
				GroomBoundMin.X = FMath::Min(GroomBoundMin.X, P.X);
				GroomBoundMin.Y = FMath::Min(GroomBoundMin.Y, P.Y);
				GroomBoundMin.Z = FMath::Min(GroomBoundMin.Z, P.Z);

				GroomBoundMax.X = FMath::Max(GroomBoundMax.X, P.X);
				GroomBoundMax.Y = FMath::Max(GroomBoundMax.Y, P.Y);
				GroomBoundMax.Z = FMath::Max(GroomBoundMax.Z, P.Z);
			}
		}

		Out.Bounds = FBoxSphereBounds3f(FBox3f(GroomBoundMin, GroomBoundMax));
	}

	// Update GroupInfo
	for (FHairDescriptionGroup& Group : Out.HairGroups)
	{
		// Sanity check
		check(Group.Strands.GetNumCurves() == Group.Info.NumCurves);
		check(Group.Guides.GetNumCurves() == Group.Info.NumGuides);

		// Update infos
		Group.Info.NumCurveVertices = Group.Strands.GetNumPoints();
		Group.Info.NumGuideVertices = Group.Guides.GetNumPoints();

		// Prepare/Resize all attributes based on position/curves counts
		Group.Strands.StrandsPoints.SetNum(Group.Strands.GetNumPoints());
		Group.Strands.StrandsCurves.SetNum(Group.Strands.GetNumCurves());

		// Prepare/Resize all attributes based on position/curves counts
		Group.Guides.StrandsPoints.SetNum(Group.Guides.GetNumPoints());
		Group.Guides.StrandsCurves.SetNum(Group.Guides.GetNumCurves());
	}

	return true;
}

void FGroomBuilder::BuildData(FHairStrandsDatas& OutStrands)
{
	HairStrandsBuilder::BuildInternalData(OutStrands, false);
}

void FGroomBuilder::BuildData(
	const FHairDescriptionGroup& InHairDescriptionGroup, 
	const FHairGroupsInterpolation& InSettings, 
	FHairGroupInfo& OutGroupInfo, 
	FHairStrandsDatas& OutRen,
	FHairStrandsDatas& OutSim)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FGroomBuilder::BuildData);

	// Sanitize decimation values. Do not update the 'InSettings' values directly as this would change 
	// the groom asset and thus would change the DDC key
	const float CurveDecimation		= FMath::Clamp(InSettings.DecimationSettings.CurveDecimation, 0.f, 1.f);
	const float VertexDecimation	= FMath::Clamp(InSettings.DecimationSettings.VertexDecimation, 0.f, 1.f);
	const float HairToGuideDensity	= FMath::Clamp(InSettings.InterpolationSettings.HairToGuideDensity, 0.f, 1.f);	
	const bool bContinuousDecimationReordering = IsHairStrandContinuousDecimationReorderingEnabled(); //this is a read only project setting which will modify the platform data that is stored in the DDC

	{
		OutGroupInfo.GroupID = InHairDescriptionGroup.Info.GroupID;
		OutGroupInfo.GroupName = InHairDescriptionGroup.Info.GroupName;
		OutGroupInfo.MaxImportedWidth = InHairDescriptionGroup.Info.MaxImportedWidth;

		// Rendering data
		{
			OutRen = InHairDescriptionGroup.Strands;

			HairStrandsBuilder::BuildInternalData(OutRen, !InHairDescriptionGroup.bHasUVData);

			// Decimate
			if (CurveDecimation < 1 || VertexDecimation < 1 || bContinuousDecimationReordering)
			{
				FHairStrandsDatas FullData = OutRen;
				OutRen.Reset();
				FHairStrandsDecimation::Decimate(FullData, CurveDecimation, VertexDecimation, bContinuousDecimationReordering, OutRen);
			}
			OutGroupInfo.NumCurves			= OutRen.GetNumCurves();
			OutGroupInfo.NumCurveVertices	= OutRen.GetNumPoints();
			OutGroupInfo.MaxCurveLength		= OutRen.StrandsCurves.MaxLength;

			// Sanity check
			check(OutGroupInfo.NumCurves		<= InHairDescriptionGroup.Info.NumCurves);
			check(OutGroupInfo.NumCurveVertices <= InHairDescriptionGroup.Info.NumCurveVertices);

		}

		// Simulation data
		{
			OutSim = InHairDescriptionGroup.Guides;
			if (InHairDescriptionGroup.Info.NumGuides > 0 && !InSettings.InterpolationSettings.bOverrideGuides)
			{
				HairStrandsBuilder::BuildInternalData(OutSim, true); // Imported guides don't currently have root UVs so force computing them
			}
			else
			{
				OutSim.Reset();
				if(InSettings.RiggingSettings.bEnableRigging)
				{
					if (InHairDescriptionGroup.Info.NumGuides > 0)
					{
						// We pick the new guides among the imported ones
						FHairStrandsDatas TempSim = InHairDescriptionGroup.Guides;
						HairStrandsBuilder::BuildInternalData(TempSim, true);
						FHairStrandsDecimation::Decimate(TempSim, InSettings.RiggingSettings.NumCurves, InSettings.RiggingSettings.NumPoints, OutSim);
					}
					else
					{
						// Otherwise let s pick the guides among the rendered strands
						FHairStrandsDecimation::Decimate(OutRen, InSettings.RiggingSettings.NumCurves, InSettings.RiggingSettings.NumPoints, OutSim);
					}
				}
				else
				{
					FHairStrandsDecimation::Decimate(OutRen, HairToGuideDensity, 1, false, OutSim);
				}
			}

			OutGroupInfo.NumGuides			= OutSim.GetNumCurves();
			OutGroupInfo.NumGuideVertices	= OutSim.GetNumPoints();

			// Sanity check
			check(OutGroupInfo.NumGuides > 0);
			check(OutGroupInfo.NumGuideVertices > 0);
		}
	}
}

void FGroomBuilder::BuildBulkData(
	const FHairGroupInfo& InInfo,
	const FHairStrandsDatas& InData,
	FHairStrandsBulkData& OutBulkData)
{
	OutBulkData.Reset();

	const int32 NumCurves = InData.GetNumCurves();
	FRandomStream Random;
	Random.Initialize(InInfo.GroupID);
	TArray<uint8> CurveSeeds;
	CurveSeeds.SetNumUninitialized(NumCurves);
	for (int32 Index = 0; Index < NumCurves; ++Index)
	{
		CurveSeeds[Index] = Random.RandHelper(255);
	}
	HairStrandsBuilder::BuildRenderData(InData, CurveSeeds, OutBulkData);
}

void FGroomBuilder::BuildInterplationData(
	const FHairGroupInfo& InInfo,
	const FHairStrandsDatas& InRenData,
	const FHairStrandsDatas& InSimData,
	const FHairInterpolationSettings& InInterpolationSettings,
	FHairStrandsInterpolationDatas& OutInterpolationData)
{
	// Build Rendering data for InterpolationData
	// Build InterpolationData from render and simulation HairStrandsDatas
	// Skip building if interpolation data was provided by the source file
	if (OutInterpolationData.Num() == 0)
	{
		// If there's usable closest guides and guide weights attributes, fill them into the asset
		// This step requires the HairSimulationData (guides) to be filled prior to this
		const bool bUsePrecomputedWeights = !InInterpolationSettings.bOverrideGuides && InRenData.StrandsCurves.CurvesClosestGuideIDs.Num() > 0 && InRenData.StrandsCurves.CurvesClosestGuideWeights.Num() > 0;
		if (bUsePrecomputedWeights)
		{
			FHairStrandsInterpolationDatas OutInterpolation;
			HairInterpolationBuilder::FillInterpolationData(InRenData, InSimData, OutInterpolationData);
		}
		else
		{
			// This randomization makes certain strands being affected by 1, 2, or 3 guides
			FRandomStream Random;
			Random.Initialize(InInfo.GroupID);
			const int32 NumRenderCurves = InRenData.GetNumCurves();
			TArray<FIntVector> GuideIndices;
			GuideIndices.SetNumUninitialized(NumRenderCurves);
			for (int32 Index = 0; Index < NumRenderCurves; ++Index)
			{
				FIntVector& RandomIndices = GuideIndices[Index];
				for (int GuideIndex = 0; GuideIndex < HairInterpolationBuilder::FMetrics::Count; ++GuideIndex)
				{
					RandomIndices[GuideIndex] = Random.RandRange(0, HairInterpolationBuilder::FMetrics::Count - 1);
				}
			}

			HairInterpolationBuilder::BuildInterpolationData(OutInterpolationData, InSimData, InRenData, InInterpolationSettings, GuideIndices);
		}


	}
}

void FGroomBuilder::BuildInterplationBulkData(
	const FHairStrandsDatas& InSimData,
	const FHairStrandsInterpolationDatas& InInterpolationData,
	FHairStrandsInterpolationBulkData& OutInterpolationData)
{
	HairInterpolationBuilder::BuildRenderData(InSimData, InInterpolationData, OutInterpolationData);
}

namespace FHairStrandsDecimation
{

inline uint32 DecimatePointCount(uint32 InCount, float InDecimationFactor)
{
	return FMath::Clamp(uint32(FMath::CeilToInt(InCount * InDecimationFactor)), 2u, InCount);
}

static void DecimateCurve(
	const TArray<FVector3f>& InPoints, 
	const uint32 InOffset, 
	const uint32 InCount, 
	const float InDecimationFactor, 
	TArray<uint32>& OutIndices)
{
	check(InCount > 2);
	const int32 OutCount = DecimatePointCount(InCount, InDecimationFactor);

	OutIndices.SetNum(InCount);
	for (uint32 CurveIt = 0; CurveIt < InCount; ++CurveIt)
	{
		OutIndices[CurveIt] = CurveIt;
	}

	while (OutIndices.Num() > OutCount)
	{
		float MinError = FLT_MAX;
		uint32 ElementToRemove = 0;
		const uint32 Count = OutIndices.Num();
		for (uint32 IndexIt = 1; IndexIt < Count - 1; ++IndexIt)
		{
			const FVector& P0 = (FVector)InPoints[InOffset + OutIndices[IndexIt - 1]];
			const FVector& P1 = (FVector)InPoints[InOffset + OutIndices[IndexIt]];
			const FVector& P2 = (FVector)InPoints[InOffset + OutIndices[IndexIt + 1]];

			const float Area = FVector::CrossProduct(P0 - P1, P2 - P1).Size() * 0.5f;

			if (Area < MinError)
			{
				MinError = Area;
				ElementToRemove = IndexIt;
			}
		}
		OutIndices.RemoveAt(ElementToRemove);
	}
}
	
void Decimate(
		const FHairStrandsDatas& InData, 
		const uint32 NumCurves,
		const int32 NumVertices,
		FHairStrandsDatas& OutData)
{
	const uint32 InCurveCount = InData.StrandsCurves.Num();
	const uint32 OutCurveCount = FMath::Clamp(NumCurves, 1u, InCurveCount);
	
	// Fill root positions and curves offsets for sampling
	TArray<FVector3f> RootPositions;
	RootPositions.Init(FVector3f::ZeroVector, InData.GetNumCurves());
	for(uint32 StrandIndex = 0; StrandIndex < InData.GetNumCurves(); ++StrandIndex)
	{
		RootPositions[StrandIndex] =
			InData.StrandsPoints.PointsPosition[InData.StrandsCurves.CurvesOffset[StrandIndex]];
	}
				
	TArray<bool> ValidPoints;
	ValidPoints.Init(true, InData.GetNumCurves());

	// Pick NumCurves strands from the guides
	GroomBinding_RBFWeighting::FPointsSampler PointsSampler(ValidPoints, RootPositions.GetData(), OutCurveCount);

	const uint32 CurveCount = PointsSampler.SampleIndices.Num();
	uint32 PointCount = CurveCount * NumVertices;

	OutData.StrandsCurves.SetNum(CurveCount);
	OutData.StrandsPoints.SetNum(PointCount);
	OutData.HairDensity = InData.HairDensity;

	const bool bHasPrecomputedWeights = InData.StrandsCurves.CurvesClosestGuideIDs.Num() > 0 && InData.StrandsCurves.CurvesClosestGuideWeights.Num() > 0;
	
	PointCount = 0;
	for(uint32 CurveIndex = 0; CurveIndex < CurveCount; ++CurveIndex)
	{
		const int32 GuideIndex = PointsSampler.SampleIndices[CurveIndex];
		const int32 GuideOffset = InData.StrandsCurves.CurvesOffset[GuideIndex];
		const int32 GuideCount = InData.StrandsCurves.CurvesCount[GuideIndex];
		
		const int32 MaxPoints = FMath::Min(GuideCount, NumVertices);
		const float BoneSpace = FMath::Max(1.0,float(GuideCount-1) / (MaxPoints-1));

		OutData.StrandsCurves.CurvesCount[CurveIndex]  = MaxPoints;
		OutData.StrandsCurves.CurvesRootUV[CurveIndex] = InData.StrandsCurves.CurvesRootUV[GuideIndex];
		OutData.StrandsCurves.CurvesOffset[CurveIndex] = PointCount;
		OutData.StrandsCurves.CurvesLength[CurveIndex] = InData.StrandsCurves.CurvesLength[GuideIndex];
		if (bHasPrecomputedWeights)
		{
			OutData.StrandsCurves.CurvesClosestGuideIDs[CurveIndex] = InData.StrandsCurves.CurvesClosestGuideIDs[GuideIndex];
			OutData.StrandsCurves.CurvesClosestGuideWeights[CurveIndex] = InData.StrandsCurves.CurvesClosestGuideWeights[GuideIndex];
		}
		OutData.StrandsCurves.MaxLength = InData.StrandsCurves.MaxLength;
		OutData.StrandsCurves.MaxRadius = InData.StrandsCurves.MaxRadius;
	
		for(int32 PointIndex = 0; PointIndex < MaxPoints; ++PointIndex, ++PointCount)
		{
			const float PointSpace = GuideOffset + PointIndex * BoneSpace;
	
			const int32 PointBegin = FMath::Min(FMath::FloorToInt32(PointSpace), GuideOffset + GuideCount-2);
			const int32 PointEnd = PointBegin+1;
	
			const float PointAlpha = PointSpace - PointBegin;
			
			OutData.StrandsPoints.PointsPosition[PointCount] = InData.StrandsPoints.PointsPosition[PointBegin] * (1.0-PointAlpha) +
				InData.StrandsPoints.PointsPosition[PointEnd] * PointAlpha;
			OutData.StrandsPoints.PointsCoordU[PointCount] = InData.StrandsPoints.PointsCoordU[PointBegin] * (1.0-PointAlpha) +
				InData.StrandsPoints.PointsCoordU[PointEnd] * PointAlpha;
			OutData.StrandsPoints.PointsRadius[PointCount] = InData.StrandsPoints.PointsRadius[PointBegin] * (1.0-PointAlpha) +
				InData.StrandsPoints.PointsRadius[PointEnd] * PointAlpha;
			OutData.StrandsPoints.PointsBaseColor[PointCount] = InData.StrandsPoints.PointsBaseColor[PointBegin] * (1.0-PointAlpha) +
				InData.StrandsPoints.PointsBaseColor[PointEnd] * PointAlpha;
			OutData.StrandsPoints.PointsRoughness[PointCount] = InData.StrandsPoints.PointsRoughness[PointBegin] * (1.0-PointAlpha) +
				InData.StrandsPoints.PointsRoughness[PointEnd] * PointAlpha;
		}
	}
	HairStrandsBuilder::BuildInternalData(OutData, false);
}
	
void Decimate(
	const FHairStrandsDatas& InData, 
	float CurveDecimationPercentage, 
	float VertexDecimationPercentage, 
	bool bContinuousDecimationReordering,
	FHairStrandsDatas& OutData)
{
	CurveDecimationPercentage = FMath::Clamp(CurveDecimationPercentage, 0.f, 1.f);
	VertexDecimationPercentage = FMath::Clamp(VertexDecimationPercentage, 0.f, 1.f);

	// Pick randomly strand as guide 
	// Divide strands in buckets and pick randomly one stand per bucket
	const uint32 InCurveCount = InData.StrandsCurves.Num();
	const uint32 OutCurveCount = FMath::Clamp(uint32(InCurveCount * CurveDecimationPercentage), 1u, InCurveCount);
	const bool bHasPrecomputedWeights = InData.StrandsCurves.CurvesClosestGuideIDs.Num() > 0 && InData.StrandsCurves.CurvesClosestGuideWeights.Num() > 0;

	TArray<uint32> CurveIndices;
	CurveIndices.SetNum(OutCurveCount);

	uint32 OutTotalPointCount = 0;
	FRandomStream Random;

	TArray<uint32> CurveRandomizedIndex;

	// this is the proof of concept version - which just does a shuffle - TODO: improved voxel based version which caters for both long hair and short hair
	if (bContinuousDecimationReordering)
	{
		CurveRandomizedIndex.SetNum(InCurveCount);

		Random.Initialize(0xdeedbeed);

		//initialize value
		for (uint32 CurveIndex = 0; CurveIndex < InCurveCount; CurveIndex++)
		{
			CurveRandomizedIndex[CurveIndex] = CurveIndex;
		}
		//shuffle
		for (uint32 CurveIndex = 0; CurveIndex < InCurveCount; CurveIndex++)
		{
			uint32 RandIndex = Random.RandRange(0, InCurveCount - 1);
			CurveRandomizedIndex.Swap(CurveIndex, RandIndex);
		}
	}

	const float CurveBucketSize = float(InCurveCount) / float(OutCurveCount);
	int32 LastCurveIndex = -1;
	for (uint32 BucketIndex = 0; BucketIndex < OutCurveCount; BucketIndex++)
	{
		const float MinBucket = FMath::Max(BucketIndex  * CurveBucketSize, float(LastCurveIndex+1));
		const float MaxBucket = (BucketIndex+1) * CurveBucketSize;
		const float AdjustedBucketSize = MaxBucket - MinBucket;
		if (AdjustedBucketSize > 0)
		{
			const uint32 CurveIndex = Random.RandRange(MinBucket, FMath::FloorToInt(MinBucket + AdjustedBucketSize)-1);
			CurveIndices[BucketIndex] = CurveIndex;
			LastCurveIndex = CurveIndex;
			
			const uint32 EffectiveCurveIndex = bContinuousDecimationReordering ? CurveRandomizedIndex[CurveIndex] : CurveIndex;

			const uint32 InPointCount = InData.StrandsCurves.CurvesCount[EffectiveCurveIndex];
			const uint32 OutPointCount = DecimatePointCount(InPointCount, VertexDecimationPercentage);
			OutTotalPointCount += OutPointCount;
		}
	}

	OutData.StrandsCurves.SetNum(OutCurveCount);
	OutData.StrandsPoints.SetNum(OutTotalPointCount);
	OutData.HairDensity = InData.HairDensity;

	uint32 OutPointOffset = 0; 
	for (uint32 OutCurveIndex = 0; OutCurveIndex < OutCurveCount; ++OutCurveIndex)
	{
		const uint32 InCurveIndex	= CurveIndices[OutCurveIndex];
		const uint32 EffectiveCurveIndex = bContinuousDecimationReordering ? CurveRandomizedIndex[InCurveIndex] : InCurveIndex;

		const uint32 InPointOffset	= InData.StrandsCurves.CurvesOffset[EffectiveCurveIndex];
		const uint32 InPointCount	= InData.StrandsCurves.CurvesCount[EffectiveCurveIndex];

		// Decimation using area metric
	#if 1
		TArray<uint32> OutPointIndices;
		// Don't need to DecimateCurve if there are only 2 control vertices
		if (InPointCount > 2)
		{
			DecimateCurve(
				InData.StrandsPoints.PointsPosition,
				InPointOffset,
				InPointCount,
				VertexDecimationPercentage,
				OutPointIndices);
		}
		else
		{
			// Just pass the start and end point of the curve, no decimation needed
			OutPointIndices.Add(0);
			OutPointIndices.Add(1);
		}

		const uint32 OutPointCount = OutPointIndices.Num();

		for (uint32 OutPointIndex = 0; OutPointIndex < OutPointCount; ++OutPointIndex)
		{
			const uint32 InPointIndex = OutPointIndices[OutPointIndex];

			OutData.StrandsPoints.PointsPosition [OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsPosition	[InPointIndex + InPointOffset];
			OutData.StrandsPoints.PointsCoordU	 [OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsCoordU	[InPointIndex + InPointOffset];
			OutData.StrandsPoints.PointsRadius	 [OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsRadius	[InPointIndex + InPointOffset];
			OutData.StrandsPoints.PointsBaseColor[OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsBaseColor[InPointIndex + InPointOffset];
			OutData.StrandsPoints.PointsRoughness[OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsRoughness[InPointIndex + InPointOffset];
		}
	#else
		// Decimation using uniform/interleaved removal

		// Insure always the original start/end points are part of the trimmed curve
		const uint32 OutPointCount = FMath::Clamp(uint32(InPointCount * VertexDecimationPercentage), 2u, InPointCount);
		const uint32 VertexBucketSize = InPointCount / OutPointCount;

		// Take vertex in the middle of the bucket
		const uint32 OffsetInsideBucket = FMath::FloorToInt(VertexBucketSize * 0.5f);
		for (uint32 OutPointIndex = 0; OutPointIndex < OutPointCount; ++OutPointIndex)
		{
			uint32 InPointIndex = FMath::Clamp(uint32(VertexBucketSize * OutPointIndex + OffsetInsideBucket), 0u, InPointCount - 1);

			// Insure first and last points map onto the root and tip vertices
			InPointIndex = OutPointIndex == 0 ? 0 : InPointIndex;
			InPointIndex = OutPointIndex == OutPointCount-1 ? InPointCount-1 : InPointIndex;

			OutData.StrandsPoints.PointsPosition [OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsPosition	[InPointIndex + InPointOffset];
			OutData.StrandsPoints.PointsCoordU	 [OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsCoordU	[InPointIndex + InPointOffset];
			OutData.StrandsPoints.PointsRadius	 [OutPointIndex + OutPointOffset] = InData.StrandsPoints.PointsRadius	[InPointIndex + InPointOffset];
			OutData.StrandsPoints.PointsBaseColor[OutPointIndex + OutPointOffset] = FLinearColor::Black;
			OutData.StrandsPoints.PointsRoughness[OutPointIndex + OutPointOffset] = 0;
		}
	#endif

		OutData.StrandsCurves.CurvesCount[OutCurveIndex]  = OutPointCount;
		OutData.StrandsCurves.CurvesRootUV[OutCurveIndex] = InData.StrandsCurves.CurvesRootUV[EffectiveCurveIndex];
		OutData.StrandsCurves.CurvesOffset[OutCurveIndex] = OutPointOffset;
		OutData.StrandsCurves.CurvesLength[OutCurveIndex] = InData.StrandsCurves.CurvesLength[EffectiveCurveIndex];
		if (bHasPrecomputedWeights)
		{
			OutData.StrandsCurves.CurvesClosestGuideIDs[OutCurveIndex] = InData.StrandsCurves.CurvesClosestGuideIDs[EffectiveCurveIndex];
			OutData.StrandsCurves.CurvesClosestGuideWeights[OutCurveIndex] = InData.StrandsCurves.CurvesClosestGuideWeights[EffectiveCurveIndex];
		}
		OutData.StrandsCurves.MaxLength = InData.StrandsCurves.MaxLength;
		OutData.StrandsCurves.MaxRadius = InData.StrandsCurves.MaxRadius;
		OutPointOffset += OutPointCount;
	}

	HairStrandsBuilder::BuildInternalData(OutData, false);
}
} // namespace FHairStrandsDecimation

///////////////////////////////////////////////////////////////////////////////////////////////////
// Hair voxelization
namespace GroomBuilder_Voxelization
{

FORCEINLINE uint32 ToLinearCoord(const FIntVector& T, const FIntVector& Resolution)
{
	// Morton instead for better locality?
	return T.X + T.Y * Resolution.X + T.Z * Resolution.X * Resolution.Y;
}

FORCEINLINE FIntVector ToCoord(const FVector& T, const FIntVector& Resolution, const FVector& MinBound, const float VoxelSize)
{
	const FVector C = (T - MinBound) / VoxelSize;
	return FIntVector(
		FMath::Clamp(FMath::FloorToInt(C.X), 0, Resolution.X - 1),
		FMath::Clamp(FMath::FloorToInt(C.Y), 0, Resolution.Y - 1),
		FMath::Clamp(FMath::FloorToInt(C.Z), 0, Resolution.Z - 1));
}

FORCEINLINE bool IsValid(const FIntVector& P, const FIntVector& Resolution)
{
	return	0 <= P.X && P.X < Resolution.X &&
			0 <= P.Y && P.Y < Resolution.Y &&
			0 <= P.Z && P.Z < Resolution.Z;
}

FORCEINLINE FIntVector ClampToVolume(const FIntVector& CellCoord, const FIntVector& Resolution, bool& bIsValid)
{
	bIsValid = IsValid(CellCoord, Resolution);
	return FIntVector(
		FMath::Clamp(CellCoord.X, 0, Resolution.X - 1),
		FMath::Clamp(CellCoord.Y, 0, Resolution.Y - 1),
		FMath::Clamp(CellCoord.Z, 0, Resolution.Z - 1));
}

FORCEINLINE FIntVector ToCellCoord(const FVector3f& P, const FVector3f& MinBound, const FVector4f& MaxBound, const FIntVector& Resolution)
{
	bool bIsValid = false;
	const FVector F = FVector((P - MinBound) / (MaxBound - MinBound));
	const FIntVector CellCoord = FIntVector(FMath::FloorToInt(F.X * Resolution.X), FMath::FloorToInt(F.Y * Resolution.Y), FMath::FloorToInt(F.Z * Resolution.Z));
	return ClampToVolume(CellCoord, Resolution, bIsValid);
}

FORCEINLINE uint32 ToIndex(const FIntVector& CellCoord, const FIntVector& Resolution)
{
	uint32 CellIndex = CellCoord.X + CellCoord.Y * Resolution.X + CellCoord.Z * Resolution.X * Resolution.Y;
	return CellIndex;
}

struct FHairGrid
{
	struct FCurve
	{
		FVector3f BaseColor = FVector3f::ZeroVector;
		float     Roughness = 0;
		uint32    CurveIndex = 0;
		float     Radius = 0;
		uint8     GroupIndex = 0;
	};

	struct FVoxel
	{
		TArray<FCurve> VoxelCurves;
	};

	float VoxelSize;
	FVector3f MinBound;
	FVector3f MaxBound;
	FIntVector Resolution;
	TArray<FVoxel> Voxels;
};

static void Voxelize(const FHairDescriptionGroups& InGroups, FHairGrid& Out)
{
	// 1. Compute the overal bound of for all hair groups
	Out.MinBound = InGroups.Bounds.GetBox().Min;
	Out.MaxBound = InGroups.Bounds.GetBox().Max;
	Out.Resolution = FIntVector::ZeroValue;

	// 2. Based on the bound, determine the voxel grid resolution
	Out.VoxelSize = 0.5f; // 5mm
	{
		const int32 MaxResolution = FMath::Max(GHairGroupIndexBuilder_MaxVoxelResolution, 2);

		bool bIsValid = false;
		while (!bIsValid)
		{
			FVector3f VoxelResolutionF = (Out.MaxBound - Out.MinBound) / Out.VoxelSize;
			Out.Resolution = FIntVector(FMath::CeilToInt(VoxelResolutionF.X), FMath::CeilToInt(VoxelResolutionF.Y), FMath::CeilToInt(VoxelResolutionF.Z));
			bIsValid = Out.Resolution.X <= MaxResolution && Out.Resolution.Y <= MaxResolution && Out.Resolution.Z <= MaxResolution;
			if (!bIsValid)
			{
				Out.VoxelSize *= 2;
			}
		}
		Out.MaxBound = Out.MinBound + FVector3f(Out.Resolution) * Out.VoxelSize;
	}
	Out.Voxels.SetNum(FMath::Max(Out.Resolution.X * Out.Resolution.Y * Out.Resolution.Z, 1));

	// 3. Voxelization curves
	uint32 GroupIndex = 0;
	for (const FHairDescriptionGroup& Group : InGroups.HairGroups)
	{
		// Local copy of the hair strands data, and build the derived data so that curve offset are available
		FHairStrandsDatas In = Group.Strands;
		HairStrandsBuilder::BuildInternalData(In, false);

		// Fill in voxel (TODO: make it parallel)
		for (uint32 CurveIt = 0, CurveCount = In.GetNumCurves(); CurveIt < CurveCount; ++CurveIt)
		{
			const uint32 PointOffset = In.StrandsCurves.CurvesOffset[CurveIt];
			const uint32 PointCount = In.StrandsCurves.CurvesCount[CurveIt];

			uint32 PrevLinearCoord = ~0;
			for (uint32 PointIndex = 0; PointIndex < PointCount - 1; ++PointIndex)
			{
				const uint32 Index0 = PointOffset + PointIndex;
				const uint32 Index1 = PointOffset + PointIndex + 1;
				const FVector3f& P0 = In.StrandsPoints.PointsPosition[Index0];
				const FVector3f& P1 = In.StrandsPoints.PointsPosition[Index1];
				const float R0 = In.StrandsPoints.PointsRadius[Index0];
				const float R1 = In.StrandsPoints.PointsRadius[Index1];
				const FVector3f C0(In.StrandsPoints.PointsBaseColor[Index0].R, In.StrandsPoints.PointsBaseColor[Index0].G, In.StrandsPoints.PointsBaseColor[Index0].B);
				const FVector3f C1(In.StrandsPoints.PointsBaseColor[Index1].R, In.StrandsPoints.PointsBaseColor[Index1].G, In.StrandsPoints.PointsBaseColor[Index1].B);
				const float Rough0 = In.StrandsPoints.PointsRoughness[Index0];
				const float Rough1 = In.StrandsPoints.PointsRoughness[Index1];
				const FVector3f Segment = P1 - P0;

				// This is a coarse/non-conservative voxelization, by ray-marching segment, instead of walking intersected voxels
				const float Length = Segment.Size();
				const uint32 StepCount = FMath::CeilToInt(Length / Out.VoxelSize);
				for (uint32 StepIt = 0; StepIt < StepCount + 1; ++StepIt)
				{
					const float T = float(StepIt) / float(StepCount);
					const FVector3f P = P0 + Segment * T;
					const FIntVector Coord = ToCoord((FVector)P, Out.Resolution, (FVector)Out.MinBound, Out.VoxelSize);
					const uint32 LinearCoord = ToLinearCoord(Coord, Out.Resolution);
					if (LinearCoord != PrevLinearCoord)
					{
						FHairGrid::FCurve RCurve;
						RCurve.CurveIndex = CurveIt;
						RCurve.GroupIndex = GroupIndex;
						RCurve.Radius = FMath::Lerp(R0, R1, T);
						RCurve.BaseColor = FVector3f(FMath::Lerp(C0.X, C1.X, T), FMath::Lerp(C0.Y, C1.Y, T), FMath::Lerp(C0.Z, C1.Z, T));
						RCurve.Roughness = FMath::Lerp(Rough0, Rough1, T);
						Out.Voxels[LinearCoord].VoxelCurves.Add(RCurve);

						PrevLinearCoord = LinearCoord;
					}
				}
			}
		}

		++GroupIndex;
	}
}

FORCEINLINE void SearchCell(const FHairStrandsVoxelData& In, const FIntVector& C, FHairStrandsVoxelData::FData& Out)
{
	const uint32 I = GroomBuilder_Voxelization::ToIndex(C, In.Resolution);
	if (In.Datas[I].GroupIndex != FHairStrandsVoxelData::InvalidGroupIndex)
	{
		Out = In.Datas[I];
	}
}

} // namespace GroomBuilder_Voxelization

FHairStrandsVoxelData::FData FHairStrandsVoxelData::GetData(const FVector3f& P) const
{
	const uint32 MaxLookupDistance = FMath::Max3(Resolution.X, Resolution.Y, Resolution.Z) * 0.5f;
	const FIntVector C = GroomBuilder_Voxelization::ToCellCoord(P, MinBound, MaxBound, Resolution);

	FHairStrandsVoxelData::FData Out;
	Out.GroupIndex = FHairStrandsVoxelData::InvalidGroupIndex;
	Out.BaseColor = FVector3f::ZeroVector;
	Out.Roughness = 0.f;
	for (int32 Offset = 1; Offset <= int32(MaxLookupDistance); ++Offset)
	{
		// Center
		{
			bool bIsValid = false;
			const FIntVector CellCoord = GroomBuilder_Voxelization::ClampToVolume(C, Resolution, bIsValid);
			if (bIsValid) GroomBuilder_Voxelization::SearchCell(*this, CellCoord, Out);
		}

		// Top & Bottom
		for (int32 X = -Offset; X <= Offset; ++X)
		for (int32 Y = -Offset; Y <= Offset; ++Y)
		{
			bool bIsValid0 = false, bIsValid1 = false;
			const FIntVector CellCoord0 = GroomBuilder_Voxelization::ClampToVolume(C + FIntVector(X, Y, Offset), Resolution, bIsValid0);
			const FIntVector CellCoord1 = GroomBuilder_Voxelization::ClampToVolume(C + FIntVector(X, Y, -Offset), Resolution, bIsValid1);
			if (bIsValid0) GroomBuilder_Voxelization::SearchCell(*this, CellCoord0, Out);
			if (bIsValid1) GroomBuilder_Voxelization::SearchCell(*this, CellCoord1, Out);
		}

		const int32 OffsetMinusOne = Offset - 1;
		// Front & Back
		for (int32 X = -Offset; X <= Offset; ++X)
		for (int32 Z = -OffsetMinusOne; Z <= OffsetMinusOne; ++Z)
		{
			bool bIsValid0 = false, bIsValid1 = false;
			const FIntVector CellCoord0 = GroomBuilder_Voxelization::ClampToVolume(C + FIntVector(X, Offset, Z), Resolution, bIsValid0);
			const FIntVector CellCoord1 = GroomBuilder_Voxelization::ClampToVolume(C + FIntVector(X, -Offset, Z), Resolution, bIsValid1);
			if (bIsValid0) GroomBuilder_Voxelization::SearchCell(*this, CellCoord0, Out);
			if (bIsValid1) GroomBuilder_Voxelization::SearchCell(*this, CellCoord1, Out);
		}

		// Left & Right
		for (int32 Y = -OffsetMinusOne; Y <= OffsetMinusOne; ++Y)
		for (int32 Z = -OffsetMinusOne; Z <= OffsetMinusOne; ++Z)
		{
			bool bIsValid0 = false, bIsValid1 = false;
			const FIntVector CellCoord0 = GroomBuilder_Voxelization::ClampToVolume(C + FIntVector(Offset, Y, Z), Resolution, bIsValid0);
			const FIntVector CellCoord1 = GroomBuilder_Voxelization::ClampToVolume(C + FIntVector(-Offset, Y, Z), Resolution, bIsValid1);
			if (bIsValid0) GroomBuilder_Voxelization::SearchCell(*this, CellCoord0, Out);
			if (bIsValid1) GroomBuilder_Voxelization::SearchCell(*this, CellCoord1, Out);
		}

		// Early out if we have found a valid group index during a ring/layer step.
		if (Out.GroupIndex != InvalidGroupIndex)
		{
			break;
		}
	}

	// No valid group index has been found. In this case force the group index to 0
	check(Out.GroupIndex != FHairStrandsVoxelData::InvalidGroupIndex);
	if (Out.GroupIndex == FHairStrandsVoxelData::InvalidGroupIndex) { Out.GroupIndex = 0; }

	return Out;
}

void FGroomBuilder::VoxelizeGroupIndex(const FHairDescriptionGroups& In, FHairStrandsVoxelData& Out)
{
	GroomBuilder_Voxelization::FHairGrid Grid;
	GroomBuilder_Voxelization::Voxelize(In, Grid);
	Out.MinBound = Grid.MinBound;
	Out.MaxBound = Grid.MaxBound;
	Out.Resolution = Grid.Resolution;
	Out.Datas.SetNum(Grid.Voxels.Num());

	TArray<uint16> GroupBins;
	for (uint32 I=0,VoxelCount=Grid.Voxels.Num();I<VoxelCount;++I)
	{
		Out.Datas[I].GroupIndex = FHairStrandsVoxelData::InvalidGroupIndex;
		Out.Datas[I].BaseColor = FVector3f::ZeroVector;
		Out.Datas[I].Roughness = 0.f;

		// 1. For a given voxel, build an histogram of all the groups falling into this voxel
		GroupBins.Init(0, 64u);
		for (const GroomBuilder_Voxelization::FHairGrid::FCurve& Curve : Grid.Voxels[I].VoxelCurves)
		{
			check(Curve.GroupIndex != FHairStrandsVoxelData::InvalidGroupIndex);
			check(Curve.GroupIndex < GroupBins.Num());
			GroupBins[Curve.GroupIndex]++;
			Out.Datas[I].BaseColor += Curve.BaseColor;
			Out.Datas[I].Roughness += Curve.Roughness;
		}

		const uint32 CurveCount = Grid.Voxels[I].VoxelCurves.Num();
		if (CurveCount > 0)
		{
			Out.Datas[I].BaseColor /= float(CurveCount);
			Out.Datas[I].Roughness /= float(CurveCount);
		}

		// 2. Select the hair group having the largest count
		if (CurveCount > 1)
		{
			uint32 MaxBinCount = 0u;
			for (uint8 GroupIndex = 0, GroupCount = GroupBins.Num(); GroupIndex < GroupCount; ++GroupIndex)
			{
				if (GroupBins[GroupIndex] > MaxBinCount)
				{
					Out.Datas[I].GroupIndex = GroupIndex;
					MaxBinCount = GroupBins[GroupIndex];
				}
			}
		}
		else if (CurveCount == 1)
		{
			Out.Datas[I].GroupIndex = Grid.Voxels[I].VoxelCurves[0].GroupIndex;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Cluster culling data

namespace GroomBuilder_Cluster
{
struct FClusterGrid
{
	struct FCurve
	{
		FCurve()
		{
			for (uint8 LODIt = 0; LODIt < FHairClusterInfo::MaxLOD; ++LODIt)
			{
				CountPerLOD[LODIt] = 0;
			}
		}
		uint32 Offset = 0;
		uint32 Count = 0;
		float Area = 0;
		float AvgRadius = 0;
		float MaxRadius = 0;
		uint32 CountPerLOD[FHairClusterInfo::MaxLOD];
	};

	struct FCluster
	{
		float CurveAvgRadius = 0;
		float CurveMaxRadius = 0;
		float RootBoundRadius = 0;
		float Area = 0;
		TArray<FCurve> ClusterCurves;
	};

	FClusterGrid(const FIntVector& InResolution, const FVector& InMinBound, const FVector& InMaxBound)
	{
		MinBound = InMinBound;
		MaxBound = InMaxBound;
		GridResolution = InResolution;
		Clusters.SetNum(FMath::Max(GridResolution.X * GridResolution.Y * GridResolution.Z, 1));
	}

	FORCEINLINE bool IsValid(const FIntVector& P) const
	{
		return	0 <= P.X && P.X < GridResolution.X &&
			0 <= P.Y && P.Y < GridResolution.Y &&
			0 <= P.Z && P.Z < GridResolution.Z;
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
		check(CellIndex < uint32(Clusters.Num()));
		return CellIndex;
	}

	void InsertRenderingCurve(FCurve& Curve, const FVector& Root)
	{
		FIntVector CellCoord = ToCellCoord(Root);
		uint32 Index = ToIndex(CellCoord);
		FCluster& Cluster = Clusters[Index];
		Cluster.ClusterCurves.Add(Curve);
	}

	FVector MinBound;
	FVector MaxBound;
	FIntVector GridResolution;
	TArray<FCluster> Clusters;
};

static void DecimateCurve(
	const TArray<FVector3f>& InPoints,
	const uint32 InOffset,
	const uint32 InCount,
	const TArray<FHairLODSettings>& InSettings,
	uint32* OutCountPerLOD,
	TArray<uint8>& OutVertexLODMask)
{
	// Insure that all settings have more and more agressive, and rectify it is not the case.
	TArray<FHairLODSettings> Settings = InSettings;
	{
		float PrevFactor = 1;
		float PrevAngle = 0;
		for (FHairLODSettings& S : Settings)
		{
			// Sanitize the decimation values
			S.CurveDecimation  = FMath::Clamp(S.CurveDecimation,  0.f,  1.f);
			S.VertexDecimation = FMath::Clamp(S.VertexDecimation, 0.f,  1.f);
			S.AngularThreshold = FMath::Clamp(S.AngularThreshold, 0.f, 90.f);

			if (S.VertexDecimation > PrevFactor)
			{
				S.VertexDecimation = PrevFactor;
			}

			if (S.AngularThreshold < PrevAngle)
			{
				S.AngularThreshold = PrevAngle;
			}

			PrevFactor = S.VertexDecimation;
			PrevAngle = S.AngularThreshold;
		}
	}

	check(InCount > 2);

	// Array containing the remaining vertex indices. This list get trimmed down as we process over all LODs.
	TArray<uint32> OutIndices;
	OutIndices.SetNum(InCount);
	for (uint32 CurveIt = 0; CurveIt < InCount; ++CurveIt)
	{
		OutIndices[CurveIt] = CurveIt;
	}

	const uint32 LODCount = Settings.Num();
	check(LODCount <= FHairClusterInfo::MaxLOD);

	for (uint8 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		const int32 LODTargetVertexCount = FMath::Clamp(uint32(FMath::CeilToInt(InCount * Settings[LODIt].VertexDecimation)), 2u, InCount);
		const float LODAngularThreshold  = FMath::DegreesToRadians(Settings[LODIt].AngularThreshold);

		// 'bCanDecimate' tracks if it is possible to reduce the remaining vertives even more while respecting the user angular constrain
		bool bCanDecimate = true;
		while (OutIndices.Num() > LODTargetVertexCount && bCanDecimate)
		{
			float MinError = FLT_MAX;
			int32 ElementToRemove = -1;
			const uint32 Count = OutIndices.Num();
			for (uint32 IndexIt = 1; IndexIt < Count - 1; ++IndexIt)
			{
				const FVector& P0 = (FVector)InPoints[InOffset + OutIndices[IndexIt - 1]];
				const FVector& P1 = (FVector)InPoints[InOffset + OutIndices[IndexIt]];
				const FVector& P2 = (FVector)InPoints[InOffset + OutIndices[IndexIt + 1]];

				const float Area = FVector::CrossProduct(P0 - P1, P2 - P1).Size() * 0.5f;

				//     P0 .       . P2
				//         \Inner/
				//   ` .    \   /
				// Thres(` . \^/ ) Angle
				//    --------.---------
				//            P1
				const FVector V0 = (P0 - P1).GetSafeNormal();
				const FVector V1 = (P2 - P1).GetSafeNormal();
				const float InnerAngle = FMath::Abs(FMath::Acos(FVector::DotProduct(V0, V1)));
				const float Angle = (PI - InnerAngle) * 0.5f;

				if (Area < MinError && Angle < LODAngularThreshold)
				{
					MinError = Area;
					ElementToRemove = IndexIt;
				}
			}
			bCanDecimate = ElementToRemove >= 0;
			if (bCanDecimate)
			{
				OutIndices.RemoveAt(ElementToRemove);
			}
		}

		OutCountPerLOD[LODIt] = OutIndices.Num();

		// For all remaining vertices, we mark them as 'used'/'valid' for the current LOD levl
		for (uint32 LocalIndex : OutIndices)
		{
			const uint32 VertexIndex = InOffset + LocalIndex;
			OutVertexLODMask[VertexIndex] |= 1 << LODIt;
		}
	}

	// Sanity check to insure that vertex LOD in a continuous fashion.
	for (uint32 VertexIt = 0; VertexIt < InCount; ++VertexIt)
	{
		const uint8 Mask = OutVertexLODMask[InOffset + VertexIt];
		check(Mask == 0 || Mask == 1 || Mask == 3 || Mask == 7 || Mask == 15 || Mask == 31 || Mask == 63 || Mask == 127 || Mask == 255);
	}
}


inline uint32 to10Bits(float V)
{
	return FMath::Clamp(uint32(V * 1024), 0u, 1023u);
}

static void BuildClusterData(
	const FHairStrandsDatas& InRenStrandsData,
	const float InGroomAssetRadius, 
	const FHairGroupsLOD& InSettings, 
	FHairStrandsClusterCullingData& Out)
{
	// 0. Rest existing culling data
	Out.Reset();

	const uint32 LODCount = FMath::Min(uint32(InSettings.LODs.Num()), FHairClusterInfo::MaxLOD);
	check(LODCount > 0);

	const uint32 RenCurveCount = InRenStrandsData.GetNumCurves();
	Out.VertexCount = InRenStrandsData.GetNumPoints();
	check(Out.VertexCount);

	// 1. Allocate cluster per voxel containing contains >=1 render curve root
	const FVector GroupMinBound = InRenStrandsData.BoundingBox.Min;
	FVector GroupMaxBound = InRenStrandsData.BoundingBox.Max;
	const float GroupRadius = FVector::Distance(GroupMaxBound, GroupMinBound) * 0.5f;

	// Compute the voxel volume resolution, and snap the max bound to the voxel grid
	// Iterate until voxel size are below max resolution, so that computation is not too long
	FIntVector VoxelResolution = FIntVector::ZeroValue;
	{
		const int32 MaxResolution = FMath::Max(GHairClusterBuilder_MaxVoxelResolution, 2);

		float ClusterWorldSize = FMath::Max(InSettings.ClusterWorldSize, 0.001f);
		bool bIsValid = false;
		while (!bIsValid)
		{
			FVector VoxelResolutionF = (GroupMaxBound - GroupMinBound) / ClusterWorldSize;
			VoxelResolution = FIntVector(FMath::CeilToInt(VoxelResolutionF.X), FMath::CeilToInt(VoxelResolutionF.Y), FMath::CeilToInt(VoxelResolutionF.Z));
			bIsValid = VoxelResolution.X <= MaxResolution && VoxelResolution.Y <= MaxResolution && VoxelResolution.Z <= MaxResolution;
			if (!bIsValid)
			{
				ClusterWorldSize *= 2;
			}
		}
		GroupMaxBound = GroupMinBound + FVector(VoxelResolution) * ClusterWorldSize;
	}

	// 2. Insert all rendering curves into the voxel structure
	FClusterGrid ClusterGrid(VoxelResolution, GroupMinBound, GroupMaxBound);
	for (uint32 RenCurveIndex = 0; RenCurveIndex < RenCurveCount; ++RenCurveIndex)
	{
		FClusterGrid::FCurve RCurve;
		RCurve.Count = InRenStrandsData.StrandsCurves.CurvesCount[RenCurveIndex];
		RCurve.Offset = InRenStrandsData.StrandsCurves.CurvesOffset[RenCurveIndex];
		RCurve.Area = 0.0f;
		RCurve.AvgRadius = 0;
		RCurve.MaxRadius = 0;

		// Compute area of each curve to later compute area correction
		for (uint32 RenPointIndex = 0; RenPointIndex < RCurve.Count; ++RenPointIndex)
		{
			uint32 PointGlobalIndex = RenPointIndex + RCurve.Offset;
			const FVector& V0 = (FVector)InRenStrandsData.StrandsPoints.PointsPosition[PointGlobalIndex];
			if (RenPointIndex > 0)
			{
				const FVector& V1 = (FVector)InRenStrandsData.StrandsPoints.PointsPosition[PointGlobalIndex - 1];
				FVector OutDir;
				float OutLength;
				(V1 - V0).ToDirectionAndLength(OutDir, OutLength);
				RCurve.Area += InRenStrandsData.StrandsPoints.PointsRadius[PointGlobalIndex] * OutLength;
			}

			const float PointRadius = InRenStrandsData.StrandsPoints.PointsRadius[PointGlobalIndex] * InRenStrandsData.StrandsCurves.MaxRadius;
			RCurve.AvgRadius += PointRadius;
			RCurve.MaxRadius = FMath::Max(RCurve.MaxRadius, PointRadius);
		}
		RCurve.AvgRadius /= FMath::Max(1u, RCurve.Count);

		const FVector Root = (FVector)InRenStrandsData.StrandsPoints.PointsPosition[RCurve.Offset];
		ClusterGrid.InsertRenderingCurve(RCurve, Root);
	}

	// 3. Count non-empty clusters
	TArray<uint32> ValidClusterIndices;
	{
		uint32 GridLinearIndex = 0;
		ValidClusterIndices.Reserve(ClusterGrid.Clusters.Num() * 0.2);
		for (FClusterGrid::FCluster& Cluster : ClusterGrid.Clusters)
		{
			if (Cluster.ClusterCurves.Num() > 0)
			{
				ValidClusterIndices.Add(GridLinearIndex);
			}
			++GridLinearIndex;
		}
	}
	Out.ClusterCount = ValidClusterIndices.Num();
	Out.ClusterInfos.Init(FHairClusterInfo(), Out.ClusterCount);
	Out.VertexToClusterIds.SetNum(Out.VertexCount);

	// Conservative allocation for inserting vertex indices for the various curves LOD
	uint32* RawClusterVertexIds = new uint32[LODCount * InRenStrandsData.GetNumPoints()];
	TAtomic<uint32> RawClusterVertexCount(0);

	// 4. Write out cluster information
	Out.ClusterLODInfos.SetNum(LODCount * Out.ClusterCount);
	TArray<uint8> VertexLODMasks;
	VertexLODMasks.SetNum(InRenStrandsData.GetNumPoints());

	// Local variable for being capture by the lambda
	TArray<FHairClusterInfo>& LocalClusterInfos = Out.ClusterInfos;
	TArray<FHairClusterLODInfo>& LocalClusterLODInfos = Out.ClusterLODInfos;
	TArray<uint32>& LocalVertexToClusterIds = Out.VertexToClusterIds;
#define USE_PARALLE_FOR 1
#if USE_PARALLE_FOR
	ParallelFor(Out.ClusterCount,
		[
			LODCount,
			InGroomAssetRadius,
			InSettings,
			&ValidClusterIndices,
			&InRenStrandsData,
			&ClusterGrid,
			&LocalClusterInfos,
			&LocalClusterLODInfos,
			&LocalVertexToClusterIds,
			&VertexLODMasks,
			&RawClusterVertexIds,
			&RawClusterVertexCount
		]
	(uint32 ClusterIt)
#else
	for (uint32 ClusterIt = 0; ClusterIt < Out.ClusterCount; ++ClusterIt)
#endif
	{
		const uint32 GridLinearIndex = ValidClusterIndices[ClusterIt];
		FClusterGrid::FCluster& Cluster = ClusterGrid.Clusters[GridLinearIndex];
		check(Cluster.ClusterCurves.Num() != 0);

		// 4.1 Sort curves
		// Sort curve to have largest area first, so that lower area curves with less influence are removed first.
		// This also helps the radius scaling to not explode.
		Cluster.ClusterCurves.Sort([](const FClusterGrid::FCurve& A, const FClusterGrid::FCurve& B) -> bool
			{
				return A.Area > B.Area;
			});

		// 4.2 Compute cluster's area & fill in the vertex to cluster ID mapping
		float ClusterArea = 0;
		FVector ClusterMinBound(FLT_MAX);
		FVector ClusterMaxBound(-FLT_MAX);

		FVector RootMinBound(FLT_MAX);
		FVector RootMaxBound(-FLT_MAX);

		Cluster.CurveMaxRadius = 0;
		Cluster.CurveAvgRadius = 0;
		Cluster.Area = 0;
		for (FClusterGrid::FCurve& ClusterCurve : Cluster.ClusterCurves)
		{
			for (uint32 RenPointIndex = 0; RenPointIndex < ClusterCurve.Count; ++RenPointIndex)
			{
				const uint32 PointGlobalIndex = RenPointIndex + ClusterCurve.Offset;
				LocalVertexToClusterIds[PointGlobalIndex] = ClusterIt;

				const FVector& P = (FVector)InRenStrandsData.StrandsPoints.PointsPosition[PointGlobalIndex];
				{
					ClusterMinBound.X = FMath::Min(ClusterMinBound.X, P.X);
					ClusterMinBound.Y = FMath::Min(ClusterMinBound.Y, P.Y);
					ClusterMinBound.Z = FMath::Min(ClusterMinBound.Z, P.Z);

					ClusterMaxBound.X = FMath::Max(ClusterMaxBound.X, P.X);
					ClusterMaxBound.Y = FMath::Max(ClusterMaxBound.Y, P.Y);
					ClusterMaxBound.Z = FMath::Max(ClusterMaxBound.Z, P.Z);
				}

				if (RenPointIndex == 0)
				{
					RootMinBound.X = FMath::Min(RootMinBound.X, P.X);
					RootMinBound.Y = FMath::Min(RootMinBound.Y, P.Y);
					RootMinBound.Z = FMath::Min(RootMinBound.Z, P.Z);

					RootMaxBound.X = FMath::Max(RootMaxBound.X, P.X);
					RootMaxBound.Y = FMath::Max(RootMaxBound.Y, P.Y);
					RootMaxBound.Z = FMath::Max(RootMaxBound.Z, P.Z);
				}
			}
			Cluster.CurveMaxRadius = FMath::Max(Cluster.CurveMaxRadius, ClusterCurve.MaxRadius);
			Cluster.CurveAvgRadius += ClusterCurve.AvgRadius;
			Cluster.Area += ClusterCurve.Area;
		}
		Cluster.CurveAvgRadius /= FMath::Max(1, Cluster.ClusterCurves.Num());
		Cluster.RootBoundRadius = (RootMaxBound - RootMinBound).GetMax() * 0.5f + Cluster.CurveAvgRadius;

		// Compute the max radius that a cluster can have. This is done by computing an estimate of the cluster coverage (using pre-computed LUT) 
		// and computing how much is visible
		// This supposes the radius is proportional to the radius of the roots bounding volume
		const float NormalizedAvgRadius = Cluster.CurveAvgRadius / Cluster.RootBoundRadius;
		const float ClusterCoverage = GetHairCoverage(Cluster.ClusterCurves.Num(), NormalizedAvgRadius);
		const float ClusterVisibleRadius = Cluster.RootBoundRadius * ClusterCoverage;

		const float ClusterRadius = FVector::Distance(ClusterMaxBound, ClusterMinBound) * 0.5f;


		// 4.3 Compute the number of curve per LOD
		// Compute LOD infos (vertx count, vertex offset, radius scale ...)
		// Compute the ratio of the cluster related the actual groom and scale the screen size accordingly
		TArray<uint32> LODCurveCount;
		LODCurveCount.SetNum(LODCount);
		for (uint8 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			const float CurveDecimation = FMath::Clamp(InSettings.LODs[LODIt].CurveDecimation, 0.0f, 1.0f);
			LODCurveCount[LODIt] = FMath::Clamp(FMath::CeilToInt(Cluster.ClusterCurves.Num() * CurveDecimation), 1, Cluster.ClusterCurves.Num());
		}

		// 4.4 Decimate each curve for all LODs
		// This fill in a bitfield per vertex which indiates on which LODs a vertex can be used
		for (uint32 CurveIt = 0, CurveCount = uint32(Cluster.ClusterCurves.Num()); CurveIt < CurveCount; ++CurveIt)
		{
			FClusterGrid::FCurve& ClusterCurve = Cluster.ClusterCurves[CurveIt];

			// Don't need to DecimateCurve if there are only 2 control vertices
			if (ClusterCurve.Count > 2)
			{
				DecimateCurve(
					InRenStrandsData.StrandsPoints.PointsPosition,
					ClusterCurve.Offset,
					ClusterCurve.Count,
					InSettings.LODs,
					ClusterCurve.CountPerLOD,
					VertexLODMasks);
			}
		}

		// 4.5 Record/Insert vertex indices for each LOD of the current cluster
		// Vertex offset is stored into the cluster LOD info
		// Stores the accumulated vertex count per LOD
		//
		// ClusterVertexIds contains the vertex index of curve belonging to a cluster.
		// Since for a given LOD, both the number of curve and vertices varies, we stores 
		// this information per LOD.
		//
		//  Global Vertex index
		//            v
		// ||0 1 2 3 4 5 6 7 8 9 ||0 1 3 5 7 9 ||0 5 9 | |0 1 2 3 4 5 6 7 || 0 1 5 7 ||0 9 ||||11 12 ...
		// ||____________________||____________||______| |________________||_________||____||||_____ _ _ 
		// ||        LOD 0			 LOD 1		 LOD2  | |    LOD 0			 LOD 1	  LOD2 ||||  LOD 0
		// ||__________________________________________| | ________________________________||||_____ _ _ 
		// |                   Curve 0								Curve 1				    ||   Curve 0
		// |________________________________________________________________________________||_____ _ _ 
		//										Cluster 0										Cluster 1

		TArray<uint32> LocalClusterVertexIds;
		LocalClusterVertexIds.Reserve(LODCount * Cluster.ClusterCurves.Num() * 32); // Guestimate pre-allocation (32 points per curve in average)

		FHairClusterInfo& ClusterInfo = LocalClusterInfos[ClusterIt];
		ClusterInfo.LODCount = LODCount;
		ClusterInfo.LODInfoOffset = LODCount * ClusterIt;
		for (uint8 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			FHairClusterLODInfo& ClusterLODInfo = LocalClusterLODInfos[ClusterInfo.LODInfoOffset + LODIt];
			ClusterLODInfo.VertexOffset = LocalClusterVertexIds.Num(); // At the end, it will be the offset at which the data are inserted into ClusterVertexIds
			ClusterLODInfo.VertexCount0 = 0;
			ClusterLODInfo.VertexCount1 = 0;
			ClusterLODInfo.RadiusScale0 = 0;
			ClusterLODInfo.RadiusScale1 = 0;

			const uint32 CurveCount = LODCurveCount[LODIt];
			const uint32 NextCurveCount = LODIt < LODCount - 1 ? LODCurveCount[LODIt + 1] : CurveCount;
			for (uint32 CurveIt = 0; CurveIt < CurveCount; ++CurveIt)
			{
				FClusterGrid::FCurve& ClusterCurve = Cluster.ClusterCurves[CurveIt];

				for (uint32 PointIt = 0; PointIt < ClusterCurve.Count; ++PointIt)
				{
					const uint32 GlobalPointIndex = PointIt + ClusterCurve.Offset;
					const uint8 LODMask = VertexLODMasks[GlobalPointIndex];
					if (LODMask & (1 << LODIt))
					{
						// Count the number of vertices for all curves in the cluster as well as the vertex 
						// of the remaining curves once the cluster has been decimated with the current LOD 
						// settings
						++ClusterLODInfo.VertexCount0;
						if (CurveIt < NextCurveCount)
						{
							++ClusterLODInfo.VertexCount1;
						}

						LocalClusterVertexIds.Add(GlobalPointIndex);
					}
				}
			}
		}

		// 4.5.1 Insert vertex indices for each LOD into the final array
		// Since this runs in parallel, we prefill LocalClusterVertexIds with 
		// all indices, then we insert the indices into the final array with a single allocation + memcopy
		// We also patch the vertex offset so that it is correct
		const uint32 AllocOffset = RawClusterVertexCount.AddExchange(LocalClusterVertexIds.Num());
		FMemory::Memcpy(RawClusterVertexIds + AllocOffset, LocalClusterVertexIds.GetData(), LocalClusterVertexIds.Num() * sizeof(uint32));
		for (uint8 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			FHairClusterLODInfo& ClusterLODInfo = LocalClusterLODInfos[ClusterInfo.LODInfoOffset + LODIt];
			ClusterLODInfo.VertexOffset += AllocOffset;
		}

		// 4.6 Compute the radius scaling to preserve the cluster apperance as we decimate 
		// the number of strands
		for (uint8 LODIt = 0; LODIt < LODCount; ++LODIt)
		{
			// Compute the visible area for various orientation? 
			// Reference: Stochastic Simplification of Aggregate Detail
			float  LODArea = 0;
			float  LODAvgRadiusRef = 0;
			float  LODMaxRadiusRef = 0;
			uint32 LODVertexCount = 0;

			const uint32 ClusterCurveCount = LODCurveCount[LODIt];
			for (uint32 CurveIt = 0; CurveIt < ClusterCurveCount; ++CurveIt)
			{
				const FClusterGrid::FCurve& ClusterCurve = Cluster.ClusterCurves[CurveIt];
				LODVertexCount += ClusterCurve.Count;
				LODArea += ClusterCurve.Area;
				LODAvgRadiusRef += ClusterCurve.AvgRadius;
				LODMaxRadiusRef = FMath::Max(LODMaxRadiusRef, ClusterCurve.MaxRadius);
			}
			LODAvgRadiusRef /= ClusterCurveCount;

			// Compute what should be the average (normalized) radius of the strands, and scale it 
			// with the radius of the clusters/roots to get an actual world radius.
			const float LODAvgRadiusTarget = Cluster.RootBoundRadius * GetHairAvgRadius(ClusterCurveCount, ClusterCoverage);

			// Compute the ratio between the size of the cluster and the size of the groom (at rest position)
			// On the GPU, we compute the screen size of the cluster, and use the LOD screensize to know which 
			// LOD needs to be pick up. Since the screen area are setup by artists based the entire groom (not 
			// based on the cluster size), we precompute the correcting ratio here, and pre-scale the LOD screensize
			const float ScreenSizeScale = InSettings.ClusterScreenSizeScale * ClusterRadius / InGroomAssetRadius;

			float LODScale = LODAvgRadiusTarget / LODAvgRadiusRef;
			if (LODMaxRadiusRef * LODScale > ClusterVisibleRadius)
			{
				LODScale = FMath::Max(LODMaxRadiusRef, ClusterVisibleRadius) / LODMaxRadiusRef;
			}
			LODScale *= FMath::Max(InSettings.LODs[LODIt].ThicknessScale, 0.f);
			//if (LODMaxRadiusRef * LODScale > Cluster.RootBoundRadius)
			//{
			//	LODScale = Cluster.RootBoundRadius / LODMaxRadiusRef;
			//}

			ClusterInfo.ScreenSize[LODIt] = InSettings.LODs[LODIt].ScreenSize * ScreenSizeScale;
			ClusterInfo.bIsVisible[LODIt] = InSettings.LODs[LODIt].bVisible;
			FHairClusterLODInfo& ClusterLODInfo = LocalClusterLODInfos[ClusterInfo.LODInfoOffset + LODIt];
			ClusterLODInfo.RadiusScale0 = LODScale;
			ClusterLODInfo.RadiusScale1 = LODScale;
		}

		// Fill in transition radius between LOD to insure that the interpolation is continuous
		for (uint8 LODIt = 0; LODIt < LODCount - 1; ++LODIt)
		{
			FHairClusterLODInfo& Curr = LocalClusterLODInfos[ClusterInfo.LODInfoOffset + LODIt];
			FHairClusterLODInfo& Next = LocalClusterLODInfos[ClusterInfo.LODInfoOffset + LODIt + 1];
			Curr.RadiusScale1 = Next.RadiusScale0;
		}
	}
#if USE_PARALLE_FOR
	);
#endif

	// Compute the screen size of the entire group at which the groom need to change LOD
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		Out.CPULODScreenSize.Add(InSettings.LODs[LODIt].ScreenSize);
		Out.LODVisibility.Add(InSettings.LODs[LODIt].bVisible);
	}

	// Copy the final value to the array which will be used to copy data to the GPU.
	// This operations is not needer per se. We could just keep & use RawClusterVertexIds
	Out.ClusterVertexIds.SetNum(RawClusterVertexCount);
	FMemory::Memcpy(Out.ClusterVertexIds.GetData(), RawClusterVertexIds, RawClusterVertexCount * sizeof(uint32));

	delete[] RawClusterVertexIds;
}

static void BuildClusterBulkData(
	const FHairStrandsClusterCullingData& In,
	FHairStrandsClusterCullingBulkData& Out)
{
	Out.Reset();

	Out.ClusterCount	= In.ClusterCount;
	Out.VertexCount		= In.VertexCount;
	Out.VertexLODCount	= In.ClusterVertexIds.Num();
	Out.ClusterLODCount = In.ClusterLODInfos.Num();

	Out.CPULODScreenSize= In.CPULODScreenSize;
	Out.LODVisibility	= In.LODVisibility;

	// Sanity check
	check(Out.ClusterCount			== uint32(In.ClusterInfos.Num()));
	check(Out.ClusterLODCount		== uint32(In.ClusterLODInfos.Num()));
	check(Out.VertexCount			== uint32(In.VertexToClusterIds.Num()));
	check(Out.VertexLODCount		== uint32(In.ClusterVertexIds.Num()));
	
	HairStrandsBuilder::CopyToBulkData<FHairClusterLODInfoFormat>(Out.ClusterLODInfos, In.ClusterLODInfos);
	HairStrandsBuilder::CopyToBulkData<FHairClusterIndexFormat>(Out.VertexToClusterIds, In.VertexToClusterIds);
	HairStrandsBuilder::CopyToBulkData<FHairClusterIndexFormat>(Out.ClusterVertexIds, In.ClusterVertexIds);

	// Pack LODInfo into GPU format
	{
		check(uint32(In.ClusterInfos.Num()) == Out.ClusterCount);
		check(uint32(In.VertexToClusterIds.Num()) == Out.VertexCount);
	
		TArray<FHairClusterInfo::Packed> PackedClusterInfos;
		PackedClusterInfos.Reserve(In.ClusterInfos.Num());
		for (const FHairClusterInfo& Info : In.ClusterInfos)
		{
			FHairClusterInfo::Packed& PackedInfo = PackedClusterInfos.AddDefaulted_GetRef();
			// Sanity check
			check(Info.LODCount <= 8);

			PackedInfo.LODCount = FMath::Clamp(Info.LODCount, 0u, 0xFFu);
			PackedInfo.LODInfoOffset = FMath::Clamp(Info.LODInfoOffset, 0u, (1u << 24u) - 1u);
			PackedInfo.LOD_ScreenSize_0 = to10Bits(Info.ScreenSize[0]);
			PackedInfo.LOD_ScreenSize_1 = to10Bits(Info.ScreenSize[1]);
			PackedInfo.LOD_ScreenSize_2 = to10Bits(Info.ScreenSize[2]);
			PackedInfo.LOD_ScreenSize_3 = to10Bits(Info.ScreenSize[3]);
			PackedInfo.LOD_ScreenSize_4 = to10Bits(Info.ScreenSize[4]);
			PackedInfo.LOD_ScreenSize_5 = to10Bits(Info.ScreenSize[5]);
			PackedInfo.LOD_ScreenSize_6 = to10Bits(Info.ScreenSize[6]);
			PackedInfo.LOD_ScreenSize_7 = to10Bits(Info.ScreenSize[7]);
			PackedInfo.LOD_bIsVisible = 0;
			for (uint32 LODIt = 0; LODIt < FHairClusterInfo::MaxLOD; ++LODIt)
			{
				if (Info.bIsVisible[LODIt])
				{
					PackedInfo.LOD_bIsVisible = PackedInfo.LOD_bIsVisible | (1 << LODIt);
				}
			}

			PackedInfo.Pad0 = 0;
			PackedInfo.Pad1 = 0;
			PackedInfo.Pad2 = 0;

			static_assert(sizeof(FHairClusterInfo::Packed) == sizeof(FHairClusterInfo::BulkType));
		}

		HairStrandsBuilder::CopyToBulkData<FHairClusterInfoFormat>(Out.PackedClusterInfos, PackedClusterInfos);
	}
}

} // namespace GroomBuilder_Cluster

void FGroomBuilder::BuildClusterBulkData(
	const FHairStrandsDatas& InRenStrandsData,
	const float InGroomAssetRadius,
	const FHairGroupsLOD& InSettings,
	FHairStrandsClusterCullingBulkData& Out)
{
	FHairStrandsClusterCullingData ClusterData;
	GroomBuilder_Cluster::BuildClusterData(InRenStrandsData, InGroomAssetRadius, InSettings, ClusterData);
	GroomBuilder_Cluster::BuildClusterBulkData(ClusterData, Out);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Asynchronous queuing for hair strands positions readback

class FHairStrandCopyPositionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairStrandCopyPositionCS);
	SHADER_USE_PARAMETER_STRUCT(FHairStrandCopyPositionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxVertexCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, InPositions)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, InPositionOffset)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutPositions)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_READ_POSITIONS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairStrandCopyPositionCS, "/Engine/Private/HairStrands/HairStrandsTexturesGeneration.usf", "MainCS", SF_Compute);

static FRDGBufferRef ReadPositions(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const UGroomComponent* Component,
	uint32 GroupIt)
{
	FRDGBufferRef OutPositions = nullptr;
	if (const FHairGroupInstance* Instance = Component->GetGroupInstance(GroupIt))
	{
		const uint32 NumVertices = Instance->Strands.Data->PointCount;

		FRDGBufferSRVRef InPositions = nullptr;
		FRDGBufferSRVRef InPositionOffset = nullptr;
		if (Instance->Strands.DeformedResource)
		{
			InPositions = Register(GraphBuilder, Instance->Strands.DeformedResource->GetBuffer(FHairStrandsDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateSRV).SRV;
			InPositionOffset = Register(GraphBuilder, Instance->Strands.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateSRV).SRV;
		}
		else
		{
			InPositions = Register(GraphBuilder, Instance->Strands.RestResource->PositionBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
			InPositionOffset = Register(GraphBuilder, Instance->Strands.RestResource->PositionOffsetBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
		}

		OutPositions = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), NumVertices), TEXT("StrandsPositionForReadback"));

		FHairStrandCopyPositionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairStrandCopyPositionCS::FParameters>();
		Parameters->MaxVertexCount = NumVertices;
		Parameters->InPositionOffset = InPositionOffset;
		Parameters->InPositions = InPositions;
		Parameters->OutPositions = GraphBuilder.CreateUAV(OutPositions, PF_A32B32G32R32F);

		FIntVector DispatchCount = FComputeShaderUtils::GetGroupCount(NumVertices, 128);

		TShaderMapRef<FHairStrandCopyPositionCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::CopyPositions"),
			ComputeShader,
			Parameters,
			DispatchCount);
	}
	return OutPositions;
}

struct FStrandsPositionData
{
	TUniquePtr<FRHIGPUBufferReadback> GPUPositions;
	uint32 NumBytes = 0;
	uint32 GroupIt = 0;
	uint32 GroupCount = 0;
	uint32 ReadbackDelay = 0;
	const UGroomComponent* Component = nullptr;
	FStrandsPositionOutput* Output;
};

TQueue<FStrandsPositionData*>		GStrandsPositionQueries;
TQueue<FStrandsPositionData*>		GStrandsPositionReadbacks;

bool HasHairStrandsPositionQueries()
{
	return !GStrandsPositionQueries.IsEmpty() || !GStrandsPositionReadbacks.IsEmpty();
}

void RunHairStrandsPositionQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* DebugShaderData)
{
	// Operations are ordered in reverse to ensure they are processed on independent frames to avoid TDR on heavy grooms.

	// 2. Readback positions
	{
		TArray<FStrandsPositionData*> QueryNotReady;
		FStrandsPositionData* Q = nullptr;
		while (GStrandsPositionReadbacks.Dequeue(Q))
		{			
			if (Q->GPUPositions->IsReady() || Q->ReadbackDelay == 0)
			{
				const FHairGroupInstance* Instance = Q->Component->GetGroupInstance(Q->GroupIt);
				const uint32 CurveCount = Instance->Strands.RestResource->BulkData.CurveCount;
				const uint32 PointCount = Instance->Strands.RestResource->BulkData.PointCount;

				check(Q->GroupIt < uint32(Q->Output->Groups.Num()));
				FStrandsPositionOutput::FGroup& Group = Q->Output->Groups[Q->GroupIt];
				Group.Reserve(CurveCount);

				FStrandsPositionOutput::FStrand Strand;
				const FVector4f* Positions = (const FVector4f*)Q->GPUPositions->Lock(Q->NumBytes);
				for (uint32 PointIt=0; PointIt<PointCount; ++PointIt)
				{
					const FVector4f P = Positions[PointIt];
					const bool bIsFirstPoint = P.W == 1;
					if (bIsFirstPoint && Strand.Num() > 0)
					{
						Group.Add(Strand);
						Strand.SetNum(0);
					}
					Strand.Add(FVector3f(P.X, P.Y, P.Z));
				}
				Q->GPUPositions->Unlock();

				--Q->Output->Status;
				check(Q->Output->Status >= 0);
			}
			else
			{
				if (Q->ReadbackDelay > 0) { --Q->ReadbackDelay; }
				QueryNotReady.Add(Q);
			}
		}

		for (FStrandsPositionData* N : QueryNotReady)
		{
			GStrandsPositionReadbacks.Enqueue(N);
		}
	}

	// 1. Copy positions
	{
		FStrandsPositionData* Q;
		while (GStrandsPositionQueries.Dequeue(Q))
		{
			check(Q->Component);
			
			FRDGBufferRef Positions = ReadPositions(GraphBuilder, ShaderMap, Q->Component, Q->GroupIt);
			FRHIGPUBufferReadback* GPUPositions = Q->GPUPositions.Get();
			AddEnqueueCopyPass(GraphBuilder, GPUPositions, Positions, Positions->Desc.GetSize());
			Q->NumBytes = Positions->Desc.GetSize();
			GStrandsPositionReadbacks.Enqueue(Q);
		}
	}
}

#if WITH_EDITOR
bool RequestStrandsPosition(const UGroomComponent* Component, FStrandsPositionOutput* Output)
{
	if (Component == nullptr || Output == nullptr)
	{
		return false;
	}

	const uint32 GroupCount = Component->GetGroupCount();
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		FStrandsPositionData* Data = new FStrandsPositionData();
		Data->NumBytes = 0;
		Data->GroupIt = GroupIt;
		Data->GroupCount = GroupCount;
		Data->Component = Component;
		Data->ReadbackDelay = 3;
		Data->GPUPositions = MakeUnique<FRHIGPUBufferReadback>(TEXT("Readback.Positions"));
		Data->Output = Output;
		GStrandsPositionQueries.Enqueue(Data);
	}

	*Output = FStrandsPositionOutput();
	Output->Component = Component;
	Output->Status = GroupCount;
	Output->Groups.SetNum(GroupCount);
	return true;
}
#endif

#undef LOCTEXT_NAMESPACE
