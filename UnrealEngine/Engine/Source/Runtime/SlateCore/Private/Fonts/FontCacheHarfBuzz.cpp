// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fonts/FontCacheHarfBuzz.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontCacheFreeType.h"
#include "Fonts/SlateFontRenderer.h"
#include "Trace/SlateMemoryTags.h"
#include "Fonts/FontUtils.h"

#if WITH_HARFBUZZ

extern "C"
{

void* HarfBuzzMalloc(size_t InSizeBytes)
{
	LLM_SCOPE_BYTAG(UI_Text);
	return FMemory::Malloc(InSizeBytes);
}

void* HarfBuzzCalloc(size_t InNumItems, size_t InItemSizeBytes)
{
	LLM_SCOPE_BYTAG(UI_Text);
	const size_t AllocSizeBytes = InNumItems * InItemSizeBytes;
	if (AllocSizeBytes > 0)
	{
		void* Ptr = FMemory::Malloc(AllocSizeBytes);
		FMemory::Memzero(Ptr, AllocSizeBytes);
		return Ptr;
	}
	return nullptr;
}

void* HarfBuzzRealloc(void* InPtr, size_t InSizeBytes)
{
	LLM_SCOPE_BYTAG(UI_Text);
	return FMemory::Realloc(InPtr, InSizeBytes);
}

void HarfBuzzFree(void* InPtr)
{
	FMemory::Free(InPtr);
}

} // extern "C"

#endif // #if WITH_HARFBUZZ

namespace HarfBuzzUtils
{

#if WITH_HARFBUZZ

namespace Internal
{

template <bool IsUnicode, size_t TCHARSize>
void AppendStringToBuffer(const FStringView InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	// todo: SHAPING - This is losing the context information that may be required to shape a sub-section of text.
	//				   In practice this may not be an issue as our platforms should all use the other functions, but to fix it we'd need UTF-8 iteration functions to find the correct points the buffer
	FStringView SubString = InString.Mid(InStartIndex, InLength);
	FTCHARToUTF8 SubStringUtf8(SubString.GetData(), SubString.Len());
	hb_buffer_add_utf8(InHarfBuzzTextBuffer, (const char*)SubStringUtf8.Get(), SubStringUtf8.Length(), 0, SubStringUtf8.Length());
}

template <>
void AppendStringToBuffer<true, 2>(const FStringView InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	// A unicode encoding with a TCHAR size of 2 bytes is assumed to be UTF-16
	hb_buffer_add_utf16(InHarfBuzzTextBuffer, reinterpret_cast<const uint16_t*>(InString.GetData()), InString.Len(), InStartIndex, InLength);
}

template <>
void AppendStringToBuffer<true, 4>(const FStringView InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	// A unicode encoding with a TCHAR size of 4 bytes is assumed to be UTF-32
	hb_buffer_add_utf32(InHarfBuzzTextBuffer, reinterpret_cast<const uint32_t*>(InString.GetData()), InString.Len(), InStartIndex, InLength);
}

} // namespace Internal

void AppendStringToBuffer(const FStringView InString, hb_buffer_t* InHarfBuzzTextBuffer)
{
	return Internal::AppendStringToBuffer<FPlatformString::IsUnicodeEncoded, sizeof(TCHAR)>(InString, 0, InString.Len(), InHarfBuzzTextBuffer);
}

void AppendStringToBuffer(const FStringView InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	return Internal::AppendStringToBuffer<FPlatformString::IsUnicodeEncoded, sizeof(TCHAR)>(InString, InStartIndex, InLength, InHarfBuzzTextBuffer);
}

#endif // #if WITH_HARFBUZZ

} // namespace HarfBuzzUtils


#if WITH_FREETYPE && WITH_HARFBUZZ

namespace HarfBuzzFontFunctions
{

hb_user_data_key_t UserDataKey;

struct FUserData
{
	FUserData(const float InFontSize, const float InFontScale, FFreeTypeCacheDirectory* InFTCacheDirectory, const hb_font_extents_t& InHarfBuzzFontExtents)
		: FontSize(InFontSize)
		, FontScale(InFontScale)
		, FTCacheDirectory(InFTCacheDirectory)
		, HarfBuzzFontExtents(InHarfBuzzFontExtents)
	{
	}

