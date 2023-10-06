// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGMetadataAttributeTraits.h"


#include "IPCGAttributeAccessor.generated.h"

class IPCGAttributeAccessorKeys;

//////////////////////////////////////////////////////////////////////////////

UENUM(Meta = (Bitflags))
enum class EPCGAttributeAccessorFlags
{	
	// Always require that the underlying type of the accessor match the expected type, 1 for 1.
	StrictType = 1 << 0,

	// Allow to broadcast the expected type to the underlying type (or vice versa, depending on the operation)
	AllowBroadcast = 1 << 1,

	// Allow to construct the expected type from the underlying type (or vice versa, depending on the operation)
	AllowConstructible = 1 << 2,

	// By default, if the key is a PCGInvalidEntryKey, it will add a new entry. With this set, it will override the default value.
	// USE WITH CAUTION
	AllowSetDefaultValue = 1 << 3
};
ENUM_CLASS_FLAGS(EPCGAttributeAccessorFlags);

//////////////////////////////////////////////////////////////////////////////

/**
* Base class for accessor. GetRange and SetRange will be sepcialized for all supported types, and there are a GetRange and SetRange virtual
* for each supported type.
* For Get/Set, you need a key, that will represent how you can access the value wanted. cf PCGAttributeAccessorKeys.
* NOTE: This is not threadsafe and not intended to be used unprotected.
*/
class PCG_API IPCGAttributeAccessor
{
public:
	virtual ~IPCGAttributeAccessor() = default;
	
	// Both GetRange and SetRange will be specialized for all supported types. (declared below and defined in the cpp). 
	// By default, all unsupported types will return false.

	/**
	* Get a value from the accessor for a given type. Not threadsafe for all accessors.
	* @param OutValue - Where the value will be written to
	* @param Index - The index to look for in the keys
	* @param Keys - Identification to know how to retrieve the value
	* @param Flags - Optional flag to allow for specific operations. Cf EPCGAttributeAccessorFlags.
	* @return true if the get succeeded, false otherwise
	*/
	template <typename T>
	bool Get(T& OutValue, int32 Index, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType) const
	{
		TArrayView<T> Temp(&OutValue, 1);
		return GetRange(TArrayView<T>(&OutValue, 1), Index, Keys, Flags);
	}

	/**
	* Get a value from the accessor for a given type at index 0. Not threadsafe for all accessors.
	* @param OutValue - Where the value will be written to
	* @param Keys - Identification to know how to retrieve the value
	* @param Flags - Optional flag to allow for specific operations. Cf EPCGAttributeAccessorFlags.
	* @return true if the get succeeded, false otherwise
	*/
	template <typename T>
	bool Get(T& OutValue, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType) const
	{
		return Get(OutValue, 0, Keys, Flags);
	}

	/**
	* Get a range of values from the accessor for a given type. Not threadsafe for all accessors.
	* If the number of elements asked is greater that the number of keys, it will wrap around.
	* @param OutValues - View on memory where to write values to. Its "Num" will determine how many elements we will read.
	* @param Index - The index to start looking for in the keys
	* @param Keys - Identification to know how to retrieve the value
	* @param Flags - Optional flag to allow for specific operations. Cf EPCGAttributeAccessorFlags.
	* @return true if the get succeeded, false otherwise
	*/
	template <typename T>
	bool GetRange(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType) const
	{
		return false;
	}

	/**
	* Set a value to the accessor for a given type. Not threadsafe for all accessors. 
	* If Index is greater than the number of keys, it will fail (return false).
	* @param InValue - the value to write
	* @param Index - The index to look for in the keys
	* @param Keys - Identification to know how to identify the value.
	* @param Flags - Optional flag to allow for specific operations. Cf EPCGAttributeAccessorFlags.
	* @return true if the set succeeded, false otherwise
	*/
	template <typename T>
	bool Set(const T& InValue, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType)
	{
		return SetRange(TArrayView<const T>(&InValue, 1), Index, Keys, Flags);
	}

	/**
	* Set a value to the accessor for a given type at index 0. Not threadsafe for all accessors.
	* @param InValue - the value to write
	* @param Keys - Identification to know how to identify the value.
	* @param Flags - Optional flag to allow for specific operations. Cf EPCGAttributeAccessorFlags.
	* @return true if the set succeeded, false otherwise
	*/
	template <typename T>
	bool Set(const T& InValue, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType)
	{
		return Set(InValue, 0, Keys, Flags);
	}

	/**
	* Set a range of values to the accessor for a given type. Not threadsafe for all accessors.
	* If the number of elements asked is greater that the number of keys, it will fail. It is to avoid writing at the same memory place multiple times.
	* @param InValues - View on memory where to read values from. Its "Num" will determine how many elements we will write.
	* @param Index - The index to start looking for in the keys
	* @param Keys - Identification to know how to retrieve the value
	* @param Flags - Optional flag to allow for specific operations. Cf EPCGAttributeAccessorFlags.
	* @return true if the get succeeded, false otherwise
	*/
	template <typename T>
	bool SetRange(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::StrictType)
	{
		return false;
	}

	int16 GetUnderlyingType() const { return UnderlyingType; }
	bool IsReadOnly() const { return bReadOnly; }

protected:
	IPCGAttributeAccessor(bool bInReadOnly, int16 InUnderlyingType)
		: bReadOnly(bInReadOnly)
		, UnderlyingType(InUnderlyingType)
	{}

	// Define all virtual calls for all supported types.
#define IACCESSOR_DECL(T) \
	virtual bool GetRange##T(TArrayView<T>& OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) const = 0; \
	virtual bool SetRange##T(TArrayView<const T>& InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) = 0;
	PCG_FOREACH_SUPPORTEDTYPES(IACCESSOR_DECL);
#undef IACCESSOR_DECL

	bool bReadOnly = true;
	int16 UnderlyingType = (int16)EPCGMetadataTypes::Unknown;
};

// Specialization of IPCGAttributeAccessor::Get and IPCGAttributeAccessor::Set, for all supported types.
#define IACCESSOR_DECL(T) \
template <> bool PCG_API IPCGAttributeAccessor::GetRange<T>(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags) const; \
template <> bool PCG_API IPCGAttributeAccessor::SetRange<T>(TArrayView<const T> InValues, int32 Index, IPCGAttributeAccessorKeys& Keys, EPCGAttributeAccessorFlags Flags);
PCG_FOREACH_SUPPORTEDTYPES(IACCESSOR_DECL);
#undef IACCESSOR_DECL

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Metadata/PCGMetadataCommon.h"
#endif
