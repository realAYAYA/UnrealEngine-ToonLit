// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"

class IVirtualKeyboardEntry;

class IPlatformTextField
{
public:
	virtual ~IPlatformTextField() {};

	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) = 0;
	virtual bool AllowMoveCursor() { return true; }

	static bool ShouldUseVirtualKeyboardAutocorrect(TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget);

private:

};
