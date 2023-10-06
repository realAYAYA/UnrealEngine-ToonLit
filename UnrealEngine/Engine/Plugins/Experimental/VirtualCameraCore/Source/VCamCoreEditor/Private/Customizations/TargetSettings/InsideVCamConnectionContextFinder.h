// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IContextFinderForConnectionTargetSettings.h"

namespace UE::VCamCoreEditor::Private::ConnectionTargetContextFinding
{
	/** Figures out whether the FConnectionTargetSettings are contained in a FVCamConnection */
	class FInsideVCamConnectionContextFinder : public IContextFinderForConnectionTargetSettings
	{
	public:

		//~ Begin IContextFinderForConnectionTargetSettings Interface
		virtual void FindAndProcessContext(
			const TSharedRef<IPropertyHandle>& ConnectionTargetSettingsStructHandle, 
			IPropertyUtilities& PropertyUtils,
			TFunctionRef<void(const FVCamConnection& Connection)> ProcessWithContext,
			TFunctionRef<void()> ProcessWithoutContext
			) override;
		//~ End IContextFinderForConnectionTargetSettings Interface
	};
}
