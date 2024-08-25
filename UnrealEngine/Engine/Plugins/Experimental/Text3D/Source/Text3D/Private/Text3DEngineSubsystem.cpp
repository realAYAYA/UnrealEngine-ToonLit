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
#include "Fonts/FontCacheFreeType.h"
#include "UObject/ConstructorHelpers.h"

FCachedFontMeshes::FCachedFontMeshes()
{
	CacheCounter = MakeShared<int32>(0);
}

int32 FCachedFontMeshes::GetCacheCount() const
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

bool UText3DEngineSubsystem::Exec(UWorld* InWorld, const TCHAR* InCmd, FOutputDevice& InAr)
{
	if (HasAllFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	if (FParse::Command(&InCmd, TEXT("text3d")))
	{
		if (FParse::Command(&InCmd, TEXT("cache")))
		{
			if (FParse::Command(&InCmd, TEXT("show")))
			{
				PrintCache();
				return true;
			}
			else if (FParse::Command(&InCmd, TEXT("clear")))
			{
				Cleanup();
				PrintCache();

				return true;
			}
		}
	}

	return false;
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

void UText3DEngineSubsystem::PrintCache() const
{
	UE_LOG(LogText3D, Log, TEXT("Text3D Engine Subsystem is currently caching the following fonts:"));
	for (const TPair<uint32, FCachedFontData>& CachedFont : CachedFonts)
	{
		CachedFont.Value.PrintCache();
	}
}

void UText3DEngineSubsystem::Cleanup()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UText3DEngineSubsystem_Cleanup);

	UE_LOG(LogText3D, Log, TEXT("Text3D Engine Subsystem is clearing unreferenced cached fonts and typefaces."));
	for (auto It = CachedFonts.CreateIterator(); It; ++It)
	{
		if (It.Value().Cleanup())
		{
			It.RemoveCurrent();
		}
	}
}

FCachedFontData& UText3DEngineSubsystem::GetCachedFontData(UFont* InFont, int32 InTypefaceEntryIndex)
{
	const uint32 FontHash = HashCombine(0, GetTypeHash(InFont));

	if (CachedFonts.Contains(FontHash))
	{
		uint32 TypefaceFontDataHash = 0;

		// First we check if the font itself has changed since we last cached it
		const FCompositeFont* CompositeFont = InFont->GetCompositeFont();
		if (CompositeFont && !CompositeFont->DefaultTypeface.Fonts.IsEmpty())
		{
			FTypefaceEntry Typeface;
			if (CompositeFont->DefaultTypeface.Fonts.IsValidIndex(InTypefaceEntryIndex))
			{
				Typeface = CompositeFont->DefaultTypeface.Fonts[InTypefaceEntryIndex];
			}
			else
			{
				Typeface = CompositeFont->DefaultTypeface.Fonts[0];
			}

			TypefaceFontDataHash = HashCombine(0, GetTypeHash(Typeface.Font));
		}

		const uint32 CachedTypefaceFontDataHash = CachedFonts[FontHash].GetTypefaceFontDataHash(InTypefaceEntryIndex);
		if (CachedTypefaceFontDataHash != TypefaceFontDataHash)
		{
			// FontFace has changed, remove the cached data
			CachedFonts[FontHash].CleanupTypeface(InTypefaceEntryIndex);
		}
	}

	if (!CachedFonts.Contains(FontHash))
	{
		FCachedFontData& NewCachedFontData = CachedFonts.Add(FontHash);
		NewCachedFontData.SetFont(InFont);
	}

	FCachedFontData& CachedFontData = CachedFonts[FontHash];
	CachedFontData.LoadFreeTypeFace(InTypefaceEntryIndex);

	return CachedFonts[FontHash];
}

FTypefaceFontData::FTypefaceFontData()
	: FreeTypeFace(nullptr)
	, TypefaceFontDataHash(0)
{
	CacheCounter = MakeShared<int32>(0);
}

FCachedFontData::FCachedFontData()
{
	Font = nullptr;
}

FCachedFontData::~FCachedFontData()
{
	ClearFreeTypeFace();
}

void FCachedFontData::ClearFreeTypeFace()
{
	TypefaceFontDataMap.Reset();
}

