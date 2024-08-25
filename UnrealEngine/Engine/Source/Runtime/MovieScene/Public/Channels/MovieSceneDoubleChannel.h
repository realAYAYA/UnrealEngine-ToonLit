// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelEditorData.h"
#include "Channels/MovieSceneCurveChannelCommon.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/EnumAsByte.h"
#include "CoreTypes.h"
#include "Curves/KeyHandle.h"
#include "Curves/RealCurve.h"
#include "Curves/RichCurve.h"
#include "HAL/PlatformCrt.h"
#include "KeyParams.h"
#include "Math/Range.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "Misc/Optional.h"
#include "MovieSceneChannel.h"
#include "MovieSceneChannelData.h"
#include "MovieSceneChannelTraits.h"
#include "Serialization/StructuredArchive.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"

#include "MovieSceneDoubleChannel.generated.h"

class FArchive;
struct FPropertyTag;
template<typename> struct TMovieSceneCurveChannelImpl;
template <typename T> struct TIsPODType;

namespace UE::MovieScene::Interpolation
{
	struct FCachedInterpolation;
}


USTRUCT()
struct FMovieSceneDoubleValue
{
	GENERATED_BODY()

	FMovieSceneDoubleValue()
		: Value(0.f), InterpMode(RCIM_Cubic), TangentMode(RCTM_Auto), PaddingByte(0)
	{}

	explicit FMovieSceneDoubleValue(double InValue)
		: Value(InValue), InterpMode(RCIM_Cubic), TangentMode(RCTM_Auto), PaddingByte(0)
	{}

	bool Serialize(FArchive& Ar);
	bool operator==(const FMovieSceneDoubleValue& Other) const;
	bool operator!=(const FMovieSceneDoubleValue& Other) const;
	friend FArchive& operator<<(FArchive& Ar, FMovieSceneDoubleValue& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	UPROPERTY(EditAnywhere, Category = "Key")
	double Value;

	UPROPERTY(EditAnywhere, Category = "Key")
	FMovieSceneTangentData Tangent;

	UPROPERTY(EditAnywhere, Category = "Key")
	TEnumAsByte<ERichCurveInterpMode> InterpMode;

	UPROPERTY(EditAnywhere, Category = "Key")
	TEnumAsByte<ERichCurveTangentMode> TangentMode;

	/**
	 * double value = 8 bytes
	 * tangent data = 4 floats + byte enum = 4*4 + 1 = 17 bytes, rounded up to 20 bytes on clang-win64
	 * interp and tangent modes = 2 byte enums = 2 bytes
	 * total = 30 bytes
	 */
	UPROPERTY()
	uint8 PaddingByte;

	// This is required because TMovieSceneCurveChannelImpl<ChannelType>::Serialize dumps us as a byte array so we need padding to be initialized to avoid indeterminism in the cooked build
	uint8 UnserializedPaddingBytes[1] = { 0 };
};
static_assert(
	sizeof(FMovieSceneDoubleValue) == 
	sizeof(FMovieSceneDoubleValue::Value) + sizeof(FMovieSceneDoubleValue::Tangent) + sizeof(FMovieSceneDoubleValue::InterpMode) + sizeof(FMovieSceneDoubleValue::TangentMode) + sizeof(FMovieSceneDoubleValue::PaddingByte) + sizeof(FMovieSceneDoubleValue::UnserializedPaddingBytes),
	"Adjust padding size to avoid cooked build indeterminism with uninitialized padded data");

template<>
struct TIsPODType<FMovieSceneDoubleValue>
{
	enum { Value = true };
};


template<>
struct TStructOpsTypeTraits<FMovieSceneDoubleValue>
	: public TStructOpsTypeTraitsBase2<FMovieSceneDoubleValue>
{
	enum
	{
		WithSerializer = true,
		WithCopy = false,
		WithIdenticalViaEquality = true,
	};
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};


USTRUCT()
struct FMovieSceneDoubleChannel : public FMovieSceneChannel
{
	GENERATED_BODY()

