// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"
#include "CryptoPP/5.6.5/include/twofish.h"

/* TwoFish Block Encryptor Module Interface */
UE_DEPRECATED(5.0, "This Encryptor Is Now Deprecated");
class FTwoFishBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};

/*
* TwoFish Block encryption
*/
UE_DEPRECATED(5.0, "This Encryptor Is Now Deprecated");
class TWOFISHBLOCKENCRYPTOR_API TwoFishBlockEncryptor : public BlockEncryptor
{
public:
	/* Initialized the encryptor */
	void Initialize(TArray<uint8>* Key) override;

	/* Encrypts outgoing packets */
	void EncryptBlock(uint8* Block) override;

	/* Decrypts incoming packets */
	void DecryptBlock(uint8* Block) override;

	/* Get the default key size for this encryptor */
	uint32 GetDefaultKeySize() { return 16; }

private:
	/* Encryptors for AES */
	CryptoPP::Twofish::Encryption Encryptor;
	CryptoPP::Twofish::Decryption Decryptor;
};