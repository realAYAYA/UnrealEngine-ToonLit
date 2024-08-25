// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ICoreUObjectPluginManager.h"

namespace UE::CoreUObject::Private
{
	class PluginHandler : public UE::PluginManager::Private::ICoreUObjectPluginManager
	{
	public:
		static void Install();

		virtual void OnPluginUnload(IPlugin& Plugin) override;
	};
}