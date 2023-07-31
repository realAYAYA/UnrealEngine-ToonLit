// Copyright Epic Games, Inc. All Rights Reserved.

#include "TexturesCache.h"
#include "Utils/LibPartInfo.h"

#include "ModelMaterial.hpp"
#include "Texture.hpp"
#include "AttributeIndex.hpp"
#include "GXImage.hpp"
#include "Graphics2D.h"
#include "Folder.hpp"
#include "FileSystem.hpp"

#include "Paths.h"
#include "DatasmithUtils.h"

BEGIN_NAMESPACE_UE_AC

FTexturesCache::FTexturesCache(const GS::UniString& InAssetsCache)
	: AssetsCache(InAssetsCache)
{
	AbsolutePath = AssetsCache + UE_AC_DirSep + GS::UniString("Textures") + UE_AC_DirSep;
	IESTexturesPath = AssetsCache + UE_AC_DirSep + GS::UniString("IESTextures") + UE_AC_DirSep;
}

const FTexturesCache::FTexturesCacheElem& FTexturesCache::GetTexture(const FSyncContext& InSyncContext,
																	 GS::Int32			 InTextureIndex)
{
	FTexturesCacheElem* ExistingTexture = Textures.Find(InTextureIndex);
	if (ExistingTexture != nullptr)
	{
		return *ExistingTexture;
	}

	UE_AC_Assert(InTextureIndex > 0 && InTextureIndex <= InSyncContext.GetModel().GetTextureCount());

	// Create an new texture element
	FTexturesCacheElem& Texture = Textures.FindOrAdd(InTextureIndex);

	ModelerAPI::Texture		   AcTexture;
	ModelerAPI::AttributeIndex IndexTextureIndex(ModelerAPI::AttributeIndex::TextureIndex, InTextureIndex);
	InSyncContext.GetModel().GetTexture(IndexTextureIndex, &AcTexture);

	if (AcTexture.GetXSize() > 0)
	{
		Texture.InvXSize = 1 / AcTexture.GetXSize();
	}
	if (AcTexture.GetYSize() > 0)
	{
		Texture.InvYSize = 1 / AcTexture.GetYSize();
	}
	Texture.bHasAlpha = AcTexture.HasAlphaChannel();
	Texture.bMirrorX = AcTexture.IsMirroredInX();
	Texture.bMirrorY = AcTexture.IsMirroredInY();
	Texture.bAlphaIsTransparence = AcTexture.IsTransparentPattern();
	Texture.bIsAvailable = AcTexture.IsAvailable();

	Texture.bUsed = false;

	if (Texture.bIsAvailable)
	{
		if (InSyncContext.bUseFingerPrint)
		{
#if 0
            // Compute pixel fingerprint to be redesing
			Texture.TextureLabel = AcTexture.GetPixelMapCheckSum();
			Texture.Fingerprint = GSGuid2APIGuid(GS::Guid(Texture.TextureLabel));
#endif
			// Compute fingerprint with content and used texture informations
			char Tmp[256];
			AcTexture.GetPixelMapCheckSum(Tmp, sizeof(Tmp));
			size_t FingerprintLen = strnlen(Tmp, sizeof(Tmp));
			UE_AC_Assert(FingerprintLen == 32);
			MD5::Generator MD5Generator;
			MD5Generator.Update(Tmp, (unsigned short)FingerprintLen);
			MD5Generator.Update(&Texture.InvXSize, sizeof(Texture.InvXSize));
			MD5Generator.Update(&Texture.InvYSize, sizeof(Texture.InvYSize));
			MD5Generator.Update(&Texture.bAlphaIsTransparence, Texture.bAlphaIsTransparence);
			MD5::FingerPrint FingerPrint;
			MD5Generator.Finish(FingerPrint);
			UE_AC_Assert(FingerPrint.GetAsString(Tmp) == NoError);
			Texture.TextureLabel = Tmp;
			Texture.Fingerprint = Fingerprint2API_Guid(FingerPrint);

			UE_AC_VerboseF("Texture name=\"%s\": TMFingerPrint=\"%s\"\n", AcTexture.GetName().ToUtf8(), Tmp);
		}
		else
		{
			// Create a unique name
			Texture.TextureLabel = AcTexture.GetName();
			unsigned int SequencialNumber = 0;
			while (TexturesNameSet.Contains(&Texture.TextureLabel))
			{
				Texture.TextureLabel = AcTexture.GetName() + GS::UniString::Printf(" %d", ++SequencialNumber);
			}
			TexturesNameSet.Add(&Texture.TextureLabel);

			GS::UniString fp(AcTexture.GetFingerprint());
			Texture.Fingerprint = GSGuid2APIGuid(GS::Guid(fp));
			UE_AC_VerboseF("Texture name=\"%s\": ACFingerprint=\"%s\"\n", Texture.TextureLabel.ToUtf8(), fp.ToUtf8());
		}

		CreateCacheFolders();
		Texture.TexturePath = AbsolutePath + Texture.TextureLabel + GetGSName(kName_TextureExtension);
		WriteTexture(AcTexture, Texture.TexturePath, InSyncContext.bUseFingerPrint);
	}
	else
	{
		GS::UniString fp(AcTexture.GetFingerprint());
		Texture.Fingerprint = GSGuid2APIGuid(GS::Guid(fp));
		UE_AC_ReportF("FTexturesCache::GetTexture - Texture name \"%s\" missing: ACFingerprint=%s\n",
					  AcTexture.GetName().ToUtf8(), fp.ToUtf8());
	}

	GS::UniString Fingerprint = APIGuidToString(Texture.Fingerprint);
	FString		  TextureId = GSStringToUE(Fingerprint);
	bool		  bTextureIdAlreadyInSet = false;
	TexturesIdsSet.Add(TextureId, &bTextureIdAlreadyInSet);
	if (!bTextureIdAlreadyInSet)
	{
		TSharedRef< IDatasmithTextureElement > BaseTexture =
			FDatasmithSceneFactory::CreateTexture(GSStringToUE(Fingerprint));
		BaseTexture->SetLabel(GSStringToUE(AcTexture.GetName()));
		BaseTexture->SetFile(GSStringToUE(Texture.TexturePath));
		if (*BaseTexture->GetFile() != 0)
		{
			FMD5Hash FileHash = FMD5Hash::HashFile(BaseTexture->GetFile());
			BaseTexture->SetFileHash(FileHash);
		}
		else
		{
			BaseTexture->SetFile(TEXT("Missing_Texture_File"));
		}
		BaseTexture->SetSRGB(EDatasmithColorSpace::sRGB);
		InSyncContext.GetScene().AddTexture(BaseTexture);
	}

	return Texture;
}

