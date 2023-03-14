// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpression.h"
#include "UObject/ObjectMacros.h"
#include "VT/RuntimeVirtualTexture.h"
#include "MaterialExpressionRuntimeVirtualTextureSample.generated.h"

/**
 * Set how Mip levels are calculated.
 * Internally we will convert to ETextureMipValueMode which is used by internal APIs.
 */
UENUM()
enum ERuntimeVirtualTextureMipValueMode
{
	/* 
	 * Use default computed mip level. Takes into account UV scaling from using the WorldPosition pin.
	 */
	RVTMVM_None UMETA(DisplayName = "Default"),

	/* 
	 * Use an absolute mip level from the MipValue pin. 
	 * 0 is full resolution.
	 */
	RVTMVM_MipLevel UMETA(DisplayName = "Mip Level"),

	/* 
	 * Bias the default computed mip level using the MipValue pin. 
	 * Negative values increase resolution.
	 */
	RVTMVM_MipBias UMETA(DisplayName = "Mip Bias"),

	/* 
	 * This is like 'Default' but it ignores the WorldPosition pin when computing the mip level.
	 * It uses the actual pixel WorldPosition instead.
	 * This can prevent sampling mip 0 if the WorldPosition pin gives a constant value.
	 */
	RVTMVM_RecalculateDerivatives UMETA(DisplayName = "Ignore Input WorldPosition "),

	RVTMVM_MAX,
};

/**
 * Defines texture addressing behavior.
 */
UENUM()
enum ERuntimeVirtualTextureTextureAddressMode
{
	/* Clamp mode. */
	RVTTA_Clamp UMETA(DisplayName = "Clamp"),
	/* Wrap mode. */
	RVTTA_Wrap UMETA(DisplayName = "Wrap"),

	RVTTA_MAX,
};

/** Material expression for sampling from a runtime virtual texture. */
UCLASS(collapsecategories, hidecategories=Object)
class ENGINE_API UMaterialExpressionRuntimeVirtualTextureSample : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Optional UV coordinates input if we want to override standard world position based coordinates. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput Coordinates;

	/** Optional world position input to override the default world position. */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput WorldPosition;

	/** Meaning depends on MipValueMode. A single unit is one mip level.  */
	UPROPERTY(meta = (RequiredInput = "false"))
	FExpressionInput MipValue;

	/** The virtual texture object to sample. */
	UPROPERTY(EditAnywhere, Category = VirtualTexture)
	TObjectPtr<class URuntimeVirtualTexture> VirtualTexture;

	/** How to interpret the virtual texture contents. Note that the bound Virtual Texture should have the same setting for sampling to work correctly. */
	UPROPERTY(EditAnywhere, Category = VirtualTexture, meta = (DisplayName = "Virtual texture content"))
	ERuntimeVirtualTextureMaterialType MaterialType = ERuntimeVirtualTextureMaterialType::BaseColor;

	/** Enable page table channel packing. Note that the bound Virtual Texture should have the same setting for sampling to work correctly. */
	UPROPERTY(EditAnywhere, Category = VirtualTexture, meta = (DisplayName = "Enable packed page table"))
	bool bSinglePhysicalSpace = true;

	/** Enable sparse adaptive page tables. Note that the bound Virtual Texture should have valid adaptive virtual texture settings for sampling to work correctly. */
	UPROPERTY(EditAnywhere, Category = VirtualTexture, meta = (DisplayName = "Enable adaptive page table"))
	bool bAdaptive = false;

	/** Defines how the mip level is calculated for the virtual texture lookup. */
	UPROPERTY(EditAnywhere, Category = TextureSample)
	TEnumAsByte<enum ERuntimeVirtualTextureMipValueMode> MipValueMode = RVTMVM_None;

	/** Defines the texture addressing mode. */
	UPROPERTY(EditAnywhere, Category = TextureSample)
	TEnumAsByte<enum ERuntimeVirtualTextureTextureAddressMode> TextureAddressMode = RVTTA_Clamp;

	/** Init settings that affect shader compilation and need to match the current VirtualTexture */
	bool InitVirtualTextureDependentSettings();

protected:
	/** Initialize the output pins. */
	void InitOutputs();

	//~ Begin UMaterialExpression Interface
	virtual UObject* GetReferencedTexture() const override;
	virtual bool CanReferenceTexture() const { return true; }

#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UMaterialExpression Interface
};
