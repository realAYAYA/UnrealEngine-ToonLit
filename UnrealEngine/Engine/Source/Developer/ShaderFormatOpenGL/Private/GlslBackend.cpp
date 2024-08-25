// Copyright Epic Games, Inc. All Rights Reserved.
// .

// This code is largely based on that in ir_print_glsl_visitor.cpp from
// glsl-optimizer.
// https://github.com/aras-p/glsl-optimizer
// The license for glsl-optimizer is reproduced below:

/*
	GLSL Optimizer is licensed according to the terms of the MIT license:

	Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
	Copyright (C) 2010-2011  Unity Technologies All Rights Reserved.

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
	BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
	AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
	CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "GlslBackend.h"
#include "hlslcc_private.h"
#include "compiler.h"

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#include "glsl_parser_extras.h"
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

#include "hash_table.h"
#include "ir_rvalue_visitor.h"
#include "PackUniformBuffers.h"
#include "IRDump.h"
//@todo-rco: Remove STL!
#include <sstream>
//#define OPTIMIZE_ANON_STRUCTURES_OUT
// We can't optimize them out presently, because apparently Windows Radeon
// OpenGL driver chokes on valid GLSL code then.

#if !PLATFORM_WINDOWS
#define _strdup strdup
#endif

static inline std::string FixHlslName(const glsl_type* Type)
{
	check(Type->is_image() || Type->is_vector() || Type->is_numeric() || Type->is_void() || Type->is_sampler() || Type->is_scalar());
	std::string Name = Type->name;
	if (Type == glsl_type::half_type)
	{
		return "float";
	}
	else if (Type == glsl_type::half2_type)
	{
		return "vec2";
	}
	else if (Type == glsl_type::half3_type)
	{
		return "vec3";
	}
	else if (Type == glsl_type::half4_type)
	{
		return "vec4";
	}
	else if (Type == glsl_type::half2x2_type)
	{
		return "mat2";
	}
	else if (Type == glsl_type::half2x3_type)
	{
		return "mat2x3";
	}
	else if (Type == glsl_type::half2x4_type)
	{
		return "mat2x4";
	}
	else if (Type == glsl_type::half3x2_type)
	{
		return "mat3x2";
	}
	else if (Type == glsl_type::half3x3_type)
	{
		return "mat3";
	}
	else if (Type == glsl_type::half3x4_type)
	{
		return "mat3x4";
	}
	else if (Type == glsl_type::half4x2_type)
	{
		return "mat4x2";
	}
	else if (Type == glsl_type::half4x3_type)
	{
		return "mat4x3";
	}
	else if (Type == glsl_type::half4x4_type)
	{
		return "mat4";
	}
	return Name;
}

/**
 * This table must match the ir_expression_operation enum.
 */
static const char * const GLSLExpressionTable[ir_opcode_count][4] =
	{
	{ "(~", ")", "", "" }, // ir_unop_bit_not,
	{ "not(", ")", "", "!" }, // ir_unop_logic_not,
	{ "(-", ")", "", "" }, // ir_unop_neg,
	{ "abs(", ")", "", "" }, // ir_unop_abs,
	{ "sign(", ")", "", "" }, // ir_unop_sign,
	{ "(1.0/(", "))", "", "" }, // ir_unop_rcp,
	{ "inversesqrt(", ")", "", "" }, // ir_unop_rsq,
	{ "sqrt(", ")", "", "" }, // ir_unop_sqrt,
	{ "exp(", ")", "", "" }, // ir_unop_exp,      /**< Log base e on gentype */
	{ "log(", ")", "", "" }, // ir_unop_log,	     /**< Natural log on gentype */
	{ "exp2(", ")", "", "" }, // ir_unop_exp2,
	{ "log2(", ")", "", "" }, // ir_unop_log2,
	{ "int(", ")", "", "" }, // ir_unop_f2i,      /**< Float-to-integer conversion. */
	{ "float(", ")", "", "" }, // ir_unop_i2f,      /**< Integer-to-float conversion. */
	{ "bool(", ")", "", "" }, // ir_unop_f2b,      /**< Float-to-boolean conversion */
	{ "float(", ")", "", "" }, // ir_unop_b2f,      /**< Boolean-to-float conversion */
	{ "bool(", ")", "", "" }, // ir_unop_i2b,      /**< int-to-boolean conversion */
	{ "int(", ")", "", "" }, // ir_unop_b2i,      /**< Boolean-to-int conversion */
	{ "uint(", ")", "", "" }, // ir_unop_b2u,
	{ "bool(", ")", "", "" }, // ir_unop_u2b,
	{ "uint(", ")", "", "" }, // ir_unop_f2u,
	{ "float(", ")", "", "" }, // ir_unop_u2f,      /**< Unsigned-to-float conversion. */
	{ "uint(", ")", "", "" }, // ir_unop_i2u,      /**< Integer-to-unsigned conversion. */
	{ "int(", ")", "", "" }, // ir_unop_u2i,      /**< Unsigned-to-integer conversion. */
	{ "int(", ")", "", "" }, // ir_unop_h2i,
	{ "float(", ")", "", "" }, // ir_unop_i2h,
	{ "(", ")", "", "" }, // ir_unop_h2f,
	{ "(", ")", "", "" }, // ir_unop_f2h,
	{ "bool(", ")", "", "" }, // ir_unop_h2b,
	{ "float(", ")", "", "" }, // ir_unop_b2h,
	{ "uint(", ")", "", "" }, // ir_unop_h2u,
	{ "uint(", ")", "", "" }, // ir_unop_u2h,
	{ "transpose(", ")", "", "" }, // ir_unop_transpose
	{ "any(", ")", "", "" }, // ir_unop_any,
	{ "all(", ")", "", "" }, // ir_unop_all,

	/**
	* \name Unary floating-point rounding operations.
	*/
	/*@{*/
	{ "trunc(", ")", "", "" }, // ir_unop_trunc,
	{ "ceil(", ")", "", "" }, // ir_unop_ceil,
	{ "floor(", ")", "", "" }, // ir_unop_floor,
	{ "fract(", ")", "", "" }, // ir_unop_fract,
	{ "round(", ")", "", "" }, // ir_unop_round,
	/*@}*/

	/**
	* \name Trigonometric operations.
	*/
	/*@{*/
	{ "sin(", ")", "", "" }, // ir_unop_sin,
	{ "cos(", ")", "", "" }, // ir_unop_cos,
	{ "tan(", ")", "", "" }, // ir_unop_tan,
	{ "asin(", ")", "", "" }, // ir_unop_asin,
	{ "acos(", ")", "", "" }, // ir_unop_acos,
	{ "atan(", ")", "", "" }, // ir_unop_atan,
	{ "sinh(", ")", "", "" }, // ir_unop_sinh,
	{ "cosh(", ")", "", "" }, // ir_unop_cosh,
	{ "tanh(", ")", "", "" }, // ir_unop_tanh,
	/*@}*/

	/**
	* \name Normalize.
	*/
	/*@{*/
	{ "normalize(", ")", "", "" }, // ir_unop_normalize,
	/*@}*/

	/**
	* \name Partial derivatives.
	*/
	/*@{*/
	{ "dFdx(", ")", "", "" }, // ir_unop_dFdx,
	{ "dFdy(", ")", "", "" }, // ir_unop_dFdy,
	{ "dfdx_fine(", ")", "", "" }, // ir_unop_dFdxFine,
	{ "dfdy_fine(", ")", "", "" }, // ir_unop_dFdyFine,
	{ "dfdx_coarse(", ")", "", "" }, // ir_unop_dFdxCoarse,
	{ "dfdy_coarse(", ")", "", "" }, // ir_unop_dFdyCoarse,
	/*@}*/

	{ "isnan(", ")", "", "" }, // ir_unop_isnan,
	{ "isinf(", ")", "", "" }, // ir_unop_isinf,

	{ "floatBitsToUint(", ")", "", "" }, // ir_unop_fasu,
	{ "floatBitsToInt(", ")", "", "" }, // ir_unop_fasi,
	{ "intBitsToFloat(", ")", "", "" }, // ir_unop_iasf,
	{ "uintBitsToFloat(", ")", "", "" }, // ir_unop_uasf,

	{ "bitfieldReverse(", ")", "", "" }, // ir_unop_bitreverse,
	{ "bitCount(", ")", "", "" }, // ir_unop_bitcount,
	{ "findMSB(", ")", "", "" }, // ir_unop_msb,
	{ "findLSB(", ")", "", "" }, // ir_unop_lsb,

	{ "ERROR_NO_SATURATE_FUNCS(", ")", "", "" }, // ir_unop_saturate,

	{ "ERROR_NO_NOISE_FUNCS(", ")", "", "" }, // ir_unop_noise,

	{ "(", "+", ")", "" }, // ir_binop_add,
	{ "(", "-", ")", "" }, // ir_binop_sub,
	{ "(", "*", ")", "" }, // ir_binop_mul,
	{ "(", "/", ")", "" }, // ir_binop_div,

	/**
	* Takes one of two combinations of arguments:
	*
	* - mod(vecN, vecN)
	* - mod(vecN, float)
	*
	* Does not take integer types.
	*/
	{ "mod(", ",", ")", "%" }, // ir_binop_mod,
	{ "modf(", ",", ")", "" }, // ir_binop_modf,

	{ "step(", ",", ")", "" }, // ir_binop_step,

	/**
	* \name Binary comparison operators which return a boolean vector.
	* The type of both operands must be equal.
	*/
	/*@{*/
	{ "lessThan(", ",", ")", "<" }, // ir_binop_less,
	{ "greaterThan(", ",", ")", ">" }, // ir_binop_greater,
	{ "lessThanEqual(", ",", ")", "<=" }, // ir_binop_lequal,
	{ "greaterThanEqual(", ",", ")", ">=" }, // ir_binop_gequal,
	{ "equal(", ",", ")", "==" }, // ir_binop_equal,
	{ "notEqual(", ",", ")", "!=" }, // ir_binop_nequal,
	/**
	* Returns single boolean for whether all components of operands[0]
	* equal the components of operands[1].
	*/
	{ "(", "==", ")", "" }, // ir_binop_all_equal,
	/**
	* Returns single boolean for whether any component of operands[0]
	* is not equal to the corresponding component of operands[1].
	*/
	{ "(", "!=", ")", "" }, // ir_binop_any_nequal,
	/*@}*/

	/**
	* \name Bit-wise binary operations.
	*/
	/*@{*/
	{ "(", "<<", ")", "" }, // ir_binop_lshift,
	{ "(", ">>", ")", "" }, // ir_binop_rshift,
	{ "(", "&", ")", "" }, // ir_binop_bit_and,
	{ "(", "^", ")", "" }, // ir_binop_bit_xor,
	{ "(", "|", ")", "" }, // ir_binop_bit_or,
	/*@}*/

	{ "bvec%d(uvec%d(", ")*uvec%d(", "))", "&&" }, // ir_binop_logic_and,
	{ "bvec%d(abs(ivec%d(", ")+ivec%d(", ")))", "^^" }, // ir_binop_logic_xor,
	{ "bvec%d(uvec%d(", ")+uvec%d(", "))", "||" }, // ir_binop_logic_or,

	{ "dot(", ",", ")", "" }, // ir_binop_dot,
	{ "cross(", ",", ")", "" }, // ir_binop_cross,
	{ "min(", ",", ")", "" }, // ir_binop_min,
	{ "max(", ",", ")", "" }, // ir_binop_max,
	{ "atan(", ",", ")", "" },
	{ "pow(", ",", ")", "" }, // ir_binop_pow,

	{ "mix(", ",", ",", ")" }, // ir_ternop_lerp,
	{ "smoothstep(", ",", ",", ")" }, // ir_ternop_smoothstep,
	{ "clamp(", ",", ",", ")" }, // ir_ternop_clamp,
	{ "ERROR_NO_FMA_FUNCS(", ",", ",", ")" }, // ir_ternop_fma,

	{ "ERROR_QUADOP_VECTOR(", ",", ")" }, // ir_quadop_vector,
};

static const char* OutputStreamTypeStrings[4] = {
	"!invalid!",
	"points",
	"line_strip",
	"triangle_strip"
};

static const char* GeometryInputStrings[6] = {
	"!invalid!",
	"points",
	"lines",
	"line_adjacency",
	"triangles",
	"triangles_adjacency"
};

static const char* DomainStrings[4] = {
	"!invalid!",
	"triangles",
	"quads",
	"isolines",
};

static const char* PartitioningStrings[5] = {
	"!invalid!",
	"equal_spacing",
	"fractional_even_spacing",
	"fractional_odd_spacing",
	"pow2",
};

static const char* OutputTopologyStrings[5] = {
	"!invalid!",
	"point_needs_to_be_fixed",
	"line_needs_to_be_fixed",
	"cw",
	"ccw",
};

static const char* GLSLIntCastTypes[5] =
{
	"!invalid!",
	"int",
	"ivec2",
	"ivec3",
	"ivec4",
};

const char* GL_FRAMEBUFFER_FETCH[5] =
{
	"GLFramebufferFetch0",
	"GLFramebufferFetch1",
	"GLFramebufferFetch2",
	"GLFramebufferFetch3",
	"GLFramebufferFetchDepth"
};

const char* GL_FRAMEBUFFER_FETCH_WRITE[5] =
{
	"GLFramebufferFetchWrite0",
	"GLFramebufferFetchWrite1",
	"GLFramebufferFetchWrite2",
	"GLFramebufferFetchWrite3",
	"GLFramebufferFetchDepthWrite"
};

const char* GL_PLS_IMAGE_FORMAT[5] =
{
	"r11f_g11f_b10f",
	"rgba8",
	"rgba8",
	"rgba8",
	""
};

const char* GL_FRAMEBUFFER_FETCH_TYPE[5] =
{
	"vec3",
	"vec4",
	"vec4",
	"vec4",
	"float"
};

static const uint32_t FRAMEBUFFER_FETCH_DEPTH_INDEX = 4;

static const char* FBF_StorageQualifier = "FBF_STORAGE_QUALIFIER ";
static_assert((sizeof(GLSLExpressionTable) / sizeof(GLSLExpressionTable[0])) == ir_opcode_count, "GLSLExpressionTableSizeMismatch");

struct SDMARange
{
	unsigned SourceCB;
	unsigned SourceOffset;
	unsigned Size;
	unsigned DestCBIndex;
	unsigned DestCBPrecision;
	unsigned DestOffset;

	bool operator <(SDMARange const & Other) const
	{
		if (SourceCB == Other.SourceCB)
		{
			return SourceOffset < Other.SourceOffset;
		}

		return SourceCB < Other.SourceCB;
	}
};
typedef std::list<SDMARange> TDMARangeList;
typedef std::map<unsigned, TDMARangeList> TCBDMARangeMap;


static void InsertRange( TCBDMARangeMap& CBAllRanges, unsigned SourceCB, unsigned SourceOffset, unsigned Size, unsigned DestCBIndex, unsigned DestCBPrecision, unsigned DestOffset ) 
{
	check(SourceCB < (1 << 12));
	check(DestCBIndex < (1 << 12));
	check(DestCBPrecision < (1 << 8));
	unsigned SourceDestCBKey = (SourceCB << 20) | (DestCBIndex << 8) | DestCBPrecision;
	SDMARange Range = { SourceCB, SourceOffset, Size, DestCBIndex, DestCBPrecision, DestOffset };

	TDMARangeList& CBRanges = CBAllRanges[SourceDestCBKey];
//printf("* InsertRange: %08x\t%u:%u - %u:%c:%u:%u\n", SourceDestCBKey, SourceCB, SourceOffset, DestCBIndex, DestCBPrecision, DestOffset, Size);
	if (CBRanges.empty())
	{
		CBRanges.push_back(Range);
	}
	else
	{
		TDMARangeList::iterator Prev = CBRanges.end();
		bool bAdded = false;
		for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
		{
			if (SourceOffset + Size <= Iter->SourceOffset)
			{
				if (Prev == CBRanges.end())
				{
					CBRanges.push_front(Range);
				}
				else
				{
					CBRanges.insert(Iter, Range);
				}

				bAdded = true;
				break;
			}

			Prev = Iter;
		}

		if (!bAdded)
		{
			CBRanges.push_back(Range);
		}

		if (CBRanges.size() > 1)
		{
			// Try to merge ranges
			bool bDirty = false;
			do
			{
				bDirty = false;
				TDMARangeList NewCBRanges;
				for (auto Iter = CBRanges.begin(); Iter != CBRanges.end(); ++Iter)
				{
					if (Iter == CBRanges.begin())
					{
						Prev = CBRanges.begin();
					}
					else
					{
						if (Prev->SourceOffset + Prev->Size == Iter->SourceOffset && Prev->DestOffset + Prev->Size == Iter->DestOffset)
						{
							SDMARange Merged = *Prev;
							Merged.Size = Prev->Size + Iter->Size;
							NewCBRanges.pop_back();
							NewCBRanges.push_back(Merged);
							++Iter;
							NewCBRanges.insert(NewCBRanges.end(), Iter, CBRanges.end());
							bDirty = true;
							break;
						}
					}

					NewCBRanges.push_back(*Iter);
					Prev = Iter;
				}

				CBRanges.swap(NewCBRanges);
			}
			while (bDirty);
		}
	}
}

static TDMARangeList SortRanges( TCBDMARangeMap& CBRanges ) 
{
	TDMARangeList Sorted;
	for (auto& Pair : CBRanges)
	{
		Sorted.insert(Sorted.end(), Pair.second.begin(), Pair.second.end());
	}

	Sorted.sort();

	return Sorted;
}

static void DumpSortedRanges(TDMARangeList& SortedRanges)
{
	printf("**********************************\n");
	for (auto& o : SortedRanges)
	{
		printf("\t%u:%u - %u:%c:%u:%u\n", o.SourceCB, o.SourceOffset, o.DestCBIndex, o.DestCBPrecision, o.DestOffset, o.Size);
	}
}

// Returns true if the passed 'intrinsic' is used
static bool UsesUEIntrinsic(exec_list* Instructions, const char * UEIntrinsic)
{
	struct SFindUEIntrinsic : public ir_hierarchical_visitor
	{
		bool bFound;
		const char * UEIntrinsic;
		SFindUEIntrinsic(const char * InUEIntrinsic) : bFound(false), UEIntrinsic(InUEIntrinsic) {}

		virtual ir_visitor_status visit_enter(ir_call* IR) override
		{
			if (IR->use_builtin && !strcmp(IR->callee_name(), UEIntrinsic))
			{
				bFound = true;
				return visit_stop;
			}

			return visit_continue;
		}
	};

	SFindUEIntrinsic Visitor(UEIntrinsic);
	Visitor.run(Instructions);
	return Visitor.bFound;
}


/**
 * IR visitor used to generate GLSL. Based on ir_print_visitor.
 */
class ir_gen_glsl_visitor : public ir_visitor
{
	/** Track which multi-dimensional arrays are used. */
	struct md_array_entry : public exec_node
	{
		const glsl_type* type;
	};

	/** Track external variables. */
	struct extern_var : public exec_node
	{
		ir_variable* var;
		explicit extern_var(ir_variable* in_var) : var(in_var) {}
	};

	/** External variables. */
	exec_list input_variables;
	exec_list output_variables;
	exec_list uniform_variables;
	exec_list sampler_variables;
	exec_list image_variables;

	/** Data tied globally to the shader via attributes */
	bool early_depth_stencil;
	int wg_size_x;
	int wg_size_y;
	int wg_size_z;

	glsl_tessellation_info tessellation;

	/** Track global instructions. */
	struct global_ir : public exec_node
	{
		ir_instruction* ir;
		explicit global_ir(ir_instruction* in_ir) : ir(in_ir) {}
	};

	/** Global instructions. */
	exec_list global_instructions;

	/** A mapping from ir_variable * -> unique printable names. */
	hash_table *printable_names;
	/** Structures required by the code. */
	hash_table *used_structures;
	/** Uniform block variables required by the code. */
	hash_table *used_uniform_blocks;
	/** Multi-dimensional arrays required by the code. */
	exec_list used_md_arrays;

	// Code generation flags
	bool bIsES;
	bool bEmitPrecision;
	bool bIsES31;
	bool bIsWebGL;
	EHlslCompileTarget CompileTarget;
	_mesa_glsl_parser_targets ShaderTarget;

	bool bGenerateLayoutLocations;
	bool bDefaultPrecisionIsHalf;

	// framebuffer fetch is in use
	bool bUsesFrameBufferFetch;

	// depthbuffer fetch is in use
	bool bUsesDepthbufferFetch;
	bool bHasGeneratedDepthTargetInput;

	// Mask for indicies using FBF
	uint32 FramebufferFetchMask;
	uint32 FramebufferFetchWriteMask;

	// uses external texture
	bool bUsesExternalTexture;

	/** Memory context within which to make allocations. */
	void *mem_ctx;
	/** Buffer to which GLSL source is being generated. */
	char** buffer;
	/** Indentation level. */
	int indentation;
	/** Scope depth. */
	int scope_depth;
	/** The number of temporary variables declared in the current scope. */
	int temp_id;
	/** The number of global variables declared. */
	int global_id;
	/** Whether a semicolon must be printed before the next EOL. */
	bool needs_semicolon;
	/**
	 * Whether uint literals should be printed as int literals. This is a hack
	 * because glCompileShader crashes on Mac OS X with code like this:
	 * foo = bar[0u];
	 */
	bool should_print_uint_literals_as_ints;
	/** number of loops in the generated code */
	int loop_count;

	/** Whether the shader being cross compiled needs GL_EXT_texture_buffer. */
	bool bUsesTextureBuffer;

	/** Whether the shader being cross compiled needs GL_OES_shader_image_atomic. */
	bool bUseImageAtomic;

	// True if the discard instruction was encountered.
	bool bUsesDiscard;

	// Uses gl_InstanceID
	bool bUsesInstanceID;
	
	// Don't allow global uniforms; instead, wrap in a struct to make a proper uniform buffer
	bool bNoGlobalUniforms;

	/**
	 * Return true if the type is a multi-dimensional array. Also, track the
	 * array.
	 */
	bool is_md_array(const glsl_type* type)
	{
		if (type->base_type == GLSL_TYPE_ARRAY &&
			type->fields.array->base_type == GLSL_TYPE_ARRAY)
		{
			foreach_iter(exec_list_iterator, iter, used_md_arrays) 
			{
				md_array_entry* entry = (md_array_entry*)iter.get();
				if (entry->type == type)
					return true;
			}
			md_array_entry* entry = new(mem_ctx) md_array_entry();
			entry->type = type;
			used_md_arrays.push_tail(entry);
			return true;
		}
		return false;
	}

	/**
	 * Fetch/generate a unique name for ir_variable.
	 *
	 * GLSL IR permits multiple ir_variables to share the same name.  This works
	 * fine until we try to print it, when we really need a unique one.
	 */
	const char *unique_name(ir_variable *var)
	{
		if (var->mode == ir_var_temporary || var->mode == ir_var_auto)
		{
			/* Do we already have a name for this variable? */
			const char *name = (const char *) hash_table_find(this->printable_names, var);
			if (name == nullptr)
			{
				bool bIsGlobal = (scope_depth == 0 && var->mode != ir_var_temporary);
				const char* prefix = "g";
				if (!bIsGlobal)
				{
					if (var->type->is_matrix())
					{
						prefix = "m";
					}
					else if (var->type->is_vector())
					{
						prefix = "v";
					}
					else
					{
						switch (var->type->base_type)
						{
						case GLSL_TYPE_BOOL: prefix = "b"; break;
						case GLSL_TYPE_UINT: prefix = "u"; break;
						case GLSL_TYPE_INT: prefix = "i"; break;
						case GLSL_TYPE_HALF: prefix = "h"; break;
						case GLSL_TYPE_FLOAT: prefix = "f"; break;
						default: prefix = "t"; break;
						}
					}
				}
				int var_id = bIsGlobal ? global_id++ : temp_id++;
				name = ralloc_asprintf(mem_ctx, "%s%d", prefix, var_id);
				hash_table_insert(this->printable_names, (void *)name, var);
			}
			return name;
		}

		/* If there's no conflict, just use the original name */
		return var->name;
	}

	/**
	 * Add tabs/spaces for the current indentation level.
	 */
	void indent(void)
	{
		for (int i = 0; i < indentation; i++)
		{
			ralloc_asprintf_append(buffer, "\t");
		}
	}

	/**
	 * Print out the internal name for a multi-dimensional array.
	 */
	void print_md_array_type(const glsl_type *t)
	{
		if (t->base_type == GLSL_TYPE_ARRAY)
		{
			ralloc_asprintf_append(buffer, "_mdarr_");
			do 
			{
				ralloc_asprintf_append(buffer, "%u_", t->length);
				t = t->fields.array;
			} while (t->base_type == GLSL_TYPE_ARRAY);
			print_base_type(t);
		}
	}

