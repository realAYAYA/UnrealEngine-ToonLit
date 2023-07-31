// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimCurveCompressionCodec_CompressedRichCurve.h"
#include "Animation/AnimSequence.h"
#include "Serialization/MemoryWriter.h"
#include "AnimationCompression.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimCurveCompressionCodec_CompressedRichCurve)

UAnimCurveCompressionCodec_CompressedRichCurve::UAnimCurveCompressionCodec_CompressedRichCurve(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	MaxCurveError = 0.0f;
	UseAnimSequenceSampleRate = true;
	ErrorSampleRate = 60.0f;
#endif
}

// This mirrors in part the FCompressedRichCurve
struct FCurveDesc
{
	TEnumAsByte<ERichCurveCompressionFormat> CompressionFormat;
	TEnumAsByte<ERichCurveKeyTimeCompressionFormat> KeyTimeCompressionFormat;
	TEnumAsByte<ERichCurveExtrapolation> PreInfinityExtrap;
	TEnumAsByte<ERichCurveExtrapolation> PostInfinityExtrap;
	FCompressedRichCurve::TConstantValueNumKeys ConstantValueNumKeys;
	int32 KeyDataOffset;
};

#if WITH_EDITORONLY_DATA

int32 ValidateCompressedRichCurveEvaluation = 0;
static FAutoConsoleVariableRef CVarValidateCompressedRichCurveEvaluation(
	TEXT("a.Compression.ValidateCompressedRichCurveEvaluation"),
	ValidateCompressedRichCurveEvaluation,
	TEXT("1 = runs validation, evaluating the compressed rich curve at animation its sampling rate comparing against the MaxCurveError. 0 = validation disabled"));

bool UAnimCurveCompressionCodec_CompressedRichCurve::Compress(const FCompressibleAnimData& AnimSeq, FAnimCurveCompressionResult& OutResult)
{
	int32 NumCurves = AnimSeq.RawFloatCurves.Num();

	TArray<FCurveDesc> Curves;
	Curves.Reserve(NumCurves);
	Curves.AddUninitialized(NumCurves);

	int32 KeyDataOffset = 0;
	KeyDataOffset += sizeof(FCurveDesc) * NumCurves;

	const FAnimKeyHelper Helper(AnimSeq.SequenceLength, AnimSeq.NumberOfKeys);
	const float SampleRate = UseAnimSequenceSampleRate ? Helper.KeysPerSecond() : ErrorSampleRate;

	TArray<uint8> KeyData;

	for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
	{
		const FFloatCurve& Curve = AnimSeq.RawFloatCurves[CurveIndex];

		FRichCurve RawCurve = Curve.FloatCurve;	// Copy
		RawCurve.RemoveRedundantAutoTangentKeys(MaxCurveError);

		FCompressedRichCurve CompressedCurve;
		RawCurve.CompressCurve(CompressedCurve, MaxCurveError, SampleRate);

		FCurveDesc& CurveDesc = Curves[CurveIndex];
		CurveDesc.CompressionFormat = CompressedCurve.CompressionFormat;
		CurveDesc.KeyTimeCompressionFormat = CompressedCurve.KeyTimeCompressionFormat;
		CurveDesc.PreInfinityExtrap = CompressedCurve.PreInfinityExtrap;
		CurveDesc.PostInfinityExtrap = CompressedCurve.PostInfinityExtrap;
		CurveDesc.ConstantValueNumKeys = CompressedCurve.ConstantValueNumKeys;
		CurveDesc.KeyDataOffset = KeyDataOffset;

		KeyDataOffset += CompressedCurve.CompressedKeys.Num();
		KeyData.Append(CompressedCurve.CompressedKeys);

		// Compressed curve validation
		if(!!ValidateCompressedRichCurveEvaluation)
		{
			const int32 SamplesToTake = AnimSeq.NumberOfKeys;
			for (int32 KeyIndex = 0; KeyIndex < SamplesToTake; ++KeyIndex)
			{
				// Evaluated RawCurve and Compressed Curve
				const float EvalTime = KeyIndex * Helper.TimePerKey();
				const float RawValue = RawCurve.Eval(EvalTime);
				const float CompressedValue = CompressedCurve.Eval(EvalTime);

				const float AbsDelta = FMath::Abs(CompressedValue - RawValue);
				if (AbsDelta > MaxCurveError)
				{
					// Delta larger than tolerated error value
					UE_LOG(LogAnimationCompression, Warning, TEXT("Curve %s: delta too large %f at %f"), *Curve.Name.DisplayName.ToString(), AbsDelta, EvalTime);
				}
			}
		}
	}

	TArray<uint8> TempBytes;
	TempBytes.Reserve(KeyDataOffset);

	// Serialize the compression settings into a temporary array. The archive
	// is flagged as persistent so that machines of different endianness produce
	// identical binary results.
	FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);

	Ar.Serialize(Curves.GetData(), sizeof(FCurveDesc) * NumCurves);
	Ar.Serialize(KeyData.GetData(), KeyData.Num());

	OutResult.CompressedBytes = TempBytes;
	OutResult.Codec = this;

	return true;
}

