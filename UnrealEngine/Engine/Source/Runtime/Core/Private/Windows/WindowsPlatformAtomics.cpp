// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformAtomics.h"
#include "Misc/AssertionMacros.h"
#include "Logging/LogMacros.h"
#include "Templates/UnrealTemplate.h"
#include "CoreGlobals.h"


void FWindowsPlatformAtomics::HandleAtomicsFailure( const TCHAR* InFormat, ... )
{	
	TCHAR TempStr[1024];
	va_list Ptr;

	va_start( Ptr, InFormat );	
	FCString::GetVarArgs( TempStr, UE_ARRAY_COUNT(TempStr), InFormat, Ptr );
	va_end( Ptr );

	UE_LOG(LogWindows, Log,  TempStr );
	check( 0 );
}
