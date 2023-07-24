// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class IDisplayClusterOperatorApp : public TSharedFromThis<IDisplayClusterOperatorApp>
{
public:
	virtual ~IDisplayClusterOperatorApp() = default;
};