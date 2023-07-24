// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IContextFinderForConnectionTargetSettings.h"

#include "Containers/Array.h"
#include "Templates/UnrealTemplate.h"

namespace UE::VCamCoreEditor::Private::ConnectionTargetContextFinding
{
	/** Goes through array of implementations and calls ProcessWithContext on the first one which finds a FVCamConnection (it is expected that only one will find a struct). */
	class FListOfContextFinders : public IContextFinderForConnectionTargetSettings
	{
	public:
		
		FListOfContextFinders(TArray<TSharedRef<IContextFinderForConnectionTargetSettings>> Implementations)
			: Implementations(MoveTemp(Implementations))
		{}

		//~ Begin IContextFinderForConnectionTargetSettings Interface
		virtual void FindAndProcessContext(
			const TSharedRef<IPropertyHandle>& ConnectionTargetSettingsStructHandle, 
			IPropertyUtilities& PropertyUtils,
			TFunctionRef<void(const FVCamConnection& Connection)> ProcessWithContext,
			TFunctionRef<void()> ProcessWithoutContext
			) override;
		//~ End IContextFinderForConnectionTargetSettings Interface
		
	private:

		TArray<TSharedRef<IContextFinderForConnectionTargetSettings>> Implementations;
	};
}