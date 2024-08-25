// Copyright Epic Games, Inc. All Rights Reserved.

#include "ListOfContextFinders.h"

#include "IPropertyUtilities.h"
#include "PropertyHandle.h"

namespace UE::VCamCoreEditor::Private::ConnectionTargetContextFinding
{
	void FListOfContextFinders::FindAndProcessContext(
		const TSharedRef<IPropertyHandle>& ConnectionTargetSettingsStructHandle,
		IPropertyUtilities& PropertyUtils,
		TFunctionRef<void(const FVCamConnection& Connection)> ProcessWithContext,
		TFunctionRef<void()> ProcessWithoutContext)
	{
		bool bFoundVCamConnection = false;
		auto ProcessWithContextWrapper = [ProcessWithContext, &bFoundVCamConnection](const FVCamConnection& Connection) mutable
		{
			bFoundVCamConnection = true;
			ProcessWithContext(Connection);
		};
		auto DoNothing = [](){};
		
		for (int32 i = 0; i < Implementations.Num() && !bFoundVCamConnection; ++i)
		{
			const TSharedRef<IContextFinderForConnectionTargetSettings>& Implementation = Implementations[i];
			Implementation->FindAndProcessContext(ConnectionTargetSettingsStructHandle, PropertyUtils, ProcessWithContextWrapper, DoNothing);
		}

		if (!bFoundVCamConnection)
		{
			ProcessWithoutContext();
		}
	}
}