	/**
	 * Print the base type, e.g. vec3.
	 */
	void print_base_type(const glsl_type *t)
	{
		if (t->base_type == GLSL_TYPE_ARRAY)
		{
			print_base_type(t->fields.array);
		}
		else if (t->base_type == GLSL_TYPE_INPUTPATCH)
		{
			ralloc_asprintf_append(buffer, "/* %s */ ", t->name);
			print_base_type(t->inner_type);
		}
		else if (t->base_type == GLSL_TYPE_OUTPUTPATCH)
		{
			ralloc_asprintf_append(buffer, "/* %s */ ", t->name);
			print_base_type(t->inner_type);
		}
		else if ((t->base_type == GLSL_TYPE_STRUCT)
				&& (strncmp("gl_", t->name, 3) != 0))
		{
			ralloc_asprintf_append(buffer, "%s", t->name);
		}
		else 
		{
			std::string Name = FixHlslName(t);
			ralloc_asprintf_append(buffer, "%s", Name.c_str());
		}
	}

	/**
	 * Print the portion of the type that appears before a variable declaration.
	 */
	void print_type_pre(const glsl_type *t)
	{
		if (is_md_array(t))
		{
			print_md_array_type(t);
		}
		else
		{
			print_base_type(t);
		}
	}

	/**
	 * Print the portion of the type that appears after a variable declaration.
	 */
	void print_type_post(const glsl_type *t, bool is_unsized = false)
	{
		if (t->base_type == GLSL_TYPE_ARRAY && !is_md_array(t))
		{
			if (is_unsized)
			{
				ralloc_asprintf_append(buffer, "[]");
			}
			else
			{
				ralloc_asprintf_append(buffer, "[%u]", t->length);
			}
		}
		else if (t->base_type == GLSL_TYPE_INPUTPATCH || t->base_type == GLSL_TYPE_OUTPUTPATCH)
		{
			ralloc_asprintf_append(buffer, "[%u] /* %s */", t->patch_length, t->name);
		}
	}

	/**
	 * Print a full variable declaration.
	 */
	void print_type_full(const glsl_type *t)
	{
		print_type_pre(t);
		print_type_post(t);
	}

	/**
	 * Visit a single instruction. Appends a semicolon and EOL if needed.
	 */
	void do_visit(ir_instruction* ir)
	{
		needs_semicolon = true;
		ir->accept(this);
		if (needs_semicolon)
		{
			ralloc_asprintf_append(buffer, ";\n");
		}
	}

	enum EPrecisionModifier
	{
		GLSL_PRECISION_DEFAULT,
		GLSL_PRECISION_LOWP,
		GLSL_PRECISION_MEDIUMP,
		GLSL_PRECISION_HIGHP,
	};

	EPrecisionModifier GetPrecisionModifier(const struct glsl_type *type)
	{
		if (type->is_sampler() || type->is_image())
		{
			if (bDefaultPrecisionIsHalf && type->inner_type->base_type == GLSL_TYPE_FLOAT)
			{
				return GLSL_PRECISION_HIGHP;
			}
			else if (!bDefaultPrecisionIsHalf && type->inner_type->base_type == GLSL_TYPE_HALF)
			{
				return GLSL_PRECISION_MEDIUMP;
			}
			else // shadow samplers, integer textures etc
			{
				return GLSL_PRECISION_HIGHP;
			}
		}
		else if (bDefaultPrecisionIsHalf && (type->base_type == GLSL_TYPE_FLOAT || (type->is_array() && type->element_type()->base_type == GLSL_TYPE_FLOAT)))
		{
			return GLSL_PRECISION_HIGHP;
		}
		else if (!bDefaultPrecisionIsHalf && (type->base_type == GLSL_TYPE_HALF || (type->is_array() && type->element_type()->base_type == GLSL_TYPE_HALF)))
		{
			return GLSL_PRECISION_MEDIUMP;
		}
		else if (type->is_integer())
		{
			// integers use default precision which is always highp
			return GLSL_PRECISION_DEFAULT;
		}
		return GLSL_PRECISION_DEFAULT;
	}

	void AppendPrecisionModifier(char** inBuffer, EPrecisionModifier PrecisionModifier)
	{
		switch (PrecisionModifier)
		{
			case GLSL_PRECISION_LOWP:
				ralloc_asprintf_append(inBuffer, "lowp ");
				break;
			case GLSL_PRECISION_MEDIUMP:
				ralloc_asprintf_append(inBuffer, "mediump ");
				break;
			case GLSL_PRECISION_HIGHP:
				ralloc_asprintf_append(inBuffer, "highp ");
				break;
			case GLSL_PRECISION_DEFAULT:
				break;
			default:
				// we missed a type
				check(false);
		}
	}

	/**
	* \name Visit methods
	*
	* As typical for the visitor pattern, there must be one \c visit method for
	* each concrete subclass of \c ir_instruction.  Virtual base classes within
	* the hierarchy should not have \c visit methods.
	*/

	virtual void visit(ir_rvalue *rvalue)
	{
		check(0 && "ir_rvalue not handled for GLSL export.");
	}

	virtual void visit(ir_variable *var)
	{
		const char * const centroid_str[] = { "", "centroid " };
		const char * const invariant_str[] = { "", "invariant " };
		const char * const patch_constant_str[] = { "", "patch " };
		const char * const GLSLmode_str[] = { "", "uniform ", "in ", "out ", "inout ", "in ", "", "shared ", "", "", "uniform_ref "};
		const char * const ESVSmode_str[] = { "", "uniform ", "attribute ", "varying ", "inout ", "in ", "", "shared " };
		const char * const ESFSmode_str[] = { "", "uniform ", "varying ", "attribute ", "", "in ", "", "shared " };
		const char * const GLSLinterp_str[] = { "", "smooth ", "flat ", "noperspective " };
		const char * const ESinterp_str[] = { "", "", "", "" };
		const char * const ES31interp_str[] = { "", "smooth ", "flat ", "" };
		const char * const layout_str[] = { "", "layout(origin_upper_left) ", "layout(pixel_center_integer) ", "layout(origin_upper_left,pixel_center_integer) " };

		const char * const * mode_str = bIsES ? ((ShaderTarget == vertex_shader) ? ESVSmode_str : ESFSmode_str) : GLSLmode_str;
		const char * const * interp_str = bIsES ? ESinterp_str : (bIsES31 ? ES31interp_str : GLSLinterp_str);

		// Check for an initialized const variable
		// If var is read-only and initialized, set it up as an initialized const
		bool constInit = false;
		if (var->has_initializer && var->read_only && (var->constant_initializer || var->constant_value))
		{
			ralloc_asprintf_append( buffer, "const ");
			constInit = true;
		}

		if (scope_depth == 0)
		{
			glsl_base_type base_type = var->type->base_type;
			if (base_type == GLSL_TYPE_ARRAY)
			{
				base_type = var->type->fields.array->base_type;
			}

			if (var->mode == ir_var_in)
			{
				input_variables.push_tail(new(mem_ctx) extern_var(var));
			}
			else if (var->mode == ir_var_out)
			{
				output_variables.push_tail(new(mem_ctx) extern_var(var));
			}
			else if (var->mode == ir_var_uniform && var->type->is_sampler())
			{
				sampler_variables.push_tail(new(mem_ctx) extern_var(var));
			}
			else if (var->mode == ir_var_uniform && var->type->is_image())
			{
				image_variables.push_tail(new(mem_ctx) extern_var(var));
			}
			else if (var->mode == ir_var_uniform && base_type == GLSL_TYPE_SAMPLER_STATE)
			{
				// ignore sampler state uniforms
			}
			else if (var->mode == ir_var_uniform && var->semantic == NULL)
			{
				uniform_variables.push_tail(new(mem_ctx) extern_var(var));
			}
		}

		const bool bBuiltinVariable = (var->name && strncmp(var->name, "gl_", 3) == 0);
		
		if (bBuiltinVariable && ShaderTarget == vertex_shader && strncmp(var->name, "gl_InstanceID", 13) == 0)
		{
			bUsesInstanceID = true;
		}

		if (bBuiltinVariable &&
			var->centroid == 0 && (var->interpolation == 0 || strncmp(var->name, "gl_Layer", 3) == 0) &&
			var->invariant == 0 && var->origin_upper_left == 0 &&
			var->pixel_center_integer == 0)
		{
			// Don't emit builtin GL variable declarations.
			needs_semicolon = false;
		}
		else if (scope_depth == 0 && var->mode == ir_var_temporary)
		{
			global_instructions.push_tail(new(mem_ctx) global_ir(var));
			needs_semicolon = false;
		}
		else
		{
			int layout_bits =
				(var->origin_upper_left ? 0x1 : 0) |
				(var->pixel_center_integer ? 0x2 : 0);
			
			// this is for NVN which doesn't support global params, so we wrap each of the typed
			// buffer in a struct, which ends up as a proper, non global parameter, uniform buffer
			bool bUseGlobalUniformBufferWrapper = false;
			if (bNoGlobalUniforms && var->mode == ir_var_uniform && var->semantic)
			{
				bUseGlobalUniformBufferWrapper = true;
			}

			if (var->is_patch_constant)
			{
				// AMD drivers reject interface blocks for per-patch data.
				// AMD drivers also need a location qualifier for each shader input/output vector.
				// So we translate patch constant data to individual structs:
				//   "layout(location = 9) patch in struct { vec4 Data; } in_PN_POSITION9;"
				// NVIDIA drivers would also accept the previous solution:
				//   "patch in PN_POSITION9 { vec4 Data; } in_PN_POSITION9;"

				// We expect a struct with single member "Data" at this point
				check(var->type->base_type == GLSL_TYPE_STRUCT);

				// Patch declarations cannot have interpolation qualifiers
				if (var->explicit_location)
				{
					ralloc_asprintf_append(
						buffer,
						"layout(location = %d) patch %sstruct",
						var->location,
						mode_str[var->mode]
					);
				}
				else
				{
					ralloc_asprintf_append(
						buffer,
						"patch %sstruct",
						mode_str[var->mode]
					);
				}

				const glsl_type* inner_type = var->type;
				if (inner_type->is_array())
				{
					inner_type = inner_type->fields.array;
				}
				check(inner_type->is_record());
				check(inner_type->length == 1);
				const glsl_struct_field* field = &inner_type->fields.structure[0];
				check(strcmp(field->name, "Data") == 0);

				ralloc_asprintf_append(buffer, " { ");
				print_type_pre(field->type);
				ralloc_asprintf_append(buffer, " Data");
				print_type_post(field->type);
				ralloc_asprintf_append(buffer, "; }");
			}
			else if (scope_depth == 0 &&
			   ((var->mode == ir_var_in) || (var->mode == ir_var_out)) && 
			   var->is_interface_block)
			{
				/**
				Hack to display our fake structs as what they are supposed to be - interface blocks

				'in' or 'out' variable qualifier becomes interface block declaration start,
				structure name becomes block name,
				we add information about block contents, taking type from sole struct member type, and
				struct variable name becomes block instance name.

				Note: With tessellation, matching interfaces between shaders is tricky, so we need
				to assign explicit locations to shader input and output variables.

				The reason we use a struct instead of an interface block is that with
				GL4.2/GL_ARB_separate_shader_objects, you can add a layout(location=foo) to a variable
				that is not part of an interface block. However, in order to add a location to a variable
				inside an interface block, you need GL4.4/GL_enhanced_layouts. Since for now, we don't want
				that dependency, we use structs.

				*/
				
				if(bGenerateLayoutLocations && var->explicit_location && var->is_patch_constant == 0)
				{
					check(layout_bits == 0);

					// Some devices (S6 G920L 6.0.1) may complain about second empty parameter in an INTERFACE_BLOCK macro
					// Make sure we put something there 
					const char* interp_qualifier = interp_str[var->interpolation];
					if (bIsES31 && strlen(interp_qualifier) == 0)
					{
						interp_qualifier = "smooth ";
					}

					ralloc_asprintf_append(
						buffer,
						"INTERFACE_BLOCK(%d, %s, %s%s%s%s, ",
						var->location,
						interp_qualifier,
						centroid_str[var->centroid],
						invariant_str[var->invariant],
						patch_constant_str[var->is_patch_constant],
						mode_str[var->mode]);

					print_type_pre(var->type);
					ralloc_asprintf_append(buffer, ", ");
					
					const glsl_type* inner_type = var->type;
					if (inner_type->is_array())
					{
						inner_type = inner_type->fields.array;
					}
					check(inner_type->is_record());
					check(inner_type->length==1);
					const glsl_struct_field* field = &inner_type->fields.structure[0];
					check(strcmp(field->name,"Data")==0);
					
					if (bEmitPrecision)
					{
						if (field->type->is_integer())
						{
							ralloc_asprintf_append(buffer, "flat ");
						}
						AppendPrecisionModifier(buffer, GetPrecisionModifier(field->type));
					}
					print_type_pre(field->type);
					ralloc_asprintf_append(buffer, ", Data");
					print_type_post(field->type);
					ralloc_asprintf_append(buffer, ")");
				}
				else
				{
					ralloc_asprintf_append(
						buffer,
						"%s%s%s%s%s",
						layout_str[layout_bits],
						centroid_str[var->centroid],
						invariant_str[var->invariant],
						patch_constant_str[var->is_patch_constant],
						mode_str[var->mode]
						);
					
					print_type_pre(var->type);

					const glsl_type* inner_type = var->type;
					if (inner_type->is_array())
					{
						inner_type = inner_type->fields.array;
					}
					check(inner_type->is_record());
					check(inner_type->length==1);
					const glsl_struct_field* field = &inner_type->fields.structure[0];
					check(strcmp(field->name,"Data")==0);
					
					ralloc_asprintf_append(buffer, " { %s", interp_str[var->interpolation]);
					
					print_type_pre(field->type);
					ralloc_asprintf_append(buffer, " Data");
					print_type_post(field->type);
					ralloc_asprintf_append(buffer, "; }");
				}
			}
			else if (var->type->is_image())
			{
				const int UAV_stage_first_unit[] = 
				{
					0, //vertex_shader, must match FOpenGL::GetFirstVertexUAVUnit()
					0,
					0, //fragment_shader, must match FOpenGL::GetFirstPixelUAVUnit()
					0,
					0,
					0, //compute_shader
				};
						
				if (var->type->HlslName && (!strncmp(var->type->HlslName, "RWStructuredBuffer<", 19) || !strncmp(var->type->HlslName, "StructuredBuffer<", 17)))
				{
					if (bGenerateLayoutLocations && var->explicit_location)
					{
						const char * const readonly_str[] = { "", "readonly " };
						const int readonly = strncmp(var->type->HlslName, "StructuredBuffer<", 17)==0 ? 1 : 0;
						
						ralloc_asprintf_append(
							buffer,
							"layout(std430,binding=%d) %s buffer ",
							var->location + UAV_stage_first_unit[ShaderTarget],
							readonly_str[readonly]
							);
					}
					else
					{
						ralloc_asprintf_append(
							buffer,
							"buffer "
						);
					}
				}
				else
				{
					const bool bSingleComp = (var->type->inner_type->vector_elements == 1);
					const char * const coherent_str[] = { "", "coherent " };
					const char * const writeonly_str[] = { "", "writeonly " };
					const char * const type_str[] = { "32ui", "32i", "16f", "32f" };
					const char * const comp_str = bSingleComp ? "r" : "rgba";
					const int writeonly = var->image_write && !(var->image_read);

					check( var->type->inner_type->base_type >= GLSL_TYPE_UINT &&
							var->type->inner_type->base_type <= GLSL_TYPE_FLOAT );

					ralloc_asprintf_append(
						buffer,
						"%s%s%s%s",
						invariant_str[var->invariant],
						mode_str[var->mode],
						coherent_str[var->coherent],
						writeonly_str[writeonly]
						);

					if (bGenerateLayoutLocations && var->explicit_location)
					{
						//should check here on base type
						ralloc_asprintf_append(
							buffer,
							"layout(%s%s,binding=%d) ",
							comp_str,
							type_str[var->type->inner_type->base_type],
							var->location + UAV_stage_first_unit[ShaderTarget]
							);
					}
					else
					{
						//should check here on base type
						ralloc_asprintf_append(
							buffer,
							"layout(%s%s) ",
							comp_str,
							type_str[var->type->inner_type->base_type]
							);
					}

					if (bEmitPrecision)
					{
						AppendPrecisionModifier(buffer, GetPrecisionModifier(var->type));
					}
					print_type_pre(var->type);
				}
			}
			else
			{

				char* layout = nullptr;

				if (bGenerateLayoutLocations && var->explicit_location)
				{
					check(layout_bits == 0);
					layout = ralloc_asprintf(nullptr, "layout(location=%d) ", var->location);
				}
				
				const bool bIsDepthTarget = var->name && bUsesDepthbufferFetch && strncmp(var->name, "out_Target1", 11) == 0;
				const bool bNeedsFBFOutput = bIsDepthTarget || (var->name && (bUsesFrameBufferFetch && strncmp(var->name, "out_Target0", 11) == 0));
				const char* StorageQualifier = bNeedsFBFOutput ? FBF_StorageQualifier : mode_str[var->mode];

				ralloc_asprintf_append(
					buffer,
					"%s%s%s%s%s%s",
					layout ? layout : layout_str[layout_bits],
					var->mode != ir_var_temporary && var->mode != ir_var_auto ? interp_str[var->interpolation] : "",
					var->mode != ir_var_temporary && var->mode != ir_var_auto ? centroid_str[var->centroid] : "",
					var->mode != ir_var_temporary && var->mode != ir_var_auto ? invariant_str[var->invariant] : "",
					patch_constant_str[var->is_patch_constant],
					StorageQualifier
					);

				if (bIsDepthTarget)
				{
					bHasGeneratedDepthTargetInput = true;
				}

				if (bUseGlobalUniformBufferWrapper)
				{
					ralloc_asprintf_append(
						buffer,
						"Block_%s { ",
						var->semantic
						);
				}

				if (bEmitPrecision)
				{
					AppendPrecisionModifier(buffer, GetPrecisionModifier(var->type));
				}

				if (bGenerateLayoutLocations && var->explicit_location)
				{
					ralloc_free(layout);
				}

				print_type_pre(var->type);
			}

			if (var->type->is_image() && (var->type->HlslName && (!strncmp(var->type->HlslName, "RWStructuredBuffer<", 19) || !strncmp(var->type->HlslName, "StructuredBuffer<", 17))))
			{
				AddTypeToUsedStructs(var->type->inner_type);
				ralloc_asprintf_append(buffer, " %s_VAR { %s %s[]; }", unique_name(var), var->type->inner_type->name, unique_name(var));
			}
			else
			{
				ralloc_asprintf_append(buffer, " %s", unique_name(var));
				const bool bUnsizedArray = var->mode == ir_var_in && ((ShaderTarget == tessellation_evaluation_shader) || (ShaderTarget == tessellation_control_shader));
				print_type_post(var->type, bUnsizedArray );
			}

			if (bUseGlobalUniformBufferWrapper)
			{
				ralloc_asprintf_append(
					buffer,
					"; }"
					);
			}

		}

		// Add the initializer if we need it
		if (constInit)
		{
			ralloc_asprintf_append(buffer, " = ");
			if (var->constant_initializer)
			{
				var->constant_initializer->accept(this);
			}
			else
			{
				var->constant_value->accept(this);
			}
		}

		// add type to used_structures so we can later declare them at the start of the GLSL shader
		// this is for the case of a variable that is declared, but not later dereferenced (which can happen
		// when debugging HLSLCC and running without optimization
		AddTypeToUsedStructs(var->type);

	}

	virtual void visit(ir_function_signature *sig)
	{
		if (sig->is_main && ShaderTarget == fragment_shader)
		{
			// print this right before 'main' where all outputs are already declared
			if (bUsesFrameBufferFetch)
			{
				ralloc_asprintf_append(buffer, "\n#ifdef GL_EXT_shader_framebuffer_fetch\n");
				ralloc_asprintf_append(buffer, "  vec4 FramebufferFetchES2() { return out_Target0; }\n");
				ralloc_asprintf_append(buffer, "#elif defined(GL_ARM_shader_framebuffer_fetch)\n");
				ralloc_asprintf_append(buffer, "  vec4 FramebufferFetchES2() { return gl_LastFragColorARM; }\n");
				ralloc_asprintf_append(buffer, "#else\n");
				ralloc_asprintf_append(buffer, "  vec4 FramebufferFetchES2() { return vec4(0.0, 0.0, 0.0, 0.0); }\n");
				ralloc_asprintf_append(buffer, "#endif\n\n");
			}
			
			if (bUsesDepthbufferFetch)
			{
				ralloc_asprintf_append(buffer, "\n#ifdef GL_ARM_shader_framebuffer_fetch_depth_stencil\n");
				ralloc_asprintf_append(buffer, "  float DepthbufferFetchES2() { return gl_LastFragDepthARM; }\n");
				ralloc_asprintf_append(buffer, "#elif defined(GL_EXT_shader_framebuffer_fetch)\n");
				if (!bHasGeneratedDepthTargetInput)
				{
					ralloc_asprintf_append(buffer, "  layout(location=1) inout float out_Target1;\n");
				}
				ralloc_asprintf_append(buffer, "  float DepthbufferFetchES2() { return out_Target1; }\n");
				ralloc_asprintf_append(buffer, "#else\n");
				ralloc_asprintf_append(buffer, "  float DepthbufferFetchES2() { return 0.0; }\n");
				ralloc_asprintf_append(buffer, "#endif\n\n");
			}
		}
				
		// Reset temporary id count.
		temp_id = 0;
		bool bPrintComma = false;
		scope_depth++;

		print_type_full(sig->return_type);
		ralloc_asprintf_append(buffer, " %s(", sig->function_name());

		foreach_iter(exec_list_iterator, iter, sig->parameters)
		{
			ir_variable *const inst = (ir_variable *) iter.get();
			if (bPrintComma)
			{
				ralloc_asprintf_append(buffer, ",");
			}
			inst->accept(this);
			bPrintComma = true;
		}
		ralloc_asprintf_append(buffer, ")\n");

		indent();
		ralloc_asprintf_append(buffer, "{\n");

		if (sig->is_main && !global_instructions.is_empty())
		{
			indentation++;
			foreach_iter(exec_list_iterator, iter, global_instructions)
			{
				global_ir* gir = (global_ir*)iter.get();
				indent();
				do_visit(gir->ir);
			}
			indentation--;
		}

		//grab the global attributes
		if (sig->is_main)
		{
			early_depth_stencil = sig->is_early_depth_stencil;
			wg_size_x = sig->wg_size_x;
			wg_size_y = sig->wg_size_y;
			wg_size_z = sig->wg_size_z;

			tessellation = sig->tessellation;
		}

		indentation++;
		foreach_iter(exec_list_iterator, iter, sig->body)
		{
			ir_instruction *const inst = (ir_instruction *) iter.get();
			indent();
			do_visit(inst);
		}
		indentation--;
		indent();
		ralloc_asprintf_append(buffer, "}\n");
		needs_semicolon = false;
		scope_depth--;
	}

	virtual void visit(ir_function *func)
	{
		foreach_iter(exec_list_iterator, iter, *func)
		{
			ir_function_signature *const sig = (ir_function_signature *) iter.get();
			if (sig->is_defined && !sig->is_builtin)
			{
				indent();
				sig->accept(this);
			}
		}
		needs_semicolon = false;
	}

