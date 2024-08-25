// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Range.h"
#include "Math/Transform.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieScenePropertySystemTypes.h"
#include "EntitySystem/MovieScenePropertyMetaDataTraits.h"
#include "EntitySystem/MovieScenePropertyTraits.h"
#include "Engine/EngineTypes.h"
#include "EulerTransform.h"
#include "TransformData.h"
#include "MovieSceneTracksPropertyTypes.h"
#include "Styling/SlateColor.h"
#include "ConstraintChannel.h"
#include "MaterialTypes.h"
#include "Sections/MovieSceneCameraShakeSection.h"
#include "Misc/Guid.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "MovieSceneTracksComponentTypes.generated.h"

class UMaterialParameterCollection;
class UMovieScene3DTransformSection;
class UMovieSceneAudioSection;
class UMovieSceneDataLayerSection;
class UMovieSceneLevelVisibilitySection;
class UMovieSceneSkeletalAnimationSection;
struct FCameraShakeBaseStartParams;
struct FCameraShakeSourceComponentStartParams;
struct FMovieSceneObjectBindingID;


/** Component data for Perlin Noise channels */
USTRUCT()
struct FPerlinNoiseParams
{
	GENERATED_BODY()

	/** The frequency of the noise, i.e. how many times per second does the noise peak */
	UPROPERTY(EditAnywhere, Category="Perlin Noise")
	float Frequency;

	/** The amplitude of the noise, which will vary between [-Amplitude, +Amplitude] */
	UPROPERTY(EditAnywhere, Category = "Perlin Noise")
	double Amplitude;

	/** Starting offset, in seconds, into the noise pattern */
	UPROPERTY(EditAnywhere, Category = "Perlin Noise")
	float Offset;

	MOVIESCENETRACKS_API FPerlinNoiseParams();
	MOVIESCENETRACKS_API FPerlinNoiseParams(float InFrequency, double InAmplitude);

	/** Generates a new random offset between [0, InMaxOffset] and sets it */
	MOVIESCENETRACKS_API void RandomizeOffset(float InMaxOffset = 100.f);
};

/** Component data for the level visibility system */
USTRUCT()
struct FLevelVisibilityComponentData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UMovieSceneLevelVisibilitySection> Section = nullptr;
};

/** Component data for the data layer system */
USTRUCT()
struct FMovieSceneDataLayerComponentData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UMovieSceneDataLayerSection> Section = nullptr;
};

/** Component data for the constraint system */
USTRUCT()
struct FConstraintComponentData
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid ConstraintID;
	UMovieScene3DTransformSection* Section;
};

/** Component data for a skeletal mesh animation */
USTRUCT()
struct FMovieSceneSkeletalAnimationComponentData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMovieSceneSkeletalAnimationSection> Section;
};

/** Component data for audio tracks */
USTRUCT()
struct FMovieSceneAudioComponentData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMovieSceneAudioSection> Section = nullptr;
};

/** Component data for camera shakes */
USTRUCT()
struct FMovieSceneCameraShakeComponentData
{
	GENERATED_BODY()

	/** The shake data from the section that created this component */
	UPROPERTY()
	FMovieSceneCameraShakeSectionData SectionData;

	/** The range of the section that created this component */
	UPROPERTY()
	FFrameNumber SectionStartTime;
	UPROPERTY()
	FFrameNumber SectionEndTime;

	/** The signature of the source section at the time the shake instance was created */
	UPROPERTY()
	FGuid SectionSignature;

	FMovieSceneCameraShakeComponentData()
	{}
	FMovieSceneCameraShakeComponentData(const FMovieSceneCameraShakeSectionData& InSectionData, const UMovieSceneSection& InSection)
		: SectionData(InSectionData)
		, SectionSignature(InSection.GetSignature())
	{
		const TRange<FFrameNumber> SectionRange = InSection.GetRange();
		SectionStartTime = SectionRange.HasLowerBound() ? SectionRange.GetLowerBoundValue() : 0;
		SectionEndTime = SectionRange.HasUpperBound() ? SectionRange.GetUpperBoundValue() : FFrameNumber(TNumericLimits<int32>::Max());
	}
};

/**
 * Component data for camera shakes created by the shake system
 * This is separate from FMovieSceneCameraShakeComponentData because that
 * one it imported from source shake sections, and our component data here
 * will be preserved on reimported entities.
 */
USTRUCT()
struct FMovieSceneCameraShakeInstanceData
{
	GENERATED_BODY()

	/** Shake instance created by the shake evaluation system */
	UPROPERTY()
	TObjectPtr<UCameraShakeBase> ShakeInstance;

