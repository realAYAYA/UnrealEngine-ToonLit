// Copyright Epic Games, Inc. All Rights Reserved.


#include "TextShaper.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Fonts/FontCache.h"
#include "Internationalization/Text.h"

THIRD_PARTY_INCLUDES_START
#include "hb.h"
#include "hb-ft.h"

#if WITH_EDITOR
extern "C"
{
void* HarfBuzzMalloc(size_t InSizeBytes)
{
	LLM_SCOPE(ELLMTag::UI);
	return FMemory::Malloc(InSizeBytes);
}

void* HarfBuzzCalloc(size_t InNumItems, size_t InItemSizeBytes)
{
	LLM_SCOPE(ELLMTag::UI);
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
	LLM_SCOPE(ELLMTag::UI);
	return FMemory::Realloc(InPtr, InSizeBytes);
}

void HarfBuzzFree(void* InPtr)
{
	FMemory::Free(InPtr);
}

} // extern "C"
#endif // WITH_EDITOR
THIRD_PARTY_INCLUDES_END

FTextShaper* FTextShaper::Instance = nullptr;

void FTextShaper::Initialize()
{
	if (Instance)
		return;

	Instance = new FTextShaper();
}

void FTextShaper::Cleanup()
{
	delete Instance;
	Instance = nullptr;
}

FTextShaper::FTextShaper()
{
	TextBiDiDetection = TextBiDi::CreateTextBiDi();
}

void FTextShaper::ShapeBidirectionalText(const FT_Face Face, const FString& Text, TArray<FShapedGlyphLine>& OutShapedLines)
{
	TextBiDi::ETextDirection Direction = TextBiDiDetection->ComputeBaseDirection(Text);
	check(Direction != TextBiDi::ETextDirection::Mixed);
	check(Face);

	OutShapedLines.AddDefaulted();

	TArray<TextBiDi::FTextDirectionInfo> TextDirectionInfos;
	TextBiDiDetection->ComputeTextDirection(Text, Direction, TextDirectionInfos);
	for (const TextBiDi::FTextDirectionInfo& TextDirectionInfo : TextDirectionInfos)
	{
		if (TextDirectionInfo.Length == 0)
		{
			continue;
		}

		if (TextDirectionInfo.TextDirection == TextBiDi::ETextDirection::RightToLeft)
		{
			PerformHarfBuzzTextShaping(Face, *Text, TextDirectionInfo.StartIndex, TextDirectionInfo.StartIndex + TextDirectionInfo.Length, OutShapedLines);
		}
		else
		{
			PerformKerningTextShaping(Face, *Text, TextDirectionInfo.StartIndex, TextDirectionInfo.StartIndex + TextDirectionInfo.Length, OutShapedLines);
		}
	}

	if (Direction == TextBiDi::ETextDirection::RightToLeft)
	{
		Algo::Reverse(OutShapedLines);
	}
}

void FTextShaper::PerformKerningTextShaping(const FT_Face Face, const TCHAR* Text, const int32 StartIndex, const int32 EndIndex, TArray<FShapedGlyphLine>& OutShapedLines)
{
	const bool bHasKerning = FT_HAS_KERNING(Face) != 0;
	for (int32 Index = StartIndex; Index < EndIndex; Index++)
	{
		if (InsertSubstituteGlyphs(Face, Text, Index, OutShapedLines))
		{
			continue;
		}

		const TCHAR CurrentChar = Text[Index];
		const bool bIsZeroWidthSpace = CurrentChar == TEXT('\u200B');
		bool bIsWhitespace = bIsZeroWidthSpace || FText::IsWhitespace(CurrentChar);

		FT_Load_Char(Face, CurrentChar, FT_LOAD_DEFAULT);
		uint32 GlyphIndex = FT_Get_Char_Index(Face, CurrentChar);
		if (GlyphIndex == 0)	// Get Space instead of invalid character
		{
			GlyphIndex = FT_Get_Char_Index(Face, ' ');
			bIsWhitespace = true;
		}
		

		int16 XAdvance = 0;
		if (!bIsZeroWidthSpace)
		{
			FT_Fixed AdvanceData = 0;
			if (FT_Get_Advance(Face, GlyphIndex, 0, &AdvanceData) == 0)
			{
				XAdvance = ((AdvanceData + (1 << 9)) >> 10) * FontInverseScale;
			}
		}

		const int32 CurrentGlyphEntryIndex = OutShapedLines.Last().GlyphsToRender.AddDefaulted();
		FShapedGlyphEntry& ShapedGlyphEntry = OutShapedLines.Last().GlyphsToRender[CurrentGlyphEntryIndex];
		ShapedGlyphEntry.GlyphIndex = GlyphIndex;
		ShapedGlyphEntry.SourceIndex = Index;
		ShapedGlyphEntry.XAdvance = bIsZeroWidthSpace ? 0 : XAdvance;
		ShapedGlyphEntry.YAdvance = 0;
		ShapedGlyphEntry.XOffset = 0;
		ShapedGlyphEntry.YOffset = 0;
		ShapedGlyphEntry.Kerning = 0;
		ShapedGlyphEntry.NumCharactersInGlyph = 1;
		ShapedGlyphEntry.NumGraphemeClustersInGlyph = 1;
		ShapedGlyphEntry.TextDirection = TextBiDi::ETextDirection::LeftToRight;
		ShapedGlyphEntry.bIsVisible = !bIsWhitespace;

		// Apply the kerning against the previous entry
		if (CurrentGlyphEntryIndex > 0 && bHasKerning && ShapedGlyphEntry.bIsVisible)
		{
			FShapedGlyphEntry& PreviousShapedGlyphEntry = OutShapedLines.Last().GlyphsToRender[CurrentGlyphEntryIndex - 1];

			FT_Vector KerningVector;
			if (FT_Get_Kerning(Face, PreviousShapedGlyphEntry.GlyphIndex, ShapedGlyphEntry.GlyphIndex, FT_KERNING_DEFAULT, &KerningVector) == 0)
			{
				const int8 Kerning = KerningVector.x * FontInverseScale;
				PreviousShapedGlyphEntry.XAdvance += Kerning;
				PreviousShapedGlyphEntry.Kerning = Kerning;
			}
		}
	}
}