	virtual void visit(ir_expression *expr)
	{
		check(scope_depth > 0);

		int numOps = expr->get_num_operands();
		ir_expression_operation op = expr->operation;

		if (numOps == 1 && op >= ir_unop_first_conversion && op <= ir_unop_last_conversion)
		{
			if (op == ir_unop_f2h || op == ir_unop_h2f)
			{
				// No need to convert from half<->float as that is part of the precision of a variable
				expr->operands[0]->accept(this);
			}
			else
			{
				ralloc_asprintf_append(buffer, "%s(", FixHlslName(expr->type).c_str());
				expr->operands[0]->accept(this);
				ralloc_asprintf_append(buffer, ")");
			}
		}
		else if (expr->type->is_scalar() &&
			((numOps == 1 && op == ir_unop_logic_not) ||
			 (numOps == 2 && op >= ir_binop_first_comparison && op <= ir_binop_last_comparison) ||
			 (numOps == 2 && op >= ir_binop_first_logic && op <= ir_binop_last_logic)))
		{
			const char* op_str = GLSLExpressionTable[op][3];
			ralloc_asprintf_append(buffer, "%s(", (numOps == 1) ? op_str : "");
			expr->operands[0]->accept(this);
			if (numOps == 2)
			{
				ralloc_asprintf_append(buffer, "%s", op_str);
				expr->operands[1]->accept(this);
			}
			ralloc_asprintf_append(buffer, ")");
		}
		else if (expr->type->is_vector() && numOps == 2 &&
			op >= ir_binop_first_logic && op <= ir_binop_last_logic)
		{
			ralloc_asprintf_append(buffer, GLSLExpressionTable[op][0], expr->type->vector_elements, expr->type->vector_elements);
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, GLSLExpressionTable[op][1], expr->type->vector_elements);
			expr->operands[1]->accept(this);
			ralloc_asprintf_append(buffer, GLSLExpressionTable[op][2]);
		}
		else if (op == ir_binop_mod && !expr->type->is_float())
		{
			ralloc_asprintf_append(buffer, "((");
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ")%%(");
			expr->operands[1]->accept(this);
			ralloc_asprintf_append(buffer, "))");
		}
		else if (op == ir_binop_mul && expr->type->is_matrix()
			&& expr->operands[0]->type->is_matrix()
			&& expr->operands[1]->type->is_matrix())
		{
			ralloc_asprintf_append(buffer, "matrixCompMult(");
			expr->operands[0]->accept(this);
			ralloc_asprintf_append(buffer, ",");
			expr->operands[1]->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else if (numOps < 4)
		{
			ralloc_asprintf_append(buffer, GLSLExpressionTable[op][0]);
			for (int i = 0; i < numOps; ++i)
			{
				expr->operands[i]->accept(this);
				ralloc_asprintf_append(buffer, GLSLExpressionTable[op][i+1]);
			}
		}
	}

	virtual void visit(ir_texture *tex)
	{
		check(scope_depth > 0);

		const char * const fetch_str[] = { "texture", "texelFetch" };
		const char * const Dim[] = { "", "2D", "3D", "Cube", "", "", "" };
		static const char * const size_str[] = { "", "Size" };
		static const char * const proj_str[] = { "", "Proj" };
		static const char * const grad_str[] = { "", "Grad" };
		static const char * const lod_str[] = { "", "Lod" };
		static const char * const offset_str[] = { "", "Offset" };
		static const char * const gather_str[] = { "", "Gather" };
		static const char * const querymips_str[] = { "", "QueryLevels" };
		static const char * const EXT_str[] = { "", "EXT" };
		const bool cube_array = tex->sampler->type->sampler_dimensionality == GLSL_SAMPLER_DIM_CUBE && 
			tex->sampler->type->sampler_array;

		ir_texture_opcode op = tex->op;
		if (op == ir_txl && tex->sampler->type->sampler_shadow && tex->sampler->type->sampler_dimensionality == GLSL_SAMPLER_DIM_CUBE)
		{
			// This very instruction is missing in OpenGL 3.2, so we need to change the sampling to instruction that exists in order for shader to compile
			op = ir_tex;
		}

		if (op == ir_txf)
		{
			bUsesTextureBuffer = true;
		}

		// Emit texture function and sampler.
		ralloc_asprintf_append(buffer, "%s%s%s%s%s%s%s%s%s%s(",
			fetch_str[op == ir_txf],
			bIsES ? Dim[tex->sampler->type->sampler_dimensionality] : "",
			gather_str[op == ir_txg],
			size_str[op == ir_txs],
			querymips_str[op == ir_txm],
			proj_str[tex->projector != 0],
			grad_str[op == ir_txd],
			lod_str[op == ir_txl],
			offset_str[tex->offset != 0],
			EXT_str[0]
		);
		tex->sampler->accept(this);

		// Emit coordinates.
		if ( (op == ir_txs && tex->lod_info.lod) || op == ir_txm)
		{
			if (!tex->sampler->type->sampler_ms && op != ir_txm)
			{
				ralloc_asprintf_append(buffer, ",");
				tex->lod_info.lod->accept(this);
			}
		}
		else if (tex->sampler->type->sampler_shadow && (op != ir_txg && !cube_array))
		{
			int coord_dims = 0;
			switch (tex->sampler->type->sampler_dimensionality)
			{
				case GLSL_SAMPLER_DIM_1D: coord_dims = 2; break;
				case GLSL_SAMPLER_DIM_2D: coord_dims = 3; break;
				case GLSL_SAMPLER_DIM_3D: coord_dims = 4; break;
				case GLSL_SAMPLER_DIM_CUBE: coord_dims = 4; break;
				default: check(0 && "Shadow sampler has unsupported dimensionality.");
			}
			ralloc_asprintf_append(buffer, ",vec%d(", coord_dims);
			tex->coordinate->accept(this);
			ralloc_asprintf_append(buffer, ",");
			tex->shadow_comparitor->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else
		{
			ralloc_asprintf_append(buffer, ",");
			tex->coordinate->accept(this);
		}

		// Emit gather compare value
		if (tex->sampler->type->sampler_shadow && (op == ir_txg || cube_array))
		{
			ralloc_asprintf_append(buffer, ",");
			tex->shadow_comparitor->accept(this);
		}

		// Emit sample index.
		if (op == ir_txf && tex->sampler->type->sampler_ms)
		{
			ralloc_asprintf_append(buffer, ",");
			tex->lod_info.sample_index->accept(this);
		}

		// Emit LOD.
		if (op == ir_txl ||
		   (op == ir_txf && tex->lod_info.lod &&
		   !tex->sampler->type->sampler_ms && !tex->sampler->type->sampler_buffer))
		{
			ralloc_asprintf_append(buffer, ",");
			tex->lod_info.lod->accept(this);
		}

		// Emit gradients.
		if (op == ir_txd)
		{
			ralloc_asprintf_append(buffer, ",");
			tex->lod_info.grad.dPdx->accept(this);
			ralloc_asprintf_append(buffer, ",");
			tex->lod_info.grad.dPdy->accept(this);
		}
		else if (op == ir_txb)
		{
			ralloc_asprintf_append(buffer, ",");
			tex->lod_info.bias->accept(this);
		}

		// Emit offset.
		if (tex->offset)
		{
			ralloc_asprintf_append(buffer, ",");
			tex->offset->accept(this);
		}

		// Emit channel selection for gather
		if (op == ir_txg && tex->channel > ir_channel_none)
		{
			check( tex->channel < ir_channel_unknown);
			ralloc_asprintf_append(buffer, ", %d", int(tex->channel) - 1);
		}

		ralloc_asprintf_append(buffer, ")");
	}

	virtual void visit(ir_swizzle *swizzle)
	{
		check(scope_depth > 0);

		const unsigned mask[4] =
		{
			swizzle->mask.x,
			swizzle->mask.y,
			swizzle->mask.z,
			swizzle->mask.w,
		};

		if (swizzle->val->type->is_scalar())
		{
			// Scalar -> Vector swizzles must use the constructor syntax.
			if (swizzle->type->is_scalar() == false)
			{
				print_type_full(swizzle->type);
				ralloc_asprintf_append(buffer, "(");
				swizzle->val->accept(this);
				ralloc_asprintf_append(buffer, ")");
			}
		}
		else
		{
			const bool is_constant = swizzle->val->as_constant() != nullptr;
			if (is_constant)
			{
				ralloc_asprintf_append(buffer, "(");
			}
			swizzle->val->accept(this);
			if (is_constant)
			{
				ralloc_asprintf_append(buffer, ")");
			}
			ralloc_asprintf_append(buffer, ".");
			for (unsigned i = 0; i < swizzle->mask.num_components; i++)
			{
				ralloc_asprintf_append(buffer, "%c", "xyzw"[mask[i]]);
			}
		}
	}

	virtual void visit(ir_dereference_variable *deref)
	{
		check(scope_depth > 0);

		ir_variable* var = deref->variable_referenced();

		// enable texture buffer extension for "uimagebuffer" and "imagebuffer" which is a sampler_buffer but not a shader_storage_buffer
		if (var->type->sampler_buffer && !var->type->shader_storage_buffer)
		{
			bUsesTextureBuffer = true;
		}

		ralloc_asprintf_append(buffer, unique_name(var));


		// add type to used_structures so we can later declare them at the start of the GLSL shader
		AddTypeToUsedStructs(var->type);


		if (var->mode == ir_var_uniform && var->semantic != NULL)
		{
			if (hash_table_find(used_uniform_blocks, var->semantic) == NULL)
			{
				hash_table_insert(used_uniform_blocks, (void*)var->semantic, var->semantic);
			}
		}

		if (is_md_array(deref->type))
		{
			ralloc_asprintf_append(buffer, ".Inner");
		}
	}

	virtual void visit(ir_dereference_array *deref)
	{
		check(scope_depth > 0);

		deref->array->accept(this);

		// Make extra sure crappy Mac OS X compiler won't have any reason to crash
		bool enforceInt = false;

		if (deref->array_index->type->base_type == GLSL_TYPE_UINT)
		{
			if (deref->array_index->ir_type == ir_type_constant)
			{
				should_print_uint_literals_as_ints = true;
			}
			else
			{
				enforceInt = true;
			}
		}

		if (enforceInt)
		{
			ralloc_asprintf_append(buffer, "[int(");
		}
		else
		{
			ralloc_asprintf_append(buffer, "[");
		}

		deref->array_index->accept(this);
		should_print_uint_literals_as_ints = false;

		if (enforceInt)
		{
			ralloc_asprintf_append(buffer, ")]");
		}
		else
		{
			ralloc_asprintf_append(buffer, "]");
		}

		if (is_md_array(deref->array->type))
		{
			ralloc_asprintf_append(buffer, ".Inner");
		}
	}

	void print_image_op( ir_dereference_image *deref, ir_rvalue *src, const char* Mask = nullptr)
	{
		const char* swizzle[] =
		{
			"x", "xy", "xyz", "xyzw"
		};
		const char* expand[] =
		{
			"xxxx", "xyxx", "xyzx", "xyzw"
		};
		const char* int_cast[] =
		{
			"int", "ivec2", "ivec3", "ivec4"
		};
		const int dst_elements = deref->type->vector_elements;
		const int src_elements = (src) ? src->type->vector_elements : 1;
		
		bool bIsStructured = deref->type->is_record() || (deref->image->type->HlslName && (!strncmp(deref->image->type->HlslName, "RWStructuredBuffer<", 19) || !strncmp(deref->image->type->HlslName, "StructuredBuffer<", 17)));

		//!strncmp(var->type->name, "RWStructuredBuffer<")
		check(bIsStructured || (1 <= dst_elements && dst_elements <= 4));
		check(bIsStructured || (1 <= src_elements && src_elements <= 4));

		if ( deref->op == ir_image_access)
		{
			if (bIsStructured)
			{
				deref->image->accept(this);
				ralloc_asprintf_append(buffer, "[");
				deref->image_index->accept(this);
				ralloc_asprintf_append(buffer, "]");
				if (Mask)
				{
					ralloc_asprintf_append(buffer, Mask);
				}
				if (src)
				{
					ralloc_asprintf_append(buffer, " = ");
					src->accept(this);
				}
			}
			else
			{
				bUsesTextureBuffer = true;
				if (src == NULL)
				{
					ralloc_asprintf_append(buffer, "imageLoad( ");
					deref->image->accept(this);
					ralloc_asprintf_append(buffer, ", %s(", GLSLIntCastTypes[deref->image_index->type->vector_elements]);
					deref->image_index->accept(this);
					ralloc_asprintf_append(buffer, ")).%s", swizzle[dst_elements-1]);
				}
				else
				{
					ralloc_asprintf_append(buffer, "imageStore( ");
					deref->image->accept(this);
					ralloc_asprintf_append(buffer, ", %s(", GLSLIntCastTypes[deref->image_index->type->vector_elements]);
					deref->image_index->accept(this);
					ralloc_asprintf_append(buffer, "), ");
					// avoid 'scalar swizzle'
					if (/*src->as_constant() && */src_elements == 1)
					{
						// Add cast if missing and avoid swizzle
						if (deref->image->type->inner_type)
						{
							switch (deref->image->type->inner_type->base_type)
							{
								case GLSL_TYPE_INT:
									ralloc_asprintf_append(buffer, "ivec4(");
									break;
								case GLSL_TYPE_UINT:
									ralloc_asprintf_append(buffer, "uvec4(");
									break;
								case GLSL_TYPE_FLOAT:
									ralloc_asprintf_append(buffer, "vec4(");
									break;
								default:
									break;
							}
						}

						src->accept(this);
						ralloc_asprintf_append(buffer, "))");
					}
					else
					{
						src->accept(this);
						ralloc_asprintf_append(buffer, ".%s)", expand[src_elements-1]);
					}
				}
			}
		}
		else if ( deref->op == ir_image_dimensions) //-V547
		{
			check(!bIsStructured);
			ralloc_asprintf_append( buffer, "imageSize( " );
			deref->image->accept(this);
			ralloc_asprintf_append(buffer, ")");
		}
		else
		{
			check(!bIsStructured);
			check( !"Unknown image operation");
		}
	}

	virtual void visit(ir_dereference_image *deref)
	{
		check(scope_depth > 0);

		print_image_op( deref, NULL);
	}

	virtual void visit(ir_dereference_record *deref)
	{
		check(scope_depth > 0);

		deref->record->accept(this);
		ralloc_asprintf_append(buffer, ".%s", deref->field);

		if (is_md_array(deref->type))
		{
			ralloc_asprintf_append(buffer, ".Inner");
		}
	}

	virtual void visit(ir_assignment *assign)
	{
		if (scope_depth == 0)
		{
			global_instructions.push_tail(new(mem_ctx) global_ir(assign));
			needs_semicolon = false;
			return;
		}

		// constant variables with initializers are statically assigned
		ir_variable *var = assign->lhs->variable_referenced();
		if (var->has_initializer && var->read_only && (var->constant_initializer || var->constant_value))
		{
			//This will leave a blank line with a semi-colon
			return;
		}

		if (assign->condition)
		{
			ralloc_asprintf_append(buffer, "if(");
			assign->condition->accept(this);
			ralloc_asprintf_append(buffer, ") { ");
		}

		char mask[6] = "";
		unsigned j = 1;
		if (assign->lhs->type->is_scalar() == false ||
			assign->write_mask != 0x1)
		{
			for (unsigned i = 0; i < 4; i++)
			{
				if ((assign->write_mask & (1 << i)) != 0)
				{
					mask[j] = "xyzw"[i];
					j++;
				}
			}
		}
		mask[j] = '\0';

		mask[0] = (j == 1) ? '\0' : '.';

		if (assign->lhs->as_dereference_image() != NULL)
		{
			print_image_op( assign->lhs->as_dereference_image(), assign->rhs, mask);
		}
		else
		{
			// decide if we need to cast to float
			const bool need_float_conv = (assign->lhs->type->is_float()
				&& ((assign->rhs->as_constant() != nullptr)
					&& assign->rhs->type->is_scalar()
					&& !assign->rhs->type->is_float()));

			assign->lhs->accept(this);
			ralloc_asprintf_append(buffer, (need_float_conv ? "%s = float(" : "%s = "), mask);
			assign->rhs->accept(this);

			if (need_float_conv)
			{
				ralloc_asprintf_append(buffer, ")");
			}
		}

		if (assign->condition)
		{
			ralloc_asprintf_append(buffer, "%s }", needs_semicolon ? ";" : "");
		}
	}

	void print_constant(ir_constant *constant, int index)
	{
		if (constant->type->is_float())
		{
			if (constant->is_component_finite(index))
			{
				float value = constant->value.f[index];
				// Original formatting code relied on %f style formatting
				// %e is more accureate, and has been available since at least ES 2.0
				// leaving original code in place, in case some drivers don't properly handle it
#if 0
				const char *format = (fabsf(fmodf(value,1.0f)) < 1.e-8f) ? "%.1f" : "%.8f";
#else
				const char *format = "%e";
#endif
				ralloc_asprintf_append(buffer, format, value);
			}
			else
			{
				switch (constant->value.u[index])
				{
					case 0x7f800000u:
						ralloc_asprintf_append(buffer, "(1.0/0.0)");
						break;

					case 0xffc00000u:
						ralloc_asprintf_append(buffer, "(0.0/0.0)");
						break;

					case 0xff800000u:
						ralloc_asprintf_append(buffer, "(-1.0/0.0)");
						break;

					case 0x7fc00000u:
						ralloc_asprintf_append(buffer, "(0.0/0.0) /*Real Nan*/");
						break;

					default:
						ralloc_asprintf_append(buffer, "Unhandled_Nan0x%08x", constant->value.u[index]);
						break;
				}
			}
		}
		else if (constant->type->base_type == GLSL_TYPE_INT
			// print literal uints as ints.
			|| (bIsES && !bIsES31 && !bIsWebGL && constant->type->base_type == GLSL_TYPE_UINT)
			)
		{
			ralloc_asprintf_append(buffer, "%d", constant->value.i[index]);
		}
		else if (constant->type->base_type == GLSL_TYPE_UINT)
		{
			ralloc_asprintf_append(buffer, "%u%s",
				constant->value.u[index],
				should_print_uint_literals_as_ints ? "" : "u"
				);
		}
		else if (constant->type->base_type == GLSL_TYPE_BOOL)
		{
			ralloc_asprintf_append(buffer, "%s", constant->value.b[index] ? "true" : "false");
		}
	}

	virtual void visit(ir_constant *constant)
	{
		if (constant->type == glsl_type::float_type
			|| constant->type == glsl_type::half_type
			|| constant->type == glsl_type::bool_type
			|| constant->type == glsl_type::int_type
			|| constant->type == glsl_type::uint_type)
		{
			print_constant(constant, 0);
		}
		else if (constant->type->is_record())
		{
			print_type_full(constant->type);
			ralloc_asprintf_append(buffer, "(");
			ir_constant* value = (ir_constant*)constant->components.get_head();
			if (value)
			{
				value->accept(this);
			}
			for (uint32 i = 1; i < constant->type->length; i++)
			{
				check(value);
				value = (ir_constant*)value->next;
				if (value)
				{
					ralloc_asprintf_append(buffer, ",");
					value->accept(this);
				}
			}
			ralloc_asprintf_append(buffer, ")");
		}
		else if (constant->type->is_array())
		{
			print_type_full(constant->type);
			ralloc_asprintf_append(buffer, "(");
			constant->get_array_element(0)->accept(this);
			for (uint32 i = 1; i < constant->type->length; ++i)
			{
				ralloc_asprintf_append(buffer, ",");
				constant->get_array_element(i)->accept(this);
			}
			ralloc_asprintf_append(buffer, ")");
		}
		else
		{
			print_type_full(constant->type);
			ralloc_asprintf_append(buffer, "(");
			print_constant(constant, 0);
			int num_components = constant->type->components();
			for (int i = 1; i < num_components; ++i)
			{
				ralloc_asprintf_append(buffer, ",");
				print_constant(constant, i);
			}
			ralloc_asprintf_append(buffer, ")");
		}
	}

	virtual void visit(ir_call *call)
	{
		if (scope_depth == 0)
		{
			global_instructions.push_tail(new(mem_ctx) global_ir(call));
			needs_semicolon = false;
			return;
		}

		if (call->return_deref)
		{
			call->return_deref->accept(this);
			ralloc_asprintf_append(buffer, " = ");
		}
		ralloc_asprintf_append(buffer, "%s(", call->callee_name());
		bool bPrintComma = false;
		foreach_iter(exec_list_iterator, iter, *call)
		{
			ir_instruction *const inst = (ir_instruction *) iter.get();
			if (bPrintComma)
			{
				ralloc_asprintf_append(buffer, ",");
			}
			inst->accept(this);
			bPrintComma = true;
		}
		ralloc_asprintf_append(buffer, ")");
	}

	virtual void visit(ir_return *ret)
	{
		check(scope_depth > 0);

		ralloc_asprintf_append(buffer, "return ");
		ir_rvalue *const value = ret->get_value();
		if (value)
		{
			value->accept(this);
		}
	}

	virtual void visit(ir_discard *discard)
	{
		check(scope_depth > 0);

		if (discard->condition)
		{
			ralloc_asprintf_append(buffer, "if (");
			discard->condition->accept(this);
			ralloc_asprintf_append(buffer, ") ");
		}
		ralloc_asprintf_append(buffer, "discard");
		bUsesDiscard = true;
	}

	bool try_conditional_move(ir_if *expr)
	{
		ir_dereference_variable *dest_deref = NULL;
		ir_rvalue *true_value = NULL;
		ir_rvalue *false_value = NULL;
		unsigned write_mask = 0;
		const glsl_type *assign_type = NULL;
		int num_inst;

		num_inst = 0;
		foreach_iter(exec_list_iterator, iter, expr->then_instructions)
		{
			if (num_inst > 0)
			{
				// multiple instructions? not a conditional move
				return false;
			}

			ir_instruction *const inst = (ir_instruction *) iter.get();
			ir_assignment *assignment = inst->as_assignment();
			if (assignment && (assignment->rhs->ir_type == ir_type_dereference_variable || assignment->rhs->ir_type == ir_type_constant))
			{
				dest_deref = assignment->lhs->as_dereference_variable();
				true_value = assignment->rhs;
				write_mask = assignment->write_mask;
			}
			num_inst++;
		}

		if (dest_deref == NULL || true_value == NULL)
			return false;

		num_inst = 0;
		foreach_iter(exec_list_iterator, iter, expr->else_instructions)
		{
			if (num_inst > 0)
			{
				// multiple instructions? not a conditional move
				return false;
			}

			ir_instruction *const inst = (ir_instruction *) iter.get();
			ir_assignment *assignment = inst->as_assignment();
			if (assignment && (assignment->rhs->ir_type == ir_type_dereference_variable || assignment->rhs->ir_type == ir_type_constant))
			{
				ir_dereference_variable *tmp_deref = assignment->lhs->as_dereference_variable();
				if (tmp_deref
					&& tmp_deref->var == dest_deref->var
					&& tmp_deref->type == dest_deref->type
					&& assignment->write_mask == write_mask)
				{
					false_value= assignment->rhs;
				}
			}
			num_inst++;
		}

		if (false_value == NULL)
			return false;

		char mask[6];
		unsigned j = 1;
		if (dest_deref->type->is_scalar() == false || write_mask != 0x1)
		{
			for (unsigned i = 0; i < 4; i++)
			{
				if ((write_mask & (1 << i)) != 0)
				{
					mask[j] = "xyzw"[i];
					j++;
				}
			}
		}
		mask[j] = '\0';
		mask[0] = (j == 1) ? '\0' : '.';

		dest_deref->accept(this);
		ralloc_asprintf_append(buffer, "%s = (", mask);
		expr->condition->accept(this);
		ralloc_asprintf_append(buffer, ")?(");
		true_value->accept(this);
		ralloc_asprintf_append(buffer, "):(");
		false_value->accept(this);
		ralloc_asprintf_append(buffer, ")");

		return true;
	}

	virtual void visit(ir_if *expr)
	{
		check(scope_depth > 0);

		if (try_conditional_move(expr) == false)
		{
			ralloc_asprintf_append(buffer, "if (");
			expr->condition->accept(this);
			ralloc_asprintf_append(buffer, ")\n");
			indent();
			ralloc_asprintf_append(buffer, "{\n");

			indentation++;
			foreach_iter(exec_list_iterator, iter, expr->then_instructions)
			{
			ir_instruction *const inst = (ir_instruction *) iter.get();
				indent();
				do_visit(inst);
			}
			indentation--;

			indent();
			ralloc_asprintf_append(buffer, "}\n");

			if (!expr->else_instructions.is_empty())
			{
				indent();
				ralloc_asprintf_append(buffer, "else\n");
				indent();
				ralloc_asprintf_append(buffer, "{\n");

				indentation++;
				foreach_iter(exec_list_iterator, iter, expr->else_instructions)
				{
				ir_instruction *const inst = (ir_instruction *) iter.get();
					indent();
					do_visit(inst);
				}
				indentation--;

				indent();
				ralloc_asprintf_append(buffer, "}\n");
			}

			needs_semicolon = false;
		}
	}

	virtual void visit(ir_loop *loop)
	{
		check(scope_depth > 0);

		if (loop->counter && loop->to)
		{
			// IR cmp operator is when to terminate loop; whereas GLSL for loop syntax
			// is while to continue the loop. Invert the meaning of operator when outputting.
			const char* termOp = NULL;
			switch (loop->cmp)
			{
				case ir_binop_less: termOp = ">="; break;
				case ir_binop_greater: termOp = "<="; break;
				case ir_binop_lequal: termOp = ">"; break;
				case ir_binop_gequal: termOp = "<"; break;
				case ir_binop_equal: termOp = "!="; break;
				case ir_binop_nequal: termOp = "=="; break;
				default: check(false);
			}
			ralloc_asprintf_append(buffer, "for (;%s%s", unique_name(loop->counter), termOp);
			loop->to->accept (this);
			ralloc_asprintf_append(buffer, ";)\n");
		}
		else
		{
#if 1
			ralloc_asprintf_append(buffer, "for (;;)\n");
#else
			ralloc_asprintf_append(buffer, "for ( int loop%d = 0; loop%d < 256; loop%d ++)\n", loop_count, loop_count, loop_count);
			loop_count++;
#endif
		}
		indent();
		ralloc_asprintf_append(buffer, "{\n");

		indentation++;
		foreach_iter(exec_list_iterator, iter, loop->body_instructions)
		{
			ir_instruction *const inst = (ir_instruction *) iter.get();
			indent();
			do_visit(inst);
		}
		indentation--;

		indent();
		ralloc_asprintf_append(buffer, "}\n");

		needs_semicolon = false;
	}

	virtual void visit(ir_loop_jump *jmp)
	{
		check(scope_depth > 0);

		ralloc_asprintf_append(buffer, "%s",
			jmp->is_break() ? "break" : "continue");
	}

	virtual void visit(ir_atomic *ir)
	{
		const char *sharedAtomicFunctions[] = 
		{
			"atomicAdd",
			"atomicAnd",
			"atomicMin",
			"atomicMax",
			"atomicOr",
			"atomicXor",
			"atomicExchange",
			"atomicCompSwap"
		};
		const char *imageAtomicFunctions[] = 
		{
			"imageAtomicAdd",
			"imageAtomicAnd",
			"imageAtomicMin",
			"imageAtomicMax",
			"imageAtomicOr",
			"imageAtomicXor",
			"imageAtomicExchange",
			"imageAtomicCompSwap"
		};
		check(scope_depth > 0);
		ir_dereference_image* image = ir->memory_ref->as_dereference_image();

		bUseImageAtomic = image != NULL;

		ir->lhs->accept(this);

		if (!image || (image->image->type && image->image->type->shader_storage_buffer))
		{
			ralloc_asprintf_append(buffer, " = %s(",
				sharedAtomicFunctions[ir->operation]);
			ir->memory_ref->accept(this);
			ralloc_asprintf_append(buffer, ", ");
			ir->operands[0]->accept(this);
			if (ir->operands[1])
			{
				ralloc_asprintf_append(buffer, ", ");
				ir->operands[1]->accept(this);
			}
			ralloc_asprintf_append(buffer, ")");
		}
		else
		{
			ralloc_asprintf_append(buffer, " = %s(",
				imageAtomicFunctions[ir->operation]);
			image->image->accept(this);
			ralloc_asprintf_append(buffer, ", %s(", GLSLIntCastTypes[image->image_index->type->vector_elements]);
			image->image_index->accept(this);
			ralloc_asprintf_append(buffer, "), ");
			ir->operands[0]->accept(this);
			if (ir->operands[1])
			{
				ralloc_asprintf_append(buffer, ", ");
				ir->operands[1]->accept(this);
			}
			ralloc_asprintf_append(buffer, ")");
		}
	}

	void AddTypeToUsedStructs(const glsl_type* type);

	/**
	 * Declare structs used to simulate multi-dimensional arrays.
	 */
	void declare_md_array_struct(const glsl_type* type, hash_table* ht)
	{
		check(type->is_array());

		if (hash_table_find(ht, (void*)type) == NULL)
		{
			const glsl_type* subtype = type->fields.array;
			if (subtype->base_type == GLSL_TYPE_ARRAY)
			{
				declare_md_array_struct(subtype, ht);

				ralloc_asprintf_append(buffer, "struct ");
				print_md_array_type(type);
				ralloc_asprintf_append(buffer, "\n{\n\t");
				print_md_array_type(subtype);
				ralloc_asprintf_append(buffer, " Inner[%u];\n};\n\n", type->length);
			}
			else
			{
				ralloc_asprintf_append(buffer, "struct ");
				print_md_array_type(type);
				ralloc_asprintf_append(buffer, "\n{\n\t");
				print_type_pre(type);
				ralloc_asprintf_append(buffer, " Inner");
				print_type_post(type);
				ralloc_asprintf_append(buffer, ";\n};\n\n");
			}
			hash_table_insert(ht, (void*)type, (void*)type);
		}
	}

	/**
	 * Declare structs used by the code that has been generated.
	 */
	void declare_structs(_mesa_glsl_parse_state* state)
	{
		// If any variable in a uniform block is in use, the entire uniform block
		// must be present including structs that are not actually accessed.
		for (unsigned i = 0; i < state->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = state->uniform_blocks[i];
			if (hash_table_find(used_uniform_blocks, block->name))
			{
				for (unsigned var_index = 0; var_index < block->num_vars; ++var_index)
				{
					const glsl_type* type = block->vars[var_index]->type;
					if (type->base_type == GLSL_TYPE_STRUCT &&
						hash_table_find(used_structures, type) == NULL)
					{
						hash_table_insert(used_structures, (void*)type, type);
					}
				}
			}
		}

		// If otherwise unused structure is a member of another, used structure, the unused structure is also, in fact, used
		{
			int added_structure_types;
			do
			{
				added_structure_types = 0;
				for (unsigned i = 0; i < state->num_user_structures; i++)
				{
					const glsl_type *const s = state->user_structures[i];

					if (hash_table_find(used_structures, s) == NULL)
					{
						continue;
					}

					for (unsigned j = 0; j < s->length; j++)
					{
						const glsl_type* type = s->fields.structure[j].type;

						if (type->base_type == GLSL_TYPE_STRUCT)
						{
							if (hash_table_find(used_structures, type) == NULL)
							{
								hash_table_insert(used_structures, (void*)type, type);
								++added_structure_types;
							}
						}
						else if (type->base_type == GLSL_TYPE_ARRAY && type->fields.array->base_type == GLSL_TYPE_STRUCT)
						{
							if (hash_table_find(used_structures, type->fields.array) == NULL)
							{
								hash_table_insert(used_structures, (void*)type->fields.array, type->fields.array);
							}
						}
						else if ((type->base_type == GLSL_TYPE_INPUTPATCH || type->base_type == GLSL_TYPE_OUTPUTPATCH) && type->inner_type->base_type == GLSL_TYPE_STRUCT)
						{
							if (hash_table_find(used_structures, type->inner_type) == NULL)
							{
								hash_table_insert(used_structures, (void*)type->inner_type, type->inner_type);
							}
						}
					}
				}
			}
			while( added_structure_types > 0 );
		}

		// Generate structures that allow support for multi-dimensional arrays.
		{
			hash_table* ht = hash_table_ctor(32, hash_table_pointer_hash, hash_table_pointer_compare);
			foreach_iter(exec_list_iterator, iter, used_md_arrays) 
			{
				md_array_entry* entry = (md_array_entry*)iter.get();
				declare_md_array_struct(entry->type, ht);
			}
			hash_table_dtor(ht);
		}

#ifdef OPTIMIZE_ANON_STRUCTURES_OUT
		// If a uniform block consists of a single, anonymous structure, don't declare this structure
		// separately. We'll remove it entirely during uniform block code generation, and name the
		// uniform block instead.
		for (unsigned i = 0; i < state->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = state->uniform_blocks[i];
			if (hash_table_find(used_uniform_blocks, block->name))
			{
				if (block->num_vars == 1)
				{
					ir_variable* var = block->vars[0];
					const glsl_type* type = var->type;

					if (type->base_type == GLSL_TYPE_STRUCT &&
						type->name &&
						!strcmp( var->name, block->name) &&
						!strncmp(type->name, "anon_struct_", 12))
					{
						hash_table_remove(used_structures, type);
					}
				}
			}
		}
#endif // OPTIMIZE_ANON_STRUCTURES_OUT

		for (unsigned i = 0; i < state->num_user_structures; i++)
		{
			const glsl_type *const s = state->user_structures[i];

			if (hash_table_find(used_structures, s) == NULL)
			{
				continue;
			}

			ralloc_asprintf_append(buffer, "struct %s\n{\n", s->name);

			if (s->length == 0)
			{
				if (bEmitPrecision)
				{
					ralloc_asprintf_append(buffer, "\thighp float glsl_doesnt_like_empty_structs;\n");
				}
				else
				{
					ralloc_asprintf_append(buffer, "\tfloat glsl_doesnt_like_empty_structs;\n");
				}
			}
			else
			{
				for (unsigned j = 0; j < s->length; j++)
				{
					const glsl_type* field_type = s->fields.structure[j].type;
					ralloc_asprintf_append(buffer, "\t");
					if (bEmitPrecision && field_type->base_type != GLSL_TYPE_STRUCT)
					{
						AppendPrecisionModifier(buffer, GetPrecisionModifier(field_type));
					}
					print_type_pre(field_type);
					ralloc_asprintf_append(buffer, " %s", s->fields.structure[j].name);
					print_type_post(field_type);
					ralloc_asprintf_append(buffer, ";\n");
				}
			}
			ralloc_asprintf_append(buffer, "};\n\n");
		}

		unsigned num_used_blocks = 0;
		for (unsigned i = 0; i < state->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = state->uniform_blocks[i];
			if (hash_table_find(used_uniform_blocks, block->name))
			{
				const char* block_name = block->name;
				if (state->has_packed_uniforms)
				{
					block_name = ralloc_asprintf(mem_ctx, "%sb%u",
						glsl_variable_tag_from_parser_target(state->target),
						num_used_blocks
						);
				}
				ralloc_asprintf_append(buffer, "layout(std140) uniform %s\n{\n", block_name);

				bool optimized_structure_out = false;

#ifdef OPTIMIZE_ANON_STRUCTURES_OUT
				if (block->num_vars == 1)
				{
					ir_variable* var = block->vars[0];
					const glsl_type* type = var->type;

					if (type->base_type == GLSL_TYPE_STRUCT &&
						type->name &&
						!strcmp( var->name, block->name) &&
						!strncmp(type->name, "anon_struct_", 12))
					{
						for (unsigned j = 0; j < type->length; j++)
						{
							const glsl_type* field_type = type->fields.structure[j].type;
							ralloc_asprintf_append(buffer, "\t");
							if (bEmitPrecision && field_type->base_type != GLSL_TYPE_STRUCT)
							{
								AppendPrecisionModifier(buffer, GetPrecisionModifier(field_type));
							}
							print_type_pre(field_type);
							ralloc_asprintf_append(buffer, " %s", type->fields.structure[j].name);
							print_type_post(field_type);
							ralloc_asprintf_append(buffer, ";\n");
						}
						ralloc_asprintf_append(buffer, "} %s;\n\n", block->name);
						optimized_structure_out = true;
					}
				}
#endif

				if (!optimized_structure_out)
				{
					for (unsigned var_index = 0; var_index < block->num_vars; ++var_index)
					{
						ir_variable* var = block->vars[var_index];
						const glsl_type* type = var->type;

						//EHart - name-mangle variables to prevent colliding names
						ralloc_asprintf_append(buffer, "#define %s %s%s\n", var->name, var->name, block_name);

						ralloc_asprintf_append(buffer, "\t");
						if (bEmitPrecision && type->base_type != GLSL_TYPE_STRUCT)
						{
							AppendPrecisionModifier(buffer, GetPrecisionModifier(type));
						}
						print_type_pre(type);
						ralloc_asprintf_append(buffer, " %s", var->name);
						print_type_post(type);
						ralloc_asprintf_append(buffer, ";\n");
					}
					ralloc_asprintf_append(buffer, "};\n\n");
				}

				num_used_blocks++;
			}
		}
	}

	void PrintPackedSamplers(_mesa_glsl_parse_state::TUniformList& Samplers, TStringToSetMap& TextureToSamplerMap)
	{
		bool bPrintHeader = true;
		bool bNeedsComma = false;
		for (_mesa_glsl_parse_state::TUniformList::iterator Iter = Samplers.begin(); Iter != Samplers.end(); ++Iter)
		{
			glsl_packed_uniform& Sampler = *Iter;
			std::string SamplerStates("");
			TStringToSetMap::iterator IterFound = TextureToSamplerMap.find(Sampler.Name);
			if (IterFound != TextureToSamplerMap.end())
			{
				TStringSet& ListSamplerStates = IterFound->second;
				check(!ListSamplerStates.empty());
				for (TStringSet::iterator IterSS = ListSamplerStates.begin(); IterSS != ListSamplerStates.end(); ++IterSS)
				{
					if (IterSS == ListSamplerStates.begin())
					{
						SamplerStates += "[";
					}
					else
					{
						SamplerStates += ",";
					}
					SamplerStates += *IterSS;
				}

				SamplerStates += "]";
			}

			ralloc_asprintf_append(
				buffer,
				"%s%s(%u:%u%s)",
				bNeedsComma ? "," : "",
				Sampler.Name.c_str(),
				Sampler.offset,
				Sampler.num_components,
				SamplerStates.c_str()
				);

			bNeedsComma = true;
		}
/*
		for (TStringToSetMap::iterator Iter = state->TextureToSamplerMap.begin(); Iter != state->TextureToSamplerMap.end(); ++Iter)
		{
		const std::string& Texture = Iter->first;
		TStringSet& Samplers = Iter->second;
		if (!Samplers.empty())
		{
		if (bFirstTexture)
		{
		bFirstTexture = false;
		}
		else
		{
		ralloc_asprintf_append(buffer, ",");
		}

		ralloc_asprintf_append(buffer, "%s(", Texture.c_str());
		bool bFirstSampler = true;
		for (TStringSet::iterator IterSamplers = Samplers.begin(); IterSamplers != Samplers.end(); ++IterSamplers)
		{
		if (bFirstSampler)
		{
		bFirstSampler = false;
		}
		else
		{
		ralloc_asprintf_append(buffer, ",");
		}

		ralloc_asprintf_append(buffer, "%s", IterSamplers->c_str());
		}
		ralloc_asprintf_append(buffer, ")");
		}
		}
		*/
	}

	bool PrintPackedUniforms(bool bPrintArrayType, char ArrayType, _mesa_glsl_parse_state::TUniformList& Uniforms, bool bFlattenUniformBuffers, bool NeedsComma)
	{
		bool bPrintHeader = true;
		for (glsl_packed_uniform& Uniform : Uniforms)
		{
			if (!bFlattenUniformBuffers || Uniform.CB_PackedSampler.empty())
			{
				if (bPrintArrayType && bPrintHeader)
				{
					ralloc_asprintf_append(buffer, "%s%c[",
						NeedsComma ? "," : "",
						ArrayType);
					bPrintHeader = false;
					NeedsComma = false;
				}
				ralloc_asprintf_append(
					buffer,
					"%s%s(%u:%u)",
					NeedsComma ? "," : "",
					Uniform.Name.c_str(),
					Uniform.offset,
					Uniform.num_components
					);
				NeedsComma = true;
			}
		}

		if (bPrintArrayType && !bPrintHeader)
		{
			ralloc_asprintf_append(buffer, "]");
		}

		return NeedsComma;
	}

	void PrintPackedGlobals(_mesa_glsl_parse_state* State)
	{
		//	@PackedGlobals: Global0(DestArrayType, DestOffset, SizeInFloats), Global1(DestArrayType, DestOffset, SizeInFloats), ...
		bool bNeedsHeader = true;
		bool bNeedsComma = false;
		for (auto& Pair : State->GlobalPackedArraysMap)
		{
			char ArrayType = Pair.first;
			if (ArrayType != EArrayType_Image && ArrayType != EArrayType_Sampler)
			{
				_mesa_glsl_parse_state::TUniformList& Uniforms = Pair.second;
				check(!Uniforms.empty());

				for (auto Iter = Uniforms.begin(); Iter != Uniforms.end(); ++Iter)
				{
					glsl_packed_uniform& Uniform = *Iter;
					if (!State->bFlattenUniformBuffers || Uniform.CB_PackedSampler.empty())
					{
						if (bNeedsHeader)
						{
							ralloc_asprintf_append(buffer, "// @PackedGlobals: ");
							bNeedsHeader = false;
						}

						ralloc_asprintf_append(
							buffer,
							"%s%s(%c:%u,%u)",
							bNeedsComma ? "," : "",
							Uniform.Name.c_str(),
							ArrayType,
							Uniform.offset,
							Uniform.num_components
							);
						bNeedsComma = true;
					}
				}
			}
		}

		if (!bNeedsHeader)
		{
			ralloc_asprintf_append(buffer, "\n");
		}
	}

	void PrintPackedUniformBuffers(_mesa_glsl_parse_state* State, bool bGroupFlattenedUBs)
	{
		// @PackedUB: UniformBuffer0(SourceIndex0): Member0(SourceOffset,SizeInFloats),Member1(SourceOffset,SizeInFloats), ...
		// @PackedUB: UniformBuffer1(SourceIndex1): Member0(SourceOffset,SizeInFloats),Member1(SourceOffset,SizeInFloats), ...
		// ...

		// First find all used CBs (since we lost that info during flattening)
		TStringSet UsedCBs;
		for (auto IterCB = State->CBPackedArraysMap.begin(); IterCB != State->CBPackedArraysMap.end(); ++IterCB)
		{
			for (auto Iter = IterCB->second.begin(); Iter != IterCB->second.end(); ++Iter)
			{
				_mesa_glsl_parse_state::TUniformList& Uniforms = Iter->second;
				for (auto IterU = Uniforms.begin(); IterU != Uniforms.end(); ++IterU)
				{
					if (!IterU->CB_PackedSampler.empty())
					{
						check(IterCB->first == IterU->CB_PackedSampler);
						UsedCBs.insert(IterU->CB_PackedSampler);
					}
				}
			}
		}

		check(UsedCBs.size() == State->CBPackedArraysMap.size());

		// Now get the CB index based off source declaration order, and print an info line for each, while creating the mem copy list
		unsigned CBIndex = 0;
		TCBDMARangeMap CBRanges;
		for (unsigned i = 0; i < State->num_uniform_blocks; i++)
		{
			const glsl_uniform_block* block = State->uniform_blocks[i];
			if (UsedCBs.find(block->name) != UsedCBs.end())
			{
				bool bNeedsHeader = true;

				// Now the members for this CB
				bool bNeedsComma = false;
				auto IterPackedArrays = State->CBPackedArraysMap.find(block->name);
				check(IterPackedArrays != State->CBPackedArraysMap.end());
				for (auto Iter = IterPackedArrays->second.begin(); Iter != IterPackedArrays->second.end(); ++Iter)
				{
					char ArrayType = Iter->first;
					check(ArrayType != EArrayType_Image && ArrayType != EArrayType_Sampler);

					_mesa_glsl_parse_state::TUniformList& Uniforms = Iter->second;
					for (auto IterU = Uniforms.begin(); IterU != Uniforms.end(); ++IterU)
					{
						glsl_packed_uniform& Uniform = *IterU;
						if (Uniform.CB_PackedSampler == block->name)
						{
							if (bNeedsHeader)
							{
								ralloc_asprintf_append(buffer, "// @PackedUB: %s(%u): ",
									block->name,
									CBIndex);
								bNeedsHeader = false;
							}

							ralloc_asprintf_append(buffer, "%s%s(%u,%u)",
								bNeedsComma ? "," : "",
								Uniform.Name.c_str(),
								Uniform.OffsetIntoCBufferInFloats,
								Uniform.SizeInFloats);

							bNeedsComma = true;
							unsigned SourceOffset = Uniform.OffsetIntoCBufferInFloats;
							unsigned DestOffset = Uniform.offset;
							unsigned Size = Uniform.SizeInFloats;
							unsigned DestCBIndex = bGroupFlattenedUBs ? std::distance(UsedCBs.begin(), UsedCBs.find(block->name)) : 0;
							unsigned DestCBPrecision = ArrayType;
							InsertRange(CBRanges, CBIndex, SourceOffset, Size, DestCBIndex, DestCBPrecision, DestOffset);
						}
					}
				}

				if (!bNeedsHeader)
				{
					ralloc_asprintf_append(buffer, "\n");
				}

				CBIndex++;
			}
		}

		//DumpSortedRanges(SortRanges(CBRanges));

		// @PackedUBCopies: SourceArray:SourceOffset-DestArray:DestOffset,SizeInFloats;SourceArray:SourceOffset-DestArray:DestOffset,SizeInFloats,...
		bool bFirst = true;
		for (auto& Pair : CBRanges)
		{
			TDMARangeList& List = Pair.second;
			for (auto IterList = List.begin(); IterList != List.end(); ++IterList)
			{
				if (bFirst)
				{
					ralloc_asprintf_append(buffer, bGroupFlattenedUBs ? "// @PackedUBCopies: " : "// @PackedUBGlobalCopies: ");
					bFirst = false;
				}
				else
				{
					ralloc_asprintf_append(buffer, ",");
				}

				if (bGroupFlattenedUBs)
				{
					ralloc_asprintf_append(buffer, "%u:%u-%u:%c:%u:%u", IterList->SourceCB, IterList->SourceOffset, IterList->DestCBIndex, IterList->DestCBPrecision, IterList->DestOffset, IterList->Size);
				}
				else
				{
					check(IterList->DestCBIndex == 0);
					ralloc_asprintf_append(buffer, "%u:%u-%c:%u:%u", IterList->SourceCB, IterList->SourceOffset, IterList->DestCBPrecision, IterList->DestOffset, IterList->Size);
				}
			}
		}

		if (!bFirst)
		{
			ralloc_asprintf_append(buffer, "\n");
		}
	}

	void PrintPackedUniforms(_mesa_glsl_parse_state* State, bool bGroupFlattenedUBs)
	{
		PrintPackedGlobals(State);

		if (State->bFlattenUniformBuffers && !State->CBuffersOriginal.empty())
		{
			PrintPackedUniformBuffers(State, bGroupFlattenedUBs);
		}
	}

	/**
	 * Print a list of external variables.
	 */
	void print_extern_vars(_mesa_glsl_parse_state* State, exec_list* extern_vars)
	{
		const char *type_str[GLSL_TYPE_MAX] = { "u", "i", "f", "f", "b", "t", "?", "?", "?", "?", "s", "os", "im", "ip", "op" };
		const char *col_str[] = { "", "", "2x", "3x", "4x" };
		const char *row_str[] = { "", "1", "2", "3", "4" };

		check( sizeof(type_str)/sizeof(char*) == GLSL_TYPE_MAX);

		bool need_comma = false;
		foreach_iter(exec_list_iterator, iter, *extern_vars)
		{
			ir_variable* var = ((extern_var*)iter.get())->var;
			const glsl_type* type = var->type;
			if (!strcmp(var->name,"gl_in"))
			{
				// Ignore it, as we can't properly frame this information in current format, and it's not used anyway for geometry shaders
				continue;
			}
			if (!strncmp(var->name,"in_",3) || !strncmp(var->name,"out_",4))
			{
				if (type->is_record())
				{
					// This is the specific case for GLSL >= 150, as we generate a struct with a member for each interpolator (which we still want to count)
					if (type->length != 1)
					{
						_mesa_glsl_warning(State, "Found a complex structure as in/out, counting is not implemented yet...\n");
						continue;
					}

					type = type->fields.structure->type;
				}
			}
			check(type);
			bool is_array = type->is_array();
			int array_size = is_array ? type->length : 0;
			if (is_array)
			{
				type = type->fields.array;
			}
			ralloc_asprintf_append(buffer, "%s%s%s%s",
				need_comma ? "," : "",
				type->base_type == GLSL_TYPE_STRUCT ? type->name : type_str[type->base_type],
				col_str[type->matrix_columns],
				row_str[type->vector_elements]);
			if (is_array)
			{
				ralloc_asprintf_append(buffer, "[%u]", array_size);
			}
			ralloc_asprintf_append(buffer, ";%d:%s", var->location, var->name);
			need_comma = true;
		}
	}

	int count_total_components(_mesa_glsl_parse_state* State, exec_list* extern_vars)
	{
		int total_components = 0;

		foreach_iter(exec_list_iterator, iter, *extern_vars)
		{
			ir_variable* var = ((extern_var*)iter.get())->var;
			const glsl_type* type = var->type;
			if (!strcmp(var->name, "gl_in"))
			{
				// Ignore it, as we can't properly frame this information in current format, and it's not used anyway for geometry shaders
				continue;
			}
			if (!strncmp(var->name, "in_", 3) || !strncmp(var->name, "out_", 4))
			{
				if (type->is_record())
				{
					// This is the specific case for GLSL >= 150, as we generate a struct with a member for each interpolator (which we still want to count)
					if (type->length != 1)
					{
						_mesa_glsl_warning(State, "Found a complex structure as in/out, counting is not implemented yet...\n");
						continue;
					}

					type = type->fields.structure->type;
				}
				check(type);
				bool is_array = type->is_array();
				total_components += (is_array ? type->length * type->vector_elements : type->vector_elements) * type->matrix_columns;
			}
		}
		return total_components;
	}

	void check_inout_limits(_mesa_glsl_parse_state* state)
	{
		if (CompileTarget == HCT_FeatureLevelES3_1)
		{
			// check the number of varying in/out varyings are within the minimum GLES 3.0 spec.
			const int GLES31_FragmentInComponentCountLimit = 60; // GL_MAX_FRAGMENT_INPUT_COMPONENTS
			const int GLES31_VertexOutComponentCountLimit = 64; // GL_MAX_VERTEX_OUTPUT_COMPONENTS

			int ins = 0;
			int outs = 0;
			if (ShaderTarget == fragment_shader && !input_variables.is_empty())
			{
				ins = count_total_components(state, &input_variables);
			}
			if (ShaderTarget == vertex_shader && !output_variables.is_empty())
			{
				outs = count_total_components(state, &output_variables);
			}

			if (ins > GLES31_FragmentInComponentCountLimit)
			{
				_mesa_glsl_error(state, "GLES 3.1 fragment input varying component count exceeded: %d used, max allowed %d\n", ins, GLES31_FragmentInComponentCountLimit);
			}

			if (outs > GLES31_VertexOutComponentCountLimit)
			{
				_mesa_glsl_error(state, "GLES 3.1 vertex output varying component count exceeded: %d used, max allowed %d\n", outs, GLES31_VertexOutComponentCountLimit);
			}
		}
	}

	/**
	 * Print the input/output signature for this shader.
	 */
	void print_signature(_mesa_glsl_parse_state *state, bool bGroupFlattenedUBs)
	{
		if (!input_variables.is_empty())
		{
			ralloc_asprintf_append(buffer, "// @Inputs: ");
			print_extern_vars(state, &input_variables);
			ralloc_asprintf_append(buffer, "\n");
		}
		if (!output_variables.is_empty())
		{
			ralloc_asprintf_append(buffer, "// @Outputs: ");
			print_extern_vars(state, &output_variables);
			ralloc_asprintf_append(buffer, "\n");
		}
		if (state->num_uniform_blocks > 0 && !state->bFlattenUniformBuffers)
		{
			bool first = true;
			int Index = 0;
			for (unsigned i = 0; i < state->num_uniform_blocks; i++)
			{
				const glsl_uniform_block* block = state->uniform_blocks[i];
				if (hash_table_find(used_uniform_blocks, block->name))
				{
					ralloc_asprintf_append(buffer, "%s%s(%d)",
						first ? "// @UniformBlocks: " : ",",
						block->name, Index);
					first = false;
					++Index;
				}
			}
			if (!first)
			{
				ralloc_asprintf_append(buffer, "\n");
			}
		}

		if (state->has_packed_uniforms)
		{
			PrintPackedUniforms(state, bGroupFlattenedUBs);

			if (!state->GlobalPackedArraysMap[EArrayType_Sampler].empty())
			{
				ralloc_asprintf_append(buffer, "// @Samplers: ");
				PrintPackedSamplers(
					state->GlobalPackedArraysMap[EArrayType_Sampler],
					state->TextureToSamplerMap
					);
				ralloc_asprintf_append(buffer, "\n");
			}

			if (!state->GlobalPackedArraysMap[EArrayType_Image].empty())
			{
				ralloc_asprintf_append(buffer, "// @UAVs: ");
				PrintPackedUniforms(
					false,
					EArrayType_Image,
					state->GlobalPackedArraysMap[EArrayType_Image],
					false,
					false
					);
				ralloc_asprintf_append(buffer, "\n");
			}
		}
		else
		{
			if (!uniform_variables.is_empty())
			{
				ralloc_asprintf_append(buffer, "// @Uniforms: ");
				print_extern_vars(state, &uniform_variables);
				ralloc_asprintf_append(buffer, "\n");
			}
			if (!sampler_variables.is_empty())
			{
				ralloc_asprintf_append(buffer, "// @Samplers: ");
				print_extern_vars(state, &sampler_variables);
				ralloc_asprintf_append(buffer, "\n");
			}
			if (!image_variables.is_empty())
			{
				ralloc_asprintf_append(buffer, "// @UAVs: ");
				print_extern_vars(state, &image_variables);
				ralloc_asprintf_append(buffer, "\n");
			}
		}
	}

	/**
	 * Print the layout directives for this shader.
	 */
	void print_layout(_mesa_glsl_parse_state *state)
	{
		if (early_depth_stencil && this->bUsesDiscard == false)
		{
			ralloc_asprintf_append(buffer, "layout(early_fragment_tests) in;\n");
		}
		if (state->target == compute_shader )
		{
			ralloc_asprintf_append(buffer, "layout( local_size_x = %d, "
				"local_size_y = %d, local_size_z = %d ) in;\n", wg_size_x,
				wg_size_y, wg_size_z );
		}

		if(state->target == tessellation_control_shader)
		{
			ralloc_asprintf_append(buffer, "layout(vertices = %d) out;\n", tessellation.outputcontrolpoints);
		}

		if(state->target == tessellation_evaluation_shader)
		{

			std::basic_stringstream<char, std::char_traits<char>, std::allocator<char> > str;

			switch (tessellation.outputtopology)
			{
				// culling is inverted, see TranslateCullMode in the OpenGL and D3D11 RHI
			case GLSL_OUTPUTTOPOLOGY_POINT:
				str << "point_mode";
				break;
			case GLSL_OUTPUTTOPOLOGY_LINE:
				str << "iso_lines";
				break;

			default:
			case GLSL_OUTPUTTOPOLOGY_NONE:
			case GLSL_OUTPUTTOPOLOGY_TRIANGLE_CW:
				str << "triangles, ccw";
				break;
			case GLSL_OUTPUTTOPOLOGY_TRIANGLE_CCW:
				str << "triangles, cw";
				break;
			}

			switch (tessellation.partitioning)
			{
			default:
			case GLSL_PARTITIONING_NONE:
			case GLSL_PARTITIONING_INTEGER:
				str << ", equal_spacing";
				break;
			case GLSL_PARTITIONING_FRACTIONAL_EVEN:
				str << ", fractional_even_spacing";
				break;
			case GLSL_PARTITIONING_FRACTIONAL_ODD:
				str << ", fractional_odd_spacing";
				break;
				// that assumes that the hull/control shader clamps the tessellation factors to be power of two
			case GLSL_PARTITIONING_POW2:
				str << ", equal_spacing";
				break;
			}
			ralloc_asprintf_append(buffer, "layout(%s) in;\n", str.str().c_str());
		}
#if 0
		if(state->target == tessellation_evaluation_shader || state->target == tessellation_control_shader)
		{
			ralloc_asprintf_append(buffer, "/* DEBUG DUMP\n");

			ralloc_asprintf_append(buffer, "tessellation.domain =  %s \n",  DomainStrings[tessellation.domain] );
			ralloc_asprintf_append(buffer, "tessellation.outputtopology =  %s \n", OutputTopologyStrings[tessellation.outputtopology] );
			ralloc_asprintf_append(buffer, "tessellation.partitioning =  %s \n", PartitioningStrings[tessellation.partitioning]);
			ralloc_asprintf_append(buffer, "tessellation.maxtessfactor =  %f \n", tessellation.maxtessfactor );
			ralloc_asprintf_append(buffer, "tessellation.outputcontrolpoints =  %d \n", tessellation.outputcontrolpoints );
			ralloc_asprintf_append(buffer, "tessellation.patchconstantfunc =  %s \n", tessellation.patchconstantfunc);
			ralloc_asprintf_append(buffer, " */\n");
		}
#endif		
	}

	void print_extensions(_mesa_glsl_parse_state* state, bool bUsesFramebufferFetchES2, bool bUsesDepthbufferFetchES2, bool bInUsesExternalTexture)
	{
		if (bInUsesExternalTexture)
		{
			if (CompileTarget == HCT_FeatureLevelES3_1)
			{
				ralloc_asprintf_append(buffer, "\n#ifdef GL_OES_EGL_image_external_essl3\n");
				ralloc_asprintf_append(buffer, "#extension GL_OES_EGL_image_external_essl3 : enable\n");
				ralloc_asprintf_append(buffer, "\n#endif\n");
			}

			ralloc_asprintf_append(buffer, "// Uses samplerExternalOES\n");
		}

		if (state->bSeparateShaderObjects && !state->bGenerateES && 
			((state->target == tessellation_control_shader) || (state->target == tessellation_evaluation_shader)))
		{
			ralloc_asprintf_append(buffer, "#extension GL_ARB_tessellation_shader : enable\n");
		}

		if (bUsesFramebufferFetchES2)
		{
			ralloc_asprintf_append(buffer, "\n#ifdef GL_EXT_shader_framebuffer_fetch\n");
			ralloc_asprintf_append(buffer, "#extension GL_EXT_shader_framebuffer_fetch : enable\n");
			ralloc_asprintf_append(buffer, "#define %sinout\n", FBF_StorageQualifier);
			ralloc_asprintf_append(buffer, "#elif defined(GL_ARM_shader_framebuffer_fetch)\n");
			ralloc_asprintf_append(buffer, "#extension GL_ARM_shader_framebuffer_fetch : enable\n");
			ralloc_asprintf_append(buffer, "#endif\n");
		}
		else if(FramebufferFetchMask || FramebufferFetchWriteMask)
		{
			ralloc_asprintf_append(buffer, "#ifdef UE_MRT_FRAMEBUFFER_FETCH\n");
			ralloc_asprintf_append(buffer, "#extension GL_EXT_shader_framebuffer_fetch : enable\n");
			ralloc_asprintf_append(buffer, "#endif\n");
			ralloc_asprintf_append(buffer, "#ifdef UE_MRT_PLS\n");
			ralloc_asprintf_append(buffer, "#extension GL_EXT_shader_pixel_local_storage : enable\n");
			ralloc_asprintf_append(buffer, "#endif\n");
		}	
		
		if (bUsesDepthbufferFetchES2 || FramebufferFetchMask || FramebufferFetchWriteMask)		
		{
			ralloc_asprintf_append(buffer, "\n#ifdef GL_ARM_shader_framebuffer_fetch_depth_stencil\n");
			ralloc_asprintf_append(buffer, "#extension GL_ARM_shader_framebuffer_fetch_depth_stencil : enable\n");
			if (!(bUsesFramebufferFetchES2 || FramebufferFetchMask || FramebufferFetchWriteMask))
			{
				ralloc_asprintf_append(buffer, "#elif defined(GL_EXT_shader_framebuffer_fetch)\n");
				ralloc_asprintf_append(buffer, "#extension GL_EXT_shader_framebuffer_fetch : enable\n");
				ralloc_asprintf_append(buffer, "#define %sinout\n", FBF_StorageQualifier);
			}
			ralloc_asprintf_append(buffer, "#endif\n");
		}

		// Cube map arrays are required for deferred rendering on mobile
		ralloc_asprintf_append(buffer, "\n#if (defined(UE_MRT_PLS) || defined(UE_MRT_FRAMEBUFFER_FETCH)) && defined(GL_EXT_texture_cube_map_array)\n");
		ralloc_asprintf_append(buffer, "#extension GL_EXT_texture_cube_map_array : enable\n");
		ralloc_asprintf_append(buffer, "\n#endif\n");

		if (bUsesFramebufferFetchES2 || bUsesDepthbufferFetchES2)
		{
			ralloc_asprintf_append(buffer, "\n#ifndef %s\n", FBF_StorageQualifier);
			ralloc_asprintf_append(buffer, "#define %sout\n", FBF_StorageQualifier);
			ralloc_asprintf_append(buffer, "#endif\n");
		}

		if (CompileTarget == HCT_FeatureLevelES3_1Ext)
		{
			ralloc_asprintf_append(buffer, "\n#ifdef GL_EXT_gpu_shader5\n");
			ralloc_asprintf_append(buffer, "#extension GL_EXT_gpu_shader5 : enable\n");
			ralloc_asprintf_append(buffer, "\n#endif\n");
			
			ralloc_asprintf_append(buffer, "\n#ifdef GL_OES_shader_image_atomic\n");
			ralloc_asprintf_append(buffer, "#extension GL_OES_shader_image_atomic : enable\n");
			ralloc_asprintf_append(buffer, "\n#endif\n");

			ralloc_asprintf_append(buffer, "\n#ifdef GL_EXT_texture_buffer\n");
			ralloc_asprintf_append(buffer, "#extension GL_EXT_texture_buffer : enable\n");
			ralloc_asprintf_append(buffer, "\n#endif\n");

			ralloc_asprintf_append(buffer, "\n#ifdef GL_EXT_texture_cube_map_array\n");
			ralloc_asprintf_append(buffer, "#extension GL_EXT_texture_cube_map_array : enable\n");
			ralloc_asprintf_append(buffer, "\n#endif\n");

			ralloc_asprintf_append(buffer, "\n#ifdef GL_EXT_shader_io_blocks\n");
			ralloc_asprintf_append(buffer, "#extension GL_EXT_shader_io_blocks : enable\n");
			ralloc_asprintf_append(buffer, "\n#endif\n");

			if (ShaderTarget == geometry_shader)
			{
				ralloc_asprintf_append(buffer, "#extension GL_EXT_geometry_shader : enable\n");
			}

			if (ShaderTarget == tessellation_control_shader || ShaderTarget == tessellation_evaluation_shader)
			{
				ralloc_asprintf_append(buffer, "#extension GL_EXT_tessellation_shader : enable\n");
			}
		}
		else if ((bIsES || bIsES31))
		{
			if (bUseImageAtomic)
			{
				ralloc_asprintf_append(buffer, "\n#ifdef GL_OES_shader_image_atomic\n");
				ralloc_asprintf_append(buffer, "#extension GL_OES_shader_image_atomic : enable\n");
				ralloc_asprintf_append(buffer, "\n#endif\n");
			}

			if (bUsesTextureBuffer)
			{
				// Not supported by ES3.1 spec, but many phones support this extension
				// GPU particles require this
				// App shall not use a shader if this extension is not supported on device
				ralloc_asprintf_append(buffer, "\n#ifdef GL_EXT_texture_buffer\n");
				ralloc_asprintf_append(buffer, "#extension GL_EXT_texture_buffer : enable\n");
				ralloc_asprintf_append(buffer, "\n#endif\n");
			}
		}
		ralloc_asprintf_append(buffer, "// end extensions\n");
	}

public:

	/** Constructor. */
	ir_gen_glsl_visitor(bool bInIsES, bool bInEmitPrecision, bool bInIsWebGL, EHlslCompileTarget InCompileTarget, _mesa_glsl_parser_targets InShaderTarget,
						bool bInGenerateLayoutLocations, bool bInDefaultPrecisionIsHalf, bool bInNoGlobalUniforms, bool bInUsesFrameBufferFetch, bool bInUsesDepthbufferFetch,
						uint32_t InFramebufferFetchMask, uint32_t InFramebufferFetchWriteMask, bool bInUsesExternalTexture)
		: early_depth_stencil(false)
		, bIsES(bInIsES)
		, bEmitPrecision(bInEmitPrecision)
		, bIsES31(InCompileTarget == HCT_FeatureLevelES3_1 || InCompileTarget == HCT_FeatureLevelES3_1Ext)
		, bIsWebGL(bInIsWebGL)
		, CompileTarget(InCompileTarget)		, ShaderTarget(InShaderTarget)
		, bGenerateLayoutLocations(bInGenerateLayoutLocations)
		, bDefaultPrecisionIsHalf(bInDefaultPrecisionIsHalf)
		, bUsesFrameBufferFetch(bInUsesFrameBufferFetch)
		, bUsesDepthbufferFetch(bInUsesDepthbufferFetch)
		, bHasGeneratedDepthTargetInput(false)
		, FramebufferFetchMask(InFramebufferFetchMask)
		, FramebufferFetchWriteMask(InFramebufferFetchWriteMask)
		, bUsesExternalTexture(bInUsesExternalTexture)
		, buffer(0)
		, indentation(0)
		, scope_depth(0)
		, temp_id(0)
		, global_id(0)
		, needs_semicolon(false)
		, should_print_uint_literals_as_ints(false)
		, loop_count(0)
		, bUsesTextureBuffer(false)
		, bUseImageAtomic(false)
		, bUsesDiscard(false)
		, bUsesInstanceID(false)
		, bNoGlobalUniforms(bInNoGlobalUniforms)
	{
		printable_names = hash_table_ctor(32, hash_table_pointer_hash, hash_table_pointer_compare);
		used_structures = hash_table_ctor(32, hash_table_pointer_hash, hash_table_pointer_compare);
		used_uniform_blocks = hash_table_ctor(32, hash_table_string_hash, hash_table_string_compare);
	}

	/** Destructor. */
	virtual ~ir_gen_glsl_visitor()
	{
		hash_table_dtor(printable_names);
		hash_table_dtor(used_structures);
		hash_table_dtor(used_uniform_blocks);
	}

	/**
	 * Executes the visitor on the provided ir.
	 * @returns the GLSL source code generated.
	 */
	const char* run(exec_list* ir, _mesa_glsl_parse_state* state, bool bGroupFlattenedUBs)
	{
		mem_ctx = ralloc_context(NULL);

		char* code_buffer = ralloc_asprintf(mem_ctx, "");
		buffer = &code_buffer;

		char* default_precision_buffer = ralloc_asprintf(mem_ctx, "");

		if (bEmitPrecision && ShaderTarget == fragment_shader)
		{
			const char* DefaultPrecision = bDefaultPrecisionIsHalf ? "mediump" : "highp";
			ralloc_asprintf_append(&default_precision_buffer, "precision %s float;\n", DefaultPrecision);
			// always use highp for integers as shaders use them as bit storage
			ralloc_asprintf_append(&default_precision_buffer, "precision %s int;\n", "highp");
		}

		// HLSLCC_DX11ClipSpace adjustment
		{
			const char* func_clipControlAdjustments =
R"RawStrDelimiter(
void compiler_internal_AdjustInputSemantic(inout vec4 TempVariable)
{
#if HLSLCC_DX11ClipSpace
	TempVariable.y = -TempVariable.y;
	TempVariable.z = ( TempVariable.z + TempVariable.w ) / 2.0;
#endif
}

void compiler_internal_AdjustOutputSemantic(inout vec4 Src)
{
#if HLSLCC_DX11ClipSpace
	Src.y = -Src.y;
	Src.z = ( 2.0 * Src.z ) - Src.w;
#endif
}

bool compiler_internal_AdjustIsFrontFacing(bool isFrontFacing)
{
#if HLSLCC_DX11ClipSpace
	return !isFrontFacing;
#else
	return isFrontFacing;
#endif
}
)RawStrDelimiter";
			
			if (ShaderTarget != compute_shader)
			{
				ralloc_asprintf_append(buffer, func_clipControlAdjustments);
			}
		}

		if (FramebufferFetchMask || FramebufferFetchWriteMask)
		{
			// Framebuffer Depth Fetch
			ralloc_asprintf_append(buffer, "highp %s %s()\n"
				"{\n"
				"\treturn gl_LastFragDepthARM;\n"
				"}\n", GL_FRAMEBUFFER_FETCH_TYPE[FRAMEBUFFER_FETCH_DEPTH_INDEX], GL_FRAMEBUFFER_FETCH[FRAMEBUFFER_FETCH_DEPTH_INDEX]);

			ralloc_asprintf_append(buffer, "void %s(%s input_vec)\n"
				"{\n"
				"\tgl_FragDepth = input_vec;"
				"}\n", GL_FRAMEBUFFER_FETCH_WRITE[FRAMEBUFFER_FETCH_DEPTH_INDEX], GL_FRAMEBUFFER_FETCH_TYPE[FRAMEBUFFER_FETCH_DEPTH_INDEX]);

			// Framebuffer Fetch
			ralloc_asprintf_append(buffer, "#ifdef UE_MRT_FRAMEBUFFER_FETCH\n");

			for (int32_t i = 0; i < UE_ARRAY_COUNT(GL_FRAMEBUFFER_FETCH); ++i)
			{
				if (FramebufferFetchMask & (1 << i) || FramebufferFetchWriteMask & (1 << i))
				{
					if (i == FRAMEBUFFER_FETCH_DEPTH_INDEX)
					{
						continue;
					}
					
					ralloc_asprintf_append(buffer, "layout(location = %d) inout %s out_Target%d;\n", i, GL_FRAMEBUFFER_FETCH_TYPE[i], i);
					ralloc_asprintf_append(buffer, "highp %s %s()\n"
						"{\n"
						"\treturn out_Target%d;\n"
						"}\n", GL_FRAMEBUFFER_FETCH_TYPE[i], GL_FRAMEBUFFER_FETCH[i], i);

					ralloc_asprintf_append(buffer, "void %s(%s input_vec)\n"
						"{\n"
						"\tout_Target%d = input_vec;\n"
						"}\n", GL_FRAMEBUFFER_FETCH_WRITE[i], GL_FRAMEBUFFER_FETCH_TYPE[i], i);
				}
			}

			ralloc_asprintf_append(buffer, "#endif\n");
			// End Framebuffer Fetch

			// Pixel Local Storage
			ralloc_asprintf_append(buffer, "#ifdef UE_MRT_PLS\n");
		
			if (FramebufferFetchMask && FramebufferFetchWriteMask)
			{
				ralloc_asprintf_append(buffer, "__pixel_localEXT PLSGBufferData\n{ \n");
			}
			else if (FramebufferFetchMask)
			{
				ralloc_asprintf_append(buffer, "__pixel_local_inEXT PLSGBufferData\n{ \n");
			}
			else
			{
				ralloc_asprintf_append(buffer, "__pixel_local_outEXT PLSGBufferData\n{ \n");
			}

			for (int32_t i = 0; i < FRAMEBUFFER_FETCH_DEPTH_INDEX; ++i)
			{
				ralloc_asprintf_append(buffer, "layout(%s) highp %s out_Target%d;\n", GL_PLS_IMAGE_FORMAT[i], GL_FRAMEBUFFER_FETCH_TYPE[i], i);
			}

			ralloc_asprintf_append(buffer, "} PLSGBuffer;\n\n");

			for (int32_t i = 0; i < UE_ARRAY_COUNT(GL_FRAMEBUFFER_FETCH); ++i)
			{
				if (i == FRAMEBUFFER_FETCH_DEPTH_INDEX)
				{
					continue;
				}

				if (FramebufferFetchMask)
				{
					ralloc_asprintf_append(buffer, "highp %s %s()\n"
						"{\n"
						"\treturn PLSGBuffer.out_Target%d;\n"
						"}\n", GL_FRAMEBUFFER_FETCH_TYPE[i], GL_FRAMEBUFFER_FETCH[i], i);
				}

				if(FramebufferFetchWriteMask)
				{
						ralloc_asprintf_append(buffer, "void %s(%s input_vec)\n"
							"{\n"
							"\tPLSGBuffer.out_Target%d = input_vec;\n"
							"}\n", GL_FRAMEBUFFER_FETCH_WRITE[i], GL_FRAMEBUFFER_FETCH_TYPE[i], i);
				}
			}

			ralloc_asprintf_append(buffer, "#endif\n");
		}
		// End Pixel Local Storage


		foreach_iter(exec_list_iterator, iter, *ir)
		{
			ir_instruction *inst = (ir_instruction *)iter.get();
			do_visit(inst);
		}
		buffer = 0;

		check_inout_limits(state);

		char* code_footer = ralloc_asprintf(mem_ctx, "");
		buffer = &code_footer;
		//
		buffer = 0;

		char* decl_buffer = ralloc_asprintf(mem_ctx, "");
		buffer = &decl_buffer;
		declare_structs(state);
		buffer = 0;

		char* signature = ralloc_asprintf(mem_ctx, "");
		buffer = &signature;
		print_signature(state, bGroupFlattenedUBs);
		buffer = 0;

		const char* geometry_layouts = "";
		if (state->maxvertexcount>0)
		{
			check(state->geometryinput>0);
			check(state->outputstream_type>0);
			geometry_layouts = ralloc_asprintf(
				mem_ctx,
				"\nlayout(%s) in;\nlayout(%s, max_vertices = %u) out;\n\n",
				GeometryInputStrings[state->geometryinput],
				OutputStreamTypeStrings[state->outputstream_type],
				state->maxvertexcount);
		}
		char* layout = ralloc_asprintf(mem_ctx, "");
		buffer = &layout;
		print_layout(state);
		buffer = 0;

		char* Extensions = ralloc_asprintf(mem_ctx, "");
		buffer = &Extensions;
		print_extensions(state, bUsesFrameBufferFetch, bUsesDepthbufferFetch, bUsesExternalTexture);
		if (state->bSeparateShaderObjects && !(state->bGenerateES || CompileTarget == HCT_FeatureLevelES3_1))
		{
			switch (state->target)
			{
				case geometry_shader:
					ralloc_asprintf_append(buffer, "in gl_PerVertex\n"
										   "{\n"
										   "\tvec4 gl_Position;\n"
										   "\tfloat gl_ClipDistance[];\n"
										   "} gl_in[];\n"
										   );
				case vertex_shader:
					ralloc_asprintf_append(buffer, "out gl_PerVertex\n"
										   "{\n"
										   "\tvec4 gl_Position;\n"
										   "\tfloat gl_ClipDistance[];\n"
										   "};\n"
										   );
					break;
				case tessellation_control_shader:
					ralloc_asprintf_append(buffer, "in gl_PerVertex\n"
										   "{\n"
										   "\tvec4 gl_Position;\n"
										   "\tfloat gl_ClipDistance[];\n"
										   "} gl_in[gl_MaxPatchVertices];\n"
										   );
					ralloc_asprintf_append(buffer, "out gl_PerVertex\n"
										   "{\n"
										   "\tvec4 gl_Position;\n"
										   "\tfloat gl_ClipDistance[];\n"
										   "} gl_out[];\n"
										   );
					break;
				case tessellation_evaluation_shader:
					ralloc_asprintf_append(buffer, "in gl_PerVertex\n"
										   "{\n"
										   "\tvec4 gl_Position;\n"
										   "\tfloat gl_ClipDistance[];\n"
										   "} gl_in[gl_MaxPatchVertices];\n"
										   );
					ralloc_asprintf_append(buffer, "out gl_PerVertex\n"
										   "{\n"
										   "\tvec4 gl_Position;\n"
										   "\tfloat gl_ClipDistance[];\n"
										   "};\n"
										   );
					break;
				case fragment_shader:
				case compute_shader:
				default:
					break;
			}
		}
		buffer = 0;

		char* full_buffer = ralloc_asprintf(
			state,
			"// Compiled by HLSLCC %d.%d\n%s#version %u %s\n%s%s%s%s%s%s%s\n",
			HLSLCC_VersionMajor, HLSLCC_VersionMinor,
			signature,
			state->language_version,
			state->language_version == 310 ? "es" : "",
			Extensions,
			default_precision_buffer,
			geometry_layouts,
			layout,
			decl_buffer,
			code_buffer,
			code_footer
			);
		ralloc_free(mem_ctx);

		return full_buffer;
	}
};

