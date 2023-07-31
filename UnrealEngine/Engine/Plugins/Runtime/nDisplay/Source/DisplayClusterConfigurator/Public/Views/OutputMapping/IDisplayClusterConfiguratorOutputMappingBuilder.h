// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/IDisplayClusterConfiguratorBuilder.h"

class IDisplayClusterConfiguratorOutputMappingSlot;
class SWidget;
class UObject;

/**
 * The Interface for generation of nodes from config
 */
class IDisplayClusterConfiguratorOutputMappingBuilder
	: public IDisplayClusterConfiguratorBuilder
{
public:
	virtual ~IDisplayClusterConfiguratorOutputMappingBuilder() = default;

public:
	struct FSlot
	{
		static const FName Canvas;
		static const FName Window;
		static const FName Viewport;
	};

	virtual void Build() = 0;

	virtual TSharedRef<SWidget> GetCanvasWidget() const = 0;

	virtual const TArray<TSharedPtr<IDisplayClusterConfiguratorOutputMappingSlot>>& GetAllSlots() const = 0;
};
