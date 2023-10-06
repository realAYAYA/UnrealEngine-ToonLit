// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SMVVMCachedViewBindingPropertyPath.h"

class SHorizontalBox;
class UWidgetBlueprint;

namespace UE::MVVM
{
DECLARE_DELEGATE_RetVal(const UFunction*, FOnGetConversionFunction);

class SCachedViewBindingConversionFunction : public SCompoundWidget
{
private:
	using Super = SCompoundWidget;

public:

	SLATE_BEGIN_ARGS(SCachedViewBindingConversionFunction)
		: _TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
	{}
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_EVENT(FOnGetConversionFunction, OnGetConversionFunction)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const UWidgetBlueprint* WidgetBlueprint);

private:
	TWeakObjectPtr<const UWidgetBlueprint> WidgetBlueprint;
	FOnGetConversionFunction OnGetConversionFunction;
};

} // namespace UE::MVVM
