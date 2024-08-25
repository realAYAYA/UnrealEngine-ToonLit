// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvalancheComponentVisualizersModule.h"

class FAvalancheComponentVisualizersModule : public IAvalancheComponentVisualizersModule
{
public:
	//~ Begin IAvalancheComponentVisualizersModule
	virtual IAvaComponentVisualizersSettings* GetSettings() const override;
	virtual void RegisterComponentVisualizer(FName InComponentClassName, TSharedRef<FComponentVisualizer> InVisualizer) const override;
	virtual bool IsAvalancheVisualizer(TSharedRef<FComponentVisualizer> InVisualizer) const override;
	virtual IAvaComponentVisualizersViewportOverlay& GetViewportOverlay() const override;
	//~ End IAvalancheComponentVisualizersModule
};
