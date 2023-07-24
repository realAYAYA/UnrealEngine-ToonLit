// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"

#include "TextureImportSettings.generated.h"

struct FPropertyChangedEvent;

/* What CompressionSettings runtime format should imported floating point textures use
 */
UENUM()
enum class ETextureImportFloatingPointFormat : uint8
{
	/* Use "HDR" RGBA16F */
	HDR_F16 = 0,
	/* Use "HDRCompressed" , BC6H */
	HDRCompressed_BC6,
	/* Use 32-bit float formats if the source is 32-bit, otherwise use 16-bit HDR */
	HDR_F32_or_F16,

	PreviousDefault = HDR_F16  UMETA(Hidden) // legacy behavior
};

UCLASS(config=Editor, defaultconfig, meta=(DisplayName="Texture Import"))
class TEXTUREUTILITIESCOMMON_API UTextureImportSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()
	
public:

	UPROPERTY(config, EditAnywhere, Category=VirtualTextures, meta = (
		DisplayName = "Auto Virtual Texturing Size",
		ToolTip = "Automatically enable the 'Virtual Texture Streaming' texture setting for textures larger than or equal to this size. This setting will not affect existing textures in the project."))
	int32 AutoVTSize = 4096;
	
	UPROPERTY(config, EditAnywhere, Category=ImportSettings, meta = (
		DisplayName = "Turn on NormalizeNormals for normal maps",
		ToolTip = "NormalizeNormals makes more correct normals in mip maps; it is recommended, but can be turned off to maintain legacy behavior. This setting is applied to newly imported textures, it does not affect existing textures in the project."))
	bool bEnableNormalizeNormals = true;
	
	UPROPERTY(config, EditAnywhere, Category=ImportSettings, meta = (
		DisplayName = "Turn on fast mip generation filter",
		ToolTip = "Use the fast mip filter on new textures; it is recommended, but can be turned off to maintain legacy behavior. This setting is applied to newly imported textures, it does not affect existing textures in the project."))
	bool bEnableFastMipFilter = true;
	
	UPROPERTY(config, EditAnywhere, Category=ImportSettings, meta = (
		DisplayName = "CompressionFormat to use for new float textures",
		ToolTip = "Optionally use HDRCompressed (BC6H), or 32-bit adaptively, instead of HDR (RGBA16F) for floating point textures.  This setting is applied to newly imported textures, it does not affect existing textures in the project."))
	ETextureImportFloatingPointFormat CompressedFormatForFloatTextures = ETextureImportFloatingPointFormat::PreviousDefault;

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ End UObject Interface
};


#if WITH_EDITOR
class UTexture;

namespace UE::TextureUtilitiesCommon
{
	/* Set default properties on Texture for newly imported textures, or reimports.
	*  Should be called after all texture properties are set, before PostEditChange() 
	*/
	TEXTUREUTILITIESCOMMON_API void ApplyDefaultsForNewlyImportedTextures(UTexture * Texture, bool bIsReimport);

}
#endif