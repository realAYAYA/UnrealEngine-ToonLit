// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/AssetDataToken.h" 

FOnMessageTokenActivated FAssetDataToken::DefaultMessageTokenActivated;
FAssetDataToken::FOnGetDisplayName FAssetDataToken::DefaultGetAssetDisplayName;

TSharedRef<FAssetDataToken> FAssetDataToken::Create(const FAssetData& InAssetData, const FText& InLabelOverride)
{
    return MakeShared<FAssetDataToken>(FPrivateToken{}, InAssetData, InLabelOverride);
}

FAssetDataToken::FAssetDataToken(FPrivateToken, const FAssetData& InAssetData, const FText& InLabelOverride)
    : AssetData(InAssetData)
{
	if ( !InLabelOverride.IsEmpty() )
	{
		CachedText = InLabelOverride;
	}
	else
	{
		if ( DefaultGetAssetDisplayName.IsBound() )
		{
			CachedText = DefaultGetAssetDisplayName.Execute(InAssetData, false);
		}
		else 
		{
			CachedText = FText::FromString(InAssetData.GetObjectPathString());
		}
	}
}

const FOnMessageTokenActivated& FAssetDataToken::GetOnMessageTokenActivated() const
{
	if(MessageTokenActivated.IsBound())
	{
		return MessageTokenActivated;
	}
	else
	{
		return DefaultMessageTokenActivated;
	}
}