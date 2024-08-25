// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Font/AvaFontObject.h"
#include "UObject/StrongObjectPtr.h"

class UAvaFontObject;
struct FAvaFont;

class FAvaFontView
{
	friend class SAvaFontField;

public:
	static TSharedRef<FAvaFontView> Make(const FAvaFont& InFont);

	UAvaFontObject* GetFontObject() const { return AvaFontObject.Get(); }
	UFont* GetFont() const;

	bool IsFallbackFont() const { return bIsFallbackFont; }
	bool IsFavorite() const { return bIsFavorite; }

	bool HasValidFont() const;
	bool IsMonospaced() const;
	bool IsBold() const;
	bool IsItalic() const;
	FName GetFontName() const;
	FString GetFontNameAsString() const;
	FText GetFontNameAsText() const;
	EAvaFontSource GetFontSource() const;
	const FString& GetMissingFontName() const { return MissingFontName; }

	bool operator==(const FAvaFontView& Other) const
	{
		const UAvaFontObject* MyFontObject = GetFontObject();
		const UAvaFontObject* OtherFontObject = Other.GetFontObject();

		if (MyFontObject && OtherFontObject && MyFontObject->GetFont() != OtherFontObject->GetFont())
		{
			return false;
		}

		return (GetFontName() == Other.GetFontName());
	}

	bool operator!=(const FAvaFontView& Other) const
	{
		return !(*this == Other);
	}

	/** Alphabetical ordering */
	bool operator<(const FAvaFontView& Other) const
	{
		return GetFontName().LexicalLess(Other.GetFontName());
	}

	bool IsAvaFontValid() const;

protected:
	FAvaFontView()
		: bIsFavorite(false)
		, bIsFallbackFont(false)
		, AvaFontObject(nullptr)
	{
	}

	FAvaFontView(const FAvaFont& InFont);

	void SetFontObject(UAvaFontObject* InFontObject);
	void SetFavorite(bool bInIsFavorite);

	bool bIsFavorite;
	bool bIsFallbackFont;
	TStrongObjectPtr<UAvaFontObject> AvaFontObject;
	FString MissingFontName;
};
