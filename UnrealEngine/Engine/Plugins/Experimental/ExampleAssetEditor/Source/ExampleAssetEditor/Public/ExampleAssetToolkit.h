// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Tools/BaseAssetToolkit.h"

class FEditorViewportClient;
class UAssetEditor;

class FExampleAssetToolkit : public FBaseAssetToolkit
{
public:
	FExampleAssetToolkit(UAssetEditor* InOwningAssetEditor);
	virtual ~FExampleAssetToolkit();


protected:
	// Base Asset Toolkit overrides
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;
	virtual void PostInitAssetEditor() override;
	// End Base Asset Toolkit overrides

	void AddInputBehaviorsForEditorClientViewport(TSharedPtr<FEditorViewportClient>& InViewportClient) const;
};
