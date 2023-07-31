// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensPlatformAtomics.h"
#include "CoreMinimal.h"

#if PLATFORM_HOLOLENS

void FHoloLensAtomics::HandleAtomicsFailure( const TCHAR* InFormat, ... )
{	
	TCHAR TempStr[1024];
	va_list Ptr;

	va_start( Ptr, InFormat );	
	FPlatformString::GetVarArgs( TempStr, UE_ARRAY_COUNT(TempStr), InFormat, Ptr );
	va_end( Ptr );

	UE_LOG(LogTemp, Log, TempStr);
	check( 0 );
}

#endif //PLATFORM_HOLOLENS