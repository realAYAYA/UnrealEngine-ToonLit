// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ApplePlatformString.mm: Mac implementations of string functions
=============================================================================*/

#include "Apple/ApplePlatformString.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"

@implementation NSString (FString_Extensions)

+ (NSString*) stringWithTCHARString:(const TCHAR*)MyTCHARString
{
	return [NSString stringWithCString:TCHAR_TO_UTF8(MyTCHARString) encoding:NSUTF8StringEncoding];
}

+ (NSString*) stringWithFString:(const FString&)InFString
{
	return [NSString stringWithTCHARString:*InFString];
}


@end