/* Tool class to search file in Folder and Sub folder of a Parent folder.
 * If found, copy the file to destination folder */
class FSearchAndCopyFile
{
  public:
	// Constructor
	FSearchAndCopyFile(IO::Folder& InDestination, const IO::Name& InFileName)
		: Destination(InDestination)
		, FileName(InFileName)
		, bFileInDestination(false)
	{
		// Check if the file is already in the destination folder
		GSErrCode GSErr = InDestination.Contains(InFileName, &bFileInDestination);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FSearchAndCopyFile::FSearchAndCopyFile - IESTexturesFolder.Contains return error %s\n",
						 GetErrorName(GSErr));
		}
	}

	// Search in the folder tree starting at parent folder.
	bool DoSearchIn(const IO::Folder& Parent) { return Parent.Enumerate(EnumCallBack, this); }

	// Callback function called for each element of the parent specified
	bool EnumCallBack(const IO::Folder& Parent, const IO::Name& EntryName, bool bIsFolder);

	// Return true if the file is in the destination folder
	bool IsFileInDestination() const { return bFileInDestination; }

  private:
	// Callback to folder enumerate function
	static bool CCALL EnumCallBack(const IO::Folder& Parent, const IO::Name& EntryName, bool bIsFolder, void* UserData);

	// Destination where we must copy the file
	IO::Folder& Destination;

	// File's name to be copied
	const IO::Name& FileName;

	// True if the file is in the destination folder
	bool bFileInDestination;
};

// Callback to folder enumerate function
bool CCALL FSearchAndCopyFile::EnumCallBack(const IO::Folder& Parent, const IO::Name& EntryName, bool bIsFolder,
											void* UserData)
{
	return reinterpret_cast< FSearchAndCopyFile* >(UserData)->EnumCallBack(Parent, EntryName, bIsFolder);
}

