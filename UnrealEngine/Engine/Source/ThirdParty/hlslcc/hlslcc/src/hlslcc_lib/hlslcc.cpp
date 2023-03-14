// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderCompilerCommon.h"
#include "hlslcc.h"
#include "hlslcc_private.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "ir_optimization.h"
#include "loop_analysis.h"
#include "macros.h"
#include "ir_track_image_access.h"
#include "ir_print_visitor.h"
#include "ir_rvalue_visitor.h"
#include "PackUniformBuffers.h"
#include "OptValueNumbering.h"
#include "IRDump.h"

extern int _mesa_hlsl_debug;

static char* DebugBuffer = 0;
#if WIN32
#include <Windows.h>
void dprintf(const char* Format, ...)
{
	const int BufSize = (1 << 20);
	va_list Args;
	int Count;

	if (DebugBuffer == nullptr)
	{
		DebugBuffer = (char*)malloc(BufSize);
	}

	va_start(Args, Format);
#if WIN32
	Count = vsnprintf_s(DebugBuffer, BufSize, _TRUNCATE, Format, Args);
#else
	Count = vsnprintf(DebugBuffer, BufSize, Format, Args);
#endif
	va_end(Args);

	if (Count < -1)
	{
		// Overflow, add a line feed and null terminate the string.
		DebugBuffer[BufSize - 2] = '\n';
		DebugBuffer[BufSize - 1] = 0;
	}
	else
	{
		// Make sure the string is null terminated.
		DebugBuffer[Count] = 0;
	}

//#if WIN32
	OutputDebugStringA(DebugBuffer);
//#elif __APPLE__
//	syslog(LOG_DEBUG, "%s", DebugBuffer);
//#endif
	fprintf(stdout, "%s", DebugBuffer);
}
#endif

void dprintf_free()
{
	if (DebugBuffer)
	{
		free(DebugBuffer);
		DebugBuffer = nullptr;
	}
}

static const _mesa_glsl_parser_targets FrequencyTable[] =
{
	vertex_shader,
	fragment_shader,
	geometry_shader,
	tessellation_control_shader,
	tessellation_evaluation_shader,
	compute_shader
};

static const int VersionTable[HCT_InvalidTarget] =
{
	150,
	310,
	430,
	150,
	310,
};

/**
 * Optimize IR.
 * @param ir - The IR to optimize.
 * @param ParseState - Parse state.
 */
static void OptimizeIR(exec_list* ir, _mesa_glsl_parse_state* ParseState);


FHlslCrossCompilerContext::FHlslCrossCompilerContext(int InFlags, EHlslShaderFrequency InShaderFrequency, EHlslCompileTarget InCompileTarget) :
	MemContext(nullptr),
	ParseState(nullptr),
	ir(nullptr),
	Flags(InFlags),
	ShaderFrequency(InShaderFrequency),
	CompileTarget(InCompileTarget)
{
	const bool bIsES2 = (InCompileTarget == HCT_FeatureLevelES2);
	const bool bIsES3_1 = (InCompileTarget == HCT_FeatureLevelES3_1);
	if (bIsES2)
	{
		// ES implies some flag modifications
		Flags |= HLSLCC_PackUniforms | HLSLCC_FlattenUniformBuffers | HLSLCC_FlattenUniformBufferStructures | HLSLCC_ExpandUBMemberArrays;
	}
	else if (bIsES3_1)
	{
		// ES implies some flag modifications
		Flags |= HLSLCC_PackUniforms;// | HLSLCC_FlattenUniformBuffers;
	}
}

FHlslCrossCompilerContext::~FHlslCrossCompilerContext()
{
	if (MemContext)
	{
		if (ParseState)
		{
			delete ParseState->symbols;
			ParseState->~_mesa_glsl_parse_state();
		}

		_mesa_glsl_release_types();
		//TIMER(cleanup);

		ralloc_free(MemContext);
	}
}

bool FHlslCrossCompilerContext::Init(
	const char* InSourceFilename,
	struct ILanguageSpec* InLanguageSpec)
{
	const bool bIsES2 = (CompileTarget == HCT_FeatureLevelES2);
	const bool bIsES3_1 = (CompileTarget == HCT_FeatureLevelES3_1);

	if (ShaderFrequency < HSF_VertexShader || ShaderFrequency > HSF_ComputeShader ||
		CompileTarget < HCT_FeatureLevelSM4 || CompileTarget >= HCT_InvalidTarget)
	{
		return false;
	}

