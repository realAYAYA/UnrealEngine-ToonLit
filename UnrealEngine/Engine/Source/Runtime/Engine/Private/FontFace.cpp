// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/FontFace.h"
#include "Engine/Font.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontCache.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FontFace)

DEFINE_LOG_CATEGORY_STATIC(LogFontFace, Log, All);

UFontFace::UFontFace()
	: AscendOverriddenValue(0)
	, bIsAscendOverridden(false)
	, DescendOverriddenValue(0)
	, bIsDescendOverridden(false)
	, FontFaceData(FFontFaceData::MakeFontFaceData())

{
}

void UFontFace::Serialize(FArchive& Ar)
{
	LLM_SCOPE_BYNAME(TEXT("FontFaceData"));
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	Super::Serialize(Ar);

	bool bCooked = Ar.IsCooking();
	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::AddedCookedBoolFontFaceAssets)
	{
		Ar << bCooked;
	}

	if (Ar.IsLoading())
	{
		if (FPlatformProperties::RequiresCookedData() || bCooked)
		{
			SourceFilename = GetCookedFilename();
		}

		if (Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::AddedInlineFontFaceAssets)
		{
#if WITH_EDITORONLY_DATA
			// Port the old property data into the shared instance
			FontFaceData->SetData(MoveTemp(FontFaceData_DEPRECATED));
#endif // WITH_EDITORONLY_DATA
		}
		else
		{
			bool bLoadInlineData = false;
			Ar << bLoadInlineData;

			if (bLoadInlineData)
			{
				if (FontFaceData->HasData())
				{
					// If we already have data, make a new instance in-case the existing one is being referenced by the font cache
					FontFaceData = FFontFaceData::MakeFontFaceData();
				}
				FontFaceData->Serialize(Ar);
			}
		}

#if WITH_EDITORONLY_DATA
		CacheSubFaces();
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
		// Only save the inline data in a cooked build if we're using the inline loading policy
		bool bSaveInlineData = LoadingPolicy == EFontLoadingPolicy::Inline || !Ar.IsCooking();
		Ar << bSaveInlineData;

		if (bSaveInlineData)
		{
			FontFaceData->Serialize(Ar);
		}
	}
}

void UFontFace::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	const bool bCountInlineData = WITH_EDITORONLY_DATA || LoadingPolicy == EFontLoadingPolicy::Inline;
	// Only count the memory size for fonts that will be loaded
	if (bCountInlineData)
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FontFaceData->GetData().GetAllocatedSize());
	}
	// only get size if lazy loading.  Resident font memory wont exist for EFontLoadingPolicy::Stream
	else if(LoadingPolicy == EFontLoadingPolicy::LazyLoad && FSlateApplication::IsInitialized())
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FSlateApplication::Get().GetRenderer()->GetFontCache()->GetFontDataAssetResidentMemory(this));
	}

}

#if WITH_EDITOR
void UFontFace::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

#if WITH_EDITORONLY_DATA
	CacheSubFaces();
#endif // WITH_EDITORONLY_DATA

	FSlateApplication::Get().GetRenderer()->FlushFontCache(TEXT("UFontFace::PostEditChangeProperty"));
}

void UFontFace::PostEditUndo()
{
	Super::PostEditUndo();

#if WITH_EDITORONLY_DATA
	CacheSubFaces();
#endif // WITH_EDITORONLY_DATA

	FSlateApplication::Get().GetRenderer()->FlushFontCache(TEXT("UFontFace::PostEditUndo"));
}

void UFontFace::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UFontFace::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	FAssetImportInfo ImportInfo;
	ImportInfo.Insert(FAssetImportInfo::FSourceFile(SourceFilename));
	Context.AddTag(FAssetRegistryTag(SourceFileTagName(), ImportInfo.ToJson(), FAssetRegistryTag::TT_Hidden));
}

void UFontFace::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
	TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	if (LoadingPolicy != EFontLoadingPolicy::Inline)
	{
		// Iterative COTF can't handle the .ufont files generated when this UFontFace is within a UFont asset (rather than its own asset)
		if (UFont* OuterFont = GetTypedOuter<UFont>())
		{
			UE_LOG(LogFontFace, Warning, 
				TEXT("The font asset '%s' contains nested font faces which can cause issues for iterative cook-on-the-fly."
					"Please edit the font asset and split the font faces into their own assets."),
				*OuterFont->GetPathName());
		}

		// We replace the package name with the cooked font face name
		// Note: This must match the replacement logic in UFontFace::GetCookedFilename
		const FString CookedFontFilename = FPaths::GetPath(PackageFilename) / GetName() + TEXT(".ufont");
		const TArray<uint8>& Data = FontFaceData->GetData();
		const int32 NumBytes = Data.Num() * Data.GetTypeSize();
		WriteAdditionalFile(*CookedFontFilename, (void*)Data.GetData(), NumBytes);
	}
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UFontFace::CacheSubFaces()
{
	if (FSlateApplication::IsInitialized())
	{
		SubFaces = FSlateApplication::Get().GetRenderer()->GetFontCache()->GetAvailableFontSubFaces(FontFaceData);
	}
}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA
void UFontFace::InitializeFromBulkData(const FString& InFilename, const EFontHinting InHinting, const void* InBulkDataPtr, const int32 InBulkDataSizeBytes)
{
	check(InBulkDataPtr && InBulkDataSizeBytes > 0 && !FontFaceData->HasData());

	SourceFilename = InFilename;
	Hinting = InHinting;
	LoadingPolicy = EFontLoadingPolicy::LazyLoad;
	
	TArray<uint8> FontData;
	FontData.Append(static_cast<const uint8*>(InBulkDataPtr), InBulkDataSizeBytes);
	FontFaceData->SetData(MoveTemp(FontData));

	CacheSubFaces();
}
#endif // WITH_EDITORONLY_DATA

const FString& UFontFace::GetFontFilename() const
{
	return SourceFilename;
}

EFontHinting UFontFace::GetHinting() const
{
	return Hinting;
}

EFontLoadingPolicy UFontFace::GetLoadingPolicy() const
{
	return LoadingPolicy;
}

EFontLayoutMethod UFontFace::GetLayoutMethod() const
{
	return LayoutMethod;
}

bool UFontFace::IsAscendOverridden() const
{
	return bIsAscendOverridden;
}

int32 UFontFace::GetAscendOverriddenValue() const
{
	return AscendOverriddenValue;
}

bool UFontFace::IsDescendOverridden() const
{
	return bIsDescendOverridden;
}

int32 UFontFace::GetDescendOverriddenValue() const
{
	return DescendOverriddenValue;
}

FFontFaceDataConstRef UFontFace::GetFontFaceData() const
{
	return FontFaceData;
}

FString UFontFace::GetCookedFilename() const
{
	// UFontFace assets themselves can't be localized, however that doesn't mean the package they're in isn't localized (ie, when they're upgraded into a UFont asset)
	FString PackageName = GetOutermost()->GetName();
	if (!GIsEditor)
	{
		PackageName = FPackageName::GetLocalizedPackagePath(PackageName);
	}
	
	// Note: This must match the replacement logic in UFontFace::CookAdditionalFiles
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
	return FPaths::GetPath(PackageFilename) / GetName() + TEXT(".ufont");
}