	/** The signature of the source section at the time the shake instance was created */
	UPROPERTY()
	FGuid SectionSignature;

	/** Whether this instance is managed by a shake previewer */
	UPROPERTY()
	bool bManagedByPreviewer = false;
};

/**
 * Component data for audio tracks inputs
 * This provides the names of the inputs whose values are stored on the
 * same entity using the DoubleResult, StringResult, BoolResult, and
 * IntegerResult components.
 */
USTRUCT()
struct FMovieSceneAudioInputData
{
	GENERATED_BODY()

	UPROPERTY()
	FName FloatInputs[9];

	UPROPERTY()
	FName StringInput;

	UPROPERTY()
	FName BoolInput;

	UPROPERTY()
	FName IntInput;
};

namespace UE
{
namespace MovieScene
{

struct FComponentAttachParamsDestination
{
	FName SocketName    = NAME_None;
	FName ComponentName = NAME_None;

	MOVIESCENETRACKS_API USceneComponent* ResolveAttachment(AActor* InParentActor) const;
};

struct FComponentAttachParams
{
	EAttachmentRule AttachmentLocationRule  = EAttachmentRule::KeepRelative;
	EAttachmentRule AttachmentRotationRule  = EAttachmentRule::KeepRelative;
	EAttachmentRule AttachmentScaleRule     = EAttachmentRule::KeepRelative;

	MOVIESCENETRACKS_API void ApplyAttach(USceneComponent* NewAttachParent, USceneComponent* ChildComponentToAttach, const FName& SocketName) const;
};

struct FComponentDetachParams
{
	EDetachmentRule DetachmentLocationRule  = EDetachmentRule::KeepRelative;
	EDetachmentRule DetachmentRotationRule  = EDetachmentRule::KeepRelative;
	EDetachmentRule DetachmentScaleRule     = EDetachmentRule::KeepRelative;

	MOVIESCENETRACKS_API void ApplyDetach(USceneComponent* NewAttachParent, USceneComponent* ChildComponentToAttach, const FName& SocketName) const;
};

struct FAttachmentComponent
{
	FComponentAttachParamsDestination Destination;

	FComponentAttachParams AttachParams;
	FComponentDetachParams DetachParams;
};

struct FFadeComponentData
{
	FLinearColor FadeColor;
	bool bFadeAudio;
};

struct FFloatPropertyTraits
{
	static constexpr bool bIsComposite = false;

	using StorageType  = double;
	using CustomAccessorStorageType = float;
	using MetaDataType = TPropertyMetaData<>;
	using TraitsType = FFloatPropertyTraits;

	using FloatTraitsImpl = TDirectPropertyTraits<float>;

	static void GetObjectPropertyValue(const UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, double& OutValue)
	{
		const TCustomPropertyAccessor<FFloatPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FFloatPropertyTraits>&>(BaseCustomAccessor);
		const float GetterValue = (*CustomAccessor.Functions.Getter)(InObject);
		OutValue = (double)GetterValue;
	}
	static void GetObjectPropertyValue(const UObject* InObject, uint16 PropertyOffset, double& OutValue)
	{
		float Value;
		FloatTraitsImpl::GetObjectPropertyValue(InObject, PropertyOffset, Value);
		OutValue = (double)Value;
	}
	static void GetObjectPropertyValue(const UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, double& OutValue)
	{
		float Value;
		FloatTraitsImpl::GetObjectPropertyValue(InObject, PropertyBindings, Value);
		OutValue = (double)Value;
	}
	static void GetObjectPropertyValue(const UObject* InObject, const FName& PropertyPath, double& OutValue)
	{
		float Value;
		FloatTraitsImpl::GetObjectPropertyValue(InObject, PropertyPath, Value);
		OutValue = (double)Value;
	}

