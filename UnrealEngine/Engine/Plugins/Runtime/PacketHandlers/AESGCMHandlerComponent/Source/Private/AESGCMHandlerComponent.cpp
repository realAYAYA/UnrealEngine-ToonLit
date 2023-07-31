// Copyright Epic Games, Inc. All Rights Reserved.

#include "AESGCMHandlerComponent.h"

#include "Net/Core/Connection/NetCloseResult.h"

IMPLEMENT_MODULE( FAESGCMHandlerComponentModule, AESGCMHandlerComponent )


const int32 FAESGCMHandlerComponent::KeySizeInBytes;
const int32 FAESGCMHandlerComponent::BlockSizeInBytes;
const int32 FAESGCMHandlerComponent::IVSizeInBytes;
const int32 FAESGCMHandlerComponent::AuthTagSizeInBytes;

TSharedPtr<HandlerComponent> FAESGCMHandlerComponentModule::CreateComponentInstance(FString& Options)
{
	return MakeShared<FAESGCMHandlerComponent>();
}


FAESGCMHandlerComponent::FAESGCMHandlerComponent()
	: FEncryptionComponent(FName(TEXT("AESGCMHandlerComponent")))
	, bEncryptionEnabled(false)
{
	EncryptionContext = IPlatformCrypto::Get().CreateContext();
}

void FAESGCMHandlerComponent::SetEncryptionData(const FEncryptionData& EncryptionData)
{
	if (EncryptionData.Key.Num() != KeySizeInBytes)
	{
		UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::SetEncryptionKey. NewKey is not %d bytes long, ignoring."), KeySizeInBytes);
		return;
	}

	Key.Reset(KeySizeInBytes);
	Key.Append(EncryptionData.Key.GetData(), EncryptionData.Key.Num());
}

void FAESGCMHandlerComponent::EnableEncryption()
{
	bEncryptionEnabled = true;
}

void FAESGCMHandlerComponent::DisableEncryption()
{
	bEncryptionEnabled = false;
}

bool FAESGCMHandlerComponent::IsEncryptionEnabled() const
{
	return bEncryptionEnabled;
}

void FAESGCMHandlerComponent::Initialize()
{
	SetActive(true);
	SetState(Handler::Component::State::Initialized);
	Initialized();
}

void FAESGCMHandlerComponent::InitFaultRecovery(UE::Net::FNetConnectionFaultRecoveryBase* InFaultRecovery)
{
	AESGCMFaultHandler.InitFaultRecovery(InFaultRecovery);
}

bool FAESGCMHandlerComponent::IsValid() const
{
	return true;
}

void FAESGCMHandlerComponent::Incoming(FIncomingPacketRef PacketRef)
{
	using namespace UE::Net;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("PacketHandler AESGCM Decrypt"), STAT_PacketHandler_AESGCM_Decrypt, STATGROUP_Net);

	FBitReader& Packet = PacketRef.Packet;
	FInPacketTraits& Traits = PacketRef.Traits;

	// Handle this packet
	if (IsValid() && Packet.GetNumBytes() > 0)
	{
		// Check first bit to see whether payload is encrypted
		if (Packet.ReadBit() != 0)
		{
			// If the key hasn't been set yet, we can't decrypt, so ignore this packet. We don't set an error in this case because it may just be an out-of-order packet.
			if (Key.Num() == 0)
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Incoming: received encrypted packet before key was set, ignoring."));
				Packet.SetData(nullptr, 0);
				return;
			}

			if (Packet.GetBytesLeft() >= IVSizeInBytes)
			{
				IV.Reset();
				IV.AddUninitialized(IVSizeInBytes);
				Packet.SerializeBits(IV.GetData(), IV.Num() * 8);
			}
			else
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Incoming: missing IV"));

				Packet.SetError();
				AddToChainResultPtr(Traits.ExtendedError, EAESGCMNetResult::AESMissingIV);

				return;
			}

			if (Packet.GetBytesLeft() >= AuthTagSizeInBytes)
			{
				AuthTag.Reset();
				AuthTag.AddUninitialized(AuthTagSizeInBytes);
				Packet.SerializeBits(AuthTag.GetData(), AuthTag.Num() * 8);
			}
			else
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Incoming: missing auth tag"));

				Packet.SetError();
				AddToChainResultPtr(Traits.ExtendedError, EAESGCMNetResult::AESMissingAuthTag);

				return;
			}

			// Copy remaining bits to a TArray so that they are byte-aligned.
			if (Packet.GetBytesLeft() > 0)
			{
				Ciphertext.Reset();
				Ciphertext.AddUninitialized(Packet.GetBytesLeft());
				Ciphertext[Ciphertext.Num() - 1] = 0;

				Packet.SerializeBits(Ciphertext.GetData(), Packet.GetBitsLeft());
			}
			else
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Incoming: missing ciphertext"));

				Packet.SetError();
				AddToChainResultPtr(Traits.ExtendedError, EAESGCMNetResult::AESMissingPayload);

				return;
			}

			UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("AESGCM packet handler received %ld bytes before decryption."), Ciphertext.Num());

			EPlatformCryptoResult DecryptResult = EPlatformCryptoResult::Failure;
			TArray<uint8> Plaintext = EncryptionContext->Decrypt_AES_256_GCM(Ciphertext, Key, IV, AuthTag, DecryptResult);

			if (DecryptResult == EPlatformCryptoResult::Failure)
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Incoming: failed to decrypt packet."));

				Packet.SetError();
				AddToChainResultPtr(Traits.ExtendedError, EAESGCMNetResult::AESDecryptionFailed);

				return;
			}

			if (Plaintext.Num() == 0)
			{
				Packet.SetData(nullptr, 0);
				return;
			}

			// Look for the termination bit that was written in Outgoing() to determine the exact bit size.
			uint8 LastByte = Plaintext.Last();

			if (LastByte != 0)
			{
				int32 BitSize = (Plaintext.Num() * 8) - 1;

				// Bit streaming, starts at the Least Significant Bit, and ends at the MSB.
				while (!(LastByte & 0x80))
				{
					LastByte *= 2;
					BitSize--;
				}

				UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("  Have %d bits after decryption."), BitSize);

				Packet.SetData(MoveTemp(Plaintext), BitSize);
			}
			else
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Incoming: malformed packet, last byte was 0."));

				Packet.SetError();
				AddToChainResultPtr(Traits.ExtendedError, EAESGCMNetResult::AESZeroLastByte);
			}
		}
	}
}

