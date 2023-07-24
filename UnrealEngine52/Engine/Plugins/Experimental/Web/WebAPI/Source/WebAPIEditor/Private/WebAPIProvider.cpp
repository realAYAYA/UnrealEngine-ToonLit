// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebAPIProvider.h"
#include "Async/Future.h"

TFuture<EWebAPIConversionResult> IWebAPIProviderInterface::ConvertToWebAPISchema(
	const TWeakObjectPtr<UWebAPIDefinition>& InDefinition)
{
	return MakeFulfilledPromise<EWebAPIConversionResult>(EWebAPIConversionResult::Failed).GetFuture();
}
