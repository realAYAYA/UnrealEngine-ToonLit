// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Fonts/FontCache.h"
#include "Framework/Text/TextLayout.h"
#include "Internationalization/Text.h"
#include "Templates/UniquePtr.h"
#include "Text3DPrivate.h"

class ITextLayoutMarshaller;
class UFont;

/** Contains a single line of text with sufficient information to fetch and transform each character. */
struct FShapedGlyphLine
{
	/** The corresponding shaped glyph for each character in this line of text. */
	TArray<FShapedGlyphEntry> GlyphsToRender;

	/** Stored result of CalculateWidth. */
	float Width = 0.0f;

	/** Get's the offset from the previous character, accounting for kerning and word spacing. */
	float GetAdvance(const int32 Index, const float Kerning, const float WordSpacing) const
	{
		check(Index >= 0 && Index < GlyphsToRender.Num());

		const FShapedGlyphEntry& Glyph = GlyphsToRender[Index];
		float Advance = Glyph.XOffset + Glyph.XAdvance;

		if (Index < GlyphsToRender.Num() - 1)
		{
			// @note: as per FSlateElementBatcher::BuildShapedTextSequence, per Glyph Kerning isn't used
			Advance += Kerning;

			if (!Glyph.bIsVisible)
			{
				Advance += WordSpacing;
			}
		}

		return Advance;
	}

	/** Calculates the total width of this line. */
	void CalculateWidth(const float Kerning, const float WordSpacing)
	{
		Width = 0.0f;
		for (int32 Index = 0; Index < GlyphsToRender.Num(); Index++)
		{
			Width += GetAdvance(Index, Kerning, WordSpacing);
		}
	}
};

/** An implementation of FTextLayout, which discards most Slate widget specific functionality. */
class FText3DLayout : public FTextLayout
{
public:
	/** Optionally provide a custom TextBlock style. */
	FText3DLayout(const FTextBlockStyle& InStyle = FTextBlockStyle::GetDefault());

protected:
	/** Parameters relevant to text layout. */
	FTextBlockStyle TextStyle;

	/** Required but unused override. */
	virtual TSharedRef<IRun> CreateDefaultTextRun(
		const TSharedRef<FString>& NewText,
		const FTextRange& NewRange) const override;
};

/** A singleton that handles shaping operations and writes the result to a provided TextLayout. */
class FTextShaper final
{
public:
	/** Returns TextShaper singleton. */
	static TSharedPtr<FTextShaper> Get();

	/**
	 * Arranges the provided text to match the requested layout, accounting for scale, offsets etc.
	 * Analogous to FSlateFontCache::ShapeBidirectionalText.
	 */
	void ShapeBidirectionalText(
		const FTextBlockStyle& Style,
		const FString& Text,
		const TSharedPtr<FTextLayout>& TextLayout,
		const TSharedPtr<ITextLayoutMarshaller>& TextMarshaller,
		TArray<FShapedGlyphLine>& OutShapedLines);

private:
	struct FPrivateToken { explicit FPrivateToken() = default; };

public:
	FTextShaper(FPrivateToken) { }

private:
	friend class UText3DComponent;

	FTextShaper(const FTextShaper&) = delete;
	FTextShaper& operator=(const FTextShaper&) = delete;
};
