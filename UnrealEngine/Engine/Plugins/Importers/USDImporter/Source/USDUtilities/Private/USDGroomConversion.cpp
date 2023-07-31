// Copyright Epic Games, Inc. All Rights Reserved.

#if USE_USD_SDK

#include "USDGroomConversion.h"

#include "GroomCacheData.h"
#include "HairDescription.h"
#include "USDPrimConversion.h"
#include "USDTypesConversion.h"
#include "UsdWrappers/UsdPrim.h"

#include "USDIncludesStart.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usdGeom/basisCurves.h"
#include "pxr/usd/usdGeom/primvar.h"
#include "pxr/usd/usdGeom/primvarsAPI.h"
#include "pxr/usd/usdGeom/xform.h"
#include "USDIncludesEnd.h"

namespace UE::UsdGroomConversion::Private
{
	bool IsAttributeValid(const FString& AttributeName)
	{
		// Ignore the attributes that aren't prefixed with groom_
		return AttributeName.StartsWith(TEXT("groom_"));
	}

	// Default USD to UE type converter
	template<typename TAttributeType>
	struct TConverter
	{
		template<typename InType>
		static TAttributeType ConvertType(const InType& In)
		{
			return In;
		}

		static TAttributeType DefaultValue()
		{
			return TAttributeType();
		}
	};

	// String to FName converter
	template<>
	struct TConverter<FName>
	{
		template<typename InType>
		static FName ConvertType(const InType& In)
		{
			return UsdToUnreal::ConvertName(In);
		}

		static FName DefaultValue()
		{
			return NAME_None;
		}
	};

	// GfVec2f/Float2 to FVector2f converter
	template<>
	struct TConverter<FVector2f>
	{
		template<typename InType>
		static FVector2f ConvertType(const InType& In)
		{
			return FVector2f(In[0], In[1]);
		}

		static FVector2f DefaultValue()
		{
			return FVector2f::ZeroVector;
		}
	};

	// GfVec3f/Float3 to FVector3f converter
	template<>
	struct TConverter<FVector3f>
	{
		template<typename InType>
		static FVector3f ConvertType(const InType& In)
		{
			return FVector3f(In[0], In[1], In[2]);
		}

		static FVector3f DefaultValue()
		{
			return FVector3f::ZeroVector;
		}
	};

