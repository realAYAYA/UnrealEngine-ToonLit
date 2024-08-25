// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaFontView.h"
#include "Font/AvaFont.h"

TSharedRef<FAvaFontView> FAvaFontView::Make(const FAvaFont& InFont)
{
	return MakeShareable(new FAvaFontView(InFont));
}

bool FAvaFontView::IsAvaFontValid() const
{
	return AvaFontObject.IsValid();
}

FAvaFontView::FAvaFontView(const FAvaFont& InFont)
{
	SetFavorite(InFont.IsFavorite());
	SetFontObject(InFont.GetFontObject());

	bIsFallbackFont = InFont.IsFallbackFont();
	if (bIsFallbackFont)
	{
		MissingFontName = InFont.GetMissingFontName();
	}
}

UFont* FAvaFontView::GetFont() const
{
	if (!IsAvaFontValid())
	{
		return nullptr;
	}

	return AvaFontObject->GetFont();
}

bool FAvaFontView::HasValidFont() const
{
	if (!IsAvaFontValid())
	{
		return false;
	}

	return AvaFontObject->HasValidFont();
}

bool FAvaFontView::IsMonospaced() const
{
	if (!IsAvaFontValid())
	{
		return false;
	}

	return AvaFontObject->IsMonospaced();
}

bool FAvaFontView::IsBold() const
{
	if (!IsAvaFontValid())
	{
		return false;
	}

	return AvaFontObject->IsBold();
}

bool FAvaFontView::IsItalic() const
{
	if (!IsAvaFontValid())
	{
		return false;
	}

	return AvaFontObject->IsItalic();
}

FName FAvaFontView::GetFontName() const
{
	return FName(GetFontNameAsString());
}

FString FAvaFontView::GetFontNameAsString() const
{
	if (!IsAvaFontValid())
	{
		return TEXT("");
	}

	return AvaFontObject->GetFontName();
}

FText FAvaFontView::GetFontNameAsText() const
{
	return FText::FromString(GetFontNameAsString());
}

EAvaFontSource FAvaFontView::GetFontSource() const
{
	if (IsAvaFontValid())
	{
		return AvaFontObject->GetSource();
	}

	return EAvaFontSource::Invalid;
}

void FAvaFontView::SetFavorite(bool bInIsFavorite)
{
	bIsFavorite = bInIsFavorite;
}

void FAvaFontView::SetFontObject(UAvaFontObject* InFontObject)
{
	if (!InFontObject)
	{
		return;
	}

	AvaFontObject = TStrongObjectPtr<UAvaFontObject>(InFontObject);
}
