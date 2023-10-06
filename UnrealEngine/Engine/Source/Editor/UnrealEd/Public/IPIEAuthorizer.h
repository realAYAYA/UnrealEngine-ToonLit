// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "HAL/Platform.h"
#include "UObject/NameTypes.h"

class FString;

class IPIEAuthorizer : public IModularFeature
{
public:
	virtual ~IPIEAuthorizer() {}

	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("PIEAuthorizer"));
		return FeatureName;
	}

	/**
	 * Used to gate play-in-editor functionality, where a PIE session may be 
	 * undesirable given some external plugin's state.
	 *
	 * @param  OutReason	[out] A string describing why play was denied by this plugin.
	 * @return False to block play-in-editor from continuing, true to allow.
	 */
	virtual bool RequestPIEPermission(bool bIsSimulateInEditor, FString& OutReason) const = 0;
};
