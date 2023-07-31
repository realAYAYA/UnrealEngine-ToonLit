// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PacketHandler.h"
#include "IPlatformCrypto.h"
#include "EncryptionComponent.h"
#include "AESGCMFaultHandler.h"

/*
* AES256 GCM block encryption component.
*/
class AESGCMHANDLERCOMPONENT_API FAESGCMHandlerComponent : public FEncryptionComponent
{
public:
	/**
	 * Default constructor that leaves the Key empty, and encryption disabled.
	 * You must set the key before enabling encryption, or before receiving encrypted
	 * packets, or those operations will fail.
	 */
	FAESGCMHandlerComponent();

	// This handler uses AES256, which has 32-byte keys.
	static const int32 KeySizeInBytes = 32;

	// This handler uses AES256, which has 32-byte keys.
	static const int32 BlockSizeInBytes = 16;

	static const int32 IVSizeInBytes = 12;
	static const int32 AuthTagSizeInBytes = 16;

	// Replace the key used for encryption with NewKey if NewKey is exactly KeySizeInBytes long.
	virtual void SetEncryptionData(const FEncryptionData& EncryptionData) override;

	// After calling this, future outgoing packets will be encrypted (until a call to DisableEncryption).
	virtual void EnableEncryption() override;

	// After calling this, future outgoing packets will not be encrypted (until a call to DisableEncryption).
	virtual void DisableEncryption() override;

	// Returns true if encryption is currently enabled.
	virtual bool IsEncryptionEnabled() const override;

	// HandlerComponent interface
	virtual void Initialize() override;
	virtual void InitFaultRecovery(UE::Net::FNetConnectionFaultRecoveryBase* InFaultRecovery) override;
	virtual bool IsValid() const override;
	virtual void Incoming(FIncomingPacketRef PacketRef) override;
	virtual void Outgoing(FBitWriter& Packet, FOutPacketTraits& Traits) override;
	virtual int32 GetReservedPacketBits() const override;
	virtual void CountBytes(FArchive& Ar) const override;

private:
	TUniquePtr<FEncryptionContext> EncryptionContext;

	TArray<uint8> Key;
	
	// Avoid per packet allocations
	TArray<uint8> Ciphertext;
	TArray<uint8> IV;
	TArray<uint8> AuthTag;

	bool bEncryptionEnabled;

	/** Fault handler for AESGCM-specific errors, that may trigger NetConnection Close */
	FAESGCMFaultHandler AESGCMFaultHandler;
};


/**
 * The public interface to this module.
 */
class FAESGCMHandlerComponentModule : public FPacketHandlerComponentModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
