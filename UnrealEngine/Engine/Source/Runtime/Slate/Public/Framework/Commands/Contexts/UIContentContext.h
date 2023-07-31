// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateTypes.h"
#include "Framework/SlateDelegates.h"

/**
 * Context for generating content from a FUICommandInfo
 */
struct SLATE_API FUIContentContext : public IUIActionContextBase
{
	virtual ~FUIContentContext() {}

	/** Holds a delegate that is executed to generate menu actions from the stack. */
	FOnGetContent OnGetContent;

	//~ Begin IUIActionContextBase Interface
	virtual FName GetContextName() const override
	{
		return ContextName;
	}
	//~ End IUIActionContextBase Interface

	inline static const FName ContextName = FName("ContentContext");
};