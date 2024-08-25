// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneOutlinerPublicTypes.h"

namespace SceneOutliner
{
	// Class to hold common functionality needed by the Outliner and helpful to modules creating Outliner instances
	class SCENEOUTLINER_API FSceneOutlinerHelpers
	{
	public:
		static FString GetExternalPackageName(const ISceneOutlinerTreeItem& TreeItem);
		static UPackage* GetExternalPackage(const ISceneOutlinerTreeItem& TreeItem);
		static TSharedPtr<SWidget> GetClassHyperlink(UObject* InObject);
	};
};
