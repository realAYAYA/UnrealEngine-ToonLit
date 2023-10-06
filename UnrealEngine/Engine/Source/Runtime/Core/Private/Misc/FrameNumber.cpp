// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/FrameNumber.h"
#include "Serialization/Archive.h"



/* FFrameNumber interface
 *****************************************************************************/

bool FFrameNumber::Serialize(FArchive& Ar)
{
	Ar << Value;
	return true;
}

FArchive& operator<<(FArchive& Ar, FFrameNumber& FrameNumber)
{
	return Ar << FrameNumber.Value;
}