struct FBreakPrecisionChangesVisitor : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* State;
	const bool bDefaultPrecisionIsHalf;

	FBreakPrecisionChangesVisitor(_mesa_glsl_parse_state* InState, bool bInDefaultPrecisionIsHalf) 
		: State(InState)
		, bDefaultPrecisionIsHalf(bInDefaultPrecisionIsHalf)
	{}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (!RValuePtr || !*RValuePtr)
		{
			return;
		}
		bool bGenerateNewVar = false;
		auto* RValue = *RValuePtr;
		auto* Expression = RValue->as_expression();
		auto* Constant = RValue->as_constant();
		if (Expression)
		{
			if (bDefaultPrecisionIsHalf)
			{
				switch (Expression->operation)
				{
					case ir_unop_i2f:
					case ir_unop_b2f:
					case ir_unop_u2f:
						// integers always use highp
						bGenerateNewVar = false;
						break;

					case ir_unop_i2h:
					case ir_unop_b2h:
					case ir_unop_u2h:
						// integers always use highp
						bGenerateNewVar = true;
						break;

					case ir_unop_h2f:
					case ir_unop_f2h:
						if (!Expression->operands[0]->as_texture())
						{
							bGenerateNewVar = true;
						}
						break;
				}
			}
		}
		else if (Constant)
		{
/*
			if ((bDefaultPrecisionIsHalf && Constant->type->base_type == GLSL_TYPE_HALF) ||
				(!bDefaultPrecisionIsHalf && Constant->type->base_type == GLSL_TYPE_FLOAT))
			{
				bGenerateNewVar = true;
			}
*/
		}
		if (bGenerateNewVar)
		{
			auto* NewVar = new(State)ir_variable(RValue->type, nullptr, ir_var_temporary);
			auto* NewAssignment = new(State)ir_assignment(new(State)ir_dereference_variable(NewVar), RValue);
			*RValuePtr = new(State)ir_dereference_variable(NewVar);
			base_ir->insert_before(NewVar);
			base_ir->insert_before(NewAssignment);
		}
	}
};