	static void SetObjectPropertyValue(UObject* InObject, const FCustomPropertyAccessor& BaseCustomAccessor, double InValue)
	{
		const TCustomPropertyAccessor<FFloatPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FFloatPropertyTraits>&>(BaseCustomAccessor);
		const float SetterValue = (float)InValue;
		(*CustomAccessor.Functions.Setter)(InObject, SetterValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, uint16 PropertyOffset, double InValue)
	{
		const float SetterValue = (float)InValue;
		FloatTraitsImpl::SetObjectPropertyValue(InObject, PropertyOffset, SetterValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, FTrackInstancePropertyBindings* PropertyBindings, double InValue)
	{
		const float SetterValue = (float)InValue;
		FloatTraitsImpl::SetObjectPropertyValue(InObject, PropertyBindings, SetterValue);
	}
};

struct FColorPropertyTraits
{
	using StorageType  = FIntermediateColor;
	using MetaDataType = TPropertyMetaData<EColorPropertyType>;

	static void GetObjectPropertyValue(const UObject* InObject, EColorPropertyType ColorType, const FCustomPropertyAccessor& BaseCustomAccessor, FIntermediateColor& OutValue)
	{
		const TCustomPropertyAccessor<FColorPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FColorPropertyTraits>&>(BaseCustomAccessor);
		OutValue = (*CustomAccessor.Functions.Getter)(InObject, ColorType);
	}
	static void GetObjectPropertyValue(const UObject* InObject, EColorPropertyType ColorType, uint16 PropertyOffset, FIntermediateColor& OutValue)
	{
		switch (ColorType)
		{
			case EColorPropertyType::Slate:  TIndirectPropertyTraits<FSlateColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue);  return;
			case EColorPropertyType::Linear: TIndirectPropertyTraits<FLinearColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue); return;
			case EColorPropertyType::Color:  TIndirectPropertyTraits<FColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue);       return;
		}
	}
	static void GetObjectPropertyValue(const UObject* InObject, EColorPropertyType ColorType, FTrackInstancePropertyBindings* PropertyBindings, FIntermediateColor& OutValue)
	{
		switch (ColorType)
		{
			case EColorPropertyType::Slate:  TIndirectPropertyTraits<FSlateColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue);  return;
			case EColorPropertyType::Linear: TIndirectPropertyTraits<FLinearColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue); return;
			case EColorPropertyType::Color:  TIndirectPropertyTraits<FColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue);       return;
		}
	}
	static void GetObjectPropertyValue(const UObject* InObject, EColorPropertyType ColorType, const FName& PropertyPath, StorageType& OutValue)
	{
		switch (ColorType)
		{
			case EColorPropertyType::Slate:  TIndirectPropertyTraits<FSlateColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyPath, OutValue);  return;
			case EColorPropertyType::Linear: TIndirectPropertyTraits<FLinearColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyPath, OutValue); return;
			case EColorPropertyType::Color:  TIndirectPropertyTraits<FColor, FIntermediateColor>::GetObjectPropertyValue(InObject, PropertyPath, OutValue);       return;
		}
	}

	static void SetObjectPropertyValue(UObject* InObject, EColorPropertyType ColorType, const FCustomPropertyAccessor& BaseCustomAccessor, const FIntermediateColor& InValue)
	{
		const TCustomPropertyAccessor<FColorPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FColorPropertyTraits>&>(BaseCustomAccessor);
		(*CustomAccessor.Functions.Setter)(InObject, ColorType, InValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, EColorPropertyType ColorType, uint16 PropertyOffset, const FIntermediateColor& InValue)
	{
		switch (ColorType)
		{
			case EColorPropertyType::Slate:  TIndirectPropertyTraits<FSlateColor, FIntermediateColor>::SetObjectPropertyValue(InObject, PropertyOffset, InValue);  return;
			case EColorPropertyType::Linear: TIndirectPropertyTraits<FLinearColor, FIntermediateColor>::SetObjectPropertyValue(InObject, PropertyOffset, InValue); return;
			case EColorPropertyType::Color:  TIndirectPropertyTraits<FColor, FIntermediateColor>::SetObjectPropertyValue(InObject, PropertyOffset, InValue);       return;
		}
	}
	static void SetObjectPropertyValue(UObject* InObject, EColorPropertyType ColorType, FTrackInstancePropertyBindings* PropertyBindings, const FIntermediateColor& InValue)
	{
		switch (ColorType)
		{
			case EColorPropertyType::Slate:  TIndirectPropertyTraits<FSlateColor, FIntermediateColor>::SetObjectPropertyValue(InObject, PropertyBindings, InValue);  return;
			case EColorPropertyType::Linear: TIndirectPropertyTraits<FLinearColor, FIntermediateColor>::SetObjectPropertyValue(InObject, PropertyBindings, InValue); return;
			case EColorPropertyType::Color:  TIndirectPropertyTraits<FColor, FIntermediateColor>::SetObjectPropertyValue(InObject, PropertyBindings, InValue);       return;
		}
	}

	static FIntermediateColor CombineComposites(EColorPropertyType InType, double InR, double InG, double InB, double InA)
	{
		return FIntermediateColor((float)InR, (float)InG, (float)InB, (float)InA);
	}
};