// Callback function called for each element of the parent specified
bool FSearchAndCopyFile::EnumCallBack(const IO::Folder& Parent, const IO::Name& EntryName, bool bIsFolder)
{
	if (bIsFolder)
	{
		// Search in sub folder
		DoSearchIn(IO::Folder(Parent, EntryName));
	}
	else
	{
		if (EntryName == FileName)
		{
			// Try to copy the file
			GSErrCode GSErr = Parent.Copy(EntryName, Destination, FileName);
			if (GSErr == NoError)
			{
				bFileInDestination = true;
			}
			else
			{
				UE_AC_DebugF("FSearchAndCopyFile::EnumCallBack - IO::Folder::Copy returned error %s\n",
							 GetErrorName(GSErr));
			}
		}
	}

	return !bFileInDestination; // Stop if copy is done
}

// Insure we have a copy of the IES file in the cache folder
GS::UniString FTexturesCache::CopyIESFile(const GS::UniString& InIESFileName)
{
	IO::Location IESTexturesLocation(IESTexturesPath);
	IO::Name	 IESFileName(InIESFileName);

	// Create the cached texture path
	IO::Location  IESTextureLocation(IESTexturesLocation, IESFileName);
	GS::UniString IESTexturePath;
	IESTextureLocation.ToPath(&IESTexturePath);

	// Create the texture folder if it's not present
	IO::Folder IESTexturesFolder(IESTexturesLocation, IO::Folder::Create);

#if 1
	// Check if the file is already in the destination folder
	bool	  bFileInDestination = false;
	GSErrCode GSErr = IESTexturesFolder.Contains(IESFileName, &bFileInDestination);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FTexturesCache::CopyIESFile - IESTexturesFolder.Contains return error %s\n", GetErrorName(GSErr));
	}
	if (!bFileInDestination)
	{
		FAuto_API_LibPart LibPart;
		USize			  Lenght = InIESFileName.GetLength() + 1;
		if (Lenght > API_UniLongNameLen)
		{
			Lenght = API_UniLongNameLen;
		}
		memcpy(LibPart.file_UName, InIESFileName.ToUStr().Get(), Lenght * sizeof(GS::uchar_t));
		GSErr = ACAPI_LibPart_Search(&LibPart, false);
		if (GSErr == NoError && LibPart.location != nullptr)
		{
			// Try to copy the file
			GSErr = IO::fileSystem.Copy(*LibPart.location, IESTextureLocation);
			if (GSErr != NoError)
			{
				UE_AC_ReportF("FTexturesCache::CopyIESFile - Cannot copy IES File \"%s\"", InIESFileName.ToUtf8());
				UE_AC_DebugF("FTexturesCache::CopyIESFile - Cannot copy IES File \"%s\", error=%s\n",
							 InIESFileName.ToUtf8(), GetErrorName(GSErr));
			}
		}
		else
		{
			UE_AC_ReportF("FTexturesCache::CopyIESFile - Cannot find IES File \"%s\"\n", InIESFileName.ToUtf8());
			if (GSErr != NoError)
			{
				UE_AC_DebugF("FTexturesCache::CopyIESFile - ACAPI_LibPart_Search error %s for IES File \"%s\"\n",
							 GetErrorName(GSErr), InIESFileName.ToUtf8());
			}
		}
	}
#else
	// Search the IES file and copy it in the cache
	FSearchAndCopyFile SearchAndCopyIESFile(IESTexturesFolder, IESFileName);
	if (!SearchAndCopyIESFile.IsFileInDestination())
	{
		GS::Array< API_LibraryInfo > LibInfoArray;
		GSErrCode					 GSErr = ACAPI_Environment(APIEnv_GetLibrariesID, &LibInfoArray);
		if (GSErr == NoError)
		{
			for (UInt32 IndexLibrary = 0;
				 IndexLibrary < LibInfoArray.GetSize() && !SearchAndCopyIESFile.IsFileInDestination(); IndexLibrary++)
			{
				const API_LibraryInfo& LibInfo = LibInfoArray[IndexLibrary];

				if (LibInfo.libraryType == API_LocalLibrary || LibInfo.libraryType == API_EmbeddedLibrary ||
					LibInfo.libraryType == API_ServerLibrary)
				{
					IO::Folder LibraryFolder(LibInfo.location, IO::Folder::Ignore);
					SearchAndCopyIESFile.DoSearchIn(LibraryFolder);
				}
			}
		}
	}

	if (!SearchAndCopyIESFile.IsFileInDestination())
	{
		UE_AC_ReportF("FTexturesCache::CopyIESFile - Cannot find IES File \"%s\"\n", InIESFileName.ToUtf8());
	}
#endif

	return IESTexturePath;
}