	if ((ShaderFrequency == HSF_HullShader || ShaderFrequency == HSF_DomainShader) &&
		CompileTarget <= HCT_FeatureLevelSM4)
	{
		return false;
	}

	if (ShaderFrequency == HSF_ComputeShader && (CompileTarget < HCT_FeatureLevelES3_1Ext || CompileTarget == HCT_FeatureLevelES2))
	{
		return false;
	}

	if (bIsES2 && ShaderFrequency != HSF_VertexShader && ShaderFrequency != HSF_PixelShader)
	{
		// ES 2 only supports VS & PS
		return false;
	}
	else if (bIsES3_1 && ShaderFrequency != HSF_VertexShader && ShaderFrequency != HSF_PixelShader && ShaderFrequency != HSF_ComputeShader)
	{
		// ES 3.1 supports VS, PS & CS
		return false;
	}

	const bool bFlattenUniformBuffers = ((Flags & HLSLCC_FlattenUniformBuffers) == HLSLCC_FlattenUniformBuffers);
	const bool bSeparateShaderObjects = (Flags & HLSLCC_SeparateShaderObjects) != 0;

	MemContext = ralloc_context(0);
	ParseState = new(MemContext)_mesa_glsl_parse_state(
		MemContext,
		FrequencyTable[ShaderFrequency],
		InLanguageSpec,
		VersionTable[CompileTarget]
		);
	ParseState->base_source_file = ralloc_strdup(MemContext, InSourceFilename);
	ParseState->error = 0;
	ParseState->adjust_clip_space_dx11_to_opengl = (Flags & HLSLCC_DX11ClipSpace) != 0;
	ParseState->bFlattenUniformBuffers = bFlattenUniformBuffers;
	ParseState->bGenerateES = bIsES2;
	ParseState->bGenerateLayoutLocations = (CompileTarget == HCT_FeatureLevelSM5) || (CompileTarget == HCT_FeatureLevelES3_1Ext) || (CompileTarget == HCT_FeatureLevelES3_1) || bSeparateShaderObjects;
	ParseState->bSeparateShaderObjects = bSeparateShaderObjects;
	glsl_type::SetTransientContext(MemContext);
	return true;
}


static bool TrySimplePreprocessor(_mesa_glsl_parse_state* ParseState, const char** InOutShaderSource)
{
	const char* Source = *InOutShaderSource;
	char* Dest = ralloc_strdup(ParseState, Source);
	char* Ptr = Dest;
	while (*Ptr)
	{
		if (*Ptr == '#')
		{
			if (Ptr[1] == 'l' && Ptr[2] == 'i' && Ptr[3] == 'n' && Ptr[4] == 'e')
			{
				// Skip to EOL
				while (Ptr && *Ptr != '\n')
				{
					++Ptr;
				}
			}
			else
			{
				// Directive not supported
				return false;
			}
		}
		else if (*Ptr == '/')
		{
			if (Ptr[1] == '*')
			{
				while (*Ptr)
				{
					if (*Ptr == '\n')
					{
						++Ptr;
					}
					else if (Ptr[0] == '*' && Ptr[1] == '/')
					{
						Ptr[0] = ' ';
						Ptr[1] = ' ';
						break;
					}
					else
					{
						*Ptr = ' ';
						++Ptr;
					}
				}
			}
			else if (Ptr[1] == '/')
			{
				Ptr[0] = ' ';
				Ptr[1] = ' ';
				Ptr += 2;
				// Skip to EOL
				while (Ptr && *Ptr != '\n')
				{
					*Ptr = ' ';
					++Ptr;
				}
			}
		}

		++Ptr;
	}

	*InOutShaderSource = Dest;
	return true;
}