struct FDoubleVectorPropertyTraits
{
	using StorageType  = FDoubleIntermediateVector;
	using MetaDataType = TPropertyMetaData<FVectorPropertyMetaData>;

	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, const FCustomPropertyAccessor& BaseCustomAccessor, FDoubleIntermediateVector& OutValue)
	{
		const TCustomPropertyAccessor<FDoubleVectorPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FDoubleVectorPropertyTraits>&>(BaseCustomAccessor);
		OutValue = (*CustomAccessor.Functions.Getter)(InObject, MetaData);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, uint16 PropertyOffset, FDoubleIntermediateVector& OutValue)
	{
		switch (MetaData.NumChannels)
		{
			case 2: TIndirectPropertyTraits<FVector2d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue); return;
			case 3: TIndirectPropertyTraits<FVector3d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue); return;
			case 4: TIndirectPropertyTraits<FVector4d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue); return;
		}
	}
	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, FTrackInstancePropertyBindings* PropertyBindings, FDoubleIntermediateVector& OutValue)
	{
		switch (MetaData.NumChannels)
		{
			case 2: TIndirectPropertyTraits<FVector2d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue); return;
			case 3: TIndirectPropertyTraits<FVector3d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue); return;
			case 4: TIndirectPropertyTraits<FVector4d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue); return;
		}
	}
	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, const FName& PropertyPath, StorageType& OutValue)
	{
		switch (MetaData.NumChannels)
		{
			case 2: TIndirectPropertyTraits<FVector2d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyPath, OutValue); return;
			case 3: TIndirectPropertyTraits<FVector3d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyPath, OutValue); return;
			case 4: TIndirectPropertyTraits<FVector4d, FDoubleIntermediateVector>::GetObjectPropertyValue(InObject, PropertyPath, OutValue); return;
		}
	}

	static void SetObjectPropertyValue(UObject* InObject, FVectorPropertyMetaData MetaData, const FCustomPropertyAccessor& BaseCustomAccessor, const FDoubleIntermediateVector& InValue)
	{
		const TCustomPropertyAccessor<FDoubleVectorPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FDoubleVectorPropertyTraits>&>(BaseCustomAccessor);
		(*CustomAccessor.Functions.Setter)(InObject, MetaData, InValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, FVectorPropertyMetaData MetaData, uint16 PropertyOffset, const FDoubleIntermediateVector& InValue)
	{
		switch (MetaData.NumChannels)
		{
			case 2: TIndirectPropertyTraits<FVector2d, FDoubleIntermediateVector>::SetObjectPropertyValue(InObject, PropertyOffset, InValue); return;
			case 3: TIndirectPropertyTraits<FVector3d, FDoubleIntermediateVector>::SetObjectPropertyValue(InObject, PropertyOffset, InValue); return;
			case 4: TIndirectPropertyTraits<FVector4d, FDoubleIntermediateVector>::SetObjectPropertyValue(InObject, PropertyOffset, InValue); return;
		}

		checkf(false, TEXT("Invalid number of channels"));
	}
	static void SetObjectPropertyValue(UObject* InObject, FVectorPropertyMetaData MetaData, FTrackInstancePropertyBindings* PropertyBindings, const FDoubleIntermediateVector& InValue)
	{
		switch (MetaData.NumChannels)
		{
			case 2: TIndirectPropertyTraits<FVector2d, FDoubleIntermediateVector>::SetObjectPropertyValue(InObject, PropertyBindings, InValue); return;
			case 3: TIndirectPropertyTraits<FVector3d, FDoubleIntermediateVector>::SetObjectPropertyValue(InObject, PropertyBindings, InValue); return;
			case 4: TIndirectPropertyTraits<FVector4d, FDoubleIntermediateVector>::SetObjectPropertyValue(InObject, PropertyBindings, InValue); return;
		}

		checkf(false, TEXT("Invalid number of channels"));
	}

	static FDoubleIntermediateVector CombineComposites(FVectorPropertyMetaData MetaData, double InX, double InY, double InZ, double InW)
	{
		return FDoubleIntermediateVector(InX, InY, InZ, InW);
	}
};