	float FontSize;
	float FontScale;
	FFreeTypeCacheDirectory* FTCacheDirectory;
	hb_font_extents_t HarfBuzzFontExtents;
};

void* CreateUserData(const float InFontSize, const float InFontScale, FFreeTypeCacheDirectory* InFTCacheDirectory, const hb_font_extents_t& InHarfBuzzFontExtents)
{
	return new FUserData(InFontSize, InFontScale, InFTCacheDirectory, InHarfBuzzFontExtents);
}

void DestroyUserData(void* UserData)
{
	FUserData* UserDataPtr = static_cast<FUserData*>(UserData);
	delete UserDataPtr;
}

namespace Internal
{

FORCEINLINE FT_Face get_ft_face(hb_font_t* InFont)
{
	hb_font_t* FontParent = hb_font_get_parent(InFont);
	check(FontParent);
	return hb_ft_font_get_face(FontParent);
}

FORCEINLINE int32 get_ft_flags(hb_font_t* InFont)
{
	hb_font_t* FontParent = hb_font_get_parent(InFont);
	check(FontParent);
	return hb_ft_font_get_load_flags(FontParent);
}

hb_bool_t get_font_h_extents(hb_font_t* InFont, void* InFontData, hb_font_extents_t* OutMetrics, void* InUserData)
{
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));
	*OutMetrics = UserDataPtr->HarfBuzzFontExtents;
	return true;
}

unsigned int get_nominal_glyphs(hb_font_t* InFont, void* InFontData, unsigned int InCount, const hb_codepoint_t *InUnicodeCharBuffer, unsigned int InUnicodeCharBufferStride, hb_codepoint_t *OutGlyphIndexBuffer, unsigned int InGlyphIndexBufferStride, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);

	const uint8* UnicodeCharRawBuffer = (const uint8*)InUnicodeCharBuffer;
	uint8* GlyphIndexRawBuffer = (uint8*)OutGlyphIndexBuffer;

	for (unsigned int ItemIndex = 0; ItemIndex < InCount; ++ItemIndex)
	{
		const hb_codepoint_t* UnicodeCharPtr = (const hb_codepoint_t*)UnicodeCharRawBuffer;
		hb_codepoint_t* OutGlyphIndexPtr = (hb_codepoint_t*)GlyphIndexRawBuffer;

		*OutGlyphIndexPtr = FT_Get_Char_Index(FreeTypeFace, *UnicodeCharPtr);

		// If the given font can't render that character (as the fallback font may be missing), try again with the fallback character
		if (*UnicodeCharPtr != 0 && *OutGlyphIndexPtr == 0)
		{
			*OutGlyphIndexPtr = FT_Get_Char_Index(FreeTypeFace, SlateFontRendererUtils::InvalidSubChar);
		}

		// If this resolution failed, return the number if items we managed to process
		if (*UnicodeCharPtr != 0 && *OutGlyphIndexPtr == 0)
		{
			return ItemIndex;
		}

		// Advance the buffers
		UnicodeCharRawBuffer += InUnicodeCharBufferStride;
		GlyphIndexRawBuffer += InGlyphIndexBufferStride;
	}

	// Processed everything - return the count
	return InCount;
}

hb_bool_t get_nominal_glyph(hb_font_t* InFont, void* InFontData, hb_codepoint_t InUnicodeChar, hb_codepoint_t* OutGlyphIndex, void* InUserData)
{
	return get_nominal_glyphs(InFont, InFontData, 1, &InUnicodeChar, sizeof(hb_codepoint_t), OutGlyphIndex, sizeof(hb_codepoint_t), InUserData) == 1;
}

