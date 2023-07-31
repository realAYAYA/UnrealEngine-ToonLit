// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "MLDeformerModelRegistry.h"

namespace UE::MLDeformer
{
	class FMLDeformerAssetActions;

	/**
	 * The ML Deformer editor module.
	 * This registers the editor mode and custom property customizations.
	 * It also contains the model registry, which you will use to register your custom models to the editor.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerEditorModule
		: public IModuleInterface
	{
	public:
		// IModuleInterface overrides.
		void StartupModule() override;
		void ShutdownModule() override;
		// ~END IModuleInterface overrides.

		/**
		 * Get a reference to the ML Deformer model registry.
		 * You use this to see what kind of model types ther are, register new models, or create editor models for specific types of runtime models.
		 * @return A reference to the ML Deformer model registry.
		 */
		FMLDeformerEditorModelRegistry& GetModelRegistry()				{ return ModelRegistry; }
		const FMLDeformerEditorModelRegistry& GetModelRegistry() const	{ return ModelRegistry; }

	private:
		/** The model registry, which keeps track of all model types and instances created of these models, and how to create the editor models for specific runtime model types. */
		FMLDeformerEditorModelRegistry ModelRegistry;

		/** The asset actions for the ML Deformer asset type. */
		TSharedPtr<FMLDeformerAssetActions> MLDeformerAssetActions;
	};
}	// namespace UE::MLDeformer
