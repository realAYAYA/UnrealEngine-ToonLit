// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpHeader.h"

#include "Interfaces/IHttpRequest.h"

FHttpHeader FHttpHeader::SetHeaders(const TMap<FString, FString>& NewHeaders)
{
	Headers = NewHeaders;
	return *this;
}

FString FHttpHeader::GetHeader(FString&& Key) const
{
	const FString* FoundHeader = Headers.Find(MoveTemp(Key));
	return FoundHeader ? *FoundHeader : TEXT("");
}

void FHttpHeader::AddHeader(TPair<FString, FString>&& NewHeader)
{
	Headers.Add(MoveTemp(NewHeader));
}

bool FHttpHeader::RemoveHeader(FString&& HeaderToRemove)
{
	return !!Headers.Remove(HeaderToRemove);
}

TArray<FString> FHttpHeader::GetAllHeaders() const
{
	TArray<FString> OutArray;
	OutArray.Reserve(Headers.Num());

	for (const TPair<FString, FString>& Header : Headers)
	{
		OutArray.Add(FString::Printf(TEXT("%s: %s"), *Header.Key, *Header.Value));
	}

	return OutArray;
}

void FHttpHeader::AssignHeadersToRequest(const TSharedRef<IHttpRequest> Request)
{
	for (const TPair<FString, FString>& HeaderPair : Headers)
	{
		Request->SetHeader(HeaderPair.Key, HeaderPair.Value);
	}
}
