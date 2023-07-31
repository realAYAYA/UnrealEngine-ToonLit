// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTabButtonBase.h"

#include "CommonLazyImage.h"
#include "UI/Common/LyraTabListWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTabButtonBase)

class UObject;
struct FSlateBrush;

void ULyraTabButtonBase::SetIconFromLazyObject(TSoftObjectPtr<UObject> LazyObject)
{
	if (LazyImage_Icon)
	{
		LazyImage_Icon->SetBrushFromLazyDisplayAsset(LazyObject);
	}
}

void ULyraTabButtonBase::SetIconBrush(const FSlateBrush& Brush)
{
	if (LazyImage_Icon)
	{
		LazyImage_Icon->SetBrush(Brush);
	}
}

void ULyraTabButtonBase::SetTabLabelInfo_Implementation(const FLyraTabDescriptor& TabLabelInfo)
{
	SetButtonText(TabLabelInfo.TabText);
	SetIconBrush(TabLabelInfo.IconBrush);
}

