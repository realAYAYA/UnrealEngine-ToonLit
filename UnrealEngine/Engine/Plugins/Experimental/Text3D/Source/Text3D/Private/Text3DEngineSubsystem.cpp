// Copyright Epic Games, Inc. All Rights Reserved.

#include "Text3DEngineSubsystem.h"
#include "Containers/Ticker.h"
#include "Misc/FileHelper.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MeshCreator.h"
#include "GlyphLoader.h"
#include "ContourNode.h"
#include "UObject/ConstructorHelpers.h"


FCachedFontMeshes::FCachedFontMeshes()
{
	CacheCounter = MakeShared<int32>(0);
}

int32 FCachedFontMeshes::GetCacheCount()
{
	return CacheCounter.GetSharedReferenceCount();
}

TSharedPtr<int32> FCachedFontMeshes::GetCacheCounter()
{
	return CacheCounter;
}

UText3DEngineSubsystem::UText3DEngineSubsystem()
{
	if (!IsRunningDedicatedServer())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinder<UMaterial> Material;
			FConstructorStatics() :
				Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		DefaultMaterial = ConstructorStatics.Material.Object;
	}
}

void UText3DEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CleanupTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UText3DEngineSubsystem::CleanupTimerCallback), 600.0f);
}

void UText3DEngineSubsystem::Deinitialize()
{
	if (CleanupTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CleanupTickerHandle);
		CleanupTickerHandle.Reset();
	}

	Super::Deinitialize();
}

void UText3DEngineSubsystem::Reset()
{
	CachedFonts.Reset();
}

bool UText3DEngineSubsystem::CleanupTimerCallback(float DeltaTime)
{
	Cleanup();
	return true;
}

void UText3DEngineSubsystem::Cleanup()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UText3DEngineSubsystem_Cleanup);

	for (auto It = CachedFonts.CreateIterator(); It; ++It)
	{
		if (It.Value().Cleanup())
		{
			It.RemoveCurrent();
		}
	}
}

FCachedFontData& UText3DEngineSubsystem::GetCachedFontData(UFont* Font)
{
	uint32 FontHash = HashCombine(0, GetTypeHash(Font));
	if (CachedFonts.Contains(FontHash))
	{
		uint32 TypefaceFontDataHash = 0;

		// First we check if the font itself has changed since we last cached it
		const FCompositeFont* CompositeFont = Font->GetCompositeFont();
		if (CompositeFont && CompositeFont->DefaultTypeface.Fonts.Num() > 0)
		{
			const FTypefaceEntry& Typeface = CompositeFont->DefaultTypeface.Fonts[0];
			TypefaceFontDataHash = HashCombine(0, GetTypeHash(Typeface.Font));
		}

		if (CachedFonts[FontHash].GetTypefaceFontDataHash() != TypefaceFontDataHash)
		{
			CachedFonts.Remove(FontHash);
		}
	}

	if (!CachedFonts.Contains(FontHash))
	{
		FCachedFontData& CachedFontData = CachedFonts.Add(FontHash);
		CachedFontData.Font = Font;
		CachedFontData.LoadFreeTypeFace();
	}

	return CachedFonts[FontHash];
}

FCachedFontData::FCachedFontData()
{
	TypefaceFontDataHash = 0;
	CacheCounter = MakeShared<int32>(0);
	Font = nullptr;
	FreeTypeFace = nullptr;
}

FCachedFontData::~FCachedFontData()
{
	ClearFreeTypeFace();
}

void FCachedFontData::ClearFreeTypeFace()
{
	TypefaceFontDataHash = 0;
	if (FreeTypeFace)
	{
		FT_Done_Face(FreeTypeFace);
		FreeTypeFace = nullptr;
		Data.Reset();
	}
}

