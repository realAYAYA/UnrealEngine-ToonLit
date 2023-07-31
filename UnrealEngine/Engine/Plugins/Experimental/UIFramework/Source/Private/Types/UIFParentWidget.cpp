// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/UIFParentWidget.h"
#include "GameFramework/PlayerController.h"
#include "UIFWidget.h"
#include "UIFPlayerComponent.h"

/**
 *
 */
FUIFrameworkParentWidget::FUIFrameworkParentWidget(UUIFrameworkWidget* InWidget)
	: Parent(InWidget)
	, bIsParentAWidget(true)
{
}


FUIFrameworkParentWidget::FUIFrameworkParentWidget(UUIFrameworkPlayerComponent* InPlayer)
	: Parent(InPlayer)
	, bIsParentAWidget(false)
{
}


UUIFrameworkWidget* FUIFrameworkParentWidget::AsWidget() const
{
	return CastChecked<UUIFrameworkWidget>(Parent);
}


UUIFrameworkPlayerComponent* FUIFrameworkParentWidget::AsPlayerComponent() const
{
	return CastChecked<UUIFrameworkPlayerComponent>(Parent);
}
