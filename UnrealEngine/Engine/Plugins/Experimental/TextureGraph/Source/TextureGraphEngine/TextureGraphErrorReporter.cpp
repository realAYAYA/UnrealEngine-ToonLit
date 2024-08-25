// Copyright Epic Games, Inc. All Rights Reserved.
#include "TextureGraphErrorReporter.h"

#include "TextureGraphEngine.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY(LogTextureGraphError);

FTextureGraphErrorReport FTextureGraphErrorReporter::ReportLog(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj /*= nullptr*/)
{
	FTextureGraphErrorReport Report{ErrorId, ErrorMsg, ReferenceObj};
	UE_LOG(LogTextureGraphError, Log, TEXT("ErrorReporter: %s"), *Report.GetFormattedMessage());
	return Report;
}

FTextureGraphErrorReport FTextureGraphErrorReporter::ReportWarning(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj /*= nullptr*/)
{
	FTextureGraphErrorReport Report{ErrorId, ErrorMsg, ReferenceObj};
	// Don't throw Warning assert when in test mode
	if (!TextureGraphEngine::IsTestMode())
	{
		UE_LOG(LogTextureGraphError, Warning, TEXT("ErrorReporter: %s"), *Report.GetFormattedMessage());
	}
	return Report; 
}

FTextureGraphErrorReport FTextureGraphErrorReporter::ReportError(int32 ErrorId, const FString& ErrorMsg, UObject* ReferenceObj /*= nullptr*/)
{
	FTextureGraphErrorReport Report{ErrorId, ErrorMsg, ReferenceObj};
	// Don't throw Error assert when in test mode
	if (!TextureGraphEngine::IsTestMode())
	{
		UE_LOG(LogTextureGraphError, Error, TEXT("ErrorReporter: %s"), *Report.GetFormattedMessage());
	}
	return Report; 
}


