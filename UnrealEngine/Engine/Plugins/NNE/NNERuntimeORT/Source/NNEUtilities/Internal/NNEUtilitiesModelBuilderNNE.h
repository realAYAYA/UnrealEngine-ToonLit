// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNEUtilitiesModelBuilder.h"
#include "Templates/UniquePtr.h"

namespace UE::NNEUtilities::Internal
{

NNEUTILITIES_API TUniquePtr<IModelBuilder> CreateNNEModelBuilder();

} // UE::NNEUtilities::Internal

