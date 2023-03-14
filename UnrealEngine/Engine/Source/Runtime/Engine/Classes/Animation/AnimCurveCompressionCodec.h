// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimCompressionTypes.h"
#include "AnimCurveCompressionCodec.generated.h"

class UAnimCurveCompressionCodec;
class UAnimSequence;
struct FBlendedCurve;

#if WITH_EDITORONLY_DATA
/** Holds the result from animation curve compression */
struct FAnimCurveCompressionResult
{
	/** The animation curves as raw compressed bytes */
	TArray<uint8> CompressedBytes;

	/** The codec used by the compressed bytes */
	UAnimCurveCompressionCodec* Codec;

	/** Default constructor */
	FAnimCurveCompressionResult() : CompressedBytes(), Codec(nullptr) {}
};
#endif

/*
 * Base class for all curve compression codecs.
 */
UCLASS(abstract, hidecategories = Object, EditInlineNew)
class ENGINE_API UAnimCurveCompressionCodec : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** A GUID that is unique to this codec instance. After creation, it never changes. */
	FGuid InstanceGuid;
#endif

	/** Allow us to convert DDC serialized path back into codec object */
	virtual UAnimCurveCompressionCodec* GetCodec(const FString& Path) { return this; }

	//////////////////////////////////////////////////////////////////////////

#if WITH_EDITORONLY_DATA
	// UObject overrides
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void Serialize(FArchive& Ar) override;

	/** Returns whether or not we can use this codec to compress. */
	virtual bool IsCodecValid() const { return true; }

	/** Compresses the curve data from an animation sequence. */
	virtual bool Compress(const FCompressibleAnimData& AnimSeq, FAnimCurveCompressionResult& OutResult) PURE_VIRTUAL(UAnimCurveCompressionCodec::Compress, return false;);

	/*
	 * Called to generate a unique DDC key for this codec instance.
	 * A suitable key should be generated from: the InstanceGuid, a codec version, and all relevant properties that drive the behavior.
	 */
	virtual void PopulateDDCKey(FArchive& Ar);
#endif

	/*
	 * Decompresses all the active blended curves.
	 * Note: Codecs should _NOT_ rely on any member properties during decompression. Decompression
	 * behavior should entirely be driven by code and the compressed data.
	 */
	virtual void DecompressCurves(const FCompressedAnimSequence& AnimSeq, FBlendedCurve& Curves, float CurrentTime) const PURE_VIRTUAL(UAnimCurveCompressionCodec::DecompressCurves, );

	/*
	 * Decompress a single curve.
	 * Note: Codecs should _NOT_ rely on any member properties during decompression. Decompression
	 * behavior should entirely be driven by code and the compressed data.
	 */
	virtual float DecompressCurve(const FCompressedAnimSequence& AnimSeq, SmartName::UID_Type CurveUID, float CurrentTime) const PURE_VIRTUAL(UAnimCurveCompressionCodec::DecompressCurve, return 0.0f;);
};