void get_glyph_h_advances(hb_font_t* InFont, void* InFontData, unsigned int InCount, const hb_codepoint_t* InGlyphIndexBuffer, unsigned int InGlyphIndexBufferStride, hb_position_t* OutAdvanceBuffer, unsigned int InAdvanceBufferStride, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	int ScaleMultiplier = 1;
	{
		int FontXScale = 0;
		int FontYScale = 0;
		hb_font_get_scale(InFont, &FontXScale, &FontYScale);

		if (FontXScale < 0)
		{
			ScaleMultiplier = -1;
		}
	}

	const uint8* GlyphIndexRawBuffer = (const uint8*)InGlyphIndexBuffer;
	uint8* AdvanceRawBuffer = (uint8*)OutAdvanceBuffer;
	TSharedRef<FFreeTypeAdvanceCache> AdvanceCache = UserDataPtr->FTCacheDirectory->GetAdvanceCache(FreeTypeFace, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale);

	for (unsigned int ItemIndex = 0; ItemIndex < InCount; ++ItemIndex)
	{
		const hb_codepoint_t* GlyphIndexPtr = (const hb_codepoint_t*)GlyphIndexRawBuffer;
		hb_position_t* OutAdvancePtr = (hb_position_t*)AdvanceRawBuffer;

		FT_Fixed CachedAdvanceData = 0;
		if (AdvanceCache->FindOrCache(*GlyphIndexPtr, CachedAdvanceData))
		{
			*OutAdvancePtr = ((CachedAdvanceData * ScaleMultiplier) + (1<<9)) >> 10;
		}
		else
		{
			*OutAdvancePtr = 0;
		}

		// Advance the buffers
		GlyphIndexRawBuffer += InGlyphIndexBufferStride;
		AdvanceRawBuffer += InAdvanceBufferStride;
	}
}

hb_position_t get_glyph_h_advance(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, void* InUserData)
{
	hb_position_t Advance = 0;
	get_glyph_h_advances(InFont, InFontData, 1, &InGlyphIndex, sizeof(hb_codepoint_t), &Advance, sizeof(hb_position_t), InUserData);
	return Advance;
}

void get_glyph_v_advances(hb_font_t* InFont, void* InFontData, unsigned int InCount, const hb_codepoint_t* InGlyphIndexBuffer, unsigned int InGlyphIndexBufferStride, hb_position_t* OutAdvanceBuffer, unsigned int InAdvanceBufferStride, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	int ScaleMultiplier = 1;
	{
		int FontXScale = 0;
		int FontYScale = 0;
		hb_font_get_scale(InFont, &FontXScale, &FontYScale);

		if (FontYScale < 0)
		{
			ScaleMultiplier = -1;
		}
	}

	const uint8* GlyphIndexRawBuffer = (const uint8*)InGlyphIndexBuffer;
	uint8* AdvanceRawBuffer = (uint8*)OutAdvanceBuffer;
	TSharedRef<FFreeTypeAdvanceCache> AdvanceCache = UserDataPtr->FTCacheDirectory->GetAdvanceCache(FreeTypeFace, FreeTypeFlags | FT_LOAD_VERTICAL_LAYOUT, UserDataPtr->FontSize, UserDataPtr->FontScale);

	for (unsigned int ItemIndex = 0; ItemIndex < InCount; ++ItemIndex)
	{
		const hb_codepoint_t* GlyphIndexPtr = (const hb_codepoint_t*)GlyphIndexRawBuffer;
		hb_position_t* OutAdvancePtr = (hb_position_t*)AdvanceRawBuffer;

		FT_Fixed CachedAdvanceData = 0;
		if (AdvanceCache->FindOrCache(*GlyphIndexPtr, CachedAdvanceData))
		{
			// Note: FreeType's vertical metrics grows downward while other FreeType coordinates have a Y growing upward. Hence the extra negation.
			*OutAdvancePtr = ((-CachedAdvanceData * ScaleMultiplier) + (1<<9)) >> 10;
		}
		else
		{
			*OutAdvancePtr = 0;
		}

		// Advance the buffers
		GlyphIndexRawBuffer += InGlyphIndexBufferStride;
		AdvanceRawBuffer += InAdvanceBufferStride;
	}
}

hb_position_t get_glyph_v_advance(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, void* InUserData)
{
	hb_position_t Advance = 0;
	get_glyph_v_advances(InFont, InFontData, 1, &InGlyphIndex, sizeof(hb_codepoint_t), &Advance, sizeof(hb_position_t), InUserData);
	return Advance;
}

hb_bool_t get_glyph_v_origin(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, hb_position_t* OutX, hb_position_t* OutY, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	TSharedRef<FFreeTypeGlyphCache> GlyphCache = UserDataPtr->FTCacheDirectory->GetGlyphCache(FreeTypeFace, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale);
	FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
	if (GlyphCache->FindOrCache(InGlyphIndex, CachedGlyphData))
	{
		// Note: FreeType's vertical metrics grows downward while other FreeType coordinates have a Y growing upward. Hence the extra negation.
		*OutX = CachedGlyphData.GlyphMetrics.horiBearingX -   CachedGlyphData.GlyphMetrics.vertBearingX;
		*OutY = CachedGlyphData.GlyphMetrics.horiBearingY - (-CachedGlyphData.GlyphMetrics.vertBearingY);

		int FontXScale = 0;
		int FontYScale = 0;
		hb_font_get_scale(InFont, &FontXScale, &FontYScale);

		if (FontXScale < 0)
		{
			*OutX = -*OutX;
		}

		if (FontYScale < 0)
		{
			*OutY = -*OutY;
		}

		return true;
	}

	return false;
}