void FAESGCMHandlerComponent::Outgoing(FBitWriter& Packet, FOutPacketTraits& Traits)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("PacketHandler AESGCM Encrypt"), STAT_PacketHandler_AESGCM_Encrypt, STATGROUP_Net);

	// Handle this packet
	if (IsValid() && Packet.GetNumBytes() > 0)
	{
		// Allow for encryption enabled bit and termination bit. Allow resizing to account for encryption padding.
		FBitWriter NewPacket(Packet.GetNumBits() + 2, true);
		NewPacket.WriteBit(bEncryptionEnabled ? 1 : 0);

		if (NewPacket.IsError())
		{
			UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Outgoing: failed to write encryption bit."));
			Packet.SetError();
			return;
		}

		if (bEncryptionEnabled)
		{
			UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("AESGCM packet handler sending %ld bits before encryption."), Packet.GetNumBits());

			// Write a termination bit so that the receiving side can calculate the exact number of bits sent.
			// Same technique used in UNetConnection.
			Packet.WriteBit(1);

			if (Packet.IsError())
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Outgoing: failed to write termination bit."));
				return;
			}

			TArray<uint8> OutIV;
			OutIV.AddUninitialized(IVSizeInBytes);
			EPlatformCryptoResult RandResult = EncryptionContext->CreateRandomBytes(OutIV);
			if (RandResult == EPlatformCryptoResult::Failure)
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Outgoing: failed to generate IV."));
				Packet.SetError();
				return;
			}

			TArray<uint8> OutAuthTag;

			EPlatformCryptoResult EncryptResult = EPlatformCryptoResult::Failure;
			TArray<uint8> OutCiphertext = EncryptionContext->Encrypt_AES_256_GCM(TArrayView<uint8>(Packet.GetData(), Packet.GetNumBytes()), Key, OutIV, OutAuthTag, EncryptResult);

			if (EncryptResult == EPlatformCryptoResult::Failure)
			{
				UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Outgoing: failed to encrypt packet."));
				Packet.SetError();
				return;
			}
			else
			{
				NewPacket.Serialize(OutIV.GetData(), OutIV.Num());
				NewPacket.Serialize(OutAuthTag.GetData(), OutAuthTag.Num());
				NewPacket.Serialize(OutCiphertext.GetData(), OutCiphertext.Num());

				if (NewPacket.IsError())
				{
					UE_LOG(PacketHandlerLog, Log, TEXT("FAESGCMHandlerComponent::Outgoing: failed to write ciphertext to packet."));
					Packet.SetError();
					return;
				}

				UE_LOG(PacketHandlerLog, VeryVerbose, TEXT("  AESGCM packet handler sending %d bytes after encryption."), NewPacket.GetNumBytes());
			}
		}
		else
		{
			NewPacket.SerializeBits(Packet.GetData(), Packet.GetNumBits());
		}

		Packet = MoveTemp(NewPacket);
	}
}

int32 FAESGCMHandlerComponent::GetReservedPacketBits() const
{
	// Worst case includes the encryption enabled bit, the termination bit, padding up to the next whole byte, and a block of padding.
	// Now also includes initialization vector and auth tag
	return 2 + 7 + ((BlockSizeInBytes + IVSizeInBytes + AuthTagSizeInBytes) * 8);
}

void FAESGCMHandlerComponent::CountBytes(FArchive& Ar) const
{
	FEncryptionComponent::CountBytes(Ar);

	const SIZE_T SizeOfThis = sizeof(*this) - sizeof(FEncryptionComponent);
	Ar.CountBytes(SizeOfThis, SizeOfThis);

	/*
	Note, as of now, EncryptionContext is just typedef'd, but none of the base
	types actually allocated memory directly in their classes (although there may be
	global state).
	if (FEncryptionContext const * const LocalContext = EncrpytionContext.Get())
	{
		LocalContext->CountBytes(Ar);
	}
	*/

	Key.CountBytes(Ar);
	Ciphertext.CountBytes(Ar);
}
