// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GenericPlatform/GenericPlatformSecuritySandbox.h"

class FWindowsPlatformSecuritySandbox : public FGenericPlatformSecuritySandbox
{
public:
	virtual ~FWindowsPlatformSecuritySandbox() {}
	virtual void PlatformRestrictSelf() override;

private:
	void SetLowIntegrityLevel();
	void SetProcessMitigations();
	void ApplyJobRestrictions();

	void CustomHandlerForURL(const FString& URL, FString* ErrorMsg);

	typedef FGenericPlatformSecuritySandbox Super;

	bool bNeedsCustomHandlerForURL = false;
};
