// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonLoadGuard.h"
#include "Components/Image.h"

#include "CommonLazyImage.generated.h"

class UCommonMcpItemDefinition;

/**
 * A special Image widget that can show unloaded images and takes care of the loading for you!
 * 
 * UCommonLazyImage is another wrapper for SLoadGuard, but it only handles image loading and 
 * a throbber during loading.
 * 
 * If this class changes to show any text, by default it will have CoreStyle styling
 */
UCLASS()
class COMMONUI_API UCommonLazyImage : public UImage
{
	GENERATED_UCLASS_BODY()

public:
	virtual void SetBrush(const FSlateBrush& InBrush) override;
	virtual void SetBrushFromAsset(USlateBrushAsset* Asset) override;
	virtual void SetBrushFromTexture(UTexture2D* Texture, bool bMatchSize = false) override;
	virtual void SetBrushFromTextureDynamic(UTexture2DDynamic* Texture, bool bMatchSize = false) override;
	virtual void SetBrushFromMaterial(UMaterialInterface* Material) override;

	/** Set the brush from a lazy texture asset pointer - will load the texture as needed. */
	UFUNCTION(BlueprintCallable, Category = LazyImage)
	void SetBrushFromLazyTexture(const TSoftObjectPtr<UTexture2D>& LazyTexture, bool bMatchSize = false);

	/** Set the brush from a lazy material asset pointer - will load the material as needed. */
	UFUNCTION(BlueprintCallable, Category = LazyImage)
	void SetBrushFromLazyMaterial(const TSoftObjectPtr<UMaterialInterface>& LazyMaterial);
	
	/** Set the brush from a string asset ref only - expects the referenced asset to be a texture or material. */
	UFUNCTION(BlueprintCallable, Category = LazyImage)
	void SetBrushFromLazyDisplayAsset(const TSoftObjectPtr<UObject>& LazyObject, bool bMatchTextureSize = false);

	UFUNCTION(BlueprintCallable, Category = LazyImage)
	bool IsLoading() const;

	/**
	 * Establishes the name of the texture parameter on the currently applied brush material to which textures should be applied.
	 * Does nothing if the current brush resource object is not a material.
	 *
	 * Note: that this is cleared out automatically if/when a new material is established on the brush.
	 * You must call this function again after doing so if the new material has a texture param.
	 */
	UFUNCTION(BlueprintCallable, Category = LazyImage)
	void SetMaterialTextureParamName(FName TextureParamName);

	FOnLoadGuardStateChangedEvent& OnLoadingStateChanged() { return OnLoadingStateChangedEvent; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override final;
	virtual void OnWidgetRebuilt() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;

	virtual void CancelImageStreaming() override;
	virtual void OnImageStreamingStarted(TSoftObjectPtr<UObject> SoftObject) override;
	virtual void OnImageStreamingComplete(TSoftObjectPtr<UObject> LoadedSoftObject) override;

	virtual TSharedRef<SWidget> RebuildImageWidget();

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif	

	void SetIsLoading(bool bIsLoading);

private:
	void HandleLoadGuardStateChanged(bool bIsLoading);
	void ShowDefaultImage();

	void SetBrushObjectInternal(UMaterialInterface* Material);
	void SetBrushObjectInternal(UTexture* Texture, bool bMatchSize = false);

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = LoadPreview)
	bool bShowLoading = false;
#endif

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush LoadingBackgroundBrush;

	/** 
	 * If this image uses a material that a texture should be applied to, this is the name of the material param to use.
	 * I.e. if this property is not blank, the resource object of our brush is a material, and we are given a lazy texture, that texture
	 * will be assigned to the texture param on the material instead of replacing the material outright on the brush.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Appearance)
	FName MaterialTextureParamName;

	UPROPERTY(BlueprintAssignable, Category = LazyImage, meta = (DisplayName = "On Loading State Changed", ScriptName = "OnLoadingStateChanged"))
	FOnLoadGuardStateChangedDynamic BP_OnLoadingStateChanged;

	TSharedPtr<SLoadGuard> MyLoadGuard;
	FOnLoadGuardStateChangedEvent OnLoadingStateChangedEvent;
};