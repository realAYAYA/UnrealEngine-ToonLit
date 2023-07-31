// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class NIAGARAEDITOR_API SDynamicLayoutBox : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateNamedWidget, FName /* InWidgetName */);

	class NIAGARAEDITOR_API FNamedWidgetProvider
	{
	public:
		FNamedWidgetProvider();
		FNamedWidgetProvider(FOnGenerateNamedWidget InGeneratedNamedWidget);

		TSharedRef<SWidget> GetNamedWidget(FName InWidgetName) const;

	private:
		mutable TMap<FName, TSharedRef<SWidget>> NameToGeneratedWidgetCache;
		FOnGenerateNamedWidget GenerateNamedWidget;
	};

	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<SWidget>, FOnGenerateNamedLayout, FName /* InLayoutName */, const FNamedWidgetProvider& /* InNamedWidgetProvider */);

	DECLARE_DELEGATE_RetVal(FName, FOnChooseLayout);

public:
	SLATE_BEGIN_ARGS(SDynamicLayoutBox)
	{}
		SLATE_EVENT(FOnGenerateNamedWidget, GenerateNamedWidget)
		SLATE_EVENT(FOnGenerateNamedLayout, GenerateNamedLayout)
		SLATE_EVENT(FOnChooseLayout, ChooseLayout);
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FNamedWidgetProvider NamedWidgetProvider;
	FOnGenerateNamedLayout GenerateNamedLayout;
	FOnChooseLayout ChooseLayout;
	TOptional<FName> CurrentLayout;
};