	/** Convert the given primvar to hair attribute in the proper scope */
	template <typename USDType, typename UEType>
	void ConvertPrimvar(const pxr::UsdGeomPrimvar& Primvar, const pxr::UsdTimeCode& TimeCode, int32 StartStrandID, int32 NumStrands, int32 StartVertexID, int32 NumVertices, FHairDescription& HairDescription)
	{
		FString PrimvarName = UsdToUnreal::ConvertToken(Primvar.GetPrimvarName());
		FName AttributeName(*PrimvarName);

		pxr::VtArray<USDType> Values;
		Primvar.Get(&Values, TimeCode);
		int32 NumValues = Values.size();

		if (NumValues == 0)
		{
			return;
		}

		// Check the supported attribute scopes by checking the primvar interpolation
		// and the number of values as a fallback
		pxr::TfToken Interpolation = Primvar.GetInterpolation();
		if (Interpolation == pxr::UsdGeomTokens->uniform || Interpolation == pxr::UsdGeomTokens->constant || NumValues == NumStrands)
		{
			if (Interpolation == pxr::UsdGeomTokens->uniform && NumValues != NumStrands)
			{
				// Attribute has a mismatch between the expected and actual number of values so skip it
				return;
			}

			TStrandAttributesRef<UEType> StrandAttributeRef = HairDescription.StrandAttributes().GetAttributesRef<UEType>(AttributeName);
			if (!StrandAttributeRef.IsValid())
			{
				HairDescription.StrandAttributes().RegisterAttribute<UEType>(AttributeName, 1, TConverter<UEType>::DefaultValue());
				StrandAttributeRef = HairDescription.StrandAttributes().GetAttributesRef<UEType>(AttributeName);
			}

			// Constant scope is converted to uniform scope by setting the single value over all strands
			bool bIsConstantValue = Interpolation == pxr::UsdGeomTokens->constant;
			for (int32 StrandIndex = 0; StrandIndex < NumStrands; ++StrandIndex)
			{
				StrandAttributeRef[FStrandID(StartStrandID + StrandIndex)] = TConverter<UEType>::ConvertType(Values[bIsConstantValue ? 0 : StrandIndex]);
			}
		}
		else if (Interpolation == pxr::UsdGeomTokens->vertex || NumValues == NumVertices)
		{
			TVertexAttributesRef<UEType> VertexAttributeRef = HairDescription.VertexAttributes().GetAttributesRef<UEType>(AttributeName);
			if (!VertexAttributeRef.IsValid())
			{
				HairDescription.VertexAttributes().RegisterAttribute<UEType>(AttributeName, 1, TConverter<UEType>::DefaultValue());
				VertexAttributeRef = HairDescription.VertexAttributes().GetAttributesRef<UEType>(AttributeName);
			}

			for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				VertexAttributeRef[FVertexID(StartVertexID + VertexIndex)] = TConverter<UEType>::ConvertType(Values[VertexIndex]);
			}
		}
	}

	/** Convert the given primvars to hair attributes */
	void ConvertPrimvars(const std::vector<pxr::UsdGeomPrimvar>& Attributes, const pxr::UsdTimeCode& TimeCode, int32 StartStrandID, int32 NumStrands, int32 StartVertexID, int32 NumVertices, FHairDescription& HairDescription)
	{
		for (const pxr::UsdGeomPrimvar& Primvar : Attributes)
		{
			// Process only groom attributes
			FString UEPrimvarName = UsdToUnreal::ConvertToken(Primvar.GetPrimvarName());
			if (!IsAttributeValid(UEPrimvarName))
			{
				continue;
			}

			// #ueent_todo: Check if the groom_color attribute is animated (for GroomCache)

			// Dispatch the USD to UE conversion by the primvar scalar type
			pxr::SdfValueTypeName ScalarTypeName = Primvar.GetTypeName().GetScalarType();
			if (ScalarTypeName == pxr::SdfValueTypeNames->Int)
			{
				ConvertPrimvar<int, int>(Primvar, TimeCode, StartStrandID, NumStrands, StartVertexID, NumVertices, HairDescription);
			}
			else if (ScalarTypeName == pxr::SdfValueTypeNames->Float)
			{
				ConvertPrimvar<float, float>(Primvar, TimeCode, StartStrandID, NumStrands, StartVertexID, NumVertices, HairDescription);
			}
			else if (ScalarTypeName == pxr::SdfValueTypeNames->Vector3f || ScalarTypeName == pxr::SdfValueTypeNames->Float3)
			{
				ConvertPrimvar<pxr::GfVec3f, FVector3f>(Primvar, TimeCode, StartStrandID, NumStrands, StartVertexID, NumVertices, HairDescription);
			}
			else if (ScalarTypeName == pxr::SdfValueTypeNames->TexCoord2f || ScalarTypeName == pxr::SdfValueTypeNames->Float2)
			{
				ConvertPrimvar<pxr::GfVec2f, FVector2f>(Primvar, TimeCode, StartStrandID, NumStrands, StartVertexID, NumVertices, HairDescription);
			}
			else if (ScalarTypeName == pxr::SdfValueTypeNames->Double2)
			{
				ConvertPrimvar<pxr::GfVec2d, FVector2f>(Primvar, TimeCode, StartStrandID, NumStrands, StartVertexID, NumVertices, HairDescription);
			}
			else if (ScalarTypeName == pxr::SdfValueTypeNames->String)
			{
				ConvertPrimvar<std::string, FName>(Primvar, TimeCode, StartStrandID, NumStrands, StartVertexID, NumVertices, HairDescription);
			}
		}
	}

	/** Fill the AnimInfo with the time data for the given attribute, if it's animated */
	void UpdateAnimInfo(const pxr::UsdAttribute& Attr, EGroomCacheAttributes Flag, FGroomAnimationInfo* AnimInfo)
	{
		if (AnimInfo)
		{
			std::vector<double> TimeSamples;
			if (Attr.GetTimeSamples(&TimeSamples))
			{
				if (TimeSamples.size() > 1)
				{
					AnimInfo->Attributes = AnimInfo->Attributes | Flag;
					AnimInfo->StartFrame = FMath::Min(AnimInfo->StartFrame, TimeSamples[0]);
					AnimInfo->EndFrame = FMath::Max(AnimInfo->EndFrame, TimeSamples[TimeSamples.size() - 1]);
					AnimInfo->NumFrames = FMath::Max(AnimInfo->NumFrames, uint32(AnimInfo->EndFrame - AnimInfo->StartFrame + 1));
				}
			}
		}
	}
}

