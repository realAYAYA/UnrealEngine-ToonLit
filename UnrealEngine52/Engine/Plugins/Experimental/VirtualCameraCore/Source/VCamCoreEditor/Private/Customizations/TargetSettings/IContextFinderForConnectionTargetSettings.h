// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class IPropertyUtilities;
struct FVCamConnection;

namespace UE::VCamCoreEditor::Private::ConnectionTargetContextFinding
{
	/**
	 * Used by FConnectionTargetSettingsTypeCustomization to find a FVCamConnection to limit the options of selectable
	 * targets according to the connection's constraints.
	 */
	class IContextFinderForConnectionTargetSettings : public TSharedFromThis<IContextFinderForConnectionTargetSettings>
	{
	public:

		/** Tries to find a context FVCamConnection if possible calls either ProcessWithContext or ProcessWithoutContext (but not both). */
		virtual void FindAndProcessContext(
			const TSharedRef<IPropertyHandle>& ConnectionTargetSettingsStructHandle, 
			IPropertyUtilities& PropertyUtils,
			TFunctionRef<void(const FVCamConnection& Connection)> ProcessWithContext,
			TFunctionRef<void()> ProcessWithoutContext
			) = 0;

		virtual ~IContextFinderForConnectionTargetSettings() = default;
	};
}