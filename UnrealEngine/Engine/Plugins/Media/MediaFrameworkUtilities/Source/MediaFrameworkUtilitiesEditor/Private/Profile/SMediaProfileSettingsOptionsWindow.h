// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Profile/MediaProfileSettingsCustomizationOptions.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"


class IStructureDetailsView;
class SWindow;


/*
 *
 */
class SMediaProfileSettingsOptionsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMediaProfileSettingsOptionsWindow) {}
	SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override;

public:
	bool ShouldConfigure(FMediaProfileSettingsCustomizationOptions& OutSettingsOptions) const;

private:
	FReply OnConfigure();
	FReply OnCancel();
	bool CanConfigure() const;

private:
	TWeakPtr<SWindow> Window;
	TSharedPtr<IStructureDetailsView> DetailView;
	FMediaProfileSettingsCustomizationOptions Options;
	bool bConfigure;
};
