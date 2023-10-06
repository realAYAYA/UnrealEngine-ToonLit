// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleActor.h"

#include "Components/SceneComponent.h"
#include "DMXControlConsoleData.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleActor"

ADMXControlConsoleActor::ADMXControlConsoleActor()
{
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	RootComponent = RootSceneComponent;
}

#if WITH_EDITOR
void ADMXControlConsoleActor::SetDMXControlConsoleData(UDMXControlConsoleData* InControlConsoleData)
{
	if (!ensureAlwaysMsgf(!ControlConsoleData, TEXT("Tried to set the DMXControlConsole for %s, but it already has one set. Changing the control console is not supported."), *GetName()))
	{
		return;
	}

	if (InControlConsoleData)
	{
		ControlConsoleData = InControlConsoleData;
	}
}
#endif // WITH_EDITOR

void ADMXControlConsoleActor::StartSendingDMX()
{
	if (ControlConsoleData)
	{
		ControlConsoleData->StartSendingDMX();
	}
}

void ADMXControlConsoleActor::StopSendingDMX()
{
	if (ControlConsoleData)
	{
		ControlConsoleData->StopSendingDMX();
	}
}

void ADMXControlConsoleActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoActivate)
	{
		StartSendingDMX();
	}
}

void ADMXControlConsoleActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	StopSendingDMX();
}

#if WITH_EDITOR
void ADMXControlConsoleActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ADMXControlConsoleActor, bSendDMXInEditor) &&
		ControlConsoleData)
	{
		ControlConsoleData->SetSendDMXInEditorEnabled(bSendDMXInEditor);
	}
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
