// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Tools/BaseAssetToolkit.h"
#include "Delegates/IDelegateInstance.h"

class SEditorViewport;
class FEditorViewportClient;
class UAssetEditor;

class FLevelEditorAssetToolkit : public FBaseAssetToolkit
{
public:
	FLevelEditorAssetToolkit(UAssetEditor* InOwningAssetEditor);
	virtual ~FLevelEditorAssetToolkit();


protected:
	// Base Asset Toolkit overrides
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void PostInitAssetEditor() override;
	// End Base Asset Toolkit overrides

	void AddInputBehaviorsForEditorClientViewport(TSharedPtr<FEditorViewportClient>& InViewportClient) const;
};
