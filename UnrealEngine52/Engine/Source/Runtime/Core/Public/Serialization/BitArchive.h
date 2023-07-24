// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/Archive.h"

/**
* Base class for serializing bitstreams.
*/
class CORE_API FBitArchive : public FArchive
{
public:
	/**
	 * Default Constructor
	 */
	FBitArchive();

	virtual void SerializeBitsWithOffset( void* Src, int32 SourceBit, int64 LengthBits ) PURE_VIRTUAL(FBitArchive::SerializeBitsWithOffset,);
};