void FTextShaper::PerformHarfBuzzTextShaping(const FT_Face Face, const TCHAR* Text, int32 StartIndex, int32 EndIndex, TArray<FShapedGlyphLine>& OutShapedLines)
{
	hb_font_t* HarfBuzzFTFont = hb_ft_font_create(Face, nullptr);
	hb_buffer_t* HarfBuzzBuffer = hb_buffer_create();

	hb_unicode_funcs_t* HarfBuzzUnicodeFuncs = hb_unicode_funcs_get_default();


	TArray<TTuple<int32, hb_script_t>> HarfBuzzSegments;
	for (int32 Index = StartIndex; Index < EndIndex; Index++)
	{
		const hb_script_t CharHarfBuzzScript = hb_unicode_script(HarfBuzzUnicodeFuncs, Text[Index]);
		if (HarfBuzzSegments.Num() > 0 && 
			(CharHarfBuzzScript == HB_SCRIPT_COMMON || CharHarfBuzzScript == HB_SCRIPT_INHERITED || CharHarfBuzzScript == HB_SCRIPT_UNKNOWN))
		{
			continue;
		}

		if (HarfBuzzSegments.Num() == 0 || HarfBuzzSegments.Last().Value != CharHarfBuzzScript)
		{
			HarfBuzzSegments.Add(TTuple<int32, hb_script_t>(Index, CharHarfBuzzScript));
		}
	}

	for (int32 Index = 0; Index < HarfBuzzSegments.Num(); Index++)
	{
		const TTuple<int32, hb_script_t>& HarfBuzzSegment = HarfBuzzSegments[Index];

		int32 SegmentLength = EndIndex - HarfBuzzSegment.Key;
		if (Index < HarfBuzzSegments.Num() - 1)
	{
			SegmentLength = HarfBuzzSegments[Index + 1].Key - HarfBuzzSegment.Key;
	}

		hb_buffer_add_utf16(HarfBuzzBuffer, (uint16_t*) Text, -1, HarfBuzzSegment.Key, SegmentLength);
		hb_buffer_set_script(HarfBuzzBuffer, HarfBuzzSegment.Value);
		hb_buffer_set_direction(HarfBuzzBuffer, HB_DIRECTION_RTL);
	}

	bool bHasKerning = FT_HAS_KERNING(Face) != 0;
	const hb_feature_t HarfBuzzFeatures[] = { { HB_TAG('k','e','r','n'), bHasKerning, 0, uint32(-1) } };
	const int32 HarfBuzzFeaturesCount = UE_ARRAY_COUNT(HarfBuzzFeatures);
	hb_shape(HarfBuzzFTFont, HarfBuzzBuffer, HarfBuzzFeatures, HarfBuzzFeaturesCount);

	uint32 glyph_count = 0;
	hb_glyph_info_t * HarfBuzzGlyphInfo = hb_buffer_get_glyph_infos(HarfBuzzBuffer, &glyph_count);
	hb_glyph_position_t * HarfBuzzGlyphPos = hb_buffer_get_glyph_positions(HarfBuzzBuffer, &glyph_count);

	for (uint32 Index = 0; Index < glyph_count; Index++)
	{
		const int32 CurrentCharIndex = static_cast<int32>(HarfBuzzGlyphInfo[Index].cluster);
		if (InsertSubstituteGlyphs(Face, Text, CurrentCharIndex, OutShapedLines))
		{
			continue;
		}

		const int32 CurrentGlyphEntryIndex = OutShapedLines.Last().GlyphsToRender.AddDefaulted();
		FShapedGlyphEntry& ShapedGlyphEntry = OutShapedLines.Last().GlyphsToRender[CurrentGlyphEntryIndex];
		ShapedGlyphEntry.GlyphIndex = HarfBuzzGlyphInfo[Index].codepoint;
		ShapedGlyphEntry.SourceIndex = Index;
		ShapedGlyphEntry.XAdvance = HarfBuzzGlyphPos[Index].x_advance * FontInverseScale;
		ShapedGlyphEntry.YAdvance = HarfBuzzGlyphPos[Index].y_advance * FontInverseScale;
		ShapedGlyphEntry.XOffset = HarfBuzzGlyphPos[Index].x_offset * FontInverseScale;
		ShapedGlyphEntry.YOffset = HarfBuzzGlyphPos[Index].y_offset * FontInverseScale;
		ShapedGlyphEntry.Kerning = 0;
		ShapedGlyphEntry.NumCharactersInGlyph = 1;
		ShapedGlyphEntry.NumGraphemeClustersInGlyph = 1;
		ShapedGlyphEntry.TextDirection = TextBiDi::ETextDirection::LeftToRight;

		const TCHAR CurrentChar = Text[CurrentCharIndex];
		const bool bIsZeroWidthSpace = CurrentChar == TEXT('\u200B');
		ShapedGlyphEntry.bIsVisible = !(bIsZeroWidthSpace || FText::IsWhitespace(CurrentChar));
	}

	hb_buffer_destroy(HarfBuzzBuffer);
	hb_font_destroy(HarfBuzzFTFont);
}

