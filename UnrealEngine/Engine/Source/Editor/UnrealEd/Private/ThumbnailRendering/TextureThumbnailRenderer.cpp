// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureLightProfile.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "EngineGlobals.h"
#include "Engine/TextureCube.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCubeArray.h"
#include "Texture2DPreview.h"
#include "Engine/TextureRenderTargetCube.h"

#include "CubemapUnwrapUtils.h"
#include "NormalMapPreview.h"
#include "CanvasTypes.h"

UTextureThumbnailRenderer::UTextureThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTextureThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	UTexture* Texture = Cast<UTexture>(Object);
	UTextureLightProfile* TextureLightProfile = Cast<UTextureLightProfile>(Object);

	if (TextureLightProfile)
	{
		// otherwise a 1D texture would result in a very boring thumbnail
		OutWidth = 192;
		OutHeight = 192;
		return;
	}

	if (Texture != nullptr)
	{
		OutWidth = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceWidth());
		OutHeight = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceHeight());
	}
	else
	{
		OutWidth = OutHeight = 0;
	}
}

void UTextureThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UTexture* Texture = Cast<UTexture>(Object);
	if (Texture != nullptr && Texture->GetResource() != nullptr)
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);

		// Take the alpha channel into account for textures that have one.
		// This provides a much better preview than just showing RGB,
		// Because the RGB content in areas with an alpha of 0 is often garbage that will not be seen in normal conditions.
		// Non-UI textures often have uncorrelated data in the alpha channel (like a skin mask, specular power, etc) so we only preview UI textures this way.
		const bool bUseTranslucentBlend = Texture2D && Texture2D->HasAlphaChannel() && ((Texture2D->LODGroup == TEXTUREGROUP_UI) || (Texture2D->LODGroup == TEXTUREGROUP_Pixels2D));

		UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
		UTexture2DArray* Texture2DArray = Cast<UTexture2DArray>(Texture);
		UTextureCubeArray* TextureCubeArray = Cast<UTextureCubeArray>(Texture);
		UTextureRenderTargetCube* RTTextureCube = Cast<UTextureRenderTargetCube>(Texture);
		UTextureLightProfile* TextureLightProfile = Cast<UTextureLightProfile>(Texture);
		const bool bIsVirtualTexture = Texture->IsCurrentlyVirtualTextured();

		TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;

		if(TextureCube || TextureCubeArray || RTTextureCube)
		{
			// is released by the render thread when it was rendered
			BatchedElementParameters = new FMipLevelBatchedElementParameters((float)0, (float)-1, TextureCubeArray != nullptr, FMatrix44f::Identity, true, false);
			
			// If the thumbnail is square then make it 2:1 for cubes.
			if(Width == Height)
			{
				Height = Width / 2;
				Y += Height / 2;
			}
		}
		else if (Texture2DArray) 
		{
			bool bIsNormalMap = Texture2DArray->IsNormalMap();
			bool bIsSingleChannel = true;
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters((float)0, (float)0, (float)-1, bIsNormalMap, bIsSingleChannel, false, false, true);
		}
		else if (TextureLightProfile)
		{
			BatchedElementParameters = new FIESLightProfileBatchedElementParameters(TextureLightProfile->Brightness);
		}
		else if (Texture2D && Texture2D->IsNormalMap())
		{
			BatchedElementParameters = new FNormalMapBatchedElementParameters();
		}

		if (bUseTranslucentBlend)
		{
			// If using alpha, draw a checkerboard underneath first.
			const int32 CheckerDensity = 8;
			auto Checker = UThumbnailManager::Get().CheckerboardTexture;
			Canvas->DrawTile(
				0.0f, 0.0f, Width, Height,							// Dimensions
				0.0f, 0.0f, CheckerDensity, CheckerDensity,			// UVs
				FLinearColor::White, Checker->GetResource());			// Tint & Texture
		}

		// Use A canvas tile item to draw
		FCanvasTileItem CanvasTile( FVector2D( X, Y ), Texture->GetResource(), FVector2D( Width,Height ), FLinearColor::White );
		CanvasTile.BlendMode = bUseTranslucentBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
		CanvasTile.BatchedElementParameters = BatchedElementParameters;
		if (bIsVirtualTexture && Texture->Source.GetNumBlocks() > 1)
		{
			// Adjust UVs to display entire UDIM range, acounting for UE inverted V-axis
			// We're not actually rendering a VT here, but the editor-only texture we're using is still using the UDIM tile layout
			// So we use inverted Y-axis, but then normalize back to [0,1)
			const FIntPoint BlockSize = Texture->Source.GetSizeInBlocks();
			const float RcpBlockSizeY = 1.0f / (float)BlockSize.Y;
			CanvasTile.UV0.Y = (1.0f - (float)BlockSize.Y) * RcpBlockSizeY;
			CanvasTile.UV1.Y = RcpBlockSizeY;
		}
		CanvasTile.Draw( Canvas );

		if (TextureLightProfile)
		{
			float Brightness = TextureLightProfile->Brightness;

			// Brightness in Lumens
			FText BrightnessText = FText::AsNumber( Brightness );
			FCanvasTextItem TextItem( FVector2D( 5.0f, 5.0f ), BrightnessText, GEngine->GetLargeFont(), FLinearColor::White );
			TextItem.EnableShadow(FLinearColor::Black);
			TextItem.Scale = FVector2D(Width / 128.0f, Height / 128.0f);
			TextItem.Draw(Canvas);
		}

		if (bIsVirtualTexture)
		{
			auto VTChars = TEXT("VT");
			int32 VTWidth = 0;
			int32 VTHeight = 0;
			StringSize(GEngine->GetLargeFont(), VTWidth, VTHeight, VTChars);
			float PaddingX = Width / 128.0f;
			float PaddingY = Height / 128.0f;
			float ScaleX = Width / 64.0f; //Text is 1/64'th of the size of the thumbnails
			float ScaleY = Height / 64.0f;
			// VT overlay
			FCanvasTextItem TextItem(FVector2D(Width - PaddingX - VTWidth * ScaleX, Height - PaddingY - VTHeight * ScaleY), FText::FromString(VTChars), GEngine->GetLargeFont(), FLinearColor::White);
			TextItem.EnableShadow(FLinearColor::Black);
			TextItem.Scale = FVector2D(ScaleX, ScaleY);
			TextItem.Draw(Canvas);
		}
	}
}
