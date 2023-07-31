// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Fonts/FontFaceInterface.h"
#include "FontFace.generated.h"

class ITargetPlatform;
struct FPropertyChangedEvent;

/**
 * A font face asset contains the raw payload data for a source TTF/OTF file as used by FreeType.
 * During cook this asset type generates a ".ufont" file containing the raw payload data (unless loaded "Inline").
 */
UCLASS(hidecategories=Object, autoexpandcategories=FontFace, MinimalAPI, BlueprintType)
class UFontFace : public UObject, public IFontFaceInterface
{
	GENERATED_BODY()

public:
	/** Default constructor */
	UFontFace();

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
#endif // WITH_EDITOR
	//~ End UObject Interface

#if WITH_EDITORONLY_DATA
	ENGINE_API void CacheSubFaces();
#endif // WITH_EDITORONLY_DATA

	//~ Begin IFontFaceInterface Interface
#if WITH_EDITORONLY_DATA
	virtual void InitializeFromBulkData(const FString& InFilename, const EFontHinting InHinting, const void* InBulkDataPtr, const int32 InBulkDataSizeBytes) override;
#endif // WITH_EDITORONLY_DATA
	virtual const FString& GetFontFilename() const override;
	virtual EFontHinting GetHinting() const override;
	virtual EFontLoadingPolicy GetLoadingPolicy() const override;
	virtual EFontLayoutMethod GetLayoutMethod() const override;
	virtual FFontFaceDataConstRef GetFontFaceData() const override;
	//~ End IFontFaceInterface interface

private:
	FString GetCookedFilename() const;
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile);
#endif // WITH_EDITOR
	//~ End UObject Interface

public:
	/** The filename of the font face we were created from. This may not always exist on disk, as we may have previously loaded and cached the font data inside this asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=FontFace)
	FString SourceFilename;

	/** The hinting algorithm to use with the font face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FontFace)
	EFontHinting Hinting;

	/** Enum controlling how this font face should be loaded at runtime. See the enum for more explanations of the options. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FontFace)
	EFontLoadingPolicy LoadingPolicy;

	/** Which method should we use when laying out the font? Try changing this if you notice clipping or height issues with your font. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FontFace, AdvancedDisplay)
	EFontLayoutMethod LayoutMethod;

	/** The data associated with the font face. This should always be filled in providing the source filename is valid. CacheSubFaces should be called after manually changing this property. */
	FFontFaceDataRef FontFaceData;

#if WITH_EDITORONLY_DATA
	/** The data associated with the font face. This should always be filled in providing the source filename is valid. */
	UPROPERTY()
	TArray<uint8> FontFaceData_DEPRECATED;

	/** Transient cache of the sub-faces available within this face */
	UPROPERTY(VisibleAnywhere, Transient, Category=FontFace, AdvancedDisplay)
	TArray<FString> SubFaces;
#endif // WITH_EDITORONLY_DATA
};
