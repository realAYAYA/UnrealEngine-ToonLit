/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse         _mesa_hlsl_parse
#define yylex           _mesa_hlsl_lex
#define yyerror         _mesa_hlsl_error
#define yylval          _mesa_hlsl_lval
#define yychar          _mesa_hlsl_char
#define yydebug         _mesa_hlsl_debug
#define yynerrs         _mesa_hlsl_nerrs
#define yylloc          _mesa_hlsl_lloc

/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 1 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"

// Copyright Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

// Enable to debug the parser state machine (Flex & Bison), enable _mesa_hlsl_debug = 1; on hlslcc.cpp
//#define YYDEBUG 1

/*
 * Copyright © 2008, 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ShaderCompilerCommon.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "glsl_types.h"

#define YYLEX_PARAM state->scanner

#undef yyerror

static void yyerror(YYLTYPE *loc, _mesa_glsl_parse_state *st, const char *msg)
{
   _mesa_glsl_error(loc, st, "%s", msg);
}


/* Line 268 of yacc.c  */
#line 128 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CONST_TOK = 258,
     BOOL_TOK = 259,
     FLOAT_TOK = 260,
     INT_TOK = 261,
     UINT_TOK = 262,
     BREAK = 263,
     CONTINUE = 264,
     DO = 265,
     ELSE = 266,
     FOR = 267,
     IF = 268,
     DISCARD = 269,
     RETURN = 270,
     SWITCH = 271,
     CASE = 272,
     DEFAULT = 273,
     BVEC2 = 274,
     BVEC3 = 275,
     BVEC4 = 276,
     IVEC2 = 277,
     IVEC3 = 278,
     IVEC4 = 279,
     UVEC2 = 280,
     UVEC3 = 281,
     UVEC4 = 282,
     VEC2 = 283,
     VEC3 = 284,
     VEC4 = 285,
     HALF_TOK = 286,
     HVEC2 = 287,
     HVEC3 = 288,
     HVEC4 = 289,
     IN_TOK = 290,
     OUT_TOK = 291,
     INOUT_TOK = 292,
     UNIFORM = 293,
     VARYING = 294,
     GLOBALLYCOHERENT = 295,
     SHARED = 296,
     PRECISE = 297,
     CENTROID = 298,
     NOPERSPECTIVE = 299,
     NOINTERPOLATION = 300,
     LINEAR = 301,
     MAT2X2 = 302,
     MAT2X3 = 303,
     MAT2X4 = 304,
     MAT3X2 = 305,
     MAT3X3 = 306,
     MAT3X4 = 307,
     MAT4X2 = 308,
     MAT4X3 = 309,
     MAT4X4 = 310,
     HMAT2X2 = 311,
     HMAT2X3 = 312,
     HMAT2X4 = 313,
     HMAT3X2 = 314,
     HMAT3X3 = 315,
     HMAT3X4 = 316,
     HMAT4X2 = 317,
     HMAT4X3 = 318,
     HMAT4X4 = 319,
     FMAT2X2 = 320,
     FMAT2X3 = 321,
     FMAT2X4 = 322,
     FMAT3X2 = 323,
     FMAT3X3 = 324,
     FMAT3X4 = 325,
     FMAT4X2 = 326,
     FMAT4X3 = 327,
     FMAT4X4 = 328,
     SAMPLERSTATE = 329,
     SAMPLERSTATE_CMP = 330,
     TEXTURE1D = 331,
     TEXTURE1D_ARRAY = 332,
     TEXTURE2D = 333,
     TEXTURE2D_ARRAY = 334,
     TEXTURE2DMS = 335,
     TEXTURE_EXTERNAL = 336,
     BUFFER = 337,
     STRUCTUREDBUFFER = 338,
     BYTEADDRESSBUFFER = 339,
     TEXTURE2DMS_ARRAY = 340,
     TEXTURE3D = 341,
     TEXTURECUBE = 342,
     TEXTURECUBE_ARRAY = 343,
     RWBUFFER = 344,
     RWTEXTURE1D = 345,
     RWTEXTURE1D_ARRAY = 346,
     RWTEXTURE2D = 347,
     RWTEXTURE2D_ARRAY = 348,
     RWTEXTURE3D = 349,
     RWSTRUCTUREDBUFFER = 350,
     RWBYTEADDRESSBUFFER = 351,
     POINT_TOK = 352,
     LINE_TOK = 353,
     TRIANGLE_TOK = 354,
     LINEADJ_TOK = 355,
     TRIANGLEADJ_TOK = 356,
     POINTSTREAM = 357,
     LINESTREAM = 358,
     TRIANGLESTREAM = 359,
     INPUTPATCH = 360,
     OUTPUTPATCH = 361,
     STRUCT = 362,
     VOID_TOK = 363,
     WHILE = 364,
     CBUFFER = 365,
     REGISTER = 366,
     PACKOFFSET = 367,
     IDENTIFIER = 368,
     TYPE_IDENTIFIER = 369,
     NEW_IDENTIFIER = 370,
     FLOATCONSTANT = 371,
     INTCONSTANT = 372,
     UINTCONSTANT = 373,
     BOOLCONSTANT = 374,
     STRINGCONSTANT = 375,
     FIELD_SELECTION = 376,
     LEFT_OP = 377,
     RIGHT_OP = 378,
     INC_OP = 379,
     DEC_OP = 380,
     LE_OP = 381,
     GE_OP = 382,
     EQ_OP = 383,
     NE_OP = 384,
     AND_OP = 385,
     OR_OP = 386,
     MUL_ASSIGN = 387,
     DIV_ASSIGN = 388,
     ADD_ASSIGN = 389,
     MOD_ASSIGN = 390,
     LEFT_ASSIGN = 391,
     RIGHT_ASSIGN = 392,
     AND_ASSIGN = 393,
     XOR_ASSIGN = 394,
     OR_ASSIGN = 395,
     SUB_ASSIGN = 396,
     INVARIANT = 397,
     VERSION_TOK = 398,
     EXTENSION = 399,
     LINE = 400,
     COLON = 401,
     EOL = 402,
     INTERFACE = 403,
     OUTPUT = 404,
     PRAGMA_DEBUG_ON = 405,
     PRAGMA_DEBUG_OFF = 406,
     PRAGMA_OPTIMIZE_ON = 407,
     PRAGMA_OPTIMIZE_OFF = 408,
     PRAGMA_INVARIANT_ALL = 409,
     ASM = 410,
     CLASS = 411,
     UNION = 412,
     ENUM = 413,
     TYPEDEF = 414,
     TEMPLATE = 415,
     THIS = 416,
     PACKED_TOK = 417,
     GOTO = 418,
     INLINE_TOK = 419,
     NOINLINE = 420,
     VOLATILE = 421,
     PUBLIC_TOK = 422,
     STATIC = 423,
     EXTERN = 424,
     EXTERNAL = 425,
     LONG_TOK = 426,
     SHORT_TOK = 427,
     DOUBLE_TOK = 428,
     HALF = 429,
     FIXED_TOK = 430,
     UNSIGNED = 431,
     DVEC2 = 432,
     DVEC3 = 433,
     DVEC4 = 434,
     FVEC2 = 435,
     FVEC3 = 436,
     FVEC4 = 437,
     SAMPLER2DRECT = 438,
     SAMPLER3DRECT = 439,
     SAMPLER2DRECTSHADOW = 440,
     SIZEOF = 441,
     CAST = 442,
     NAMESPACE = 443,
     USING = 444,
     ERROR_TOK = 445,
     COMMON = 446,
     PARTITION = 447,
     ACTIVE = 448,
     SAMPLERBUFFER = 449,
     FILTER = 450,
     IMAGE1D = 451,
     IMAGE2D = 452,
     IMAGE3D = 453,
     IMAGECUBE = 454,
     IMAGE1DARRAY = 455,
     IMAGE2DARRAY = 456,
     IIMAGE1D = 457,
     IIMAGE2D = 458,
     IIMAGE3D = 459,
     IIMAGECUBE = 460,
     IIMAGE1DARRAY = 461,
     IIMAGE2DARRAY = 462,
     UIMAGE1D = 463,
     UIMAGE2D = 464,
     UIMAGE3D = 465,
     UIMAGECUBE = 466,
     UIMAGE1DARRAY = 467,
     UIMAGE2DARRAY = 468,
     IMAGE1DSHADOW = 469,
     IMAGE2DSHADOW = 470,
     IMAGEBUFFER = 471,
     IIMAGEBUFFER = 472,
     UIMAGEBUFFER = 473,
     IMAGE1DARRAYSHADOW = 474,
     IMAGE2DARRAYSHADOW = 475,
     ROW_MAJOR = 476,
     COLUMN_MAJOR = 477
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 293 of yacc.c  */
#line 64 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"

   int n;
   float real;
   const char *identifier;
   const char *string_literal;

   struct ast_type_qualifier type_qualifier;

   ast_node *node;
   ast_type_specifier *type_specifier;
   ast_fully_specified_type *fully_specified_type;
   ast_function *function;
   ast_parameter_declarator *parameter_declarator;
   ast_function_definition *function_definition;
   ast_compound_statement *compound_statement;
   ast_expression *expression;
   ast_declarator_list *declarator_list;
   ast_struct_specifier *struct_specifier;
   ast_declaration *declaration;
   ast_switch_body *switch_body;
   ast_case_label *case_label;
   ast_case_label_list *case_label_list;
   ast_case_statement *case_statement;
   ast_case_statement_list *case_statement_list;

   struct {
	  ast_node *cond;
	  ast_expression *rest;
   } for_rest_statement;

   struct {
	  ast_node *then_statement;
	  ast_node *else_statement;
   } selection_rest_statement;

   ast_attribute* attribute;
   ast_attribute_arg* attribute_arg;



