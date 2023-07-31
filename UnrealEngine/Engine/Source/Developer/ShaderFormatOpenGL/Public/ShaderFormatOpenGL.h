// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "RHIDefinitions.h"
#include "Templates/SharedPointer.h"
#include "hlslcc.h"

class FArchive;

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

class FShaderCompilerFlags;
struct FShaderCompilerInput;
struct FShaderCompilerOutput;

enum GLSLVersion 
{
	GLSL_150_REMOVED,
	GLSL_430_REMOVED,
	GLSL_ES2_REMOVED,
	GLSL_ES2_WEBGL_REMOVED,
	GLSL_150_ES2_DEPRECATED,	// ES2 Emulation
	GLSL_150_ES2_NOUB_DEPRECATED,	// ES2 Emulation with NoUBs
	GLSL_150_ES3_1,	// ES3.1 Emulation
	GLSL_ES2_IOS_REMOVED,
	GLSL_310_ES_EXT_REMOVED,
	GLSL_ES3_1_ANDROID,
	GLSL_SWITCH,
	GLSL_SWITCH_FORWARD,

	GLSL_MAX
};

class SHADERFORMATOPENGL_API FOpenGLFrontend
{
public:
	virtual ~FOpenGLFrontend()
	{

	}

	void CompileShader(const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output, const class FString& WorkingDirectory, GLSLVersion Version);

protected:
	// if true, the shader output map will contain true names (i.e. ColorModifier) instead of helper names for runtime binding (i.e. pb_5)
	virtual bool OutputTrueParameterNames()
	{
		return false;
	}

	virtual bool IsSM5(GLSLVersion Version)
	{
		return false;
	}

	// what is the max number of samplers the shader platform can use?
	virtual uint32 GetMaxSamplers(GLSLVersion Version);

	virtual uint32 CalculateCrossCompilerFlags(GLSLVersion Version, const bool bFullPrecisionInPS, const FShaderCompilerFlags& CompilerFlags);

	// set up compilation information like defines and HlslCompileTarget
	virtual void SetupPerVersionCompilationEnvironment(GLSLVersion Version, class FShaderCompilerDefinitions& AdditionalDefines, EHlslCompileTarget& HlslCompilerTarget);

	virtual void ConvertOpenGLVersionFromGLSLVersion(GLSLVersion InVersion, int& OutMajorVersion, int& OutMinorVersion);

	// create the compiling backend
	virtual struct FGlslCodeBackend* CreateBackend(GLSLVersion Version, uint32 CCFlags, EHlslCompileTarget HlslCompilerTarget);

	// create the language spec
	virtual class FGlslLanguageSpec* CreateLanguageSpec(GLSLVersion Version, bool bDefaultPrecisionIsHalf);


	// Allow a subclass to perform additional work on the cross compiled source code
	virtual bool PostProcessShaderSource(GLSLVersion Version, EShaderFrequency Frequency, const ANSICHAR* ShaderSource,
		uint32 SourceLen, class FShaderParameterMap& ParameterMap, TMap<FString, FString>& BindingNameMap, TArray<struct FShaderCompilerError>& Errors,
		const FShaderCompilerInput& ShaderInput)
	{
		return true;
	}

	// allow subclass to write out different output, returning true if it did write everything it needed
	virtual bool OptionalSerializeOutputAndReturnIfSerialized(FArchive& Ar)
	{
		return false;
	}


	void BuildShaderOutput(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* InShaderSource, int32 SourceLen, GLSLVersion Version);
	void PrecompileShader(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, GLSLVersion Version, EHlslShaderFrequency Frequency);

	bool PlatformSupportsOfflineCompilation(const GLSLVersion ShaderVersion) const;

	// fills device capabilities in 'offline', mostly hardcoded values
	void FillDeviceCapsOfflineCompilation(struct FDeviceCapabilities &Caps, const GLSLVersion ShaderVersion) const;
	// final source code processing, based on device capabilities, before actual (offline) compilation; it mostly mirrors the behaviour of OpenGLShaders.cpp/GLSLToDeviceCompatibleGLSL()
	TSharedPtr<ANSICHAR> PrepareCodeForOfflineCompilation(GLSLVersion ShaderVersion, EShaderFrequency Frequency, const ANSICHAR* InShaderSource) const;

	void CompileOffline(const FShaderCompilerInput& ShaderInput, FShaderCompilerOutput &Output, const GLSLVersion ShaderVersion, const ANSICHAR *InShaderSource);

	// based on ShaderVersion decides what platform specific compiler to use
	void PlatformCompileOffline(const FShaderCompilerInput &Input, FShaderCompilerOutput& ShaderOutput, const ANSICHAR* ShaderSource, const GLSLVersion ShaderVersion);
};
