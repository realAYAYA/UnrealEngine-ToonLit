// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "RenderResource.h"
#include "Curves/CurveLinearColor.h"
#include "SpecularProfile.generated.h"

#define MAX_SPECULAR_PROFILE_COUNT 256

class UTexture2D;
class FRDGBuilder;
class FTextureReference;

/** List of niagara solvers */
UENUM(BlueprintType)
enum class ESpecularProfileFormat : uint8
{
	ViewLightVector UMETA(DisplayName = "View / Light"),
	HalfVector UMETA(DisplayName = "Half Angle"),
};

// struct with all the settings we want in USpecularProfile, separate to make it easer to pass this data around in the engine.
USTRUCT(BlueprintType)
struct FSpecularProfileStruct
{
	GENERATED_USTRUCT_BODY()
	
	/**
	 * Define the texture used as a specular profile
	 */
	UPROPERTY(Category = "Common", EditAnywhere, BlueprintReadOnly, meta=(DisplayName="LUT Format"))
	ESpecularProfileFormat Format;

	/**
	* Define the view facing color
	*/
	UPROPERTY(Category = "Procedural", EditAnywhere, meta = (AllowZoomOutput = "false", ShowZoomButtons = "false", ViewMinInput = "0", ViewMaxInput = "1", ViewMinOutput = "0", ViewMaxOutput = "1", TimelineLength = "1", ShowInputGridNumbers="false", ShowOutputGridNumbers="false"))
	FRuntimeCurveLinearColor ViewColor;

	/**
	* Define the light facing color
	*/
	UPROPERTY(Category = "Procedural", EditAnywhere, meta = (AllowZoomOutput = "false", ShowZoomButtons = "false", ViewMinInput = "0", ViewMaxInput = "1", ViewMinOutput = "0", ViewMaxOutput = "1", TimelineLength = "1", ShowInputGridNumbers="false", ShowOutputGridNumbers="false"))
	FRuntimeCurveLinearColor LightColor;

	/**
	 * Define the texture used as a specular profile
	 */
	UPROPERTY(Category  ="Texture", EditAnywhere, BlueprintReadOnly, meta=(DisplayName="Texture"))
	TObjectPtr<UTexture2D> Texture;

	FSpecularProfileStruct();

	bool IsProcedural() const { return Texture == nullptr; }

	void Invalidate()
	{
		*this = FSpecularProfileStruct();
	}
};

/**
 * Specular profile asset, can be specified at a material. 
 * Don't change at runtime. All properties in here are per material.
 */
UCLASS(autoexpandcategories = SpecularProfile, MinimalAPI)
class USpecularProfile : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = USpecularProfile, EditAnywhere, meta = (ShowOnlyInnerProperties))
	struct FSpecularProfileStruct Settings;

	UPROPERTY()
	FGuid Guid;

	//~ Begin UObject Interface
	virtual void BeginDestroy();
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
	//~ End UObject Interface
};

namespace SpecularProfileAtlas
{
// Atlas - Initializes or updates the contents of the specular profile texture.
ENGINE_API void UpdateSpecularProfileTextureAtlas(FRDGBuilder& GraphBuilder, EShaderPlatform ShaderPlatform);

// Atlas - Returns the specular profile texture if it exists, or null.
ENGINE_API FRHITexture* GetSpecularProfileTextureAtlas();

// Atlas - Returns the specular profile texture if it exists, or black.
ENGINE_API FRHITexture* GetSpecularProfileTextureAtlasWithFallback();

// Profile - Initializes or updates the contents of the specular profile texture.
ENGINE_API int32 AddOrUpdateProfile(const USpecularProfile* InProfile, const FGuid& InGuid, const FSpecularProfileStruct InSettings, const FTextureReference* InTexture);

// Profile - Returns the specular profile ID shader parameter name
ENGINE_API FName GetSpecularProfileParameterName(const USpecularProfile* InProfile);

// Profile - Returns the specular profile ID for a given Specular Profile object
ENGINE_API float GetSpecularProfileId(const USpecularProfile* In);
}