// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/Archive.h"

/**
* Base class for serializing bitstreams.
*/
class FBitArchive : public FArchive
{
public:
	/**
	 * Default Constructor
	 */
	CORE_API FBitArchive();

	CORE_API virtual void SerializeBitsWithOffset( void* Src, int32 SourceBit, int64 LengthBits ) PURE_VIRTUAL(FBitArchive::SerializeBitsWithOffset,);
};
