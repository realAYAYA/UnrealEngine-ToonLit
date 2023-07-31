// Copyright Epic Games, Inc. All Rights Reserved.

#include "ORTExceptionHandler.h"
#include "ThirdPartyHelperAndDLLLoaderUtils.h"



/* FORTExceptionHandler public classes
 *****************************************************************************/

void FORTExceptionHandler::ThrowPseudoException(const std::string& InString, const int32 InCode)
{
	const FString ErrorMessage = FString::Format(TEXT("ONNXRuntime threw an exception with code {0}, e.what(): \"{1}\"."),
		{ FString::FromInt(InCode), FString(ANSI_TO_TCHAR(InString.c_str())) });
	UE_LOG(LogNeuralNetworkInferenceThirdPartyHelperAndDLLLoader, Warning, TEXT("%s"), *ErrorMessage);
	checkf(false, TEXT("%s"), *ErrorMessage);
}
