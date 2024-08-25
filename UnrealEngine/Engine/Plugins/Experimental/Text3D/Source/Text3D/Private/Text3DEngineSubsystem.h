// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BevelType.h"
#include "Containers/Ticker.h"
#include "ContourNode.h"
#include "Subsystems/EngineSubsystem.h"
#include "Text3DPrivate.h"

#include "Text3DEngineSubsystem.generated.h"

class FFreeTypeFace;
class UFont;
class UStaticMesh;

USTRUCT()
struct FGlyphMeshParameters
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float Extrude = 5.0f;

	UPROPERTY()
	float Bevel = 0.0f;

	UPROPERTY()
	EText3DBevelType BevelType = EText3DBevelType::Convex;

	UPROPERTY()
	int32 BevelSegments = 8;

	UPROPERTY()
	bool bOutline = false;

	UPROPERTY()
	float OutlineExpand = 0.5f;

	UPROPERTY()
	uint32 TypefaceIndex = 0;
};

inline uint32 GetTypeHash(const FGlyphMeshParameters A)
{
	uint32 HashParameters = 0;
	HashParameters = HashCombine(HashParameters, GetTypeHash(A.Extrude));
	HashParameters = HashCombine(HashParameters, GetTypeHash(A.Bevel));
    HashParameters = HashCombine(HashParameters, GetTypeHash(A.BevelType));
	HashParameters = HashCombine(HashParameters, GetTypeHash(A.BevelSegments));
	HashParameters = HashCombine(HashParameters, GetTypeHash(A.bOutline));
	HashParameters = HashCombine(HashParameters, GetTypeHash(A.OutlineExpand));
	HashParameters = HashCombine(HashParameters, GetTypeHash(A.TypefaceIndex));
	return HashParameters;
}

USTRUCT()
struct FCachedFontMeshes
{
	GENERATED_BODY()

public:
	FCachedFontMeshes();

	int32 GetCacheCount() const;
	TSharedPtr<int32> GetCacheCounter();

	UPROPERTY()
	TMap<uint32, TObjectPtr<UStaticMesh>> Glyphs;

private:
	TSharedPtr<int32> CacheCounter;
};

USTRUCT()
struct FTypefaceFontData
{
	GENERATED_BODY()

public:
	FTypefaceFontData();

	void Reset()
	{
		FT_Done_Face(FreeTypeFace);
		FreeTypeFace = nullptr;
		TypefaceData.Reset();
	}

	int32 GetCacheCount() const
	{
		const int32 Count = CacheCounter.GetSharedReferenceCount();
		return Count;
	}

	FT_Face& GetTypeface() { return FreeTypeFace; }

	TArray<uint8>& GetTypefaceData() { return TypefaceData; }

	void SetTypefaceData(const TArray<uint8>& InTypefaceData)
	{
		TypefaceData = InTypefaceData;
	}

	FName GetTypefaceName() const { return TypefaceName; }
	void SetTypefaceName(FName InTypefaceName)
	{
		TypefaceName = InTypefaceName;
	}

	uint32 GetTypefaceFontDataHash() const { return TypefaceFontDataHash; }

	void SetTypefaceFontDataHash(uint32 InDataHash)
	{
		TypefaceFontDataHash = InDataHash;
	}

	TMap<uint32, FCachedFontMeshes>& GetMeshes() { return Meshes; }

	TSharedPtr<int32> GetCacheCounter() { return CacheCounter; }

	FCachedFontMeshes& FindOrAddMeshes(uint32 InHashParameters)
	{
		return Meshes.FindOrAdd(InHashParameters);
	}

private:
	UPROPERTY()
	TMap<uint32, FCachedFontMeshes> Meshes;

	FName TypefaceName;
	FT_Face FreeTypeFace;
	TArray<uint8> TypefaceData;
	TSharedPtr<int32> CacheCounter;
	uint32 TypefaceFontDataHash;
};

USTRUCT()
struct FCachedFontData
{
	GENERATED_BODY()

public:
	FCachedFontData();
	~FCachedFontData();

	FT_Face GetFreeTypeFace(uint32 InTypefaceIndex);

	FString GetFontName() const;

	UFont* GetFont() { return Font; }

	void SetFont(UFont* InFont)
	{
		Font = InFont;
	}

	void LoadFreeTypeFace(uint32 InTypefaceIndex);
	void ClearFreeTypeFace();

	bool Cleanup();
	bool CleanupTypeface(uint32 InTypefaceIndex);

	uint32 GetTypefaceFontDataHash(int32 InTypefaceEntryIndex) const;
	TSharedPtr<int32> GetCacheCounter(int32 InTypefaceEntryIndex);
	TSharedPtr<int32> GetMeshesCacheCounter(const FGlyphMeshParameters& InParameters);

	UStaticMesh* GetGlyphMesh(uint32 InGlyphIndex, const FGlyphMeshParameters& InParameters, const FFreeTypeFace* InFontFaceData = nullptr);
	TSharedContourNode GetGlyphContours(uint32 InGlyphIndex, int32 InTypefaceEntryIndex, const FFreeTypeFace* InFontFaceData = nullptr);

	void PrintCache() const;

private:
	UPROPERTY()
	TObjectPtr<UFont> Font;

	UPROPERTY()
	TMap<uint32, FTypefaceFontData> TypefaceFontDataMap;
};

UCLASS()
class TEXT3D_API UText3DEngineSubsystem : public UEngineSubsystem, public FSelfRegisteringExec
{
	GENERATED_BODY()

public:
	UText3DEngineSubsystem();

	// ~Begin UEngineSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UEngineSubsystem

	// ~Begin FSelfRegisteringExec
	virtual bool Exec(class UWorld* InWorld, const TCHAR* InCmd, FOutputDevice& InAr) override;
	// ~End FSelfRegisteringExec

	void PrintCache() const;

	void Reset();
	void Cleanup();
	FCachedFontData& GetCachedFontData(UFont* InFont, int32 InTypefaceEntryIndex = 0);

	UPROPERTY()
	TObjectPtr<class UMaterial> DefaultMaterial;

private:
	bool CleanupTimerCallback(float DeltaTime);

	UPROPERTY()
	TMap<uint32, FCachedFontData> CachedFonts;

	FTSTicker::FDelegateHandle CleanupTickerHandle;
};