bool FHlslCrossCompilerContext::RunFrontend(const char** InOutShaderSource)
{
	if (!TrySimplePreprocessor(ParseState, InOutShaderSource))
	{
		const bool bPreprocess = (Flags & HLSLCC_NoPreprocess) == 0;
		if (bPreprocess)
		{
			ParseState->error = preprocess(ParseState, InOutShaderSource, &ParseState->info_log);
			//TIMER(preprocess);
			if (ParseState->error != 0)
			{
				return false;
			}
		}
	}

	// Enable to debug the parser state machine (Flex & Bison), enable #define YYDEBUG 1 on hlsl_parser.yy
	//_mesa_hlsl_debug = 1;

	_mesa_hlsl_lexer_ctor(ParseState, *InOutShaderSource);
	_mesa_hlsl_parse(ParseState);
	_mesa_hlsl_lexer_dtor(ParseState);

	//TIMER(parse);
	if (ParseState->error != 0 || ParseState->translation_unit.is_empty())
	{
		return false;
	}

	/**
	 * Debug only functionality to write out the AST to stdout
	 */
	const bool bPrintAST = (Flags & HLSLCC_PrintAST) != 0;
	if (bPrintAST)
	{
		printf( "###########################################################################\n");
		printf( "## Begin AST dump\n");
		_mesa_ast_print(ParseState);
		printf( "## End AST dump\n");
		printf( "###########################################################################\n");
	}
	ir = new(MemContext) exec_list();
	_mesa_ast_to_hir(ir, ParseState);
	//TIMER(ast_to_hir);
	//IRDump(ir);
	if (ParseState->error != 0 || ir->is_empty())
	{
		return false;
	}

	const bool bValidate = (Flags & HLSLCC_NoValidation) == 0;
	if (bValidate)
	{
		validate_ir_tree(ir, ParseState);
		//TIMER(validate);
		if (ParseState->error != 0)
		{
			return false;
		}
	}

	const bool bIsES2 = (CompileTarget == HCT_FeatureLevelES2);
	if (bIsES2)
	{
		ParseState->language_version = 100;
	}

	return true;
}

bool FHlslCrossCompilerContext::RunBackend(
	const char* InShaderSource,
	const char* InEntryPoint,
	FCodeBackend* InShaderBackEnd)
{
	if (InShaderBackEnd == nullptr)
	{
		_mesa_glsl_error(
			ParseState,
			"No Shader code generation backend specified!"
			);
		return false;
	}

	ParseState->bInBackEnd = true;

	if (!InShaderBackEnd->GenerateMain(ShaderFrequency, InEntryPoint, ir, ParseState))
	{
		return false;
	}
	//TIMER(gen_main);

	//IRDump(ir);
	if (Flags & HLSLCC_DisableBackendOptimizations)
	{
		if (!InShaderBackEnd->Validate(ir, ParseState))
		{
			return false;
		}
	}
	else
	{
		if (!InShaderBackEnd->OptimizeAndValidate(ir, ParseState))
		{
			return false;
		}
	}

	// Fix the case where a variable is used with an atomic and also w/o an atomic access
	if (Flags & HLSLCC_FixAtomicReferences)
	{
		TIRVarSet AtomicVariables;
		FindAtomicVariables(ir, AtomicVariables);
		FixAtomicReferences(ir, ParseState, AtomicVariables);
	}

	{
		// Extract sampler states
		if (!ExtractSamplerStatesNameInformation(ir, ParseState))
		{
			return false;
		}

		if (Flags & HLSLCC_DisableBackendOptimizations)
		{
			if (!InShaderBackEnd->Validate(ir, ParseState))
			{
				return false;
			}
		}
		else
		{
			if (!InShaderBackEnd->OptimizeAndValidate(ir, ParseState))
			{
				return false;
			}
		}
	}

	const bool bPackUniforms = (Flags & HLSLCC_PackUniforms) != 0;
	const bool bFlattenUBStructures = ((Flags & HLSLCC_FlattenUniformBufferStructures) == HLSLCC_FlattenUniformBufferStructures);

	if (bPackUniforms)
	{
		if (bFlattenUBStructures)
		{
			FlattenUniformBufferStructures(ir, ParseState);
			
			if ((Flags & HLSLCC_ExpandUBMemberArrays) == HLSLCC_ExpandUBMemberArrays)
			{
				// this is needed to help out packing uniform buffers into global arrays 
				ExpandUniformBufferArrays(ir, ParseState);
			}
						
			validate_ir_tree(ir, ParseState);

			if (!InShaderBackEnd->OptimizeAndValidate(ir, ParseState))
			{
				return false;
			}
		}
	}

	{
		if (!InShaderBackEnd->ApplyAndVerifyPlatformRestrictions(ir, ParseState, ShaderFrequency))
		{
			return false;
		}

		if (Flags & HLSLCC_DisableBackendOptimizations)
		{
			if (!InShaderBackEnd->Validate(ir, ParseState))
			{
				return false;
			}
		}
		else
		{
			if (!InShaderBackEnd->OptimizeAndValidate(ir, ParseState))
			{
				return false;
			}
		}
	}

	if (bPackUniforms)
	{
		TVarVarMap UniformMap;
		PackUniforms(Flags, ir, ParseState, UniformMap);
		//TIMER(pack_uniforms);

		RemovePackedUniformBufferReferences(ir, ParseState, UniformMap);

		if (!InShaderBackEnd->OptimizeAndValidate(ir, ParseState))
		{
			return false;
		}
	}

	const bool bDoCSE = (Flags & HLSLCC_ApplyCommonSubexpressionElimination) != 0;
	if (bDoCSE)
	{
		if (LocalValueNumbering(ir, ParseState))
		{
			if (!InShaderBackEnd->OptimizeAndValidate(ir, ParseState))
			{
				return false;
			}
		}
	}

	const bool bDoSubexpressionExpansion = (Flags & HLSLCC_ExpandSubexpressions) != 0;
	if (bDoSubexpressionExpansion)
	{
		ExpandSubexpressions(ir, ParseState);
	}

	// pass over the shader to tag image accesses
	TrackImageAccess(ir, ParseState);

	// Just run validation once at the end to make sure it is OK in release mode
	if (!InShaderBackEnd->Validate(ir, ParseState))
	{
		return false;
	}

	return true;
}

