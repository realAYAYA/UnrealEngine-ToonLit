// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/TextureImportTestFunctions.h"
#include "Engine/Texture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureImportTestFunctions)


UClass* UTextureImportTestFunctions::GetAssociatedAssetType() const
{
	return UTexture::StaticClass();
}

FInterchangeTestFunctionResult UTextureImportTestFunctions::CheckImportedTextureCount(const TArray<UTexture*>& Textures, int32 ExpectedNumberOfImportedTextures)
{
	FInterchangeTestFunctionResult Result;
	if (Textures.Num() != ExpectedNumberOfImportedTextures)
	{
		Result.AddError(FString::Printf(TEXT("Expected %d textures, imported %d."), ExpectedNumberOfImportedTextures, Textures.Num()));
	}

	return Result;
}