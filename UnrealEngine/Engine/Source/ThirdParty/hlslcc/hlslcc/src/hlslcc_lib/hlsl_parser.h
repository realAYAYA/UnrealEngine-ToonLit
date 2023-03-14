/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
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
     CENTROID = 297,
     NOPERSPECTIVE = 298,
     NOINTERPOLATION = 299,
     LINEAR = 300,
     MAT2X2 = 301,
     MAT2X3 = 302,
     MAT2X4 = 303,
     MAT3X2 = 304,
     MAT3X3 = 305,
     MAT3X4 = 306,
     MAT4X2 = 307,
     MAT4X3 = 308,
     MAT4X4 = 309,
     HMAT2X2 = 310,
     HMAT2X3 = 311,
     HMAT2X4 = 312,
     HMAT3X2 = 313,
     HMAT3X3 = 314,
     HMAT3X4 = 315,
     HMAT4X2 = 316,
     HMAT4X3 = 317,
     HMAT4X4 = 318,
     FMAT2X2 = 319,
     FMAT2X3 = 320,
     FMAT2X4 = 321,
     FMAT3X2 = 322,
     FMAT3X3 = 323,
     FMAT3X4 = 324,
     FMAT4X2 = 325,
     FMAT4X3 = 326,
     FMAT4X4 = 327,
     SAMPLERSTATE = 328,
     SAMPLERSTATE_CMP = 329,
     TEXTURE1D = 330,
     TEXTURE1D_ARRAY = 331,
     TEXTURE2D = 332,
     TEXTURE2D_ARRAY = 333,
     TEXTURE2DMS = 334,
     TEXTURE_EXTERNAL = 335,
     BUFFER = 336,
     STRUCTUREDBUFFER = 337,
     BYTEADDRESSBUFFER = 338,
     TEXTURE2DMS_ARRAY = 339,
     TEXTURE3D = 340,
     TEXTURECUBE = 341,
     TEXTURECUBE_ARRAY = 342,
     RWBUFFER = 343,
     RWTEXTURE1D = 344,
     RWTEXTURE1D_ARRAY = 345,
     RWTEXTURE2D = 346,
     RWTEXTURE2D_ARRAY = 347,
     RWTEXTURE3D = 348,
     RWSTRUCTUREDBUFFER = 349,
     RWBYTEADDRESSBUFFER = 350,
     POINT_TOK = 351,
     LINE_TOK = 352,
     TRIANGLE_TOK = 353,
     LINEADJ_TOK = 354,
     TRIANGLEADJ_TOK = 355,
     POINTSTREAM = 356,
     LINESTREAM = 357,
     TRIANGLESTREAM = 358,
     INPUTPATCH = 359,
     OUTPUTPATCH = 360,
     STRUCT = 361,
     VOID_TOK = 362,
     WHILE = 363,
     CBUFFER = 364,
     REGISTER = 365,
     PACKOFFSET = 366,
     IDENTIFIER = 367,
     TYPE_IDENTIFIER = 368,
     NEW_IDENTIFIER = 369,
     FLOATCONSTANT = 370,
     INTCONSTANT = 371,
     UINTCONSTANT = 372,
     BOOLCONSTANT = 373,
     STRINGCONSTANT = 374,
     FIELD_SELECTION = 375,
     LEFT_OP = 376,
     RIGHT_OP = 377,
     INC_OP = 378,
     DEC_OP = 379,
     LE_OP = 380,
     GE_OP = 381,
     EQ_OP = 382,
     NE_OP = 383,
     AND_OP = 384,
     OR_OP = 385,
     MUL_ASSIGN = 386,
     DIV_ASSIGN = 387,
     ADD_ASSIGN = 388,
     MOD_ASSIGN = 389,
     LEFT_ASSIGN = 390,
     RIGHT_ASSIGN = 391,
     AND_ASSIGN = 392,
     XOR_ASSIGN = 393,
     OR_ASSIGN = 394,
     SUB_ASSIGN = 395,
     INVARIANT = 396,
     VERSION_TOK = 397,
     EXTENSION = 398,
     LINE = 399,
     COLON = 400,
     EOL = 401,
     INTERFACE = 402,
     OUTPUT = 403,
     PRAGMA_DEBUG_ON = 404,
     PRAGMA_DEBUG_OFF = 405,
     PRAGMA_OPTIMIZE_ON = 406,
     PRAGMA_OPTIMIZE_OFF = 407,
     PRAGMA_INVARIANT_ALL = 408,
     ASM = 409,
     CLASS = 410,
     UNION = 411,
     ENUM = 412,
     TYPEDEF = 413,
     TEMPLATE = 414,
     THIS = 415,
     PACKED_TOK = 416,
     GOTO = 417,
     INLINE_TOK = 418,
     NOINLINE = 419,
     VOLATILE = 420,
     PUBLIC_TOK = 421,
     STATIC = 422,
     EXTERN = 423,
     EXTERNAL = 424,
     LONG_TOK = 425,
     SHORT_TOK = 426,
     DOUBLE_TOK = 427,
     HALF = 428,
     FIXED_TOK = 429,
     UNSIGNED = 430,
     DVEC2 = 431,
     DVEC3 = 432,
     DVEC4 = 433,
     FVEC2 = 434,
     FVEC3 = 435,
     FVEC4 = 436,
     SAMPLER2DRECT = 437,
     SAMPLER3DRECT = 438,
     SAMPLER2DRECTSHADOW = 439,
     SIZEOF = 440,
     CAST = 441,
     NAMESPACE = 442,
     USING = 443,
     ERROR_TOK = 444,
     COMMON = 445,
     PARTITION = 446,
     ACTIVE = 447,
     SAMPLERBUFFER = 448,
     FILTER = 449,
     IMAGE1D = 450,
     IMAGE2D = 451,
     IMAGE3D = 452,
     IMAGECUBE = 453,
     IMAGE1DARRAY = 454,
     IMAGE2DARRAY = 455,
     IIMAGE1D = 456,
     IIMAGE2D = 457,
     IIMAGE3D = 458,
     IIMAGECUBE = 459,
     IIMAGE1DARRAY = 460,
     IIMAGE2DARRAY = 461,
     UIMAGE1D = 462,
     UIMAGE2D = 463,
     UIMAGE3D = 464,
     UIMAGECUBE = 465,
     UIMAGE1DARRAY = 466,
     UIMAGE2DARRAY = 467,
     IMAGE1DSHADOW = 468,
     IMAGE2DSHADOW = 469,
     IMAGEBUFFER = 470,
     IIMAGEBUFFER = 471,
     UIMAGEBUFFER = 472,
     IMAGE1DARRAYSHADOW = 473,
     IMAGE2DARRAYSHADOW = 474,
     ROW_MAJOR = 475,
     COLUMN_MAJOR = 476
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 64 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.yy"

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



/* Line 2068 of yacc.c  */
#line 312 "../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib/hlsl_parser.h"
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