bool FHlslCrossCompilerContext::Run(
	const char* InShaderSource,
	const char* InEntryPoint,
	FCodeBackend* InShaderBackEnd,
	char** OutShaderSource,
	char** OutErrorLog)
{
	if (InShaderSource == 0 || OutShaderSource == 0 || OutErrorLog == 0)
	{
		return 0;
	}

	*OutShaderSource = 0;
	*OutErrorLog = 0;

	if (RunFrontend(&InShaderSource))
	{
		if (RunBackend(InShaderSource, InEntryPoint, InShaderBackEnd))
		{
			check(ParseState->error == 0);
			*OutShaderSource = InShaderBackEnd->GenerateCode(ir, ParseState, ShaderFrequency);
			//TIMER(gen_glsl);
		}
	}

	if (ParseState->info_log && ParseState->info_log[0] != 0)
	{
		*OutErrorLog = strdup(ParseState->info_log);
	}

	return !ParseState->error;
}

/**
 * Parses a semantic in to its base name and index.
 * @param MemContext - Memory context with which allocations can be made.
 * @param InSemantic - The semantic to parse.
 * @param OutSemantic - Upon return contains the base semantic.
 * @param OutIndex - Upon return contains the semantic index.
 */
void ParseSemanticAndIndex(
	void* MemContext,
	const char* InSemantic,
	const char** OutSemantic,
	int* OutIndex
	)
{
	check(InSemantic);
	const size_t SemanticLen = strlen(InSemantic);
	const char* p = InSemantic + SemanticLen - 1;

	*OutIndex = 0;
	while (p >= InSemantic && *p >= '0' && *p <= '9')
	{
		*OutIndex = *OutIndex * 10 + (*p - '0');
		p--;
	}
	*OutSemantic = ralloc_strndup(MemContext, InSemantic, p - InSemantic + 1);
}

/**
 * Optimize IR.
 * @param ir - The IR to optimize.
 * @param ParseState - Parse state.
 */
static void DoOptimizeIR(exec_list* ir, _mesa_glsl_parse_state* ParseState, bool bPerformGlobalDeadCodeRemoval)
{
	bool bProgress = false;
	do 
	{
		bProgress = MoveGlobalInstructionsToMain(ir);
		bProgress = do_optimization_pass(ir, ParseState, bPerformGlobalDeadCodeRemoval) || bProgress;
		if (bPerformGlobalDeadCodeRemoval)
		{
			bProgress = ExpandArrayAssignments(ir, ParseState) || bProgress;
		}
	} while (bProgress);
}

static void OptimizeIR(exec_list* ir, _mesa_glsl_parse_state* ParseState)
{
	// We split this into two passes, as there is an issue when we set a value into a static global and the global dead code removal
	// will remove the assignment, leaving the static uninitialized; this happens when a static has a non-const initializer, then
	// is read in a function that's not inline yet; the IR will see a reference, then an assignment
	// so it will then remove the assignment as it thinks it's not used (as it hasn't inlined the function where it will read it!)
	DoOptimizeIR(ir, ParseState, false);
	//do_function_inlining(ir);
	DoOptimizeIR(ir, ParseState, true);
}

