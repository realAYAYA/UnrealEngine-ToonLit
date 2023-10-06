// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"
#include "CryptoPP/5.6.5/include/aes.h"

/* AES Block Encryptor Module Interface */
class UE_DEPRECATED(5.3, "This component is not supported for encryption.")
FAESBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};

/*
* AES Block encryption
*/
class UE_DEPRECATED(5.3, "This component is not supported for encryption.")
AESBLOCKENCRYPTOR_API AESBlockEncryptor : public BlockEncryptor
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
	CryptoPP::AES::Encryption Encryptor;
	CryptoPP::AES::Decryption Decryptor;
};