	typedef double CurveValueType;
	typedef FMovieSceneDoubleValue ChannelValueType;

	FMovieSceneDoubleChannel() 
		: PreInfinityExtrap(RCCE_Constant)
		, PostInfinityExtrap(RCCE_Constant)
		, DefaultValue(0.0)
		, bHasDefaultValue(false)
#if WITH_EDITORONLY_DATA
		, bShowCurve(false)
#endif
	{}

	/**
	 * Access a mutable interface for this channel's data
	 *
	 * @return An object that is able to manipulate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<FMovieSceneDoubleValue> GetData()
	{
		return TMovieSceneChannelData<FMovieSceneDoubleValue>(&Times, &Values, &KeyHandles);
	}

	/**
	 * Access a constant interface for this channel's data
	 *
	 * @return An object that is able to interrogate this channel's data
	 */
	FORCEINLINE TMovieSceneChannelData<const FMovieSceneDoubleValue> GetData() const
	{
		return TMovieSceneChannelData<const FMovieSceneDoubleValue>(&Times, &Values);
	}

	/**
	 * Const access to this channel's times
	 */
	FORCEINLINE TArrayView<const FFrameNumber> GetTimes() const
	{
		return Times;
	}

	/**
	 * Const access to this channel's values
	 */
	FORCEINLINE TArrayView<const FMovieSceneDoubleValue> GetValues() const
	{
		return Values;
	}

	/**
	* Evaluate this channel with the frame resolution 
	*
	* @param InTime     The time to evaluate at
	* @param InTime     The Frame Rate the time to evaluate at
	* @param OutValue   A value to receive the result
	* @return true if the channel was evaluated successfully, false otherwise
	*/
	MOVIESCENE_API bool Evaluate(FFrameTime InTime, double& OutValue) const;

	/**
	 * Type shortening version of Evaluate above.
	 */
	MOVIESCENE_API bool Evaluate(FFrameTime InTime, float& OutValue) const;

	/**
	 * Retrieve a cached interpolation from this channel for the specified time
	 */
	MOVIESCENE_API UE::MovieScene::Interpolation::FCachedInterpolation GetInterpolationForTime(FFrameTime InTime) const;

	/**
	 * Set the channel's times and values to the requested values
	 */
	MOVIESCENE_API void Set(TArray<FFrameNumber> InTimes, TArray<FMovieSceneDoubleValue> InValues);

	/**
	 * Set the channel's times and values to the requested values, but does not allocate key handles
	 */
	MOVIESCENE_API void SetKeysOnly(TArrayView<FFrameNumber> InTimes, TArrayView<FMovieSceneDoubleValue> InValues);
public:

