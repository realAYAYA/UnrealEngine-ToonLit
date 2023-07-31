// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Node acts as a base class for TextureSamples and TextureObjects 
 * to cover their shared functionality. 
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionTextureBase.generated.h"

class UTexture;
struct FPropertyChangedEvent;

UCLASS(abstract, hidecategories=Object)
class ENGINE_API UMaterialExpressionTextureBase : public UMaterialExpression 
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionTextureBase)
	TObjectPtr<class UTexture> Texture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionTextureBase, meta = (ShowAsInputPin = "Advanced"))
	TEnumAsByte<enum EMaterialSamplerType> SamplerType;
	
	/** Is default selected texture when using mesh paint mode texture painting */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureBase)
	uint8 IsDefaultMeshpaintTexture:1;
	
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual FString GetDescription() const override;

	virtual FText GetPreviewOverlayText() const override;
#endif
	//~ End UMaterialExpression Interface

	/** 
	 * Callback to get any texture reference this expression emits.
	 * This is used to link the compiled uniform expressions with their default texture values. 
	 * Any UMaterialExpression whose compilation creates a texture uniform expression (eg Compiler->Texture, Compiler->TextureParameter) must implement this.
	 */
	virtual UObject* GetReferencedTexture() const override;
	virtual bool CanReferenceTexture() const override { return true; }

#if WITH_EDITOR
	/**
	 * Automatically determines and set the sampler type for the current texture.
	 */
	void AutoSetSampleType();

	/**
	 * Returns the default sampler type for the specified texture.
	 * @param Texture - The texture for which the default sampler type will be returned.
	 * @returns the default sampler type for the specified texture.
	 */
	static EMaterialSamplerType GetSamplerTypeForTexture( const UTexture* Texture, bool ForceNoVT = false );

	/**
	 * Verify that the texture and sampler type. Generates a compiler waring if
	 * they do not.
	 * @param Texture - The texture to verify. A nullptr texture is considered valid!
	 * @param SamplerType - The sampler type to verify.
	 * @param OutErrorMessage - If 'false' is returned, will contain a message describing the error
	 */
	static bool VerifySamplerType(
		ERHIFeatureLevel::Type FeatureLevel,
		const ITargetPlatform* TargetPlatform,
		const UTexture* Texture,
		EMaterialSamplerType SamplerType,
		FString& OutErrorMessage);
#endif // WITH_EDITOR
};
