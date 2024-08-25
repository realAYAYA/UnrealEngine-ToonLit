// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateMaterialBrush.h"
#include "Widgets/SCompoundWidget.h"

class UMaterialInterface;

class SMaterialToolTip : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMaterialToolTip)
		: _Text(FText())
		, _MaterialSize(FVector2D(512.0f))
		, _ShowMaterial(true)
		, _ShowDefault(false)
	{}
		/** The material to display in the tooltip. */
		SLATE_ARGUMENT(UMaterialInterface*, Material)
		/** The tooltip text to display. */
		SLATE_ATTRIBUTE(FText, Text)
		/** The size of the space for the texture. This is not the texture size itself. */
		SLATE_ATTRIBUTE(FVector2D, MaterialSize)
		/** If true, shows the material in the tooltip along with the tooltip text. If false, shows only the tooltip text. */
		SLATE_ATTRIBUTE(bool, ShowMaterial)
		/** If true, shows the default text widget instead of the customized texture tooltip. */
		SLATE_ARGUMENT(bool, ShowDefault)
	SLATE_END_ARGS()

public:
	SMaterialToolTip();
	virtual ~SMaterialToolTip() {}

	void Construct(const FArguments& InArgs);

protected:
	TWeakObjectPtr<UMaterialInterface> Material;
	TAttribute<FText> Text;
	TAttribute<FVector2D> MaterialSize;
	TAttribute<bool> ShowMaterial;
	bool ShowDefault;

	FSlateMaterialBrush MaterialBrush;
	
	TSharedRef<SWidget> CreateToolTipWidget();

	EVisibility GetShowMaterialVisibility() const;
	FOptionalSize GetMaterialSizeX() const;
	FOptionalSize GetMaterialSizeY() const;
};
