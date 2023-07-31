// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaPlateEditorMediaDetails.h"
#include "IDetailsView.h"
#include "MediaPlateComponent.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMediaPlateEditorMediaDetails"

/* SMediaPlateEditorMediaDetails interface
 *****************************************************************************/

void SMediaPlateEditorMediaDetails::Construct(const FArguments& InArgs,
	UMediaPlateComponent& InMediaPlate)
{
	MediaPlate = &InMediaPlate;

	ChildSlot
		[
			SNew(SScrollBox)

			// Add details.
			+ SScrollBox::Slot()
				[
					SNew(SHorizontalBox)

					// Left side.
					+ SHorizontalBox::Slot()
						[
							SNew(SVerticalBox)

							// Resolution.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(ResolutionText, STextBlock)
								]

							// Frame rate.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(FrameRateText, STextBlock)
								]

							// Resource size.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(ResourceSizeText, STextBlock)
								]

							// Method.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(MethodText, STextBlock)
								]
						]

					// Right side.
					+ SHorizontalBox::Slot()
						[
							SNew(SVerticalBox)

							// Format.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(FormatText, STextBlock)
								]

							// LOD bias.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(LODBiasText, STextBlock)
								]

							// Num mips.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(NumMipsText, STextBlock)
								]

							// Num tiles.
							+ SVerticalBox::Slot()
								.AutoHeight()
								.VAlign(VAlign_Center)
								.Padding(4.0f)
								[
									SAssignNew(NumTilesText, STextBlock)
								]
						]
				]

		];

	UpdateDetails();
}


void SMediaPlateEditorMediaDetails::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Call parent.
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	UpdateDetails();
}

void SMediaPlateEditorMediaDetails::UpdateDetails()
{
	FString Format;
	float FrameRate = 0.0f;
	int32 LODBias = 0;
	FText Method;
	int32 NumMips = 0;
	int32 NumTotalTiles = 0;
	int64 ResourceSize = 0;
	int32 SurfaceWidth = 0;
	int32 SurfaceHeight = 0;

	if (MediaPlate.IsValid())
	{
		// Get player info.
		UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
		if (MediaPlayer != nullptr)
		{
			FrameRate = MediaPlayer->GetVideoTrackFrameRate(INDEX_NONE, INDEX_NONE);
			Format = MediaPlayer->GetVideoTrackType(INDEX_NONE, INDEX_NONE);
			FIntPoint NumTiles(EForceInit::ForceInitToZero);
			MediaPlayer->GetMediaInfo<FIntPoint>(NumTiles, UMediaPlayer::MediaInfoNameSourceNumTiles.Resolve());
			NumTotalTiles = NumTiles.X * NumTiles.Y;
		}

		// Get texture info,
		UMediaTexture* MediaTexture = MediaPlate->GetMediaTexture();
		if (MediaTexture != nullptr)
		{
			LODBias = MediaTexture->GetCachedLODBias();
			Method = MediaTexture->IsCurrentlyVirtualTextured() ?
				LOCTEXT("MethodVirtualStreamed", "Virtual Streamed")
				: (!MediaTexture->IsStreamable() ? LOCTEXT("QuickInfo_MethodNotStreamed", "Not Streamed")
					: LOCTEXT("MethodStreamed", "Streamed"));
			NumMips = MediaTexture->GetTextureNumMips();
			ResourceSize = (MediaTexture->GetResourceSizeBytes(EResourceSizeMode::Exclusive) + 512) / 1024;
			SurfaceWidth = MediaTexture->GetSurfaceWidth();
			SurfaceHeight = MediaTexture->GetSurfaceHeight();
		}
	}

	// Update text.
	FormatText->SetText(FText::Format(LOCTEXT("Format", "Format: {0}"),
		FText::FromString(Format)));
	FrameRateText->SetText(FText::Format(LOCTEXT("FrameRate", "Frame Rate: {0}"), 
		FText::AsNumber(FrameRate)));
	LODBiasText->SetText(FText::Format(LOCTEXT("LODBias", "Combined LOD Bias: {0}"),
		FText::AsNumber(LODBias)));
	MethodText->SetText(FText::Format(LOCTEXT("Method", "Method: {0}"), Method));
	NumMipsText->SetText(FText::Format(LOCTEXT("NumberOfMips", "Number Of Mips: {0}"),
		FText::AsNumber(NumMips)));
	NumTilesText->SetText(FText::Format(LOCTEXT("NumberOfTiles", "Number Of Tiles: {0}"),
		FText::AsNumber(NumTotalTiles)));
	ResolutionText->SetText(FText::Format(LOCTEXT("Resolution", "Resolution: {0}x{1}"),
		FText::AsNumber(SurfaceWidth), FText::AsNumber(SurfaceHeight)));
	ResourceSizeText->SetText(FText::Format(LOCTEXT("ResourceSize", "Resource Size: {0} KB"),
		FText::AsNumber(ResourceSize)));
}

#undef LOCTEXT_NAMESPACE
