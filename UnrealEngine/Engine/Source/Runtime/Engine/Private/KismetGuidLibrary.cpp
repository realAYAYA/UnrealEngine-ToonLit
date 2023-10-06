// Copyright Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetGuidLibrary.h"
#include "Misc/Guid.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(KismetGuidLibrary)


/* Guid functions
 *****************************************************************************/

UKismetGuidLibrary::UKismetGuidLibrary( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


FString UKismetGuidLibrary::Conv_GuidToString( const FGuid& InGuid )
{
	return InGuid.ToString(EGuidFormats::Digits);
}


bool UKismetGuidLibrary::EqualEqual_GuidGuid( const FGuid& A, const FGuid& B )
{
	return A == B;
}


bool UKismetGuidLibrary::NotEqual_GuidGuid( const FGuid& A, const FGuid& B )
{
	return A != B;
}


bool UKismetGuidLibrary::IsValid_Guid( const FGuid& InGuid )
{
	return InGuid.IsValid();
}


void UKismetGuidLibrary::Invalidate_Guid( FGuid& InGuid )
{
	InGuid.Invalidate();
}


FGuid UKismetGuidLibrary::NewGuid()
{
	return FGuid::NewGuid();
}


void UKismetGuidLibrary::Parse_StringToGuid( const FString& GuidString, FGuid& OutGuid, bool& Success )
{
	Success = FGuid::Parse(GuidString, OutGuid);
}

