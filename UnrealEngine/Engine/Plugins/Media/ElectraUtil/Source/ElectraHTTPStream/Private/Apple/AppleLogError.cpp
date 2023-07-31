// Copyright Epic Games, Inc. All Rights Reserved.

#if ELECTRA_HTTPSTREAM_APPLE

#include "Apple/AppleLogError.h"
#include "ElectraHTTPStreamModule.h"



namespace ElectraHTTPStreamApple
{

FString GetErrorMessage(NSError* ErrorCode)
{
	SCOPED_AUTORELEASE_POOL;

	FString Domain([ErrorCode domain]);
	FString Descr([ErrorCode localizedDescription]);
	int32 Code = ErrorCode.code;
	FString msg = FString::Printf(TEXT("Error %d (0x%x) in %s: %s"), Code, Code, *Domain, *Descr);
	return msg;
}


void LogError(const FString& Message)
{
	UE_LOG(LogElectraHTTPStream, Error, TEXT("%s"), *Message);
}

}

#endif