hb_position_t get_glyph_h_kerning(hb_font_t* InFont, void* InFontData, hb_codepoint_t InLeftGlyphIndex, hb_codepoint_t InRightGlyphIndex, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	TSharedPtr<FFreeTypeKerningCache> KerningCache = UserDataPtr->FTCacheDirectory->GetKerningCache(FreeTypeFace, FT_KERNING_DEFAULT, UserDataPtr->FontSize, UserDataPtr->FontScale);
	if (KerningCache)
	{
		FT_Vector KerningVector;
		if (KerningCache->FindOrCache(InLeftGlyphIndex, InRightGlyphIndex, KerningVector))
		{
			return KerningVector.x;
		}
	}

	return 0;
}

hb_bool_t get_glyph_extents(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, hb_glyph_extents_t* OutExtents, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	TSharedRef<FFreeTypeGlyphCache> GlyphCache = UserDataPtr->FTCacheDirectory->GetGlyphCache(FreeTypeFace, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale);
	FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
	if (GlyphCache->FindOrCache(InGlyphIndex, CachedGlyphData))
	{
		OutExtents->x_bearing	=  CachedGlyphData.GlyphMetrics.horiBearingX;
		OutExtents->y_bearing	=  CachedGlyphData.GlyphMetrics.horiBearingY;
		OutExtents->width		=  CachedGlyphData.GlyphMetrics.width;
		OutExtents->height		= -CachedGlyphData.GlyphMetrics.height;

		int FontXScale = 0;
		int FontYScale = 0;
		hb_font_get_scale(InFont, &FontXScale, &FontYScale);

		if (FontXScale < 0)
		{
			OutExtents->x_bearing = -OutExtents->x_bearing;
			OutExtents->width = -OutExtents->width;
		}

		if (FontYScale < 0)
		{
			OutExtents->y_bearing = -OutExtents->y_bearing;
			OutExtents->height = -OutExtents->height;
		}

		return true;
	}

	return false;
}

hb_bool_t get_glyph_contour_point(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, unsigned int InPointIndex, hb_position_t* OutX, hb_position_t* OutY, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	TSharedRef<FFreeTypeGlyphCache> GlyphCache = UserDataPtr->FTCacheDirectory->GetGlyphCache(FreeTypeFace, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale);
	FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
	if (GlyphCache->FindOrCache(InGlyphIndex, CachedGlyphData))
	{
		if (InPointIndex < static_cast<unsigned int>(CachedGlyphData.OutlinePoints.Num()))
		{
			*OutX = CachedGlyphData.OutlinePoints[InPointIndex].x;
			*OutY = CachedGlyphData.OutlinePoints[InPointIndex].y;
			return true;
		}
	}

	return false;
}

} // namespace Internal

} // namespace HarfBuzzFontFunctions

#endif // WITH_FREETYPE && WITH_HARFBUZZ

FHarfBuzzFontFactory::FHarfBuzzFontFactory(FFreeTypeCacheDirectory* InFTCacheDirectory)
	: FTCacheDirectory(InFTCacheDirectory)
{
	check(FTCacheDirectory);

#if WITH_HARFBUZZ
	CustomHarfBuzzFuncs = hb_font_funcs_create();

	hb_font_funcs_set_font_h_extents_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_font_h_extents, nullptr, nullptr);
	hb_font_funcs_set_nominal_glyph_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_nominal_glyph, nullptr, nullptr);
#if WITH_HARFBUZZ_V24
	hb_font_funcs_set_nominal_glyphs_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_nominal_glyphs, nullptr, nullptr);
#endif // WITH_HARFBUZZ_V24
	hb_font_funcs_set_glyph_h_advance_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_h_advance, nullptr, nullptr);