struct FFloatVectorPropertyTraits
{
	using StorageType = FFloatIntermediateVector;
	using MetaDataType = TPropertyMetaData<FVectorPropertyMetaData>;

	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, const FCustomPropertyAccessor& BaseCustomAccessor, FFloatIntermediateVector& OutValue)
	{
		const TCustomPropertyAccessor<FFloatVectorPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FFloatVectorPropertyTraits>&>(BaseCustomAccessor);
		OutValue = (*CustomAccessor.Functions.Getter)(InObject, MetaData);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, uint16 PropertyOffset, FFloatIntermediateVector& OutValue)
	{
		switch (MetaData.NumChannels)
		{
		case 2: TIndirectPropertyTraits<FVector2f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue); return;
		case 3: TIndirectPropertyTraits<FVector3f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue); return;
		case 4: TIndirectPropertyTraits<FVector4f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyOffset, OutValue); return;
		}
	}
	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, FTrackInstancePropertyBindings* PropertyBindings, FFloatIntermediateVector& OutValue)
	{
		switch (MetaData.NumChannels)
		{
		case 2: TIndirectPropertyTraits<FVector2f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue); return;
		case 3: TIndirectPropertyTraits<FVector3f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue); return;
		case 4: TIndirectPropertyTraits<FVector4f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyBindings, OutValue); return;
		}
	}
	static void GetObjectPropertyValue(const UObject* InObject, FVectorPropertyMetaData MetaData, const FName& PropertyPath, StorageType& OutValue)
	{
		switch (MetaData.NumChannels)
		{
		case 2: TIndirectPropertyTraits<FVector2f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyPath, OutValue); return;
		case 3: TIndirectPropertyTraits<FVector3f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyPath, OutValue); return;
		case 4: TIndirectPropertyTraits<FVector4f, FFloatIntermediateVector>::GetObjectPropertyValue(InObject, PropertyPath, OutValue); return;
		}
	}

	static void SetObjectPropertyValue(UObject* InObject, FVectorPropertyMetaData MetaData, const FCustomPropertyAccessor& BaseCustomAccessor, const FFloatIntermediateVector& InValue)
	{
		const TCustomPropertyAccessor<FFloatVectorPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FFloatVectorPropertyTraits>&>(BaseCustomAccessor);
		(*CustomAccessor.Functions.Setter)(InObject, MetaData, InValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, FVectorPropertyMetaData MetaData, uint16 PropertyOffset, const FFloatIntermediateVector& InValue)
	{
		switch (MetaData.NumChannels)
		{
		case 2: TIndirectPropertyTraits<FVector2f, FFloatIntermediateVector>::SetObjectPropertyValue(InObject, PropertyOffset, InValue); return;
		case 3: TIndirectPropertyTraits<FVector3f, FFloatIntermediateVector>::SetObjectPropertyValue(InObject, PropertyOffset, InValue); return;
		case 4: TIndirectPropertyTraits<FVector4f, FFloatIntermediateVector>::SetObjectPropertyValue(InObject, PropertyOffset, InValue); return;
		}

		checkf(false, TEXT("Invalid number of channels"));
	}
	static void SetObjectPropertyValue(UObject* InObject, FVectorPropertyMetaData MetaData, FTrackInstancePropertyBindings* PropertyBindings, const FFloatIntermediateVector& InValue)
	{
		switch (MetaData.NumChannels)
		{
		case 2: TIndirectPropertyTraits<FVector2f, FFloatIntermediateVector>::SetObjectPropertyValue(InObject, PropertyBindings, InValue); return;
		case 3: TIndirectPropertyTraits<FVector3f, FFloatIntermediateVector>::SetObjectPropertyValue(InObject, PropertyBindings, InValue); return;
		case 4: TIndirectPropertyTraits<FVector4f, FFloatIntermediateVector>::SetObjectPropertyValue(InObject, PropertyBindings, InValue); return;
		}

		checkf(false, TEXT("Invalid number of channels"));
	}

	static FFloatIntermediateVector CombineComposites(FVectorPropertyMetaData MetaData, float InX, float InY, float InZ, float InW)
	{
		return FFloatIntermediateVector(InX, InY, InZ, InW);
	}
};

struct FBoolPropertyTraits
{
	static constexpr bool bIsComposite = false;

	struct FBoolMetaData
	{
		/** Size of the bitfield/bool property in bytes. Must be one of 1(uint8), 2(uint16), 4(uint32), 8(uint64), or 0 signifying that this is not a bitfield, but a regular bool.. */
		uint8 BitFieldSize = 0;
		uint8 BitIndex = 0;
	};
	using StorageType  = bool;
	using MetaDataType = TPropertyMetaData<FBoolMetaData>;
	using TraitsType   = TDirectPropertyTraits<bool>;
	using ParamType    = bool;

