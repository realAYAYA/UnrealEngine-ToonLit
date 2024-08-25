// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IDetailLayoutBuilder;
class IDetailGroup;
class UVCamWidget;

namespace UE::VCamCoreEditor
{
	class IConnectionRemapUtils;

	struct FTargetConnectionDisplaySettings
	{
		bool bOnlyShowManuallyConfiguredConnections = true;
	};
	
	struct FShouldGenerateArgs
	{
		TWeakObjectPtr<UVCamWidget> CustomizedWidget;
		FTargetConnectionDisplaySettings DisplaySettings;
	};

	struct FConnectionRemapCustomizationArgs
	{
		/** Use to register custom layouts or add external object / struct data. */
		IDetailLayoutBuilder& Builder;
		/** Builder for the group under which to add the widget's data. */
		IDetailGroup& WidgetGroup;
		
		TSharedRef<IConnectionRemapUtils> Utils;
		TWeakObjectPtr<UVCamWidget> CustomizedWidget;
		
		FTargetConnectionDisplaySettings DisplaySettings;
	};

	/**
	 * Implements support for customizing the connection target points of specific widget types.
	 *
	 * Instances are created when a widget needs customization and kept alive until 1. the widget becomes invalid or 2. the details panel displays a new object;
	 * stays alive across calls to IDetailBuilder::ForceRefreshDetails.
	 */
	class VCAMCOREEDITOR_API IConnectionRemapCustomization : public TSharedFromThis<IConnectionRemapCustomization>
	{
	public:

		/** Whether a group should be generated for this widget */
		virtual bool CanGenerateGroup(const FShouldGenerateArgs& Args) const = 0;
		
		/** Adds rows for CustomizedWidget */
		virtual void Customize(const FConnectionRemapCustomizationArgs& Args) = 0;
		
		virtual ~IConnectionRemapCustomization() = default;
	};
}