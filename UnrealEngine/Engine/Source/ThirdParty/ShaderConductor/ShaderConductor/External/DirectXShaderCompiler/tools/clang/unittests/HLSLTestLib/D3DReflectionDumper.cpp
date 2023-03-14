///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// D3DReflectionDumper.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Use this to dump D3D Reflection data for testing.                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/test/D3DReflectionDumper.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/dxcapi.h"
#include <sstream>

// Remove this workaround once newer version of d3dcommon.h can be compiled against
#define ADD_16_64_BIT_TYPES

namespace hlsl {
namespace dump {

LPCSTR ToString(D3D_CBUFFER_TYPE CBType) {
  switch (CBType) {
  case D3D_CT_CBUFFER: return "D3D_CT_CBUFFER";
  case D3D_CT_TBUFFER: return "D3D_CT_TBUFFER";
  case D3D_CT_INTERFACE_POINTERS: return "D3D_CT_INTERFACE_POINTERS";
  case D3D_CT_RESOURCE_BIND_INFO: return "D3D_CT_RESOURCE_BIND_INFO";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_SHADER_INPUT_TYPE Type) {
  switch ((UINT32)Type) {
  case D3D_SIT_CBUFFER: return "D3D_SIT_CBUFFER";
  case D3D_SIT_TBUFFER: return "D3D_SIT_TBUFFER";
  case D3D_SIT_TEXTURE: return "D3D_SIT_TEXTURE";
  case D3D_SIT_SAMPLER: return "D3D_SIT_SAMPLER";
  case D3D_SIT_UAV_RWTYPED: return "D3D_SIT_UAV_RWTYPED";
  case D3D_SIT_STRUCTURED: return "D3D_SIT_STRUCTURED";
  case D3D_SIT_UAV_RWSTRUCTURED: return "D3D_SIT_UAV_RWSTRUCTURED";
  case D3D_SIT_BYTEADDRESS: return "D3D_SIT_BYTEADDRESS";
  case D3D_SIT_UAV_RWBYTEADDRESS: return "D3D_SIT_UAV_RWBYTEADDRESS";
  case D3D_SIT_UAV_APPEND_STRUCTURED: return "D3D_SIT_UAV_APPEND_STRUCTURED";
  case D3D_SIT_UAV_CONSUME_STRUCTURED: return "D3D_SIT_UAV_CONSUME_STRUCTURED";
  case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: return "D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER";
  case (D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER + 1): return "D3D_SIT_RTACCELERATIONSTRUCTURE";
  case (D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER + 2): return "D3D_SIT_UAV_FEEDBACKTEXTURE";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_RESOURCE_RETURN_TYPE ReturnType) {
  switch (ReturnType) {
  case D3D_RETURN_TYPE_UNORM: return "D3D_RETURN_TYPE_UNORM";
  case D3D_RETURN_TYPE_SNORM: return "D3D_RETURN_TYPE_SNORM";
  case D3D_RETURN_TYPE_SINT: return "D3D_RETURN_TYPE_SINT";
  case D3D_RETURN_TYPE_UINT: return "D3D_RETURN_TYPE_UINT";
  case D3D_RETURN_TYPE_FLOAT: return "D3D_RETURN_TYPE_FLOAT";
  case D3D_RETURN_TYPE_MIXED: return "D3D_RETURN_TYPE_MIXED";
  case D3D_RETURN_TYPE_DOUBLE: return "D3D_RETURN_TYPE_DOUBLE";
  case D3D_RETURN_TYPE_CONTINUED: return "D3D_RETURN_TYPE_CONTINUED";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_SRV_DIMENSION Dimension) {
  switch (Dimension) {
  case D3D_SRV_DIMENSION_UNKNOWN: return "D3D_SRV_DIMENSION_UNKNOWN";
  case D3D_SRV_DIMENSION_BUFFER: return "D3D_SRV_DIMENSION_BUFFER";
  case D3D_SRV_DIMENSION_TEXTURE1D: return "D3D_SRV_DIMENSION_TEXTURE1D";
  case D3D_SRV_DIMENSION_TEXTURE1DARRAY: return "D3D_SRV_DIMENSION_TEXTURE1DARRAY";
  case D3D_SRV_DIMENSION_TEXTURE2D: return "D3D_SRV_DIMENSION_TEXTURE2D";
  case D3D_SRV_DIMENSION_TEXTURE2DARRAY: return "D3D_SRV_DIMENSION_TEXTURE2DARRAY";
  case D3D_SRV_DIMENSION_TEXTURE2DMS: return "D3D_SRV_DIMENSION_TEXTURE2DMS";
  case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY: return "D3D_SRV_DIMENSION_TEXTURE2DMSARRAY";
  case D3D_SRV_DIMENSION_TEXTURE3D: return "D3D_SRV_DIMENSION_TEXTURE3D";
  case D3D_SRV_DIMENSION_TEXTURECUBE: return "D3D_SRV_DIMENSION_TEXTURECUBE";
  case D3D_SRV_DIMENSION_TEXTURECUBEARRAY: return "D3D_SRV_DIMENSION_TEXTURECUBEARRAY";
  case D3D_SRV_DIMENSION_BUFFEREX: return "D3D_SRV_DIMENSION_BUFFEREX";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_PRIMITIVE_TOPOLOGY GSOutputTopology) {
  switch (GSOutputTopology) {
  case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED: return "D3D_PRIMITIVE_TOPOLOGY_UNDEFINED";
  case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: return "D3D_PRIMITIVE_TOPOLOGY_POINTLIST";
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST: return "D3D_PRIMITIVE_TOPOLOGY_LINELIST";
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: return "D3D_PRIMITIVE_TOPOLOGY_LINESTRIP";
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST: return "D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST";
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: return "D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP";
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ: return "D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ";
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ: return "D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ";
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ: return "D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ";
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ: return "D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ";
  case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST";
  case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST: return "D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_PRIMITIVE InputPrimitive) {
  switch (InputPrimitive) {
  case D3D_PRIMITIVE_UNDEFINED: return "D3D_PRIMITIVE_UNDEFINED";
  case D3D_PRIMITIVE_POINT: return "D3D_PRIMITIVE_POINT";
  case D3D_PRIMITIVE_LINE: return "D3D_PRIMITIVE_LINE";
  case D3D_PRIMITIVE_TRIANGLE: return "D3D_PRIMITIVE_TRIANGLE";
  case D3D_PRIMITIVE_LINE_ADJ: return "D3D_PRIMITIVE_LINE_ADJ";
  case D3D_PRIMITIVE_TRIANGLE_ADJ: return "D3D_PRIMITIVE_TRIANGLE_ADJ";
  case D3D_PRIMITIVE_1_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_1_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_2_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_2_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_3_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_3_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_4_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_4_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_5_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_5_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_6_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_6_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_7_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_7_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_8_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_8_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_9_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_9_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_10_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_10_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_11_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_11_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_12_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_12_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_13_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_13_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_14_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_14_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_15_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_15_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_16_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_16_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_17_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_17_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_18_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_18_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_19_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_19_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_20_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_20_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_21_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_21_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_22_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_22_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_23_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_23_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_24_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_24_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_25_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_25_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_26_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_26_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_27_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_27_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_28_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_28_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_29_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_29_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_30_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_30_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_31_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_31_CONTROL_POINT_PATCH";
  case D3D_PRIMITIVE_32_CONTROL_POINT_PATCH: return "D3D_PRIMITIVE_32_CONTROL_POINT_PATCH";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive) {
  switch (HSOutputPrimitive) {
  case D3D_TESSELLATOR_OUTPUT_UNDEFINED: return "D3D_TESSELLATOR_OUTPUT_UNDEFINED";
  case D3D_TESSELLATOR_OUTPUT_POINT: return "D3D_TESSELLATOR_OUTPUT_POINT";
  case D3D_TESSELLATOR_OUTPUT_LINE: return "D3D_TESSELLATOR_OUTPUT_LINE";
  case D3D_TESSELLATOR_OUTPUT_TRIANGLE_CW: return "D3D_TESSELLATOR_OUTPUT_TRIANGLE_CW";
  case D3D_TESSELLATOR_OUTPUT_TRIANGLE_CCW: return "D3D_TESSELLATOR_OUTPUT_TRIANGLE_CCW";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_TESSELLATOR_PARTITIONING HSPartitioning) {
  switch (HSPartitioning) {
  case D3D_TESSELLATOR_PARTITIONING_UNDEFINED: return "D3D_TESSELLATOR_PARTITIONING_UNDEFINED";
  case D3D_TESSELLATOR_PARTITIONING_INTEGER: return "D3D_TESSELLATOR_PARTITIONING_INTEGER";
  case D3D_TESSELLATOR_PARTITIONING_POW2: return "D3D_TESSELLATOR_PARTITIONING_POW2";
  case D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD: return "D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD";
  case D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN: return "D3D_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_TESSELLATOR_DOMAIN TessellatorDomain) {
  switch (TessellatorDomain) {
  case D3D_TESSELLATOR_DOMAIN_UNDEFINED: return "D3D_TESSELLATOR_DOMAIN_UNDEFINED";
  case D3D_TESSELLATOR_DOMAIN_ISOLINE: return "D3D_TESSELLATOR_DOMAIN_ISOLINE";
  case D3D_TESSELLATOR_DOMAIN_TRI: return "D3D_TESSELLATOR_DOMAIN_TRI";
  case D3D_TESSELLATOR_DOMAIN_QUAD: return "D3D_TESSELLATOR_DOMAIN_QUAD";
  default: return nullptr;
  }
}
LPCSTR ToString(D3D_SHADER_VARIABLE_CLASS Class) {
  switch (Class) {
  case D3D_SVC_SCALAR: return "D3D_SVC_SCALAR";
  case D3D_SVC_VECTOR: return "D3D_SVC_VECTOR";
  case D3D_SVC_MATRIX_ROWS: return "D3D_SVC_MATRIX_ROWS";
  case D3D_SVC_MATRIX_COLUMNS: return "D3D_SVC_MATRIX_COLUMNS";
  case D3D_SVC_OBJECT: return "D3D_SVC_OBJECT";
  case D3D_SVC_STRUCT: return "D3D_SVC_STRUCT";
  case D3D_SVC_INTERFACE_CLASS: return "D3D_SVC_INTERFACE_CLASS";
  case D3D_SVC_INTERFACE_POINTER: return "D3D_SVC_INTERFACE_POINTER";
  default: return nullptr;
  }
}

#ifdef ADD_16_64_BIT_TYPES
// Disable warning about value not being valid in enum
#pragma warning( disable : 4063 )
#define D3D_SVT_INT16   ((D3D_SHADER_VARIABLE_TYPE)58)
#define D3D_SVT_UINT16  ((D3D_SHADER_VARIABLE_TYPE)59)
#define D3D_SVT_FLOAT16 ((D3D_SHADER_VARIABLE_TYPE)60)
#define D3D_SVT_INT64   ((D3D_SHADER_VARIABLE_TYPE)61)
#define D3D_SVT_UINT64  ((D3D_SHADER_VARIABLE_TYPE)62)
#endif // ADD_16_64_BIT_TYPES

LPCSTR ToString(D3D_SHADER_VARIABLE_TYPE Type) {
  switch (Type) {
  case D3D_SVT_VOID: return "D3D_SVT_VOID";
  case D3D_SVT_BOOL: return "D3D_SVT_BOOL";
  case D3D_SVT_INT: return "D3D_SVT_INT";
  case D3D_SVT_FLOAT: return "D3D_SVT_FLOAT";
  case D3D_SVT_STRING: return "D3D_SVT_STRING";
  case D3D_SVT_TEXTURE: return "D3D_SVT_TEXTURE";
  case D3D_SVT_TEXTURE1D: return "D3D_SVT_TEXTURE1D";
  case D3D_SVT_TEXTURE2D: return "D3D_SVT_TEXTURE2D";
  case D3D_SVT_TEXTURE3D: return "D3D_SVT_TEXTURE3D";
  case D3D_SVT_TEXTURECUBE: return "D3D_SVT_TEXTURECUBE";
  case D3D_SVT_SAMPLER: return "D3D_SVT_SAMPLER";
  case D3D_SVT_SAMPLER1D: return "D3D_SVT_SAMPLER1D";
  case D3D_SVT_SAMPLER2D: return "D3D_SVT_SAMPLER2D";
  case D3D_SVT_SAMPLER3D: return "D3D_SVT_SAMPLER3D";
  case D3D_SVT_SAMPLERCUBE: return "D3D_SVT_SAMPLERCUBE";
  case D3D_SVT_PIXELSHADER: return "D3D_SVT_PIXELSHADER";
  case D3D_SVT_VERTEXSHADER: return "D3D_SVT_VERTEXSHADER";
  case D3D_SVT_PIXELFRAGMENT: return "D3D_SVT_PIXELFRAGMENT";
  case D3D_SVT_VERTEXFRAGMENT: return "D3D_SVT_VERTEXFRAGMENT";
  case D3D_SVT_UINT: return "D3D_SVT_UINT";
  case D3D_SVT_UINT8: return "D3D_SVT_UINT8";
  case D3D_SVT_GEOMETRYSHADER: return "D3D_SVT_GEOMETRYSHADER";
  case D3D_SVT_RASTERIZER: return "D3D_SVT_RASTERIZER";
  case D3D_SVT_DEPTHSTENCIL: return "D3D_SVT_DEPTHSTENCIL";
  case D3D_SVT_BLEND: return "D3D_SVT_BLEND";
  case D3D_SVT_BUFFER: return "D3D_SVT_BUFFER";
  case D3D_SVT_CBUFFER: return "D3D_SVT_CBUFFER";
  case D3D_SVT_TBUFFER: return "D3D_SVT_TBUFFER";
  case D3D_SVT_TEXTURE1DARRAY: return "D3D_SVT_TEXTURE1DARRAY";
  case D3D_SVT_TEXTURE2DARRAY: return "D3D_SVT_TEXTURE2DARRAY";
  case D3D_SVT_RENDERTARGETVIEW: return "D3D_SVT_RENDERTARGETVIEW";
  case D3D_SVT_DEPTHSTENCILVIEW: return "D3D_SVT_DEPTHSTENCILVIEW";
  case D3D_SVT_TEXTURE2DMS: return "D3D_SVT_TEXTURE2DMS";
  case D3D_SVT_TEXTURE2DMSARRAY: return "D3D_SVT_TEXTURE2DMSARRAY";
  case D3D_SVT_TEXTURECUBEARRAY: return "D3D_SVT_TEXTURECUBEARRAY";
  case D3D_SVT_HULLSHADER: return "D3D_SVT_HULLSHADER";
  case D3D_SVT_DOMAINSHADER: return "D3D_SVT_DOMAINSHADER";
  case D3D_SVT_INTERFACE_POINTER: return "D3D_SVT_INTERFACE_POINTER";
  case D3D_SVT_COMPUTESHADER: return "D3D_SVT_COMPUTESHADER";
  case D3D_SVT_DOUBLE: return "D3D_SVT_DOUBLE";
  case D3D_SVT_RWTEXTURE1D: return "D3D_SVT_RWTEXTURE1D";
  case D3D_SVT_RWTEXTURE1DARRAY: return "D3D_SVT_RWTEXTURE1DARRAY";
  case D3D_SVT_RWTEXTURE2D: return "D3D_SVT_RWTEXTURE2D";
  case D3D_SVT_RWTEXTURE2DARRAY: return "D3D_SVT_RWTEXTURE2DARRAY";
  case D3D_SVT_RWTEXTURE3D: return "D3D_SVT_RWTEXTURE3D";
  case D3D_SVT_RWBUFFER: return "D3D_SVT_RWBUFFER";
  case D3D_SVT_BYTEADDRESS_BUFFER: return "D3D_SVT_BYTEADDRESS_BUFFER";
  case D3D_SVT_RWBYTEADDRESS_BUFFER: return "D3D_SVT_RWBYTEADDRESS_BUFFER";
  case D3D_SVT_STRUCTURED_BUFFER: return "D3D_SVT_STRUCTURED_BUFFER";
  case D3D_SVT_RWSTRUCTURED_BUFFER: return "D3D_SVT_RWSTRUCTURED_BUFFER";
  case D3D_SVT_APPEND_STRUCTURED_BUFFER: return "D3D_SVT_APPEND_STRUCTURED_BUFFER";
  case D3D_SVT_CONSUME_STRUCTURED_BUFFER: return "D3D_SVT_CONSUME_STRUCTURED_BUFFER";
  case D3D_SVT_MIN8FLOAT: return "D3D_SVT_MIN8FLOAT";
  case D3D_SVT_MIN10FLOAT: return "D3D_SVT_MIN10FLOAT";
  case D3D_SVT_MIN16FLOAT: return "D3D_SVT_MIN16FLOAT";
  case D3D_SVT_MIN12INT: return "D3D_SVT_MIN12INT";
  case D3D_SVT_MIN16INT: return "D3D_SVT_MIN16INT";
  case D3D_SVT_MIN16UINT: return "D3D_SVT_MIN16UINT";
  case D3D_SVT_INT16: return "D3D_SVT_INT16";
  case D3D_SVT_UINT16: return "D3D_SVT_UINT16";
  case D3D_SVT_FLOAT16: return "D3D_SVT_FLOAT16";
  case D3D_SVT_INT64: return "D3D_SVT_INT64";
  case D3D_SVT_UINT64: return "D3D_SVT_UINT64";
  default: return nullptr;
  }
}

LPCSTR ToString(D3D_SHADER_VARIABLE_FLAGS Flag) {
  switch (Flag) {
  case D3D_SVF_USERPACKED: return "D3D_SVF_USERPACKED";
  case D3D_SVF_USED: return "D3D_SVF_USED";
  case D3D_SVF_INTERFACE_POINTER: return "D3D_SVF_INTERFACE_POINTER";
  case D3D_SVF_INTERFACE_PARAMETER: return "D3D_SVF_INTERFACE_PARAMETER";
  }
  return nullptr;
}

LPCSTR ToString(D3D_SHADER_INPUT_FLAGS Flag) {
  switch (Flag) {
  case D3D_SIF_USERPACKED: return "D3D_SIF_USERPACKED";
  case D3D_SIF_COMPARISON_SAMPLER: return "D3D_SIF_COMPARISON_SAMPLER";
  case D3D_SIF_TEXTURE_COMPONENT_0: return "D3D_SIF_TEXTURE_COMPONENT_0";
  case D3D_SIF_TEXTURE_COMPONENT_1: return "D3D_SIF_TEXTURE_COMPONENT_1";
  case D3D_SIF_TEXTURE_COMPONENTS: return "D3D_SIF_TEXTURE_COMPONENTS";
  case D3D_SIF_UNUSED: return "D3D_SIF_UNUSED";
  }
  return nullptr;
}

LPCSTR ToString(D3D_SHADER_CBUFFER_FLAGS Flag) {
  switch (Flag) {
  case D3D_CBF_USERPACKED: return "D3D_CBF_USERPACKED";
  }
  return nullptr;
}

LPCSTR ToString(D3D_PARAMETER_FLAGS Flag) {
  switch (Flag) {
  case D3D_PF_IN: return "D3D_PF_IN";
  case D3D_PF_OUT: return "D3D_PF_OUT";
  }
  return nullptr;
}

#ifndef D3D_NAME_SHADINGRATE
#define D3D_NAME_SHADINGRATE ((D3D_NAME)hlsl::DxilProgramSigSemantic::ShadingRate)
#endif
#ifndef D3D_NAME_CULLPRIMITIVE
#define D3D_NAME_CULLPRIMITIVE ((D3D_NAME)hlsl::DxilProgramSigSemantic::CullPrimitive)
#endif

LPCSTR ToString(D3D_NAME Name) {
  switch (Name) {
  case D3D_NAME_UNDEFINED: return "D3D_NAME_UNDEFINED";
  case D3D_NAME_POSITION: return "D3D_NAME_POSITION";
  case D3D_NAME_CLIP_DISTANCE: return "D3D_NAME_CLIP_DISTANCE";
  case D3D_NAME_CULL_DISTANCE: return "D3D_NAME_CULL_DISTANCE";
  case D3D_NAME_RENDER_TARGET_ARRAY_INDEX: return "D3D_NAME_RENDER_TARGET_ARRAY_INDEX";
  case D3D_NAME_VIEWPORT_ARRAY_INDEX: return "D3D_NAME_VIEWPORT_ARRAY_INDEX";
  case D3D_NAME_VERTEX_ID: return "D3D_NAME_VERTEX_ID";
  case D3D_NAME_PRIMITIVE_ID: return "D3D_NAME_PRIMITIVE_ID";
  case D3D_NAME_INSTANCE_ID: return "D3D_NAME_INSTANCE_ID";
  case D3D_NAME_IS_FRONT_FACE: return "D3D_NAME_IS_FRONT_FACE";
  case D3D_NAME_SAMPLE_INDEX: return "D3D_NAME_SAMPLE_INDEX";
  case D3D_NAME_FINAL_QUAD_EDGE_TESSFACTOR: return "D3D_NAME_FINAL_QUAD_EDGE_TESSFACTOR";
  case D3D_NAME_FINAL_QUAD_INSIDE_TESSFACTOR: return "D3D_NAME_FINAL_QUAD_INSIDE_TESSFACTOR";
  case D3D_NAME_FINAL_TRI_EDGE_TESSFACTOR: return "D3D_NAME_FINAL_TRI_EDGE_TESSFACTOR";
  case D3D_NAME_FINAL_TRI_INSIDE_TESSFACTOR: return "D3D_NAME_FINAL_TRI_INSIDE_TESSFACTOR";
  case D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR: return "D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR";
  case D3D_NAME_FINAL_LINE_DENSITY_TESSFACTOR: return "D3D_NAME_FINAL_LINE_DENSITY_TESSFACTOR";
  case D3D_NAME_BARYCENTRICS: return "D3D_NAME_BARYCENTRICS";
  case D3D_NAME_TARGET: return "D3D_NAME_TARGET";
  case D3D_NAME_DEPTH: return "D3D_NAME_DEPTH";
  case D3D_NAME_COVERAGE: return "D3D_NAME_COVERAGE";
  case D3D_NAME_DEPTH_GREATER_EQUAL: return "D3D_NAME_DEPTH_GREATER_EQUAL";
  case D3D_NAME_DEPTH_LESS_EQUAL: return "D3D_NAME_DEPTH_LESS_EQUAL";
  case D3D_NAME_STENCIL_REF: return "D3D_NAME_STENCIL_REF";
  case D3D_NAME_INNER_COVERAGE: return "D3D_NAME_INNER_COVERAGE";
  case D3D_NAME_SHADINGRATE: return "D3D_NAME_SHADINGRATE";
  case D3D_NAME_CULLPRIMITIVE: return "D3D_NAME_CULLPRIMITIVE";
  }
  return nullptr;
}

LPCSTR ToString(D3D_REGISTER_COMPONENT_TYPE CompTy) {
  switch (CompTy) {
  case D3D_REGISTER_COMPONENT_UNKNOWN: return "D3D_REGISTER_COMPONENT_UNKNOWN";
  case D3D_REGISTER_COMPONENT_UINT32: return "D3D_REGISTER_COMPONENT_UINT32";
  case D3D_REGISTER_COMPONENT_SINT32: return "D3D_REGISTER_COMPONENT_SINT32";
  case D3D_REGISTER_COMPONENT_FLOAT32: return "D3D_REGISTER_COMPONENT_FLOAT32";
  }
  return nullptr;
}

LPCSTR ToString(D3D_MIN_PRECISION MinPrec) {
  switch (MinPrec) {
  case D3D_MIN_PRECISION_DEFAULT: return "D3D_MIN_PRECISION_DEFAULT";
  case D3D_MIN_PRECISION_FLOAT_16: return "D3D_MIN_PRECISION_FLOAT_16";
  case D3D_MIN_PRECISION_FLOAT_2_8: return "D3D_MIN_PRECISION_FLOAT_2_8";
  case D3D_MIN_PRECISION_RESERVED: return "D3D_MIN_PRECISION_RESERVED";
  case D3D_MIN_PRECISION_SINT_16: return "D3D_MIN_PRECISION_SINT_16";
  case D3D_MIN_PRECISION_UINT_16: return "D3D_MIN_PRECISION_UINT_16";
  case D3D_MIN_PRECISION_ANY_16: return "D3D_MIN_PRECISION_ANY_16";
  case D3D_MIN_PRECISION_ANY_10: return "D3D_MIN_PRECISION_ANY_10";
  }
  return nullptr;
}

LPCSTR CompMaskToString(unsigned CompMask) {
  static const LPCSTR masks[16] = {
    "----",
    "x---",
    "-y--",
    "xy--",
    "--z-",
    "x-z-",
    "-yz-",
    "xyz-",
    "---w",
    "x--w",
    "-y-w",
    "xy-w",
    "--zw",
    "x-zw",
    "-yzw",
    "xyzw"
  };
  if (CompMask < 16) {
    return masks[CompMask];
  }
  return "<invalid mask>";
}


void D3DReflectionDumper::DumpDefaultValue(LPCVOID pDefaultValue, UINT Size) {
  WriteLn("DefaultValue: ", pDefaultValue ? "<present>" : "<nullptr>");    // TODO: Dump DefaultValue
}
void D3DReflectionDumper::DumpShaderVersion(UINT Version) {
  const char *szType = "<unknown>";
  UINT Type = D3D12_SHVER_GET_TYPE(Version);
  switch (Type) {
  case (UINT)hlsl::DXIL::ShaderKind::Pixel: szType = "Pixel"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Vertex: szType = "Vertex"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Geometry: szType = "Geometry"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Hull: szType = "Hull"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Domain: szType = "Domain"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Compute: szType = "Compute"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Library: szType = "Library"; break;
  case (UINT)hlsl::DXIL::ShaderKind::RayGeneration: szType = "RayGeneration"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Intersection: szType = "Intersection"; break;
  case (UINT)hlsl::DXIL::ShaderKind::AnyHit: szType = "AnyHit"; break;
  case (UINT)hlsl::DXIL::ShaderKind::ClosestHit: szType = "ClosestHit"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Miss: szType = "Miss"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Callable: szType = "Callable"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Mesh: szType = "Mesh"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Amplification: szType = "Amplification"; break;
  case (UINT)hlsl::DXIL::ShaderKind::Invalid: szType = "Invalid"; break;
  }
  UINT Major = D3D12_SHVER_GET_MAJOR(Version);
  UINT Minor = D3D12_SHVER_GET_MINOR(Version);
  WriteLn("Shader Version: ", szType, " ", Major, ".", Minor);
}

void D3DReflectionDumper::Dump(D3D12_SHADER_TYPE_DESC &tyDesc) {
  SetLastName(tyDesc.Name);
  WriteLn("D3D12_SHADER_TYPE_DESC: Name: ", m_LastName);
  Indent();
  DumpEnum("Class", tyDesc.Class);
  DumpEnum("Type", tyDesc.Type);
  WriteLn("Elements: ", tyDesc.Elements);
  WriteLn("Rows: ", tyDesc.Rows);
  WriteLn("Columns: ", tyDesc.Columns);
  WriteLn("Members: ", tyDesc.Members);
  WriteLn("Offset: ", tyDesc.Offset);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_VARIABLE_DESC &varDesc) {
  SetLastName(varDesc.Name);
  WriteLn("D3D12_SHADER_VARIABLE_DESC: Name: ", m_LastName);
  Indent();
  WriteLn("Size: ", varDesc.Size);
  WriteLn("StartOffset: ", varDesc.StartOffset);
  WriteLn("uFlags: ", FlagsValue<D3D_SHADER_VARIABLE_FLAGS>(varDesc.uFlags));
  DumpDefaultValue(varDesc.DefaultValue, varDesc.Size);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_BUFFER_DESC &Desc) {
  SetLastName(Desc.Name);
  WriteLn("D3D12_SHADER_BUFFER_DESC: Name: ", m_LastName);
  Indent();
  DumpEnum("Type", Desc.Type);
  WriteLn("Size: ", Desc.Size);
  WriteLn("uFlags: ", FlagsValue<D3D_SHADER_CBUFFER_FLAGS>(Desc.uFlags));
  WriteLn("Num Variables: ", Desc.Variables);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  SetLastName(resDesc.Name);
  WriteLn("D3D12_SHADER_INPUT_BIND_DESC: Name: ", m_LastName);
  Indent();
  DumpEnum("Type", resDesc.Type);
  WriteLn("uID: ", resDesc.uID);
  WriteLn("BindCount: ", resDesc.BindCount);
  WriteLn("BindPoint: ", resDesc.BindPoint);
  WriteLn("Space: ", resDesc.Space);
  DumpEnum("ReturnType", resDesc.ReturnType);
  DumpEnum("Dimension", resDesc.Dimension);
  WriteLn("NumSamples (or stride): ", resDesc.NumSamples);
  WriteLn("uFlags: ", FlagsValue<D3D_SHADER_INPUT_FLAGS>(resDesc.uFlags));
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SIGNATURE_PARAMETER_DESC &elDesc) {
  WriteLn("D3D12_SIGNATURE_PARAMETER_DESC: SemanticName: ", elDesc.SemanticName, " SemanticIndex: ", elDesc.SemanticIndex);
  Indent();
  WriteLn("Register: ", elDesc.Register);
  DumpEnum("SystemValueType", elDesc.SystemValueType);
  DumpEnum("ComponentType", elDesc.ComponentType);
  WriteLn("Mask: ", CompMaskToString(elDesc.Mask), " (", (UINT)elDesc.Mask, ")");
  WriteLn("ReadWriteMask: ", CompMaskToString(elDesc.ReadWriteMask), " (", (UINT)elDesc.ReadWriteMask, ") (AlwaysReads/NeverWrites)");
  WriteLn("Stream: ", elDesc.Stream);
  DumpEnum("MinPrecision", elDesc.MinPrecision);
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_SHADER_DESC &Desc) {
  WriteLn("D3D12_SHADER_DESC:");
  Indent();
  DumpShaderVersion(Desc.Version);
  WriteLn("Creator: ", Desc.Creator ? Desc.Creator : "<nullptr>");
  WriteLn("Flags: ", std::hex, std::showbase, Desc.Flags);  // TODO: fxc compiler flags
  WriteLn("ConstantBuffers: ", Desc.ConstantBuffers);
  WriteLn("BoundResources: ", Desc.BoundResources);
  WriteLn("InputParameters: ", Desc.InputParameters);
  WriteLn("OutputParameters: ", Desc.OutputParameters);
  hlsl::DXIL::ShaderKind ShaderKind = (hlsl::DXIL::ShaderKind)D3D12_SHVER_GET_TYPE(Desc.Version);
  if (ShaderKind == hlsl::DXIL::ShaderKind::Geometry) {
    WriteLn("cGSInstanceCount: ", Desc.cGSInstanceCount);
    WriteLn("GSMaxOutputVertexCount: ", Desc.GSMaxOutputVertexCount);
    DumpEnum("GSOutputTopology", Desc.GSOutputTopology);
    DumpEnum("InputPrimitive", Desc.InputPrimitive);
  }
  if (ShaderKind == hlsl::DXIL::ShaderKind::Hull) {
    WriteLn("PatchConstantParameters: ", Desc.PatchConstantParameters);
    WriteLn("cControlPoints: ", Desc.cControlPoints);
    DumpEnum("InputPrimitive", Desc.InputPrimitive);
    DumpEnum("HSOutputPrimitive", Desc.HSOutputPrimitive);
    DumpEnum("HSPartitioning", Desc.HSPartitioning);
    DumpEnum("TessellatorDomain", Desc.TessellatorDomain);
  }
  if (ShaderKind == hlsl::DXIL::ShaderKind::Domain) {
    WriteLn("PatchConstantParameters: ", Desc.PatchConstantParameters);
    WriteLn("cControlPoints: ", Desc.cControlPoints);
    DumpEnum("TessellatorDomain", Desc.TessellatorDomain);
  }
  if (ShaderKind == hlsl::DXIL::ShaderKind::Mesh) {
    WriteLn("PatchConstantParameters: ", Desc.PatchConstantParameters, " (output primitive parameters for mesh shader)");
  }
  // Instruction Counts
  WriteLn("InstructionCount: ", Desc.InstructionCount);
  WriteLn("TempArrayCount: ", Desc.TempArrayCount);
  WriteLn("DynamicFlowControlCount: ", Desc.DynamicFlowControlCount);
  WriteLn("ArrayInstructionCount: ", Desc.ArrayInstructionCount);
  WriteLn("TextureNormalInstructions: ", Desc.TextureNormalInstructions);
  WriteLn("TextureLoadInstructions: ", Desc.TextureLoadInstructions);
  WriteLn("TextureCompInstructions: ", Desc.TextureCompInstructions);
  WriteLn("TextureBiasInstructions: ", Desc.TextureBiasInstructions);
  WriteLn("TextureGradientInstructions: ", Desc.TextureGradientInstructions);
  WriteLn("FloatInstructionCount: ", Desc.FloatInstructionCount);
  WriteLn("IntInstructionCount: ", Desc.IntInstructionCount);
  WriteLn("UintInstructionCount: ", Desc.UintInstructionCount);
  WriteLn("CutInstructionCount: ", Desc.CutInstructionCount);
  WriteLn("EmitInstructionCount: ", Desc.EmitInstructionCount);
  WriteLn("cBarrierInstructions: ", Desc.cBarrierInstructions);
  WriteLn("cInterlockedInstructions: ", Desc.cInterlockedInstructions);
  WriteLn("cTextureStoreInstructions: ", Desc.cTextureStoreInstructions);

  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_FUNCTION_DESC &Desc) {
  SetLastName(Desc.Name);
  WriteLn("D3D12_FUNCTION_DESC: Name: ", EscapedString(m_LastName));
  Indent();
  DumpShaderVersion(Desc.Version);
  WriteLn("Creator: ", Desc.Creator ? Desc.Creator : "<nullptr>");
  WriteLn("Flags: ", std::hex, std::showbase, Desc.Flags);
  WriteLn("ConstantBuffers: ", Desc.ConstantBuffers);
  WriteLn("BoundResources: ", Desc.BoundResources);
  WriteLn("FunctionParameterCount: ", Desc.FunctionParameterCount);
  WriteLn("HasReturn: ", Desc.HasReturn ? "TRUE" : "FALSE");
  Dedent();
}
void D3DReflectionDumper::Dump(D3D12_LIBRARY_DESC &Desc) {
  WriteLn("D3D12_LIBRARY_DESC:");
  Indent();
  WriteLn("Creator: ", Desc.Creator ? Desc.Creator : "<nullptr>");
  WriteLn("Flags: ", std::hex, std::showbase, Desc.Flags);
  WriteLn("FunctionCount: ", Desc.FunctionCount);
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12ShaderReflectionType *pType) {
  WriteLn("ID3D12ShaderReflectionType:");
  Indent();
  D3D12_SHADER_TYPE_DESC tyDesc;
  if (!pType || FAILED(pType->GetDesc(&tyDesc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(tyDesc);
  if (tyDesc.Members) {
    WriteLn("{");
    Indent();
    for (UINT uMember = 0; uMember < tyDesc.Members; uMember++) {
      Dump(pType->GetMemberTypeByIndex(uMember));
    }
    Dedent();
    WriteLn("}");
  }
  Dedent();
}
void D3DReflectionDumper::Dump(ID3D12ShaderReflectionVariable *pVar) {
  WriteLn("ID3D12ShaderReflectionVariable:");
  Indent();
  D3D12_SHADER_VARIABLE_DESC varDesc;
  if (!pVar || FAILED(pVar->GetDesc(&varDesc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(varDesc);
  Dump(pVar->GetType());
  ID3D12ShaderReflectionConstantBuffer* pCB = pVar->GetBuffer();
  D3D12_SHADER_BUFFER_DESC CBDesc;
  if (pCB && SUCCEEDED(pCB->GetDesc(&CBDesc))) {
    WriteLn("CBuffer: ", CBDesc.Name);
  }
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12ShaderReflectionConstantBuffer *pCBReflection) {
  WriteLn("ID3D12ShaderReflectionConstantBuffer:");
  Indent();
  D3D12_SHADER_BUFFER_DESC Desc;
  if (!pCBReflection || FAILED(pCBReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.Variables) {
    WriteLn("{");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uVar = 0; uVar < Desc.Variables; uVar++) {
      if (m_bCheckByName)
        SetLastName();
      ID3D12ShaderReflectionVariable *pVar = pCBReflection->GetVariableByIndex(uVar);
      Dump(pVar);
      if (m_bCheckByName) {
        if (pCBReflection->GetVariableByName(m_LastName) != pVar) {
          bCheckByNameFailed = true;
          Failure("GetVariableByName ", m_LastName);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetVariableByName checks succeeded.");
    }
    Dedent();
    WriteLn("}");
  }
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12ShaderReflection *pShaderReflection) {
  WriteLn("ID3D12ShaderReflection:");
  Indent();
  D3D12_SHADER_DESC Desc;
  if (!pShaderReflection || FAILED(pShaderReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);

  if (Desc.InputParameters) {
    WriteLn("InputParameter Elements: ", Desc.InputParameters);
    Indent();
    for (UINT i = 0; i < Desc.InputParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC elDesc;
      if(FAILED(pShaderReflection->GetInputParameterDesc(i, &elDesc))) {
        Failure("GetInputParameterDesc ", i);
        continue;
      }
      Dump(elDesc);
    }
    Dedent();
  }
  if (Desc.OutputParameters) {
    WriteLn("OutputParameter Elements: ", Desc.OutputParameters);
    Indent();
    for (UINT i = 0; i < Desc.OutputParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC elDesc;
      if(FAILED(pShaderReflection->GetOutputParameterDesc(i, &elDesc))) {
        Failure("GetOutputParameterDesc ", i);
        continue;
      }
      Dump(elDesc);
    }
    Dedent();
  }
  if (Desc.PatchConstantParameters) {
    WriteLn("PatchConstantParameter Elements: ", Desc.PatchConstantParameters);
    Indent();
    for (UINT i = 0; i < Desc.PatchConstantParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC elDesc;
      if(FAILED(pShaderReflection->GetPatchConstantParameterDesc(i, &elDesc))) {
        Failure("GetPatchConstantParameterDesc ", i);
        continue;
      }
      Dump(elDesc);
    }
    Dedent();
  }

  if (Desc.ConstantBuffers) {
    WriteLn("Constant Buffers:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uCB = 0; uCB < Desc.ConstantBuffers; uCB++) {
      ID3D12ShaderReflectionConstantBuffer *pCB = pShaderReflection->GetConstantBufferByIndex(uCB);
      if (!pCB) {
        Failure("GetConstantBufferByIndex ", uCB);
        continue;
      }
      Dump(pCB);
      if (m_bCheckByName && m_LastName) {
        if (pShaderReflection->GetConstantBufferByName(m_LastName) != pCB) {
          bCheckByNameFailed = true;
          Failure("GetConstantBufferByName ", m_LastName);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetConstantBufferByName checks succeeded.");
    }
    Dedent();
  }
  if (Desc.BoundResources) {
    WriteLn("Bound Resources:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uRes = 0; uRes < Desc.BoundResources; uRes++) {
      D3D12_SHADER_INPUT_BIND_DESC bindDesc;
      if (FAILED(pShaderReflection->GetResourceBindingDesc(uRes, &bindDesc))) {
        Failure("GetResourceBindingDesc ", uRes);
        continue;
      }
      Dump(bindDesc);
      if (m_bCheckByName && bindDesc.Name) {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc2;
        if (FAILED(pShaderReflection->GetResourceBindingDescByName(bindDesc.Name, &bindDesc2)) || bindDesc2.Name != bindDesc.Name) {
          bCheckByNameFailed = true;
          Failure("GetResourceBindingDescByName ", bindDesc.Name);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetResourceBindingDescByName checks succeeded.");
    }
    Dedent();
  }
  // TODO
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12FunctionReflection *pFunctionReflection) {
  WriteLn("ID3D12FunctionReflection:");
  Indent();
  D3D12_FUNCTION_DESC Desc;
  if (!pFunctionReflection || FAILED(pFunctionReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.ConstantBuffers) {
    WriteLn("Constant Buffers:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uCB = 0; uCB < Desc.ConstantBuffers; uCB++) {
      ID3D12ShaderReflectionConstantBuffer *pCB = pFunctionReflection->GetConstantBufferByIndex(uCB);
      Dump(pCB);
      if (m_bCheckByName && m_LastName) {
        if (pFunctionReflection->GetConstantBufferByName(m_LastName) != pCB) {
          bCheckByNameFailed = true;
          Failure("GetConstantBufferByName ", m_LastName);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetConstantBufferByName checks succeeded.");
    }
    Dedent();
  }
  if (Desc.BoundResources) {
    WriteLn("Bound Resources:");
    Indent();
    bool bCheckByNameFailed = false;
    for (UINT uRes = 0; uRes < Desc.BoundResources; uRes++) {
      D3D12_SHADER_INPUT_BIND_DESC bindDesc;
      if (FAILED(pFunctionReflection->GetResourceBindingDesc(uRes, &bindDesc))) {
      }
      Dump(bindDesc);
      if (m_bCheckByName && bindDesc.Name) {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc2;
        if (FAILED(pFunctionReflection->GetResourceBindingDescByName(bindDesc.Name, &bindDesc2)) || bindDesc2.Name != bindDesc.Name) {
          bCheckByNameFailed = true;
          Failure("GetResourceBindingDescByName ", bindDesc.Name);
        }
      }
    }
    if (m_bCheckByName && !bCheckByNameFailed) {
      WriteLn("GetResourceBindingDescByName checks succeeded.");
    }
    Dedent();
  }
  // TODO
  Dedent();
}

void D3DReflectionDumper::Dump(ID3D12LibraryReflection *pLibraryReflection) {
  WriteLn("ID3D12LibraryReflection:");
  Indent();
  D3D12_LIBRARY_DESC Desc;
  if (!pLibraryReflection || FAILED(pLibraryReflection->GetDesc(&Desc))) {
    Failure("GetDesc");
    Dedent();
    return;
  }
  Dump(Desc);
  if (Desc.FunctionCount) {
    for (UINT uFunc = 0; uFunc < Desc.FunctionCount; uFunc++)
      Dump(pLibraryReflection->GetFunctionByIndex((INT)uFunc));
  }
  Dedent();
}

} // namespace dump
} // namespace hlsl
