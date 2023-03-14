// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

/** String Table Editor public interface */
class IStringTableEditor : public FAssetEditorToolkit
{
public:
	/**
	 * Notify the String Table editor that it needs to refresh due to an external String Table change.
	 */
	virtual void RefreshStringTableEditor(const FString& NewSelection = FString()) = 0;
};
