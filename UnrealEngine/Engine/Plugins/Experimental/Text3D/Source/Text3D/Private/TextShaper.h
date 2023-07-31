// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Text3DPrivate.h"
#include "Fonts/FontCache.h"

#include "Containers/Array.h"
#include "Templates/UniquePtr.h"
#include "Internationalization/Text.h"


struct FShapedGlyphLine
{
	TArray<FShapedGlyphEntry> GlyphsToRender;
	float Width;

	float GetAdvanced(const int32 Index, const float Kerning, const float WordSpacing) const
	{
		check(Index >= 0 && Index < GlyphsToRender.Num());

		const FShapedGlyphEntry& Glyph = GlyphsToRender[Index];
		float Advance = Glyph.XOffset + Glyph.XAdvance;

		if (Index < GlyphsToRender.Num() - 1)
		{
			Advance += Glyph.Kerning + Kerning;

			if (!Glyph.bIsVisible)
			{
				Advance += WordSpacing;
			}
		}

		return Advance;
	}

	void CalculateWidth(const float Kerning, const float WordSpacing)
	{
		Width = 0.0f;
		for (int32 Index = 0; Index < GlyphsToRender.Num(); Index++)
		{
			Width += GetAdvanced(Index, Kerning, WordSpacing);
		}
	}

	FShapedGlyphLine()
	{
		Width = 0.0f;
	}
};

class FTextShaper final
{
public:
	static FTextShaper* Get()							{ return Instance; }

	void ShapeBidirectionalText(const FT_Face Face, const FString& Text, TArray<FShapedGlyphLine>& OutShapedLines);

	static void Initialize();
	static void Cleanup();

private:	
	FTextShaper();
	FTextShaper(const FTextShaper&) = delete;
	FTextShaper& operator=(const FTextShaper&) = delete;

	void PerformKerningTextShaping(const FT_Face Face, const TCHAR* Text, const int32 StartIndex, const int32 EndIndex, TArray<FShapedGlyphLine>& OutShapedLines);
	void PerformHarfBuzzTextShaping(const FT_Face Face, const TCHAR* Text, const int32 StartIndex, const int32 EndIndex, TArray<FShapedGlyphLine>& OutShapedLines);
	bool InsertSubstituteGlyphs(const FT_Face Face, const TCHAR* Text, const int32 Index, TArray<FShapedGlyphLine>& OutShapedLines);

	/** Unicode BiDi text detection */
	TUniquePtr<TextBiDi::ITextBiDi> TextBiDiDetection;

	static FTextShaper* Instance;
};
