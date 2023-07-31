// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"

#include "VariantSet.generated.h"

class UVariant;

UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UVariantSet : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnVariantSetChanged, UVariantSet*);
	static FOnVariantSetChanged OnThumbnailUpdated;

	UFUNCTION(BlueprintPure, Category="VariantSet")
	class ULevelVariantSets* GetParent();

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	// Sets whether this variant set is expanded or not when displayed
	// in a variant manager
	bool IsExpanded() const;
	void SetExpanded(bool bInExpanded);

	UFUNCTION(BlueprintCallable, Category="VariantSet")
	void SetDisplayText(const FText& NewDisplayText);

	UFUNCTION(BlueprintPure, Category="VariantSet")
	FText GetDisplayText() const;

	FString GetUniqueVariantName(const FString& InPrefix) const;

	void AddVariants(const TArray<UVariant*>& NewVariants, int32 Index = INDEX_NONE);
	int32 GetVariantIndex(UVariant* Var) const;
	const TArray<UVariant*>& GetVariants() const;
	void RemoveVariants(const TArray<UVariant*>& InVariants);

	UFUNCTION(BlueprintPure, Category="VariantSet")
	int32 GetNumVariants() const;

	UFUNCTION(BlueprintPure, Category="VariantSet")
	UVariant* GetVariant(int32 VariantIndex);

	UFUNCTION(BlueprintPure, Category="VariantSet")
	UVariant* GetVariantByName(FString VariantName);

	// Sets the thumbnail to use for this variant set. Can receive nullptr to clear it
	UFUNCTION(BlueprintCallable, Category = "VariantSet|Thumbnail")
	void SetThumbnailFromTexture(UTexture2D* NewThumbnail);

	UFUNCTION(BlueprintCallable, Category = "VariantSet|Thumbnail")
	void SetThumbnailFromFile(FString FilePath);

	UFUNCTION(BlueprintCallable, Category = "VariantSet|Thumbnail", meta = (WorldContext = "WorldContextObject"))
	void SetThumbnailFromCamera(UObject* WorldContextObject, const FTransform& CameraTransform, float FOVDegrees = 50.0f, float MinZ = 50.0f, float Gamma = 2.2f);

	// Sets the thumbnail from the active editor viewport. Doesn't do anything if the Editor is not available
	UFUNCTION(BlueprintCallable, Category = "VariantSet|Thumbnail", meta = (CallInEditor = "true"))
	void SetThumbnailFromEditorViewport();

	// Gets the thumbnail currently used for this variant set
	UFUNCTION(BlueprintCallable, Category = "VariantSet|Thumbnail")
	UTexture2D* GetThumbnail();

private:
	void SetThumbnailInternal(UTexture2D* NewThumbnail);

private:

	// The display name used to be a property. Use the non-deprecated, non-property version from now on
	UPROPERTY()
	FText DisplayText_DEPRECATED;

	FText DisplayText;

	UPROPERTY()
	bool bExpanded;

	UPROPERTY()
	TArray<TObjectPtr<UVariant>> Variants;

	UPROPERTY()
	TObjectPtr<UTexture2D> Thumbnail;
};
