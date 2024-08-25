// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

/**
* Use of curiously recursive template pattern (CRTP) to dispatch GetRangeImpl and SetRangeImpl at compile time.
* Override all virtual functions for the supported types and will handle the conversion between
* "U" the incoming type and "T" the underlying type.
* 
* Class that inherit this one needs to define:
* -> bool GetRangeImpl(TArrayView<T>& OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys) const
* -> bool SetRangeImpl(TArrayView<const T>& InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags)
* -> The underlying type: (using Type = ...);
*/
template <typename Derived>
class IPCGAttributeAccessorT : public IPCGAttributeAccessor
{
protected:
	IPCGAttributeAccessorT(bool bInReadOnly)
		: IPCGAttributeAccessor(bInReadOnly, /*UnderlyingType=*/ PCG::Private::MetadataTypes<typename Derived::Type>::Id)
	{}

	// Override all virtual calls to call our templated method.
#define IACCESSOR_DECL(T) \
	virtual bool GetRange##T(TArrayView<T>& OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) const override { return InternalGetRange(OutValues, Index, Keys, Flags); } \
	virtual bool SetRange##T(TArrayView<const T>& InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) override { return InternalSetRange(InValues, Index, Keys, Flags); }
	PCG_FOREACH_SUPPORTEDTYPES(IACCESSOR_DECL);
#undef IACCESSOR_DECL

private:
	template<typename U>
	bool InternalGetRange(TArrayView<U>& OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGAttributeAccessorT::InternalGetRange);

		using T = typename Derived::Type;
		const Derived* This = static_cast<const Derived*>(this);

		if constexpr (std::is_same_v<T, U>)
		{
			return This->GetRangeImpl(OutValues, Index, Keys);
		}
		else
		{
			// Special case'd because FSoftObjectPath currently has a deprecated constructor from FName which generates compile warnings.
			constexpr bool bNameToSoftObjectPath = std::is_same_v<U, FSoftObjectPath> && std::is_same_v<T, FName>;

			if constexpr (PCG::Private::IsBroadcastable<T, U>())
			{
				// Special case - always allow broadcasting from soft object path to string, so that legacy code that grabbed soft path attributes
				// as FStrings will still work.
				constexpr bool bSoftReferenceToString = (std::is_same_v<T, FSoftObjectPath> || std::is_same_v<T, FSoftClassPath>) && std::is_same_v<U, FString>;
				if (!!(Flags & EPCGAttributeAccessorFlags::AllowBroadcast) || bSoftReferenceToString)
				{
					TArray<T, TInlineAllocator<4>> InValues;
					InValues.SetNum(OutValues.Num());
					if (This->GetRangeImpl(InValues, Index, Keys))
					{
						for (int32 i = 0; i < OutValues.Num(); ++i)
						{
							PCG::Private::GetValueWithBroadcast<T, U>(InValues[i], OutValues[i]);
						}

						return true;
					}
				}
			}
			else if constexpr (std::is_constructible_v<U, T> && !bNameToSoftObjectPath)
			{
				if (!!(Flags & EPCGAttributeAccessorFlags::AllowConstructible))
				{
					TArray<T, TInlineAllocator<4>> InValues;
					InValues.SetNum(OutValues.Num());
					if (This->GetRangeImpl(InValues, Index, Keys))
					{
						for (int32 i = 0; i < OutValues.Num(); ++i)
						{
							OutValues[i] = U(InValues[i]);
						}
						return true;
					}
				}
			}
			// If U is different of T, and we either can't or allow broadcast or construct from, we will fail, and return false.
			return false;
		}
	}

	template<typename U>
	bool InternalSetRange(TArrayView<const U>& InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGAttributeAccessorT::InternalSetRange);

		// Can't set if read only, and discard any set if there are too many values.
		if (bReadOnly || (Index + InValues.Num()) > Keys.GetNum())
		{
			return false;
		}

		using T = typename Derived::Type;
		Derived* This = static_cast<Derived*>(this);

		if constexpr (std::is_same_v<T, U>)
		{
			return This->SetRangeImpl(InValues, Index, Keys, Flags);
		}
		else
		{
			// Special case'd because FSoftObjectPath currently has a deprecated constructor from FName which generates compile warnings.
			constexpr bool bNameToSoftObjectPath = std::is_same_v<T, FSoftObjectPath> && std::is_same_v<U, FName>;

			if constexpr (PCG::Private::IsBroadcastable<U, T>())
			{
				if (!!(Flags & EPCGAttributeAccessorFlags::AllowBroadcast))
				{
					TArray<T> OutValues;
					OutValues.SetNum(InValues.Num());
					for (int32 i = 0; i < OutValues.Num(); ++i)
					{
						PCG::Private::GetValueWithBroadcast<U, T>(InValues[i], OutValues[i]);
					}

					return This->SetRangeImpl(OutValues, Index, Keys, Flags);
				}
			}
			else if constexpr (std::is_constructible_v<T, U> && !bNameToSoftObjectPath)
			{
				if (!!(Flags & EPCGAttributeAccessorFlags::AllowConstructible))
				{
					TArray<T> OutValues;
					OutValues.SetNum(InValues.Num());
					for (int32 i = 0; i < OutValues.Num(); ++i)
					{
						OutValues[i] = T(InValues[i]);
					}

					return This->SetRangeImpl(OutValues, Index, Keys, Flags);
				}
			}
			// If U is different of T, and we either can't or allow broadcast or construct from, we will fail, and return false.
			return false;
		}

	}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
