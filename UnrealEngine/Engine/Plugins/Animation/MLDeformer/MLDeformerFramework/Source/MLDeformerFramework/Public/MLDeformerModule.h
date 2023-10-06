// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Modules/ModuleManager.h"

// The log category for the ML Deformer framework.
MLDEFORMERFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogMLDeformer, Log, All);

namespace UE::MLDeformer
{
	/**
	 * The runtime module for the ML Deformer.
	 */
	class MLDEFORMERFRAMEWORK_API FMLDeformerModule
		: public IModuleInterface
	{
	public:
		// IModuleInterface overrides.
		void StartupModule() override;
		// ~END IModuleInterface overrides.
	};
}	// namespace UE::MLDeformer
