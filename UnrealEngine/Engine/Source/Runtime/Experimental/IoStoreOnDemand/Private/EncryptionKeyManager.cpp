// Copyright Epic Games, Inc. All Rights Reserved.

#include "EncryptionKeyManager.h"
#include "Misc/CoreDelegates.h"

namespace UE
{

FEncryptionKeyManager::FEncryptionKeyManager()
{
	if (FCoreDelegates::GetPakEncryptionKeyDelegate().IsBound())
	{
		FAES::FAESKey Key;
		FCoreDelegates::GetPakEncryptionKeyDelegate().Execute(Key.Key);

		if (Key.IsValid())
		{
			AddKey(FGuid(), Key);
		}
	}

	FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().AddRaw(this, &FEncryptionKeyManager::AddKey);
}

FEncryptionKeyManager::~FEncryptionKeyManager()
{
	FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().RemoveAll(this);
}

bool FEncryptionKeyManager::ContainsKey(const FGuid& Id)
{
	return nullptr != GetKey(Id);
}

void FEncryptionKeyManager::AddKey(const FGuid& Id, const FAES::FAESKey& Key)
{
	bool bAdded = false;
	{
		FScopeLock _(&CriticalSection);

		if (!Keys.Contains(Id))
		{
			Keys.Add(Id, Key);
			bAdded = true;
		}
	}

	if (bAdded && KeyAdded.IsBound())
	{
		KeyAdded.Broadcast(Id, Key);
	}
}

FAES::FAESKey* FEncryptionKeyManager::GetKey(const FGuid& Id)
{
	FScopeLock _(&CriticalSection);
	return Keys.Find(Id);
}

bool FEncryptionKeyManager::TryGetKey(const FGuid& Id, FAES::FAESKey& OutKey)
{
	if (FAES::FAESKey* Key = GetKey(Id))
	{
		OutKey = *Key;
		return true;
	}

	return false;
}

FEncryptionKeyManager& FEncryptionKeyManager::Get()
{
	static FEncryptionKeyManager Mgr;
	return Mgr;
}

} // namespace UE
