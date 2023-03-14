// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemOculusPackage.h"
#include "OVR_Platform.h"

using FUniqueNetIdOculusPtr = TSharedPtr<const class FUniqueNetIdOculus>;
using FUniqueNetIdOculusRef = TSharedRef<const class FUniqueNetIdOculus>;

class FUniqueNetIdOculus : public FUniqueNetId {
private:
	ovrID ID;

protected:
	bool Compare(const FUniqueNetId& Other) const override
	{
		if (Other.GetType() != GetType())
		{
			return false;
		}

		if (Other.GetSize() != sizeof(ovrID))
		{
			return false;
		}

		return ID == static_cast<const FUniqueNetIdOculus&>(Other).ID;
	}

public:
	template<typename... TArgs>
	static FUniqueNetIdOculusRef Create(TArgs&&... Args)
	{
		return MakeShareable(new FUniqueNetIdOculus(Forward<TArgs>(Args)...));
	}

	static const FUniqueNetIdOculus& Cast(const FUniqueNetId& NetId)
	{
		check(NetId.GetType() == OCULUS_SUBSYSTEM);
		return *static_cast<const FUniqueNetIdOculus*>(&NetId);
	}

	FUniqueNetIdOculusRef AsShared() const
	{
		return StaticCastSharedRef<const FUniqueNetIdOculus>(FUniqueNetId::AsShared());
	}

	virtual FName GetType() const override
	{
		return OCULUS_SUBSYSTEM;
	}

	// IOnlinePlatformData

	virtual const uint8* GetBytes() const override
	{
		return reinterpret_cast<const uint8*>(&ID);
	}

	virtual int32 GetSize() const override
	{
		return sizeof(ID);
	}

	virtual bool IsValid() const override
	{
		// Not completely accurate, but safe to assume numbers below this is invalid
		return ID > 100000;
	}

	ovrID GetID() const
	{
		return ID;
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("%llu"), ID);
	}

	virtual FString ToDebugString() const override
	{
		const FString UniqueNetIdStr = FString::Printf(TEXT("%llu"), ID);
		return TEXT("ovrID:") + OSS_UNIQUEID_REDACT(*this, UniqueNetIdStr);
	}

	virtual uint32 GetTypeHash() const override
	{
		return ::GetTypeHash((uint64)ID);
	}

	/** global static instance of invalid (zero) id */
	static const FUniqueNetIdOculusRef& EmptyId()
	{
		static const FUniqueNetIdOculusRef EmptyId(Create());
		return EmptyId;
	}

private:
	/** Default constructor */
	FUniqueNetIdOculus()
	{
		ID = 0;
	}

	FUniqueNetIdOculus(const ovrID& id)
	{
		ID = id;
	}

	FUniqueNetIdOculus(const FString& id)
	{
		ovrID_FromString(&ID, TCHAR_TO_ANSI(*id));
	}

	/**
	* Copy Constructor
	*
	* @param Src the id to copy
	*/
	explicit FUniqueNetIdOculus(const FUniqueNetId& Src)
	{
		if (Src.GetType() == OCULUS_SUBSYSTEM)
		{
			ID = FUniqueNetIdOculus::Cast(Src).ID;
		}
	}
};

/**
* Implementation of session information
*/
class FOnlineSessionInfoOculus : public FOnlineSessionInfo
{
protected:

	/** Hidden on purpose */
	FOnlineSessionInfoOculus(const FOnlineSessionInfoOculus& Src) = delete;
	FOnlineSessionInfoOculus& operator=(const FOnlineSessionInfoOculus& Src) = delete;

PACKAGE_SCOPE:

	FOnlineSessionInfoOculus(ovrID RoomId);

	/** Unique Id for this session */
	FUniqueNetIdOculusRef SessionId;

public:

	virtual ~FOnlineSessionInfoOculus() {}

	bool operator==(const FOnlineSessionInfoOculus& Other) const
	{
		return *Other.SessionId == *SessionId;
	}

	virtual const uint8* GetBytes() const override
	{
		return nullptr;
	}

	virtual int32 GetSize() const override
	{
		return 0;
	}

	virtual bool IsValid() const override
	{
		return true;
	}

	virtual FString ToString() const override
	{
		return SessionId->ToString();
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("SessionId: %s"), *SessionId->ToDebugString());
	}

	virtual const FUniqueNetId& GetSessionId() const override
	{
		return *SessionId;
	}
};
