// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionMathNodes.h"
#include "Dataflow/DataflowCore.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionMathNodes)

namespace Dataflow
{

	void GeometryCollectionMathNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAddDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSubtractDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMultiplyDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSafeDivideDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDivideDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDivisionDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSafeReciprocalDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSquareDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSquareRootDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FInverseSqrtDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCubeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FNegateDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FAbsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCeilDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRoundDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTruncDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFracDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMinDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMaxDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMin3DataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMax3DataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSignDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FClampDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFitDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FEFitDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FPowDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FLogDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FLogeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FLerpDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FWrapDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExpDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FSinDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FArcSinDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCosDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FArcCosDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FTanDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FArcTanDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FArcTan2DataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FNormalizeToRangeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FScaleVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDotProductDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCrossProductDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FNormalizeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FLengthDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDistanceDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FIsNearlyZeroDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomFloatDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomFloatInRangeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomUnitVectorDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRandomUnitVectorInConeDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRadiansToDegreesDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDegreesToRadiansDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FMathConstantsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FOneMinusDataflowNode);

		// Math
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("Math", FLinearColor(0.f, 0.4f, 0.8f), CDefaultNodeBodyTintColor);
	}
}

void FAddDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);

		const float Result = InFloatA + InFloatB;
		SetValue(Context, Result, &ReturnValue);
	}
}

void FSubtractDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);

		const float Result = InFloatA - InFloatB;
		SetValue(Context, Result, &ReturnValue);
	}
}

void FMultiplyDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);

		const float Result = InFloatA * InFloatB;
		SetValue(Context, Result, &ReturnValue);
	}
}

void FSafeDivideDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);

		float Result = 0.f;
		if (InFloatB != 0.f)
		{
			Result = InFloatA / InFloatB;
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FDivisionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Remainder) ||
		Out->IsA<int32>(&ReturnValue))
	{
		const float InDividend = GetValue<float>(Context, &Dividend, Dividend);
		const float InDivisor = GetValue<float>(Context, &Divisor, Divisor);

		float ResultRemainder = 0.f;
		int32 Result = 0;

		if (InDivisor != 0.f)
		{
			Result = (int32)(InDividend / InDivisor);
			ResultRemainder = InDividend - (float)Result * InDivisor;
		}
		SetValue(Context, ResultRemainder, &Remainder);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FSafeReciprocalDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		float Result = 0.f;
		if (InFloat != 0.f)
		{
			Result = 1.f / InFloat;
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FSquareDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = InFloat * InFloat;
		SetValue(Context, Result, &ReturnValue);
	}
}

void FSquareRootDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		float Result = 0.f;
		if (InFloat > 0.f)
		{
			Result = FMath::Sqrt(InFloat);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FInverseSqrtDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		float Result = 0.f;
		if (InFloat > 0.f)
		{
			Result = FMath::InvSqrt(InFloat);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FCubeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = InFloat * InFloat * InFloat;
		SetValue(Context, Result, &ReturnValue);
	}
}

void FNegateDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = -1.f * InFloat;
		SetValue(Context, Result, &ReturnValue);
	}
}

void FAbsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Abs(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FFloorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Floor(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FCeilDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		float Result = FMath::CeilToFloat(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FRoundDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		float Result = FMath::RoundToFloat(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FTruncDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::TruncToFloat(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FFracDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Frac(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FMinDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);

		const float Result = FMath::Min(InFloatA, InFloatB);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FMaxDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);

		const float Result = FMath::Max(InFloatA, InFloatB);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FMin3DataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);
		const float InFloatC = GetValue<float>(Context, &FloatC, FloatB);

		const float Result = FMath::Min3(InFloatA, InFloatB, InFloatC);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FMax3DataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloatA = GetValue<float>(Context, &FloatA, FloatA);
		const float InFloatB = GetValue<float>(Context, &FloatB, FloatB);
		const float InFloatC = GetValue<float>(Context, &FloatC, FloatB);

		const float Result = FMath::Max3(InFloatA, InFloatB, InFloatC);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FSignDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Sign(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FClampDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);
		const float InMin = GetValue<float>(Context, &Min, Min);
		const float InMax = GetValue<float>(Context, &Max, Max);

		const float Result = FMath::Clamp(InFloat, InMin, InMax);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FFitDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		float InFloat = GetValue<float>(Context, &Float, Float);
		const float InOldMin = GetValue<float>(Context, &OldMin, OldMin);
		const float InOldMax = GetValue<float>(Context, &OldMax, OldMax);
		const float InNewMin = GetValue<float>(Context, &NewMin, NewMin);
		const float InNewMax = GetValue<float>(Context, &NewMax, NewMax);

		float Result = InFloat;
		if (InOldMax > InOldMin && InNewMax > InNewMin)
		{
			InFloat = FMath::Clamp(InFloat, InOldMin, InOldMax);
			float Q = (InFloat - InOldMin) / (InOldMax - InOldMin);
			Result = InNewMin + Q * (InNewMax - InNewMin);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FEFitDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);
		const float InOldMin = GetValue<float>(Context, &OldMin, OldMin);
		const float InOldMax = GetValue<float>(Context, &OldMax, OldMax);
		const float InNewMin = GetValue<float>(Context, &NewMin, NewMin);
		const float InNewMax = GetValue<float>(Context, &NewMax, NewMax);

		float Result = InFloat;
		if (InOldMax > InOldMin && InNewMax > InNewMin)
		{
			float Q = (InFloat - InOldMin) / (InOldMax - InOldMin);
			Result = InNewMin + Q * (InNewMax - InNewMin);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FPowDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InBase = GetValue<float>(Context, &Base, Base);
		const float InExp = GetValue<float>(Context, &Exp, Exp);

		const float Result = FMath::Pow(InBase, InExp);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FLogDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InBase = GetValue<float>(Context, &Base, Base);
		const float InA = GetValue<float>(Context, &A, A);

		float Result = 0;
		if (InBase > 0.f)
		{
			Result = FMath::LogX(InBase, A);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FLogeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InA = GetValue<float>(Context, &A, A);

		const float Result = FMath::Loge(A);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FLerpDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InA = GetValue<float>(Context, &A, A);
		const float InB = GetValue<float>(Context, &B, B);
		const float InAlpha = GetValue<float>(Context, &Alpha, Alpha);

		float Result = InA;
		if (InB > InA)
		{
			Result = FMath::Lerp(InA, InB, InAlpha);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FWrapDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);
		const float InMin = GetValue<float>(Context, &Min, Min);
		const float InMax = GetValue<float>(Context, &Max, Max);

		float Result = InFloat;
		if (InMax > InMin)
		{
			Result = FMath::Wrap(InFloat, InMin, InMax);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FExpDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Exp(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FSinDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Sin(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FArcSinDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		float Result = 0.f;
		if (InFloat >= -1.f && InFloat <= 1.f)
		{
			Result = FMath::Asin(InFloat);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FCosDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Cos(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FArcCosDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		float Result = 0.f;
		if (InFloat >= -1.f && InFloat <= 1.f)
		{
			Result = FMath::Acos(InFloat);
		}
		SetValue(Context, Result, &ReturnValue);
	}
}

void FTanDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Tan(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FArcTanDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const float Result = FMath::Atan(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FArcTan2DataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InY = GetValue<float>(Context, &Y, Y);
		const float InX = GetValue<float>(Context, &X, X);

		const float Result = FMath::Atan2(InY, InX);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FNormalizeToRangeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);
		float IRangeMin = GetValue<float>(Context, &RangeMin, RangeMin);
		float InRangeMax = GetValue<float>(Context, &RangeMax, RangeMax);

		float Result;
		if (IRangeMin == InRangeMax)
		{
			if (InFloat < IRangeMin)
			{
				Result = 0.f;
			}
			else
			{
				Result = 1.f;
			}
		}

		if (IRangeMin > InRangeMax)
		{
			Swap(IRangeMin, InRangeMax);
		}
		Result = (InFloat - IRangeMin) / (InRangeMax - IRangeMin);

		SetValue(Context, Result, &ReturnValue);
	}
}


void FScaleVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&ScaledVector))
	{
		const FVector InVector = GetValue(Context, &Vector, Vector);
		const float InScale = GetValue(Context, &Scale, Scale);

		const FVector Result = InVector * InScale;
		SetValue(Context, Result, &ScaledVector);
	}
}

void FDotProductDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const FVector InVectorA = GetValue<FVector>(Context, &VectorA, VectorA);
		const FVector InVectorB = GetValue<FVector>(Context, &VectorB, VectorB);

		const float Result = FVector::DotProduct(InVectorA, InVectorB);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FCrossProductDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&ReturnValue))
	{
		const FVector InVectorA = GetValue<FVector>(Context, &VectorA, VectorA);
		const FVector InVectorB = GetValue<FVector>(Context, &VectorB, VectorB);

		const FVector Result = FVector::CrossProduct(InVectorA, InVectorB);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FNormalizeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&ReturnValue))
	{
		const FVector InVectorA = GetValue<FVector>(Context, &VectorA, VectorA);
		const float InTolerance = GetValue<float>(Context, &Tolerance, Tolerance);

		const FVector Result = InVectorA.GetSafeNormal(Tolerance);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FLengthDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const FVector InVector = GetValue<FVector>(Context, &Vector, Vector);

		const float Result = InVector.Length();
		SetValue(Context, Result, &ReturnValue);
	}
}

void FDistanceDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const FVector InPointA = GetValue<FVector>(Context, &PointA, PointA);
		const FVector InPointB = GetValue<FVector>(Context, &PointB, PointB);

		const float Result = (InPointB - InPointA).Length();
		SetValue(Context, Result, &ReturnValue);
	}
}

void FIsNearlyZeroDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<bool>(&ReturnValue))
	{
		const float InFloat = GetValue<float>(Context, &Float, Float);

		const bool Result = FMath::IsNearlyZero(InFloat);
		SetValue(Context, Result, &ReturnValue);
	}
}

void FRandomFloatDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		if (bDeterministic)
		{
			const float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			const FRandomStream Stream(RandomSeedVal);
			SetValue(Context, Stream.FRand(), &ReturnValue);
		}
		else
		{
			SetValue(Context, FMath::FRand(), &ReturnValue);
		}
	}
}

void FRandomFloatInRangeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		float MinVal = GetValue<float>(Context, &Min);
		float MaxVal = GetValue<float>(Context, &Max);

		if (bDeterministic)
		{
			const float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			const FRandomStream Stream(RandomSeedVal);
			SetValue(Context, (float)Stream.FRandRange(MinVal, MaxVal), &ReturnValue);
		}
		else
		{
			SetValue(Context, FMath::FRandRange(MinVal, MaxVal), &ReturnValue);
		}
	}
}

void FRandomUnitVectorDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&ReturnValue))
	{
		if (bDeterministic)
		{
			const float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			const FRandomStream Stream(RandomSeedVal);
			SetValue(Context, Stream.VRand(), &ReturnValue);
		}
		else
		{
			SetValue(Context, FMath::VRand(), &ReturnValue);
		}
	}
}

void FRandomUnitVectorInConeDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FVector>(&ReturnValue))
	{
		FVector ConeDirectionVal = GetValue<FVector>(Context, &ConeDirection);
		float ConeHalfAngleVal = GetValue<float>(Context, &ConeHalfAngle);

		if (bDeterministic)
		{
			const float RandomSeedVal = GetValue<float>(Context, &RandomSeed);

			const FRandomStream Stream(RandomSeedVal);
			SetValue(Context, Stream.VRandCone(ConeDirectionVal, ConeHalfAngleVal), &ReturnValue);
		}
		else
		{
			SetValue(Context, FMath::VRandCone(ConeDirectionVal, ConeHalfAngleVal), &ReturnValue);
		}
	}
}

void FRadiansToDegreesDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Degrees))
	{
		SetValue(Context, FMath::RadiansToDegrees(GetValue<float>(Context, &Radians)), &Degrees);
	}
}

void FDegreesToRadiansDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&Radians))
	{
		SetValue(Context, FMath::DegreesToRadians(GetValue<float>(Context, &Degrees)), &Radians);
	}
}

void FMathConstantsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		if (Constant == EMathConstantsEnum::Dataflow_MathConstants_Pi)
		{
			SetValue(Context, FMathf::Pi, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_HalfPi)
		{
			SetValue(Context, FMathf::HalfPi, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_TwoPi)
		{
			SetValue(Context, FMathf::TwoPi, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_FourPi)
		{
			SetValue(Context, FMathf::FourPi, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvPi)
		{
			SetValue(Context, FMathf::InvPi, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvTwoPi)
		{
			SetValue(Context, FMathf::InvTwoPi, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_Sqrt2)
		{
			SetValue(Context, FMathf::Sqrt2, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvSqrt2)
		{
			SetValue(Context, FMathf::InvSqrt2, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_Sqrt3)
		{
			SetValue(Context, FMathf::Sqrt3, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_MathConstants_InvSqrt3)
		{
			SetValue(Context, FMathf::InvSqrt3, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_E)
		{
			SetValue(Context, 2.71828182845904523536f, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_Gamma)
		{
			SetValue(Context, 0.577215664901532860606512090082f, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_GoldenRatio)
		{
			SetValue(Context, 1.618033988749894f, &ReturnValue);
		}
		else if (Constant == EMathConstantsEnum::Dataflow_FloatToInt_Function_ZeroTolerance)
		{
			SetValue(Context, FMathf::ZeroTolerance, &ReturnValue);
		}
	}
}

void FOneMinusDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ReturnValue))
	{
		const float InA = GetValue<float>(Context, &A, A);

		const float Result = 1.f - InA;
		SetValue(Context, Result, &ReturnValue);
	}
}



