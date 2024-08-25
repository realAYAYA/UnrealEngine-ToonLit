// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessDistribution.h"
#include "HAL/IConsoleManager.h"

namespace NiagaraStatelessDistributionPrivate
{
#if WITH_EDITORONLY_DATA
	bool GOptimizeLUTs = true;
	FAutoConsoleVariableRef CVarNiagaraStatelessDistributionOptimizeLUTs(
		TEXT("fx.NiagaraStateless.Distribution.OptimizeLUTs"),
		GOptimizeLUTs,
		TEXT("When enabled we optimize the LUT generation."),
		ECVF_Default
	);

	TArray<float> CurvesToLUT(TArrayView<const FRichCurve> Curves, int32 NumSamples)
	{
		const int32 NumChannels = Curves.Num();

		TArray<float> LUT;
		LUT.SetNumUninitialized(NumChannels * NumSamples);
		for (int32 iSample=0; iSample < NumSamples; ++iSample)
		{
			const float Time = float(iSample) / float(NumSamples - 1);
			for (int32 iChannel=0; iChannel < NumChannels; ++iChannel)
			{
				LUT[(iSample * NumChannels) + iChannel] = Curves[iChannel].Eval(Time);
			}
		}
		return LUT;
	}

	bool AreLUTsAlmostEqual(TArrayView<float> Lhs, TArrayView<float> Rhs, int32 NumChannels, float ErrorThreshold = 0.01f)
	{
		const int32 LhsNumSamples = Lhs.Num() / NumChannels;
		const int32 RhsNumSamples = Rhs.Num() / NumChannels;
		const int32 MaxSamples = FMath::Max(LhsNumSamples, RhsNumSamples);
		for ( int32 i=0; i < MaxSamples; ++i )
		{
			const float U = float(i) / float(MaxSamples - 1);

			const float LhsT = U * float(LhsNumSamples - 1);
			const float LhsU = FMath::Frac(LhsT);
			const int32 LhsA = FMath::Min(FMath::FloorToInt(LhsT), LhsNumSamples - 1);
			const int32 LhsB = FMath::Min(LhsA + 1, LhsNumSamples - 1);

			const float RhsT = U * float(RhsNumSamples - 1);
			const float RhsU = FMath::Frac(RhsT);
			const int32 RhsA = FMath::Min(FMath::FloorToInt(RhsT), RhsNumSamples - 1);
			const int32 RhsB = FMath::Min(RhsA + 1, RhsNumSamples - 1);

			for (int iChannel=0; iChannel < NumChannels; ++iChannel)
			{
				const float LhsValue = FMath::Lerp(Lhs[LhsA * NumChannels + iChannel], Lhs[LhsB * NumChannels + iChannel], LhsU);
				const float RhsValue = FMath::Lerp(Rhs[RhsA * NumChannels + iChannel], Rhs[RhsB * NumChannels + iChannel], RhsU);
				const float Error = FMath::Abs(LhsValue - RhsValue);
				if (Error > ErrorThreshold)
				{
					return false;
				}
			}
		}
		return true;
	}

	TArray<float> CurvesToOptimizedLUT(TArrayView<const FRichCurve> Curves, int32 MaxLutSampleCount)
	{
		TArray<float> LUT = CurvesToLUT(Curves, MaxLutSampleCount);
		if (GOptimizeLUTs)
		{
			for (int32 iSamples = 2; iSamples < MaxLutSampleCount; ++iSamples)
			{
				TArray<float> NewLUT = CurvesToLUT(Curves, iSamples);
				if (AreLUTsAlmostEqual(NewLUT, LUT, Curves.Num()))
				{
					return NewLUT;
				}
			}
		}
		return LUT;
	}

