// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterMetadata.h"
#include "Templates/AlignmentTemplates.h"

class RENDERCORE_API FShaderParametersMetadataBuilder
{
public:
	FShaderParametersMetadataBuilder() {}

	explicit FShaderParametersMetadataBuilder(const FShaderParametersMetadata* RootParametersMetadata)
	{
		if (RootParametersMetadata)
		{
			Members = RootParametersMetadata->GetMembers();
			NextMemberOffset = RootParametersMetadata->GetSize();
		}
	}

	template<typename T>
	void AddParam(
		const TCHAR* Name,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		)
	{
		using TParamTypeInfo = TShaderParameterTypeInfo<T>;

		NextMemberOffset = Align(NextMemberOffset, TParamTypeInfo::Alignment);

		new(Members) FShaderParametersMetadata::FMember(
			Name,
			TEXT(""),
			__LINE__,
			NextMemberOffset,
			TParamTypeInfo::BaseType,
			Precision,
			TParamTypeInfo::NumRows,
			TParamTypeInfo::NumColumns,
			TParamTypeInfo::NumElements,
			TParamTypeInfo::GetStructMetadata()
			);

		NextMemberOffset += sizeof(typename TParamTypeInfo::TAlignedType);
	}

	template<typename T>
	void AddParamArray(
		const TCHAR* Name,
		int32 NumElements,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		)
	{
		using TParamTypeInfo = TShaderParameterTypeInfo<T>;

		NextMemberOffset = Align(NextMemberOffset, SHADER_PARAMETER_ARRAY_ELEMENT_ALIGNMENT);

		new(Members) FShaderParametersMetadata::FMember(
			Name,
			TEXT(""),
			__LINE__,
			NextMemberOffset,
			TParamTypeInfo::BaseType,
			Precision,
			TParamTypeInfo::NumRows,
			TParamTypeInfo::NumColumns,
			NumElements,
			TParamTypeInfo::GetStructMetadata()
			);

		NextMemberOffset += sizeof(typename TParamTypeInfo::TAlignedType) * NumElements;
	}

	template<typename T>
	void AddReferencedStruct(
		const TCHAR* Name,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		)
	{
		AddReferencedStruct(Name, TShaderParameterStructTypeInfo<T>::GetStructMetadata(), Precision);
	}

	void AddReferencedStruct(
		const TCHAR* Name,
		const FShaderParametersMetadata* StructMetadata,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		);

	template<typename T>
	void AddIncludedStruct(
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
	)
	{
		AddIncludedStruct(TShaderParameterStructTypeInfo<T>::GetStructMetadata(), Precision);
	}

	void AddIncludedStruct(
		const FShaderParametersMetadata* StructMetadata,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
	);

	template<typename T>
	uint32 AddNestedStruct(
		const TCHAR* Name,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		)
	{
		using TParamTypeInfo = TShaderParameterStructTypeInfo<T>;

		NextMemberOffset = Align(NextMemberOffset, TParamTypeInfo::Alignment);
		const uint32 ThisMemberOffset = NextMemberOffset;

		new(Members) FShaderParametersMetadata::FMember(
			Name,
			TEXT(""),
			__LINE__,
			NextMemberOffset,
			UBMT_NESTED_STRUCT,
			Precision,
			TParamTypeInfo::NumRows,
			TParamTypeInfo::NumColumns,
			TParamTypeInfo::NumElements,
			TParamTypeInfo::GetStructMetadata()
		);

		NextMemberOffset += sizeof(typename TParamTypeInfo::TAlignedType);
		return ThisMemberOffset;
	}

	uint32 AddNestedStruct(
		const TCHAR* Name,
		const FShaderParametersMetadata* StructMetadata,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		);

	void AddBufferSRV(
		const TCHAR* Name,
		const TCHAR* ShaderType,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		);

	void AddBufferUAV(
		const TCHAR* Name,
		const TCHAR* ShaderType,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		);

	void AddRDGBufferSRV(
		const TCHAR* Name,
		const TCHAR* ShaderType,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		);

	void AddRDGBufferUAV(
		const TCHAR* Name,
		const TCHAR* ShaderType,
		EShaderPrecisionModifier::Type Precision = EShaderPrecisionModifier::Float
		);

	void AlignNextMemberToStruct()
	{
		NextMemberOffset = Align(NextMemberOffset, SHADER_PARAMETER_STRUCT_ALIGNMENT);
	}

	uint32 GetNextMemberOffset() const { return NextMemberOffset; }

	FShaderParametersMetadata* Build(
		FShaderParametersMetadata::EUseCase UseCase,
		const TCHAR* ShaderParameterName
		);

private:
	TArray<FShaderParametersMetadata::FMember> Members;
	uint32 NextMemberOffset = 0;
};