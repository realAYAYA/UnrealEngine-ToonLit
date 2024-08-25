// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ToolkitHost/FloatingPropertiesToolkitHostDataProvider.h"
#include "Containers/Map.h"
#include "Data/IFloatingPropertiesWidgetContainer.h"
#include "Templates/SharedPointer.h"

class FFloatingPropertiesLevelEditorWidgetContainer;
class ILevelEditor;
class SLevelViewport;

class FFloatingPropertiesLevelEditorDataProvider : public FFloatingPropertiesToolkitHostDataProvider
{
public:
	FFloatingPropertiesLevelEditorDataProvider(TSharedRef<ILevelEditor> InLevelEditor);
	virtual ~FFloatingPropertiesLevelEditorDataProvider() = default;

	//~ Begin IFloatingPropertiesDataProvider
	virtual TArray<TSharedRef<IFloatingPropertiesWidgetContainer>> GetWidgetContainers() override;
	virtual bool IsWidgetVisibleInContainer(TSharedRef<IFloatingPropertiesWidgetContainer> InContainer) const override;
	//~ End IFloatingPropertiesDataProvider

protected:
	TMap<TWeakPtr<SLevelViewport>, TSharedRef<FFloatingPropertiesLevelEditorWidgetContainer>> CachedContainers;

	void ClearExpiredViewports();
};
