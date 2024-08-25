// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_UNSUPPORTED - Includes JsonSerializer.h which is not in Core module

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AES.h"
#include "Misc/IEngineCrypto.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Base64.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonSerializer.h"
#include "RSA.h"

struct FNamedAESKey
{
	FString Name;
	FGuid Guid;
	FAES::FAESKey Key;

	bool IsValid() const
	{
		return Key.IsValid();
	}
};

struct FKeyChain
{
public:

	FKeyChain() = default;

	FKeyChain(const FKeyChain& Other)
	{
		SetSigningKey(Other.GetSigningKey());
		SetEncryptionKeys(Other.GetEncryptionKeys());

		if (Other.GetPrincipalEncryptionKey())
		{
			SetPrincipalEncryptionKey(GetEncryptionKeys().Find(Other.GetPrincipalEncryptionKey()->Guid));
		}
	}
	
	FKeyChain(FKeyChain&& Other)
	{
		SetSigningKey(Other.GetSigningKey());
		SetEncryptionKeys(MoveTemp(Other.GetEncryptionKeys()));

		if (Other.GetPrincipalEncryptionKey())
		{
			SetPrincipalEncryptionKey(GetEncryptionKeys().Find(Other.GetPrincipalEncryptionKey()->Guid));
		}
		
		Other.SetSigningKey(InvalidRSAKeyHandle);
		Other.SetPrincipalEncryptionKey(nullptr);
		Other.SetEncryptionKeys(TMap<FGuid, FNamedAESKey>());
	}

	FKeyChain& operator=(const FKeyChain& Other)
	{
		SetSigningKey(Other.GetSigningKey());
		SetEncryptionKeys(Other.GetEncryptionKeys());
		
		if (Other.GetPrincipalEncryptionKey())
		{
			SetPrincipalEncryptionKey(GetEncryptionKeys().Find(Other.GetPrincipalEncryptionKey()->Guid));
		}
		else
		{
			SetPrincipalEncryptionKey(nullptr);
		}

		return *this;
	}

	FKeyChain& operator=(FKeyChain&& Other)
	{
		SetSigningKey(Other.GetSigningKey());
		SetEncryptionKeys(MoveTemp(Other.GetEncryptionKeys()));
		
		if (Other.GetPrincipalEncryptionKey())
		{
			SetPrincipalEncryptionKey(GetEncryptionKeys().Find(Other.GetPrincipalEncryptionKey()->Guid));
		}
		else
		{
			SetPrincipalEncryptionKey(nullptr);
		}

		Other.SetSigningKey(InvalidRSAKeyHandle);
		Other.SetPrincipalEncryptionKey(nullptr);
		Other.SetEncryptionKeys(TMap<FGuid, FNamedAESKey>());

		return *this;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FRSAKeyHandle GetSigningKey() const { return SigningKey; }
	void SetSigningKey(FRSAKeyHandle key) { SigningKey = key; }

	const FNamedAESKey* GetPrincipalEncryptionKey() const { return MasterEncryptionKey; }
	void SetPrincipalEncryptionKey(const FNamedAESKey* key) { MasterEncryptionKey =key; }

	const TMap<FGuid, FNamedAESKey>& GetEncryptionKeys() const { return EncryptionKeys; }
	TMap<FGuid, FNamedAESKey>& GetEncryptionKeys() { return EncryptionKeys; }

	void SetEncryptionKeys(const TMap<FGuid, FNamedAESKey>& keys) { EncryptionKeys = keys; }
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.1, "Use Get/SetSigningKey instead")
	FRSAKeyHandle SigningKey = InvalidRSAKeyHandle;

	UE_DEPRECATED(5.1, "Use GetEncryptionKeys instead")
	TMap<FGuid, FNamedAESKey> EncryptionKeys;
	
	UE_DEPRECATED(5.1, "Use Get/SetPrincipalEncryptionKey instead")
	const FNamedAESKey* MasterEncryptionKey = nullptr;
};


namespace KeyChainUtilities
{
	static FRSAKeyHandle ParseRSAKeyFromJson(TSharedPtr<FJsonObject> InObj)
	{
		TSharedPtr<FJsonObject> PublicKey = InObj->GetObjectField(TEXT("PublicKey"));
		TSharedPtr<FJsonObject> PrivateKey = InObj->GetObjectField(TEXT("PrivateKey"));

		FString PublicExponentBase64, PrivateExponentBase64, PublicModulusBase64, PrivateModulusBase64;

		if (PublicKey->TryGetStringField(TEXT("Exponent"), PublicExponentBase64)
			&& PublicKey->TryGetStringField(TEXT("Modulus"), PublicModulusBase64)
			&& PrivateKey->TryGetStringField(TEXT("Exponent"), PrivateExponentBase64)
			&& PrivateKey->TryGetStringField(TEXT("Modulus"), PrivateModulusBase64))
		{
			check(PublicModulusBase64 == PrivateModulusBase64);

			TArray<uint8> PublicExponent, PrivateExponent, Modulus;
			FBase64::Decode(PublicExponentBase64, PublicExponent);
			FBase64::Decode(PrivateExponentBase64, PrivateExponent);
			FBase64::Decode(PublicModulusBase64, Modulus);

			return FRSA::CreateKey(PublicExponent, PrivateExponent, Modulus);
		}
		else
		{
			return nullptr;
		}
	}

