// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

typedef TMap<FString, FString> FContextRequestHeaders;
DECLARE_DELEGATE_FourParams(FOnBeforeContextResourceLoadDelegate, FString /*Url*/, FString /*ResourceType*/, FContextRequestHeaders& /*AdditionalHeaders*/, const bool /*AllowUserCredentials*/);
