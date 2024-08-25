// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SwitchboardCredentialInterface.h"


namespace UE::SwitchboardListener::Private
{
	class FPosixCredentialManager : public ICredentialManager
	{
	public:
		virtual TSharedPtr<ICredential> LoadCredential(FStringView InCredentialName) override;
		virtual bool SaveCredential(FStringView InCredentialName, FStringView InUser, FStringView InBlob) override;
	};
};
