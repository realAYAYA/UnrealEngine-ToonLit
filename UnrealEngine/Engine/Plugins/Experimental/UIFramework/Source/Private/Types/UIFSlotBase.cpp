// Copyright Epic Games, Inc. All Rights Reserved.

#include "Types/UIFSlotBase.h"
#include "UIFWidget.h"
//#include "UIFManagerSubsystem.h"
#include "UIFPlayerComponent.h"

#include "Engine/ActorChannel.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"


/**
 *
 */
void FUIFrameworkSlotBase::AuthoritySetWidget(UUIFrameworkWidget* InWidget)
{
	Widget = InWidget;
	WidgetId = Widget ? Widget->GetWidgetId() : FUIFrameworkWidgetId();
}