// Create IES texture
const FTexturesCache::FIESTexturesCacheElem& FTexturesCache::GetIESTexture(const FSyncContext& InSyncContext,
																		   const FString&	   InIESFileName)
{
	CreateCacheFolders();
	FTexturesCache::FIESTexturesCacheElem* found = IESTextures.Find(InIESFileName);
	if (found == nullptr)
	{
		// Find and copy the texture in the cache.
		FString IESFilePath(GSStringToUE(CopyIESFile(UEToGSString(*InIESFileName))));

		// Create IES texture
		const FString BaseFilename = FPaths::GetBaseFilename(IESFilePath);
		FString		  TextureName = FDatasmithUtils::SanitizeObjectName(BaseFilename + TEXT("_IES"));

		TSharedPtr< IDatasmithTextureElement > Texture = FDatasmithSceneFactory::CreateTexture(*TextureName);
		Texture->SetTextureMode(EDatasmithTextureMode::Ies);
		Texture->SetLabel(*BaseFilename);
		Texture->SetFile(*IESFilePath);

		InSyncContext.GetScene().AddTexture(Texture);

		found = &IESTextures.Add(InIESFileName, TextureName);
	}

	return *found;
}

void FTexturesCache::CreateCacheFolders()
{
	if (bCacheFoldersCreated == false)
	{
		{
			// Create the asset folder if it's not present
			IO::Location AssetsFolderLocation(AssetsCache);
			IO::Folder	 AssetsFolder(AssetsFolderLocation, IO::Folder::Create);
			if (AssetsFolder.GetStatus() != NoError)
			{
				UE_AC_ReportF("Unable to create/access assets cache folder: \"%s\"", AssetsCache.ToUtf8());
				UE_AC::ThrowGSError(AssetsFolder.GetStatus(), __FILE__, __LINE__);
			}
		}

		{
			// Create the textures folder if it's not present
			IO::Location TexturesLocation(AbsolutePath);
			IO::Folder	 TexturesFolder(TexturesLocation, IO::Folder::Create);
			if (TexturesFolder.GetStatus() != NoError)
			{
				UE_AC_ReportF("Unable to create/access cache textures cache folder: \"%s\"", AbsolutePath.ToUtf8());
				UE_AC::ThrowGSError(TexturesFolder.GetStatus(), __FILE__, __LINE__);
			}
		}

		bCacheFoldersCreated = true;
	}
}

// Write the texture to the cache
void FTexturesCache::WriteTexture(const ModelerAPI::Texture& InACTexture, const GS::UniString& InPath,
								  bool InIsFingerprint) const
{
	IO::Location TextureLoc(InPath);

	// If texture already exist, we do nothing
	if (InIsFingerprint)
	{
		IO::File TextureFile(TextureLoc);
		if ((TextureFile.GetStatus() == NoError) &&
			(TextureFile.IsOpen() || (TextureFile.Open(IO::File::ReadMode) == NoError)))
		{
			return;
		}
	}

	// Create a pixmap of the same size of the texture
	GSPixMapHandle PixMap = GXCreateGSPixMap(InACTexture.GetPixelMapXSize(), InACTexture.GetPixelMapYSize());
	UE_AC_TestPtr(PixMap);
	try
	{
		// Test the invariant
		UE_AC_Assert(InACTexture.GetPixelMapSize() * sizeof(ModelerAPI::Texture::Pixel) ==
					 GXGetGSPixMapBytesPerRow(PixMap) * InACTexture.GetPixelMapYSize());

		// Copy the pixels from the texture to the PixMap.
		GSPtr Pixels = GXGetGSPixMapBaseAddr(PixMap);
		UE_AC_TestPtr(Pixels);
		InACTexture.GetPixelMap(reinterpret_cast< ModelerAPI::Texture::Pixel* >(Pixels));

		GX::ImageSaveOptions  ImgSaveOpt(GX::PixelBits_MillionsWithAlpha);
		GX::ImageSaveOptions* PixelBitSize = &ImgSaveOpt;
		GX::Image			  Img(PixMap);
		UE_AC_TestGSError(Img.WriteToFile(
			TextureLoc, FTM::FileTypeManager::SearchForMime(GetStdName(kName_TextureMime), NULL), PixelBitSize));
	}
	catch (...)
	{
		GXDeleteGSPixMap(PixMap);
		throw;
	}

	// Delete the pixmap
	GXDeleteGSPixMap(PixMap);
}

END_NAMESPACE_UE_AC