	// ~ FMovieSceneChannel Interface
	MOVIESCENE_API virtual void GetKeys(const TRange<FFrameNumber>& WithinRange, TArray<FFrameNumber>* OutKeyTimes, TArray<FKeyHandle>* OutKeyHandles) override;
	MOVIESCENE_API virtual void GetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<FFrameNumber> OutKeyTimes) override;
	MOVIESCENE_API virtual void SetKeyTimes(TArrayView<const FKeyHandle> InHandles, TArrayView<const FFrameNumber> InKeyTimes) override;
	MOVIESCENE_API virtual void DuplicateKeys(TArrayView<const FKeyHandle> InHandles, TArrayView<FKeyHandle> OutNewHandles) override;
	MOVIESCENE_API virtual void DeleteKeys(TArrayView<const FKeyHandle> InHandles) override;
	MOVIESCENE_API virtual void DeleteKeysFrom(FFrameNumber InTime, bool bDeleteKeysBefore) override;
	MOVIESCENE_API virtual void ChangeFrameResolution(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	MOVIESCENE_API virtual TRange<FFrameNumber> ComputeEffectiveRange() const override;
	MOVIESCENE_API virtual int32 GetNumKeys() const override;
	MOVIESCENE_API virtual void Reset() override;
	MOVIESCENE_API virtual void Offset(FFrameNumber DeltaPosition) override;
	MOVIESCENE_API virtual void Optimize(const FKeyDataOptimizationParams& InParameters) override;
	MOVIESCENE_API virtual void ClearDefault() override;
	MOVIESCENE_API virtual void PostEditChange() override;
	MOVIESCENE_API virtual FKeyHandle GetHandle(int32 Index) override;
	MOVIESCENE_API virtual int32 GetIndex(FKeyHandle Handle) override;

public:

	/**
	 * Check whether this channel has any data
	 */
	FORCEINLINE bool HasAnyData() const
	{
		return Times.Num() != 0 || bHasDefaultValue == true;
	}

	/**
	 * Set this channel's default value that should be used when no keys are present
	 *
	 * @param InDefaultValue The desired default value
	 */
	FORCEINLINE void SetDefault(double InDefaultValue)
	{
		bHasDefaultValue = true;
		DefaultValue = InDefaultValue;
	}

	/**
	 * Get this channel's default value that will be used when no keys are present
	 *
	 * @return (Optional) The channel's default value
	 */
	FORCEINLINE TOptional<double> GetDefault() const
	{
		return bHasDefaultValue ? TOptional<double>(DefaultValue) : TOptional<double>();
	}

	/**
	 * Remove this channel's default value causing the channel to have no effect where no keys are present
	 */
	FORCEINLINE void RemoveDefault()
	{
		bHasDefaultValue = false;
	}

public:

	MOVIESCENE_API bool Serialize(FArchive& Ar);
	friend FArchive& operator<<(FArchive& Ar, FMovieSceneDoubleChannel& Me)
	{
		Me.Serialize(Ar);
		return Ar;
	}

	/** Serialize this double channel from a mismatching property tag (FRichCurve or FMovieSceneFloatChannel) */
	MOVIESCENE_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

	MOVIESCENE_API int32 AddConstantKey(FFrameNumber InTime, double InValue);

	MOVIESCENE_API int32 AddLinearKey(FFrameNumber InTime, double InValue);

	MOVIESCENE_API int32 AddCubicKey(FFrameNumber InTime, double InValue, ERichCurveTangentMode TangentMode = RCTM_Auto, const FMovieSceneTangentData& Tangent = FMovieSceneTangentData());

	MOVIESCENE_API void AutoSetTangents(float Tension = 0.f);

	/** Get the channel's frame resolution */
	FFrameRate GetTickResolution() const { return TickResolution; }
	/** Set the channel's frame resolution */
	void SetTickResolution(FFrameRate InTickSolution) { TickResolution = InTickSolution; }

	/**
	 * Populate the specified array with times and values that represent the smooth interpolation of this channel across the specified range
	 *
	 * @param StartTimeSeconds      The first time in seconds to include in the resulting array
	 * @param EndTimeSeconds        The last time in seconds to include in the resulting array
	 * @param TimeThreshold         A small time threshold in seconds below which we should stop adding new points
	 * @param ValueThreshold        A small value threshold below which we should stop adding new points where the linear interpolation would suffice
	 * @param TickResolution        The tick resolution with which to interpret this channel's times
	 * @param InOutPoints           An array to populate with the evaluated points
	 */
	MOVIESCENE_API void PopulateCurvePoints(double StartTimeSeconds, double EndTimeSeconds, double TimeThreshold, double ValueThreshold, FFrameRate TickResolution, TArray<TTuple<double, double>>& InOutPoints) const;	

	/**
	* Add keys with these times to channel. The number of elements in both arrays much match or nothing is added.
	* Also assume that the times are greater than last time in the channel and are increasing. If not bad things can happen.
	* @param InTimes Times to add
	* @param InValues Values to add
	*/
	MOVIESCENE_API void AddKeys(const TArray<FFrameNumber>& InTimes, const TArray<FMovieSceneDoubleValue>& InValues);

#if WITH_EDITORONLY_DATA
	/* Get whether to show this curve in the UI */
	MOVIESCENE_API bool GetShowCurve() const;
	/* Set whether to show this curve in the UI */
	MOVIESCENE_API void SetShowCurve(bool bInShowCurve);
#endif

public:

	/** Pre-infinity extrapolation state */
	UPROPERTY()
	TEnumAsByte<ERichCurveExtrapolation> PreInfinityExtrap;

	/** Post-infinity extrapolation state */
	UPROPERTY()
	TEnumAsByte<ERichCurveExtrapolation> PostInfinityExtrap;

private:

	UPROPERTY(meta=(KeyTimes))
	TArray<FFrameNumber> Times;

	UPROPERTY(meta=(KeyValues))
	TArray<FMovieSceneDoubleValue> Values;

	UPROPERTY()
	double DefaultValue;

	UPROPERTY()
	bool bHasDefaultValue;

	/** This needs to be a UPROPERTY so it gets saved into editor transactions but transient so it doesn't get saved into assets. */
	UPROPERTY(Transient)
	FMovieSceneKeyHandleMap KeyHandles;

	UPROPERTY()
	FFrameRate TickResolution;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	bool bShowCurve;

#endif

	friend struct TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>;
	using FMovieSceneDoubleChannelImpl = TMovieSceneCurveChannelImpl<FMovieSceneDoubleChannel>;
};

template<>
struct TStructOpsTypeTraits<FMovieSceneDoubleChannel> : public TStructOpsTypeTraitsBase2<FMovieSceneDoubleChannel>
{
	enum 
	{ 
		WithStructuredSerializeFromMismatchedTag = true, 
	    WithSerializer = true
	};
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

template<>
struct TMovieSceneChannelTraits<FMovieSceneDoubleChannel> : TMovieSceneChannelTraitsBase<FMovieSceneDoubleChannel>
{
#if WITH_EDITOR

