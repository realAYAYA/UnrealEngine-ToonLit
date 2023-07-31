// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRenderGridDeveloperModule.h"


namespace UE::RenderGrid::Private
{
	/**
	 * The implementation of the IRenderGridDeveloperModule interface.
	 */
	class FRenderGridDeveloperModule : public IRenderGridDeveloperModule
	{
	public:
		//~ Begin IModuleInterface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface
	};
}
