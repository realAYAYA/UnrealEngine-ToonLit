// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebAPIUtilities.h"

const FText& UWebAPIUtilities::GetResponseMessage(const FWebAPIMessageResponse& MessageResponse)
{
	return MessageResponse.GetMessage();
}

FString UWebAPIUtilities::GetHostFromUrl(const FString& InUrl)
{
	FString SchemeName;
	FParse::SchemeNameFromURI(*InUrl, SchemeName);
	FString Host = InUrl.Replace(*(SchemeName + TEXT("://")), TEXT(""));
		
	int32 DelimiterIdx = -1;
	if(Host.FindChar(TEXT('/'), DelimiterIdx))
	{
		Host = Host.Left(DelimiterIdx);
	}

	return Host;
}
