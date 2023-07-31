// Copyright Epic Games, Inc. All Rights Reserved.

#include "Fonts/FontTypes.h"


FSlateFontAtlas::FSlateFontAtlas(uint32 InWidth, uint32 InHeight, const bool InIsGrayscale)
	: FSlateTextureAtlas(InWidth, InHeight, InIsGrayscale ? 1 : 4, ESlateTextureAtlasPaddingStyle::PadWithZero, true)
{
}

FSlateFontAtlas::~FSlateFontAtlas()
{
}	

bool FSlateFontAtlas::IsGrayscale() const
{
	return BytesPerPixel == 1;
}

const FAtlasedTextureSlot* FSlateFontAtlas::AddCharacter( const FCharacterRenderData& RenderData )
{
	check(RenderData.bIsGrayscale == IsGrayscale());
	return AddTexture( RenderData.SizeX, RenderData.SizeY, RenderData.RawPixels );
}

void FSlateFontAtlas::Flush()
{
	// Empty the atlas
	EmptyAtlasData();

	// Recreate the data
	InitAtlasData();

	bNeedsUpdate = true;
	ConditionalUpdateTexture();
}
