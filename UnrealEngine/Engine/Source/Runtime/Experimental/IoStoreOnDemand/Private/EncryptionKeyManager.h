// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Misc/AES.h"
#include "Misc/Guid.h"

namespace UE
{

/** Manages a set of registered encryption key(s). */
class FEncryptionKeyManager
{
public:
	FEncryptionKeyManager(const FEncryptionKeyManager&) = delete;
	FEncryptionKeyManager& operator=(const FEncryptionKeyManager&) = delete;
	~FEncryptionKeyManager();

	/** Returns whether the specified encrypton key exist or not. */
	bool ContainsKey(const FGuid& Id);
	/** Add a new encryption key, ignored if the key already exist. */
	void AddKey(const FGuid& Id, const FAES::FAESKey& Key);
	/** Returns the encryption key for the specified key ID. */
	FAES::FAESKey* GetKey(const FGuid& Id);
	/** Try retrieve the encryption key for the specified key ID. */
	bool TryGetKey(const FGuid& Id, FAES::FAESKey& OutKey);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FEncryptionKeyAddedDelegate, const FGuid&, const FAES::FAESKey&);
	/** Event triggered when a new key as been added. */
	FEncryptionKeyAddedDelegate& OnKeyAdded() { return KeyAdded; }
	/** Returns the single instance of the key manager. */
	static FEncryptionKeyManager& Get();

private:
	FEncryptionKeyManager();

	FCriticalSection CriticalSection;
	TMap<FGuid, FAES::FAESKey> Keys;
	FEncryptionKeyAddedDelegate KeyAdded;
};

} // namespace UE