void ir_gen_glsl_visitor::AddTypeToUsedStructs(const glsl_type* type)
{
	if (type->base_type == GLSL_TYPE_STRUCT)
	{
		if (hash_table_find(used_structures, type) == NULL)
		{
			hash_table_insert(used_structures, (void*)type, type);
		}
	}

	if (type->base_type == GLSL_TYPE_ARRAY && type->fields.array->base_type == GLSL_TYPE_STRUCT)
	{
		if (hash_table_find(used_structures, type->fields.array) == NULL)
		{
			hash_table_insert(used_structures, (void*)type->fields.array, type->fields.array);
		}
	}

	if ((type->base_type == GLSL_TYPE_INPUTPATCH || type->base_type == GLSL_TYPE_OUTPUTPATCH) && type->inner_type->base_type == GLSL_TYPE_STRUCT)
	{
		if (hash_table_find(used_structures, type->inner_type) == NULL)
		{
			hash_table_insert(used_structures, (void*)type->inner_type, type->inner_type);
		}
	}
}

char* FGlslCodeBackend::GenerateCode(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency)
{
	FixRedundantCasts(ir);
	//IRDump(ir);

	const bool bDefaultPrecisionIsHalf = (Frequency == HSF_PixelShader) && (HlslCompileFlags & HLSLCC_UseFullPrecisionInPS) == 0;
	const bool bUsesExternalTexture = ((HlslCompileFlags & HLSLCC_UsesExternalTexture) == HLSLCC_UsesExternalTexture);
	
	FBreakPrecisionChangesVisitor BreakPrecisionChangesVisitor(state, bDefaultPrecisionIsHalf);
	BreakPrecisionChangesVisitor.run(ir);

	if (!AllowsESLanguage())
	{
		state->bGenerateES = false;
	}

	const bool bGroupFlattenedUBs = ((HlslCompileFlags & HLSLCC_GroupFlattenedUniformBuffers) == HLSLCC_GroupFlattenedUniformBuffers);
	const bool bGenerateLayoutLocations = state->bGenerateLayoutLocations;
	const bool bEmitPrecision = WantsPrecisionModifiers();
	const bool bUsesFrameBufferFetch = Frequency == HSF_PixelShader && UsesUEIntrinsic(ir, FRAMEBUFFER_FETCH_ES2);
	const bool bUsesDepthBufferFetch = Frequency == HSF_PixelShader && UsesUEIntrinsic(ir, DEPTHBUFFER_FETCH_ES2);

	uint32 FramebufferFetchMask = 0;
	uint32 FramebufferFetchWriteMask = 0;
	if (Frequency == HSF_PixelShader)
	{
		for (int32 i = 0; i < UE_ARRAY_COUNT(GL_FRAMEBUFFER_FETCH); ++i)
		{
			FramebufferFetchWriteMask |= UsesUEIntrinsic(ir, GL_FRAMEBUFFER_FETCH_WRITE[i]) ? 1 << i : 0;
			FramebufferFetchMask |= UsesUEIntrinsic(ir, GL_FRAMEBUFFER_FETCH[i]) ? 1 << i : 0;
		}
	}

	ir_gen_glsl_visitor visitor(state->bGenerateES,
								bEmitPrecision,
								false,
								Target,
								state->target,
								bGenerateLayoutLocations,
								bDefaultPrecisionIsHalf,
								!AllowsGlobalUniforms(),
								bUsesFrameBufferFetch,
								bUsesDepthBufferFetch,
								FramebufferFetchMask,
								FramebufferFetchWriteMask,
								bUsesExternalTexture);

	const char* code = visitor.run(ir, state, bGroupFlattenedUBs);
	return _strdup(code);
}

