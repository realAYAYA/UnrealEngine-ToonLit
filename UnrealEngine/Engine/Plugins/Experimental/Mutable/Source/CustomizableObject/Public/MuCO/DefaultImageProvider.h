// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectSystem.h"

#include "DefaultImageProvider.generated.h"

class UTexture2D;
class UCustomizableObjectInstance;

namespace mu
{
	class Parameters;
}

/** Simple image provider that translates UTexture2D to Mutable IDs.
 *
 * Allows the reuse of UTexture2D. */
UCLASS()
class CUSTOMIZABLEOBJECT_API UDefaultImageProvider : public UCustomizableSystemImageProvider
{
	GENERATED_BODY()

public:
	// UCustomizableSystemImageProvider interface
	virtual ValueType HasTextureParameterValue(const FName& ID) override;
	virtual UTexture2D* GetTextureParameterValue(const FName& ID) override;
	virtual void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) override;
	
	/** Add a Texture to the provider. */
	FString Add(UTexture2D* Texture);

	/** Remove a Texture from the provider. */
	void Remove(UTexture2D* Texture); 

private:
	// Always contains valid pointers
	UPROPERTY()
	TMap<FName, TObjectPtr<UTexture2D>> Textures;
};

