// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpTexture.h"

#include "DatasmithSketchUpCommon.h"
#include "DatasmithSketchUpUtils.h"
#include "DatasmithSketchUpExportContext.h"
#include "DatasmithSketchUpString.h"

#include "DatasmithSketchUpMaterial.h"


#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "IDatasmithSceneElements.h"

#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"

#include "SketchUpAPI/model/texture.h"
#include "SketchUpAPI/model/image_rep.h"

#include "DatasmithSketchUpSDKCeases.h"


using namespace DatasmithSketchUp;

TSharedPtr<FTexture> FTextureCollection::FindOrAdd(SUTextureRef TextureRef)
{
	FTextureIDType TextureId = DatasmithSketchUpUtils::GetEntityID(SUTextureToEntity(TextureRef));

	if (TSharedPtr<FTexture>* TexturePtr = TexturesMap.Find(TextureId))
	{
		return *TexturePtr;
	}
	TSharedPtr<FTexture> Texture = MakeShared<FTexture>(TextureRef, TextureId);
	TexturesMap.Add(TextureId, Texture);
	return Texture;
}

FTexture* FTextureCollection::AddTexture(SUTextureRef TextureRef, FString MaterialName, bool bColorized)
{
	TSharedPtr<FTexture> Texture = FindOrAdd(TextureRef);
	Texture->bColorized = bColorized;
	Texture->MaterialName = MaterialName;
	Texture->Invalidate();
	return Texture.Get();
}

FTexture* FTextureCollection::AddColorizedTexture(SUTextureRef TextureRef, FString MaterialName)
{
	return AddTexture(TextureRef, MaterialName, true);
}

void FTextureCollection::Update()
{
	if (!TexturesMap.IsEmpty())
	{
		// Making sure _Assets folder is present
		// SU API need to have folder created before writing image files
		// It's not neccessarily present at this point - it's only created when meshes are exported and they are being exported in a separate thread
		if (!FPaths::FileExists(Context.GetAssetsOutputPath()))
		{
			IFileManager::Get().MakeDirectory(Context.GetAssetsOutputPath());
		}
	}

	for (TPair<FTextureIDType, TSharedPtr<FTexture>>& TextureNameAndTextureImageFile : TexturesMap)
	{
		TSharedPtr<FTexture> Texture = TextureNameAndTextureImageFile.Value;
		Texture->Update(Context);
		if (Texture->TextureImageFile)
		{
			Texture->TextureImageFile->Update(Context);
		}
	}
}

void FTextureCollection::RegisterMaterial(FMaterial* Material)
{
	FTexture* Texture = Material->GetTexture();
	if (!Texture)
	{
		return;
	}
}

void FTextureCollection::UnregisterMaterial(FMaterial* Material)
{
	FTexture* Texture = Material->GetTexture();
	if (!Texture)
	{
		return;
	}

	// Remove texture that is not used by its material
	// todo: remove in descturtor?
	ReleaseImage(*Texture);
	TexturesMap.Remove(Texture->TextureId);
}

bool FTexture::GetTextureUseAlphaChannel()
{
	// Get the flag indicating whether or not the SketchUp texture alpha channel is used.
	bool bUseAlphaChannel = false;
	// Make sure the flag was retrieved properly (no SU_ERROR_NO_DATA).
	return (SUTextureGetUseAlphaChannel(TextureRef, &bUseAlphaChannel) == SU_ERROR_NONE) && bUseAlphaChannel;
}

void FTexture::WriteImageFile(FExportContext& Context, const FString& TextureFilePath)
{
	// Write the SketchUp texture into a file when required.
	SUResult SResult = SUTextureWriteToFile(TextureRef, TCHAR_TO_UTF8(*TextureFilePath));
	if (SResult == SU_ERROR_SERIALIZATION)
	{
		// TODO: Append an error message to the export summary.
	}
}

const TCHAR* FTexture::GetDatasmithElementName()
{
	return TextureImageFile->TextureElement->GetName();
}

