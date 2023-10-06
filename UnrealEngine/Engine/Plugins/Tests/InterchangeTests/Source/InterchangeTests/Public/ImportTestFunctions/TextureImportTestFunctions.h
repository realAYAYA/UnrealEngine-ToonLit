// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/TextureDefines.h"
#include "ImportTestFunctionsBase.h"
#include "TextureImportTestFunctions.generated.h"

class UTexture;

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API UTextureImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of textures are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckImportedTextureCount(const TArray<UTexture*>& Textures, int32 ExpectedNumberOfImportedTextures);

	/** Check whether the imported texture has the expected filtering mode */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckTextureFilter(const UTexture* Texture, TextureFilter ExpectedTextureFilter);

	/** Check whether the imported texture has the expected addressing mode for X-axis */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckTextureAddressX(const UTexture* Texture, TextureAddress ExpectedTextureAddressX);

	/** Check whether the imported texture has the expected addressing mode for Y-axis */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckTextureAddressY(const UTexture* Texture, TextureAddress ExpectedTextureAddressY);

	/** Check whether the imported texture has the expected addressing mode for Z-axis */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckTextureAddressZ(const UTexture* Texture, TextureAddress ExpectedTextureAddressZ);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "InterchangeTestFunction.h"
#endif
