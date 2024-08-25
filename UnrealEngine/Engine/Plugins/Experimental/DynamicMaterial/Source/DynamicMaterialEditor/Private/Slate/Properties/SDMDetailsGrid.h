// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "Widgets/SCompoundWidget.h"

class IPropertyHandle;
class SVerticalBox;
class UDMMaterialValue;
class UWorld;
struct FOnGenerateGlobalRowExtensionArgs;

/**
 * Material Designer Property Details Grid Panel
 * 
 * Manages width, labels, splitters, extensions and styling for a group of material properties.
*/
class SDMDetailsGrid : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMDetailsGrid)
		: _MinColumnWidth(0.0f)
		, _RowSeparatorColor(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
		, _Resizeable(false)
		, _ShowSeparators(true)
		, _SeparatorColor(FLinearColor(1, 1, 1, 0.2f))
		, _UseEvenOddRowBackgroundColors(false)
		, _OddRowBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
		, _EvenRowBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.1f))
		, _LabelFillWidth(0.2f)
		, _ContentFillWidth(0.8f)
	{}
		SLATE_ARGUMENT(float, MinColumnWidth)
		SLATE_ARGUMENT(FLinearColor, RowSeparatorColor)
		SLATE_ARGUMENT(bool, Resizeable)
		SLATE_ARGUMENT(bool, ShowSeparators)
		SLATE_ARGUMENT(FLinearColor, SeparatorColor)
		SLATE_ARGUMENT(bool, UseEvenOddRowBackgroundColors)
		SLATE_ARGUMENT(FLinearColor, OddRowBackgroundColor)
		SLATE_ARGUMENT(FLinearColor, EvenRowBackgroundColor)
		SLATE_ATTRIBUTE(float, LabelFillWidth)
		SLATE_ATTRIBUTE(float, ContentFillWidth)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	/**  */
	void AddRow(const TSharedRef<SWidget>& InLabel, const TSharedRef<SWidget>& InContent, const TSharedPtr<SWidget>& InExtensions = nullptr);
	void AddRow_TextLabel(const TAttribute<FText>& InText, const TSharedRef<SWidget>& InContent, const TSharedRef<SWidget>& InExtensions);
	void AddRow_TextLabel(const TAttribute<FText>& InText, const TSharedRef<SWidget>& InContent);
	void AddFullRowContent(const TSharedRef<SWidget>& InContent);

	void AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle);
	void AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget);
	void AddPropertyRow(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, const TSharedRef<SWidget>& InEditWidget, 
		const FOnGenerateGlobalRowExtensionArgs& InExtensionArgs);

	static TSharedRef<SWidget> CreateDefaultLabel(const TAttribute<FText>& InText);

	static TSharedRef<SWidget> CreateExtensionButtons(UObject* InObject, const TSharedPtr<IPropertyHandle>& InPropertyHandle, 
		const FOnGenerateGlobalRowExtensionArgs& InExtensionArgs);

protected:
	float MinColumnWidth;
	FLinearColor RowSeparatorColor;
	bool bResizeable;
	bool bShowSeparators;
	FLinearColor SeparatorColor;
	bool UseEvenOddRowBackgroundColors;
	FLinearColor OddRowBackgroundColor;
	FLinearColor EvenRowBackgroundColor;
	TAttribute<float> LabelFillWidth;
	TAttribute<float> ContentFillWidth;

	TSharedPtr<SVerticalBox> MainContainer;

	TSharedRef<SWidget> CreateSplitterRow(const TSharedPtr<SWidget>& InLabel, const TSharedPtr<SWidget>& InContent, 
		const TSharedPtr<SWidget>& InExtensions);
	TSharedRef<SWidget> CreateHorizontalRow(const TSharedPtr<SWidget>& InLabel, const TSharedPtr<SWidget>& InContent,
		const TSharedPtr<SWidget>& InExtensions);

	FOnGenerateGlobalRowExtensionArgs CreateExtensionArgs(UDMMaterialValue* InValue);

	static void CreateKey(UObject* InObject, TSharedPtr<IPropertyHandle> InPropertyHandle);
	static void CreateKey(UDMMaterialValue* InMaterialValue);
};
