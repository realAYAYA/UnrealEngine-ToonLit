// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserMenuContexts.h"

#include "Containers/UnrealString.h"
#include "ContentBrowserDataSubsystem.h"
#include "SContentBrowser.h"
#include "UObject/UnrealNames.h"
#include "ToolMenuSection.h"

FName UContentBrowserToolbarMenuContext::GetCurrentPath() const
{
	if (TSharedPtr<SContentBrowser> Browser = ContentBrowser.Pin())
	{
		return *Browser->GetCurrentPath(EContentBrowserPathType::Virtual);
	}

	return NAME_None;
}

bool UContentBrowserToolbarMenuContext::CanWriteToCurrentPath() const
{
	if (TSharedPtr<SContentBrowser> Browser = ContentBrowser.Pin())
	{
		return Browser->CanWriteToCurrentPath();
	}

	return false;
}