/* Line 293 of yacc.c  */
#line 427 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 452 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   6460

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  247
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  107
/* YYNRULES -- Number of rules.  */
#define YYNRULES  380
/* YYNRULES -- Number of states.  */
#define YYNSTATES  596

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   477

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   231,     2,     2,     2,   235,   238,     2,
     223,   224,   233,   229,   228,   230,   227,   234,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   242,   244,
     236,   243,   237,   241,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   225,     2,   226,   239,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   245,   240,   246,   232,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219,   220,   221,   222
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     7,     9,    11,    13,    15,    18,
      20,    22,    24,    26,    28,    30,    32,    36,    38,    43,
      45,    49,    52,    55,    57,    59,    61,    65,    68,    71,
      74,    76,    79,    83,    86,    88,    90,    92,    95,    98,
     101,   103,   106,   110,   113,   115,   118,   121,   124,   129,
     135,   141,   143,   145,   147,   149,   151,   155,   159,   163,
     165,   169,   173,   175,   179,   183,   185,   189,   193,   197,
     201,   203,   207,   211,   213,   217,   219,   223,   225,   229,
     231,   235,   237,   241,   243,   249,   251,   255,   257,   259,
     261,   263,   265,   267,   269,   271,   273,   275,   277,   279,
     283,   285,   288,   290,   293,   296,   301,   303,   305,   308,
     312,   316,   321,   324,   329,   334,   337,   345,   349,   353,
     358,   361,   364,   366,   370,   373,   375,   377,   379,   382,
     385,   387,   389,   391,   393,   395,   397,   399,   403,   409,
     413,   421,   427,   433,   435,   438,   443,   446,   453,   458,
     463,   466,   468,   471,   473,   475,   477,   479,   482,   485,
     487,   489,   491,   493,   495,   497,   500,   503,   507,   509,
     511,   514,   517,   519,   521,   524,   526,   528,   531,   534,
     536,   538,   540,   542,   545,   548,   550,   552,   554,   556,
     558,   562,   566,   571,   576,   578,   580,   587,   592,   597,
     602,   607,   612,   617,   622,   627,   634,   641,   643,   645,
     647,   649,   651,   653,   655,   657,   659,   661,   663,   665,
     667,   669,   671,   673,   675,   677,   679,   681,   683,   685,
     687,   689,   691,   693,   695,   697,   699,   701,   703,   705,
     707,   709,   711,   713,   715,   717,   719,   721,   723,   725,
     727,   729,   731,   733,   735,   737,   739,   741,   743,   745,
     747,   749,   751,   753,   755,   757,   759,   761,   763,   765,
     767,   769,   771,   773,   775,   781,   789,   794,   799,   806,
     810,   816,   821,   823,   826,   830,   832,   835,   837,   840,
     843,   845,   847,   849,   853,   856,   858,   862,   869,   874,
     879,   881,   885,   887,   891,   894,   896,   898,   900,   903,
     906,   908,   910,   912,   914,   916,   918,   921,   922,   927,
     929,   931,   934,   938,   940,   943,   945,   948,   954,   958,
     960,   962,   967,   973,   976,   980,   984,   987,   989,   992,
     995,   998,  1000,  1003,  1009,  1017,  1024,  1026,  1028,  1030,
    1031,  1034,  1038,  1041,  1044,  1047,  1051,  1054,  1056,  1058,
    1060,  1062,  1064,  1068,  1072,  1076,  1083,  1090,  1093,  1095,
    1098,  1102,  1107,  1114,  1119,  1126,  1127,  1130,  1132,  1137,
    1140
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     248,     0,    -1,    -1,   249,   251,    -1,   113,    -1,   114,
      -1,   115,    -1,   344,    -1,   251,   344,    -1,   113,    -1,
     115,    -1,   252,    -1,   117,    -1,   118,    -1,   116,    -1,
     119,    -1,   223,   282,   224,    -1,   253,    -1,   254,   225,
     255,   226,    -1,   256,    -1,   254,   227,   250,    -1,   254,
     124,    -1,   254,   125,    -1,   282,    -1,   257,    -1,   258,
      -1,   254,   227,   263,    -1,   260,   224,    -1,   259,   224,
      -1,   261,   108,    -1,   261,    -1,   261,   280,    -1,   260,
     228,   280,    -1,   262,   223,    -1,   301,    -1,   252,    -1,
     121,    -1,   265,   224,    -1,   264,   224,    -1,   266,   108,
      -1,   266,    -1,   266,   280,    -1,   265,   228,   280,    -1,
     252,   223,    -1,   254,    -1,   124,   267,    -1,   125,   267,
      -1,   268,   267,    -1,   223,   304,   224,   267,    -1,   223,
       3,   304,   224,   267,    -1,   223,   304,     3,   224,   267,
      -1,   229,    -1,   230,    -1,   231,    -1,   232,    -1,   267,
      -1,   269,   233,   267,    -1,   269,   234,   267,    -1,   269,
     235,   267,    -1,   269,    -1,   270,   229,   269,    -1,   270,
     230,   269,    -1,   270,    -1,   271,   122,   270,    -1,   271,
     123,   270,    -1,   271,    -1,   272,   236,   271,    -1,   272,
     237,   271,    -1,   272,   126,   271,    -1,   272,   127,   271,
      -1,   272,    -1,   273,   128,   272,    -1,   273,   129,   272,
      -1,   273,    -1,   274,   238,   273,    -1,   274,    -1,   275,
     239,   274,    -1,   275,    -1,   276,   240,   275,    -1,   276,
      -1,   277,   130,   276,    -1,   277,    -1,   278,   131,   277,
      -1,   278,    -1,   278,   241,   282,   242,   280,    -1,   279,
      -1,   267,   281,   280,    -1,   243,    -1,   132,    -1,   133,
      -1,   135,    -1,   134,    -1,   141,    -1,   136,    -1,   137,
      -1,   138,    -1,   139,    -1,   140,    -1,   280,    -1,   282,
     228,   280,    -1,   279,    -1,   286,   244,    -1,   284,    -1,
     294,   244,    -1,   287,   224,    -1,   287,   224,   242,   250,
      -1,   289,    -1,   288,    -1,   289,   291,    -1,   288,   228,
     291,    -1,   296,   252,   223,    -1,   164,   296,   252,   223,
      -1,   301,   250,    -1,   301,   250,   243,   283,    -1,   301,
     250,   242,   250,    -1,   301,   318,    -1,   301,   250,   225,
     283,   226,   242,   250,    -1,   298,   292,   290,    -1,   292,
     298,   290,    -1,   298,   292,   298,   290,    -1,   292,   290,
      -1,   298,   290,    -1,   290,    -1,   298,   292,   293,    -1,
     292,   293,    -1,    35,    -1,    36,    -1,    37,    -1,    35,
      36,    -1,    36,    35,    -1,    97,    -1,    98,    -1,    99,
      -1,   100,    -1,   101,    -1,   301,    -1,   295,    -1,   294,
     228,   250,    -1,   294,   228,   250,   225,   226,    -1,   294,
     228,   318,    -1,   294,   228,   250,   225,   226,   243,   319,
      -1,   294,   228,   318,   243,   319,    -1,   294,   228,   250,
     243,   319,    -1,   296,    -1,   296,   250,    -1,   296,   250,
     225,   226,    -1,   296,   318,    -1,   296,   250,   225,   226,
     243,   319,    -1,   296,   318,   243,   319,    -1,   296,   250,
     243,   319,    -1,   142,   252,    -1,   301,    -1,   299,   301,
      -1,    46,    -1,    45,    -1,    44,    -1,   297,    -1,    43,
     297,    -1,   297,    43,    -1,    43,    -1,     3,    -1,    38,
      -1,    42,    -1,   300,    -1,   297,    -1,   297,   300,    -1,
     142,   300,    -1,   142,   297,   300,    -1,   142,    -1,    42,
      -1,    42,   299,    -1,   299,    42,    -1,     3,    -1,    39,
      -1,    43,    39,    -1,    35,    -1,    36,    -1,    43,    35,
      -1,    43,    36,    -1,    38,    -1,   221,    -1,   222,    -1,
     168,    -1,     3,   168,    -1,   168,     3,    -1,    40,    -1,
      41,    -1,   302,    -1,   304,    -1,   303,    -1,   304,   225,
     226,    -1,   303,   225,   226,    -1,   304,   225,   283,   226,
      -1,   303,   225,   283,   226,    -1,   305,    -1,   306,    -1,
     306,   236,   305,   228,   117,   237,    -1,   306,   236,   305,
     237,    -1,    83,   236,   305,   237,    -1,    83,   236,   310,
     237,    -1,    83,   236,   114,   237,    -1,    95,   236,   305,
     237,    -1,    95,   236,   310,   237,    -1,    95,   236,   114,
     237,    -1,   307,   236,   114,   237,    -1,   308,   236,   114,
     228,   117,   237,    -1,   309,   236,   114,   228,   117,   237,
      -1,   310,    -1,   114,    -1,   108,    -1,     5,    -1,    31,
      -1,     6,    -1,     7,    -1,     4,    -1,    28,    -1,    29,
      -1,    30,    -1,    32,    -1,    33,    -1,    34,    -1,    19,
      -1,    20,    -1,    21,    -1,    22,    -1,    23,    -1,    24,
      -1,    25,    -1,    26,    -1,    27,    -1,    47,    -1,    48,
      -1,    49,    -1,    50,    -1,    51,    -1,    52,    -1,    53,
      -1,    54,    -1,    55,    -1,    56,    -1,    57,    -1,    58,
      -1,    59,    -1,    60,    -1,    61,    -1,    62,    -1,    63,
      -1,    64,    -1,    74,    -1,    75,    -1,    82,    -1,    84,
      -1,    76,    -1,    77,    -1,    78,    -1,    79,    -1,    80,
      -1,    81,    -1,    85,    -1,    86,    -1,    87,    -1,    88,
      -1,    89,    -1,    96,    -1,    90,    -1,    91,    -1,    92,
      -1,    93,    -1,    94,    -1,   102,    -1,   103,    -1,   104,
      -1,   105,    -1,   106,    -1,   107,   250,   245,   312,   246,
      -1,   107,   250,   242,   114,   245,   312,   246,    -1,   107,
     245,   312,   246,    -1,   107,   250,   245,   246,    -1,   107,
     250,   242,   114,   245,   246,    -1,   107,   245,   246,    -1,
     110,   250,   245,   312,   246,    -1,   110,   250,   245,   246,
      -1,   313,    -1,   312,   313,    -1,   314,   316,   244,    -1,
     301,    -1,   315,   301,    -1,   297,    -1,    43,   297,    -1,
     297,    43,    -1,    43,    -1,    42,    -1,   317,    -1,   316,
     228,   317,    -1,   250,   352,    -1,   318,    -1,   250,   242,
     250,    -1,   250,   225,   283,   226,   242,   250,    -1,   250,
     225,   283,   226,    -1,   318,   225,   283,   226,    -1,   280,
      -1,   245,   320,   246,    -1,   319,    -1,   320,   228,   319,
      -1,   320,   228,    -1,   285,    -1,   324,    -1,   323,    -1,
     348,   324,    -1,   348,   323,    -1,   321,    -1,   329,    -1,
     330,    -1,   333,    -1,   339,    -1,   343,    -1,   245,   246,
      -1,    -1,   245,   325,   328,   246,    -1,   327,    -1,   323,
      -1,   245,   246,    -1,   245,   328,   246,    -1,   322,    -1,
     328,   322,    -1,   244,    -1,   282,   244,    -1,    13,   223,
     282,   224,   331,    -1,   322,    11,   322,    -1,   322,    -1,
     282,    -1,   296,   250,   243,   319,    -1,    16,   223,   282,
     224,   334,    -1,   245,   246,    -1,   245,   338,   246,    -1,
      17,   282,   242,    -1,    18,   242,    -1,   335,    -1,   336,
     335,    -1,   336,   322,    -1,   337,   322,    -1,   337,    -1,
     338,   337,    -1,   109,   223,   332,   224,   326,    -1,    10,
     322,   109,   223,   282,   224,   244,    -1,    12,   223,   340,
     342,   224,   326,    -1,   329,    -1,   321,    -1,   332,    -1,
      -1,   341,   244,    -1,   341,   244,   282,    -1,     9,   244,
      -1,     8,   244,    -1,    15,   244,    -1,    15,   282,   244,
      -1,    14,   244,    -1,   349,    -1,   353,    -1,   283,    -1,
     120,    -1,   345,    -1,   346,   228,   345,    -1,   225,   250,
     226,    -1,   225,   305,   226,    -1,   225,   250,   223,   346,
     224,   226,    -1,   225,   305,   223,   346,   224,   226,    -1,
     348,   347,    -1,   347,    -1,   286,   327,    -1,   348,   286,
     327,    -1,   111,   223,   115,   224,    -1,   111,   223,   115,
     228,   115,   224,    -1,   112,   223,   115,   224,    -1,   112,
     223,   115,   227,   115,   224,    -1,    -1,   242,   351,    -1,
     284,    -1,   295,   242,   350,   244,    -1,   294,   244,    -1,
     311,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   282,   282,   282,   294,   295,   296,   300,   308,   319,
     320,   324,   331,   338,   352,   359,   366,   373,   374,   380,
     384,   391,   397,   406,   410,   414,   415,   424,   425,   429,
     430,   434,   440,   452,   456,   462,   469,   479,   480,   484,
     485,   489,   495,   507,   518,   519,   525,   531,   537,   544,
     551,   562,   563,   564,   565,   569,   570,   576,   582,   591,
     592,   598,   607,   608,   614,   623,   624,   630,   636,   642,
     651,   652,   658,   667,   668,   677,   678,   687,   688,   697,
     698,   707,   708,   717,   718,   727,   728,   737,   738,   739,
     740,   741,   742,   743,   744,   745,   746,   747,   751,   755,
     771,   775,   783,   787,   794,   797,   805,   806,   810,   815,
     823,   834,   848,   858,   869,   880,   892,   908,   915,   922,
     930,   935,   940,   941,   952,   964,   969,   974,   980,   986,
     992,   997,  1002,  1007,  1012,  1020,  1024,  1025,  1035,  1045,
    1054,  1064,  1074,  1088,  1095,  1104,  1113,  1122,  1131,  1141,
    1150,  1164,  1171,  1182,  1187,  1192,  1200,  1201,  1206,  1211,
    1216,  1221,  1226,  1234,  1235,  1236,  1241,  1246,  1252,  1257,
    1262,  1267,  1275,  1280,  1285,  1291,  1296,  1301,  1306,  1311,
    1316,  1321,  1326,  1331,  1337,  1343,  1348,  1356,  1363,  1364,
    1368,  1374,  1379,  1385,  1394,  1400,  1406,  1413,  1419,  1425,
    1431,  1437,  1443,  1449,  1455,  1461,  1468,  1475,  1481,  1490,
    1491,  1492,  1493,  1494,  1495,  1496,  1497,  1498,  1499,  1500,
    1501,  1502,  1503,  1504,  1505,  1506,  1507,  1508,  1509,  1510,
    1511,  1512,  1513,  1514,  1515,  1516,  1517,  1518,  1519,  1520,
    1521,  1522,  1523,  1524,  1525,  1526,  1527,  1528,  1529,  1530,
    1565,  1566,  1567,  1568,  1569,  1570,  1571,  1572,  1573,  1574,
    1575,  1576,  1577,  1578,  1579,  1580,  1581,  1582,  1583,  1587,
    1588,  1589,  1593,  1597,  1601,  1608,  1615,  1621,  1628,  1635,
    1644,  1650,  1658,  1663,  1671,  1681,  1688,  1699,  1700,  1705,
    1710,  1715,  1723,  1728,  1736,  1743,  1748,  1756,  1766,  1772,
    1781,  1782,  1791,  1796,  1801,  1808,  1814,  1815,  1816,  1821,
    1829,  1830,  1831,  1832,  1833,  1834,  1838,  1845,  1844,  1858,
    1859,  1863,  1869,  1878,  1888,  1900,  1906,  1915,  1924,  1929,
    1937,  1941,  1959,  1967,  1972,  1980,  1985,  1993,  2001,  2009,
    2017,  2025,  2033,  2041,  2048,  2055,  2065,  2066,  2070,  2072,
    2078,  2083,  2092,  2098,  2104,  2110,  2116,  2125,  2126,  2130,
    2136,  2145,  2149,  2157,  2163,  2169,  2176,  2186,  2191,  2198,
    2208,  2222,  2223,  2227,  2228,  2231,  2232,  2236,  2240,  2248,
    2256
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CONST_TOK", "BOOL_TOK", "FLOAT_TOK",
  "INT_TOK", "UINT_TOK", "BREAK", "CONTINUE", "DO", "ELSE", "FOR", "IF",
  "DISCARD", "RETURN", "SWITCH", "CASE", "DEFAULT", "BVEC2", "BVEC3",
  "BVEC4", "IVEC2", "IVEC3", "IVEC4", "UVEC2", "UVEC3", "UVEC4", "VEC2",
  "VEC3", "VEC4", "HALF_TOK", "HVEC2", "HVEC3", "HVEC4", "IN_TOK",
  "OUT_TOK", "INOUT_TOK", "UNIFORM", "VARYING", "GLOBALLYCOHERENT",
  "SHARED", "PRECISE", "CENTROID", "NOPERSPECTIVE", "NOINTERPOLATION",
  "LINEAR", "MAT2X2", "MAT2X3", "MAT2X4", "MAT3X2", "MAT3X3", "MAT3X4",
  "MAT4X2", "MAT4X3", "MAT4X4", "HMAT2X2", "HMAT2X3", "HMAT2X4", "HMAT3X2",
  "HMAT3X3", "HMAT3X4", "HMAT4X2", "HMAT4X3", "HMAT4X4", "FMAT2X2",
  "FMAT2X3", "FMAT2X4", "FMAT3X2", "FMAT3X3", "FMAT3X4", "FMAT4X2",
  "FMAT4X3", "FMAT4X4", "SAMPLERSTATE", "SAMPLERSTATE_CMP", "TEXTURE1D",
  "TEXTURE1D_ARRAY", "TEXTURE2D", "TEXTURE2D_ARRAY", "TEXTURE2DMS",
  "TEXTURE_EXTERNAL", "BUFFER", "STRUCTUREDBUFFER", "BYTEADDRESSBUFFER",
  "TEXTURE2DMS_ARRAY", "TEXTURE3D", "TEXTURECUBE", "TEXTURECUBE_ARRAY",
  "RWBUFFER", "RWTEXTURE1D", "RWTEXTURE1D_ARRAY", "RWTEXTURE2D",
  "RWTEXTURE2D_ARRAY", "RWTEXTURE3D", "RWSTRUCTUREDBUFFER",
  "RWBYTEADDRESSBUFFER", "POINT_TOK", "LINE_TOK", "TRIANGLE_TOK",
  "LINEADJ_TOK", "TRIANGLEADJ_TOK", "POINTSTREAM", "LINESTREAM",
  "TRIANGLESTREAM", "INPUTPATCH", "OUTPUTPATCH", "STRUCT", "VOID_TOK",
  "WHILE", "CBUFFER", "REGISTER", "PACKOFFSET", "IDENTIFIER",
  "TYPE_IDENTIFIER", "NEW_IDENTIFIER", "FLOATCONSTANT", "INTCONSTANT",
  "UINTCONSTANT", "BOOLCONSTANT", "STRINGCONSTANT", "FIELD_SELECTION",
  "LEFT_OP", "RIGHT_OP", "INC_OP", "DEC_OP", "LE_OP", "GE_OP", "EQ_OP",
  "NE_OP", "AND_OP", "OR_OP", "MUL_ASSIGN", "DIV_ASSIGN", "ADD_ASSIGN",
  "MOD_ASSIGN", "LEFT_ASSIGN", "RIGHT_ASSIGN", "AND_ASSIGN", "XOR_ASSIGN",
  "OR_ASSIGN", "SUB_ASSIGN", "INVARIANT", "VERSION_TOK", "EXTENSION",
  "LINE", "COLON", "EOL", "INTERFACE", "OUTPUT", "PRAGMA_DEBUG_ON",
  "PRAGMA_DEBUG_OFF", "PRAGMA_OPTIMIZE_ON", "PRAGMA_OPTIMIZE_OFF",
  "PRAGMA_INVARIANT_ALL", "ASM", "CLASS", "UNION", "ENUM", "TYPEDEF",
  "TEMPLATE", "THIS", "PACKED_TOK", "GOTO", "INLINE_TOK", "NOINLINE",
  "VOLATILE", "PUBLIC_TOK", "STATIC", "EXTERN", "EXTERNAL", "LONG_TOK",
  "SHORT_TOK", "DOUBLE_TOK", "HALF", "FIXED_TOK", "UNSIGNED", "DVEC2",
  "DVEC3", "DVEC4", "FVEC2", "FVEC3", "FVEC4", "SAMPLER2DRECT",
  "SAMPLER3DRECT", "SAMPLER2DRECTSHADOW", "SIZEOF", "CAST", "NAMESPACE",
  "USING", "ERROR_TOK", "COMMON", "PARTITION", "ACTIVE", "SAMPLERBUFFER",
  "FILTER", "IMAGE1D", "IMAGE2D", "IMAGE3D", "IMAGECUBE", "IMAGE1DARRAY",
  "IMAGE2DARRAY", "IIMAGE1D", "IIMAGE2D", "IIMAGE3D", "IIMAGECUBE",
  "IIMAGE1DARRAY", "IIMAGE2DARRAY", "UIMAGE1D", "UIMAGE2D", "UIMAGE3D",
  "UIMAGECUBE", "UIMAGE1DARRAY", "UIMAGE2DARRAY", "IMAGE1DSHADOW",
  "IMAGE2DSHADOW", "IMAGEBUFFER", "IIMAGEBUFFER", "UIMAGEBUFFER",
  "IMAGE1DARRAYSHADOW", "IMAGE2DARRAYSHADOW", "ROW_MAJOR", "COLUMN_MAJOR",
  "'('", "')'", "'['", "']'", "'.'", "','", "'+'", "'-'", "'!'", "'~'",
  "'*'", "'/'", "'%'", "'<'", "'>'", "'&'", "'^'", "'|'", "'?'", "':'",
  "'='", "';'", "'{'", "'}'", "$accept", "translation_unit", "$@1",
  "any_identifier", "external_declaration_list", "variable_identifier",
  "primary_expression", "postfix_expression", "integer_expression",
  "function_call", "function_call_or_method", "function_call_generic",
  "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "method_call_generic",
  "method_call_header_no_parameters", "method_call_header_with_parameters",
  "method_call_header", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_or_expression",
  "conditional_expression", "assignment_expression", "assignment_operator",
  "expression", "constant_expression", "function_decl", "declaration",
  "function_prototype", "function_declarator",
  "function_header_with_parameters", "function_header",
  "parameter_declarator", "parameter_declaration", "parameter_qualifier",
  "parameter_type_specifier", "init_declarator_list", "single_declaration",
  "fully_specified_type", "interpolation_qualifier",
  "parameter_type_qualifier", "type_qualifier", "storage_qualifier",
  "type_specifier", "type_specifier_no_prec", "type_specifier_array",
  "type_specifier_nonarray", "basic_type_specifier_nonarray",
  "texture_type_specifier_nonarray",
  "outputstream_type_specifier_nonarray",
  "inputpatch_type_specifier_nonarray",
  "outputpatch_type_specifier_nonarray", "struct_specifier",
  "cbuffer_declaration", "struct_declaration_list", "struct_declaration",
  "struct_type_specifier", "struct_type_qualifier",
  "struct_declarator_list", "struct_declarator", "array_identifier",
  "initializer", "initializer_list", "declaration_statement", "statement",
  "simple_statement", "compound_statement", "$@2",
  "statement_no_new_scope", "compound_statement_no_new_scope",
  "statement_list", "expression_statement", "selection_statement",
  "selection_rest_statement", "condition", "switch_statement",
  "switch_body", "case_label", "case_label_list", "case_statement",
  "case_statement_list", "iteration_statement", "for_init_statement",
  "conditionopt", "for_rest_statement", "jump_statement",
  "external_declaration", "attribute_arg", "attribute_arg_list",
  "attribute", "attribute_list", "function_definition", "register",
  "packoffset", "packoffset_opt", "global_declaration", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458,   459,   460,   461,   462,   463,   464,
     465,   466,   467,   468,   469,   470,   471,   472,   473,   474,
     475,   476,   477,    40,    41,    91,    93,    46,    44,    43,
      45,    33,   126,    42,    47,    37,    60,    62,    38,    94,
     124,    63,    58,    61,    59,   123,   125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   247,   249,   248,   250,   250,   250,   251,   251,   252,
     252,   253,   253,   253,   253,   253,   253,   254,   254,   254,
     254,   254,   254,   255,   256,   257,   257,   258,   258,   259,
     259,   260,   260,   261,   262,   262,   262,   263,   263,   264,
     264,   265,   265,   266,   267,   267,   267,   267,   267,   267,
     267,   268,   268,   268,   268,   269,   269,   269,   269,   270,
     270,   270,   271,   271,   271,   272,   272,   272,   272,   272,
     273,   273,   273,   274,   274,   275,   275,   276,   276,   277,
     277,   278,   278,   279,   279,   280,   280,   281,   281,   281,
     281,   281,   281,   281,   281,   281,   281,   281,   282,   282,
     283,   284,   285,   285,   286,   286,   287,   287,   288,   288,
     289,   289,   290,   290,   290,   290,   290,   291,   291,   291,
     291,   291,   291,   291,   291,   292,   292,   292,   292,   292,
     292,   292,   292,   292,   292,   293,   294,   294,   294,   294,
     294,   294,   294,   295,   295,   295,   295,   295,   295,   295,
     295,   296,   296,   297,   297,   297,   298,   298,   298,   298,
     298,   298,   298,   299,   299,   299,   299,   299,   299,   299,
     299,   299,   300,   300,   300,   300,   300,   300,   300,   300,
     300,   300,   300,   300,   300,   300,   300,   301,   302,   302,
     303,   303,   303,   303,   304,   304,   304,   304,   304,   304,
     304,   304,   304,   304,   304,   304,   304,   304,   304,   305,
     305,   305,   305,   305,   305,   305,   305,   305,   305,   305,
     305,   305,   305,   305,   305,   305,   305,   305,   305,   305,
     305,   305,   305,   305,   305,   305,   305,   305,   305,   305,
     305,   305,   305,   305,   305,   305,   305,   305,   305,   305,
     306,   306,   306,   306,   306,   306,   306,   306,   306,   306,
     306,   306,   306,   306,   306,   306,   306,   306,   306,   307,
     307,   307,   308,   309,   310,   310,   310,   310,   310,   310,
     311,   311,   312,   312,   313,   314,   314,   315,   315,   315,
     315,   315,   316,   316,   317,   317,   317,   317,   318,   318,
     319,   319,   320,   320,   320,   321,   322,   322,   322,   322,
     323,   323,   323,   323,   323,   323,   324,   325,   324,   326,
     326,   327,   327,   328,   328,   329,   329,   330,   331,   331,
     332,   332,   333,   334,   334,   335,   335,   336,   336,   337,
     337,   338,   338,   339,   339,   339,   340,   340,   341,   341,
     342,   342,   343,   343,   343,   343,   343,   344,   344,   345,
     345,   346,   346,   347,   347,   347,   347,   348,   348,   349,
     349,   350,   350,   351,   351,   352,   352,   353,   353,   353,
     353
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     1,     1,     1,     1,     2,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     4,     1,
       3,     2,     2,     1,     1,     1,     3,     2,     2,     2,
       1,     2,     3,     2,     1,     1,     1,     2,     2,     2,
       1,     2,     3,     2,     1,     2,     2,     2,     4,     5,
       5,     1,     1,     1,     1,     1,     3,     3,     3,     1,
       3,     3,     1,     3,     3,     1,     3,     3,     3,     3,
       1,     3,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     1,     5,     1,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     3,
       1,     2,     1,     2,     2,     4,     1,     1,     2,     3,
       3,     4,     2,     4,     4,     2,     7,     3,     3,     4,
       2,     2,     1,     3,     2,     1,     1,     1,     2,     2,
       1,     1,     1,     1,     1,     1,     1,     3,     5,     3,
       7,     5,     5,     1,     2,     4,     2,     6,     4,     4,
       2,     1,     2,     1,     1,     1,     1,     2,     2,     1,
       1,     1,     1,     1,     1,     2,     2,     3,     1,     1,
       2,     2,     1,     1,     2,     1,     1,     2,     2,     1,
       1,     1,     1,     2,     2,     1,     1,     1,     1,     1,
       3,     3,     4,     4,     1,     1,     6,     4,     4,     4,
       4,     4,     4,     4,     4,     6,     6,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     5,     7,     4,     4,     6,     3,
       5,     4,     1,     2,     3,     1,     2,     1,     2,     2,
       1,     1,     1,     3,     2,     1,     3,     6,     4,     4,
       1,     3,     1,     3,     2,     1,     1,     1,     2,     2,
       1,     1,     1,     1,     1,     1,     2,     0,     4,     1,
       1,     2,     3,     1,     2,     1,     2,     5,     3,     1,
       1,     4,     5,     2,     3,     3,     2,     1,     2,     2,
       2,     1,     2,     5,     7,     6,     1,     1,     1,     0,
       2,     3,     2,     2,     2,     3,     2,     1,     1,     1,
       1,     1,     3,     3,     3,     6,     6,     2,     1,     2,
       3,     4,     6,     4,     6,     0,     2,     1,     4,     2,
       1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,   172,   214,   210,   212,   213,   221,
     222,   223,   224,   225,   226,   227,   228,   229,   215,   216,
     217,   211,   218,   219,   220,   175,   176,   179,   173,   185,
     186,   169,     0,   155,   154,   153,   230,   231,   232,   233,
     234,   235,   236,   237,   238,   239,   240,   241,   242,   243,
     244,   245,   246,   247,   248,   249,   252,   253,   254,   255,
     256,   257,   250,     0,   251,   258,   259,   260,   261,   262,
     264,   265,   266,   267,   268,     0,   263,   269,   270,   271,
     272,   273,     0,   209,     0,   208,   168,     0,   182,   180,
     181,     0,     3,   377,     0,     0,   107,   106,     0,   136,
     143,   164,     0,   163,   151,   187,   189,   188,   194,   195,
       0,     0,     0,   207,   380,     7,   368,     0,   357,   358,
     183,   168,   170,   177,   178,   174,     0,     0,     4,     5,
       6,     0,     0,     0,     9,    10,   150,     0,   166,     0,
     184,     0,     0,     8,   101,     0,   369,   104,     0,   160,
     125,   126,   127,   161,   162,   159,   130,   131,   132,   133,
     134,   122,   108,     0,   156,     0,     0,     0,   379,     0,
       4,     6,   144,     0,   146,   165,   171,   152,     0,     0,
       0,     0,     0,     0,     0,     0,   367,     0,     0,     0,
       0,     0,     0,   291,   290,   279,   287,   285,     0,   282,
       0,     0,     0,     0,     0,   167,     0,     0,   363,     0,
     364,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      14,    12,    13,    15,    36,     0,     0,     0,    51,    52,
      53,    54,   325,   317,   321,    11,    17,    44,    19,    24,
      25,     0,     0,    30,     0,    55,     0,    59,    62,    65,
      70,    73,    75,    77,    79,    81,    83,    85,    98,     0,
     102,   305,     0,     0,   136,   151,   310,   323,   307,   306,
       0,   311,   312,   313,   314,   315,     0,     0,   109,   128,
     129,   157,   120,   124,     0,   135,   158,   121,     0,   112,
     115,   137,   139,     0,     0,     0,     0,   110,     0,     0,
     191,    55,   100,     0,    34,   190,     0,     0,     0,     0,
       0,   370,   200,   198,   199,   203,   201,   202,   288,   289,
     276,   283,   375,     0,   292,   295,   286,     0,   277,     0,
     281,     0,   111,   360,   359,   361,     0,     0,   353,   352,
       0,     0,     0,   356,   354,     0,     0,     0,    45,    46,
       0,     0,   188,   316,     0,    21,    22,     0,     0,    28,
      27,     0,   209,    31,    33,    88,    89,    91,    90,    93,
      94,    95,    96,    97,    92,    87,     0,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   326,   103,
     322,   324,   309,   308,   105,   118,   117,   123,     0,     0,
       0,     0,     0,     0,     0,     0,   378,   145,     0,     0,
     300,   149,     0,   148,   193,   192,     0,   197,   204,     0,
       0,     0,     0,   294,     0,   284,     0,   274,   280,     0,
       0,     0,     0,   347,   346,   349,     0,   355,     0,   330,
       0,     0,     0,    16,     0,     0,     0,     0,    23,    20,
       0,    26,     0,     0,    40,    32,    86,    56,    57,    58,
      60,    61,    63,    64,    68,    69,    66,    67,    71,    72,
      74,    76,    78,    80,    82,     0,    99,   119,     0,   114,
     113,   138,   142,   141,     0,     0,   298,   302,     0,   299,
       0,     0,     0,     0,     0,   296,   376,   293,   278,     0,
     365,   362,   366,     0,   348,     0,     0,     0,     0,     0,
       0,     0,     0,    48,   318,    18,    43,    38,    37,     0,
     209,    41,     0,   298,     0,   371,     0,   147,   304,   301,
     196,   205,   206,   298,     0,   275,     0,   350,     0,   329,
     327,     0,   332,     0,   320,   343,   319,    49,    50,    42,
      84,     0,   140,     0,   303,     0,     0,     0,   351,   345,
       0,     0,     0,   333,   337,     0,   341,     0,   331,   116,
     372,   297,   373,     0,   344,   328,     0,   336,   339,   338,
     340,   334,   342,     0,   335,   374
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,   289,    92,   235,   236,   237,   457,   238,
     239,   240,   241,   242,   243,   244,   461,   462,   463,   464,
     245,   246,   247,   248,   249,   250,   251,   252,   253,   254,
     255,   256,   257,   258,   376,   259,   334,   260,   261,   262,
      95,    96,    97,   161,   162,   163,   283,   263,   264,   100,
     101,   165,   102,   103,   304,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   198,   199,   200,   201,   323,
     324,   290,   421,   498,   266,   267,   268,   269,   354,   555,
     556,   270,   271,   272,   550,   451,   273,   552,   574,   575,
     576,   577,   274,   445,   515,   516,   275,   115,   335,   336,
     116,   276,   118,   294,   506,   433,   119
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -273
static const yytype_int16 yypact[] =
{
    -273,    40,  5471,  -273,  -119,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,   183,   134,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -180,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -160,  -273,  -273,  -273,  -273,
    -273,  -273,   -91,  -273,   132,  -273,   542,  5775,    81,  -273,
    -273,   959,  5471,  -273,  -118,  -138,  -134,  5925,  -155,  -143,
     177,   154,  6241,  -273,  -273,  -273,   -89,   -73,  -273,  -130,
     -70,   -53,   -45,  -273,  -273,  -273,  -273,  5623,  -273,  -273,
    -273,   519,   164,  -273,  -273,  -273,  1204,  1448,  -273,  -273,
    -273,  2037,  -126,   -69,  -273,  -273,  -273,   154,  -273,   145,
    -273,  -122,   -38,  -273,  -273,   590,  -273,   -32,  5925,  -273,
     198,   215,  -273,  -273,  -273,   256,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  6031,   218,  6136,   132,   132,  -273,   152,
      43,    48,  -177,    52,  -172,  -273,  -273,  -273,  3955,  4141,
    1690,   163,   167,   180,    58,   145,  -273,    69,    82,    84,
      87,    89,    91,  -273,   256,  -273,   287,  -273,  2142,  -273,
     132,  6346,   219,  2247,  2352,  -273,   111,  4327,  -273,  4327,
    -273,    92,    93,  1565,   112,   117,    98,  3353,   121,   123,
    -273,  -273,  -273,  -273,  -273,  4885,  4885,  3769,  -273,  -273,
    -273,  -273,  -273,    97,  -273,   124,  -273,   -78,  -273,  -273,
    -273,   126,  -137,  5071,   136,   281,  4885,    75,   -13,   120,
     -92,   125,   110,   118,   109,   231,  -100,  -273,  -273,  -146,
    -273,  -273,   122,  -135,  -273,   142,  -273,  -273,  -273,  -273,
     834,  -273,  -273,  -273,  -273,  -273,  1565,   132,  -273,  -273,
    -273,  -273,  -273,  -273,  6346,   132,  -273,  -273,  6031,  -173,
     143,  -166,  -163,   144,   128,  4513,  3124,  -273,  4885,  3124,
    -273,  -273,  -273,   147,  -273,  -273,   148,  -106,   133,   141,
     149,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -192,  -132,  -273,   143,  -273,   137,  -273,  2457,
    -273,  2562,  -273,  -273,  -273,  -273,   -60,   -49,  -273,  -273,
     269,  2895,  4885,  -273,  -273,  -131,  4885,  3583,  -273,  -273,
    6346,   -47,     7,  -273,  1565,  -273,  -273,  4885,   177,  -273,
    -273,  4885,   155,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -273,  4885,  -273,  4885,  4885,
    4885,  4885,  4885,  4885,  4885,  4885,  4885,  4885,  4885,  4885,
    4885,  4885,  4885,  4885,  4885,  4885,  4885,  4885,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  6346,  4885,
     132,  4885,  4699,  3124,  3124,   268,  -273,   146,   159,  3124,
    -273,  -273,   160,  -273,  -273,  -273,   270,  -273,  -273,   273,
     275,  4885,   100,  -273,   132,  -273,  2667,  -273,  -273,   168,
    4327,   169,   173,  -273,  -273,  3583,   -46,  -273,   -44,   170,
     132,   175,   176,  -273,   178,  4885,  1078,   181,   170,  -273,
     185,  -273,   179,   -24,  5257,  -273,  -273,  -273,  -273,  -273,
      75,    75,   -13,   -13,   120,   120,   120,   120,   -92,   -92,
     125,   110,   118,   109,   231,  -125,  -273,  -273,   184,  -273,
    -273,   158,  -273,  -273,   -21,  3124,  -273,  -273,  -165,  -273,
     172,   174,   186,   199,   201,  -273,  -273,  -273,  -273,  2772,
    -273,  -273,  -273,  4885,  -273,   182,   203,  1565,   187,   188,
    1808,  4885,  4885,  -273,  -273,  -273,  -273,  -273,  -273,  4885,
     204,  -273,  4885,   193,  3124,  -273,   315,  -273,  3124,  -273,
    -273,  -273,  -273,   194,   318,  -273,   -19,  4885,  1808,   426,
    -273,     9,  -273,  3124,  -273,  -273,  -273,  -273,  -273,  -273,
    -273,   132,  -273,   214,  -273,   132,     6,   195,   170,  -273,
    1565,  4885,   200,  -273,  -273,  1322,  1565,    11,  -273,  -273,
    -273,  -273,  -273,   325,  -273,  -273,  -117,  -273,  -273,  -273,
    -273,  -273,  1565,   217,  -273,  -273
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -273,  -273,  -273,   -79,  -273,   -75,  -273,  -273,  -273,  -273,
    -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,  -273,
      86,  -273,   -68,   -87,  -148,   -74,    53,    54,    55,    51,
      56,  -273,  -142,  -141,  -273,  -209,  -124,    28,  -273,    13,
    -273,  -273,  -273,  -149,   299,   284,   162,    36,    42,   -85,
     -80,  -121,   421,   -29,    -2,  -273,  -273,  -188,   -48,  -273,
    -273,  -273,  -273,   191,  -273,  -184,  -189,  -273,  -273,  -273,
      19,   -93,  -254,  -273,   114,  -212,  -272,   207,  -273,   -90,
     -34,   102,   116,  -273,  -273,    14,  -273,  -273,  -115,  -273,
    -116,  -273,  -273,  -273,  -273,  -273,  -273,   370,    23,   276,
    -104,    59,  -273,  -273,  -273,  -273,  -273
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -40
static const yytype_int16 yytable[] =
{
     104,   340,   139,   132,   402,   133,   137,   174,   345,   321,
     454,   136,   141,   186,   282,    94,   287,   164,   351,   329,
     331,   172,   128,   129,   130,   173,   571,   572,   571,   572,
      93,   395,   185,   431,   385,   386,   302,   302,    98,   352,
       3,   137,   284,   142,    99,   423,   355,   356,   295,   120,
     432,   196,   409,   298,   303,   306,   126,   138,   401,   412,
     146,   117,   298,   538,   206,   302,   296,   302,   164,   410,
     411,   299,   175,   167,   292,   281,   127,   413,   188,   191,
     414,   539,   397,   164,   140,   104,   147,   360,   291,   168,
     104,   361,   138,   167,   148,   166,   434,   397,   398,   169,
     177,   207,   363,   397,   208,    94,   180,   325,   205,   399,
     173,   397,   435,   447,   318,   104,   202,   532,   196,   203,
      93,   322,   426,   196,   196,   594,   144,   145,    98,   197,
     184,   427,   307,   446,    99,   405,   178,   448,   449,   406,
     321,   396,   321,   265,   387,   388,   166,   357,   458,   358,
     311,   117,   179,   302,   131,   420,   302,     4,   420,   492,
     493,   285,   452,   166,   439,   497,   181,   408,   440,   123,
     124,   418,   186,   125,   422,   441,   204,   453,   517,   440,
     518,   397,   397,   182,   397,   209,     4,   485,   210,    25,
      26,   183,    27,    28,    29,    30,   197,    32,   404,   326,
     528,   197,   197,   535,   529,   567,   176,   536,   164,   397,
     277,   265,   504,   128,   129,   130,   381,   382,    25,    26,
     465,    27,    28,    29,    30,    31,    32,    33,    34,    35,
     582,   455,   179,   583,   279,   466,   449,   474,   475,   476,
     477,   537,   383,   384,   401,   128,   129,   130,   554,   196,
     280,   196,   509,   389,   390,   573,   486,   591,   134,   487,
     135,   286,   450,   293,   301,   301,    -9,   302,   265,   302,
     302,   -10,   420,   420,   265,   297,   554,   308,   420,   459,
     562,   309,   166,   460,   564,   488,   285,   490,   418,   302,
     170,   129,   171,   301,   310,   301,   472,   473,   302,   578,
      33,    34,    35,   145,   546,   549,   312,   503,   378,   379,
     380,   348,   349,   470,   471,   478,   479,   189,   192,   313,
     321,   314,    88,   531,   315,   121,   316,   197,   317,   197,
     319,   489,   377,   327,   332,   341,   338,   339,   568,   265,
     342,   325,   343,   353,   346,   265,   347,   -35,   391,   393,
     359,    88,   265,   505,   420,   322,   196,   392,   585,   364,
     450,   394,   586,   588,   590,   -34,   144,   415,   298,   429,
     428,   519,   416,   424,   425,    89,    90,   430,   442,   -29,
     590,   301,   436,   494,   301,   496,   499,   500,   559,   495,
     501,   560,   502,   420,   510,   512,   513,   420,   397,   520,
     521,   534,   522,   527,    89,    90,   166,   525,   526,   540,
     533,   541,   420,   365,   366,   367,   368,   369,   370,   371,
     372,   373,   374,   542,   544,   543,   547,   548,   -39,   196,
     563,   553,   551,   566,   197,   561,   565,   570,   580,   584,
     593,   595,   587,   265,   480,   483,   481,   278,   482,   288,
     407,   484,   122,   507,   265,   443,   456,   444,   569,   514,
     589,   592,   143,   511,   467,   468,   469,   301,   301,   301,
     301,   301,   301,   301,   301,   301,   301,   301,   301,   301,
     301,   301,   579,   403,     0,   337,   581,     0,     0,     0,
       0,     0,     0,     0,     0,   301,     0,   301,   301,     0,
       0,     0,     0,     0,     0,     0,     0,   197,     0,     0,
       0,     0,     0,     0,     0,   265,     0,   301,   265,     0,
       0,     0,     4,     0,   375,     0,   301,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   523,     0,     0,     0,     4,   265,     0,     0,     0,
       0,     0,     0,     0,    25,    26,     0,    27,    28,    29,
      30,     0,    32,    33,    34,    35,     0,     0,   265,     0,
       0,     0,     0,   265,   265,     0,     0,    25,    26,     0,
      27,    28,    29,    30,     0,    32,    33,    34,    35,     0,
     265,     0,     0,     4,     5,     6,     7,     8,   211,   212,
     213,     0,   214,   215,   216,   217,   218,   557,   558,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,     0,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,   134,     0,   135,     0,     0,
       0,     0,     0,     0,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    88,     0,     0,
       0,     0,    77,    78,    79,    80,    81,    82,    83,   219,
       0,     0,     0,   134,    85,   135,   220,   221,   222,   223,
      88,   224,     0,     0,   225,   226,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    86,     0,     0,     0,     0,     0,     0,     0,
      89,    90,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    87,     0,     0,     0,    88,     0,
       0,     0,     0,    89,    90,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    89,    90,   227,     0,    91,     0,     0,     0,   228,
     229,   230,   231,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   232,   233,   234,     4,     5,     6,
       7,     8,   211,   212,   213,     0,   214,   215,   216,   217,
     218,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,     0,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,     0,     0,     0,     0,     0,    77,    78,    79,    80,
      81,    82,    83,   219,     0,     0,     0,   134,    85,   135,
     220,   221,   222,   223,     0,   224,     0,     0,   225,   226,
       0,     0,     0,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    86,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,     0,     0,    87,     0,
       0,     0,    88,     0,     0,     0,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    54,    55,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    89,    90,   227,     0,    91,
       0,     0,     0,   228,   229,   230,   231,    83,     0,     0,
       0,     0,   128,   129,   130,     0,     0,     0,   232,   233,
     400,     4,     5,     6,     7,     8,   211,   212,   213,     0,
     214,   215,   216,   217,   218,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,     0,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,     0,     0,     0,     0,     0,
      77,    78,    79,    80,    81,    82,    83,   219,     0,     0,
       0,   134,    85,   135,   220,   221,   222,   223,     0,   224,
       0,     0,   225,   226,     0,     0,     0,     0,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,     0,     0,
      86,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
       0,     0,    87,     0,     0,     0,    88,     0,     0,     0,
       0,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    54,    55,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,   227,     0,    91,     0,     0,     0,   228,   229,   230,
     231,    82,    83,     0,     0,     0,     0,     0,   187,     0,
       0,     0,   232,   233,   524,     4,     5,     6,     7,     8,
     211,   212,   213,     0,   214,   215,   216,   217,   218,   571,
     572,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,     0,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
       0,     0,     0,     0,    77,    78,    79,    80,    81,    82,
      83,   219,     0,     0,     0,   134,    85,   135,   220,   221,
     222,   223,     0,   224,     0,     0,   225,   226,     0,     0,
       0,     0,     5,     6,     7,     8,     0,     0,     0,     0,
       0,     0,     0,     0,    86,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,     0,     0,    87,     0,     0,     0,
      88,     0,     0,     0,     0,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    54,    55,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    89,    90,   227,     0,    91,     0,     0,
       0,   228,   229,   230,   231,    82,    83,     0,     0,     0,
       0,     0,   190,     0,     0,     0,   232,   233,     4,     5,
       6,     7,     8,   211,   212,   213,     0,   214,   215,   216,
     217,   218,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,     0,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,     0,     0,     0,     0,     0,    77,    78,    79,
      80,    81,    82,    83,   219,     0,     0,     0,   134,    85,
     135,   220,   221,   222,   223,     0,   224,     0,     0,   225,
     226,     0,     0,     0,     5,     6,     7,     8,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    86,     0,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,     0,     0,     0,    87,
       0,     0,     0,    88,     0,     0,     0,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    54,    55,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    89,    90,   227,     0,
      91,     0,     0,     0,   228,   229,   230,   231,    83,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   232,
     233,     4,     5,     6,     7,     8,   211,   212,   213,     0,
     214,   215,   216,   217,   218,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,     0,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,     0,     0,     0,     0,     0,
      77,    78,    79,    80,    81,    82,    83,   219,     0,     0,
       0,   134,    85,   135,   220,   221,   222,   223,     0,   224,
       0,     0,   225,   226,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      86,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    87,     0,     0,     0,    88,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    89,
      90,   227,     0,     0,     0,     0,     0,   228,   229,   230,
     231,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,   232,   145,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,     0,     0,     0,     0,     0,   193,
     194,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,     0,     0,     0,     0,     0,    77,
      78,    79,    80,    81,    82,    83,     5,     6,     7,     8,
       0,    85,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,     0,     0,
       0,     0,     0,     0,   193,   194,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
       0,     0,     0,     0,    77,    78,    79,    80,    81,    82,
      83,     5,     6,     7,     8,     0,    85,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,   195,     0,     0,     0,     0,     0,   193,
     194,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,     0,     0,     0,     0,     0,    77,
      78,    79,    80,    81,    82,    83,     5,     6,     7,     8,
       0,    85,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,   320,     0,
       0,     0,     0,     0,   193,   194,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
       0,     0,     0,     0,    77,    78,    79,    80,    81,    82,
      83,     5,     6,     7,     8,     0,    85,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,   328,     0,     0,     0,     0,     0,   193,
     194,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,     0,     0,     0,     0,     0,    77,
      78,    79,    80,    81,    82,    83,     5,     6,     7,     8,
       0,    85,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,   330,     0,
       0,     0,     0,     0,   193,   194,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
       0,     0,     0,     0,    77,    78,    79,    80,    81,    82,
      83,     5,     6,     7,     8,     0,    85,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,   437,     0,     0,     0,     0,     0,   193,
     194,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,     0,     0,     0,     0,     0,    77,
      78,    79,    80,    81,    82,    83,     5,     6,     7,     8,
       0,    85,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,   438,     0,
       0,     0,     0,     0,   193,   194,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,     0,
       0,     0,     0,     0,    77,    78,    79,    80,    81,    82,
      83,     0,     0,     0,     0,     0,    85,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     4,     5,
       6,     7,     8,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   508,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,     0,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,     0,     0,     0,     0,     0,    77,    78,    79,
      80,    81,    82,    83,     0,     0,     0,     0,   134,    85,
     135,   220,   221,   222,   223,     0,   224,     0,   545,   225,
     226,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    86,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    87,
       0,     0,     0,    88,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    89,    90,   227,     0,
       0,     0,     0,     0,   228,   229,   230,   231,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,     0,   232,
       0,     0,     0,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,     0,     0,     0,     0,     0,    77,    78,    79,    80,
      81,    82,    83,     0,     0,     0,     0,   134,    85,   135,
     220,   221,   222,   223,     0,   224,     0,     0,   225,   226,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   227,     0,     0,
       0,     0,     0,   228,   229,   230,   231,     5,     6,     7,
       8,     0,     0,     0,     0,     0,     0,     0,     0,   419,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
       0,     0,     0,     0,     0,    77,    78,    79,    80,    81,
      82,    83,     0,     0,     0,     0,   134,    85,   135,   220,
     221,   222,   223,     0,   224,     0,     0,   225,   226,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   227,     0,     0,     0,
       0,     0,   228,   229,   230,   231,     4,     5,     6,     7,
       8,     0,     0,     0,     0,     0,     0,   344,     0,     0,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
       0,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
       0,     0,     0,     0,     0,    77,    78,    79,    80,    81,
      82,    83,     0,     0,     0,     0,   134,    85,   135,   220,
     221,   222,   223,     0,   224,     0,     0,   225,   226,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   121,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    88,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   350,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    89,    90,   227,     0,     0,     0,
       0,     0,   228,   229,   230,   231,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,     0,     0,     0,     0,
       0,    77,    78,    79,    80,    81,    82,    83,     0,     0,
       0,     0,   134,    85,   135,   220,   221,   222,   223,     0,
     224,     0,     0,   225,   226,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     8,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
       0,     0,   227,     0,     0,     0,     0,     0,   228,   229,
     230,   231,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,     0,     0,     0,     0,     0,    77,    78,    79,
      80,    81,    82,    83,     0,     0,     0,     0,   134,    85,
     135,   220,   221,   222,   223,     0,   224,     0,     0,   225,
     226,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     8,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,     0,   227,     0,
       0,   300,     0,     0,   228,   229,   230,   231,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,     0,     0,
       0,     0,     0,    77,    78,    79,    80,    81,    82,    83,
       0,     0,     0,     0,   134,    85,   135,   220,   221,   222,
     223,     0,   224,     0,     0,   225,   226,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,   227,     0,     0,   305,     0,     0,
     228,   229,   230,   231,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,     0,     0,     0,     0,     0,    77,
      78,    79,    80,    81,    82,    83,     0,     0,     0,     0,
     134,    85,   135,   220,   221,   222,   223,   333,   224,     0,
       0,   225,   226,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       8,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,     0,
     227,     0,     0,     0,     0,     0,   228,   229,   230,   231,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
       0,     0,     0,     0,     0,    77,    78,    79,    80,    81,
      82,    83,     0,     0,     0,     0,   134,    85,   135,   220,
     221,   222,   223,     0,   224,     0,     0,   225,   226,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     8,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,   227,     0,     0,   417,
       0,     0,   228,   229,   230,   231,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,     0,     0,     0,     0,
       0,    77,    78,    79,    80,    81,    82,    83,     0,     0,
       0,     0,   134,    85,   135,   220,   221,   222,   223,     0,
     224,     0,     0,   225,   226,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     8,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
       0,     0,   227,     0,     0,   491,     0,     0,   228,   229,
     230,   231,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,     0,     0,     0,     0,     0,    77,    78,    79,
      80,    81,    82,    83,     0,     0,     0,     0,   134,    85,
     135,   220,   221,   222,   223,     0,   224,     0,     0,   225,
     226,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     8,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,     0,   227,     0,
       0,     0,     0,     0,   228,   229,   230,   231,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,     0,     0,
       0,     0,     0,    77,    78,    79,    80,    81,    82,   362,
       0,     0,     0,     0,   134,    85,   135,   220,   221,   222,
     223,     0,   224,     0,     0,   225,   226,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     8,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,   227,     0,     0,     0,     0,     0,
     228,   229,   230,   231,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,     0,     0,     0,     0,     0,    77,
      78,    79,    80,    81,    82,   530,     0,     0,     0,     0,
     134,    85,   135,   220,   221,   222,   223,     0,   224,     0,
       0,   225,   226,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     4,     5,     6,     7,     8,     0,
     227,     0,     0,     0,     0,     0,   228,   229,   230,   231,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,     0,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,     0,     0,
       0,     0,     0,    77,    78,    79,    80,    81,    82,    83,
       0,    84,     0,     0,     0,    85,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    86,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     4,     5,     6,     7,
       8,     0,     0,     0,     0,    87,     0,     0,     0,    88,
       0,     0,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
       0,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,     0,     0,
       0,     0,    89,    90,     0,     0,    91,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
       0,     0,     0,     0,     0,    77,    78,    79,    80,    81,
      82,    83,     0,     0,     0,     0,     0,    85,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   121,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     4,     5,
       6,     7,     8,     0,     0,     0,     0,    87,     0,     0,
       0,    88,     0,     0,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,     0,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
       0,     0,     0,     0,    89,    90,     0,     0,    91,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,     0,     0,     0,     0,     0,    77,    78,    79,
      80,    81,    82,    83,     0,     0,     0,     0,     0,    85,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   121,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   149,     5,
       6,     7,     8,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    88,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
     150,   151,   152,   153,     0,     0,     0,   154,   155,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
       0,     0,     0,     0,     0,     0,    89,    90,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,   156,   157,   158,   159,   160,    77,    78,    79,
      80,    81,    82,    83,   149,     5,     6,     7,     8,    85,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,     0,     0,   153,
       0,     0,     0,   154,   155,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,     0,     0,
       0,     0,     0,    77,    78,    79,    80,    81,    82,    83,
       5,     6,     7,     8,     0,    85,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,   150,   151,   152,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,   156,   157,   158,   159,   160,    77,    78,
      79,    80,    81,    82,    83,     5,     6,     7,     8,     0,
      85,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,     0,     0,     0,
       0,     0,     0,   176,     0,     0,     0,     0,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,     0,     0,
       0,     0,     0,    77,    78,    79,    80,    81,    82,    83,
       5,     6,     7,     8,     0,    85,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,     0,     0,     0,     0,     0,    77,    78,
      79,    80,    81,    82,    83,     0,     0,     0,     0,     0,
      85
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-273))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       2,   213,    87,    82,   276,    84,    86,   100,   217,   198,
       3,    86,    91,   117,   163,     2,   165,    97,   227,   203,
     204,   100,   113,   114,   115,   100,    17,    18,    17,    18,
       2,   131,   117,   225,   126,   127,   178,   179,     2,   227,
       0,   121,   163,    91,     2,   299,   124,   125,   225,   168,
     242,   131,   225,   225,   178,   179,   236,    86,   270,   225,
      94,     2,   225,   228,   139,   207,   243,   209,   148,   242,
     243,   243,   101,   228,   167,   155,   236,   243,   126,   127,
     243,   246,   228,   163,     3,    87,   224,   224,   167,   244,
      92,   228,   121,   228,   228,    97,   228,   228,   244,   242,
     102,   223,   243,   228,   226,    92,   236,   200,   137,   244,
     185,   228,   244,   244,   194,   117,   242,   242,   198,   245,
      92,   200,   228,   203,   204,   242,   244,   245,    92,   131,
     117,   237,   180,   342,    92,   284,   225,   346,   347,   288,
     329,   241,   331,   145,   236,   237,   148,   225,   357,   227,
     184,    92,   225,   295,   245,   296,   298,     3,   299,   413,
     414,   163,   350,   165,   224,   419,   236,   288,   228,    35,
      36,   295,   276,    39,   298,   224,   245,   224,   224,   228,
     224,   228,   228,   236,   228,   223,     3,   396,   226,    35,
      36,   236,    38,    39,    40,    41,   198,    43,   277,   201,
     224,   203,   204,   224,   228,   224,    42,   228,   288,   228,
     242,   213,   112,   113,   114,   115,   229,   230,    35,    36,
     361,    38,    39,    40,    41,    42,    43,    44,    45,    46,
     224,   224,   225,   227,    36,   376,   445,   385,   386,   387,
     388,   495,   122,   123,   456,   113,   114,   115,   520,   329,
      35,   331,   436,   128,   129,   246,   397,   246,   113,   408,
     115,    43,   347,   111,   178,   179,   223,   409,   270,   411,
     412,   223,   413,   414,   276,   223,   548,   114,   419,   358,
     534,   114,   284,   358,   538,   409,   288,   411,   412,   431,
     113,   114,   115,   207,   114,   209,   383,   384,   440,   553,
      44,    45,    46,   245,   513,   517,   237,   431,   233,   234,
     235,   225,   226,   381,   382,   389,   390,   126,   127,   237,
     509,   237,   168,   464,   237,   142,   237,   329,   237,   331,
      43,   410,   246,   114,   223,   223,   244,   244,   547,   341,
     223,   434,   244,   246,   223,   347,   223,   223,   238,   240,
     224,   168,   354,   432,   495,   434,   436,   239,   570,   223,
     445,   130,   571,   575,   576,   223,   244,   223,   225,   228,
     237,   450,   244,   226,   226,   221,   222,   228,   109,   224,
     592,   295,   245,   115,   298,   226,   226,   117,   529,   243,
     117,   532,   117,   534,   226,   226,   223,   538,   228,   224,
     224,   243,   224,   224,   221,   222,   408,   226,   223,   237,
     226,   237,   553,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   237,   223,   226,   244,   224,   224,   509,
     115,   243,   245,   115,   436,   242,   242,    11,   224,   244,
     115,   224,   242,   445,   391,   394,   392,   148,   393,   165,
     288,   395,    31,   434,   456,   341,   354,   341,   548,   445,
     575,   577,    92,   440,   378,   379,   380,   381,   382,   383,
     384,   385,   386,   387,   388,   389,   390,   391,   392,   393,
     394,   395,   561,   276,    -1,   209,   565,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   409,    -1,   411,   412,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   509,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   517,    -1,   431,   520,    -1,
      -1,    -1,     3,    -1,   243,    -1,   440,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   455,    -1,    -1,    -1,     3,   548,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    35,    36,    -1,    38,    39,    40,
      41,    -1,    43,    44,    45,    46,    -1,    -1,   570,    -1,
      -1,    -1,    -1,   575,   576,    -1,    -1,    35,    36,    -1,
      38,    39,    40,    41,    -1,    43,    44,    45,    46,    -1,
     592,    -1,    -1,     3,     4,     5,     6,     7,     8,     9,
      10,    -1,    12,    13,    14,    15,    16,   521,   522,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    -1,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,   113,    -1,   115,    -1,    -1,
      -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,   168,    -1,    -1,
      -1,    -1,   102,   103,   104,   105,   106,   107,   108,   109,
      -1,    -1,    -1,   113,   114,   115,   116,   117,   118,   119,
     168,   121,    -1,    -1,   124,   125,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   142,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     221,   222,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   164,    -1,    -1,    -1,   168,    -1,
      -1,    -1,    -1,   221,   222,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   221,   222,   223,    -1,   225,    -1,    -1,    -1,   229,
     230,   231,   232,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   244,   245,   246,     3,     4,     5,
       6,     7,     8,     9,    10,    -1,    12,    13,    14,    15,
      16,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    -1,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
     106,   107,   108,   109,    -1,    -1,    -1,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,    -1,    -1,   124,   125,
      -1,    -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   142,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    -1,   164,    -1,
      -1,    -1,   168,    -1,    -1,    -1,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    74,    75,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   221,   222,   223,    -1,   225,
      -1,    -1,    -1,   229,   230,   231,   232,   108,    -1,    -1,
      -1,    -1,   113,   114,   115,    -1,    -1,    -1,   244,   245,
     246,     3,     4,     5,     6,     7,     8,     9,    10,    -1,
      12,    13,    14,    15,    16,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    -1,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,
     102,   103,   104,   105,   106,   107,   108,   109,    -1,    -1,
      -1,   113,   114,   115,   116,   117,   118,   119,    -1,   121,
      -1,    -1,   124,   125,    -1,    -1,    -1,    -1,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     142,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    -1,   164,    -1,    -1,    -1,   168,    -1,    -1,    -1,
      -1,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,    75,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   221,
     222,   223,    -1,   225,    -1,    -1,    -1,   229,   230,   231,
     232,   107,   108,    -1,    -1,    -1,    -1,    -1,   114,    -1,
      -1,    -1,   244,   245,   246,     3,     4,     5,     6,     7,
       8,     9,    10,    -1,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    -1,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,   107,
     108,   109,    -1,    -1,    -1,   113,   114,   115,   116,   117,
     118,   119,    -1,   121,    -1,    -1,   124,   125,    -1,    -1,
      -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   142,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,    -1,    -1,   164,    -1,    -1,    -1,
     168,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    74,    75,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   221,   222,   223,    -1,   225,    -1,    -1,
      -1,   229,   230,   231,   232,   107,   108,    -1,    -1,    -1,
      -1,    -1,   114,    -1,    -1,    -1,   244,   245,     3,     4,
       5,     6,     7,     8,     9,    10,    -1,    12,    13,    14,
      15,    16,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,
     105,   106,   107,   108,   109,    -1,    -1,    -1,   113,   114,
     115,   116,   117,   118,   119,    -1,   121,    -1,    -1,   124,
     125,    -1,    -1,    -1,     4,     5,     6,     7,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   142,    -1,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    -1,    -1,    -1,    -1,   164,
      -1,    -1,    -1,   168,    -1,    -1,    -1,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    74,    75,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   221,   222,   223,    -1,
     225,    -1,    -1,    -1,   229,   230,   231,   232,   108,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   244,
     245,     3,     4,     5,     6,     7,     8,     9,    10,    -1,
      12,    13,    14,    15,    16,    -1,    -1,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    -1,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,
     102,   103,   104,   105,   106,   107,   108,   109,    -1,    -1,
      -1,   113,   114,   115,   116,   117,   118,   119,    -1,   121,
      -1,    -1,   124,   125,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     142,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   164,    -1,    -1,    -1,   168,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   221,
     222,   223,    -1,    -1,    -1,    -1,    -1,   229,   230,   231,
     232,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   244,   245,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,
     103,   104,   105,   106,   107,   108,     4,     5,     6,     7,
      -1,   114,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,   107,
     108,     4,     5,     6,     7,    -1,   114,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,   246,    -1,    -1,    -1,    -1,    -1,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,
     103,   104,   105,   106,   107,   108,     4,     5,     6,     7,
      -1,   114,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,   246,    -1,
      -1,    -1,    -1,    -1,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,   107,
     108,     4,     5,     6,     7,    -1,   114,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,   246,    -1,    -1,    -1,    -1,    -1,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,
     103,   104,   105,   106,   107,   108,     4,     5,     6,     7,
      -1,   114,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,   246,    -1,
      -1,    -1,    -1,    -1,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,   107,
     108,     4,     5,     6,     7,    -1,   114,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,   246,    -1,    -1,    -1,    -1,    -1,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,
     103,   104,   105,   106,   107,   108,     4,     5,     6,     7,
      -1,   114,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,   246,    -1,
      -1,    -1,    -1,    -1,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    -1,
      -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,   107,
     108,    -1,    -1,    -1,    -1,    -1,   114,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   246,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,
     105,   106,   107,   108,    -1,    -1,    -1,    -1,   113,   114,
     115,   116,   117,   118,   119,    -1,   121,    -1,   246,   124,
     125,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   142,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   164,
      -1,    -1,    -1,   168,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   221,   222,   223,    -1,
      -1,    -1,    -1,    -1,   229,   230,   231,   232,     4,     5,
       6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   244,
      -1,    -1,    -1,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,
     106,   107,   108,    -1,    -1,    -1,    -1,   113,   114,   115,
     116,   117,   118,   119,    -1,   121,    -1,    -1,   124,   125,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   223,    -1,    -1,
      -1,    -1,    -1,   229,   230,   231,   232,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   245,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,
     107,   108,    -1,    -1,    -1,    -1,   113,   114,   115,   116,
     117,   118,   119,    -1,   121,    -1,    -1,   124,   125,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   223,    -1,    -1,    -1,
      -1,    -1,   229,   230,   231,   232,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,    -1,    -1,   244,    -1,    -1,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      -1,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,
     107,   108,    -1,    -1,    -1,    -1,   113,   114,   115,   116,
     117,   118,   119,    -1,   121,    -1,    -1,   124,   125,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   142,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   168,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,   221,   222,   223,    -1,    -1,    -1,
      -1,    -1,   229,   230,   231,   232,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    -1,    -1,    -1,    -1,
      -1,   102,   103,   104,   105,   106,   107,   108,    -1,    -1,
      -1,    -1,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,    -1,    -1,   124,   125,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      -1,    -1,   223,    -1,    -1,    -1,    -1,    -1,   229,   230,
     231,   232,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,
     105,   106,   107,   108,    -1,    -1,    -1,    -1,   113,   114,
     115,   116,   117,   118,   119,    -1,   121,    -1,    -1,   124,
     125,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     4,     5,     6,     7,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,   223,    -1,
      -1,   226,    -1,    -1,   229,   230,   231,   232,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,   102,   103,   104,   105,   106,   107,   108,
      -1,    -1,    -1,    -1,   113,   114,   115,   116,   117,   118,
     119,    -1,   121,    -1,    -1,   124,   125,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,   223,    -1,    -1,   226,    -1,    -1,
     229,   230,   231,   232,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,
     103,   104,   105,   106,   107,   108,    -1,    -1,    -1,    -1,
     113,   114,   115,   116,   117,   118,   119,   120,   121,    -1,
      -1,   124,   125,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
     223,    -1,    -1,    -1,    -1,    -1,   229,   230,   231,   232,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,
     107,   108,    -1,    -1,    -1,    -1,   113,   114,   115,   116,
     117,   118,   119,    -1,   121,    -1,    -1,   124,   125,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     4,     5,     6,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,   223,    -1,    -1,   226,
      -1,    -1,   229,   230,   231,   232,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    -1,    -1,    -1,    -1,
      -1,   102,   103,   104,   105,   106,   107,   108,    -1,    -1,
      -1,    -1,   113,   114,   115,   116,   117,   118,   119,    -1,
     121,    -1,    -1,   124,   125,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      -1,    -1,   223,    -1,    -1,   226,    -1,    -1,   229,   230,
     231,   232,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,
     105,   106,   107,   108,    -1,    -1,    -1,    -1,   113,   114,
     115,   116,   117,   118,   119,    -1,   121,    -1,    -1,   124,
     125,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     4,     5,     6,     7,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,   223,    -1,
      -1,    -1,    -1,    -1,   229,   230,   231,   232,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,   102,   103,   104,   105,   106,   107,   108,
      -1,    -1,    -1,    -1,   113,   114,   115,   116,   117,   118,
     119,    -1,   121,    -1,    -1,   124,   125,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     4,     5,     6,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,   223,    -1,    -1,    -1,    -1,    -1,
     229,   230,   231,   232,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,
     103,   104,   105,   106,   107,   108,    -1,    -1,    -1,    -1,
     113,   114,   115,   116,   117,   118,   119,    -1,   121,    -1,
      -1,   124,   125,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,    -1,
     223,    -1,    -1,    -1,    -1,    -1,   229,   230,   231,   232,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    -1,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,   102,   103,   104,   105,   106,   107,   108,
      -1,   110,    -1,    -1,    -1,   114,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   142,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    -1,   164,    -1,    -1,    -1,   168,
      -1,    -1,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      -1,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    -1,    -1,
      -1,    -1,   221,   222,    -1,    -1,   225,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      -1,    -1,    -1,    -1,    -1,   102,   103,   104,   105,   106,
     107,   108,    -1,    -1,    -1,    -1,    -1,   114,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   142,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,   164,    -1,    -1,
      -1,   168,    -1,    -1,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      -1,    -1,    -1,    -1,   221,   222,    -1,    -1,   225,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    -1,    -1,    -1,    -1,    -1,   102,   103,   104,
     105,   106,   107,   108,    -1,    -1,    -1,    -1,    -1,   114,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   142,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   168,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    -1,    -1,    -1,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      -1,    -1,    -1,    -1,    -1,    -1,   221,   222,    -1,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,     3,     4,     5,     6,     7,   114,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,    -1,    38,
      -1,    -1,    -1,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,   102,   103,   104,   105,   106,   107,   108,
       4,     5,     6,     7,    -1,   114,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,     4,     5,     6,     7,    -1,
     114,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    42,    -1,    -1,    -1,    -1,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    -1,    -1,
      -1,    -1,    -1,   102,   103,   104,   105,   106,   107,   108,
       4,     5,     6,     7,    -1,   114,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    -1,    -1,    -1,    -1,    -1,   102,   103,
     104,   105,   106,   107,   108,    -1,    -1,    -1,    -1,    -1,
     114
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   248,   249,     0,     3,     4,     5,     6,     7,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,   102,   103,   104,
     105,   106,   107,   108,   110,   114,   142,   164,   168,   221,
     222,   225,   251,   284,   286,   287,   288,   289,   294,   295,
     296,   297,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   311,   344,   347,   348,   349,   353,
     168,   142,   299,    35,    36,    39,   236,   236,   113,   114,
     115,   245,   250,   250,   113,   115,   252,   297,   300,   296,
       3,   250,   305,   344,   244,   245,   327,   224,   228,     3,
      35,    36,    37,    38,    42,    43,    97,    98,    99,   100,
     101,   290,   291,   292,   297,   298,   301,   228,   244,   242,
     113,   115,   250,   252,   318,   300,    42,   301,   225,   225,
     236,   236,   236,   236,   286,   296,   347,   114,   305,   310,
     114,   305,   310,    42,    43,   246,   297,   301,   312,   313,
     314,   315,   242,   245,   245,   300,   252,   223,   226,   223,
     226,     8,     9,    10,    12,    13,    14,    15,    16,   109,
     116,   117,   118,   119,   121,   124,   125,   223,   229,   230,
     231,   232,   244,   245,   246,   252,   253,   254,   256,   257,
     258,   259,   260,   261,   262,   267,   268,   269,   270,   271,
     272,   273,   274,   275,   276,   277,   278,   279,   280,   282,
     284,   285,   286,   294,   295,   301,   321,   322,   323,   324,
     328,   329,   330,   333,   339,   343,   348,   242,   291,    36,
      35,   297,   290,   293,   298,   301,    43,   290,   292,   250,
     318,   250,   318,   111,   350,   225,   243,   223,   225,   243,
     226,   267,   279,   283,   301,   226,   283,   305,   114,   114,
     114,   327,   237,   237,   237,   237,   237,   237,   297,    43,
     246,   313,   250,   316,   317,   318,   301,   114,   246,   312,
     246,   312,   223,   120,   283,   345,   346,   346,   244,   244,
     322,   223,   223,   244,   244,   282,   223,   223,   267,   267,
       3,   282,   304,   246,   325,   124,   125,   225,   227,   224,
     224,   228,   108,   280,   223,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   243,   281,   267,   233,   234,
     235,   229,   230,   122,   123,   126,   127,   236,   237,   128,
     129,   238,   239,   240,   130,   131,   241,   228,   244,   244,
     246,   322,   323,   324,   250,   290,   290,   293,   298,   225,
     242,   243,   225,   243,   243,   223,   244,   226,   283,   245,
     280,   319,   283,   319,   226,   226,   228,   237,   237,   228,
     228,   225,   242,   352,   228,   244,   245,   246,   246,   224,
     228,   224,   109,   321,   329,   340,   282,   244,   282,   282,
     296,   332,   304,   224,     3,   224,   328,   255,   282,   250,
     252,   263,   264,   265,   266,   280,   280,   267,   267,   267,
     269,   269,   270,   270,   271,   271,   271,   271,   272,   272,
     273,   274,   275,   276,   277,   282,   280,   290,   283,   250,
     283,   226,   319,   319,   115,   243,   226,   319,   320,   226,
     117,   117,   117,   283,   112,   250,   351,   317,   246,   312,
     226,   345,   226,   223,   332,   341,   342,   224,   224,   250,
     224,   224,   224,   267,   246,   226,   223,   224,   224,   228,
     108,   280,   242,   226,   243,   224,   228,   319,   228,   246,
     237,   237,   237,   226,   223,   246,   282,   244,   224,   322,
     331,   245,   334,   243,   323,   326,   327,   267,   267,   280,
     280,   242,   319,   115,   319,   242,   115,   224,   282,   326,
      11,    17,    18,   246,   335,   336,   337,   338,   319,   250,
     224,   250,   224,   227,   244,   322,   282,   242,   322,   335,
     322,   246,   337,   115,   242,   224
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (&yylloc, state, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, &yylloc, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, &yylloc, scanner)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, Location, state); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
  YYUSE (state);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp, state)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, state);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, struct _mesa_glsl_parse_state *state)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule, state)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
    struct _mesa_glsl_parse_state *state;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       , state);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule, state); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, struct _mesa_glsl_parse_state *state)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp, state)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
    struct _mesa_glsl_parse_state *state;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (state);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (struct _mesa_glsl_parse_state *state);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (struct _mesa_glsl_parse_state *state)
