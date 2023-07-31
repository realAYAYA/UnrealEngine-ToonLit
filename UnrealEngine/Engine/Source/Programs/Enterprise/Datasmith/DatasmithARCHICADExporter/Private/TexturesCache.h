// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"
#include "SyncContext.h"

#include "Model.hpp"

#include "Map.h"
#include "Set.h"

class FString;

BEGIN_NAMESPACE_UE_AC

class FUniStringPtr
{
  public:
	FUniStringPtr(const GS::UniString* InStr)
		: UniStringPtr(InStr)
	{
		UE_AC_TestPtr(UniStringPtr);
	}

	FUniStringPtr(const FUniStringPtr& InStr)
		: UniStringPtr(InStr.UniStringPtr)
	{
	}

	inline bool operator==(const FUniStringPtr& B) const { return *UniStringPtr == *B.UniStringPtr; }

	const GS::UniString* UniStringPtr;
};

inline uint32 GetTypeHash(const FUniStringPtr& A)
{
	utf8_string Utf8Str(A.UniStringPtr->ToUtf8());
	return FCrc::MemCrc32(Utf8Str.c_str(), int32(Utf8Str.size()));
}

class FTexturesCache
{
  public:
	// Handle textures used.
	class FTexturesCacheElem
	{
	  public:
		FTexturesCacheElem() {}

		GS::UniString TextureLabel; // Texture's name (Maybe not unique)
		GS::UniString TexturePath; // Path of the written texture (Unique)
		API_Guid	  Fingerprint = APINULLGuid; // The texture fingerprint as GUID
		double		  InvXSize = 1.0; // Used to compute uv
		double		  InvYSize = 1.0; // Used to compute uv
		bool bHasAlpha = false; // Texture has alpha channel (transparent, bump, diffuse, specular, ambient, surface)
		bool bMirrorX = false; // Mirror on X
		bool bMirrorY = false; // Mirror on Y
		bool bAlphaIsTransparence = false; // Texture use alpha channel for transparency
		bool bIsAvailable = false; // Texture can be loaded
		bool bUsed = false; // True if this texture is used
	};

	class FIESTexturesCacheElem
	{
	  public:
		FIESTexturesCacheElem(FString& InTexturePath)
			: TexturePath(InTexturePath)
		{
		}

		FString TexturePath; // Path of the written texture
	};

	FTexturesCache(const GS::UniString& InAssetsCache);

	// Return the texture specified by the index. (Will throw an exception if index is invalid)
	const FTexturesCacheElem& GetTexture(const FSyncContext& InSyncContext, GS::Int32 InTextureIndex);

	// Create IES texture
	const FIESTexturesCacheElem& GetIESTexture(const FSyncContext& InSyncContext, const FString& InIESFilePath);

	// Write the texture to the cache
	void WriteTexture(const ModelerAPI::Texture& inACTexture, const GS::UniString& InPath, bool InIsFingerprint) const;

	size_t GetCount() const { return Textures.Num(); }

	// Insure we have a copy of the IES file in the cache folder
	GS::UniString CopyIESFile(const GS::UniString& InIESFileName);

  private:
	void CreateCacheFolders();

	typedef TMap< GS::Int32, FTexturesCacheElem >  MapTextureIndex2CacheElem;
	typedef TMap< FString, FIESTexturesCacheElem > MapIESTexture2CacheElem;

	typedef TSet< FString >	  SetTexturesIds;
	MapTextureIndex2CacheElem Textures;
	MapIESTexture2CacheElem	  IESTextures;
	bool					  bCacheFoldersCreated = false;
	GS::UniString			  AssetsCache;
	GS::UniString			  AbsolutePath;
	GS::UniString			  IESTexturesPath;

	typedef TSet< FUniStringPtr > SetStrings;

	SetStrings	   TexturesNameSet;
	SetTexturesIds TexturesIdsSet;
};

END_NAMESPACE_UE_AC
