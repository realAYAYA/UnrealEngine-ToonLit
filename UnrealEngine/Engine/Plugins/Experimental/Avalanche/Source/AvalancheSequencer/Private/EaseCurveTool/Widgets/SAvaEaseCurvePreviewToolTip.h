// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EaseCurveTool/Widgets/SAvaEaseCurvePreview.h"
#include "Templates/SharedPointer.h"
#include "Widgets/IToolTip.h"

class SToolTip;
class SWidget;

class SAvaEaseCurvePreviewToolTip : public IToolTip
{
public:
	static FText GetToolTipText(const FAvaEaseCurveTangents& InTangents);

	static TSharedRef<SToolTip> CreateDefaultToolTip(const SAvaEaseCurvePreview::FArguments& InPreviewArgs, const TSharedPtr<SWidget>& InAdditionalContent = nullptr);

	SAvaEaseCurvePreviewToolTip(const SAvaEaseCurvePreview::FArguments& InPreviewArgs, const TSharedPtr<SWidget>& InAdditionalContent = nullptr);
	virtual ~SAvaEaseCurvePreviewToolTip() override {}

protected:
	void CreateToolTipWidget();

	void InvalidateWidget();

	//~ Begin IToolTip
	virtual TSharedRef<SWidget> AsWidget() override;
	virtual TSharedRef<SWidget> GetContentWidget() override;
	virtual void SetContentWidget(const TSharedRef<SWidget>& InContentWidget) override;
	virtual bool IsEmpty() const override { return false; }
	virtual bool IsInteractive() const override { return false; }
	virtual void OnOpening() override {}
	virtual void OnClosed() override {}
	//~ End IToolTip

	SAvaEaseCurvePreview::FArguments PreviewArgs;
	TSharedPtr<SWidget> AdditionalContent;

	TSharedPtr<SToolTip> ToolTipWidget;
};
