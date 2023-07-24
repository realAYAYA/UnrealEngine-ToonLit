// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraCompileHashVisitor.h"
#include "ShaderCompilerCore.h"

bool FNiagaraCompileHashVisitor::UpdateShaderFile(const TCHAR* ShaderFilePath)
{
	const FSHAHash Hash = GetShaderFileHash(ShaderFilePath, EShaderPlatform::SP_PCD3D_SM5);
	FString SanitizedShaderName(ShaderFilePath);
	SanitizedShaderName.ReplaceCharInline('/', '_');
	SanitizedShaderName.ReplaceCharInline('.', '_');
	return UpdateString(*SanitizedShaderName, Hash.ToString());
}

bool FNiagaraCompileHashVisitor::UpdateString(const TCHAR* InDebugName, FStringView InData)
{
	HashState.Update((const uint8*)InData.GetData(), sizeof(TCHAR) * InData.Len());
#if WITH_EDITORONLY_DATA
	if (LogCompileIdGeneration != 0)
	{
		Values.Top().PropertyKeys.Push(InDebugName);
		Values.Top().PropertyValues.Push(FString(InData));
	}
#endif
	return true;
}
