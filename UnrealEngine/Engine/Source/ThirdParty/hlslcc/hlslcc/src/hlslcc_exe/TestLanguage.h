// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FTestCodeBackend : public FCodeBackend
{
	FTestCodeBackend(unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget) : FCodeBackend(InHlslCompileFlags, InTarget) {}

	// Returns false if any issues
	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) override
	{
		ir_function_signature* EntryPointSig = FindEntryPointFunction(Instructions, ParseState, EntryPoint);
		if (EntryPointSig)
		{
			EntryPointSig->is_main = true;
			return true;
		}

		return false;
	}

	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override
	{
		IRDump(ir, ParseState);
		return 0;
	}
};
#define FRAMEBUFFER_FETCH_ES2	"FramebufferFetchES2"
#include "ir.h"

struct FTestLanguageSpec : public ILanguageSpec
{
	virtual bool SupportsDeterminantIntrinsic() const { return false; }
	virtual bool SupportsTransposeIntrinsic() const { return false; }
	virtual bool SupportsIntegerModulo() const { return true; }
	virtual bool AllowsSharingSamplers() const { return false; }

	virtual bool SupportsSinCosIntrinsic() const { return false; }

	// half3x3 <-> float3x3
	virtual bool SupportsMatrixConversions() const { return false; }
	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir)
	{
		make_intrinsic_genType(ir, State, FRAMEBUFFER_FETCH_ES2, ir_invalid_opcode, IR_INTRINSIC_FLOAT, 0, 4, 4);

		{
			/**
			* Create GLSL functions that are left out of the symbol table
			*  Prevent pollution, but make them so thay can be used to
			*  implement the hlsl barriers
			*/
			const int glslFuncCount = 7;
			const char * glslFuncName[glslFuncCount] =
			{
				"barrier", "memoryBarrier", "memoryBarrierAtomicCounter", "memoryBarrierBuffer",
				"memoryBarrierShared", "memoryBarrierImage", "groupMemoryBarrier"
			};
			ir_function* glslFuncs[glslFuncCount];

			for (int i = 0; i < glslFuncCount; i++)
			{
				void* ctx = State;
				ir_function* func = new(ctx)ir_function(glslFuncName[i]);
				ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
				sig->is_builtin = true;
				func->add_signature(sig);
				ir->push_tail(func);
				glslFuncs[i] = func;
			}

			/** Implement HLSL barriers in terms of GLSL functions */
			const char * functions[] =
			{
				"GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync",
				"DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
				"AllMemoryBarrier", "AllMemoryBarrierWithGroupSync"
			};
			const int max_children = 4;
			ir_function * implFuncs[][max_children] =
			{
				{glslFuncs[4]} /**{"memoryBarrierShared"}*/,
				{glslFuncs[4], glslFuncs[0]} /**{"memoryBarrierShared","barrier"}*/,
				{glslFuncs[2], glslFuncs[3], glslFuncs[5]} /**{"memoryBarrierAtomicCounter", "memoryBarrierBuffer", "memoryBarrierImage"}*/,
				{glslFuncs[2], glslFuncs[3], glslFuncs[5], glslFuncs[0]} /**{"memoryBarrierAtomicCounter", "memoryBarrierBuffer", "memoryBarrierImage", "barrier"}*/,
				{glslFuncs[1]} /**{"memoryBarrier"}*/,
				{glslFuncs[1], glslFuncs[0]} /**{"groupMemoryBarrier","barrier"}*/
			};

			for (size_t i = 0; i < sizeof(functions) / sizeof(const char*); i++)
			{
				void* ctx = State;
				ir_function* func = new(ctx)ir_function(functions[i]);

				ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
				sig->is_builtin = true;
				sig->is_defined = true;

				for (int j = 0; j < max_children; j++)
				{
					if (implFuncs[i][j] == NULL)
						break;
					ir_function* child = implFuncs[i][j];
					check(child);
					check(child->signatures.get_head() == child->signatures.get_tail());
					ir_function_signature *childSig = (ir_function_signature *)child->signatures.get_head();
					exec_list actual_parameter;
					sig->body.push_tail(
						new(ctx)ir_call(childSig, NULL, &actual_parameter)
					);
				}

				func->add_signature(sig);

				State->symbols->add_global_function(func);
				ir->push_tail(func);
			}
		}
	}
};

