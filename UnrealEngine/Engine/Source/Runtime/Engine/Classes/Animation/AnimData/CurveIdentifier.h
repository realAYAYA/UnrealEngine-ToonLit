// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/SmartName.h"
#include "Animation/AnimCurveTypes.h"

#include "CurveIdentifier.generated.h"

/** Enum used to determine a component channel of a transform curve */
UENUM(BlueprintType)
enum class ETransformCurveChannel : uint8
{
	Position,
	Rotation,
	Scale,
	Invalid
};

/** Enum used to determine an axis channel of a vector curve */
UENUM(BlueprintType)
enum class EVectorCurveChannel : uint8
{
	X,
	Y,
	Z,
	Invalid
};

/** Script-friendly structure for identifying an animation curve.
* Wrapping the unique type and smart-name for use within the AnimDataController API. */
USTRUCT(BlueprintType)
struct ENGINE_API FAnimationCurveIdentifier
{
	GENERATED_USTRUCT_BODY();

	FAnimationCurveIdentifier() : CurveType(ERawCurveTrackTypes::RCT_MAX), Channel(ETransformCurveChannel::Invalid), Axis(EVectorCurveChannel::Invalid) {}
	FAnimationCurveIdentifier(const FSmartName& InSmartName, ERawCurveTrackTypes InCurveType) : InternalName(InSmartName), CurveType(InCurveType), Channel(ETransformCurveChannel::Invalid), Axis(EVectorCurveChannel::Invalid) {}
	
	explicit FAnimationCurveIdentifier(const SmartName::UID_Type& InUID, ERawCurveTrackTypes InCurveType) : CurveType(InCurveType)
	{
		InternalName.UID = InUID;
	}

	bool operator==(const FAnimationCurveIdentifier& Other) const
	{
		return (InternalName == Other.InternalName && CurveType == Other.CurveType 
			&& Channel == Other.Channel && Axis == Other.Axis);
	}
	bool operator!=(const FAnimationCurveIdentifier& Other) const
	{
		return !(*this == Other);
	}

	bool IsValid() const
	{
		return InternalName.IsValid() && CurveType != ERawCurveTrackTypes::RCT_MAX;
	}

	FAnimationCurveIdentifier& operator=(const FAnimationCurveIdentifier& Other)
	{
		InternalName = Other.InternalName;
		CurveType = Other.CurveType;
		Channel = Other.Channel;
		Axis = Other.Axis;

		return *this;
	}

	FSmartName InternalName;
	ERawCurveTrackTypes CurveType;
	ETransformCurveChannel Channel;
	EVectorCurveChannel Axis;
};

template<>
struct TStructOpsTypeTraits<FAnimationCurveIdentifier> : public TStructOpsTypeTraitsBase2<FAnimationCurveIdentifier>
{
	enum
	{
		WithCopy = true,
		WithIdenticalViaEquality = true
	};
};

/** Script-exposed functionality for wrapping native functionality and constructing valid FAnimationCurveIdentifier instances */
UCLASS()
class ENGINE_API UAnimationCurveIdentifierExtensions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	* @return	Whether or not the Curve Identifier is valid
	*/
	UFUNCTION(BlueprintPure, Category = Curve, meta = (ScriptMethod))
	static bool IsValid(UPARAM(ref) FAnimationCurveIdentifier& Identifier)
	{
		return Identifier.IsValid();
	}

	/**
	* @return	The name used for displaying the Curve Identifier
	*/
	UFUNCTION(BlueprintPure, Category = Curve, meta = (ScriptMethod))
	static FName GetName(UPARAM(ref) FAnimationCurveIdentifier& Identifier)
	{
		return Identifier.InternalName.DisplayName;
	}

	/**
	* @return	The animation curve type for the curve represented by the Curve Identifier
	*/
	UFUNCTION(BlueprintPure, Category = Curve, meta = (ScriptMethod))
	static ERawCurveTrackTypes GetType(UPARAM(ref) FAnimationCurveIdentifier& Identifier)
	{
		return Identifier.CurveType;
	}

#if WITH_EDITOR
	/**
	* Constructs a valid FAnimationCurveIdentifier instance. Ensuring that the underlying SmartName exists on the provided Skeleton for the provided curve type.
	* If it is not found initially it will add it to the Skeleton thus modifying it.
	*
	* @param	InSkeleton			Skeleton on which to look for or add the curve name
	* @param	Name				Name of the curve to find or add
	* @param	CurveType			Type of the curve to find or add
	*
	* @return	Valid FAnimationCurveIdentifier if the operation was successful
	*/
	UFUNCTION(BlueprintCallable, Category = Curves, meta = (ScriptMethod))
	static FAnimationCurveIdentifier GetCurveIdentifier(USkeleton* InSkeleton, FName Name, ERawCurveTrackTypes CurveType);

	/**
	* Tries to construct a valid FAnimationCurveIdentifier instance. It tries to find the underlying SmartName on the provided Skeleton for the provided curve type.
	*
	* @param	InSkeleton			Skeleton on which to look for the curve name
	* @param	Name				Name of the curve to find
	* @param	CurveType			Type of the curve to find
	*
	* @return	Valid FAnimationCurveIdentifier if the name exists on the skeleton and the operation was successful, invalid otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = Curves, meta = (ScriptMethod))
	static FAnimationCurveIdentifier FindCurveIdentifier(const USkeleton* InSkeleton, FName Name, ERawCurveTrackTypes CurveType);

	/**
	* Retrieves all curve identifiers for a specific curve types from the provided Skeleton
	*
	* @param	InSkeleton			Skeleton from which to retrieve the curve identifiers
	* @param	CurveType			Type of the curve identifers to retrieve
	*
	* @return	Array of FAnimationCurveIdentifier instances each representing a unique curve if the operation was successful, empyty array otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = Curves, meta = (ScriptMethod))
	static TArray<FAnimationCurveIdentifier> GetCurveIdentifiers(USkeleton* InSkeleton, ERawCurveTrackTypes CurveType);

	/**
	* Converts a valid FAnimationCurveIdentifier instance with RCT_Transform curve type to target a child curve.
	*
	* @param	InOutIdentifier		[out] Identifier to be converted
	* @param	Channel				Channel to target
	* @param	Axis				Axis within channel to target
	*
	* @return	Valid FAnimationCurveIdentifier if the operation was successful
	*/
	UFUNCTION(BlueprintCallable, Category = Curves, meta = (ScriptMethod))
	static bool GetTransformChildCurveIdentifier(UPARAM(ref) FAnimationCurveIdentifier& InOutIdentifier, ETransformCurveChannel Channel, EVectorCurveChannel Axis);
#endif // WITH_EDITOR
};

