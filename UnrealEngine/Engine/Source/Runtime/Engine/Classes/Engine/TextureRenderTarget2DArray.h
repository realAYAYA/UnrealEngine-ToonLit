// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/TextureRenderTarget.h"
#include "TextureRenderTarget2DArray.generated.h"

class FTextureResource;
struct FPropertyChangedEvent;

/**
 * TextureRenderTarget2DArray
 *
 * 2D Array render target texture resource. This can be used as a target
 * for rendering as well as rendered as a regular 2DArray texture resource.
 *
 */
UCLASS(hidecategories=Object, hidecategories=Texture, MinimalAPI)
class UTextureRenderTarget2DArray : public UTextureRenderTarget
{
	GENERATED_UCLASS_BODY()

	/** The width of the texture.												*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureRenderTarget2DArray, AssetRegistrySearchable)
	int32 SizeX;

	/** The height of the texture.												*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TextureRenderTarget2DArray, AssetRegistrySearchable)
	int32 SizeY;

	/** The slices of the texture.												*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TextureRenderTarget2DArray, AssetRegistrySearchable)
	int32 Slices;

	/** the color the texture is cleared to */
	UPROPERTY()
	FLinearColor ClearColor;

	/** The format of the texture data.											*/
	/** Normally the format is derived from bHDR, this allows code to set the format explicitly. */
	UPROPERTY()
	TEnumAsByte<enum EPixelFormat> OverrideFormat;

	/** Whether to support storing HDR values, which requires more memory. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2DArray, AssetRegistrySearchable)
	uint8 bHDR:1;

	/** True to force linear gamma space for this render target */
	UPROPERTY()
	uint8 bForceLinearGamma:1;

	/** 
	* Initialize the settings needed to create a render target texture and create its resource
	* @param	InSizeX - width of the texture
	* @param	InSizeY - height of the texture
	* @param	InSlices - slices of the texture
	* @param	InFormat - format of the texture
	*/
	ENGINE_API void Init(uint32 InSizeX, uint32 InSizeY, uint32 InSlices, EPixelFormat InFormat);

	/** Initializes the render target, the format will be derived from the value of bHDR. */
	ENGINE_API void InitAutoFormat(uint32 InSizeX, uint32 InSizeY, uint32 InSlices);

	ENGINE_API void UpdateResourceImmediate(bool bClearRenderTarget/*=true*/);

	/**
	 * Utility for creating a new UTexture2DArray from a UTextureRenderTarget2DArray
	 * @param InOuter - Outer to use when constructing the new UTexture2DArray.
	 * @param InNewTextureName - Name of new UTexture2DArray object.
	 * @param InObjectFlags - Flags to apply to the new UTexture2DArray object
	 * @param InFlags - Various control flags for operation (see EConstructTextureFlags)
	 * @param InAlphaOverride - If specified, the values here will become the alpha values in the resulting texture
	 * @return New UTexture2DArray object.
	 */
	ENGINE_API class UTexture2DArray* ConstructTexture2DArray(UObject* InOuter, const FString& InNewTextureName, EObjectFlags InObjectFlags, uint32 InFlags = CTF_Default, TArray<uint8>* InAlphaOverride = nullptr);

	//~ Begin UTexture Interface.
	virtual float GetSurfaceWidth() const  override { return static_cast<float>(SizeX); }
	// PVS-Studio notices that the implementation of GetSurfaceWidth is identical to this one
	// and warns us. In this case, it is intentional, so we disable the warning:
	virtual float GetSurfaceHeight()const  override { return static_cast<float>(SizeX); }	 //-V524
	virtual float GetSurfaceDepth() const override { return 0.0f; }
	virtual uint32 GetSurfaceArraySize() const override { return Slices; }
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override;
	//~ End UTexture Interface.

	FORCEINLINE int32 GetNumMips() const
	{
		return 1;
	}
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostLoad() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual FString GetDesc() override;
	//~ End UObject Interface

	//~ Begin UTextureRenderTarget Interface
	virtual bool CanConvertToTexture(ETextureSourceFormat& OutTextureSourceFormat, EPixelFormat& OutPixelFormat, FText* OutErrorMessage) const override;
	virtual TSubclassOf<UTexture> GetTextureUClass() const override;
	virtual EPixelFormat GetFormat() const override;
	virtual bool IsSRGB() const override;
	virtual float GetDisplayGamma() const override;
	virtual ETextureClass GetRenderTargetTextureClass() const override { return ETextureClass::Array; }
	//~ End UTextureRenderTarget Interface
};