#if WITH_HARFBUZZ_V24
	hb_font_funcs_set_glyph_h_advances_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_h_advances, nullptr, nullptr);
#endif // WITH_HARFBUZZ_V24
	hb_font_funcs_set_glyph_v_advance_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_v_advance, nullptr, nullptr);
#if WITH_HARFBUZZ_V24
	hb_font_funcs_set_glyph_v_advances_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_v_advances, nullptr, nullptr);
#endif // WITH_HARFBUZZ_V24
	hb_font_funcs_set_glyph_v_origin_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_v_origin, nullptr, nullptr);
	hb_font_funcs_set_glyph_h_kerning_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_h_kerning, nullptr, nullptr);
	hb_font_funcs_set_glyph_extents_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_extents, nullptr, nullptr);
	hb_font_funcs_set_glyph_contour_point_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_contour_point, nullptr, nullptr);

	hb_font_funcs_make_immutable(CustomHarfBuzzFuncs);
#endif // WITH_HARFBUZZ
}

FHarfBuzzFontFactory::~FHarfBuzzFontFactory()
{
#if WITH_HARFBUZZ
	hb_font_funcs_destroy(CustomHarfBuzzFuncs);
	CustomHarfBuzzFuncs = nullptr;
#endif // WITH_HARFBUZZ
}

#if WITH_HARFBUZZ

hb_font_t* FHarfBuzzFontFactory::CreateFont(const FFreeTypeFace& InFace, const uint32 InGlyphFlags, const FSlateFontInfo& InFontInfo, const float InFontScale) const
{
	hb_font_t* HarfBuzzFont = nullptr;

#if WITH_FREETYPE
	FT_Face FreeTypeFace = InFace.GetFace();
	FreeTypeUtils::ApplySizeAndScale(FreeTypeFace, InFontInfo.Size, InFontScale);

	hb_font_extents_t HarfBuzzFontExtents;
	FMemory::Memzero(HarfBuzzFontExtents);

	// Create a sub-font from the default FreeType implementation so we can override some font functions to provide low-level caching
	{
		hb_font_t* HarfBuzzFTFont = hb_ft_font_create(FreeTypeFace, nullptr);
		hb_ft_font_set_load_flags(HarfBuzzFTFont, InGlyphFlags);

		// The default FreeType implementation doesn't apply the font scale, so we have to do that ourselves (in 16.16 space for maximum precision)
		{
			int HarfBuzzFTFontXScale = 0;
			int HarfBuzzFTFontYScale = 0;
			hb_font_get_scale(HarfBuzzFTFont, &HarfBuzzFTFontXScale, &HarfBuzzFTFontYScale);

			// Cache the font extents
			const bool IsAscentDescentOverridenEnabled = UE::Slate::FontUtils::IsAscentDescentOverrideEnabled(InFontInfo.FontObject);
			HarfBuzzFontExtents.ascender = InFace.GetAscender(IsAscentDescentOverridenEnabled);
			HarfBuzzFontExtents.descender = InFace.GetDescender(IsAscentDescentOverridenEnabled);
			HarfBuzzFontExtents.line_gap = InFace.GetScaledHeight(IsAscentDescentOverridenEnabled) - (HarfBuzzFontExtents.ascender - HarfBuzzFontExtents.descender);
			if (HarfBuzzFTFontYScale < 0)
			{
				HarfBuzzFontExtents.ascender = -HarfBuzzFontExtents.ascender;
				HarfBuzzFontExtents.descender = -HarfBuzzFontExtents.descender;
				HarfBuzzFontExtents.line_gap = -HarfBuzzFontExtents.line_gap;
			}
		}

		HarfBuzzFont = hb_font_create_sub_font(HarfBuzzFTFont);
		
		hb_font_destroy(HarfBuzzFTFont);
	}

	hb_font_set_funcs(HarfBuzzFont, CustomHarfBuzzFuncs, nullptr, nullptr);

	hb_font_set_user_data(
		HarfBuzzFont, 
		&HarfBuzzFontFunctions::UserDataKey, 
		HarfBuzzFontFunctions::CreateUserData(InFontInfo.Size, InFontScale, FTCacheDirectory, HarfBuzzFontExtents),
		&HarfBuzzFontFunctions::DestroyUserData, 
		true
		);
#endif // WITH_FREETYPE

	return HarfBuzzFont;
}

#endif // WITH_HARFBUZZ