// Verify if SampleLevel() is used
struct SPromoteSampleLevel : public ir_hierarchical_visitor
{
	_mesa_glsl_parse_state* ParseState;
	const bool bIsVertexShader;
	SPromoteSampleLevel(_mesa_glsl_parse_state* InParseState, bool bInIsVertexShader) :
		ParseState(InParseState),
		bIsVertexShader(bInIsVertexShader)
	{
	}

	virtual ir_visitor_status visit_leave(ir_texture* IR) override
	{
		if (IR->offset)
		{
			YYLTYPE loc;
			loc.first_column = IR->SourceLocation.Column;
			loc.first_line = IR->SourceLocation.Line;
			loc.source_file = IR->SourceLocation.SourceFile;
			_mesa_glsl_error(&loc, ParseState, "Texture offset not supported on GLSL ES\n");
		}

		return visit_continue;
	}
}; 


// Converts an array index expression using an integer input attribute, to a float input attribute using a conversion to int
struct SConvertIntVertexAttributeES final : public ir_hierarchical_visitor
{
	_mesa_glsl_parse_state* ParseState;
	exec_list* FunctionBody;
	int InsideArrayDeref;
	std::map<ir_variable*, ir_variable*> ConvertedVarMap;

	SConvertIntVertexAttributeES(_mesa_glsl_parse_state* InParseState, exec_list* InFunctionBody) : ParseState(InParseState), FunctionBody(InFunctionBody), InsideArrayDeref(0)
	{
	}

	virtual ~SConvertIntVertexAttributeES()
	{
	}

	virtual ir_visitor_status visit_enter(ir_dereference_array* DeRefArray) override
	{
		// Break the array dereference so we know we want to modify the array index part
		auto Result = ir_hierarchical_visitor::visit_enter(DeRefArray);
		++InsideArrayDeref;
		DeRefArray->array_index->accept(this);
		--InsideArrayDeref;

		return visit_continue;
	}

	virtual ir_visitor_status visit(ir_dereference_variable* DeRefVar) override
	{
		if (InsideArrayDeref > 0)
		{
			ir_variable* SourceVar = DeRefVar->var;
			if (SourceVar->mode == ir_var_in)
			{
				// First time it still is an integer, so add the temporary and a conversion, and switch to float
				if (SourceVar->type->is_integer())
				{
					check(SourceVar->type->is_integer() && !SourceVar->type->is_matrix() && !SourceVar->type->is_array());

					// Double check we haven't processed this
					auto IterFound = ConvertedVarMap.find(SourceVar);
					check(IterFound == ConvertedVarMap.end());

					// New temp var
					ir_variable* NewVar = new(ParseState)ir_variable(SourceVar->type, NULL, ir_var_temporary);
					base_ir->insert_before(NewVar);

					// Switch original type to float
					SourceVar->type = glsl_type::get_instance(GLSL_TYPE_FLOAT, SourceVar->type->vector_elements, 1);

					// Convert float to int
					ir_dereference_variable* NewSourceDeref = new(ParseState)ir_dereference_variable(SourceVar);
					ir_expression* NewCastExpression = new(ParseState)ir_expression(ir_unop_f2i, NewSourceDeref);
					ir_assignment* NewAssigment = new(ParseState)ir_assignment(new(ParseState)ir_dereference_variable(NewVar), NewCastExpression);
					base_ir->insert_before(NewAssigment);

					// Add the entry and modify the original Var
					ConvertedVarMap[SourceVar] = NewVar;
					DeRefVar->var = NewVar;
				}
				else
				{
					auto IterFound = ConvertedVarMap.find(SourceVar);
					if (IterFound != ConvertedVarMap.end())
					{
						DeRefVar->var = IterFound->second;
					}
				}
			}
		}

		return ir_hierarchical_visitor::visit(DeRefVar);
	}
};


bool FGlslCodeBackend::ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency)
{
	if (ParseState->bGenerateES)
	{
		bool bIsVertexShader = (Frequency == HSF_VertexShader);

		// Handle SampleLevel
		{
			SPromoteSampleLevel Visitor(ParseState, bIsVertexShader);
			Visitor.run(Instructions);
		}

		// Handle matrices (flatten to vectors so we can support non-sqaure)
		ExpandMatricesIntoArrays(Instructions, ParseState);

		// Handle integer vertex attributes used as array indices
		if (bIsVertexShader)
		{
			SConvertIntVertexAttributeES ConvertIntVertexAttributeVisitor(ParseState, Instructions);
			ConvertIntVertexAttributeVisitor.run(Instructions);
		}
	}

	return true;
}

/** Qualifers that apply to semantics. */
union FSemanticQualifier
{
	struct
	{
		unsigned bCentroid : 1;
		unsigned InterpolationMode : 2;
		unsigned bIsPatchConstant : 1;
	} Fields;
	unsigned Packed;

	FSemanticQualifier() : Packed(0) {}
};

/** Information on system values. */
struct FSystemValue
{
	const char* Semantic;
	const glsl_type* Type;
	const char* GlslName;
	ir_variable_mode Mode;
	bool bOriginUpperLeft;
	bool bArrayVariable;
	bool bApplyClipSpaceAdjustment;
	bool bESOnly;
};

/** Vertex shader system values. */
static FSystemValue VertexSystemValueTable[] =
{
	{ "SV_VertexID", glsl_type::int_type, "gl_VertexID", ir_var_in, false, false, false, false },
	{ "SV_InstanceID", glsl_type::int_type, "gl_InstanceID", ir_var_in, false, false, false, false },
	{ "SV_Position", glsl_type::vec4_type, "gl_Position", ir_var_out, false, false, true, false },
	{ "SV_ViewID", glsl_type::uint_type, "gl_ViewID_OVR", ir_var_in, false, false, false, true }, // Mobile multi-view support
	{ NULL, NULL, NULL, ir_var_auto, false, false, false, false }
};

/** Pixel shader system values. */
static FSystemValue PixelSystemValueTable[] =
{
	{ "SV_Depth", glsl_type::float_type, "gl_FragDepth", ir_var_out, false, false, false, false },
	{ "SV_Position", glsl_type::vec4_type, "gl_FragCoord", ir_var_in, true, false, false, false },
	{ "SV_IsFrontFace", glsl_type::bool_type, "gl_FrontFacing", ir_var_in, false, false, true, false },
	{ "SV_PrimitiveID", glsl_type::int_type, "gl_PrimitiveID", ir_var_in, false, false, false, false },
	{ "SV_RenderTargetArrayIndex", glsl_type::int_type, "gl_Layer", ir_var_in, false, false, false, false },
	{ "SV_Target0", glsl_type::half4_type, "gl_FragColor", ir_var_out, false, false, false, true },
	{ "SV_ViewID", glsl_type::uint_type, "gl_ViewID_OVR", ir_var_in, false, false, false, true }, // Mobile multi-view support
	{ "SV_SampleIndex", glsl_type::uint_type, "gl_SampleID", ir_var_in, false, false, false, false }, // Mobile multi-view support
	{ NULL, NULL, NULL, ir_var_auto, false, false, false }
};

/** Geometry shader system values. */
static FSystemValue GeometrySystemValueTable[] =
{
	{ "SV_VertexID", glsl_type::int_type, "gl_VertexID", ir_var_in, false, false, false, false },
	{ "SV_InstanceID", glsl_type::int_type, "gl_InstanceID", ir_var_in, false, false, false, false },
	{ "SV_Position", glsl_type::vec4_type, "gl_Position", ir_var_in, false, true, true, false },
	{ "SV_Position", glsl_type::vec4_type, "gl_Position", ir_var_out, false, false, true, false },
	{ "SV_RenderTargetArrayIndex", glsl_type::int_type, "gl_Layer", ir_var_out, false, false, false, false },
	{ "SV_PrimitiveID", glsl_type::int_type, "gl_PrimitiveID", ir_var_out, false, false, false, false },
	{ "SV_PrimitiveID", glsl_type::int_type, "gl_PrimitiveIDIn", ir_var_in, false, false, false, false },
	{ NULL, NULL, NULL, ir_var_auto, false, false, false, false }
};


/** Unsupported shader system values: Hull and Domain no longer supported in UE5, Mesh and Amplification shaders never supported for GLSL backend. */
static FSystemValue DummySystemValueTable[] =
{
	{ NULL, NULL, NULL, ir_var_auto, false, false, false, false }
};

/** Compute shader system values. */
static FSystemValue ComputeSystemValueTable[] =
{
	{ "SV_DispatchThreadID", glsl_type::uvec3_type, "gl_GlobalInvocationID", ir_var_in, false, false, false, false },
	{ "SV_GroupID", glsl_type::uvec3_type, "gl_WorkGroupID", ir_var_in, false, false, false, false },
	{ "SV_GroupIndex", glsl_type::uint_type, "gl_LocalInvocationIndex", ir_var_in, false, false, false, false },
	{ "SV_GroupThreadID", glsl_type::uvec3_type, "gl_LocalInvocationID", ir_var_in, false, false, false, false },
	{ NULL, NULL, NULL, ir_var_auto, false, false, false, false }
};

FSystemValue* SystemValueTable[HSF_FrequencyCount] =
{
	VertexSystemValueTable,
	PixelSystemValueTable,
	GeometrySystemValueTable,
	DummySystemValueTable,
	DummySystemValueTable,
	ComputeSystemValueTable
};


#define CUSTOM_LAYER_INDEX_SEMANTIC "HLSLCC_LAYER_INDEX"

// Returns the number of slots an in/out variable occupies. This is determined by the variable type, e.g. vec4 has offset 1, mat4 has offset 4.
// For geometry shader input variables, the first array specifier is ignored as geometry shaders get their input from multiple vertex shader invocations.
static unsigned GetInOutVariableSlotSize(EHlslShaderFrequency Frequency, ir_variable_mode Mode, const struct glsl_type* VarType)
{
	check(VarType != nullptr);
	if (VarType->base_type == GLSL_TYPE_STRUCT)
	{
		uint32 NumSlots = 0;
		for (uint32 Index = 0; Index < VarType->length; ++Index)
		{
			NumSlots += GetInOutVariableSlotSize(Frequency, Mode, VarType->fields.structure[Index].type);
		}

		return NumSlots;
	}
	else if (VarType->base_type == GLSL_TYPE_ARRAY)
	{
		if (Frequency == HSF_GeometryShader && Mode == ir_var_in)
		{
			// Ignore frequency after first array specifier, so use HSF_InvalidFrequency
			return GetInOutVariableSlotSize(HSF_InvalidFrequency, Mode, VarType->fields.array);
		}
		else
		{
			// Multiply inner array type by array size, i.e. both 'vec2[4]' and 'float[4]' occupy 4 slots
			// Ignore frequency after first array specifier, so use HSF_InvalidFrequency
			return GetInOutVariableSlotSize(HSF_InvalidFrequency, Mode, VarType->fields.array) * VarType->length;
		}
	}
	else
	{
		check(VarType->matrix_columns >= 0);

		// Only use number of matrix columns to determine number of slots this variable occupies: 1 for scalars and vectors, 2/3/4 for matrices
		return VarType->matrix_columns;
	}
}

static void ConfigureInOutVariableLayout(EHlslShaderFrequency Frequency,
										 _mesa_glsl_parse_state* ParseState,
										 const char* Semantic,
										 ir_variable* Variable,
										 ir_variable_mode Mode
										 )
{
	if (Frequency == HSF_VertexShader && Mode == ir_var_in)
	{
		const int PrefixLength = 9;
		if ( (FCStringAnsi::Strnicmp(Semantic, "ATTRIBUTE", PrefixLength) == 0) &&
			(Semantic[PrefixLength] >= '0') &&	 (Semantic[PrefixLength] <= '9')
			)
		{
			int AttributeIndex = atoi(Semantic + PrefixLength);
			
			Variable->explicit_location = true;
			Variable->location = AttributeIndex;
			Variable->semantic = ralloc_strdup(Variable, Semantic);
		}
		else
		{
#ifdef DEBUG
	#define _mesh_glsl_report _mesa_glsl_warning
#else
	#define _mesh_glsl_report _mesa_glsl_error
#endif
			_mesh_glsl_report(ParseState, "Vertex shader input semantic must be ATTRIBUTE and not \'%s\' in order to determine location/semantic index", Semantic);
#undef _mesh_glsl_report
		}
	}
	else if (Semantic && FCStringAnsi::Strnicmp(Variable->name, "gl_", 3) != 0)
	{
		Variable->semantic = ralloc_strdup(Variable, Semantic);

		if(Mode == ir_var_in)
		{
			Variable->location = ParseState->next_in_location_slot;
		 	ParseState->next_in_location_slot += GetInOutVariableSlotSize(Frequency, Mode, Variable->type);
		}
		else
		{
			// Location may be already assigned to a pixel shader outputs (SV_TargetX HLSL output semantics).
			// We want to preserve explicitly assigned render target indices and auto-generate them otherwise.
			const bool bIsRenderTargetOutput = Frequency == HSF_PixelShader && Mode == ir_var_out;
			if (!bIsRenderTargetOutput || !Variable->explicit_location)
			{
				Variable->location = ParseState->next_out_location_slot;
				ParseState->next_out_location_slot += GetInOutVariableSlotSize(Frequency, Mode, Variable->type);
			}
		}

		Variable->explicit_location = true;
	}
}