#else
int
yyparse (state)
    struct _mesa_glsl_parse_state *state;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Location data for the lookahead symbol.  */
YYLTYPE yylloc;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.
       `yyls': related to locations.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;

#if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 1;
#endif

/* User initialization code.  */

/* Line 1590 of yacc.c  */
#line 53 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
{
   yylloc.first_line = 1;
   yylloc.first_column = 1;
   yylloc.last_line = 1;
   yylloc.last_column = 1;
   yylloc.source_file = 0;
}

/* Line 1590 of yacc.c  */
#line 3424 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
  yylsp[0] = yylloc;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);

	yyls = yyls1;
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
	YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;
  *++yylsp = yylloc;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1806 of yacc.c  */
#line 282 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   _mesa_glsl_initialize_types(state);
	}
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 286 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   delete state->symbols;
	   state->symbols = new(ralloc_parent(state)) glsl_symbol_table;
	   _mesa_glsl_initialize_types(state);
	}
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 301 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   /* FINISHME: The NULL test is required because pragmas are set to
		* FINISHME: NULL. (See production rule for external_declaration.)
		*/
	   if ((yyvsp[(1) - (1)].node) != NULL)
		  state->translation_unit.push_tail(& (yyvsp[(1) - (1)].node)->link);
	}
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 309 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   /* FINISHME: The NULL test is required because pragmas are set to
		* FINISHME: NULL. (See production rule for external_declaration.)
		*/
	   if ((yyvsp[(2) - (2)].node) != NULL)
		  state->translation_unit.push_tail(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 325 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_identifier, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.identifier = (yyvsp[(1) - (1)].identifier);
	}
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 332 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_int_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.int_constant = (yyvsp[(1) - (1)].n);
	}
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 339 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(state->bGenerateES ? ast_int_constant : ast_uint_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   if (state->bGenerateES)
	   {
		   (yyval.expression)->primary_expression.int_constant = (yyvsp[(1) - (1)].n);
	   }
	   else
	   {
		   (yyval.expression)->primary_expression.uint_constant = (yyvsp[(1) - (1)].n);
	   }
	}
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 353 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_float_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.float_constant = (yyvsp[(1) - (1)].real);
	}
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 360 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_bool_constant, NULL, NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.bool_constant = (yyvsp[(1) - (1)].n);
	}
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 367 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(2) - (3)].expression);
	}
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 375 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_array_index, (yyvsp[(1) - (4)].expression), (yyvsp[(3) - (4)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 381 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (1)].expression);
	}
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 385 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_field_selection, (yyvsp[(1) - (3)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->primary_expression.identifier = (yyvsp[(3) - (3)].identifier);
	}
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 392 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_post_inc, (yyvsp[(1) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 398 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_post_dec, (yyvsp[(1) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 416 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_field_selection, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 435 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (2)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(2) - (2)].expression)->link);
	}
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 441 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 457 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_function_expression((yyvsp[(1) - (1)].type_specifier));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 463 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (1)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 470 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (1)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 490 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (2)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(2) - (2)].expression)->link);
	}
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 496 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   (yyval.expression)->set_location(yylloc);
	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 508 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_expression *callee = new(ctx) ast_expression((yyvsp[(1) - (2)].identifier));
	   (yyval.expression) = new(ctx) ast_function_expression(callee);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 520 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_pre_inc, (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 526 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_pre_dec, (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 47:

/* Line 1806 of yacc.c  */
#line 532 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression((yyvsp[(1) - (2)].n), (yyvsp[(2) - (2)].expression), NULL, NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 538 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_expression(ast_type_cast, (yyvsp[(4) - (4)].expression), NULL, NULL);
		(yyval.expression)->primary_expression.type_specifier = (yyvsp[(2) - (4)].type_specifier);
		(yyval.expression)->set_location(yylloc);
	}
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 545 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_expression(ast_type_cast, (yyvsp[(5) - (5)].expression), NULL, NULL);
		(yyval.expression)->primary_expression.type_specifier = (yyvsp[(3) - (5)].type_specifier);
		(yyval.expression)->set_location(yylloc);
	}
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 552 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_expression(ast_type_cast, (yyvsp[(5) - (5)].expression), NULL, NULL);
		(yyval.expression)->primary_expression.type_specifier = (yyvsp[(2) - (5)].type_specifier);
		(yyval.expression)->set_location(yylloc);
	}
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 562 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_plus; }
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 563 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_neg; }
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 564 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_logic_not; }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 565 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_bit_not; }
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 571 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_mul, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 577 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_div, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 583 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_mod, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 593 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_add, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 599 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_sub, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 609 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_lshift, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 615 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_rshift, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 625 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_less, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 631 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_greater, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 68:

/* Line 1806 of yacc.c  */
#line 637 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_lequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 643 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_gequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 653 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_equal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 659 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_nequal, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 669 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_and, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 679 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_xor, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 689 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_or, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 699 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_and, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 709 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_or, (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 719 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression(ast_conditional, (yyvsp[(1) - (5)].expression), (yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].expression));
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 729 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.expression) = new(ctx) ast_expression((yyvsp[(2) - (3)].n), (yyvsp[(1) - (3)].expression), (yyvsp[(3) - (3)].expression), NULL);
	   (yyval.expression)->set_location(yylloc);
	}
    break;

  case 87:

/* Line 1806 of yacc.c  */
#line 737 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_assign; }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 738 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_mul_assign; }
    break;

  case 89:

/* Line 1806 of yacc.c  */
#line 739 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_div_assign; }
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 740 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_mod_assign; }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 741 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_add_assign; }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 742 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_sub_assign; }
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 743 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_ls_assign; }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 744 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_rs_assign; }
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 745 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_and_assign; }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 746 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_xor_assign; }
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 747 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.n) = ast_or_assign; }
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 752 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.expression) = (yyvsp[(1) - (1)].expression);
	}
    break;

  case 99:

/* Line 1806 of yacc.c  */
#line 756 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   if ((yyvsp[(1) - (3)].expression)->oper != ast_sequence) {
		  (yyval.expression) = new(ctx) ast_expression(ast_sequence, NULL, NULL, NULL);
		  (yyval.expression)->set_location(yylloc);
		  (yyval.expression)->expressions.push_tail(& (yyvsp[(1) - (3)].expression)->link);
	   } else {
		  (yyval.expression) = (yyvsp[(1) - (3)].expression);
	   }

	   (yyval.expression)->expressions.push_tail(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 776 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   state->symbols->pop_scope();
	   (yyval.function) = (yyvsp[(1) - (2)].function);
	}
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 784 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(1) - (1)].function);
	}
    break;

  case 103:

/* Line 1806 of yacc.c  */
#line 788 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (yyvsp[(1) - (2)].declarator_list);
	}
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 795 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	}
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 798 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.function) = (yyvsp[(1) - (4)].function);
		(yyval.function)->return_semantic = (yyvsp[(4) - (4)].identifier);
	}
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 811 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.function) = (yyvsp[(1) - (2)].function);
	   (yyval.function)->parameters.push_tail(& (yyvsp[(2) - (2)].parameter_declarator)->link);
	}
    break;

  case 109:

/* Line 1806 of yacc.c  */
#line 816 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.function) = (yyvsp[(1) - (3)].function);
	   (yyval.function)->parameters.push_tail(& (yyvsp[(3) - (3)].parameter_declarator)->link);
	}
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 824 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function) = new(ctx) ast_function();
	   (yyval.function)->set_location(yylloc);
	   (yyval.function)->return_type = (yyvsp[(1) - (3)].fully_specified_type);
	   (yyval.function)->identifier = (yyvsp[(2) - (3)].identifier);

	   state->symbols->add_function(new(state) ir_function((yyvsp[(2) - (3)].identifier)));
	   state->symbols->push_scope();
	}
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 835 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function) = new(ctx) ast_function();
	   (yyval.function)->set_location(yylloc);
	   (yyval.function)->return_type = (yyvsp[(2) - (4)].fully_specified_type);
	   (yyval.function)->identifier = (yyvsp[(3) - (4)].identifier);

	   state->symbols->add_function(new(state) ir_function((yyvsp[(3) - (4)].identifier)));
	   state->symbols->push_scope();
	}
    break;

  case 112:

/* Line 1806 of yacc.c  */
#line 849 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (2)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (2)].identifier);
	}
    break;

  case 113:

/* Line 1806 of yacc.c  */
#line 859 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
		(yyval.parameter_declarator)->set_location(yylloc);
		(yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
		(yyval.parameter_declarator)->type->set_location(yylloc);
		(yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (4)].type_specifier);
		(yyval.parameter_declarator)->identifier = (yyvsp[(2) - (4)].identifier);
		(yyval.parameter_declarator)->default_value = (yyvsp[(4) - (4)].expression);
	}
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 870 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
		(yyval.parameter_declarator)->set_location(yylloc);
		(yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
		(yyval.parameter_declarator)->type->set_location(yylloc);
		(yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (4)].type_specifier);
		(yyval.parameter_declarator)->identifier = (yyvsp[(2) - (4)].identifier);
		(yyval.parameter_declarator)->semantic = (yyvsp[(4) - (4)].identifier);
	}
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 881 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (2)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (2)].declaration)->identifier;
	   (yyval.parameter_declarator)->is_array = true;
	   (yyval.parameter_declarator)->array_size = (yyvsp[(2) - (2)].declaration)->array_size;
	}
    break;

  case 116:

/* Line 1806 of yacc.c  */
#line 893 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->set_location(yylloc);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(1) - (7)].type_specifier);
	   (yyval.parameter_declarator)->identifier = (yyvsp[(2) - (7)].identifier);
	   (yyval.parameter_declarator)->is_array = true;
	   (yyval.parameter_declarator)->array_size = (yyvsp[(4) - (7)].expression);
	   (yyval.parameter_declarator)->semantic = (yyvsp[(7) - (7)].identifier);
	}
    break;

  case 117:

/* Line 1806 of yacc.c  */
#line 909 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(3) - (3)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	}
    break;

  case 118:

/* Line 1806 of yacc.c  */
#line 916 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(3) - (3)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	}
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 923 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyvsp[(1) - (4)].type_qualifier).flags.i |= (yyvsp[(2) - (4)].type_qualifier).flags.i;
	   (yyvsp[(1) - (4)].type_qualifier).flags.i |= (yyvsp[(3) - (4)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = (yyvsp[(4) - (4)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (4)].type_qualifier);
	}
    break;

  case 120:

/* Line 1806 of yacc.c  */
#line 931 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.parameter_declarator) = (yyvsp[(2) - (2)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 121:

/* Line 1806 of yacc.c  */
#line 936 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.parameter_declarator) = (yyvsp[(2) - (2)].parameter_declarator);
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 942 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyvsp[(1) - (3)].type_qualifier).flags.i |= (yyvsp[(2) - (3)].type_qualifier).flags.i;

	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (3)].type_qualifier);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(3) - (3)].type_specifier);
	}
    break;

  case 124:

/* Line 1806 of yacc.c  */
#line 953 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
	   (yyval.parameter_declarator)->set_location(yylloc);
	   (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
	   (yyval.parameter_declarator)->type->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.parameter_declarator)->type->specifier = (yyvsp[(2) - (2)].type_specifier);
	}
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 965 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 970 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 975 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 981 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.in = 1;
		(yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 987 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.in = 1;
		(yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 993 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_point = 1;
	}
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 998 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_line = 1;
	}
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 1003 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_triangle = 1;
	}
    break;

  case 133:

/* Line 1806 of yacc.c  */
#line 1008 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_lineadj = 1;
	}
    break;

  case 134:

/* Line 1806 of yacc.c  */
#line 1013 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.gs_triangleadj = 1;
	}
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 1026 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (3)].identifier), false, NULL, NULL);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (3)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (3)].identifier), ir_var_auto));
	}
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 1036 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (5)].identifier), true, NULL, NULL);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].identifier), ir_var_auto));
	}
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 1046 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(3) - (3)].declaration);

	   (yyval.declarator_list) = (yyvsp[(1) - (3)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (3)].declaration)->identifier, ir_var_auto));
	}
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 1055 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (7)].identifier), true, NULL, (yyvsp[(7) - (7)].expression));
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (7)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (7)].identifier), ir_var_auto));
	}
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 1065 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(3) - (5)].declaration);
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].declaration)->identifier, ir_var_auto));
	}
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 1075 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(3) - (5)].identifier), false, NULL, (yyvsp[(5) - (5)].expression));
	   decl->set_location(yylloc);

	   (yyval.declarator_list) = (yyvsp[(1) - (5)].declarator_list);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(3) - (5)].identifier), ir_var_auto));
	}
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 1089 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   /* Empty declaration list is valid. */
	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (1)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	}
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 1096 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (2)].identifier), false, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (2)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 1105 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), true, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 146:

/* Line 1806 of yacc.c  */
#line 1114 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(2) - (2)].declaration);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (2)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 147:

/* Line 1806 of yacc.c  */
#line 1123 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (6)].identifier), true, NULL, (yyvsp[(6) - (6)].expression));

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (6)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 148:

/* Line 1806 of yacc.c  */
#line 1132 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = (yyvsp[(2) - (4)].declaration);
	   decl->initializer = (yyvsp[(4) - (4)].expression);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 149:

/* Line 1806 of yacc.c  */
#line 1142 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), false, NULL, (yyvsp[(4) - (4)].expression));

	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 150:

/* Line 1806 of yacc.c  */
#line 1151 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (2)].identifier), false, NULL, NULL);

	   (yyval.declarator_list) = new(ctx) ast_declarator_list(NULL);
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->invariant = true;

	   (yyval.declarator_list)->declarations.push_tail(&decl->link);
	}
    break;

  case 151:

/* Line 1806 of yacc.c  */
#line 1165 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
	   (yyval.fully_specified_type)->set_location(yylloc);
	   (yyval.fully_specified_type)->specifier = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 152:

/* Line 1806 of yacc.c  */
#line 1172 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
	   (yyval.fully_specified_type)->set_location(yylloc);
	   (yyval.fully_specified_type)->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.fully_specified_type)->specifier = (yyvsp[(2) - (2)].type_specifier);
	}
    break;

  case 153:

/* Line 1806 of yacc.c  */
#line 1183 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.smooth = 1;
	}
    break;

  case 154:

/* Line 1806 of yacc.c  */
#line 1188 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.flat = 1;
	}
    break;

  case 155:

/* Line 1806 of yacc.c  */
#line 1193 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.noperspective = 1;
	}
    break;

  case 157:

/* Line 1806 of yacc.c  */
#line 1202 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 158:

/* Line 1806 of yacc.c  */
#line 1207 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 159:

/* Line 1806 of yacc.c  */
#line 1212 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 160:

/* Line 1806 of yacc.c  */
#line 1217 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.constant = 1;
	}
    break;

  case 161:

/* Line 1806 of yacc.c  */
#line 1222 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.uniform = 1;
	}
    break;

  case 162:

/* Line 1806 of yacc.c  */
#line 1227 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.precise = 1;
	}
    break;

  case 165:

/* Line 1806 of yacc.c  */
#line 1237 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.i |= (yyvsp[(2) - (2)].type_qualifier).flags.i;
	}
    break;

  case 166:

/* Line 1806 of yacc.c  */
#line 1242 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 167:

/* Line 1806 of yacc.c  */
#line 1247 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(2) - (3)].type_qualifier);
	   (yyval.type_qualifier).flags.i |= (yyvsp[(3) - (3)].type_qualifier).flags.i;
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 168:

/* Line 1806 of yacc.c  */
#line 1253 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.invariant = 1;
	}
    break;

  case 169:

/* Line 1806 of yacc.c  */
#line 1258 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.precise = 1;
	}
    break;

  case 170:

/* Line 1806 of yacc.c  */
#line 1263 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.q.precise = 1;
	}
    break;

  case 171:

/* Line 1806 of yacc.c  */
#line 1268 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
	   (yyval.type_qualifier).flags.q.precise = 1;
	}
    break;

  case 172:

/* Line 1806 of yacc.c  */
#line 1276 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.constant = 1;
	}
    break;

  case 173:

/* Line 1806 of yacc.c  */
#line 1281 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.varying = 1;
	}
    break;

  case 174:

/* Line 1806 of yacc.c  */
#line 1286 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1;
	   (yyval.type_qualifier).flags.q.varying = 1;
	}
    break;

  case 175:

/* Line 1806 of yacc.c  */
#line 1292 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 176:

/* Line 1806 of yacc.c  */
#line 1297 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 177:

/* Line 1806 of yacc.c  */
#line 1302 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1; (yyval.type_qualifier).flags.q.in = 1;
	}
    break;

  case 178:

/* Line 1806 of yacc.c  */
#line 1307 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.centroid = 1; (yyval.type_qualifier).flags.q.out = 1;
	}
    break;

  case 179:

/* Line 1806 of yacc.c  */
#line 1312 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
	   (yyval.type_qualifier).flags.q.uniform = 1;
	}
    break;

  case 180:

/* Line 1806 of yacc.c  */
#line 1317 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.row_major = 1;
	}
    break;

  case 181:

/* Line 1806 of yacc.c  */
#line 1322 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.column_major = 1;
	}
    break;

  case 182:

/* Line 1806 of yacc.c  */
#line 1327 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 183:

/* Line 1806 of yacc.c  */
#line 1332 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.constant = 1;
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 184:

/* Line 1806 of yacc.c  */
#line 1338 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.constant = 1;
		(yyval.type_qualifier).flags.q.is_static = 1;
	}
    break;

  case 185:

/* Line 1806 of yacc.c  */
#line 1344 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.coherent = 1;
	}
    break;

  case 186:

/* Line 1806 of yacc.c  */
#line 1349 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.shared = 1;
	}
    break;

  case 187:

/* Line 1806 of yacc.c  */
#line 1357 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 190:

/* Line 1806 of yacc.c  */
#line 1369 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (3)].type_specifier);
	   (yyval.type_specifier)->is_array = true;
	   (yyval.type_specifier)->is_unsized_array++;
	}
    break;

  case 191:

/* Line 1806 of yacc.c  */
#line 1375 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (3)].type_specifier);
	   (yyval.type_specifier)->is_unsized_array++;
	}
    break;

  case 192:

/* Line 1806 of yacc.c  */
#line 1380 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (4)].type_specifier);
	   (yyval.type_specifier)->is_array = true;
	   (yyval.type_specifier)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 193:

/* Line 1806 of yacc.c  */
#line 1386 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.type_specifier) = (yyvsp[(1) - (4)].type_specifier);
	   (yyvsp[(3) - (4)].expression)->link.next = &((yyval.type_specifier)->array_size->link);
	   (yyval.type_specifier)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 194:

/* Line 1806 of yacc.c  */
#line 1395 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 195:

/* Line 1806 of yacc.c  */
#line 1401 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier),"vec4");
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 196:

/* Line 1806 of yacc.c  */
#line 1407 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->texture_ms_num_samples = (yyvsp[(5) - (6)].n);
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 197:

/* Line 1806 of yacc.c  */
#line 1414 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (4)].identifier),(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 198:

/* Line 1806 of yacc.c  */
#line 1420 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("StructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 199:

/* Line 1806 of yacc.c  */
#line 1426 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("StructuredBuffer",(yyvsp[(3) - (4)].struct_specifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 200:

/* Line 1806 of yacc.c  */
#line 1432 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("StructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 201:

/* Line 1806 of yacc.c  */
#line 1438 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("RWStructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 202:

/* Line 1806 of yacc.c  */
#line 1444 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("RWStructuredBuffer",(yyvsp[(3) - (4)].struct_specifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 203:

/* Line 1806 of yacc.c  */
#line 1450 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier("RWStructuredBuffer",(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 204:

/* Line 1806 of yacc.c  */
#line 1456 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (4)].identifier),(yyvsp[(3) - (4)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 205:

/* Line 1806 of yacc.c  */
#line 1462 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
		(yyval.type_specifier)->patch_size = (yyvsp[(5) - (6)].n);
	}
    break;

  case 206:

/* Line 1806 of yacc.c  */
#line 1469 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (6)].identifier),(yyvsp[(3) - (6)].identifier));
		(yyval.type_specifier)->set_location(yylloc);
		(yyval.type_specifier)->patch_size = (yyvsp[(5) - (6)].n);
	}
    break;

  case 207:

/* Line 1806 of yacc.c  */
#line 1476 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].struct_specifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 208:

/* Line 1806 of yacc.c  */
#line 1482 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[(1) - (1)].identifier));
	   (yyval.type_specifier)->set_location(yylloc);
	}
    break;

  case 209:

/* Line 1806 of yacc.c  */
#line 1490 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "void"; }
    break;

  case 210:

/* Line 1806 of yacc.c  */
#line 1491 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "float"; }
    break;

  case 211:

/* Line 1806 of yacc.c  */
#line 1492 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half"; }
    break;

  case 212:

/* Line 1806 of yacc.c  */
#line 1493 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "int"; }
    break;

  case 213:

/* Line 1806 of yacc.c  */
#line 1494 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uint"; }
    break;

  case 214:

/* Line 1806 of yacc.c  */
#line 1495 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bool"; }
    break;

  case 215:

/* Line 1806 of yacc.c  */
#line 1496 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec2"; }
    break;

  case 216:

/* Line 1806 of yacc.c  */
#line 1497 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec3"; }
    break;

  case 217:

/* Line 1806 of yacc.c  */
#line 1498 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "vec4"; }
    break;

  case 218:

/* Line 1806 of yacc.c  */
#line 1499 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2"; }
    break;

  case 219:

/* Line 1806 of yacc.c  */
#line 1500 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3"; }
    break;

  case 220:

/* Line 1806 of yacc.c  */
#line 1501 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4"; }
    break;

  case 221:

/* Line 1806 of yacc.c  */
#line 1502 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec2"; }
    break;

  case 222:

/* Line 1806 of yacc.c  */
#line 1503 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec3"; }
    break;

  case 223:

/* Line 1806 of yacc.c  */
#line 1504 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "bvec4"; }
    break;

  case 224:

/* Line 1806 of yacc.c  */
#line 1505 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec2"; }
    break;

  case 225:

/* Line 1806 of yacc.c  */
#line 1506 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec3"; }
    break;

  case 226:

/* Line 1806 of yacc.c  */
#line 1507 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ivec4"; }
    break;

  case 227:

/* Line 1806 of yacc.c  */
#line 1508 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec2"; }
    break;

  case 228:

/* Line 1806 of yacc.c  */
#line 1509 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec3"; }
    break;

  case 229:

/* Line 1806 of yacc.c  */
#line 1510 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "uvec4"; }
    break;

  case 230:

/* Line 1806 of yacc.c  */
#line 1511 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2"; }
    break;

  case 231:

/* Line 1806 of yacc.c  */
#line 1512 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2x3"; }
    break;

  case 232:

/* Line 1806 of yacc.c  */
#line 1513 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat2x4"; }
    break;

  case 233:

/* Line 1806 of yacc.c  */
#line 1514 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3x2"; }
    break;

  case 234:

/* Line 1806 of yacc.c  */
#line 1515 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3"; }
    break;

  case 235:

/* Line 1806 of yacc.c  */
#line 1516 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat3x4"; }
    break;

  case 236:

/* Line 1806 of yacc.c  */
#line 1517 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4x2"; }
    break;

  case 237:

/* Line 1806 of yacc.c  */
#line 1518 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4x3"; }
    break;

  case 238:

/* Line 1806 of yacc.c  */
#line 1519 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "mat4"; }
    break;

  case 239:

/* Line 1806 of yacc.c  */
#line 1520 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x2"; }
    break;

  case 240:

/* Line 1806 of yacc.c  */
#line 1521 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x3"; }
    break;

  case 241:

/* Line 1806 of yacc.c  */
#line 1522 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half2x4"; }
    break;

  case 242:

/* Line 1806 of yacc.c  */
#line 1523 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x2"; }
    break;

  case 243:

/* Line 1806 of yacc.c  */
#line 1524 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x3"; }
    break;

  case 244:

/* Line 1806 of yacc.c  */
#line 1525 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half3x4"; }
    break;

  case 245:

/* Line 1806 of yacc.c  */
#line 1526 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x2"; }
    break;

  case 246:

/* Line 1806 of yacc.c  */
#line 1527 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x3"; }
    break;

  case 247:

/* Line 1806 of yacc.c  */
#line 1528 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "half4x4"; }
    break;

  case 248:

/* Line 1806 of yacc.c  */
#line 1529 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "samplerState"; }
    break;

  case 249:

/* Line 1806 of yacc.c  */
#line 1530 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "samplerComparisonState"; }
    break;

  case 250:

/* Line 1806 of yacc.c  */
#line 1565 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Buffer"; }
    break;

  case 251:

/* Line 1806 of yacc.c  */
#line 1566 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "ByteAddressBuffer"; }
    break;

  case 252:

/* Line 1806 of yacc.c  */
#line 1567 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture1D"; }
    break;

  case 253:

/* Line 1806 of yacc.c  */
#line 1568 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture1DArray"; }
    break;

  case 254:

/* Line 1806 of yacc.c  */
#line 1569 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2D"; }
    break;

  case 255:

/* Line 1806 of yacc.c  */
#line 1570 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DArray"; }
    break;

  case 256:

/* Line 1806 of yacc.c  */
#line 1571 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DMS"; }
    break;

  case 257:

/* Line 1806 of yacc.c  */
#line 1572 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureExternal"; }
    break;

  case 258:

/* Line 1806 of yacc.c  */
#line 1573 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture2DMSArray"; }
    break;

  case 259:

/* Line 1806 of yacc.c  */
#line 1574 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "Texture3D"; }
    break;

  case 260:

/* Line 1806 of yacc.c  */
#line 1575 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureCube"; }
    break;

  case 261:

/* Line 1806 of yacc.c  */
#line 1576 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TextureCubeArray"; }
    break;

  case 262:

/* Line 1806 of yacc.c  */
#line 1577 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWBuffer"; }
    break;

  case 263:

/* Line 1806 of yacc.c  */
#line 1578 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWByteAddressBuffer"; }
    break;

  case 264:

/* Line 1806 of yacc.c  */
#line 1579 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture1D"; }
    break;

  case 265:

/* Line 1806 of yacc.c  */
#line 1580 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture1DArray"; }
    break;

  case 266:

/* Line 1806 of yacc.c  */
#line 1581 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture2D"; }
    break;

  case 267:

/* Line 1806 of yacc.c  */
#line 1582 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture2DArray"; }
    break;

  case 268:

/* Line 1806 of yacc.c  */
#line 1583 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "RWTexture3D"; }
    break;

  case 269:

/* Line 1806 of yacc.c  */
#line 1587 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "PointStream"; }
    break;

  case 270:

/* Line 1806 of yacc.c  */
#line 1588 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "LineStream"; }
    break;

  case 271:

/* Line 1806 of yacc.c  */
#line 1589 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "TriangleStream"; }
    break;

  case 272:

/* Line 1806 of yacc.c  */
#line 1593 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "InputPatch"; }
    break;

  case 273:

/* Line 1806 of yacc.c  */
#line 1597 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.identifier) = "OutputPatch"; }
    break;

  case 274:

/* Line 1806 of yacc.c  */
#line 1602 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (5)].identifier), (yyvsp[(4) - (5)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	   state->symbols->add_type((yyvsp[(2) - (5)].identifier), glsl_type::void_type);
	}
    break;

  case 275:

/* Line 1806 of yacc.c  */
#line 1609 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (7)].identifier), (yyvsp[(4) - (7)].identifier), (yyvsp[(6) - (7)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	   state->symbols->add_type((yyvsp[(2) - (7)].identifier), glsl_type::void_type);
	}
    break;

  case 276:

/* Line 1806 of yacc.c  */
#line 1616 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.struct_specifier) = new(ctx) ast_struct_specifier(NULL, (yyvsp[(3) - (4)].node));
	   (yyval.struct_specifier)->set_location(yylloc);
	}
    break;

  case 277:

/* Line 1806 of yacc.c  */
#line 1622 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (4)].identifier),NULL);
		(yyval.struct_specifier)->set_location(yylloc);
		state->symbols->add_type((yyvsp[(2) - (4)].identifier), glsl_type::void_type);
	}
    break;

  case 278:

/* Line 1806 of yacc.c  */
#line 1629 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[(2) - (6)].identifier), (yyvsp[(4) - (6)].identifier), NULL);
		(yyval.struct_specifier)->set_location(yylloc);
		state->symbols->add_type((yyvsp[(2) - (6)].identifier), glsl_type::void_type);
	}
    break;

  case 279:

/* Line 1806 of yacc.c  */
#line 1636 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.struct_specifier) = new(ctx) ast_struct_specifier(NULL,NULL);
		(yyval.struct_specifier)->set_location(yylloc);
	}
    break;

  case 280:

/* Line 1806 of yacc.c  */
#line 1645 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_cbuffer_declaration((yyvsp[(2) - (5)].identifier), (yyvsp[(4) - (5)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 281:

/* Line 1806 of yacc.c  */
#line 1651 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		/* Do nothing! */
		(yyval.node) = NULL;
	}
    break;

  case 282:

/* Line 1806 of yacc.c  */
#line 1659 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].declarator_list);
	   (yyvsp[(1) - (1)].declarator_list)->link.self_link();
	}
    break;

  case 283:

/* Line 1806 of yacc.c  */
#line 1664 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (2)].node);
	   (yyval.node)->link.insert_before(& (yyvsp[(2) - (2)].declarator_list)->link);
	}
    break;

  case 284:

/* Line 1806 of yacc.c  */
#line 1672 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[(1) - (3)].fully_specified_type));
	   (yyval.declarator_list)->set_location(yylloc);
	   (yyval.declarator_list)->declarations.push_degenerate_list_at_head(& (yyvsp[(2) - (3)].declaration)->link);
	}
    break;

  case 285:

/* Line 1806 of yacc.c  */
#line 1682 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
		(yyval.fully_specified_type)->set_location(yylloc);
		(yyval.fully_specified_type)->specifier = (yyvsp[(1) - (1)].type_specifier);
	}
    break;

  case 286:

/* Line 1806 of yacc.c  */
#line 1689 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
		(yyval.fully_specified_type)->set_location(yylloc);
		(yyval.fully_specified_type)->specifier = (yyvsp[(2) - (2)].type_specifier);
		(yyval.fully_specified_type)->qualifier = (yyvsp[(1) - (2)].type_qualifier);
	}
    break;

  case 288:

/* Line 1806 of yacc.c  */
#line 1701 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(2) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 289:

/* Line 1806 of yacc.c  */
#line 1706 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.type_qualifier) = (yyvsp[(1) - (2)].type_qualifier);
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 290:

/* Line 1806 of yacc.c  */
#line 1711 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.centroid = 1;
	}
    break;

  case 291:

/* Line 1806 of yacc.c  */
#line 1716 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
		(yyval.type_qualifier).flags.q.precise = 1;
	}
    break;

  case 292:

/* Line 1806 of yacc.c  */
#line 1724 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.declaration) = (yyvsp[(1) - (1)].declaration);
	   (yyvsp[(1) - (1)].declaration)->link.self_link();
	}
    break;

  case 293:

/* Line 1806 of yacc.c  */
#line 1729 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.declaration) = (yyvsp[(1) - (3)].declaration);
	   (yyval.declaration)->link.insert_before(& (yyvsp[(3) - (3)].declaration)->link);
	}
    break;

  case 294:

/* Line 1806 of yacc.c  */
#line 1737 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (2)].identifier), false, NULL, NULL);
	   (yyval.declaration)->set_location(yylloc);
	   state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(1) - (2)].identifier), ir_var_auto));
	}
    break;

  case 295:

/* Line 1806 of yacc.c  */
#line 1744 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = (yyvsp[(1) - (1)].declaration);
	}
    break;

  case 296:

/* Line 1806 of yacc.c  */
#line 1749 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (3)].identifier), false, NULL, NULL);
		(yyval.declaration)->set_location(yylloc);
		(yyval.declaration)->semantic = (yyvsp[(3) - (3)].identifier);
		state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[(1) - (3)].identifier), ir_var_auto));
	}
    break;

  case 297:

/* Line 1806 of yacc.c  */
#line 1757 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (6)].identifier), true, (yyvsp[(3) - (6)].expression), NULL);
	   (yyval.declaration)->set_location(yylloc);
	   (yyval.declaration)->semantic = (yyvsp[(6) - (6)].identifier);
	}
    break;

  case 298:

/* Line 1806 of yacc.c  */
#line 1767 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = new(ctx) ast_declaration((yyvsp[(1) - (4)].identifier), true, (yyvsp[(3) - (4)].expression), NULL);
	   (yyval.declaration)->set_location(yylloc);
	}
    break;

  case 299:

/* Line 1806 of yacc.c  */
#line 1773 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.declaration) = (yyvsp[(1) - (4)].declaration);
	   (yyvsp[(3) - (4)].expression)->link.next = &((yyval.declaration)->array_size->link);
	   (yyval.declaration)->array_size = (yyvsp[(3) - (4)].expression);
	}
    break;

  case 301:

/* Line 1806 of yacc.c  */
#line 1783 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.expression) = new(ctx) ast_initializer_list_expression();
		(yyval.expression)->expressions.push_degenerate_list_at_head(& (yyvsp[(2) - (3)].expression)->link);
	}
    break;

  case 302:

/* Line 1806 of yacc.c  */
#line 1792 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (1)].expression);
		(yyval.expression)->link.self_link();
	}
    break;

  case 303:

/* Line 1806 of yacc.c  */
#line 1797 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (3)].expression);
		(yyval.expression)->link.insert_before(& (yyvsp[(3) - (3)].expression)->link);
	}
    break;

  case 304:

/* Line 1806 of yacc.c  */
#line 1802 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.expression) = (yyvsp[(1) - (2)].expression);
	}
    break;

  case 306:

/* Line 1806 of yacc.c  */
#line 1814 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].compound_statement); }
    break;

  case 308:

/* Line 1806 of yacc.c  */
#line 1817 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (ast_node *) (yyvsp[(2) - (2)].compound_statement);
		(yyval.node)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (2)].attribute)->link);
	}
    break;

  case 309:

/* Line 1806 of yacc.c  */
#line 1822 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(2) - (2)].node);
		(yyval.node)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (2)].attribute)->link);
	}
    break;

  case 316:

/* Line 1806 of yacc.c  */
#line 1839 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(true, NULL);
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 317:

/* Line 1806 of yacc.c  */
#line 1845 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   state->symbols->push_scope();
	}
    break;

  case 318:

/* Line 1806 of yacc.c  */
#line 1849 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(true, (yyvsp[(3) - (4)].node));
	   (yyval.compound_statement)->set_location(yylloc);
	   state->symbols->pop_scope();
	}
    break;

  case 319:

/* Line 1806 of yacc.c  */
#line 1858 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].compound_statement); }
    break;

  case 321:

/* Line 1806 of yacc.c  */
#line 1864 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(false, NULL);
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 322:

/* Line 1806 of yacc.c  */
#line 1870 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.compound_statement) = new(ctx) ast_compound_statement(false, (yyvsp[(2) - (3)].node));
	   (yyval.compound_statement)->set_location(yylloc);
	}
    break;

  case 323:

/* Line 1806 of yacc.c  */
#line 1879 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   if ((yyvsp[(1) - (1)].node) == NULL) {
		  _mesa_glsl_error(& (yylsp[(1) - (1)]), state, "<nil> statement\n");
		  check((yyvsp[(1) - (1)].node) != NULL);
	   }

	   (yyval.node) = (yyvsp[(1) - (1)].node);
	   (yyval.node)->link.self_link();
	}
    break;

  case 324:

/* Line 1806 of yacc.c  */
#line 1889 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   if ((yyvsp[(2) - (2)].node) == NULL) {
		  _mesa_glsl_error(& (yylsp[(2) - (2)]), state, "<nil> statement\n");
		  check((yyvsp[(2) - (2)].node) != NULL);
	   }
	   (yyval.node) = (yyvsp[(1) - (2)].node);
	   (yyval.node)->link.insert_before(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 325:

/* Line 1806 of yacc.c  */
#line 1901 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_expression_statement(NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 326:

/* Line 1806 of yacc.c  */
#line 1907 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_expression_statement((yyvsp[(1) - (2)].expression));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 327:

/* Line 1806 of yacc.c  */
#line 1916 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = new(state) ast_selection_statement((yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].selection_rest_statement).then_statement,
						   (yyvsp[(5) - (5)].selection_rest_statement).else_statement);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 328:

/* Line 1806 of yacc.c  */
#line 1925 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.selection_rest_statement).then_statement = (yyvsp[(1) - (3)].node);
	   (yyval.selection_rest_statement).else_statement = (yyvsp[(3) - (3)].node);
	}
    break;

  case 329:

/* Line 1806 of yacc.c  */
#line 1930 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.selection_rest_statement).then_statement = (yyvsp[(1) - (1)].node);
	   (yyval.selection_rest_statement).else_statement = NULL;
	}
    break;

  case 330:

/* Line 1806 of yacc.c  */
#line 1938 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (ast_node *) (yyvsp[(1) - (1)].expression);
	}
    break;

  case 331:

/* Line 1806 of yacc.c  */
#line 1942 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   ast_declaration *decl = new(ctx) ast_declaration((yyvsp[(2) - (4)].identifier), false, NULL, (yyvsp[(4) - (4)].expression));
	   ast_declarator_list *declarator = new(ctx) ast_declarator_list((yyvsp[(1) - (4)].fully_specified_type));
	   decl->set_location(yylloc);
	   declarator->set_location(yylloc);

	   declarator->declarations.push_tail(&decl->link);
	   (yyval.node) = declarator;
	}
    break;

  case 332:

/* Line 1806 of yacc.c  */
#line 1960 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = new(state) ast_switch_statement((yyvsp[(3) - (5)].expression), (yyvsp[(5) - (5)].switch_body));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 333:

/* Line 1806 of yacc.c  */
#line 1968 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.switch_body) = new(state) ast_switch_body(NULL);
	   (yyval.switch_body)->set_location(yylloc);
	}
    break;

  case 334:

/* Line 1806 of yacc.c  */
#line 1973 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.switch_body) = new(state) ast_switch_body((yyvsp[(2) - (3)].case_statement_list));
	   (yyval.switch_body)->set_location(yylloc);
	}
    break;

  case 335:

/* Line 1806 of yacc.c  */
#line 1981 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label) = new(state) ast_case_label((yyvsp[(2) - (3)].expression));
	   (yyval.case_label)->set_location(yylloc);
	}
    break;

  case 336:

/* Line 1806 of yacc.c  */
#line 1986 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label) = new(state) ast_case_label(NULL);
	   (yyval.case_label)->set_location(yylloc);
	}
    break;

  case 337:

/* Line 1806 of yacc.c  */
#line 1994 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_label_list *labels = new(state) ast_case_label_list();

	   labels->labels.push_tail(& (yyvsp[(1) - (1)].case_label)->link);
	   (yyval.case_label_list) = labels;
	   (yyval.case_label_list)->set_location(yylloc);
	}
    break;

  case 338:

/* Line 1806 of yacc.c  */
#line 2002 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_label_list) = (yyvsp[(1) - (2)].case_label_list);
	   (yyval.case_label_list)->labels.push_tail(& (yyvsp[(2) - (2)].case_label)->link);
	}
    break;

  case 339:

/* Line 1806 of yacc.c  */
#line 2010 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_statement *stmts = new(state) ast_case_statement((yyvsp[(1) - (2)].case_label_list));
	   stmts->set_location(yylloc);

	   stmts->stmts.push_tail(& (yyvsp[(2) - (2)].node)->link);
	   (yyval.case_statement) = stmts;
	}
    break;

  case 340:

/* Line 1806 of yacc.c  */
#line 2018 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_statement) = (yyvsp[(1) - (2)].case_statement);
	   (yyval.case_statement)->stmts.push_tail(& (yyvsp[(2) - (2)].node)->link);
	}
    break;

  case 341:

/* Line 1806 of yacc.c  */
#line 2026 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   ast_case_statement_list *cases= new(state) ast_case_statement_list();
	   cases->set_location(yylloc);

	   cases->cases.push_tail(& (yyvsp[(1) - (1)].case_statement)->link);
	   (yyval.case_statement_list) = cases;
	}
    break;

  case 342:

/* Line 1806 of yacc.c  */
#line 2034 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.case_statement_list) = (yyvsp[(1) - (2)].case_statement_list);
	   (yyval.case_statement_list)->cases.push_tail(& (yyvsp[(2) - (2)].case_statement)->link);
	}
    break;

  case 343:

/* Line 1806 of yacc.c  */
#line 2042 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_while,
							NULL, (yyvsp[(3) - (5)].node), NULL, (yyvsp[(5) - (5)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 344:

/* Line 1806 of yacc.c  */
#line 2049 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_do_while,
							NULL, (yyvsp[(5) - (7)].expression), NULL, (yyvsp[(2) - (7)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 345:

/* Line 1806 of yacc.c  */
#line 2056 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_for,
							(yyvsp[(3) - (6)].node), (yyvsp[(4) - (6)].for_rest_statement).cond, (yyvsp[(4) - (6)].for_rest_statement).rest, (yyvsp[(6) - (6)].node));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 349:

/* Line 1806 of yacc.c  */
#line 2072 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = NULL;
	}
    break;

  case 350:

/* Line 1806 of yacc.c  */
#line 2079 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.for_rest_statement).cond = (yyvsp[(1) - (2)].node);
	   (yyval.for_rest_statement).rest = NULL;
	}
    break;

  case 351:

/* Line 1806 of yacc.c  */
#line 2084 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.for_rest_statement).cond = (yyvsp[(1) - (3)].node);
	   (yyval.for_rest_statement).rest = (yyvsp[(3) - (3)].expression);
	}
    break;

  case 352:

/* Line 1806 of yacc.c  */
#line 2093 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_continue, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 353:

/* Line 1806 of yacc.c  */
#line 2099 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_break, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 354:

/* Line 1806 of yacc.c  */
#line 2105 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 355:

/* Line 1806 of yacc.c  */
#line 2111 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, (yyvsp[(2) - (3)].expression));
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 356:

/* Line 1806 of yacc.c  */
#line 2117 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_discard, NULL);
	   (yyval.node)->set_location(yylloc);
	}
    break;

  case 357:

/* Line 1806 of yacc.c  */
#line 2125 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (yyvsp[(1) - (1)].function_definition); }
    break;

  case 358:

/* Line 1806 of yacc.c  */
#line 2126 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    { (yyval.node) = (yyvsp[(1) - (1)].node); }
    break;

  case 359:

/* Line 1806 of yacc.c  */
#line 2131 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute_arg) = new(ctx) ast_attribute_arg( (yyvsp[(1) - (1)].expression) );
		(yyval.attribute_arg)->link.self_link();
	}
    break;

  case 360:

/* Line 1806 of yacc.c  */
#line 2137 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute_arg) = new(ctx) ast_attribute_arg( (yyvsp[(1) - (1)].string_literal) );
		(yyval.attribute_arg)->link.self_link();
	}
    break;

  case 361:

/* Line 1806 of yacc.c  */
#line 2146 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute_arg) = (yyvsp[(1) - (1)].attribute_arg);
	}
    break;

  case 362:

/* Line 1806 of yacc.c  */
#line 2150 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute_arg) = (yyvsp[(1) - (3)].attribute_arg);
		(yyval.attribute_arg)->link.insert_before( & (yyvsp[(3) - (3)].attribute_arg)->link);
	}
    break;

  case 363:

/* Line 1806 of yacc.c  */
#line 2158 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (3)].identifier) );
		(yyval.attribute)->link.self_link();
	}
    break;

  case 364:

/* Line 1806 of yacc.c  */
#line 2164 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (3)].identifier) );
		(yyval.attribute)->link.self_link();
	}
    break;

  case 365:

/* Line 1806 of yacc.c  */
#line 2170 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (6)].identifier) );
		(yyval.attribute)->link.self_link();
		(yyval.attribute)->arguments.push_degenerate_list_at_head( & (yyvsp[(4) - (6)].attribute_arg)->link );
	}
    break;

  case 366:

/* Line 1806 of yacc.c  */
#line 2177 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		void *ctx = state;
		(yyval.attribute) = new(ctx) ast_attribute( (yyvsp[(2) - (6)].identifier) );
		(yyval.attribute)->link.self_link();
		(yyval.attribute)->arguments.push_degenerate_list_at_head( & (yyvsp[(4) - (6)].attribute_arg)->link );
	}
    break;

  case 367:

/* Line 1806 of yacc.c  */
#line 2187 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute) = (yyvsp[(1) - (2)].attribute);
		(yyval.attribute)->link.insert_before( & (yyvsp[(2) - (2)].attribute)->link);
	}
    break;

  case 368:

/* Line 1806 of yacc.c  */
#line 2192 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.attribute) = (yyvsp[(1) - (1)].attribute);
	}
    break;

  case 369:

/* Line 1806 of yacc.c  */
#line 2199 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function_definition) = new(ctx) ast_function_definition();
	   (yyval.function_definition)->set_location(yylloc);
	   (yyval.function_definition)->prototype = (yyvsp[(1) - (2)].function);
	   (yyval.function_definition)->body = (yyvsp[(2) - (2)].compound_statement);

	   state->symbols->pop_scope();
	}
    break;

  case 370:

/* Line 1806 of yacc.c  */
#line 2209 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   void *ctx = state;
	   (yyval.function_definition) = new(ctx) ast_function_definition();
	   (yyval.function_definition)->set_location(yylloc);
	   (yyval.function_definition)->prototype = (yyvsp[(2) - (3)].function);
	   (yyval.function_definition)->body = (yyvsp[(3) - (3)].compound_statement);
	   (yyval.function_definition)->attributes.push_degenerate_list_at_head( & (yyvsp[(1) - (3)].attribute)->link);

	   state->symbols->pop_scope();
	}
    break;

  case 377:

/* Line 1806 of yacc.c  */
#line 2237 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		(yyval.node) = (yyvsp[(1) - (1)].function);
	}
    break;

  case 378:

/* Line 1806 of yacc.c  */
#line 2241 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		if ((yyvsp[(1) - (4)].declarator_list)->type->qualifier.flags.q.is_static == 0 && (yyvsp[(1) - (4)].declarator_list)->type->qualifier.flags.q.shared == 0)
		{
			(yyvsp[(1) - (4)].declarator_list)->type->qualifier.flags.q.uniform = 1;
		}
		(yyval.node) = (yyvsp[(1) - (4)].declarator_list);
	}
    break;

  case 379:

/* Line 1806 of yacc.c  */
#line 2249 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
		if ((yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.is_static == 0 && (yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.shared == 0)
		{
			(yyvsp[(1) - (2)].declarator_list)->type->qualifier.flags.q.uniform = 1;
		}
		(yyval.node) = (yyvsp[(1) - (2)].declarator_list);
	}
    break;

  case 380:

/* Line 1806 of yacc.c  */
#line 2257 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"
    {
	   (yyval.node) = (yyvsp[(1) - (1)].node);
	}
    break;



/* Line 1806 of yacc.c  */
#line 6844 "../../../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.inl"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, state, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (&yylloc, state, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, &yylloc, state);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[1] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp, state);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, state, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, state);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp, state);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



