// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaComponentVisualizersViewportOverlay.h"
#include "Templates/SharedPointer.h"

class SWidget;

class FAvaComponentVisualizersViewportOverlay : public IAvaComponentVisualizersViewportOverlay
{
public:
	virtual ~FAvaComponentVisualizersViewportOverlay() override = default;

	//~ Begin IAvaComponentVisualizersViewportOverlay
	virtual void AddWidget(const TArray<TSharedPtr<IAvaViewportClient>>& InAvaViewportClients, const TArray<UObject*>& InSelectedObjects) override;
	virtual void RemoveWidget(const TArray<TSharedPtr<IAvaViewportClient>>& InAvaViewportClients) override;
	virtual bool IsWidgetActive() const override;
	//~ End IAvaComponentVisualizersViewportOverlay

protected:
	TSharedPtr<SWidget> OverlayWidget;
};
