// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class LIVELINKHUB_API ILiveLinkHubModule : public IModuleInterface
{
public:
	/** Launch the slate application hosting the live link hub. */
	virtual void StartLiveLinkHub() = 0;
};

