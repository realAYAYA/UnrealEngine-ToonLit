// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FShaderValueTypeHandle;
class FShaderParametersMetadataBuilder;
class FShaderParametersMetadata;

namespace ComputeFramework
{
	COMPUTEFRAMEWORK_API void AddParamForType(FShaderParametersMetadataBuilder& InOutBuilder, TCHAR const* InName, FShaderValueTypeHandle const& InValueType, TArray<FShaderParametersMetadata*>& OutNestedStructs);

	/** Helper struct to convert shader value type to shader parameter metadata */
	 struct COMPUTEFRAMEWORK_API FTypeMetaData
	{
		FTypeMetaData(FShaderValueTypeHandle InType);
		FTypeMetaData(const FTypeMetaData& InOther) = delete;
		FTypeMetaData& operator=(const FTypeMetaData& InOther) = delete;
		~FTypeMetaData();

		const FShaderParametersMetadata* Metadata;
		
		TArray<FShaderParametersMetadata*> AllocatedMetadatas;
	};	
}
