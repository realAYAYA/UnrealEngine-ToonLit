// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "PerPlatformProperties.h"
#include "VirtualTextureBuilder.generated.h"

enum class EShadingPath;

#if WITH_EDITOR

/** Description object used to build the contents of a UVirtualTextureBuilder. */
struct FVirtualTextureBuildDesc
{
	uint64 BuildHash = 0;

	int32 LayerCount = 0;
	TArray<ETextureSourceFormat, TInlineAllocator<4>> LayerFormats;
	TArray<FTextureFormatSettings, TInlineAllocator<4>> LayerFormatSettings;

	int32 TileSize = 0;
	int32 TileBorderSize = 0;

	TEnumAsByte<enum TextureGroup> LODGroup = TEXTUREGROUP_World;
	ETextureLossyCompressionAmount LossyCompressionAmount = TLCA_Default;

	bool bContinuousUpdate = false;
	bool bSinglePhysicalSpace = false;

	int32 NumMips = 0;

	uint32 InSizeX = 0;
	uint32 InSizeY = 0;
	uint8 const* InData = nullptr;
};

#endif

/**
 * Container for a UVirtualTexture2D that can be built from a FVirtualTextureBuildDesc description.
 * This has a simple BuildTexture() interface but we may want to extend in the future to support partial builds
 * or other more blueprint driven approaches for data generation.
 */
UCLASS(ClassGroup = Rendering, BlueprintType, MinimalAPI)
class UVirtualTextureBuilder : public UObject
{
public:
	GENERATED_UCLASS_BODY()
	ENGINE_API ~UVirtualTextureBuilder();

	/** The UTexture object. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Texture)
	TObjectPtr<class UVirtualTexture2D> Texture;

	/** The UTexture object for Mobile rendering. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Texture)
	TObjectPtr<class UVirtualTexture2D> TextureMobile;

	/** Some client defined hash of that defines how the Texture was built. */
	UPROPERTY()
	uint64 BuildHash;

	/** Virtual texture for a specific shading path */
	ENGINE_API UVirtualTexture2D* GetVirtualTexture(EShadingPath ShadingPath) const;

	/** Whether to use a separate texture for Mobile rendering. A separate texure will be built using mobile preview editor mode */
	UPROPERTY(EditAnywhere, Category = Texture)
	bool bSeparateTextureForMobile = false;

	/** Per platform overrides for cooking the virtual texture. */
	UPROPERTY(EditAnywhere, Category = Texture)
	FPerPlatformBool EnableCookPerPlatform;

#if WITH_EDITOR
	/** Creates a new UVirtualTexture2D and stores it in the contained Texture. */
	ENGINE_API void BuildTexture(EShadingPath ShadingPath, FVirtualTextureBuildDesc const& BuildDesc);
#endif

protected:
	//~ Begin UObject Interface
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void PostLoad() override;
	//~ End UObject Interface
};