TSharedPtr<FTextureImageFile> FTextureImageFile::Create(FTexture& Texture, const FMD5Hash& ImageHash)
{

	FString SourceTextureFileName;
	FString TextureBaseName;

	SourceTextureFileName = SuGetString(SUTextureGetFileName, Texture.TextureRef);

	// Set texture to be material-specific. SketchUp allows to have different material have different texture images under the same name
	TextureBaseName = Texture.MaterialName;

	TSharedPtr<FTextureImageFile> TextureImageFile = MakeShared<FTextureImageFile>();
	TextureImageFile->ImageHash = ImageHash;
	TextureImageFile->Texture = &Texture;

	TextureImageFile->TextureFileName = FDatasmithUtils::SanitizeFileName(TextureBaseName) + FPaths::GetExtension(SourceTextureFileName, /*bIncludeDot*/ true);
	TextureImageFile->TextureName = FDatasmithUtils::SanitizeObjectName(FPaths::GetBaseFilename(TextureImageFile->TextureFileName));
	TextureImageFile->TextureElement = FDatasmithSceneFactory::CreateTexture(*TextureImageFile->TextureName);
	TextureImageFile->TextureElement->SetSRGB(EDatasmithColorSpace::sRGB);

	return TextureImageFile;
}

void FTexture::Invalidate()
{
	bInvalidated = true;
}

void FTexture::Update(FExportContext& Context)
{
	if(!bInvalidated)
	{
		return;
	}

	// Get the pixel scale factors of the source SketchUp texture.
	size_t TextureWidth = 0;
	size_t TextureHeight = 0;
	double TextureSScale = 1.0;
	double TextureTScale = 1.0;
	SUTextureGetDimensions(TextureRef, &TextureWidth, &TextureHeight, &TextureSScale, &TextureTScale); // we can ignore the returned SU_RESULT
	TextureScale = FVector2D(TextureSScale, TextureTScale);

	Context.Textures.AcquireImage(*this);
}

void FTextureImageFile::Update(FExportContext& Context)
{
	if (!bInvalidated)
	{
		return;
	}

	FString TextureFilePath = FPaths::Combine(Context.GetAssetsOutputPath(), TextureFileName);
	Texture->WriteImageFile(Context, TextureFilePath);
	TextureElement->SetFile(*TextureFilePath);
	Context.DatasmithScene->AddTexture(TextureElement); // todo: make sure that texture not created/added twice

	bInvalidated = false;
}

FTextureImageFile::~FTextureImageFile()
{
}

void FTextureCollection::AcquireImage(FTexture& Texture)
{
	// Compute image md5
	SUImageRepRef ImageRep = SU_INVALID;
	SUImageRepCreate(&ImageRep);
	SUTextureGetImageRep(Texture.TextureRef, &ImageRep);

	size_t Width, Height;
	SUImageRepGetPixelDimensions(ImageRep, &Width, &Height);

	size_t DataSize, Bpp;
	SUImageRepGetDataSize(ImageRep, &DataSize, &Bpp);

	TArray<SUByte> Data;
	Data.SetNumUninitialized(DataSize);
	SUImageRepGetData(ImageRep, DataSize, Data.GetData());

	SUImageRepRelease(&ImageRep);

	FMD5 MD5;
	MD5.Update(reinterpret_cast<const uint8*>(&Width), sizeof(Width));
	MD5.Update(reinterpret_cast<const uint8*>(&Height), sizeof(Height));
	MD5.Update(reinterpret_cast<const uint8*>(&Bpp), sizeof(Bpp));
	MD5.Update(reinterpret_cast<const uint8*>(&Texture.bColorized), sizeof(Texture.bColorized)); // Colorized flag affects resulting texture image
	MD5.Update(Data.GetData(), Data.Num());

	FMD5Hash TextureHash;
	TextureHash.Set(MD5);

	TSharedPtr<FTextureImageFile>& Image = Images.FindOrAdd(TextureHash);
	if(!Image)
	{
		Image = FTextureImageFile::Create(Texture, TextureHash);
	}

	Texture.TextureImageFile = Image;
	Image->Textures.Add(&Texture);
}

void FTextureCollection::ReleaseImage(FTexture& Texture)
{
	TSharedPtr<FTextureImageFile> Image = Texture.TextureImageFile;
	Texture.TextureImageFile.Reset();

	// Remove reference from Image to texture 
	Image->Textures.Remove(&Texture);

	// When image not used remove it along with Datasmith texture element
	if (!Image->Textures.Num())
	{
		if (Image->TextureElement)
		{
			Context.DatasmithScene->RemoveTexture(Image->TextureElement); // todo: make sure that texture not created/added twice
		}
		Images.Remove(Image->ImageHash);
	}
}
