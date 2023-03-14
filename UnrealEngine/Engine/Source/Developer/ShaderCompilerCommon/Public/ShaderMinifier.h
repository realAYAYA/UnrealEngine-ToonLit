// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Containers/UnrealString.h"
#include "Misc/EnumClassFlags.h"

namespace UE::ShaderMinifier
{

enum EMinifyShaderFlags
{
	None = 0,
	OutputReasons = 1 << 1,
	OutputStats   = 1 << 2,
	OutputLines   = 1 << 3,
};
ENUM_CLASS_FLAGS(EMinifyShaderFlags)

struct FDiagnosticMessage
{
	FString Message;
	FString File;
	int32 Offset  = INDEX_NONE;
	int32 Line    = INDEX_NONE;
	int32 Column  = INDEX_NONE;
};

struct FDiagnostics
{
	TArray<FDiagnosticMessage> Errors;
	TArray<FDiagnosticMessage> Warnings;
};

struct FMinifiedShader
{
	FString Code;
	FDiagnostics Diagnostics;

	bool Success() const
	{
		return Diagnostics.Errors.IsEmpty() && !Code.IsEmpty();
	}
};

extern SHADERCOMPILERCOMMON_API FMinifiedShader Minify(const FStringView PreprocessedShader, const FStringView EntryPoint, EMinifyShaderFlags Flags = EMinifyShaderFlags::None);

} // UE::ShaderMinifier

