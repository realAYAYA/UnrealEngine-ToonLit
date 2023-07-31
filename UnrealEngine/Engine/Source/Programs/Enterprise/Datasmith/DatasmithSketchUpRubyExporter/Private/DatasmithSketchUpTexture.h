// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithSketchUpCommon.h"

#include "Misc/SecureHash.h"
#include "Containers/UnrealString.h"

class IDatasmithTextureElement;

namespace DatasmithSketchUp
{
	class FExportContext;
	class FTexture;
	class FTextureImageFile;
	class FMaterial;

	class FTextureImageFile
	{
	public:
		FTextureImageFile() : bInvalidated(true) {}
		~FTextureImageFile();

		FMD5Hash ImageHash;

		FString TextureName;
		FString TextureFileName;

		FTexture* Texture;

		TSharedPtr<IDatasmithTextureElement> TextureElement; // Texture element is created once per texture image file

		static TSharedPtr<FTextureImageFile> Create(FTexture& Texture, const FMD5Hash& ImageHash);

		void Update(FExportContext& Context);

		void Invalidate()
		{
			bInvalidated = true;
		}

		uint8 bInvalidated:1;

		TSet<FTexture*> Textures; // Textures sharing this(same) image
	};

	// Represents texture instantiated for Datasmith
	// Each SketchUp texture can have at least two instances in Datasmith - for regular and 'colorized' materials(SketchUp applies color to texture itself)
	class FTexture
	{
	public:
		FTexture(SUTextureRef InTextureRef, FTextureIDType InTextureId) : TextureRef(InTextureRef), TextureId(InTextureId), bInvalidated(true) {}

		bool GetTextureUseAlphaChannel();

		void WriteImageFile(FExportContext& Context, const FString& TextureFilePath);

		const TCHAR* GetDatasmithElementName();

		void Invalidate();
		void Update(FExportContext& Context);


		// Sketchup reference
		SUTextureRef TextureRef;
		FTextureIDType TextureId;

		FString MaterialName; // Material that this texture is bound to, name stored in case we need to make unique name for texture element
		TSharedPtr<FTextureImageFile> TextureImageFile;

		bool bColorized;

		// Extracted from Sketchup
		FVector2D TextureScale;

		uint8 bInvalidated : 1;
	};
}



