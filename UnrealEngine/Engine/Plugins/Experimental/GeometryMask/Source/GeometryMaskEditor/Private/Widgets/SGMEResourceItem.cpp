// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGMEResourceItem.h"

#include "Engine/CanvasRenderTarget2D.h"
#include "SlateOptMacros.h"
#include "ViewModels/GMEResourceItemViewModel.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SGMEResourceItem::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& InOwnerTableView,
	const TSharedRef<FGMEResourceItemViewModel>& InViewModel)
{
	ViewModel = InViewModel;

	TextureBrush = MakeShared<FSlateBrush>();
	TextureBrush->SetResourceObject(const_cast<UCanvasRenderTarget2D*>(InViewModel->GetResourceTexture()));
	TextureBrush->ImageSize = FVector2D(InViewModel->GetResourceTexture()->SizeX, InViewModel->GetResourceTexture()->SizeY);

	ImageWidget = SNew(SImage)
					.Image(TextureBrush.IsValid()
						? TextureBrush.Get()
						: FAppStyle::GetBrush("WhiteTexture"));
	
	SGMEImageItem::Construct(
		SGMEImageItem::FArguments()
			.Label(this, &SGMEResourceItem::GetLabel)
		, InOwnerTableView);
}

FText SGMEResourceItem::GetLabel() const
{
	if (ViewModel.IsValid())
	{
		return ViewModel->GetResourceInfo();
	}

	return FText::GetEmpty();
}

FOptionalSize SGMEResourceItem::GetAspectRatio()
{
	float AspectRatio = 16.0f / 9.0f;
	if (const UTexture* ResourceTexture = ViewModel->GetResourceTexture())
	{
		AspectRatio = ResourceTexture->GetSurfaceWidth() / ResourceTexture->GetSurfaceHeight();
	}

	return AspectRatio;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
