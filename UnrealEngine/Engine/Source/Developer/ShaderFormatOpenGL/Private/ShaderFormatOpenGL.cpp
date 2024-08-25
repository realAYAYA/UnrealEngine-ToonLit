// Copyright Epic Games, Inc. All Rights Reserved.
//

#include "ShaderFormatOpenGL.h"

#include "ShaderCompilerCommon.h"
#include "ShaderPreprocessor.h"
#include "ShaderPreprocessTypes.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"

static FName NAME_GLSL_150_ES3_1(TEXT("GLSL_150_ES31"));
static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));

extern bool ShouldUseDXC(FShaderCompilerFlags Flags);

extern void CompileOpenGLShader(
	const FShaderCompilerInput& Input,
	const FShaderPreprocessOutput& InPreprocessOutput,
	FShaderCompilerOutput& Output,
	const FString& WorkingDirectory,
	GLSLVersion Version);

/** Version for shader format, this becomes part of the DDC key. */
static const FGuid UE_SHADER_GLSL_VER = FGuid("33988E19-4962-4762-8219-F15483DA764A");

class FShaderFormatGLSL : public UE::ShaderCompilerCommon::FBaseShaderFormat 
{
	static void CheckFormat(FName Format)
	{
		check(Format == NAME_GLSL_150_ES3_1 || Format == NAME_GLSL_ES3_1_ANDROID);
	}

public:
	virtual uint32 GetVersion(FName Format) const override
	{
		CheckFormat(Format);
		FGuid GLSLVersion{};
		if (ensure(Format == NAME_GLSL_150_ES3_1 || Format == NAME_GLSL_ES3_1_ANDROID))
		{
			GLSLVersion = UE_SHADER_GLSL_VER;
		}

		const uint32 BaseHash = GetTypeHash(GLSLVersion);

		uint32 Version = GetTypeHash(HLSLCC_VersionMinor);
		Version = HashCombine(Version, BaseHash);

		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Add(NAME_GLSL_150_ES3_1);
		OutFormats.Add(NAME_GLSL_ES3_1_ANDROID);
	}

	static GLSLVersion TranslateFormatNameToEnum(FName Format)
	{
		if (Format == NAME_GLSL_150_ES3_1)
		{
			return GLSL_150_ES3_1;
		}
		else if (Format == NAME_GLSL_ES3_1_ANDROID)
		{
			return GLSL_ES3_1_ANDROID;
		}
		else
		{
			check(0);
			return GLSL_MAX;
		}
	}

	virtual void ModifyShaderCompilerInput(FShaderCompilerInput& Input) const override
	{
		GLSLVersion Version = TranslateFormatNameToEnum(Input.ShaderFormat);
		switch (Version)
		{
		case GLSL_ES3_1_ANDROID:
			Input.Environment.SetDefine(TEXT("COMPILER_GLSL_ES3_1"), 1);
			Input.Environment.SetDefine(TEXT("ES3_1_PROFILE"), 1);
			break;

		case GLSL_150_ES3_1:
			Input.Environment.SetDefine(TEXT("COMPILER_GLSL"), 1);
			Input.Environment.SetDefine(TEXT("ES3_1_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("row_major"), TEXT(""));
			break;

		default:
			check(0);
		}
		Input.Environment.SetDefine(TEXT("OPENGL_PROFILE"), 1);

		const bool bUseDXC = ShouldUseDXC(Input.Environment.CompilerFlags);
		Input.Environment.SetDefine(TEXT("COMPILER_HLSLCC"), bUseDXC ? 2 : 1);
		Input.Environment.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);

		if (Input.Environment.FullPrecisionInPS || (IsValidRef(Input.SharedEnvironment) && Input.SharedEnvironment->FullPrecisionInPS))
		{
			Input.Environment.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
		}
	}

	virtual void CompilePreprocessedShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, FShaderCompilerOutput& Output, const FString& WorkingDirectory) const override
	{
		CheckFormat(Input.ShaderFormat);
		CompileOpenGLShader(Input, PreprocessOutput, Output, WorkingDirectory, TranslateFormatNameToEnum(Input.ShaderFormat));		
	}

	virtual const TCHAR* GetPlatformIncludeDirectory() const override
	{
		return TEXT("GL");
	}
};

/**
 * Module for OpenGL shaders
 */

static IShaderFormat* Singleton = nullptr;

class FShaderFormatOpenGLModule : public IShaderFormatModule
{
public:
	virtual ~FShaderFormatOpenGLModule() override
	{
		delete Singleton;
		Singleton = nullptr;
	}
	virtual IShaderFormat* GetShaderFormat() override
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatGLSL();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FShaderFormatOpenGLModule, ShaderFormatOpenGL);
