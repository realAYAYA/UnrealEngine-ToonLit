// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWarningIcon.h"

#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"

namespace UE::MultiUserClient
{
	void SWarningIcon::Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor"))
				.DesiredSizeOverride(FVector2D(16,16))
			]
		];
	}
}

