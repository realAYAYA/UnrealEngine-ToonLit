// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEUtilitiesHelpers.h"

#include "NNE.h"

#include "NNEUtilitiesThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT

#include <onnx/defs/schema.h>
NNE_THIRD_PARTY_INCLUDES_END


namespace UE::NNEUtilities::Internal
{

TOptional<uint32> GetOpVersionFromOpsetVersion(const FString& OpType, int OpsetVersion)
{
	const onnx::OpSchema* OpSchema = onnx::OpSchemaRegistry::Schema(TCHAR_TO_ANSI(*OpType), OpsetVersion);
	if(OpSchema == nullptr)
	{
		UE_LOG(LogNNE, Warning, TEXT("No OpSchema found for operator %s and OpSet version %d."), *OpType, OpsetVersion);
		return TOptional<uint32>();
	}
	return (uint32) OpSchema->SinceVersion();
}

}