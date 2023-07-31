// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"


class FSkeletalMeshModelingToolsStyle
    : public FSlateStyleSet
{
public:
	static FSkeletalMeshModelingToolsStyle& Get();

protected:
	friend class FSkeletalMeshModelingToolsModule;

	static void Register();
	static void Unregister();

private:
	FSkeletalMeshModelingToolsStyle();
};