	template<typename FValueContainerSetNum, typename FValueAndChannelAccessor>
	void UpdateDistributionValues(ENiagaraDistributionMode InMode, const TArray<float>& InChannelConstantsAndRanges, const TArray<FRichCurve>& InChannelCurves, int32 InChannelCount, FValueContainerSetNum InContainerNum, FValueAndChannelAccessor InValueAndChannelAccessor, int32 MaxLutSampleCount)
	{
		switch(InMode)
		{
			case ENiagaraDistributionMode::UniformConstant:
				if (InChannelConstantsAndRanges.Num() >= 1)
				{
					// Note we set two values to simplify the GPU code
					InContainerNum(2);
					for (int32 ChannelIndex = 0; ChannelIndex < InChannelCount; ChannelIndex++)
					{
						InValueAndChannelAccessor(0, ChannelIndex) = InChannelConstantsAndRanges[0];
						InValueAndChannelAccessor(1, ChannelIndex) = InChannelConstantsAndRanges[0];
					}
				}
				break;
			case ENiagaraDistributionMode::NonUniformConstant:
				if (InChannelConstantsAndRanges.Num() >= InChannelCount)
				{
					// Note we set two values to simplify the GPU code
					InContainerNum(2);
					for (int32 ChannelIndex = 0; ChannelIndex < InChannelCount; ChannelIndex++)
					{
						InValueAndChannelAccessor(0, ChannelIndex) = InChannelConstantsAndRanges[ChannelIndex];
						InValueAndChannelAccessor(1, ChannelIndex) = InChannelConstantsAndRanges[ChannelIndex];
					}
				}
				break;
			case ENiagaraDistributionMode::UniformRange:
				if (InChannelConstantsAndRanges.Num() >= 2)
				{
					InContainerNum(2);
					for (int32 ChannelIndex = 0; ChannelIndex < InChannelCount; ChannelIndex++)
					{
						InValueAndChannelAccessor(0, ChannelIndex) = InChannelConstantsAndRanges[0];
						InValueAndChannelAccessor(1, ChannelIndex) = InChannelConstantsAndRanges[1];
					}
				}
				break;
			case ENiagaraDistributionMode::NonUniformRange:
				if (InChannelConstantsAndRanges.Num() >= 2 * InChannelCount)
				{
					InContainerNum(2);
					for (int32 ChannelIndex = 0; ChannelIndex < InChannelCount; ChannelIndex++)
					{
						InValueAndChannelAccessor(0, ChannelIndex) = InChannelConstantsAndRanges[ChannelIndex];
						InValueAndChannelAccessor(1, ChannelIndex) = InChannelConstantsAndRanges[InChannelCount + ChannelIndex];
					}
				}
				break;
			case ENiagaraDistributionMode::UniformCurve:
			case ENiagaraDistributionMode::NonUniformCurve:
			{
				const int32 ExpectedChannels = InMode == ENiagaraDistributionMode::NonUniformCurve ? InChannelCount : 1;
				if (InChannelCurves.Num() >= ExpectedChannels)
				{
					MaxLutSampleCount = FMath::Max(MaxLutSampleCount, 2);
					//const TArray<float> LUT = CurvesToLUT(MakeArrayView(InChannelCurves.GetData(), ExpectedChannels), MaxLutSampleCount);
					const TArray<float> LUT = CurvesToOptimizedLUT(MakeArrayView(InChannelCurves.GetData(), ExpectedChannels), MaxLutSampleCount);
					const int32 NumSamples = LUT.Num() / ExpectedChannels;

					InContainerNum(NumSamples);
					for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
					{
						for (int32 ChannelIndex=0; ChannelIndex < InChannelCount; ++ChannelIndex)
						{
							const int32 LUTChannelIndex = InMode == ENiagaraDistributionMode::NonUniformCurve ? ChannelIndex : 0;
							InValueAndChannelAccessor(SampleIndex, ChannelIndex) = LUT[(SampleIndex * ExpectedChannels) + LUTChannelIndex];
						}
					}
				}
				break;
			}
		}
	}
#endif
} //NiagaraStatelessCommon

void FNiagaraDistributionRangeInt::InitConstant(int32 Value)
{
	Mode = ENiagaraDistributionMode::UniformConstant;
	Min = Value;
	Max = Value;
}

FNiagaraStatelessRangeInt FNiagaraDistributionRangeInt::CalculateRange(const int32 Default) const
{
	return Mode == ENiagaraDistributionMode::UniformConstant ? FNiagaraStatelessRangeInt(Min, Min) : FNiagaraStatelessRangeInt(Min, Max);
}

void FNiagaraDistributionRangeFloat::InitConstant(float Value)
{
	Mode = ENiagaraDistributionMode::UniformConstant;
	Min = Value;
	Max = Value;
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value });
#endif
}

FNiagaraStatelessRangeFloat FNiagaraDistributionRangeFloat::CalculateRange(const float Default) const
{
	return Mode == ENiagaraDistributionMode::UniformConstant ? FNiagaraStatelessRangeFloat(Min, Min) : FNiagaraStatelessRangeFloat(Min, Max);
}

void FNiagaraDistributionRangeVector2::InitConstant(const FVector2f& Value)
{
	Mode = ENiagaraDistributionMode::NonUniformConstant;
	Min = Value;
	Max = Value;
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value.X, Value.Y });
#endif
}

FNiagaraStatelessRangeVector2 FNiagaraDistributionRangeVector2::CalculateRange(const FVector2f& Default) const
{
	return Mode == ENiagaraDistributionMode::UniformConstant ? FNiagaraStatelessRangeVector2(Min, Min) : FNiagaraStatelessRangeVector2(Min, Max);
}