	static void LoadKeyChainFromFile(const FString& InFilename, FKeyChain& OutCryptoSettings)
	{
		FArchive* File = IFileManager::Get().CreateFileReader(*InFilename);
		checkf(File != nullptr, TEXT("Specified crypto keys cache '%s' does not exist!"), *InFilename);
		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<UTF8CHAR>> Reader = TJsonReaderFactory<UTF8CHAR>::Create(File);
		if (FJsonSerializer::Deserialize(Reader, RootObject))
		{
			const TSharedPtr<FJsonObject>* EncryptionKeyObject;
			if (RootObject->TryGetObjectField(TEXT("EncryptionKey"), EncryptionKeyObject))
			{
				FString EncryptionKeyBase64;
				if ((*EncryptionKeyObject)->TryGetStringField(TEXT("Key"), EncryptionKeyBase64))
				{
					if (EncryptionKeyBase64.Len() > 0)
					{
						TArray<uint8> Key;
						FBase64::Decode(EncryptionKeyBase64, Key);
						check(Key.Num() == sizeof(FAES::FAESKey::Key));
						FNamedAESKey NewKey;
						NewKey.Name = TEXT("Default");
						NewKey.Guid = FGuid();
						FMemory::Memcpy(NewKey.Key.Key, &Key[0], sizeof(FAES::FAESKey::Key));
						OutCryptoSettings.GetEncryptionKeys().Add(NewKey.Guid, NewKey);
					}
				}
			}

			const TSharedPtr<FJsonObject>* SigningKey = nullptr;
			if (RootObject->TryGetObjectField(TEXT("SigningKey"), SigningKey))
			{
				OutCryptoSettings.SetSigningKey(ParseRSAKeyFromJson(*SigningKey));
			}

			const TArray<TSharedPtr<FJsonValue>>* SecondaryEncryptionKeyArray = nullptr;
			if (RootObject->TryGetArrayField(TEXT("SecondaryEncryptionKeys"), SecondaryEncryptionKeyArray))
			{
				for (TSharedPtr<FJsonValue> EncryptionKeyValue : *SecondaryEncryptionKeyArray)
				{
					FNamedAESKey NewKey;
					TSharedPtr<FJsonObject> SecondaryEncryptionKeyObject = EncryptionKeyValue->AsObject();
					FGuid::Parse(SecondaryEncryptionKeyObject->GetStringField(TEXT("Guid")), NewKey.Guid);
					NewKey.Name = SecondaryEncryptionKeyObject->GetStringField(TEXT("Name"));
					FString KeyBase64 = SecondaryEncryptionKeyObject->GetStringField(TEXT("Key"));

					TArray<uint8> Key;
					FBase64::Decode(KeyBase64, Key);
					check(Key.Num() == sizeof(FAES::FAESKey::Key));
					FMemory::Memcpy(NewKey.Key.Key, &Key[0], sizeof(FAES::FAESKey::Key));

					check(!OutCryptoSettings.GetEncryptionKeys().Contains(NewKey.Guid) || OutCryptoSettings.GetEncryptionKeys()[NewKey.Guid].Key == NewKey.Key);
					OutCryptoSettings.GetEncryptionKeys().Add(NewKey.Guid, NewKey);
				}
			}
		}
		delete File;
		FGuid EncryptionKeyOverrideGuid;
		OutCryptoSettings.SetPrincipalEncryptionKey(OutCryptoSettings.GetEncryptionKeys().Find(EncryptionKeyOverrideGuid));
	}

	static void ApplyEncryptionKeys(const FKeyChain& KeyChain)
	{
		if (KeyChain.GetEncryptionKeys().Contains(FGuid()))
		{
			FAES::FAESKey DefaultKey = KeyChain.GetEncryptionKeys()[FGuid()].Key;
			FCoreDelegates::GetPakEncryptionKeyDelegate().BindLambda([DefaultKey](uint8 OutKey[32]) { FMemory::Memcpy(OutKey, DefaultKey.Key, sizeof(DefaultKey.Key)); });
		}

		for (const TMap<FGuid, FNamedAESKey>::ElementType& Key : KeyChain.GetEncryptionKeys())
		{
			if (Key.Key.IsValid())
			{
				FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(Key.Key, Key.Value.Key);
			}
		}
	}
}
