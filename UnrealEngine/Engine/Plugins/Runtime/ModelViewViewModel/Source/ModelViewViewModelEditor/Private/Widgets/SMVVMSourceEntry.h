// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Types/MVVMBindingSource.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class STextBlock;

namespace UE::MVVM
{

class SSourceEntry : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSourceEntry) :
		_TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		{
		}
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ARGUMENT(UE::MVVM::FBindingSource, Source)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void RefreshSource(const UE::MVVM::FBindingSource& Source);

private:
	TSharedPtr<STextBlock> Label;
	TSharedPtr<SImage> Image;
};

} // namespace UE::MVVM