void FCachedFontData::LoadFreeTypeFace(uint32 InTypefaceIndex)
{
	const bool bContainsTypeface = TypefaceFontDataMap.Contains(InTypefaceIndex);
	if (bContainsTypeface)
	{
		return;
	}

	if (!Font)
	{
		return;
	}

	const FCompositeFont* const CompositeFont = Font->GetCompositeFont();
	if (!CompositeFont || CompositeFont->DefaultTypeface.Fonts.IsEmpty())
	{
		return;
	}

	if (!CompositeFont->DefaultTypeface.Fonts.IsValidIndex(InTypefaceIndex))
	{
		return;
	}

	const FTypefaceEntry& Typeface = CompositeFont->DefaultTypeface.Fonts[InTypefaceIndex];
	const FFontFaceDataConstPtr FaceData = Typeface.Font.GetFontFaceData();

	if (!bContainsTypeface)
	{
		FTypefaceFontData& TypefaceFontData = TypefaceFontDataMap.Add(InTypefaceIndex);
		TypefaceFontData.SetTypefaceName(Typeface.Name);
	}

	if (FaceData.IsValid() && FaceData->HasData() && FaceData->GetData().Num() > 0)
	{
		TypefaceFontDataMap[InTypefaceIndex].SetTypefaceData(FaceData->GetData());
		FT_New_Memory_Face(FText3DModule::GetFreeTypeLibrary(), TypefaceFontDataMap[InTypefaceIndex].GetTypefaceData().GetData(), TypefaceFontDataMap[InTypefaceIndex].GetTypefaceData().Num(), 0, &TypefaceFontDataMap[InTypefaceIndex].GetTypeface());
	}
	else if (FFileHelper::LoadFileToArray(TypefaceFontDataMap[InTypefaceIndex].GetTypefaceData(), *Typeface.Font.GetFontFilename()) && TypefaceFontDataMap[InTypefaceIndex].GetTypefaceData().Num() > 0)
	{
		FT_New_Memory_Face(FText3DModule::GetFreeTypeLibrary(), TypefaceFontDataMap[InTypefaceIndex].GetTypefaceData().GetData(), TypefaceFontDataMap[InTypefaceIndex].GetTypefaceData().Num(), 0, &TypefaceFontDataMap[InTypefaceIndex].GetTypeface());
	}

	if (const FT_Face* FreeTypeface = &TypefaceFontDataMap[InTypefaceIndex].GetTypeface())
	{
		TypefaceFontDataMap[InTypefaceIndex].SetTypefaceFontDataHash(HashCombine(0, GetTypeHash(Typeface.Font)));
		FT_Set_Char_Size(*FreeTypeface, FontSize, FontSize, 96, 96);
		FT_Set_Pixel_Sizes(*FreeTypeface, FontSize, FontSize);
	}
}

uint32 FCachedFontData::GetTypefaceFontDataHash(int32 InTypefaceEntryIndex) const
{
	if (TypefaceFontDataMap.Contains(InTypefaceEntryIndex))
	{
		return TypefaceFontDataMap[InTypefaceEntryIndex].GetTypefaceFontDataHash();
	}

	return 0;
}

TSharedPtr<int32> FCachedFontData::GetCacheCounter(int32 InTypefaceEntryIndex)
{
	if (TypefaceFontDataMap.Contains(InTypefaceEntryIndex))
	{
		return TypefaceFontDataMap[InTypefaceEntryIndex].GetCacheCounter();
	}

	return nullptr;
}

TSharedPtr<int32> FCachedFontData::GetMeshesCacheCounter(const FGlyphMeshParameters& InParameters)
{
	const uint32 HashParameters = GetTypeHash(InParameters);

	const uint32 TypefaceIndex = InParameters.TypefaceIndex;
	if (TypefaceFontDataMap.Contains(TypefaceIndex))
	{
		FCachedFontMeshes& CachedMeshes = TypefaceFontDataMap[TypefaceIndex].FindOrAddMeshes(HashParameters);
		return CachedMeshes.GetCacheCounter();
	}

	return nullptr;
}

UStaticMesh* FCachedFontData::GetGlyphMesh(uint32 InGlyphIndex, const FGlyphMeshParameters& InParameters, const FFreeTypeFace* InFontFaceData)
{
	const uint32 TypefaceIndex = InParameters.TypefaceIndex;
	const uint32 HashParameters = GetTypeHash(InParameters);

	if (!TypefaceFontDataMap.Contains(TypefaceIndex))
	{
		return nullptr;
	}

	FCachedFontMeshes& CachedMeshes = TypefaceFontDataMap[TypefaceIndex].FindOrAddMeshes(HashParameters);

	TObjectPtr<UStaticMesh>* CachedStaticMesh = CachedMeshes.Glyphs.Find(InGlyphIndex);
	if (CachedStaticMesh)
	{
		return *CachedStaticMesh;
	}

	uint32 HashGroup = 0;
	HashGroup = HashCombine(HashGroup, GetTypeHash(Font));
	HashGroup = HashCombine(HashGroup, GetTypeHash(InGlyphIndex));
	const FString StaticMeshName = FString::Printf(TEXT("Text3D_Char_%u_%u"), HashGroup, HashParameters);

	const TSharedContourNode Root = GetGlyphContours(InGlyphIndex, TypefaceIndex, InFontFaceData);
	if (Root->Children.Num() == 0)
	{
		return nullptr;
	}

	UText3DEngineSubsystem* Subsystem = GEngine->GetEngineSubsystem<UText3DEngineSubsystem>();
	const FName StrokeShapeUniqueName = MakeUniqueObjectName(Subsystem, UStaticMesh::StaticClass(), FName(*StaticMeshName));

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Subsystem, StrokeShapeUniqueName);
	CachedMeshes.Glyphs.Add(InGlyphIndex, StaticMesh);

	FMeshCreator MeshCreator;
	MeshCreator.CreateMeshes(Root, InParameters.Extrude, InParameters.Bevel, InParameters.BevelType, InParameters.BevelSegments, InParameters.bOutline, InParameters.OutlineExpand);
	MeshCreator.SetFrontAndBevelTextureCoordinates(InParameters.Bevel);
	MeshCreator.MirrorGroups(InParameters.Extrude);

	MeshCreator.BuildMesh(StaticMesh, Subsystem->DefaultMaterial);

	return StaticMesh;
}

