// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class UModifierCompilationBlueprintExtension;

namespace UE::VCamCoreEditor::Private
{
	/** Manages all of the plugin's UBlueprintExtensions making sure they are added when a Blueprint asset is loaded. */
	class FCompilationExtensionManager : public TSharedFromThis<FCompilationExtensionManager>
	{
	public:

		~FCompilationExtensionManager();

		void Init();

	private:

		void OnAssetLoaded(UObject* Object) const;
	};
}


