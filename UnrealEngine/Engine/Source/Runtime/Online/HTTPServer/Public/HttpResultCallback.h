// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HttpServerResponse.h"

/**
* FHttpResultCallback
* This callback is intended to be invoked exclusively by FHttpRequestHandler delegates
* 
* @param Response The response to write
*/
typedef TFunction<void(TUniquePtr<FHttpServerResponse>&& Response)> FHttpResultCallback;