/**
* Generate an input semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param Semantic - The semantic name to generate.
* @param Type - Value type.
* @param DeclInstructions - IR to which declarations may be added.
* @returns reference to IR variable for the semantic.
*/
static ir_rvalue* GenShaderInputSemantic(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* Semantic,
	FSemanticQualifier InputQualifier,
	const glsl_type* Type,
	exec_list* DeclInstructions,
	int SemanticArraySize,
	int SemanticArrayIndex,
	bool& ApplyClipSpaceAdjustment,
	bool& ApplyFlipFrontFacingAdjustment
	)
{
	if (Semantic == nullptr)
	{
		return nullptr;
	}

	if (FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
	{
		FSystemValue* SystemValues = SystemValueTable[Frequency];
		for (int i = 0; SystemValues[i].Semantic != NULL; ++i)
		{
			if (SystemValues[i].Mode == ir_var_in
				&& (!SystemValues[i].bESOnly || ParseState->bGenerateES)
				&& FCStringAnsi::Stricmp(SystemValues[i].Semantic, Semantic) == 0)
			{
				if (SystemValues[i].bArrayVariable)
				{
					// Built-in array variable. Like gl_in[x].gl_Position.
					// The variable for it has already been created in GenShaderInput().
					ir_variable* Variable = ParseState->symbols->get_variable("gl_in");
					check(Variable);
					ir_dereference_variable* ArrayDeref = new(ParseState)ir_dereference_variable(Variable);
					ir_dereference_array* StructDeref = new(ParseState)ir_dereference_array(
						ArrayDeref,
						new(ParseState)ir_constant((unsigned)SemanticArrayIndex)
						);
					ir_dereference_record* VariableDeref = new(ParseState)ir_dereference_record(
						StructDeref,
						SystemValues[i].GlslName
						);
					ApplyClipSpaceAdjustment = SystemValues[i].bApplyClipSpaceAdjustment;
					// TO DO - in case of SV_ClipDistance, we need to defer appropriate index in variable too.
					return VariableDeref;
				}
				else
				{
					// Built-in variable that shows up only once, like gl_FragCoord in fragment
					// shader, or gl_PrimitiveIDIn in geometry shader. Unlike gl_in[x].gl_Position.
					// Even in geometry shader input pass it shows up only once.

					// Create it on first pass, ignore the call on others.
					if (SemanticArrayIndex == 0)
					{
						ir_variable* Variable = new(ParseState)ir_variable(
							SystemValues[i].Type,
							SystemValues[i].GlslName,
							ir_var_in
							);
						Variable->read_only = true;
						Variable->origin_upper_left = SystemValues[i].bOriginUpperLeft;
						DeclInstructions->push_tail(Variable);
						ParseState->symbols->add_variable(Variable);
						ir_dereference_variable* VariableDeref = new(ParseState)ir_dereference_variable(Variable);

						if (FCStringAnsi::Stricmp(Semantic, "SV_Position") == 0 && Frequency == HSF_PixelShader)
						{
							// This is for input of gl_FragCoord into pixel shader only.
							
							// Generate a local variable to do the conversion in, keeping source type.
							ir_variable* TempVariable = new(ParseState)ir_variable(Variable->type, NULL, ir_var_temporary);
							DeclInstructions->push_tail(TempVariable);
							
							// Assign input to this variable
							ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(TempVariable);
							DeclInstructions->push_tail(
								new(ParseState)ir_assignment(
								TempVariableDeref,
								VariableDeref
								)
							);
							
							// TempVariable.w = ( 1.0f / TempVariable.w );
							DeclInstructions->push_tail(
								new(ParseState)ir_assignment(
								new(ParseState)ir_swizzle(TempVariableDeref->clone(ParseState, NULL), 3, 0, 0, 0, 1),
								new(ParseState)ir_expression(ir_binop_div,
								new(ParseState)ir_constant(1.0f),
								new(ParseState)ir_swizzle(TempVariableDeref->clone(ParseState, NULL), 3, 0, 0, 0, 1)
								)
								)
								);

							return TempVariableDeref->clone(ParseState, NULL);
						}
						else if (ParseState->adjust_clip_space_dx11_to_opengl && SystemValues[i].bApplyClipSpaceAdjustment)
						{
							// incoming gl_FrontFacing. Make it (!gl_FrontFacing), due to vertical flip in OpenGL
							ApplyFlipFrontFacingAdjustment = true;
							return VariableDeref;
						}
						else
						{
							return VariableDeref;
						}
					}
					else
					{
						return NULL;
					}
				}
			}
		}
	}

	ir_variable* Variable = NULL;

	// Mobile multi-view support
	if (Variable == NULL && (Frequency == HSF_VertexShader || Frequency == HSF_PixelShader))
	{
		const int PrefixLength = 9;
		if (Semantic && FCStringAnsi::Strnicmp(Semantic, "SV_ViewID", PrefixLength) == 0)
		{
			Variable = new(ParseState)ir_variable(
				Type,
				ralloc_asprintf(ParseState, "gl_ViewID_OVR"),
				ir_var_in
			);
		}
	}

	if (Variable)
	{
		// Up to this point, variables aren't contained in structs
		DeclInstructions->push_tail(Variable);
		ParseState->symbols->add_variable(Variable);
		Variable->centroid = InputQualifier.Fields.bCentroid;
		Variable->interpolation = InputQualifier.Fields.InterpolationMode;
		Variable->is_patch_constant = InputQualifier.Fields.bIsPatchConstant;
		ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);

		return VariableDeref;
	}

	// If we're here, no built-in variables matched.

	if (FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
	{
		_mesa_glsl_warning(ParseState, "unrecognized system "
			"value input '%s'", Semantic);
	}

	// Patch constants must be variables, not structs or interface blocks, in GLSL <= 4.10
	bool bUseGLSL410Rules = InputQualifier.Fields.bIsPatchConstant && ParseState->language_version <= 410;
	bool bUseESRules = ParseState->bGenerateES || ParseState->language_version == 310;

	if (Frequency == HSF_VertexShader || bUseESRules || bUseGLSL410Rules)
	{
		const char* Prefix = "in";
		if ((ParseState->bGenerateES && Frequency == HSF_PixelShader) || bUseGLSL410Rules)
		{
			Prefix = "var";
		}

		// Vertex shader inputs don't get packed into structs that we'll later morph into interface blocks
		if (ParseState->bGenerateES && Type->is_integer())
		{
			// Convert integer attributes to floats
			Variable = new(ParseState)ir_variable(
				Type,
				ralloc_asprintf(ParseState, "%s_%s_I", Prefix, Semantic),
				ir_var_temporary
				);
			Variable->centroid = InputQualifier.Fields.bCentroid;
			Variable->interpolation = InputQualifier.Fields.InterpolationMode;
			check(Type->is_vector() || Type->is_scalar());
			check(Type->base_type == GLSL_TYPE_INT || Type->base_type == GLSL_TYPE_UINT);

			// New float attribute
			ir_variable* ReplacedAttributeVar = new (ParseState)ir_variable(glsl_type::get_instance(GLSL_TYPE_FLOAT, Variable->type->vector_elements, 1), ralloc_asprintf(ParseState, "%s_%s", Prefix, Semantic), ir_var_in);
			ReplacedAttributeVar->read_only = true;
			ReplacedAttributeVar->centroid = InputQualifier.Fields.bCentroid;
			ReplacedAttributeVar->interpolation = InputQualifier.Fields.InterpolationMode;

			// Convert to integer
			ir_assignment* ConversionAssignment = new(ParseState)ir_assignment(
				new(ParseState)ir_dereference_variable(Variable),
				new(ParseState)ir_expression(
				Type->base_type == GLSL_TYPE_INT ? ir_unop_f2i : ir_unop_f2u,
				new (ParseState)ir_dereference_variable(ReplacedAttributeVar)
				)
				);

			DeclInstructions->push_tail(ReplacedAttributeVar);
			DeclInstructions->push_tail(Variable);
			DeclInstructions->push_tail(ConversionAssignment);
			ParseState->symbols->add_variable(Variable);
			ParseState->symbols->add_variable(ReplacedAttributeVar);

			ir_dereference_variable* VariableDeref = new(ParseState)ir_dereference_variable(ReplacedAttributeVar);
			return VariableDeref;
		}

		// Regular attribute
		Variable = new(ParseState)ir_variable(
			Type,
			ralloc_asprintf(ParseState, "%s_%s", Prefix, Semantic),
			ir_var_in
			);
		Variable->read_only = true;
		Variable->centroid = InputQualifier.Fields.bCentroid;
		Variable->interpolation = InputQualifier.Fields.InterpolationMode;
		Variable->is_patch_constant = InputQualifier.Fields.bIsPatchConstant;

		if(ParseState->bGenerateLayoutLocations)
		{
			ConfigureInOutVariableLayout(Frequency, ParseState, Semantic, Variable, ir_var_in);
		}

		DeclInstructions->push_tail(Variable);
		ParseState->symbols->add_variable(Variable);

		ir_dereference_variable* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
		return VariableDeref;
	}
	else if (SemanticArrayIndex == 0)
	{
		// On first pass, create variable

		glsl_struct_field *StructField = ralloc_array(ParseState, glsl_struct_field, 1);

		memset(StructField, 0, sizeof(glsl_struct_field));
		StructField[0].type = Type;
		StructField[0].name = ralloc_strdup(ParseState, "Data");

		const glsl_type* VariableType = glsl_type::get_record_instance(StructField, 1, ralloc_strdup(ParseState, Semantic));
		if (SemanticArraySize)
		{
			// Pack it into an array too
			VariableType = glsl_type::get_array_instance(VariableType, SemanticArraySize);
		}

		Variable = new(ParseState)ir_variable(VariableType, ralloc_asprintf(ParseState, "in_%s", Semantic), ir_var_in);
		Variable->read_only = true;
		Variable->is_interface_block = true;
		Variable->centroid = InputQualifier.Fields.bCentroid;
		Variable->interpolation = InputQualifier.Fields.InterpolationMode;
		Variable->is_patch_constant = InputQualifier.Fields.bIsPatchConstant;
		DeclInstructions->push_tail(Variable);
		ParseState->symbols->add_variable(Variable);

		if (ParseState->bGenerateLayoutLocations)
		{
			ConfigureInOutVariableLayout(Frequency, ParseState, Semantic, Variable, ir_var_in);
		}

		ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
		if (SemanticArraySize)
		{
			// Deref inside array first
			VariableDeref = new(ParseState)ir_dereference_array(VariableDeref, new(ParseState)ir_constant((unsigned)SemanticArrayIndex)
				);
		}
		VariableDeref = new(ParseState)ir_dereference_record(VariableDeref, ralloc_strdup(ParseState, "Data"));
		return VariableDeref;
	}
	else
	{
		// Array variable, not first pass. It already exists, get it.
		Variable = ParseState->symbols->get_variable(ralloc_asprintf(ParseState, "in_%s", Semantic));
		check(Variable);

		ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
		VariableDeref = new(ParseState)ir_dereference_array(VariableDeref, new(ParseState)ir_constant((unsigned)SemanticArrayIndex));
		VariableDeref = new(ParseState)ir_dereference_record(VariableDeref, ralloc_strdup(ParseState, "Data"));
		return VariableDeref;
	}
}

/**
* Generate an output semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param Semantic - The semantic name to generate.
* @param Type - Value type.
* @param DeclInstructions - IR to which declarations may be added.
* @returns the IR variable for the semantic.
*/
static ir_rvalue* GenShaderOutputSemantic(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* Semantic,
	FSemanticQualifier OutputQualifier,
	const glsl_type* Type,
	exec_list* DeclInstructions,
	const glsl_type** DestVariableType,
	bool& ApplyClipSpaceAdjustment,
	bool& ApplyClampPowerOfTwo
	)
{
	check(Semantic);

	FSystemValue* SystemValues = SystemValueTable[Frequency];
	ir_variable* Variable = NULL;

	if (FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
	{
		for (int i = 0; SystemValues[i].Semantic != NULL; ++i)
		{
			if (!SystemValues[i].bESOnly || ParseState->bGenerateES)
			{
				if (SystemValues[i].Mode == ir_var_out
					&& FCStringAnsi::Stricmp(SystemValues[i].Semantic, Semantic) == 0)
				{
					Variable = new(ParseState)ir_variable(
						SystemValues[i].Type,
						SystemValues[i].GlslName,
						ir_var_out
						);
					Variable->origin_upper_left = SystemValues[i].bOriginUpperLeft;
					ApplyClipSpaceAdjustment = SystemValues[i].bApplyClipSpaceAdjustment;
				}
			}
		}
	}

	if (Variable == NULL && (Frequency == HSF_VertexShader || Frequency == HSF_GeometryShader))
	{
		const int PrefixLength = 15;
		if (FCStringAnsi::Strnicmp(Semantic, "SV_ClipDistance", PrefixLength) == 0)
		{
			int OutputIndex = -1;
			if (Semantic[PrefixLength] >= '0' && Semantic[PrefixLength] <= '9')
			{
				OutputIndex = Semantic[15] - '0';
			}
			else if (Semantic[PrefixLength] == 0)
			{
				OutputIndex = 0;
			}
			if (OutputIndex != -1)
			{
				Variable = new(ParseState)ir_variable(
					glsl_type::float_type,
					ralloc_asprintf(ParseState, "gl_ClipDistance[%d]", OutputIndex),
					ir_var_out
					);
			}
		}
	}

	if (Variable == NULL && Frequency == HSF_PixelShader)
	{
		const int PrefixLength = 9;
		if (FCStringAnsi::Strnicmp(Semantic, "SV_Target", PrefixLength) == 0
			&& Semantic[PrefixLength] >= '0'
			&& Semantic[PrefixLength] <= '7')
		{
			int OutputIndex = Semantic[PrefixLength] - '0';
			Variable = new(ParseState)ir_variable(
				Type,
				ralloc_asprintf(ParseState, "out_Target%d", OutputIndex),
				ir_var_out
				);

			if (ParseState->bGenerateLayoutLocations)
			{
				Variable->explicit_location = true;
				Variable->location = OutputIndex;
			}
		}
	}

	bool bUseGLSL410Rules = OutputQualifier.Fields.bIsPatchConstant && ParseState->language_version == 410;
	bool bUseESRules = ParseState->bGenerateES || ParseState->language_version == 310;

	if (Variable == NULL && (bUseESRules || bUseGLSL410Rules))
	{
		// Create a variable so that a struct will not get added
		Variable = new(ParseState)ir_variable(Type, ralloc_asprintf(ParseState, "var_%s", Semantic), ir_var_out);
	}

	if (ParseState->bGenerateLayoutLocations && Variable)
	{
		ConfigureInOutVariableLayout(Frequency, ParseState, Semantic, Variable, ir_var_out);
	}

	if (Variable)
	{
		// Up to this point, variables aren't contained in structs
		*DestVariableType = Variable->type;
		DeclInstructions->push_tail(Variable);
		ParseState->symbols->add_variable(Variable);
		Variable->centroid = OutputQualifier.Fields.bCentroid;
		Variable->interpolation = OutputQualifier.Fields.InterpolationMode;
		Variable->is_patch_constant = OutputQualifier.Fields.bIsPatchConstant;
		ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);
		return VariableDeref;
	}

	if (Semantic && FCStringAnsi::Strnicmp(Semantic, "SV_", 3) == 0)
	{
		_mesa_glsl_warning(ParseState, "unrecognized system value output '%s'",
			Semantic);
	}

	*DestVariableType = Type;

	// Create variable
	glsl_struct_field *StructField = ralloc_array(ParseState, glsl_struct_field, 1);

	memset(StructField, 0, sizeof(glsl_struct_field));
	StructField[0].type = Type;
	StructField[0].name = ralloc_strdup(ParseState, "Data");

	const glsl_type* VariableType = glsl_type::get_record_instance(StructField, 1, ralloc_strdup(ParseState, Semantic));

	Variable = new(ParseState)ir_variable(VariableType, ralloc_asprintf(ParseState, "out_%s", Semantic), ir_var_out);

	Variable->centroid = OutputQualifier.Fields.bCentroid;
	Variable->interpolation = OutputQualifier.Fields.InterpolationMode;
	Variable->is_interface_block = true;
	Variable->is_patch_constant = OutputQualifier.Fields.bIsPatchConstant;

	if (ParseState->bGenerateLayoutLocations)
	{
		ConfigureInOutVariableLayout(Frequency, ParseState, Semantic, Variable, ir_var_out);
	}

	DeclInstructions->push_tail(Variable);
	ParseState->symbols->add_variable(Variable);

	ir_rvalue* VariableDeref = new(ParseState)ir_dereference_variable(Variable);

	VariableDeref = new(ParseState)ir_dereference_record(VariableDeref, ralloc_strdup(ParseState, "Data"));

	return VariableDeref;
}

/**
* Generate an input semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param InputSemantic - The semantic name to generate.
* @param InputQualifier - Qualifiers applied to the semantic.
* @param InputVariableDeref - Deref for the argument variable.
* @param DeclInstructions - IR to which declarations may be added.
* @param PreCallInstructions - IR to which instructions may be added before the
*                              entry point is called.
*/
static void GenShaderInputForVariable(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* InputSemantic,
	FSemanticQualifier InputQualifier,
	ir_dereference* InputVariableDeref,
	exec_list* DeclInstructions,
	exec_list* PreCallInstructions,
	int SemanticArraySize,
	int SemanticArrayIndex
	)
{
	const glsl_type* InputType = InputVariableDeref->type;

	if (InputType->is_record())
	{
		for (uint32 i = 0; i < InputType->length; ++i)
		{
			const char* FieldSemantic = InputType->fields.structure[i].semantic;
			const char* Semantic = 0;

			if (InputSemantic && FieldSemantic)
			{

				_mesa_glsl_warning(ParseState, "semantic '%s' of field '%s' will be overridden by enclosing types' semantic '%s'",
					InputType->fields.structure[i].semantic,
					InputType->fields.structure[i].name,
					InputSemantic);


				FieldSemantic = 0;
			}

			if (InputSemantic && !FieldSemantic)
			{
				Semantic = ralloc_asprintf(ParseState, "%s%u", InputSemantic, i);
				_mesa_glsl_warning(ParseState, "  creating semantic '%s' for struct field '%s'", Semantic, InputType->fields.structure[i].name);
			}
			else if (!InputSemantic && FieldSemantic)
			{
				Semantic = FieldSemantic;
			}
			else
			{
				Semantic = 0;
			}

			if (InputType->fields.structure[i].type->is_record() ||
				Semantic)
			{
				FSemanticQualifier Qualifier = InputQualifier;
				if (Qualifier.Packed == 0)
				{
					Qualifier.Fields.bCentroid = InputType->fields.structure[i].centroid;
					Qualifier.Fields.InterpolationMode = InputType->fields.structure[i].interpolation;
					Qualifier.Fields.bIsPatchConstant = InputType->fields.structure[i].patchconstant;
				}

				ir_dereference_record* FieldDeref = new(ParseState)ir_dereference_record(
					InputVariableDeref->clone(ParseState, NULL),
					InputType->fields.structure[i].name);
				GenShaderInputForVariable(
					Frequency,
					ParseState,
					Semantic,
					Qualifier,
					FieldDeref,
					DeclInstructions,
					PreCallInstructions,
					SemanticArraySize,
					SemanticArrayIndex
					);
			}
			else
			{
				_mesa_glsl_error(
					ParseState,
					"field '%s' in input structure '%s' does not specify a semantic",
					InputType->fields.structure[i].name,
					InputType->name
					);
			}
		}
	}
	else if (InputType->is_array() || InputType->is_inputpatch() || InputType->is_outputpatch())
	{
		int BaseIndex = 0;
		const char* Semantic = 0;
		check(InputSemantic);
		ParseSemanticAndIndex(ParseState, InputSemantic, &Semantic, &BaseIndex);
		check(BaseIndex >= 0);
		check(InputType->is_array() || InputType->is_inputpatch() || InputType->is_outputpatch());
		const unsigned ElementCount = InputType->is_array() ? InputType->length : InputType->patch_length;

		{
			//check(!InputQualifier.Fields.bIsPatchConstant);
			InputQualifier.Fields.bIsPatchConstant = false;
		}

		for (unsigned i = 0; i < ElementCount; ++i)
		{
			ir_dereference_array* ArrayDeref = new(ParseState)ir_dereference_array(
				InputVariableDeref->clone(ParseState, NULL),
				new(ParseState)ir_constant((unsigned)i)
				);
			GenShaderInputForVariable(
				Frequency,
				ParseState,
				ralloc_asprintf(ParseState, "%s%u", Semantic, BaseIndex + i),
				InputQualifier,
				ArrayDeref,
				DeclInstructions,
				PreCallInstructions,
				SemanticArraySize,
				SemanticArrayIndex
				);
		}
	}
	else
	{
		bool ApplyFlipFrontFacingAdjustment = false;
		bool ApplyClipSpaceAdjustment = false;
		ir_rvalue* SrcValue = GenShaderInputSemantic(Frequency, ParseState, InputSemantic,
			InputQualifier, InputType, DeclInstructions, SemanticArraySize,
			SemanticArrayIndex, ApplyClipSpaceAdjustment, ApplyFlipFrontFacingAdjustment);

		if (SrcValue)
		{
			YYLTYPE loc;

			if (ParseState->adjust_clip_space_dx11_to_opengl && ApplyClipSpaceAdjustment)
			{
				// This is for input of gl_Position into geometry shader only.

				// Generate a local variable to do the conversion in, keeping source type.
				ir_variable* TempVariable = new(ParseState)ir_variable(SrcValue->type, NULL, ir_var_temporary);
				PreCallInstructions->push_tail(TempVariable);

				// Assign input to this variable
				ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(TempVariable);
				PreCallInstructions->push_tail(
					new(ParseState)ir_assignment(
					TempVariableDeref,
					SrcValue
					)
					);

				ir_function *adjustFunc = ParseState->symbols->get_function("compiler_internal_AdjustInputSemantic");
				check(adjustFunc);
				check(adjustFunc->signatures.get_head() == adjustFunc->signatures.get_tail());
				ir_function_signature *adjustFuncSig = (ir_function_signature *)adjustFunc->signatures.get_head();
				exec_list actual_parameter;
				actual_parameter.push_tail(TempVariableDeref->clone(ParseState, NULL));
				ir_call* adjustFuncCall = new(ParseState) ir_call(adjustFuncSig, NULL, &actual_parameter);
				PreCallInstructions->push_tail(adjustFuncCall);

				SrcValue = TempVariableDeref->clone(ParseState, NULL);
			}
			else if (ParseState->adjust_clip_space_dx11_to_opengl && ApplyFlipFrontFacingAdjustment)
			{
				// Generate a local variable to do the conversion in, keeping source type.
				ir_variable* TempVariable = new(ParseState)ir_variable(SrcValue->type, NULL, ir_var_temporary);
				PreCallInstructions->push_tail(TempVariable);

				ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(TempVariable);

				// incoming gl_FrontFacing. Make it (!gl_FrontFacing), due to vertical flip in OpenGL
				ir_function *adjustFunc = ParseState->symbols->get_function("compiler_internal_AdjustIsFrontFacing");
				check(adjustFunc);
				check(adjustFunc->signatures.get_head() == adjustFunc->signatures.get_tail());
				ir_function_signature *adjustFuncSig = (ir_function_signature *)adjustFunc->signatures.get_head();
				exec_list actual_parameter;
				actual_parameter.push_tail(SrcValue);
				ir_call* adjustFuncCall = new(ParseState) ir_call(adjustFuncSig, TempVariableDeref, &actual_parameter);
				PreCallInstructions->push_tail(adjustFuncCall);

				check(adjustFuncCall->return_deref);
				SrcValue = adjustFuncCall->return_deref->clone(ParseState, NULL);
			}

			apply_type_conversion(InputType, SrcValue, PreCallInstructions, ParseState, true, &loc);
			PreCallInstructions->push_tail(
				new(ParseState)ir_assignment(
				InputVariableDeref->clone(ParseState, NULL),
				SrcValue
				)
				);
		}
	}
}


/**
* Generate a shader input.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param InputSemantic - The semantic name to generate.
* @param InputQualifier - Qualifiers applied to the semantic.
* @param InputType - Value type.
* @param DeclInstructions - IR to which declarations may be added.
* @param PreCallInstructions - IR to which instructions may be added before the
*                              entry point is called.
* @returns the IR variable deref for the semantic.
*/
static ir_dereference_variable* GenShaderInput(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* InputSemantic,
	FSemanticQualifier InputQualifier,
	const glsl_type* InputType,
	exec_list* DeclInstructions,
	exec_list* PreCallInstructions)
{
	ir_variable* TempVariable = new(ParseState)ir_variable(
		InputType,
		NULL,
		ir_var_temporary);
	ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(TempVariable);
	PreCallInstructions->push_tail(TempVariable);

	//check ( InputSemantic ?  (FCStringAnsi::Strnicmp(InputSemantic, "SV_", 3) ==0) : true);


	// everything that's not an Outputpatch is patch constant. System values are treated specially
	if (Frequency == HSF_GeometryShader && TempVariableDeref->type->is_array())
	{
		check(InputType->is_array() || InputType->is_inputpatch() || InputType->is_outputpatch());
		check(InputType->length || InputType->patch_length);


		const unsigned ElementCount = InputType->is_array() ? InputType->length : InputType->patch_length;

		if (!ParseState->symbols->get_variable("gl_in"))
		{
			// Create a built-in OpenGL variable gl_in[] containing built-in types.
			// This variable will be used for OpenGL optimization by IR, so IR must know about it,
			// but will not end up in final GLSL code.

			// It has to be created here, as it contains multiple built-in variables in one interface block,
			// which is not usual, so avoiding special cases in code.

			glsl_struct_field *BuiltinFields = ralloc_array(ParseState, glsl_struct_field, 3);
			memset(BuiltinFields, 0, 3 * sizeof(glsl_struct_field));

			BuiltinFields[0].type = glsl_type::vec4_type;
			BuiltinFields[0].name = ralloc_strdup(ParseState, "gl_Position");
			BuiltinFields[1].type = glsl_type::float_type;
			BuiltinFields[1].name = ralloc_strdup(ParseState, "gl_PointSize");
			BuiltinFields[2].type = glsl_type::get_array_instance(glsl_type::float_type, 6);	// magic number is gl_MaxClipDistances
			BuiltinFields[2].name = ralloc_strdup(ParseState, "gl_ClipDistance");

			const glsl_type* BuiltinStruct = glsl_type::get_record_instance(BuiltinFields, 3, "gl_PerVertex");
			const glsl_type* BuiltinArray = glsl_type::get_array_instance(BuiltinStruct, ElementCount);
			ir_variable* BuiltinVariable = new(ParseState)ir_variable(BuiltinArray, "gl_in", ir_var_in);
			BuiltinVariable->read_only = true;
			BuiltinVariable->is_interface_block = true;
			DeclInstructions->push_tail(BuiltinVariable);
			ParseState->symbols->add_variable(BuiltinVariable);
		}

		for (unsigned i = 0; i < ElementCount; ++i)
		{
			ir_dereference_array* ArrayDeref = new(ParseState)ir_dereference_array(
				TempVariableDeref->clone(ParseState, NULL),
				new(ParseState)ir_constant((unsigned)i)
				);
			// Parse input variable
			GenShaderInputForVariable(
				Frequency,
				ParseState,
				InputSemantic,
				InputQualifier,
				ArrayDeref,
				DeclInstructions,
				PreCallInstructions,
				ElementCount,
				i
				);
		}
	}
	else
	{
		GenShaderInputForVariable(
			Frequency,
			ParseState,
			InputSemantic,
			InputQualifier,
			TempVariableDeref,
			DeclInstructions,
			PreCallInstructions,
			0,
			0
			);
	}
	return TempVariableDeref;
}

/**
* Generate an output semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param OutputSemantic - The semantic name to generate.
* @param OutputQualifier - Qualifiers applied to the semantic.
* @param OutputVariableDeref - Deref for the argument variable.
* @param DeclInstructions - IR to which declarations may be added.
* @param PostCallInstructions - IR to which instructions may be added after the
*                               entry point returns.
*/
void GenShaderOutputForVariable(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* OutputSemantic,
	FSemanticQualifier OutputQualifier,
	ir_dereference* OutputVariableDeref,
	exec_list* DeclInstructions,
	exec_list* PostCallInstructions
	)
{
	const glsl_type* OutputType = OutputVariableDeref->type;
	if (OutputType->is_record())
	{
		for (uint32 i = 0; i < OutputType->length; ++i)
		{
			const char* FieldSemantic = OutputType->fields.structure[i].semantic;
			const char* Semantic = 0;

			if (OutputSemantic && FieldSemantic)
			{

				_mesa_glsl_warning(ParseState, "semantic '%s' of field '%s' will be overridden by enclosing types' semantic '%s'",
					OutputType->fields.structure[i].semantic,
					OutputType->fields.structure[i].name,
					OutputSemantic);


				FieldSemantic = 0;
			}

			if (OutputSemantic && !FieldSemantic)
			{
				Semantic = ralloc_asprintf(ParseState, "%s%u", OutputSemantic, i);
				_mesa_glsl_warning(ParseState, "  creating semantic '%s' for struct field '%s'", Semantic, OutputType->fields.structure[i].name);
			}
			else if (!OutputSemantic && FieldSemantic)
			{
				Semantic = FieldSemantic;
			}
			else
			{
				Semantic = 0;
			}

			if (OutputType->fields.structure[i].type->is_record() ||
				Semantic
				)
			{
				FSemanticQualifier Qualifier = OutputQualifier;
				if (Qualifier.Packed == 0)
				{
					Qualifier.Fields.bCentroid = OutputType->fields.structure[i].centroid;
					Qualifier.Fields.InterpolationMode = OutputType->fields.structure[i].interpolation;
					Qualifier.Fields.bIsPatchConstant = OutputType->fields.structure[i].patchconstant;
				}

				// Dereference the field and generate shader outputs for the field.
				ir_dereference* FieldDeref = new(ParseState)ir_dereference_record(
					OutputVariableDeref->clone(ParseState, NULL),
					OutputType->fields.structure[i].name);
				GenShaderOutputForVariable(
					Frequency,
					ParseState,
					Semantic,
					Qualifier,
					FieldDeref,
					DeclInstructions,
					PostCallInstructions
					);
			}
			else
			{
				_mesa_glsl_error(
					ParseState,
					"field '%s' in output structure '%s' does not specify a semantic",
					OutputType->fields.structure[i].name,
					OutputType->name
					);
			}
		}
	}
	// TODO clean this up!!
	else if ((OutputType->is_array() || OutputType->is_outputpatch()))
	{
		if (OutputSemantic)
		{
			int BaseIndex = 0;
			const char* Semantic = 0;

			ParseSemanticAndIndex(ParseState, OutputSemantic, &Semantic, &BaseIndex);

			const unsigned ElementCount = OutputType->is_array() ? OutputType->length : (OutputType->patch_length);

			for (unsigned i = 0; i < ElementCount; ++i)
			{
				ir_dereference_array* ArrayDeref = new(ParseState)ir_dereference_array(
					OutputVariableDeref->clone(ParseState, NULL),
					new(ParseState)ir_constant((unsigned)i)
					);
				GenShaderOutputForVariable(
					Frequency,
					ParseState,
					ralloc_asprintf(ParseState, "%s%u", Semantic, BaseIndex + i),
					OutputQualifier,
					ArrayDeref,
					DeclInstructions,
					PostCallInstructions
					);
			}
		}
		else
		{
			_mesa_glsl_error(ParseState, "entry point does not specify a semantic for its return value");
		}
	}
	else
	{
		if (OutputSemantic)
		{
			YYLTYPE loc;
			ir_rvalue* Src = OutputVariableDeref->clone(ParseState, NULL);
			const glsl_type* DestVariableType = NULL;
			bool ApplyClipSpaceAdjustment = false;
			bool ApplyClampPowerOfTwo = false;
			ir_rvalue* DestVariableDeref = GenShaderOutputSemantic(Frequency, ParseState, OutputSemantic,
				OutputQualifier, OutputType, DeclInstructions, &DestVariableType, ApplyClipSpaceAdjustment, ApplyClampPowerOfTwo);

			apply_type_conversion(DestVariableType, Src, PostCallInstructions, ParseState, true, &loc);

			if (ParseState->adjust_clip_space_dx11_to_opengl && ApplyClipSpaceAdjustment)
			{
				ir_function *adjustFunc = ParseState->symbols->get_function("compiler_internal_AdjustOutputSemantic");
				check(adjustFunc);
				check(adjustFunc->signatures.get_head() == adjustFunc->signatures.get_tail());
				ir_function_signature *adjustFuncSig = (ir_function_signature *)adjustFunc->signatures.get_head();
				exec_list actual_parameter;
				actual_parameter.push_tail(Src->clone(ParseState, NULL));
				ir_call* adjustFuncCall = new(ParseState) ir_call(adjustFuncSig, NULL, &actual_parameter);
				PostCallInstructions->push_tail(adjustFuncCall);
			}

			// GLSL doesn't support pow2 partitioning, so we treate pow2 as integer partitioning and
			// manually compute the next power of two via exp2(pow(ceil(log2(Src)));
			if (ApplyClampPowerOfTwo)
			{
				ir_variable* temp = new(ParseState)ir_variable(glsl_type::float_type, NULL, ir_var_temporary);

				PostCallInstructions->push_tail(temp);

				PostCallInstructions->push_tail(
					new(ParseState)ir_assignment(
					new(ParseState)ir_dereference_variable(temp),
					new(ParseState)ir_expression(ir_unop_exp2,
					new(ParseState)ir_expression(ir_unop_ceil,
					new(ParseState)ir_expression(ir_unop_log2,
					glsl_type::float_type,
					Src->clone(ParseState, NULL),
					NULL
					)
					)
					)
					)
					);

				// assign pow2 clamped variable to output variable
				PostCallInstructions->push_tail(
					new(ParseState)ir_assignment(
					DestVariableDeref->clone(ParseState, NULL),
					new(ParseState)ir_dereference_variable(temp)
					)
					);
			}
			else
			{
				PostCallInstructions->push_tail(new(ParseState)ir_assignment(DestVariableDeref, Src));
			}
		}
		else
		{
			_mesa_glsl_error(ParseState, "entry point does not specify a semantic for its return value");
		}
	}
}
/**
* Generate an output semantic.
* @param Frequency - The shader frequency.
* @param ParseState - Parse state.
* @param OutputSemantic - The semantic name to generate.
* @param OutputQualifier - Qualifiers applied to the semantic.
* @param OutputType - Value type.
* @param DeclInstructions - IR to which declarations may be added.
* @param PreCallInstructions - IR to which isntructions may be added before the
entry point is called.
* @param PostCallInstructions - IR to which instructions may be added after the
*                               entry point returns.
* @returns the IR variable deref for the semantic.
*/
static ir_dereference_variable* GenShaderOutput(
	EHlslShaderFrequency Frequency,
	_mesa_glsl_parse_state* ParseState,
	const char* OutputSemantic,
	FSemanticQualifier OutputQualifier,
	const glsl_type* OutputType,
	exec_list* DeclInstructions,
	exec_list* PreCallInstructions,
	exec_list* PostCallInstructions
	)
{
	// Generate a local variable to hold the output.
	ir_variable* TempVariable = new(ParseState)ir_variable(
		OutputType,
		NULL,
		ir_var_temporary);
	ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(TempVariable);
	PreCallInstructions->push_tail(TempVariable);
	GenShaderOutputForVariable(
		Frequency,
		ParseState,
		OutputSemantic,
		OutputQualifier,
		TempVariableDeref,
		DeclInstructions,
		PostCallInstructions
		);
	return TempVariableDeref;
}

static void GenerateAppendFunctionBody(
	_mesa_glsl_parse_state* ParseState,
	exec_list* DeclInstructions,
	const glsl_type* geometry_append_type
	)
{
	ir_function *func = ParseState->symbols->get_function("OutputStream_Append");
	check(func);

	exec_list comparison_parameter;
	ir_variable* var = new(ParseState)ir_variable(geometry_append_type, ralloc_asprintf(ParseState, "arg0"), ir_var_in);
	comparison_parameter.push_tail(var);

	bool is_exact = false;
	ir_function_signature *sig = func->matching_signature(&comparison_parameter, &is_exact);
	check(sig && is_exact);
	var = (ir_variable*)sig->parameters.get_head();

	//	{
	//		const glsl_type* output_type = var->type;
	//		_mesa_glsl_warning(ParseState, "GenerateAppendFunctionBody: parsing argument struct '%s'", output_type->name );
	//		int indexof_RenderTargetArrayIndex = -1;
	//		for (int i = 0; i < output_type->length; i++)
	//		{
	//			_mesa_glsl_warning(ParseState, "   name '%s' : semantic '%s'", output_type->fields.structure[i].name, output_type->fields.structure[i].semantic );
	//		}
	//	}

	// Generate assignment instructions from function argument to out variables
	FSemanticQualifier OutputQualifier;
	ir_dereference_variable* TempVariableDeref = new(ParseState)ir_dereference_variable(var);
	GenShaderOutputForVariable(
		HSF_GeometryShader,
		ParseState,
		NULL,
		OutputQualifier,
		TempVariableDeref,
		DeclInstructions,
		&sig->body
		);

	// If the output structure type contains a SV_RenderTargetArrayIndex semantic, add a custom user output semantic.
	// It's used to pass layer index to pixel shader, as GLSL 1.50 doesn't allow pixel shader to read from gl_Layer.
	const glsl_type* output_type = var->type;
	int indexof_RenderTargetArrayIndex = -1;
	for (uint32 i = 0; i < output_type->length; i++)
	{
		if (output_type->fields.structure[i].semantic && (strcmp(output_type->fields.structure[i].semantic, "SV_RenderTargetArrayIndex") == 0))
		{
			indexof_RenderTargetArrayIndex = i;
			break;
		}
	}

	if (indexof_RenderTargetArrayIndex != -1)
	{
		// Add the new member with semantic
		glsl_struct_field field;
		field.type = output_type->fields.structure[indexof_RenderTargetArrayIndex].type;
		field.name = "HLSLCCLayerIndex";
		field.semantic = CUSTOM_LAYER_INDEX_SEMANTIC;
		field.centroid = 0;
		field.interpolation = ir_interp_qualifier_flat;
		field.geometryinput = 0;
		field.patchconstant = 0;

		glsl_type* non_const_type = (glsl_type*)output_type;
		non_const_type->add_structure_member(&field);

		// Create new out variable for the new member and generate assignment that will copy input's layer index field to it
		FSemanticQualifier Qualifier;
		Qualifier.Fields.bCentroid = 0;
		Qualifier.Fields.InterpolationMode = ir_interp_qualifier_flat;

		const glsl_type* new_output_type = ((ir_variable*)sig->parameters.get_head())->type;
		GenShaderOutputForVariable(
			HSF_GeometryShader,
			ParseState,
			CUSTOM_LAYER_INDEX_SEMANTIC,
			Qualifier,
			new(ParseState)ir_dereference_record(var, new_output_type->fields.structure[indexof_RenderTargetArrayIndex].name),
			DeclInstructions,
			&sig->body
			);
	}

	// Call EmitVertex()
	ir_function *emitVertexFunc = ParseState->symbols->get_function("EmitVertex");
	check(emitVertexFunc);
	check(emitVertexFunc->signatures.get_head() == emitVertexFunc->signatures.get_tail());
	ir_function_signature *emitVertexSig = (ir_function_signature *)emitVertexFunc->signatures.get_head();
	exec_list actual_parameter;
	sig->body.push_tail(new(ParseState)ir_call(emitVertexSig, NULL, &actual_parameter));
}

bool FGlslCodeBackend::GenerateMain(
	EHlslShaderFrequency Frequency,
	const char* EntryPoint,
	exec_list* Instructions,
	_mesa_glsl_parse_state* ParseState)
{
	{
		// Set up origin_upper_left for gl_FragCoord, depending on HLSLCC_DX11ClipSpace flag presence.
		FSystemValue* SystemValues = SystemValueTable[HSF_PixelShader];
		for (int i = 0; SystemValues[i].Semantic != NULL; ++i)
		{
			if (FCStringAnsi::Stricmp(SystemValues[i].GlslName, "gl_FragCoord") == 0)
			{
				SystemValues[i].bOriginUpperLeft = false;
				break;
			}
		}
	}

	ir_function_signature* EntryPointSig = FindEntryPointFunction(Instructions, ParseState, EntryPoint);
	if (EntryPointSig)
	{
		void* TempMemContext = ralloc_context(NULL);
		exec_list DeclInstructions;
		exec_list PreCallInstructions;
		exec_list ArgInstructions;
		exec_list PostCallInstructions;
		const glsl_type* geometry_append_type = NULL;

		ParseState->maxvertexcount = EntryPointSig->maxvertexcount;

		ParseState->tessellation = EntryPointSig->tessellation;

		ParseState->symbols->push_scope();

		foreach_iter(exec_list_iterator, Iter, EntryPointSig->parameters)
		{
			ir_variable *const Variable = (ir_variable *)Iter.get();
			if (Variable->semantic != NULL || Variable->type->is_record()
				|| (Frequency == HSF_GeometryShader && (Variable->type->is_outputstream() || Variable->type->is_array()))
				)
			{
				FSemanticQualifier Qualifier;
				Qualifier.Fields.bCentroid = Variable->centroid;
				Variable->centroid = 0;
				Qualifier.Fields.InterpolationMode = Variable->interpolation;
				Variable->interpolation = 0;
				Qualifier.Fields.bIsPatchConstant = Variable->is_patch_constant;
				Variable->is_patch_constant = 0;

				ir_dereference_variable* ArgVarDeref = NULL;
				switch (Variable->mode)
				{
				case ir_var_in:
					if (Frequency == HSF_GeometryShader && Variable->type->is_array())
					{
						// Remember information about geometry input type globally
						ParseState->geometryinput = Variable->geometryinput;
					}

					if (Frequency == HSF_PixelShader)
					{
						// Replace SV_RenderTargetArrayIndex in
						// input structure semantic with custom semantic.
						if (Variable->semantic && (strcmp(Variable->semantic, "SV_RenderTargetArrayIndex") == 0))
						{
							//							_mesa_glsl_warning(ParseState, "Replacing semantic of variable '%s' with our custom one", Variable->name);
							Variable->semantic = ralloc_strdup(Variable, CUSTOM_LAYER_INDEX_SEMANTIC);
							Variable->interpolation = ir_interp_qualifier_flat;
						}
						else if (Variable->type->is_record())
						{
							const glsl_type* output_type = Variable->type;
							int indexof_RenderTargetArrayIndex = -1;
							for (uint32 i = 0; i < output_type->length; i++)
							{
								if (Variable->type->fields.structure[i].semantic && (strcmp(Variable->type->fields.structure[i].semantic, "SV_RenderTargetArrayIndex") == 0))
								{
									indexof_RenderTargetArrayIndex = i;
									break;
								}
							}

							if (indexof_RenderTargetArrayIndex != -1)
							{
								// _mesa_glsl_warning(ParseState, "Replacing semantic of member %d of variable '%s' with our custom one", indexof_RenderTargetArrayIndex, Variable->name);
								// Replace the member with one with semantic
								glsl_struct_field field;
								field.type = Variable->type->fields.structure[indexof_RenderTargetArrayIndex].type;
								field.name = Variable->type->fields.structure[indexof_RenderTargetArrayIndex].name;
								field.semantic = CUSTOM_LAYER_INDEX_SEMANTIC;
								field.centroid = 0;
								field.interpolation = ir_interp_qualifier_flat;
								field.geometryinput = 0;
								field.patchconstant = 0;

								glsl_type* non_const_type = (glsl_type*)output_type;
								non_const_type->replace_structure_member(indexof_RenderTargetArrayIndex, &field);
							}
						}
					}

					ArgVarDeref = GenShaderInput(
						Frequency,
						ParseState,
						Variable->semantic,
						Qualifier,
						Variable->type,
						&DeclInstructions,
						&PreCallInstructions
						);
					break;
				case ir_var_out:
					if (Frequency == HSF_PixelShader && Variable->semantic && (strcmp(Variable->semantic, "SV_Depth") == 0))
					{
						bExplicitDepthWrites = true;
					}

					ArgVarDeref = GenShaderOutput(
						Frequency,
						ParseState,
						Variable->semantic,
						Qualifier,
						Variable->type,
						&DeclInstructions,
						&PreCallInstructions,
						&PostCallInstructions
						);
					break;
				case ir_var_inout:
				{
					check(Frequency == HSF_GeometryShader);
					// This is an output stream for geometry shader. It's not referenced as a variable inside the function,
					// instead OutputStream.Append(vertex) and OutputStream.RestartStrip() are called, and this variable
					// has already been optimized out of them in ast_to_hir translation.

					// Generate a local variable to add to arguments. It won't be referenced anywhere, so it should get optimized out.
					ir_variable* TempVariable = new(ParseState)ir_variable(
						Variable->type,
						NULL,
						ir_var_temporary);
					ArgVarDeref = new(ParseState)ir_dereference_variable(TempVariable);
					PreCallInstructions.push_tail(TempVariable);

					// We need to move this information somewhere safer, as this pseudo-variable will get optimized out of existence
					ParseState->outputstream_type = Variable->type->outputstream_type;

					check(Variable->type->is_outputstream());
					check(Variable->type->inner_type->is_record());

					geometry_append_type = Variable->type->inner_type;
				}
					break;
				default:
				{
					_mesa_glsl_error(
						ParseState,
						"entry point parameter '%s' must be an input or output",
						Variable->name
						);
				}
					break;
				}
				ArgInstructions.push_tail(ArgVarDeref);
			}
			else
			{
				_mesa_glsl_error(ParseState, "entry point parameter "
					"'%s' does not specify a semantic", Variable->name);
			}
		}

		// The function's return value should have an output semantic if it's not void.
		ir_dereference_variable* EntryPointReturn = NULL;
		if (EntryPointSig->return_type->is_void() == false)
		{
			FSemanticQualifier Qualifier;
			EntryPointReturn = GenShaderOutput(
				Frequency,
				ParseState,
				EntryPointSig->return_semantic,
				Qualifier,
				EntryPointSig->return_type,
				&DeclInstructions,
				&PreCallInstructions,
				&PostCallInstructions
				);
		}

		if (Frequency == HSF_GeometryShader)
		{
			GenerateAppendFunctionBody(
				ParseState,
				&DeclInstructions,
				geometry_append_type
				);
		}

		ParseState->symbols->pop_scope();

		// Build the void main() function for GLSL.
		ir_function_signature* MainSig = new(ParseState)ir_function_signature(glsl_type::void_type);
		MainSig->is_defined = true;
		MainSig->is_main = true;
		MainSig->body.append_list(&PreCallInstructions);
		MainSig->body.push_tail(new(ParseState)ir_call(EntryPointSig, EntryPointReturn, &ArgInstructions));
		MainSig->body.append_list(&PostCallInstructions);
		MainSig->maxvertexcount = EntryPointSig->maxvertexcount;
		MainSig->is_early_depth_stencil = (EntryPointSig->is_early_depth_stencil && !bExplicitDepthWrites);
		MainSig->wg_size_x = EntryPointSig->wg_size_x;
		MainSig->wg_size_y = EntryPointSig->wg_size_y;
		MainSig->wg_size_z = EntryPointSig->wg_size_z;
		MainSig->tessellation = EntryPointSig->tessellation;

		if (MainSig->is_early_depth_stencil && Frequency != HSF_PixelShader)
		{
			_mesa_glsl_error(ParseState, "'earlydepthstencil' attribute only applies to pixel shaders");
		}

		if (MainSig->maxvertexcount > 0 && Frequency != HSF_GeometryShader)
		{
			_mesa_glsl_error(ParseState, "'maxvertexcount' attribute only applies to geometry shaders");
		}

		if (MainSig->is_early_depth_stencil && ParseState->language_version < 310)
		{
			_mesa_glsl_error(ParseState, "'earlydepthstencil' attribute only supported on GLSL 4.30 target and later");
		}

		if (MainSig->wg_size_x > 0 && Frequency != HSF_ComputeShader)
		{
			_mesa_glsl_error(ParseState, "'num_threads' attribute only applies to compute shaders");
		}

		// in GLSL, unlike in HLSL fixed-function tessellator properties are specified on the domain shader
		// and not the hull shader, so we specify them for both in the .usf shaders and then print a warning,
		// similar to what fxc is doing

		if (MainSig->tessellation.domain != GLSL_DOMAIN_NONE)
		{
			_mesa_glsl_warning(ParseState, "'domain' attribute only applies to hull or domain shaders");
		}

		if (MainSig->tessellation.outputtopology != GLSL_OUTPUTTOPOLOGY_NONE)
		{
			_mesa_glsl_warning(ParseState, "'outputtopology' attribute only applies to hull shaders");
		}

		if (MainSig->tessellation.partitioning != GLSL_PARTITIONING_NONE)
		{
			_mesa_glsl_warning(ParseState, "'partitioning' attribute only applies to hull shaders");
		}

		if (MainSig->tessellation.outputcontrolpoints > 0)
		{
			_mesa_glsl_warning(ParseState, "'outputcontrolpoints' attribute only applies to hull shaders");
		}

		if (MainSig->tessellation.maxtessfactor > 0.0f)
		{
			_mesa_glsl_warning(ParseState, "'maxtessfactor' attribute only applies to hull shaders");
		}

		if (MainSig->tessellation.patchconstantfunc != 0)
		{
			_mesa_glsl_warning(ParseState, "'patchconstantfunc' attribute only applies to hull shaders");
		}

		ir_function* MainFunction = new(ParseState)ir_function("main");
		MainFunction->add_signature(MainSig);

		Instructions->append_list(&DeclInstructions);
		Instructions->push_tail(MainFunction);

		ralloc_free(TempMemContext);

		// Now that we have a proper main(), move global setup to main().
		MoveGlobalInstructionsToMain(Instructions);
	}
	else
	{
		_mesa_glsl_error(ParseState, "shader entry point '%s' not "
			"found", EntryPoint);
	}

	return true;
}
ir_function_signature*  FGlslCodeBackend::FindPatchConstantFunction(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
	ir_function_signature* PatchConstantSig = 0;

	// TODO refactor this and the fetching of the main siganture
	foreach_iter(exec_list_iterator, Iter, *Instructions)
	{
		ir_instruction *ir = (ir_instruction *)Iter.get();
		ir_function *Function = ir->as_function();
		if (Function && strcmp(Function->name, ParseState->tessellation.patchconstantfunc) == 0)
		{
			int NumSigs = 0;
			foreach_iter(exec_list_iterator, SigIter, *Function)
			{
				if (++NumSigs == 1)
				{
					PatchConstantSig = (ir_function_signature *)SigIter.get();
				}
			}
			if (NumSigs == 1)
			{
				break;
			}
			else
			{
				_mesa_glsl_error(ParseState, "patch constant function "
					"`%s' has multiple signatures", ParseState->tessellation.patchconstantfunc);
			}
		}
	}

	return PatchConstantSig;
}


void FGlslLanguageSpec::SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir)
{
	unsigned IntrinsicReturnType = bDefaultPrecisionIsHalf ? IR_INTRINSIC_ALL_FLOATING : IR_INTRINSIC_FLOAT;
	make_intrinsic_genType(ir, State, FRAMEBUFFER_FETCH_ES2, ir_invalid_opcode, IntrinsicReturnType, 0, 4, 4);
	make_intrinsic_genType(ir, State, DEPTHBUFFER_FETCH_ES2, ir_invalid_opcode, IR_INTRINSIC_FLOAT, 0, 1, 1);

	{
		ir_function* func = new(State)ir_function("compiler_internal_AdjustInputSemantic");
		ir_variable* param = new (State) ir_variable(glsl_type::vec4_type, "TempVariable", ir_variable_mode::ir_var_inout);

		exec_list* params = new(State) exec_list();
		params->push_tail(param);

		ir_function_signature* sig = new(State)ir_function_signature(glsl_type::void_type);
		sig->replace_parameters(params);
		sig->is_builtin = true;
		sig->is_defined = false;
		sig->has_output_parameters = true;

		func->add_signature(sig);
		State->symbols->add_global_function(func);
	}

	{
		ir_function* func = new(State)ir_function("compiler_internal_AdjustOutputSemantic");
		ir_variable* param = new (State) ir_variable(glsl_type::vec4_type, "Src", ir_variable_mode::ir_var_inout);

		exec_list* params = new(State) exec_list();
		params->push_tail(param);

		ir_function_signature* sig = new(State)ir_function_signature(glsl_type::void_type);
		sig->replace_parameters(params);
		sig->is_builtin = true;
		sig->is_defined = false;
		sig->has_output_parameters = true;

		func->add_signature(sig);
		State->symbols->add_global_function(func);
	}

	{
		ir_function* func = new(State)ir_function("compiler_internal_AdjustIsFrontFacing");
		ir_variable* param = new (State) ir_variable(glsl_type::bool_type, "isFrontFacing", ir_variable_mode::ir_var_in);

		exec_list* params = new(State) exec_list();
		params->push_tail(param);

		ir_function_signature* sig = new(State)ir_function_signature(glsl_type::bool_type);
		sig->replace_parameters(params);
		sig->is_builtin = true;
		sig->is_defined = false;
		sig->has_output_parameters = false;
		func->add_signature(sig);
		State->symbols->add_global_function(func);
	}

	if (State->language_version >= 310)
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

		auto AddIntrisicReturningFloat = [](_mesa_glsl_parse_state* State, exec_list* ir, const char* Name, int32 NumColumns)
		{
			ir_function* Func = new(State) ir_function(Name);
			auto* ReturnType = glsl_type::get_instance(GLSL_TYPE_FLOAT, NumColumns, 1);
			ir_function_signature* Sig = new(State) ir_function_signature(ReturnType);
			Sig->is_builtin = true;
			Func->add_signature(Sig);
			State->symbols->add_global_function(Func);
			ir->push_head(Func);
		};

		for (int32 i = 0; i < UE_ARRAY_COUNT(GL_FRAMEBUFFER_FETCH); ++i)
		{
			if (!strcmp(GL_FRAMEBUFFER_FETCH_TYPE[i], "float"))
			{
				AddIntrisicReturningFloat(State, ir, GL_FRAMEBUFFER_FETCH[i], 1);
			}
			else if (!strcmp(GL_FRAMEBUFFER_FETCH_TYPE[i], "vec3"))
			{
				AddIntrisicReturningFloat(State, ir, GL_FRAMEBUFFER_FETCH[i], 3);
			}
			else
			{
				AddIntrisicReturningFloat(State, ir, GL_FRAMEBUFFER_FETCH[i], 4);
			}
		}

		auto AddIntrisicVecParam = [](_mesa_glsl_parse_state* State, exec_list* ir, const char* Name, const glsl_type* GlslType)
		{
			ir_function* Func = new(State) ir_function(Name);

			ir_variable* param = new (State) ir_variable(GlslType, "Src", ir_variable_mode::ir_var_in);
			exec_list* params = new(State) exec_list();
			params->push_tail(param);

			ir_function_signature* sig = new(State)ir_function_signature(glsl_type::void_type);
			sig->replace_parameters(params);
			sig->is_defined = false;
			sig->has_output_parameters = true;
			sig->is_builtin = true;
			
			Func->add_signature(sig);
			State->symbols->add_global_function(Func);
			ir->push_head(Func);
		};

		for (int32 i = 0; i < UE_ARRAY_COUNT(GL_FRAMEBUFFER_FETCH_WRITE); ++i)
		{
			if (!strcmp(GL_FRAMEBUFFER_FETCH_TYPE[i], "float"))
			{
				AddIntrisicVecParam(State, ir, GL_FRAMEBUFFER_FETCH_WRITE[i], glsl_type::half_type);
			}
			else if (!strcmp(GL_FRAMEBUFFER_FETCH_TYPE[i], "vec3"))
			{
				AddIntrisicVecParam(State, ir, GL_FRAMEBUFFER_FETCH_WRITE[i], glsl_type::half3_type);
			}
			else
			{
				AddIntrisicVecParam(State, ir, GL_FRAMEBUFFER_FETCH_WRITE[i], glsl_type::half4_type);
			}
		}
	}
}

