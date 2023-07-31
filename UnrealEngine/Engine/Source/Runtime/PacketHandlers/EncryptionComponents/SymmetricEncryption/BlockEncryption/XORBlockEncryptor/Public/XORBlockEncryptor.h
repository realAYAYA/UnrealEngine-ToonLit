// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlockEncryptionHandlerComponent.h"

/* XOR Block Encryptor Module Interface */
UE_DEPRECATED(5.0, "This Encryptor Is Now Deprecated");
class FXORBlockEncryptorModuleInterface : public FBlockEncryptorModuleInterface
{
	virtual BlockEncryptor* CreateBlockEncryptorInstance() override;
};

/*
* XOR Block encryption
*/
UE_DEPRECATED(5.0, "This Encryptor Is Now Deprecated");
class XORBLOCKENCRYPTOR_API XORBlockEncryptor : public BlockEncryptor
{
public:
	/* Initialized the encryptor */
	void Initialize(TArray<uint8>* Key) override;

	/* Encrypts outgoing packets */
	void EncryptBlock(uint8* Block) override;

	/* Decrypts incoming packets */
	void DecryptBlock(uint8* Block) override;

	/* Get the default key size for this encryptor */
	uint32 GetDefaultKeySize() { return 4; }
};
