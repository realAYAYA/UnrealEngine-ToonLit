// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "BlendableInterface.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;

/** Where to place a material node in the post processing graph. */
UENUM()
enum EBlendableLocation
{
	/** Input0:former pass color, Input1:SeparateTranslucency. */
	BL_AfterTonemapping UMETA(DisplayName="After Tonemapping"),
	/** Input0:former pass color, Input1:SeparateTranslucency. */
	BL_BeforeTonemapping UMETA(DisplayName="Before Tonemapping"),
	/** Input0:former pass color, Input1:SeparateTranslucency. */
	BL_BeforeTranslucency UMETA(DisplayName="Before Translucency"),
	/**
	* Input0:former pass color, Input1:SeparateTranslucency, Input2: BloomOutput
	*/
	BL_ReplacingTonemapper UMETA(DisplayName="Replacing the Tonemapper"),
//	BL_AfterOpaque,
//	BL_AfterFog,
//	BL_AfterTranslucency,
//	BL_AfterPostProcessAA,

	/** Input0:former pass color. */
	BL_SSRInput UMETA(DisplayName = "SSR Input"),

	BL_MAX,
};

/** Dummy class needed to support Cast<IBlendableInterface>(Object). */
UINTERFACE()
class ENGINE_API UBlendableInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * Derive from this class if you want to be blended by the PostProcess blending e.g. PostproceessVolume
 */
class IBlendableInterface
{
	GENERATED_IINTERFACE_BODY()
		
	/** @param Weight 0..1, excluding 0, 1=fully take the values from this object, crash if outside the valid range. */
	virtual void OverrideBlendableSettings(class FSceneView& View, float Weight) const = 0;
};

struct FPostProcessMaterialNode
{
	// Default constructor
	FPostProcessMaterialNode()
		: MaterialInterface(0), bIsMID(false), Location(BL_MAX), Priority(0)
	{
		check(!IsValid());
	}

	// Constructor
	FPostProcessMaterialNode(UMaterialInterface* InMaterialInterface, EBlendableLocation InLocation, int32 InPriority, bool InbIsBlendable)
		: MaterialInterface(InMaterialInterface), bIsMID(false), Location(InLocation), Priority(InPriority), bIsBlendable(InbIsBlendable)
	{
		check(IsValid());
	}

	// Constructor
	FPostProcessMaterialNode(UMaterialInstanceDynamic* InMID, EBlendableLocation InLocation, int32 InPriority, bool InbIsBlendable)
		: MaterialInterface((UMaterialInterface*)InMID), bIsMID(true), Location(InLocation), Priority(InPriority), bIsBlendable(InbIsBlendable)
	{
		check(IsValid());
	}

	UMaterialInterface* GetMaterialInterface() const { return MaterialInterface; }
	/** Only call if you are sure it's an MID. */
	UMaterialInstanceDynamic* GetMID() const { check(bIsMID); return (UMaterialInstanceDynamic*)MaterialInterface; }

	/** For type safety in FBlendableManager. */
	static const FName& GetFName()
	{
		static const FName Name = FName(TEXT("FPostProcessMaterialNode"));

		return Name;
	}

	struct FCompare
	{
		FORCEINLINE bool operator()(const FPostProcessMaterialNode& P1, const FPostProcessMaterialNode& P2) const
		{
			if(P1.Location < P2.Location) return true;
			if(P1.Location > P2.Location) return false;

			if(P1.Priority < P2.Priority) return true;
			if(P1.Priority > P2.Priority) return false;

			return false;
		}
	};

	EBlendableLocation GetLocation() const { return Location; }
	int32 GetPriority() const { return Priority; }
	bool GetIsBlendable() const { return bIsBlendable; }

	bool IsValid() const { return MaterialInterface != 0; }

private:
	UMaterialInterface* MaterialInterface;

	/** if MaterialInterface is an MID, needed for GetMID(). */
	bool bIsMID;

	EBlendableLocation Location;

	/** Default is 0. */
	int32 Priority;

	/** Flag for whether the material should be blendable */
	bool bIsBlendable;
};


