// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

class HORDE_API FHordeHttpClient
{
public:
	FHordeHttpClient(FString InServerUrl);
	~FHordeHttpClient();

	bool LoginWithOidc(const TCHAR* Profile, bool bUnattended, FFeedbackContext* Warn = nullptr);

	bool LoginWithEnvironmentVariable();
	
	TSharedRef<IHttpRequest> CreateRequest(const TCHAR* Verb, const TCHAR* Path);
	TSharedRef<IHttpResponse> Get(const TCHAR* Path);

	template<typename T>
	T Get(const TCHAR* Path)
	{
		T Result;
		Result.FromJson(Get(Path)->GetContentAsString());
		return Result;
	}

	static TSharedRef<IHttpResponse> ExecuteRequest(TSharedRef<IHttpRequest> Request);

private:
	FString ServerUrl;
	FString Token;
};