	/** Property Value Getters  */
	static void GetObjectPropertyValue(const UObject* InObject, FBoolMetaData MetaData, const FCustomPropertyAccessor& BaseCustomAccessor, bool& OutValue)
	{
		const TCustomPropertyAccessor<TraitsType>& CustomAccessor = static_cast<const TCustomPropertyAccessor<TraitsType>&>(BaseCustomAccessor);
		OutValue = (*CustomAccessor.Functions.Getter)(InObject);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FBoolMetaData MetaData, uint16 PropertyOffset, bool& OutValue)
	{
		const void* PropertyAddress = reinterpret_cast<const uint8*>(InObject) + PropertyOffset;
		switch(MetaData.BitFieldSize)
		{
		case 0: OutValue = (*reinterpret_cast<const bool  *>(PropertyAddress)); return; // 0 means no bitfield
		case 1: OutValue = (*reinterpret_cast<const uint8 *>(PropertyAddress) & (uint8 (1u) << MetaData.BitIndex)); return;
		case 2: OutValue = (*reinterpret_cast<const uint16*>(PropertyAddress) & (uint16(1u) << MetaData.BitIndex)); return;
		case 4: OutValue = (*reinterpret_cast<const uint32*>(PropertyAddress) & (uint32(1u) << MetaData.BitIndex)); return;
		case 8: OutValue = (*reinterpret_cast<const uint64*>(PropertyAddress) & (uint64(1u) << MetaData.BitIndex)); return;
		}
	}
	static void GetObjectPropertyValue(const UObject* InObject, FBoolMetaData MetaData, FTrackInstancePropertyBindings* PropertyBindings, bool& OutValue)
	{
		OutValue = PropertyBindings->GetCurrentValue<bool>(*InObject);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FBoolMetaData MetaData, const FName& PropertyPath, bool& OutValue)
	{
		TOptional<bool> Property = FTrackInstancePropertyBindings::StaticValue<bool>(InObject, *PropertyPath.ToString());
		if (Property)
		{
			OutValue = MoveTemp(Property.GetValue());
		}
	}

	/** Property Value Setters  */
	static void SetObjectPropertyValue(UObject* InObject, FBoolMetaData MetaData, const FCustomPropertyAccessor& BaseCustomAccessor, bool InValue)
	{
		const TCustomPropertyAccessor<TraitsType>& CustomAccessor = static_cast<const TCustomPropertyAccessor<TraitsType>&>(BaseCustomAccessor);
		(*CustomAccessor.Functions.Setter)(InObject, InValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, FBoolMetaData MetaData, uint16 PropertyOffset, bool InValue)
	{
		void* PropertyAddress = reinterpret_cast<uint8*>(InObject) + PropertyOffset;

		// Perform a branchless set by getting the current value, removing the bit, then a bitwise or with InValue in the bit's position.
		// For example, bit index 4 of a uint8 field:
		//			f(true)  = (Value & 0b11101111) | 0b00010000
		//			f(false) = (Value & 0b11101111) | 0b00000000
		switch(MetaData.BitFieldSize)
		{
		case 0: *reinterpret_cast<bool  *>(PropertyAddress) = InValue; return; // 0 means no bitfield
		case 1: *reinterpret_cast<uint8 *>(PropertyAddress) = (*reinterpret_cast<uint8 *>(PropertyAddress) & ~(uint8 (1u)  << MetaData.BitIndex)) | (uint8 (InValue) << MetaData.BitIndex); return;
		case 2: *reinterpret_cast<uint16*>(PropertyAddress) = (*reinterpret_cast<uint16*>(PropertyAddress) & ~(uint16(1u)  << MetaData.BitIndex)) | (uint16(InValue) << MetaData.BitIndex); return;
		case 4: *reinterpret_cast<uint32*>(PropertyAddress) = (*reinterpret_cast<uint32*>(PropertyAddress) & ~(uint32(1u)  << MetaData.BitIndex)) | (uint32(InValue) << MetaData.BitIndex); return;
		case 8: *reinterpret_cast<uint64*>(PropertyAddress) = (*reinterpret_cast<uint64*>(PropertyAddress) & ~(uint64(1u)  << MetaData.BitIndex)) | (uint64(InValue) << MetaData.BitIndex); return;
		}
	}
	static void SetObjectPropertyValue(UObject* InObject, FBoolMetaData MetaData, FTrackInstancePropertyBindings* PropertyBindings, bool InValue)
	{
		PropertyBindings->CallFunction<bool>(*InObject, InValue);
	}
};

struct FObjectPropertyTraits
{
	static constexpr bool bIsComposite = false;

	struct FObjectMetadata
	{
		TObjectPtr<UClass> ObjectClass = UObject::StaticClass();
		bool bAllowsClear = true;
	};

