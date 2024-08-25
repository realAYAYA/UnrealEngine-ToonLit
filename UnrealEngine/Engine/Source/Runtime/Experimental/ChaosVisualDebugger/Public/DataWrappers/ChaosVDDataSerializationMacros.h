// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef CVD_READ_PACKED_BITFIELD_SERIALIZER
	#define CVD_UNPACK_BITFIELD_DATA(Value, PackedBits, EnumFlag) \
				Value = EnumHasAnyFlags(PackedBits, EnumFlag);
#endif

#ifndef CVD_WRITE_PACKED_BITFIELD_SERIALIZER
	#define CVD_PACK_BITFIELD_DATA(Value, PackedBits, EnumFlag) \
				if (Value) \
				{ \
					EnumAddFlags(PackedBits, EnumFlag);\
				}
#endif

#ifndef CVD_SERIALIZE_STATIC_ARRAY
	#define CVD_SERIALIZE_STATIC_ARRAY(Archive, Array) \
	{ \
		constexpr int32 Size = UE_ARRAY_COUNT(Array) ; \
		for (int32 Index = 0; Index < Size; Index++)\
		{\
			Archive << Array[Index]; \
		}\
	}
#endif