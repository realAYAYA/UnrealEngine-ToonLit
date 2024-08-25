// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/SDMPropertyEdit.h"

class FAssetThumbnailPool;
class UDMMaterialValue;
class UDMMaterialValueTexture;
struct FAssetData;

class SDMPropertyEditTextureValue : public SDMPropertyEdit
{
	SLATE_BEGIN_ARGS(SDMPropertyEditTextureValue)
		{}
		SLATE_ARGUMENT(TSharedPtr<SDMComponentEdit>, ComponentEditWidget)
	SLATE_END_ARGS()

public:
	static TSharedPtr<SWidget> CreateEditWidget(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InTextureValue);

	SDMPropertyEditTextureValue() = default;
	virtual ~SDMPropertyEditTextureValue() override = default;

	void Construct(const FArguments& InArgs, UDMMaterialValueTexture* InTextureValue);

	UDMMaterialValueTexture* GetTextureValue() const;

protected:
	virtual TSharedRef<SWidget> GetComponentWidget(int32 InIndex) override;

	FString GetTexturePath() const;
	void OnTextureSelected(const FAssetData& InAssetData);
};
