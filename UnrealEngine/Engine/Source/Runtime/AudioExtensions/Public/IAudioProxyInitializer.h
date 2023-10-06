// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"
#include "Misc/CoreMiscDefines.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

#include <type_traits>

/**
 * Interfaces for Audio Proxy Objects 
 * These are used to spawn thread safe instances of UObjects that may be garbage collected on the game thread.
 * In shipping builds, these are effectively abstract pointers, but CHECK_AUDIOPROXY_TYPES can optionally be used
 * to check downcasts.
 */

#define  IMPL_AUDIOPROXY_CLASS(FClassName) \
	static FName GetAudioProxyTypeName() \
	{ \
		static FName MyClassName = #FClassName; \
		return MyClassName; \
	} \
	static constexpr bool bWasAudioProxyClassImplemented = true; \
	friend class ::Audio::IProxyData; \
	friend class ::Audio::TProxyData<FClassName>;


namespace Audio
{
	// Forward Declarations
	class IProxyData;
	using IProxyDataPtr UE_DEPRECATED(5.2, "Replace IProxyDataPtr with TSharedPtr<Audio::IProxyData>") = TUniquePtr<Audio::IProxyData>;

	/*
	 * Base class that allows us to typecheck proxy data before downcasting it in debug builds.
	*/
	class IProxyData
	{
	private:
		FName ProxyTypeName;
	public:
		virtual ~IProxyData() = default;

		template<typename ProxyType>
		bool CheckTypeCast() const
		{
			const FName DestinationTypeName = ProxyType::GetAudioProxyTypeName();
			return ensureAlwaysMsgf(ProxyTypeName == DestinationTypeName, TEXT("Tried to downcast type %s to %s!"), *ProxyTypeName.ToString(), *DestinationTypeName.ToString());
		}

		FName GetProxyTypeName() const
		{
			return ProxyTypeName;
		}

		template<typename ProxyType>
		ProxyType& GetAs()
		{
			static_assert(std::is_base_of_v<IProxyData, ProxyType>, "Tried to downcast IProxyInitData to an unrelated type!");
			if (CheckTypeCast<ProxyType>())
			{
				return static_cast<ProxyType&>(*this);
			}
			else
			{
				// This is an illegal cast, and is considered a fatal error.
				checkNoEntry();
				return *((ProxyType*)0x1);
			}
		}

		template<typename ProxyType>
		const ProxyType& GetAs() const
		{
			static_assert(std::is_base_of_v<IProxyData, ProxyType>, "Tried to downcast IProxyInitData to an unrelated type!");
			if (CheckTypeCast<ProxyType>())
			{
				return static_cast<const ProxyType&>(*this);
			}
			else
			{
				// This is an illegal cast, and is considered a fatal error.
				checkNoEntry();
				return *((ProxyType*)0x1);
			}
		}

		IProxyData(FName InProxyTypeName)
			: ProxyTypeName(InProxyTypeName)
		{}

		UE_DEPRECATED(5.2, "Proxy data is stored in a TSharedPtr<> and no longer requires cloning")
		virtual TUniquePtr<IProxyData> Clone() const { return nullptr; }
	};

	/**
	 * This class can be implemented to create a custom, threadsafe instance of a given UObject.
	 * This is a CRTP class, and should always be subclassed with the name of the subclass.
	 */
	template <typename Type>
	class TProxyData : public IProxyData
	{
	protected:
		static constexpr bool bWasAudioProxyClassImplemented = false;

	public:
		TProxyData()
			: IProxyData(Type::GetAudioProxyTypeName())
		{
			static_assert(Type::bWasAudioProxyClassImplemented, "Make sure to include IMPL_AUDIOPROXY_CLASS(ClassName) in your implementation of TProxyData.");
		}
	};

	struct FProxyDataInitParams
	{
		FName NameOfFeatureRequestingProxy;
	};
} // namespace Audio

/*
* This can be subclassed to make a UClass an audio proxy factory.
*/
class IAudioProxyDataFactory
{
public:
	UE_DEPRECATED(5.2, "Call TSharedPtr<Audio::IProxyData> CreateProxyData(...) instead of a TUniquePtr<Audio::IProxyData> CreateNewProxyData(...).")
	AUDIOEXTENSIONS_API virtual TUniquePtr<Audio::IProxyData> CreateNewProxyData(const Audio::FProxyDataInitParams& InitParams);

	AUDIOEXTENSIONS_API virtual TSharedPtr<Audio::IProxyData> CreateProxyData(const Audio::FProxyDataInitParams& InitParams);
};

namespace Audio
{
	template <typename UClassToUse>
	IAudioProxyDataFactory* CastToProxyDataFactory(UObject* InObject)
	{
		if constexpr (std::is_base_of_v<IAudioProxyDataFactory, UClassToUse>)
		{
			if (InObject)
			{
				UClassToUse* DowncastObject = Cast<UClassToUse>(InObject);
				if (ensureAlways(DowncastObject))
				{
					return static_cast<IAudioProxyDataFactory*>(DowncastObject);
				}
			}
		}

		return nullptr;
	}
} // namespace Audio
