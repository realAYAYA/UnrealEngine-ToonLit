// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCommonEditorViewportToolbarBase.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FMenuBuilder;
class ICommonEditorViewportToolbarInfoProvider;
class SEditorViewportViewMenu;

/**
 * Viewport toolbar widget used by the Chaos Visual Debugger
 */
class CHAOSVD_API SChaosVDViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SChaosVDViewportToolbar)
		{
		}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<ICommonEditorViewportToolbarInfoProvider> InInfoProvider);

protected:
	virtual TSharedRef<SEditorViewportViewMenu> MakeViewMenu() override;
	virtual void ExtendOptionsMenu(FMenuBuilder& OptionsMenuBuilder) const override;

private:
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;

};