void FNiagaraDistributionRangeVector3::InitConstant(const FVector3f& Value)
{
	Mode = ENiagaraDistributionMode::NonUniformConstant;
	Min = Value;
	Max = Value;
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value.X, Value.Y, Value.Z });
#endif
}

FNiagaraStatelessRangeVector3 FNiagaraDistributionRangeVector3::CalculateRange(const FVector3f& Default) const
{
	return Mode == ENiagaraDistributionMode::UniformConstant ? FNiagaraStatelessRangeVector3(Min, Min) : FNiagaraStatelessRangeVector3(Min, Max);
}

void FNiagaraDistributionRangeColor::InitConstant(const FLinearColor& Value)
{
	Mode = ENiagaraDistributionMode::NonUniformConstant;
	Min = Value;
	Max = Value;
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value.R, Value.G, Value.B, Value.A });
#endif
}

FNiagaraStatelessRangeColor FNiagaraDistributionRangeColor::CalculateRange(const FLinearColor& Default) const
{
	return Mode == ENiagaraDistributionMode::UniformConstant ? FNiagaraStatelessRangeColor(Min, Min) : FNiagaraStatelessRangeColor(Min, Max);
}


void FNiagaraDistributionFloat::InitConstant(float Value)
{
	Mode = ENiagaraDistributionMode::UniformConstant;
	Values = TArray<float>({ Value, Value });
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value });
#endif
}

FNiagaraStatelessRangeFloat FNiagaraDistributionFloat::CalculateRange(const float Default) const
{
	FNiagaraStatelessRangeFloat Range(Default, Default);
	if (Values.Num() > 0)
	{
		Range.Min = Values[0];
		Range.Max = Values[0];
		for (int i=1; i < Values.Num(); ++i)
		{
			Range.Min = FMath::Min(Range.Min, Values[i]);
			Range.Max = FMath::Max(Range.Max, Values[i]);
		}
	}
	return Range;
}

void FNiagaraDistributionVector2::InitConstant(const float Value)
{
	Mode = ENiagaraDistributionMode::UniformConstant;
	Values = TArray<FVector2f>({ FVector2f(Value, Value), FVector2f(Value, Value) });
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value, Value });
#endif
}

void FNiagaraDistributionVector2::InitConstant(const FVector2f& Value)
{
	Mode = ENiagaraDistributionMode::NonUniformConstant;
	Values = TArray<FVector2f>({ Value, Value });
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value.X, Value.Y });
#endif
}

FNiagaraStatelessRangeVector2 FNiagaraDistributionVector2::CalculateRange(const FVector2f& Default) const
{
	FNiagaraStatelessRangeVector2 Range(Default, Default);
	if (Values.Num() > 0)
	{
		Range.Min = Values[0];
		Range.Max = Values[0];
		for (int i = 1; i < Values.Num(); ++i)
		{
			Range.Min = FVector2f::Min(Range.Min, Values[i]);
			Range.Max = FVector2f::Max(Range.Max, Values[i]);
		}
	}
	return Range;
}

void FNiagaraDistributionVector3::InitConstant(const float Value)
{
	Mode = ENiagaraDistributionMode::UniformConstant;
	Values = TArray<FVector3f>({ FVector3f(Value, Value, Value), FVector3f(Value, Value, Value) });
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value, Value });
#endif
}

void FNiagaraDistributionVector3::InitConstant(const FVector3f& Value)
{
	Mode = ENiagaraDistributionMode::NonUniformConstant;
	Values = TArray<FVector3f>({ Value, Value });
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value.X, Value.Y, Value.Z });
#endif
}

FNiagaraStatelessRangeVector3 FNiagaraDistributionVector3::CalculateRange(const FVector3f& Default) const
{
	FNiagaraStatelessRangeVector3 Range(Default, Default);
	if (Values.Num() > 0)
	{
		Range.Min = Values[0];
		Range.Max = Values[0];
		for (int i = 1; i < Values.Num(); ++i)
		{
			Range.Min = FVector3f::Min(Range.Min, Values[i]);
			Range.Max = FVector3f::Max(Range.Max, Values[i]);
		}
	}
	return Range;
}

void FNiagaraDistributionColor::InitConstant(const FLinearColor& Value)
{
	Mode = ENiagaraDistributionMode::NonUniformConstant;
	Values = TArray<FLinearColor>({ Value, Value });
#if WITH_EDITORONLY_DATA
	ChannelConstantsAndRanges = TArray<float>({ Value.R, Value.G, Value.B, Value.A });
#endif
}

