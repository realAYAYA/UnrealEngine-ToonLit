// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace UE::NNEUtilities::Internal
{

NNEUTILITIES_API TOptional<uint32> GetOpVersionFromOpsetVersion(const FString& OpType, int OpsetVersion);

} // namespace UE::NNEUtilities::Internal