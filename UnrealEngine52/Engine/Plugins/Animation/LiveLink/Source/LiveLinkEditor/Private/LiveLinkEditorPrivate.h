// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLiveLinkEditor, Log, All);


class ISlateStyle;

struct FLiveLinkEditorPrivate
{
	static TSharedPtr< class ISlateStyle > GetStyleSet();
};
