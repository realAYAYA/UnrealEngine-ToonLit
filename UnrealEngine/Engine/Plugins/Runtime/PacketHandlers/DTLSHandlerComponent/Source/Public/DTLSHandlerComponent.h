// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PacketHandler.h"
#include "EncryptionComponent.h"
#include "UObject/CoreNet.h"
#include "DTLSContext.h"

extern TAutoConsoleVariable<int32> CVarPreSharedKeys;

/*
* DTLS encryption component.
*/
class DTLSHANDLERCOMPONENT_API FDTLSHandlerComponent : public FEncryptionComponent
{
public:
	FDTLSHandlerComponent();
	virtual ~FDTLSHandlerComponent();

	virtual void SetEncryptionData(const FEncryptionData& EncryptionData) override;

	// After calling this, future outgoing packets will be encrypted (until a call to DisableEncryption).
	virtual void EnableEncryption() override;

	// After calling this, future outgoing packets will not be encrypted (until a call to DisableEncryption).
	virtual void DisableEncryption() override;

	// Returns true if encryption is currently enabled.
	virtual bool IsEncryptionEnabled() const override;

	// HandlerComponent interface
	virtual void Initialize() override;
	virtual bool IsValid() const override;
	virtual void Incoming(FBitReader& Packet) override;
	virtual void Outgoing(FBitWriter& Packet, FOutPacketTraits& Traits) override;
	virtual int32 GetReservedPacketBits() const override;
	virtual void CountBytes(FArchive& Ar) const override;

	virtual void Tick(float DeltaTime) override;

	enum class EDTLSHandlerState
	{
		Unencrypted,
		Handshaking,
		Encrypted,
	};

	// Retrieve pre shared key, if set
	const FDTLSPreSharedKey* GetPreSharedKey() const { return PreSharedKey.Get(); }

	// Retrieve expected remote certificate fingerprint, if set
	const FDTLSFingerprint* GetRemoteFingerprint() const { return RemoteFingerprint.Get(); }

private:
	// Process DTLS handshake
	void TickHandshake();
	void DoHandshake();

	void LogError(const TCHAR* Context, int32 Result);

private:
	EDTLSHandlerState InternalState;

	TUniquePtr<FDTLSContext> DTLSContext;
	TUniquePtr<FDTLSPreSharedKey> PreSharedKey;
	TUniquePtr<FDTLSFingerprint> RemoteFingerprint;

	FString CertId;

	uint8 TempBuffer[MAX_PACKET_SIZE];

	bool bPendingHandshakeData;
};

/**
 * The public interface to this module.
 */
class FDTLSHandlerComponentModule : public FPacketHandlerComponentModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
