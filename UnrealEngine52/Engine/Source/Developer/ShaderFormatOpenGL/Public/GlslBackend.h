// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "hlslcc.h"
#include "LanguageSpec.h"

class SHADERFORMATOPENGL_API FGlslLanguageSpec : public ILanguageSpec
{
protected:
	bool bDefaultPrecisionIsHalf;

public:
	FGlslLanguageSpec(bool bInDefaultPrecisionIsHalf)
		: bDefaultPrecisionIsHalf(bInDefaultPrecisionIsHalf)
	{}

	virtual bool SupportsDeterminantIntrinsic() const override
	{
		return true;
	}

	virtual bool SupportsTransposeIntrinsic() const override
	{
		return true;
	}

	virtual bool SupportsIntegerModulo() const override
	{
		return true;
	}

	virtual bool SupportsMatrixConversions() const override { return true; }

	//#todo-rco: Enable
	virtual bool AllowsSharingSamplers() const override { return false; }

	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir) override;

	virtual bool AllowsImageLoadsForNonScalar() const { return true; }

	virtual bool EmulateStructuredWithTypedBuffers() const override { return false; }
};

class ir_variable;

// Generates GLSL compliant code from IR tokens
#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif // __GNUC__
struct SHADERFORMATOPENGL_API FGlslCodeBackend : public FCodeBackend
{
	FGlslCodeBackend(unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget) :
		FCodeBackend(InHlslCompileFlags, InTarget),
		bExplicitDepthWrites(false)
	{
	}

	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	// Return false if there were restrictions that made compilation fail
	virtual bool ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	/**
	* Generate a GLSL main() function that calls the entry point and handles
	* reading and writing all input and output semantics.
	* @param Frequency - The shader frequency.
	* @param EntryPoint - The name of the shader entry point.
	* @param Instructions - IR code.
	* @param ParseState - Parse state.
	*/
	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) override;

	ir_function_signature* FindPatchConstantFunction(exec_list* Instructions, _mesa_glsl_parse_state* ParseState);


	// subclass functionality
	virtual bool AllowsGlobalUniforms()
	{
		return true;
	}

	virtual bool AllowsESLanguage()
	{
		return true;
	}

	virtual bool WantsPrecisionModifiers()
	{
		return Target == HCT_FeatureLevelES3_1 || Target == HCT_FeatureLevelES3_1Ext;
	}

	bool bExplicitDepthWrites;
};


#ifdef __GNUC__
#pragma GCC visibility pop
#endif // __GNUC__
