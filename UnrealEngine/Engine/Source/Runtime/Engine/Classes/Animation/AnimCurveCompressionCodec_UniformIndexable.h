// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Stores the raw rich curves as FCompressedRichCurve internally with optional key reduction and key time quantization.
*/

#include "CoreMinimal.h"
#include "Animation/AnimCurveCompressionCodec.h"
#include "AnimCurveCompressionCodec_UniformIndexable.generated.h"

struct ENGINE_API FAnimCurveBufferAccess
{
private:
	int32 NumSamples;
	float SampleRate;

	const float* CompressedBuffer;

	const FFloatCurve* RawCurve;
	bool bUseCompressedData;
public:

	FAnimCurveBufferAccess(const UAnimSequenceBase* InSequence, USkeleton::AnimCurveUID InUID);

	bool IsValid() const
	{
		return CompressedBuffer || RawCurve;
	}

	int32 GetNumSamples() const { return NumSamples; }

	float GetValue(int32 SampleIndex) const;

	float GetTime(int32 SampleIndex) const;
};

UCLASS(meta = (DisplayName = "Uniform Indexable"))
class ENGINE_API UAnimCurveCompressionCodec_UniformIndexable : public UAnimCurveCompressionCodec
{
	GENERATED_UCLASS_BODY()

	//////////////////////////////////////////////////////////////////////////

#if WITH_EDITORONLY_DATA
	// UAnimCurveCompressionCodec overrides
	virtual bool Compress(const FCompressibleAnimData& AnimSeq, FAnimCurveCompressionResult& OutResult) override;
	virtual void PopulateDDCKey(FArchive& Ar) override;
#endif

	virtual void DecompressCurves(const FCompressedAnimSequence& AnimSeq, FBlendedCurve& Curves, float CurrentTime) const override;
	virtual float DecompressCurve(const FCompressedAnimSequence& AnimSeq, SmartName::UID_Type CurveUID, float CurrentTime) const override;

	bool GetCurveBufferAndSamples(const FCompressedAnimSequence& AnimSeq, SmartName::UID_Type CurveUID, const float*& OutCurveBuffer, int32& OutSamples, float& OutSampleRate);
};