	using StorageType = FObjectComponent;
	//using CustomAccessorStorageType = float;
	using MetaDataType = TPropertyMetaData<FObjectMetadata>;
	using TraitsType = FObjectPropertyTraits;
	using ParamType = UObject*;

	using ObjectTraitsImpl = TIndirectPropertyTraits<UObject*, FObjectComponent>;

	static void GetObjectPropertyValue(const UObject* InObject, FObjectMetadata ObjectMetadata, const FCustomPropertyAccessor& BaseCustomAccessor, FObjectComponent& OutValue)
	{
		const TCustomPropertyAccessor<FObjectPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FObjectPropertyTraits>&>(BaseCustomAccessor);
		OutValue = (*CustomAccessor.Functions.Getter)(InObject, ObjectMetadata);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FObjectMetadata ObjectMetadata, uint16 PropertyOffset, FObjectComponent& OutValue)
	{
		ObjectTraitsImpl::GetObjectPropertyValue(InObject, PropertyOffset, OutValue);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FObjectMetadata ObjectMetadata, FTrackInstancePropertyBindings* PropertyBindings, FObjectComponent& OutValue)
	{
		ObjectTraitsImpl::GetObjectPropertyValue(InObject, PropertyBindings, OutValue);
	}
	static void GetObjectPropertyValue(const UObject* InObject, FObjectMetadata ObjectMetadata, const FName& PropertyPath, StorageType& OutValue)
	{
		ObjectTraitsImpl::GetObjectPropertyValue(InObject, PropertyPath, OutValue);
	}

	static bool CanAssignValue(const FObjectMetadata& ObjectMetadata, UObject* DesiredValue)
	{
		if (!ObjectMetadata.ObjectClass)
		{
			return false;
		}
		else if (!DesiredValue)
		{
			return ObjectMetadata.bAllowsClear;
		}
		else if (DesiredValue->GetClass() != nullptr)
		{
			return DesiredValue->GetClass()->IsChildOf(ObjectMetadata.ObjectClass);
		}
		return false;
	}

	static void SetObjectPropertyValue(UObject* InObject, FObjectMetadata ObjectMetadata, const FCustomPropertyAccessor& BaseCustomAccessor, const FObjectComponent& InValue)
	{
		const TCustomPropertyAccessor<FObjectPropertyTraits>& CustomAccessor = static_cast<const TCustomPropertyAccessor<FObjectPropertyTraits>&>(BaseCustomAccessor);
		(*CustomAccessor.Functions.Setter)(InObject, ObjectMetadata, InValue);
	}
	static void SetObjectPropertyValue(UObject* InObject, FObjectMetadata ObjectMetadata, uint16 PropertyOffset, const FObjectComponent& InValue)
	{
		if (CanAssignValue(ObjectMetadata, InValue.GetObject()))
		{
			ObjectTraitsImpl::SetObjectPropertyValue(InObject, PropertyOffset, InValue);
		}
	}
	static void SetObjectPropertyValue(UObject* InObject, FObjectMetadata ObjectMetadata, FTrackInstancePropertyBindings* PropertyBindings, const FObjectComponent& InValue)
	{
		if (CanAssignValue(ObjectMetadata, InValue.GetObject()))
		{
			ObjectTraitsImpl::SetObjectPropertyValue(InObject, PropertyBindings, InValue);
		}
	}
};

using FBytePropertyTraits               = TDirectPropertyTraits<uint8, false>;
using FEnumPropertyTraits               = TDirectPropertyTraits<uint8, false>;
using FIntPropertyTraits                = TDirectPropertyTraits<int32, false>;
using FDoublePropertyTraits             = TDirectPropertyTraits<double, false>;
using FTransformPropertyTraits          = TIndirectPropertyTraits<FTransform, FIntermediate3DTransform>;
using FEulerTransformPropertyTraits     = TIndirectPropertyTraits<FEulerTransform, FIntermediate3DTransform>;
using FComponentTransformPropertyTraits = TDirectPropertyTraits<FIntermediate3DTransform>;
using FStringPropertyTraits			    = TDirectPropertyTraits<FString>;

using FFloatParameterTraits             = TIndirectPropertyTraits<float, double, false>;
using FColorParameterTraits             = TIndirectPropertyTraits<FLinearColor, FIntermediateColor>;

struct FMovieSceneTracksComponentTypes
{
	MOVIESCENETRACKS_API ~FMovieSceneTracksComponentTypes();

