// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SwitchboardCredentialInterface.h"


namespace UE::SwitchboardListener::Private
{
	class FWindowsCredentialManager : public ICredentialManager
	{
	public:
		virtual bool IsEncryptedAtRest() const override { return true; }
		virtual TSharedPtr<ICredential> LoadCredential(FStringView InCredentialName) override;
		virtual bool SaveCredential(FStringView InCredentialName, FStringView InUser, FStringView InBlob) override;
	};
};


namespace UE::SwitchboardListener
{
	inline ICredentialManager& ICredentialManager::GetPlatformCredentialManager()
	{
		using namespace UE::SwitchboardListener::Private;
		static FWindowsCredentialManager CredentialManager;
		return CredentialManager;
	}
};
