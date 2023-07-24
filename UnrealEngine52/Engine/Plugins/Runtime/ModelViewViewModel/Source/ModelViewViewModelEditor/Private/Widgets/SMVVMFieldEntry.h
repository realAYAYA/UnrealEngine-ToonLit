// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMPropertyPath.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateWidgetStyleAsset.h"

template <typename ValueType, typename ErrorType> class TValueOrError;

class SHorizontalBox;

namespace UE::MVVM
{

class SFieldIcon;

using FIsFieldValidResult = TValueOrError<bool, FString>;
DECLARE_DELEGATE_RetVal_OneParam(FIsFieldValidResult, FIsFieldValid, FMVVMBlueprintPropertyPath);

class SFieldEntry : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFieldEntry) :
		_TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
	{}
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ARGUMENT(FMVVMBlueprintPropertyPath, Field)
		SLATE_ARGUMENT(bool, ShowOnlyLast)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Refresh();
	void SetField(const FMVVMBlueprintPropertyPath& InField);

private:
	const FTextBlockStyle* TextStyle = nullptr;
	FMVVMBlueprintPropertyPath Field;
	TSharedPtr<SHorizontalBox> FieldBox;
	bool bShowOnlyLast = false;
};

} // namespace UE::MVVM
