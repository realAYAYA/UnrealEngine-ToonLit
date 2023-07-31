// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Text3DPrivate.h"
#include "BevelType.h"
#include "ContourNode.h"
#include "Containers/Ticker.h"

#include "Text3DEngineSubsystem.generated.h"

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
	return HashParameters;
}

USTRUCT()
struct FCachedFontMeshes
{
	GENERATED_BODY()

public:
	FCachedFontMeshes();

	int32 GetCacheCount();
	TSharedPtr<int32> GetCacheCounter();

	UPROPERTY()
	TMap<uint32, TObjectPtr<class UStaticMesh>> Glyphs;

private:
	TSharedPtr<int32> CacheCounter;
};

USTRUCT()
struct FCachedFontData
{
	GENERATED_BODY()

public:
	FCachedFontData();
	~FCachedFontData();

	FT_Face GetFreeTypeFace();
	const FString& GetFontName();

	void LoadFreeTypeFace();
	void ClearFreeTypeFace();

	bool Cleanup();

	uint32 GetTypefaceFontDataHash();
	TSharedPtr<int32> GetCacheCounter();
	TSharedPtr<int32> GetMeshesCacheCounter(const FGlyphMeshParameters& Parameters);

	UStaticMesh* GetGlyphMesh(uint32 GlyphIndex, const FGlyphMeshParameters& Parameters);
	TSharedContourNode GetGlyphContours(uint32 GlyphIndex);

	UPROPERTY()
	TObjectPtr<class UFont> Font;

	UPROPERTY()
	TMap<uint32, FCachedFontMeshes> Meshes;

	TMap<uint32, TSharedContourNode> Glyphs;

private:
	FT_Face FreeTypeFace;
	FString FontName;
	TArray<uint8> Data;
	TSharedPtr<int32> CacheCounter;
	uint32 TypefaceFontDataHash;
};

UCLASS()
class TEXT3D_API UText3DEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:	
	UText3DEngineSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void Reset();
	void Cleanup();
	FCachedFontData& GetCachedFontData(class UFont* Font);

	UPROPERTY()
	TObjectPtr<class UMaterial> DefaultMaterial;

private:
	bool CleanupTimerCallback(float DeltaTime);

	UPROPERTY()
	TMap<uint32, FCachedFontData> CachedFonts;

	FTSTicker::FDelegateHandle CleanupTickerHandle;
};
