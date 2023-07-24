// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWebAPIModule.h"


class FWebAPIModule final
    : public IWebAPIModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
