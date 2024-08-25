// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "Widgets/SCompoundWidget.h"

class IPropertyHandle;
class SVerticalBox;
class UDMMaterialValue;
class UWorld;
struct FOnGenerateGlobalRowExtensionArgs;

class SDMDetailsGridRow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMDetailsGridRow)
		: _MinColumnWidth(0.0f)
		, _RowSeparatorColor(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
		, _Resizeable(true)
		, _ShowSeparators(true)
		, _SeparatorColor(FLinearColor(1, 1, 1, 0.2f))
		, _UseEvenOddRowBackgroundColors(false)
		, _OddRowBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
		, _EvenRowBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.1f))
	{}
		SLATE_ARGUMENT(float, MinColumnWidth)
		SLATE_ARGUMENT(FLinearColor, RowSeparatorColor)
		SLATE_ARGUMENT(bool, Resizeable)
		SLATE_ARGUMENT(bool, ShowSeparators)
		SLATE_ARGUMENT(FLinearColor, SeparatorColor)
		SLATE_ARGUMENT(bool, UseEvenOddRowBackgroundColors)
		SLATE_ARGUMENT(FLinearColor, OddRowBackgroundColor)
		SLATE_ARGUMENT(FLinearColor, EvenRowBackgroundColor)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	void AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle);
	void AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget);
	void AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget,
		const FOnGenerateGlobalRowExtensionArgs& InExtensionArgs);
	void AddPropertyRow(const FText& InText, const TSharedRef<SWidget>& InContent, const TSharedRef<SWidget>& InExtensions);

protected:
	float MinColumnWidth;
	FLinearColor RowSeparatorColor;
	bool bResizeable;
	bool bShowSeparators;
	FLinearColor SeparatorColor;
	bool UseEvenOddRowBackgroundColors;
	FLinearColor OddRowBackgroundColor;
	FLinearColor EvenRowBackgroundColor;

	TSharedPtr<SVerticalBox> LabelColumnBox;
	TSharedPtr<SVerticalBox> ContentColumnBox;
	TSharedPtr<SVerticalBox> ExtensionsColumnBox;
};
