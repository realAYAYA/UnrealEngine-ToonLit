// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatD3D.h"
#include "ShaderCompilerCommon.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "DXCWrapper.h"

static FName NAME_PCD3D_SM6(TEXT("PCD3D_SM6"));
static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));
static FName NAME_D3D_ES3_1_HOLOLENS(TEXT("D3D_ES3_1_HOLOLENS"));

class FShaderFormatD3D : public IShaderFormat
{
	enum
	{
		UE_SHADER_PCD3D_SHARED_VER = 4,

		/** Version for shader format, this becomes part of the DDC key. */
		UE_SHADER_PCD3D_SM6_VER = UE_SHADER_PCD3D_SHARED_VER + 6,
		UE_SHADER_PCD3D_SM5_VER = UE_SHADER_PCD3D_SHARED_VER + 11,
		UE_SHADER_PCD3D_ES3_1_VER = UE_SHADER_PCD3D_SHARED_VER + 8,
		UE_SHADER_D3D_ES3_1_HOLOLENS_VER = UE_SHADER_PCD3D_ES3_1_VER,
	};

	void CheckFormat(FName Format) const
	{
		check(Format == NAME_PCD3D_SM6 || Format == NAME_PCD3D_SM5 || Format == NAME_PCD3D_ES3_1 || Format == NAME_D3D_ES3_1_HOLOLENS);
	}

	uint32 DxcVersionHash = 0;

public:

	FShaderFormatD3D(uint32 InDxcVersionHash)
		: DxcVersionHash(InDxcVersionHash)
	{
	}

	virtual uint32 GetVersion(FName Format) const override
	{
		CheckFormat(Format);
		if (Format == NAME_PCD3D_SM6)
		{
			uint32 ShaderModelHash = GetTypeHash(UE_SHADER_PCD3D_SM6_VER);

		#if USE_SHADER_MODEL_6_6
			ShaderModelHash ^= 0x96ED7F56;
		#endif

			return HashCombine(DxcVersionHash, ShaderModelHash);
		}
		else if (Format == NAME_PCD3D_SM5)
		{
			// Technically not needed for regular SM5 compiled with legacy compiler,
			// but PCD3D_SM5 currently includes ray tracing shaders that are compiled with new compiler stack.
			return HashCombine(DxcVersionHash, GetTypeHash(UE_SHADER_PCD3D_SM5_VER));
		}
		else if (Format == NAME_PCD3D_ES3_1) 
		{
			// Shader DXC signature is intentionally not included, as ES3_1 target always uses legacy compiler.
			return UE_SHADER_PCD3D_ES3_1_VER;
		}
		else if (Format == NAME_D3D_ES3_1_HOLOLENS)
		{
			// Shader DXC signature is intentionally not included, as ES3_1 target always uses legacy compiler.
			return UE_SHADER_D3D_ES3_1_HOLOLENS_VER;
		}
		checkf(0, TEXT("Unknown Format %s"), *Format.ToString());
		return 0;
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_PCD3D_SM6);
		OutFormats.Add(NAME_PCD3D_SM5);
		OutFormats.Add(NAME_PCD3D_ES3_1);
		OutFormats.Add(NAME_D3D_ES3_1_HOLOLENS);
	}

	virtual void CompileShader(FName Format, const struct FShaderCompilerInput& Input, struct FShaderCompilerOutput& Output,const FString& WorkingDirectory) const
	{
		CheckFormat(Format);
		if (Format == NAME_PCD3D_SM6)
		{
			CompileShader_Windows(Input, Output, WorkingDirectory, ELanguage::SM6);
		}
		else if(Format == NAME_PCD3D_SM5)
		{
			CompileShader_Windows(Input, Output, WorkingDirectory, ELanguage::SM5);
		}
		else if (Format == NAME_PCD3D_ES3_1)
		{
			CompileShader_Windows(Input, Output, WorkingDirectory, ELanguage::ES3_1);
		}
		else if (Format == NAME_D3D_ES3_1_HOLOLENS)
		{
			CompileShader_Windows(Input, Output, WorkingDirectory, ELanguage::ES3_1);
		}
		else
		{
			checkf(0, TEXT("Unknown format %s"), *Format.ToString());
		}
	}

	void AddShaderTargetDefines(FShaderCompilerInput& Input, uint32 ShaderTargetMajor, uint32 ShaderTargetMinor) const
	{
		// Inserting our own versions of these defines since we preprocess our shader source before we actually use something that defines them.
		Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MAJOR"), ShaderTargetMajor);
		Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MINOR"), ShaderTargetMinor);
	}

	void ModifyShaderCompilerInput(FShaderCompilerInput& Input) const final
	{
		CheckFormat(Input.ShaderFormat);
		if (Input.ShaderFormat == NAME_PCD3D_SM6 || Input.IsRayTracingShader())
		{
			Input.Environment.SetDefine(TEXT("SM6_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("COMPILER_DXC"), 1);
			Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_UB_STRUCT"), 1);

			if (USE_SHADER_MODEL_6_6)
			{
				AddShaderTargetDefines(Input, 6, 6);
			}
			else
			{
				AddShaderTargetDefines(Input, 6, 5);
			}
		}
		else if (Input.ShaderFormat == NAME_PCD3D_SM5)
		{
			Input.Environment.SetDefine(TEXT("SM5_PROFILE"), 1);
			const bool bUseDXC =
				Input.Environment.CompilerFlags.Contains(CFLAG_WaveOperations)
				|| Input.Environment.CompilerFlags.Contains(CFLAG_ForceDXC);
			Input.Environment.SetDefine(TEXT("COMPILER_DXC"), bUseDXC);

			if (bUseDXC)
			{
				AddShaderTargetDefines(Input, 6, 0);
			}
			else
			{
				AddShaderTargetDefines(Input, 5, 0);
			}
		}
		else if (Input.ShaderFormat == NAME_PCD3D_ES3_1)
		{
			Input.Environment.SetDefine(TEXT("ES3_1_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("COMPILER_DXC"), 0);
			Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MAJOR"), 5);
			Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MINOR"), 0);
		}
		else if (Input.ShaderFormat == NAME_D3D_ES3_1_HOLOLENS)
		{
			Input.Environment.SetDefine(TEXT("ES3_1_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("COMPILER_DXC"), 0);
			Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MAJOR"), 5);
			Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MINOR"), 0);
		}
		else
		{
			checkf(0, TEXT("Unknown format %s"), *Input.ShaderFormat.ToString());
		}
	}

	virtual const TCHAR* GetPlatformIncludeDirectory() const
	{
		return TEXT("D3D");
	}
};


/**
 * Module for D3D shaders
 */

static IShaderFormat* Singleton = nullptr;

class FShaderFormatD3DModule : public IShaderFormatModule, public FDxcModuleWrapper
{
public:
	virtual ~FShaderFormatD3DModule()
	{
		delete Singleton;
		Singleton = nullptr;
	}

	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatD3D(FDxcModuleWrapper::GetModuleVersionHash());
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FShaderFormatD3DModule, ShaderFormatD3D);