/**
 * Moves any instructions in the global instruction stream to the beginning
 * of main. This can happen due to conversions and initializers of global
 * variables. Note however that instructions can be moved iff main() is the
 * only function in the program!
 */
bool MoveGlobalInstructionsToMain(exec_list* Instructions)
{
	ir_function_signature* MainSig = NULL;
	int NumFunctions = 0;
		
	foreach_iter(exec_list_iterator, iter, *Instructions)
	{
		ir_instruction *ir = (ir_instruction *)iter.get();
		ir_function* Function = ir->as_function();
		if (Function)
		{
			foreach_iter(exec_list_iterator, sigiter, *Function)
			{
				ir_function_signature* Sig = (ir_function_signature *)sigiter.get();
				if (Sig->is_main)
				{
					MainSig = Sig;
				}
				if (Sig->is_defined && !Sig->is_builtin)
				{
					NumFunctions++;
				}
			}
		}
	}

	if (MainSig)
	{
		exec_list GlobalIR;
		const bool bMoveGlobalVars = (NumFunctions == 1);

		foreach_iter(exec_list_iterator, iter, *Instructions)
		{
			ir_instruction *ir = (ir_instruction *)iter.get();
			switch (ir->ir_type)
			{
			case ir_type_variable:
				{
					ir_variable* Var = (ir_variable*)ir;
					const bool bBuiltin = Var->name && strncmp(Var->name, "gl_", 3) == 0;
					const bool bTemp = (Var->mode == ir_var_temporary) ||
						(Var->mode == ir_var_auto && bMoveGlobalVars);

					if (!bBuiltin && bTemp)
					{
						Var->remove();
						GlobalIR.push_tail(Var);
					}
				}
				break;

			case ir_type_assignment:
				ir->remove();
				GlobalIR.push_tail(ir);
				break;

			default:
				break;
			}
		}

		if (!GlobalIR.is_empty())
		{
			GlobalIR.append_list(&MainSig->body);
			GlobalIR.move_nodes_to(&MainSig->body);
			return true;
		}
	}
	return false;
}


bool FCodeBackend::Optimize(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	if (ParseState->error != 0)
	{
		return false;
	}
	OptimizeIR(Instructions, ParseState);
	//TIMER(optimize);

	return (ParseState->error == 0);
}

bool FCodeBackend::Validate(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	if (ParseState->error != 0)
	{
		return false;
	}

#ifndef NDEBUG
	// This validation always runs. The optimized IR is very small and you really
	// want to know if the final IR is valid.
	validate_ir_tree(Instructions, ParseState);
	//TIMER(validate);
	if (ParseState->error != 0)
	{
		return false;
	}
#endif

	return true;
}

ir_function_signature* FCodeBackend::FindEntryPointFunction(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, const char* EntryPoint)
{
	ir_function_signature* EntryPointSig = nullptr;
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction *ir = (ir_instruction *)Iter.get();
		ir_function *Function = ir->as_function();
		if (Function && strcmp(Function->name, EntryPoint) == 0)
		{
			int NumSigs = 0;
			foreach_iter(exec_list_iterator, SigIter, *Function)
			{
				if (++NumSigs == 1)
				{
					EntryPointSig = (ir_function_signature *)SigIter.get();
				}
			}
			if (NumSigs == 1)
			{
				break;
			}
			else
			{
				_mesa_glsl_error(ParseState, "shader entry point "
					"'%s' has multiple signatures", EntryPoint);
			}
		}
	}

	return EntryPointSig;
}

ir_function_signature* FCodeBackend::GetMainFunction(exec_list* Instructions)
{
	ir_function_signature* EntryPointSig = nullptr;
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction *ir = (ir_instruction *)Iter.get();
		ir_function *Function = ir->as_function();
		if (Function)
		{
			int NumSigs = 0;
			foreach_iter(exec_list_iterator, SigIter, *Function)
			{
				if (++NumSigs == 1)
				{
					EntryPointSig = (ir_function_signature *)SigIter.get();
				}
			}
			if (NumSigs == 1 && EntryPointSig->is_main)
			{
				return EntryPointSig;
			}
		}
	}
	return nullptr;
}