namespace UsdToUnreal
{
	bool ConvertGroomHierarchy(const pxr::UsdPrim& Prim, const pxr::UsdTimeCode& TimeCode, const FTransform& ParentTransform, FHairDescription& HairDescription, FGroomAnimationInfo* AnimInfo)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UsdToUnreal::ConvertGroomHierarchy);

		FScopedUsdAllocs Allocs;

		// ref. AlembicHairTranslator ParseObject

		bool bSuccess = true;
		FTransform TransformToPropagate = ParentTransform;
		if (pxr::UsdGeomBasisCurves Curves = pxr::UsdGeomBasisCurves(Prim))
		{
			// #ueent_todo: Add support for GroomCache import (needs AnimInfo)
			// There are also curve attributes that are not currently queried (knots, orders, basis, type, etc.)

			// Get curve and curve vextex count and vertex positions, the minimal data set required
			const int32 NumCurves = Curves.GetCurveCount(TimeCode);
			if (NumCurves == 0)
			{
				return false;
			}

			pxr::UsdAttribute NumVerticesAttr = Curves.GetCurveVertexCountsAttr();
			pxr::VtArray<int> NumVertices;
			if (!NumVerticesAttr || !NumVerticesAttr.Get(&NumVertices, TimeCode) || NumVertices.empty())
			{
				return false;
			}

			pxr::UsdAttribute PointsAttr = Curves.GetPointsAttr();
			pxr::VtArray<pxr::GfVec3f> Points;
			if (!PointsAttr || !PointsAttr.Get(&Points, TimeCode) || Points.empty())
			{
				return false;
			}

			UE::UsdGroomConversion::Private::UpdateAnimInfo(PointsAttr, EGroomCacheAttributes::Position, AnimInfo);

			// Get the starting strand and vertex IDs for this group of Curves
			int32 StartStrandID = HairDescription.GetNumStrands();
			int32 StartVertexID = HairDescription.GetNumVertices();

			bool bResetXformStack = false;
			FTransform PrimTransform = FTransform::Identity;
			bool bConverted = UsdToUnreal::ConvertXformable(Prim.GetStage(), Curves, PrimTransform, TimeCode.GetValue(), &bResetXformStack);
			if (bConverted)
			{
				TransformToPropagate = bResetXformStack ? PrimTransform : ParentTransform * PrimTransform;
			}

			uint32 PointIndex = 0;
			const FUsdStageInfo StageInfo(Prim.GetStage());
			for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
			{
				const int CurveNumVertices = NumVertices[CurveIndex];

				FStrandID StrandID = HairDescription.AddStrand();

				SetHairStrandAttribute(HairDescription, StrandID, HairAttribute::Strand::VertexCount, CurveNumVertices);

				for (int32 CurveVertexIndex = 0; CurveVertexIndex < CurveNumVertices; ++CurveVertexIndex, ++PointIndex)
				{
					FVertexID VertexID = HairDescription.AddVertex();

					const pxr::GfVec3f& Point = Points[PointIndex];

					FVector Position = UsdToUnreal::ConvertVector(StageInfo, Point);
					FVector ConvertedPosition = TransformToPropagate.TransformPosition(Position);
					SetHairVertexAttribute(HairDescription, VertexID, HairAttribute::Vertex::Position, FVector3f(ConvertedPosition));
				}
			}

			// Set width values. Width attribute is not a generic primvar so must be processed separately
			int NumPoints = Points.size();
			pxr::UsdAttribute Widths = Curves.GetWidthsAttr();
			pxr::VtArray<float> WidthsArray;
			if (Widths && Widths.Get(&WidthsArray, TimeCode) && !WidthsArray.empty())
			{
				UE::UsdGroomConversion::Private::UpdateAnimInfo(Widths, EGroomCacheAttributes::Width, AnimInfo);

				// Determine the scope of the Widths attribute
				pxr::TfToken Interpolation = Curves.GetWidthsInterpolation();

				const float UEMetersPerUnit = 0.01f;
				const float Scale = !FMath::IsNearlyEqual(StageInfo.MetersPerUnit, UEMetersPerUnit) ? StageInfo.MetersPerUnit / UEMetersPerUnit : 1.0f;
				if (Interpolation == pxr::UsdGeomTokens->uniform || Interpolation == pxr::UsdGeomTokens->constant)
				{
					const float ConstWidth = WidthsArray[0] * Scale;
					TStrandAttributesRef<float> WidthStrandAttributeRef = HairDescription.StrandAttributes().GetAttributesRef<float>(HairAttribute::Strand::Width);
					if (!WidthStrandAttributeRef.IsValid())
					{
						HairDescription.StrandAttributes().RegisterAttribute<float>(HairAttribute::Strand::Width);
						WidthStrandAttributeRef = HairDescription.StrandAttributes().GetAttributesRef<float>(HairAttribute::Strand::Width);
					}

					for (int32 Index = 0; Index < NumCurves; ++Index)
					{
						WidthStrandAttributeRef[FStrandID(StartStrandID + Index)] = Interpolation == pxr::UsdGeomTokens->constant ? ConstWidth : WidthsArray[Index] * Scale;
					}
				}
				else if (Interpolation == pxr::UsdGeomTokens->vertex)
				{
					TVertexAttributesRef<float> VertexAttributeRef = HairDescription.VertexAttributes().GetAttributesRef<float>(HairAttribute::Vertex::Width);
					if (!VertexAttributeRef.IsValid())
					{
						HairDescription.VertexAttributes().RegisterAttribute<float>(HairAttribute::Vertex::Width);
						VertexAttributeRef = HairDescription.VertexAttributes().GetAttributesRef<float>(HairAttribute::Vertex::Width);
					}

					for (int32 Index = 0; Index < NumPoints; ++Index)
					{
						VertexAttributeRef[FVertexID(StartVertexID + Index)] = WidthsArray[Index] * Scale;
					}
				}
			}

			// Extract the groom attributes (at vertex and strand scope) from the generic primvars
			pxr::UsdGeomPrimvarsAPI PrimvarsAPI(Prim);
			std::vector<pxr::UsdGeomPrimvar> Primvars = PrimvarsAPI.GetPrimvars();
			if (!Primvars.empty())
			{
				UE::UsdGroomConversion::Private::ConvertPrimvars(Primvars, TimeCode, StartStrandID, NumCurves, StartVertexID, NumPoints, HairDescription);
			}

			// Groom-scope attributes are not currently extracted since they are not strictly necessary

			// Following the USD recommendation that gprims be not nested, a Curves prim is handled as a leaf
		}
		else if (pxr::UsdGeomImageable(Prim))
		{
			// UsdGeomImageable includes UsdGeomXformable and UsdGeomScope
			if (pxr::UsdGeomXform Xform = pxr::UsdGeomXform(Prim))
			{
				// Propagate the prim transform to the children
				bool bResetXformStack = false;
				FTransform PrimTransform = FTransform::Identity;
				bool bConverted = UsdToUnreal::ConvertXformable(Prim.GetStage(), Xform, PrimTransform, TimeCode.GetValue(), &bResetXformStack);
				if (bConverted)
				{
					TransformToPropagate = bResetXformStack ? PrimTransform : ParentTransform * PrimTransform;
				}
			}

			for (const pxr::UsdPrim& Child : Prim.GetChildren())
			{
				bSuccess &= ConvertGroomHierarchy(Child, TimeCode, TransformToPropagate, HairDescription, AnimInfo);
				if (!bSuccess)
				{
					break;
				}
			}
		}

		return bSuccess;
	}
}

#endif // #if USE_USD_SDK
