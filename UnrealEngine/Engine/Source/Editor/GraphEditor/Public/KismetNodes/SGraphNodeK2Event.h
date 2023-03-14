// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "KismetNodes/SGraphNodeK2Default.h"
#include "Layout/Visibility.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWidget.h"

class SGraphPin;
class SNodeTitle;
class SOverlay;
class SWidget;

class GRAPHEDITOR_API SGraphNodeK2Event : public SGraphNodeK2Default
{
public:
	SGraphNodeK2Event() : SGraphNodeK2Default(), bHasDelegateOutputPin(false) {}

protected:
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle) override;
	virtual bool UseLowDetailNodeTitles() const override;
	virtual void AddPin( const TSharedRef<SGraphPin>& PinToAdd ) override;


	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override
	{
		TitleAreaWidget = DefaultTitleAreaWidget;
	}

private:
	bool ParentUseLowDetailNodeTitles() const
	{
		return SGraphNodeK2Default::UseLowDetailNodeTitles();
	}

	EVisibility GetTitleVisibility() const;

	TSharedPtr<SOverlay> TitleAreaWidget;
	bool bHasDelegateOutputPin;
};
