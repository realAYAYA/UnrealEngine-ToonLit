// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"
#include "Features/IPluginsEditorFeature.h"
#include "GameFeatureTypesFwd.h"

class IDetailLayoutBuilder;
struct FPluginEditingContext;
class IPlugin;
struct FPluginDescriptor;

//////////////////////////////////////////////////////////////////////////
// FGameFeaturePluginMetadataCustomization

class FGameFeaturePluginMetadataCustomization : public FPluginEditorExtension
{
public:
	void CustomizeDetails(FPluginEditingContext& InPluginContext, IDetailLayoutBuilder& DetailBuilder);

	virtual void CommitEdits(FPluginDescriptor& Descriptor) override;
private:
	EGameFeaturePluginState GetDefaultState() const;

	void ChangeDefaultState(EGameFeaturePluginState DesiredState);

	TSharedPtr<IPlugin> Plugin;

	EGameFeaturePluginState InitialState;
};
