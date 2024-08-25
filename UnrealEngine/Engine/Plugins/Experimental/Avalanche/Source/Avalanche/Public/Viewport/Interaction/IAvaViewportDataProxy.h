// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"

class IAvaViewportDataProvider;

/**
 *
 */
class IAvaViewportDataProxy
{
public:
	UE_AVA_TYPE(IAvaViewportDataProxy);

#if WITH_EDITOR
	virtual IAvaViewportDataProvider* GetViewportDataProvider() const = 0;
#endif
};