	TPropertyComponents<FBoolPropertyTraits> Bool;
	TPropertyComponents<FBytePropertyTraits> Byte;
	TPropertyComponents<FEnumPropertyTraits> Enum;
	TPropertyComponents<FIntPropertyTraits> Integer;
	TPropertyComponents<FFloatPropertyTraits> Float;
	TPropertyComponents<FDoublePropertyTraits> Double;
	TPropertyComponents<FColorPropertyTraits> Color;
	TPropertyComponents<FFloatVectorPropertyTraits> FloatVector;
	TPropertyComponents<FDoubleVectorPropertyTraits> DoubleVector;
	TPropertyComponents<FTransformPropertyTraits> Transform;
	TPropertyComponents<FEulerTransformPropertyTraits> EulerTransform;
	TPropertyComponents<FComponentTransformPropertyTraits> ComponentTransform;
	TPropertyComponents<FStringPropertyTraits> String;
	TPropertyComponents<FObjectPropertyTraits> Object;

	TPropertyComponents<FFloatParameterTraits> FloatParameter;
	TPropertyComponents<FColorParameterTraits> ColorParameter;

	TComponentTypeID<FSourceDoubleChannel> QuaternionRotationChannel[3];

	TComponentTypeID<FConstraintComponentData> ConstraintChannel;

	TComponentTypeID<TWeakObjectPtr<USceneComponent>> AttachParent;
	TComponentTypeID<FAttachmentComponent> AttachComponent;
	TComponentTypeID<FMovieSceneObjectBindingID> AttachParentBinding;
	TComponentTypeID<FPerlinNoiseParams> FloatPerlinNoiseChannel;
	TComponentTypeID<FPerlinNoiseParams> DoublePerlinNoiseChannel;

	TComponentTypeID<FMovieSceneSkeletalAnimationComponentData> SkeletalAnimation;

	TComponentTypeID<FComponentMaterialInfo> ComponentMaterialInfo;

	TComponentTypeID<FName> BoolParameterName;
	TComponentTypeID<FName> ScalarParameterName;
	TComponentTypeID<FName> Vector2DParameterName;
	TComponentTypeID<FName> VectorParameterName;
	TComponentTypeID<FName> ColorParameterName;
	TComponentTypeID<FName> TransformParameterName;

	TComponentTypeID<FMaterialParameterInfo> ScalarMaterialParameterInfo;
	TComponentTypeID<FMaterialParameterInfo> ColorMaterialParameterInfo;
	TComponentTypeID<FMaterialParameterInfo> VectorMaterialParameterInfo;

	TComponentTypeID<FObjectComponent> BoundMaterial;
	TComponentTypeID<TWeakObjectPtr<UMaterialParameterCollection>> MPC;

	TComponentTypeID<FFadeComponentData> Fade;

	TComponentTypeID<FMovieSceneAudioComponentData> Audio;
	TComponentTypeID<FMovieSceneAudioInputData> AudioInputs;
	TComponentTypeID<FName> AudioTriggerName;

	TComponentTypeID<FMovieSceneCameraShakeComponentData> CameraShake;
	TComponentTypeID<FMovieSceneCameraShakeInstanceData> CameraShakeInstance;

	struct
	{
		TCustomPropertyRegistration<FBoolPropertyTraits> Bool;
		TCustomPropertyRegistration<FBytePropertyTraits> Byte;
		TCustomPropertyRegistration<FEnumPropertyTraits> Enum;
		TCustomPropertyRegistration<FIntPropertyTraits> Integer;
		TCustomPropertyRegistration<FFloatPropertyTraits> Float;
		TCustomPropertyRegistration<FDoublePropertyTraits> Double;
		TCustomPropertyRegistration<FColorPropertyTraits> Color;
		TCustomPropertyRegistration<FFloatVectorPropertyTraits> FloatVector;
		TCustomPropertyRegistration<FDoubleVectorPropertyTraits> DoubleVector;
		TCustomPropertyRegistration<FComponentTransformPropertyTraits, 1> ComponentTransform;
		TCustomPropertyRegistration<FObjectPropertyTraits> Object;
	} Accessors;

	struct
	{
		FComponentTypeID BoundMaterialChanged;
		FComponentTypeID CustomPrimitiveData;
		FComponentTypeID Slomo;
		FComponentTypeID Visibility;
	} Tags;

	TComponentTypeID<FLevelVisibilityComponentData> LevelVisibility;
	TComponentTypeID<FMovieSceneDataLayerComponentData> DataLayer;

	static MOVIESCENETRACKS_API void Destroy();

	static MOVIESCENETRACKS_API FMovieSceneTracksComponentTypes* Get();

private:
	FMovieSceneTracksComponentTypes();
};

} // namespace MovieScene
} // namespace UE
