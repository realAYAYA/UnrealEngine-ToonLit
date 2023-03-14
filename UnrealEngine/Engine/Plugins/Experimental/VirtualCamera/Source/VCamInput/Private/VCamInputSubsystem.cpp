// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputSubsystem.h"
#include "VCamInputProcessor.h"

#include "Framework/Application/SlateApplication.h"

LLM_DEFINE_TAG(VirtualCamera_CamInputSubsystem);
DEFINE_LOG_CATEGORY(LogVCamInput);

UVCamInputSubsystem::UVCamInputSubsystem()
{
	LLM_SCOPE_BYTAG(VirtualCamera_CamInputSubsystem);

	// Registering the input processor is only valid in the actual subsystem and not the CDO
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		InputProcessor = MakeShared<FVCamInputProcessor>();
		if (FSlateApplication::IsInitialized())
		{
			bIsRegisterd = FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
		}
	}
}

UVCamInputSubsystem::~UVCamInputSubsystem()
{
	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
}


void UVCamInputSubsystem::SetShouldConsumeGamepadInput(const bool bInShouldConsumeGamepadInput)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->bShouldConsumeGamepadInput = bInShouldConsumeGamepadInput;
	}
}

bool UVCamInputSubsystem::GetShouldConsumeGamepadInput() const
{
	bool bShouldConsumeGamepadInput = false;
	if (InputProcessor.IsValid())
	{
		bShouldConsumeGamepadInput = InputProcessor->bShouldConsumeGamepadInput;
	}
	return bShouldConsumeGamepadInput;
}

void UVCamInputSubsystem::BindKeyDownEvent(const FKey Key, FKeyInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->KeyDownDelegateStore.AddDelegate(Key, Delegate);
	}
}

void UVCamInputSubsystem::BindKeyUpEvent(const FKey Key, FKeyInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->KeyUpDelegateStore.AddDelegate(Key, Delegate);
	}
}

void UVCamInputSubsystem::BindAnalogEvent(const FKey Key, FAnalogInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->AnalogDelegateStore.AddDelegate(Key, Delegate);
	}
}

void UVCamInputSubsystem::BindMouseMoveEvent(FPointerInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->MouseMoveDelegateStore.AddDelegate(EKeys::Invalid, Delegate);
	}
}

void UVCamInputSubsystem::BindMouseButtonDownEvent(const FKey Key, FPointerInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->MouseButtonDownDelegateStore.AddDelegate(Key, Delegate);
	}
}

void UVCamInputSubsystem::BindMouseButtonUpEvent(const FKey Key, FPointerInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->MouseButtonUpDelegateStore.AddDelegate(Key, Delegate);
	}
}

void UVCamInputSubsystem::BindMouseDoubleClickEvent(const FKey Key, FPointerInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->MouseButtonDoubleClickDelegateStore.AddDelegate(Key, Delegate);
	}
}

void UVCamInputSubsystem::BindMouseWheelEvent(FPointerInputDelegate Delegate)
{
	if (InputProcessor.IsValid())
	{
		InputProcessor->MouseWheelDelegateStore.AddDelegate(EKeys::Invalid, Delegate);
	}
}