	/** Double channels can have external values (ie, they can get their values from external objects for UI purposes) */
	typedef TMovieSceneExternalValue<double> ExtendedEditorDataType;

#endif
};

/**
 * Overload for getting the interpolation mode for a channel at a specified time, it could be the previous key's mode.See UE::MovieScene::GetInterpolationMode for default implementation.
 */
MOVIESCENE_API EMovieSceneKeyInterpolation GetInterpolationMode(FMovieSceneDoubleChannel* InChannel, const FFrameNumber& InTime, EMovieSceneKeyInterpolation DefaultInterpolationMode);

/**
 * Overload for adding a new key to a double channel at a given time. See UE::MovieScene::AddKeyToChannel for default implementation.
 */
MOVIESCENE_API FKeyHandle AddKeyToChannel(FMovieSceneDoubleChannel* Channel, FFrameNumber InFrameNumber, double InValue, EMovieSceneKeyInterpolation Interpolation);


/**
 * Overload for dilating double channel data. See UE::MovieScene::Dilate for default implementation.
 */
MOVIESCENE_API void Dilate(FMovieSceneDoubleChannel* InChannel, FFrameNumber Origin, double DilationFactor);


/**
 * Overloads for common utility functions.
 */
MOVIESCENE_API bool ValueExistsAtTime(const FMovieSceneDoubleChannel* InChannel, FFrameNumber InFrameNumber, const FMovieSceneDoubleValue& InValue);
MOVIESCENE_API bool ValueExistsAtTime(const FMovieSceneDoubleChannel* InChannel, FFrameNumber InFrameNumber, double Value);
MOVIESCENE_API bool ValueExistsAtTime(const FMovieSceneDoubleChannel* InChannel, FFrameNumber InFrameNumber, float Value);
MOVIESCENE_API void AssignValue(FMovieSceneDoubleChannel* InChannel, FKeyHandle InKeyHandle, double InValue);
MOVIESCENE_API void AssignValue(FMovieSceneDoubleChannel* InChannel, FKeyHandle InKeyHandle, float InValue);