void FCachedFontData::LoadFreeTypeFace()
{
	ClearFreeTypeFace();
	if (!Font)
	{
		return;
	}

	const FCompositeFont* const CompositeFont = Font->GetCompositeFont();
	if (!CompositeFont || CompositeFont->DefaultTypeface.Fonts.Num() == 0)
	{
		return;
	}

	const FTypefaceEntry& Typeface = CompositeFont->DefaultTypeface.Fonts[0];
	const FFontFaceDataConstPtr FaceData = Typeface.Font.GetFontFaceData();

	if (FaceData.IsValid() && FaceData->HasData() && FaceData->GetData().Num() > 0)
	{
		Data = FaceData->GetData();
		FT_New_Memory_Face(FText3DModule::GetFreeTypeLibrary(), Data.GetData(), Data.Num(), 0, &FreeTypeFace);
	}
	else if (FFileHelper::LoadFileToArray(Data, *Typeface.Font.GetFontFilename()) && Data.Num() > 0)
	{
		FT_New_Memory_Face(FText3DModule::GetFreeTypeLibrary(), Data.GetData(), Data.Num(), 0, &FreeTypeFace);
	}

	if (FreeTypeFace)
	{
		TypefaceFontDataHash = HashCombine(0, GetTypeHash(Typeface.Font));
		FT_Set_Char_Size(FreeTypeFace, FontSize, FontSize, 96, 96);
		FT_Set_Pixel_Sizes(FreeTypeFace, FontSize, FontSize);
	}
}

uint32 FCachedFontData::GetTypefaceFontDataHash()
{
	return TypefaceFontDataHash;
}

TSharedPtr<int32> FCachedFontData::GetCacheCounter()
{
	return CacheCounter;
}

TSharedPtr<int32> FCachedFontData::GetMeshesCacheCounter(const FGlyphMeshParameters& Parameters)
{
	const uint32 HashParameters = GetTypeHash(Parameters);
	FCachedFontMeshes& CachedMeshes = Meshes.FindOrAdd(HashParameters);

	return CachedMeshes.GetCacheCounter();
}

UStaticMesh* FCachedFontData::GetGlyphMesh(uint32 GlyphIndex, const FGlyphMeshParameters& Parameters)
{
	const uint32 HashParameters = GetTypeHash(Parameters);
	FCachedFontMeshes& CachedMeshes = Meshes.FindOrAdd(HashParameters);

	TObjectPtr<UStaticMesh>* CachedStaticMesh = CachedMeshes.Glyphs.Find(GlyphIndex);
	if (CachedStaticMesh)
	{
		return *CachedStaticMesh;
	}

	uint32 HashGroup = 0;
	HashGroup = HashCombine(HashGroup, GetTypeHash(Font));
	HashGroup = HashCombine(HashGroup, GetTypeHash(GlyphIndex));
	const FString StaticMeshName = FString::Printf(TEXT("Text3D_Char_%u_%u"), HashGroup, HashParameters);

	const TSharedContourNode Root = GetGlyphContours(GlyphIndex);
	if (Root->Children.Num() == 0)
	{
		return nullptr;
	}

	UText3DEngineSubsystem* Subsystem = GEngine->GetEngineSubsystem<UText3DEngineSubsystem>();
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Subsystem, *StaticMeshName);
	CachedMeshes.Glyphs.Add(GlyphIndex, StaticMesh);

	FMeshCreator MeshCreator;
	MeshCreator.CreateMeshes(Root, Parameters.Extrude, Parameters.Bevel, Parameters.BevelType, Parameters.BevelSegments, Parameters.bOutline, Parameters.OutlineExpand);
	MeshCreator.SetFrontAndBevelTextureCoordinates(Parameters.Bevel);
	MeshCreator.MirrorGroups(Parameters.Extrude);
		
	MeshCreator.BuildMesh(StaticMesh, Subsystem->DefaultMaterial);

	return StaticMesh;
}

FT_Face FCachedFontData::GetFreeTypeFace()
{ 
	return FreeTypeFace;
}

const FString& FCachedFontData::GetFontName()
{
	return FontName;
}

TSharedContourNode FCachedFontData::GetGlyphContours(uint32 GlyphIndex)
{
	check(FreeTypeFace);

	if (FT_Load_Glyph(FreeTypeFace, GlyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP))
	{
		return nullptr;
	}

	FGlyphLoader GlyphLoader(FreeTypeFace->glyph);
	TSharedContourNode Root = GlyphLoader.GetContourList();

	return Root;
}

bool FCachedFontData::Cleanup()
{
	// If there is no Text3D objects using the same font we can just release it all
	if (CacheCounter.GetSharedReferenceCount() <= 1)
	{
		Meshes.Reset();
		Glyphs.Reset();
		ClearFreeTypeFace();
		return true;
	}

	for (auto It = Meshes.CreateIterator(); It; ++It)
	{
		if (It.Value().GetCacheCount() <= 1)
		{
			It.RemoveCurrent();
		}
	}

	return false;
}