void UAnimCurveCompressionCodec_CompressedRichCurve::PopulateDDCKey(FArchive& Ar)
{
	Super::PopulateDDCKey(Ar);

	int32 CodecVersion = 0;

	Ar << CodecVersion;
	Ar << MaxCurveError;
	Ar << UseAnimSequenceSampleRate;
	Ar << ErrorSampleRate;
}
#endif

void UAnimCurveCompressionCodec_CompressedRichCurve::DecompressCurves(const FCompressedAnimSequence& AnimSeq, FBlendedCurve& Curves, float CurrentTime) const
{
	const uint8* Buffer = AnimSeq.CompressedCurveByteStream.GetData();
	const FCurveDesc* CurveDescriptions = (const FCurveDesc*)(Buffer);

	const TArray<FSmartName>& CompressedCurveNames = AnimSeq.CompressedCurveNames;
	const int32 NumCurves = CompressedCurveNames.Num();
	for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
	{
		const FSmartName& CurveName = CompressedCurveNames[CurveIndex];
		if (Curves.IsEnabled(CurveName.UID))
		{
			const FCurveDesc& Curve = CurveDescriptions[CurveIndex];
			const uint8* CompressedKeys = Buffer + Curve.KeyDataOffset;
			const float Value = FCompressedRichCurve::StaticEval(Curve.CompressionFormat, Curve.KeyTimeCompressionFormat, Curve.PreInfinityExtrap, Curve.PostInfinityExtrap, Curve.ConstantValueNumKeys, CompressedKeys, CurrentTime);
			Curves.Set(CurveName.UID, Value);
		}
	}
}

float UAnimCurveCompressionCodec_CompressedRichCurve::DecompressCurve(const FCompressedAnimSequence& AnimSeq, SmartName::UID_Type CurveUID, float CurrentTime) const
{
	const uint8* Buffer = AnimSeq.CompressedCurveByteStream.GetData();
	const FCurveDesc* CurveDescriptions = (const FCurveDesc*)(Buffer);

	const TArray<FSmartName>& CompressedCurveNames = AnimSeq.CompressedCurveNames;
	const int32 NumCurves = CompressedCurveNames.Num();
	for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
	{
		const FSmartName& CurveName = CompressedCurveNames[CurveIndex];
		if (CurveName.UID == CurveUID)
		{
			const FCurveDesc& Curve = CurveDescriptions[CurveIndex];
			const uint8* CompressedKeys = Buffer + Curve.KeyDataOffset;
			const float Value = FCompressedRichCurve::StaticEval(Curve.CompressionFormat, Curve.KeyTimeCompressionFormat, Curve.PreInfinityExtrap, Curve.PostInfinityExtrap, Curve.ConstantValueNumKeys, CompressedKeys, CurrentTime);
			return Value;
		}
	}

	return 0.0f;
}

