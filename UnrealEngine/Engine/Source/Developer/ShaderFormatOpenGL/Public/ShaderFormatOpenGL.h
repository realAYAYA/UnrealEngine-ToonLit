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
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual ~FOpenGLFrontend()
	{

	}

	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	void CompileShader(const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output, const class FString& WorkingDirectory, GLSLVersion Version);

protected:
	// if true, the shader output map will contain true names (i.e. ColorModifier) instead of helper names for runtime binding (i.e. pb_5)
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual bool OutputTrueParameterNames()
	{
		return false;
	}

	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual bool IsSM5(GLSLVersion Version)
	{
		return false;
	}

	// what is the max number of samplers the shader platform can use?
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual uint32 GetMaxSamplers(GLSLVersion Version);

	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual uint32 CalculateCrossCompilerFlags(GLSLVersion Version, const bool bFullPrecisionInPS, const FShaderCompilerFlags& CompilerFlags);

	// set up compilation information like defines and HlslCompileTarget
	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual void SetupPerVersionCompilationEnvironment(GLSLVersion Version, class FShaderCompilerDefinitions& AdditionalDefines, EHlslCompileTarget& HlslCompilerTarget);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual void ConvertOpenGLVersionFromGLSLVersion(GLSLVersion InVersion, int& OutMajorVersion, int& OutMinorVersion);

	// create the compiling backend
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual struct FGlslCodeBackend* CreateBackend(GLSLVersion Version, uint32 CCFlags, EHlslCompileTarget HlslCompilerTarget);

	// create the language spec
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual class FGlslLanguageSpec* CreateLanguageSpec(GLSLVersion Version, bool bDefaultPrecisionIsHalf);


	// Allow a subclass to perform additional work on the cross compiled source code
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual bool PostProcessShaderSource(GLSLVersion Version, EShaderFrequency Frequency, const ANSICHAR* ShaderSource,
		uint32 SourceLen, class FShaderParameterMap& ParameterMap, TMap<FString, FString>& BindingNameMap, TArray<struct FShaderCompilerError>& Errors,
		const FShaderCompilerInput& ShaderInput)
	{
		return true;
	}

	// allow subclass to write out different output, returning true if it did write everything it needed
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	virtual bool OptionalSerializeOutputAndReturnIfSerialized(FArchive& Ar)
	{
		return false;
	}


	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	void BuildShaderOutput(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* InShaderSource, int32 SourceLen, GLSLVersion Version);
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	void PrecompileShader(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, GLSLVersion Version, EHlslShaderFrequency Frequency);

	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	bool PlatformSupportsOfflineCompilation(const GLSLVersion ShaderVersion) const;

	// fills device capabilities in 'offline', mostly hardcoded values
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	void FillDeviceCapsOfflineCompilation(struct FDeviceCapabilities &Caps, const GLSLVersion ShaderVersion) const;
	// final source code processing, based on device capabilities, before actual (offline) compilation; it mostly mirrors the behaviour of OpenGLShaders.cpp/GLSLToDeviceCompatibleGLSL()
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	TSharedPtr<ANSICHAR> PrepareCodeForOfflineCompilation(GLSLVersion ShaderVersion, EShaderFrequency Frequency, const ANSICHAR* InShaderSource) const;

	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	void CompileOffline(const FShaderCompilerInput& ShaderInput, FShaderCompilerOutput &Output, const GLSLVersion ShaderVersion, const ANSICHAR *InShaderSource);

	// based on ShaderVersion decides what platform specific compiler to use
	UE_DEPRECATED(5.4, "FOpenGLFrontend is deprecated and derived implementations no longer possible; OpenGL shader compilation should only be triggered from within the shader format code.")
	void PlatformCompileOffline(const FShaderCompilerInput &Input, FShaderCompilerOutput& ShaderOutput, const ANSICHAR* ShaderSource, const GLSLVersion ShaderVersion);
};
