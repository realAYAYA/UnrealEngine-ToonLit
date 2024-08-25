// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreGlobals.h"
#include "Templates/SharedPointer.h"

#if WITH_EOS_SDK
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif

#include "eos_common.h"
#endif

// Expect URLs to look like "EOS:PUID:SocketName:Channel"
#define EOS_CONNECTION_URL_PREFIX TEXT("EOS")
#define EOS_URL_SEPARATOR TEXT(":")

/** Used to store a pointer to the EOS callback object without knowing type */
class EOSSHARED_API FCallbackBase
{
public:
	virtual ~FCallbackBase() {}
};

#if WITH_EOS_SDK

/** Wrapper for EOS notification callbacks, ensures calling object is still alive before calling passed lambda. */
template<typename CallbackFuncType, typename CallbackParamType, typename OwningType, typename CallbackReturnType = void, typename... CallbackExtraParams>
class TEOSGlobalCallback :
	public FCallbackBase
{
public:
	TFunction<CallbackReturnType(const CallbackParamType*, CallbackExtraParams... ExtraParams)> CallbackLambda;

	TEOSGlobalCallback(TWeakPtr<OwningType> InOwner)
		: FCallbackBase()
		, Owner(InOwner)
	{
	}
	virtual ~TEOSGlobalCallback() = default;

	CallbackFuncType GetCallbackPtr()
	{
		return &CallbackImpl;
	}

	/** Is this callback intended for the game thread */
	bool bIsGameThreadCallback = true;

private:
	/** The object that needs to be checked for lifetime before calling the callback */
	TWeakPtr<OwningType> Owner;

	static CallbackReturnType EOS_CALL CallbackImpl(const CallbackParamType* Data, CallbackExtraParams... ExtraParams)
	{
		TEOSGlobalCallback* CallbackThis = (TEOSGlobalCallback*)Data->ClientData;
		check(CallbackThis);

		if (CallbackThis->bIsGameThreadCallback)
		{
			check(IsInGameThread());
		}

		TSharedPtr <OwningType> Pinned = CallbackThis->Owner.Pin();
		if (Pinned.IsValid())
		{
			check(CallbackThis->CallbackLambda);

			if constexpr (std::is_void<CallbackReturnType>::value)
			{
				CallbackThis->CallbackLambda(Data, ExtraParams...);
			}
			else
			{
				return CallbackThis->CallbackLambda(Data, ExtraParams...);
			}
		}
		
		if constexpr (!std::is_void<CallbackReturnType>::value)
		{
			// we need to return _something_ to compile.
			return CallbackReturnType{};
		}
	}
};

/** Wrapper for EOS async API callbacks, ensures the caller is still alive before calling passed lambda, then deletes itself. */
template<typename CallbackFuncType, typename CallbackType, typename OwningType>
class TEOSCallback :
	public FCallbackBase
{
public:
	TFunction<void(const CallbackType*)> CallbackLambda;

	TEOSCallback(TWeakPtr<OwningType> InOwner)
		: FCallbackBase()
		, Owner(InOwner)
	{
	}
	TEOSCallback(TWeakPtr<const OwningType> InOwner)
		: FCallbackBase()
		, Owner(InOwner)
	{
	}
	virtual ~TEOSCallback() = default;

	CallbackFuncType GetCallbackPtr()
	{
		return &CallbackImpl;
	}

protected:
	/** The object that needs to be checked for lifetime before calling the callback */
	TWeakPtr<const OwningType> Owner;

private:
	static void EOS_CALL CallbackImpl(const CallbackType* Data)
	{
		if (EOS_EResult_IsOperationComplete(Data->ResultCode) == EOS_FALSE)
		{
			// Ignore
			return;
		}
		check(IsInGameThread());

		TEOSCallback* CallbackThis = (TEOSCallback*)Data->ClientData;
		check(CallbackThis);

		if (CallbackThis->Owner.IsValid())
		{
			check(CallbackThis->CallbackLambda);
			CallbackThis->CallbackLambda(Data);
		}
		delete CallbackThis;
	}
};

#endif