FT_Face FCachedFontData::GetFreeTypeFace(uint32 InTypefaceIndex)
{
	if (TypefaceFontDataMap.Contains(InTypefaceIndex))
	{
		return TypefaceFontDataMap[InTypefaceIndex].GetTypeface();
	}

	return nullptr;
}

FString FCachedFontData::GetFontName() const
{
	if (Font)
	{
		return Font->GetName();
	}

	return TEXT("");
}

TSharedContourNode FCachedFontData::GetGlyphContours(uint32 InGlyphIndex, int32 InTypefaceEntryIndex, const FFreeTypeFace* InFontFaceData)
{
	const bool bFontFaceValid = InFontFaceData && InFontFaceData->IsFaceValid();
	if (!bFontFaceValid && !GetFreeTypeFace(InTypefaceEntryIndex))
	{
		return nullptr;
	}

	const FT_Face& GlyphFreeTypeFace = bFontFaceValid
		? InFontFaceData->GetFace()
		: GetFreeTypeFace(InTypefaceEntryIndex);
	check(GlyphFreeTypeFace);

	if (FT_Load_Glyph(GlyphFreeTypeFace, InGlyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP))
	{
		return nullptr;
	}

#if UE_BUILD_DEBUG
	// If true, the symbol itself was probably bitmap-only, and this will have placeholder contours
	bool bIsLastResort = InFontFaceData && InFontFaceData->GetFace()->family_name
		? FString(InFontFaceData->GetFace()->family_name) == TEXT("LastResort")
		: false;
	if (bIsLastResort)
	{
		UE_LOG(LogText3D, Warning, TEXT("The following glyph index is bitmap-only: %i"), InGlyphIndex);
	}
#endif

	FGlyphLoader GlyphLoader(GlyphFreeTypeFace->glyph);
	TSharedContourNode Root = GlyphLoader.GetContourList();

	return Root;
}

void FCachedFontData::PrintCache() const
{
	UE_LOG(LogText3D, Log, TEXT("\n== Cached typefaces for font: %s =="), *GetFontName());

	for (const TPair<uint32, FTypefaceFontData>& TypefaceData : TypefaceFontDataMap)
	{
		UE_LOG(LogText3D, Log, TEXT("\t- %s"), *TypefaceData.Value.GetTypefaceName().ToString());
	}
}

bool FCachedFontData::Cleanup()
{
	TArray<uint32> TypefaceIndexes;
	TypefaceFontDataMap.GetKeys(TypefaceIndexes);

	for (const uint32 TypefaceIndex : TypefaceIndexes)
	{
		CleanupTypeface(TypefaceIndex);
	}

	if (TypefaceFontDataMap.IsEmpty())
	{
		return true;
	}

	return false;
}

bool FCachedFontData::CleanupTypeface(uint32 InTypefaceIndex)
{
	bool bRemoved = false;
	if (TypefaceFontDataMap.Contains(InTypefaceIndex))
	{
		FTypefaceFontData& TypefaceFontData = TypefaceFontDataMap[InTypefaceIndex];

		for (auto MeshesIterator = TypefaceFontData.GetMeshes().CreateIterator(); MeshesIterator; ++MeshesIterator)
		{
			if (MeshesIterator.Value().GetCacheCount() <= 1)
			{
				MeshesIterator.RemoveCurrent();
			}
		}

		if (TypefaceFontData.GetCacheCount() <= 1)
		{
			TypefaceFontData.Reset();

			bRemoved = true;
		}
	}

	if (bRemoved)
	{
		TypefaceFontDataMap.Remove(InTypefaceIndex);
	}

	return bRemoved;
}
