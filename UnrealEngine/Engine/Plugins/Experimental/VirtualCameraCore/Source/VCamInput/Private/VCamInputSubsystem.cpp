// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputSubsystem.h"
#include "VCamInputProcessor.h"

#include "Framework/Application/SlateApplication.h"

LLM_DEFINE_TAG(VirtualCamera_CamInputSubsystem);
DEFINE_LOG_CATEGORY(LogVCamInput);

UDEPRECATED_VCamInputSubsystem::UDEPRECATED_VCamInputSubsystem()
{
	
}

UDEPRECATED_VCamInputSubsystem::~UDEPRECATED_VCamInputSubsystem()
{
	
}

void UDEPRECATED_VCamInputSubsystem::SetShouldConsumeGamepadInput(const bool bInShouldConsumeGamepadInput)
{
	
}

bool UDEPRECATED_VCamInputSubsystem::GetShouldConsumeGamepadInput() const
{
	return false;
}

void UDEPRECATED_VCamInputSubsystem::BindKeyDownEvent(const FKey Key, FKeyInputDelegate Delegate)
{
	
}

void UDEPRECATED_VCamInputSubsystem::BindKeyUpEvent(const FKey Key, FKeyInputDelegate Delegate)
{
	
}

void UDEPRECATED_VCamInputSubsystem::BindAnalogEvent(const FKey Key, FAnalogInputDelegate Delegate)
{
	
}

void UDEPRECATED_VCamInputSubsystem::BindMouseMoveEvent(FPointerInputDelegate Delegate)
{
	
}

void UDEPRECATED_VCamInputSubsystem::BindMouseButtonDownEvent(const FKey Key, FPointerInputDelegate Delegate)
{
	
}

void UDEPRECATED_VCamInputSubsystem::BindMouseButtonUpEvent(const FKey Key, FPointerInputDelegate Delegate)
{
	
}

void UDEPRECATED_VCamInputSubsystem::BindMouseDoubleClickEvent(const FKey Key, FPointerInputDelegate Delegate)
{
	
}

void UDEPRECATED_VCamInputSubsystem::BindMouseWheelEvent(FPointerInputDelegate Delegate)
{
	
}