bool FTextShaper::InsertSubstituteGlyphs(const FT_Face Face, const TCHAR* Text, const int32 Index, TArray<FShapedGlyphLine>& OutShapedLines)
{
	TCHAR Char = Text[Index];
	if (TextBiDi::IsControlCharacter(Char))
	{
		// We insert a stub entry for control characters to avoid them being drawn as a visual glyph with size
		const int32 CurrentGlyphEntryIndex = OutShapedLines.Last().GlyphsToRender.AddDefaulted();
		FShapedGlyphEntry& ShapedGlyphEntry = OutShapedLines.Last().GlyphsToRender[CurrentGlyphEntryIndex];
		//ShapedGlyphEntry.FontFaceData = InShapedGlyphFaceData;
		ShapedGlyphEntry.GlyphIndex = 0;
		ShapedGlyphEntry.SourceIndex = Index;
		ShapedGlyphEntry.XAdvance = 0;
		ShapedGlyphEntry.YAdvance = 0;
		ShapedGlyphEntry.XOffset = 0;
		ShapedGlyphEntry.YOffset = 0;
		ShapedGlyphEntry.Kerning = 0;
		ShapedGlyphEntry.NumCharactersInGlyph = 1;
		ShapedGlyphEntry.NumGraphemeClustersInGlyph = 1;
		ShapedGlyphEntry.TextDirection = TextBiDi::ETextDirection::LeftToRight;
		ShapedGlyphEntry.bIsVisible = false;
		return true;
	}

	if (Char == TEXT('\r'))
	{
		return true;
	}

	if (Char == TEXT('\n'))
	{
		OutShapedLines.AddDefaulted();
		return true;
	}

	if (Char == TEXT('\t'))
	{
		int16 SpaceXAdvance = 0;
		uint32 SpaceGlyphIndex = FT_Get_Char_Index(Face, TEXT(' '));

		FT_Fixed AdvanceData = 0;
		if (FT_Get_Advance(Face, SpaceGlyphIndex, 0, &AdvanceData) == 0)
		{
			SpaceXAdvance = ((AdvanceData + (1 << 9)) >> 10) * FontInverseScale;
		}

		// We insert a spacer glyph with (up-to) the width of 4 space glyphs in-place of a tab character
		const int32 NumSpacesToInsert = 4 - (OutShapedLines.Last().GlyphsToRender.Num() % 4);
		if (NumSpacesToInsert > 0)
		{
			const int32 CurrentGlyphEntryIndex = OutShapedLines.Last().GlyphsToRender.AddDefaulted();
			FShapedGlyphEntry& ShapedGlyphEntry = OutShapedLines.Last().GlyphsToRender[CurrentGlyphEntryIndex];
			ShapedGlyphEntry.GlyphIndex = SpaceGlyphIndex;
			ShapedGlyphEntry.SourceIndex = Index;
			ShapedGlyphEntry.XAdvance = SpaceXAdvance * NumSpacesToInsert;
			ShapedGlyphEntry.YAdvance = 0;
			ShapedGlyphEntry.XOffset = 0;
			ShapedGlyphEntry.YOffset = 0;
			ShapedGlyphEntry.Kerning = 0;
			ShapedGlyphEntry.NumCharactersInGlyph = 1;
			ShapedGlyphEntry.NumGraphemeClustersInGlyph = 1;
			ShapedGlyphEntry.TextDirection = TextBiDi::ETextDirection::LeftToRight;
			ShapedGlyphEntry.bIsVisible = false;
		}

		return true;
	}

	return false;
}