FNiagaraStatelessRangeColor FNiagaraDistributionColor::CalculateRange(const FLinearColor& Default) const
{
	FNiagaraStatelessRangeColor Range(Default, Default);
	if (Values.Num() > 0)
	{
		Range.Min = Values[0];
		Range.Max = Values[0];
		for (int i = 1; i < Values.Num(); ++i)
		{
			Range.Min.R = FMath::Min(Range.Min.R, Values[i].R);
			Range.Min.G = FMath::Min(Range.Min.G, Values[i].G);
			Range.Min.B = FMath::Min(Range.Min.B, Values[i].B);
			Range.Min.A = FMath::Min(Range.Min.A, Values[i].A);

			Range.Max.R = FMath::Max(Range.Max.R, Values[i].R);
			Range.Max.G = FMath::Max(Range.Max.G, Values[i].G);
			Range.Max.B = FMath::Max(Range.Max.B, Values[i].B);
			Range.Max.A = FMath::Max(Range.Max.A, Values[i].A);
		}
	}
	return Range;
}

#if WITH_EDITORONLY_DATA
void FNiagaraDistributionBase::PostEditChangeProperty(UObject* OwnerObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	FStructProperty* StructProperty = CastField<FStructProperty>(PropertyChangedEvent.Property);
	if (StructProperty && StructProperty->Struct && StructProperty->Struct->IsChildOf(FNiagaraDistributionBase::StaticStruct()))
	{
		FNiagaraDistributionBase* ValuePtr = nullptr;
		if (PropertyChangedEvent.Property != PropertyChangedEvent.MemberProperty)
		{
			// Properties stored in a UStruct inside a UObject need to first offset from UObject -> UStruct then UStruct -> Property
			FStructProperty* MemberStructProperty = CastField<FStructProperty>(PropertyChangedEvent.MemberProperty);
			void* StructPtr = MemberStructProperty->ContainerPtrToValuePtr<void>(OwnerObject);
			ValuePtr = StructProperty->ContainerPtrToValuePtr<FNiagaraDistributionBase>(StructPtr);
		}
		else
		{
			ValuePtr = StructProperty->ContainerPtrToValuePtr<FNiagaraDistributionBase>(OwnerObject);
		}

		if (ValuePtr != nullptr)
		{
			ValuePtr->UpdateValuesFromDistribution();
		}
	}
}

void FNiagaraDistributionRangeFloat::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode,
		ChannelConstantsAndRanges,
		ChannelCurves,
		1,
		[this](int32 Num) {},
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return ValueIndex == 0 ? Min : Max; },
		MaxLutSampleCount
	);
}

void FNiagaraDistributionRangeVector2::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode,
		ChannelConstantsAndRanges,
		ChannelCurves,
		2,
		[this](int32 Num) {},
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return ValueIndex == 0 ? Min[ChannelIndex] : Max[ChannelIndex]; },
		MaxLutSampleCount
	);
}

void FNiagaraDistributionRangeVector3::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode,
		ChannelConstantsAndRanges,
		ChannelCurves,
		3,
		[this](int32 Num) {},
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return ValueIndex == 0 ? Min[ChannelIndex] : Max[ChannelIndex]; },
		MaxLutSampleCount
	);
}

void FNiagaraDistributionRangeColor::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode,
		ChannelConstantsAndRanges,
		ChannelCurves,
		4,
		[this](int32 Num) {},
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return ValueIndex == 0 ? Min.Component(ChannelIndex) : Max.Component(ChannelIndex); },
		MaxLutSampleCount
	);
}

void FNiagaraDistributionFloat::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode, 
		ChannelConstantsAndRanges, 
		ChannelCurves,
		1,
		[this](int32 NumValues) { Values.SetNum(NumValues); },
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return Values[ValueIndex]; },
		MaxLutSampleCount
	);
}

void FNiagaraDistributionVector2::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode,
		ChannelConstantsAndRanges,
		ChannelCurves,
		2,
		[this](int32 NumValues) { Values.SetNum(NumValues); },
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return Values[ValueIndex][ChannelIndex]; },
		MaxLutSampleCount
	);
}

void FNiagaraDistributionVector3::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode,
		ChannelConstantsAndRanges,
		ChannelCurves,
		3,
		[this](int32 NumValues) { Values.SetNum(NumValues); },
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return Values[ValueIndex][ChannelIndex]; },
		MaxLutSampleCount
	);
}

void FNiagaraDistributionColor::UpdateValuesFromDistribution()
{
	NiagaraStatelessDistributionPrivate::UpdateDistributionValues(
		Mode,
		ChannelConstantsAndRanges,
		ChannelCurves,
		4,
		[this](int32 NumValues) { Values.SetNum(NumValues); },
		[this](int32 ValueIndex, int32 ChannelIndex) -> float& { return Values[ValueIndex].Component(ChannelIndex); },
		MaxLutSampleCount
	);
}

#endif

