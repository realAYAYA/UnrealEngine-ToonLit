// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/INiagaraDistributionAdapter.h"
#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;
struct FSlateIcon;
class INiagaraDistributionAdapter;
class SBox;

class SNiagaraDistributionEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraDistributionEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<INiagaraDistributionAdapter> InDistributionAdapter);

	static const float DefaultInputSize;

private:
	void OnDistributionModeChanged();

	TSharedRef<SWidget> ConstructContentForMode();

private:
	TSharedPtr<INiagaraDistributionAdapter> DistributionAdapter;
	TSharedPtr<SBox> ContentBox;
};