/* =========================================================================

  Program:   EasyBlendSDKDX10
  Language:  C++
  Date:      $Date: $
  Version:   $Revision: $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved
  The source code contained herein is confidential and is considered a 
  trade secret of Scalable Display Technologies, Inc

===================================================================auto== */

#ifndef _EasyBlendSDKDXVer_H_
#define _EasyBlendSDKDXVer_H_


// This #define important to decide whether to use DX 10 or 11
#ifndef EASYBLEND_DX_VER
 //#define EASYBLEND_DX_VER 10
 #define EASYBLEND_DX_VER 11
#endif

#if EASYBLEND_DX_VER==10
#include <d3d10_1.h>
 #include <d3d10.h>
 #include <d3dcompiler.h>
#include <DirectXMath.h>
 #define D3D1X_SDK_VERSION   D3D10_SDK_VERSION
    #define D3D1X_DRIVER_TYPE_HARDWARE D3D10_DRIVER_TYPE_HARDWARE
    #define D3D1X_CREATE_DEVICE_DEBUG D3D10_CREATE_DEVICE_DEBUG
 
 typedef ID3D10Device    ID3D1XDevice;   
 typedef void      ID3D1XDeviceContext;
 typedef ID3D10RenderTargetView  ID3D1XRenderTargetView;
 typedef ID3D10Texture2D    ID3D1XTexture2D;  
 typedef ID3D10Texture3D    ID3D1XTexture3D;  
 typedef ID3D10Resource    ID3D1XResource;
 typedef ID3D10DepthStencilView  ID3D1XDepthStencilView;
 typedef ID3D10InputLayout   ID3D1XInputLayout;
 typedef ID3D10Buffer    ID3D1XBuffer;            
 typedef ID3D10Effect    ID3D1XEffect;    
 typedef ID3D10EffectTechnique  ID3D1XEffectTechnique;
 typedef ID3D10BlendState   ID3D1XBlendState;  
 typedef ID3D10DepthStencilState  ID3D1XDepthStencilState;
 typedef ID3D10RasterizerState  ID3D1XRasterizerState;
 typedef ID3D10EffectMatrixVariable ID3D1XEffectMatrixVariable;
 typedef ID3D10EffectShaderResourceVariable ID3D1XEffectShaderResourceVariable;
 typedef ID3D10EffectVectorVariable ID3D1XEffectVectorVariable;
 typedef ID3D10EffectScalarVariable ID3D1XEffectScalarVariable;
 typedef ID3D10StateBlock   ID3D1XStateBlock;
 typedef ID3D10ShaderResourceView ID3D1XShaderResourceView;
 typedef ID3D10PixelShader ID3D1XPixelShader;
 typedef ID3D10VertexShader ID3D1XVertexShader;

#define D3DX_DRIVER_TYPE D3D10_DRIVER_TYPE
#define D3DX_DRIVER_TYPE_HARDWARE D3D10_DRIVER_TYPE_HARDWARE
#define D3DX_DRIVER_TYPE_WARP D3D10_DRIVER_TYPE_WARP
#define D3DX_DRIVER_TYPE_SOFTWARE D3D10_DRIVER_TYPE_SOFTWARE
#define D3DX_DRIVER_TYPE_REFERENCE D3D10_DRIVER_TYPE_REFERENCE
#define D3DX_DRIVER_TYPE_NULL D3D10_DRIVER_TYPE_NULL

 typedef D3D10_INPUT_ELEMENT_DESC D3D1X_INPUT_ELEMENT_DESC;
 #define D3D1X_INPUT_PER_VERTEX_DATA D3D10_INPUT_PER_VERTEX_DATA
                
 #define D3D1X_BIND_DEPTH_STENCIL D3D10_BIND_DEPTH_STENCIL
 #define D3D1X_BIND_VERTEX_BUFFER D3D10_BIND_VERTEX_BUFFER
 #define D3D1X_BIND_INDEX_BUFFER  D3D10_BIND_INDEX_BUFFER
 #define D3D1X_BIND_RENDER_TARGET D3D10_BIND_RENDER_TARGET
 #define D3D1X_BIND_SHADER_RESOURCE D3D10_BIND_SHADER_RESOURCE

 typedef D3D10_PASS_DESC    D3D1X_PASS_DESC;
 typedef D3D10_BUFFER_DESC   D3D1X_BUFFER_DESC;
 typedef D3D10_BLEND_DESC   D3D1X_BLEND_DESC;
 typedef D3D10_DEPTH_STENCIL_DESC D3D1X_DEPTH_STENCIL_DESC;
 typedef D3D10_RASTERIZER_DESC  D3D1X_RASTERIZER_DESC;
 typedef D3D10_TECHNIQUE_DESC  D3D1X_TECHNIQUE_DESC;
 typedef D3D10_DEPTH_STENCIL_VIEW_DESC D3D1X_DEPTH_STENCIL_VIEW_DESC;
 typedef D3D10_SHADER_RESOURCE_VIEW_DESC D3D1X_SHADER_RESOURCE_VIEW_DESC;
 typedef D3D10_TEXTURE2D_DESC  D3D1X_TEXTURE2D_DESC;
 typedef CD3D10_TEXTURE2D_DESC  CD3D1X_TEXTURE2D_DESC;
 typedef D3D10_TEXTURE3D_DESC  D3D1X_TEXTURE3D_DESC;
 typedef CD3D10_TEXTURE3D_DESC  CD3D1X_TEXTURE3D_DESC;

 #define D3D1X_DSV_DIMENSION_TEXTURE2D D3D10_DSV_DIMENSION_TEXTURE2D
 #define D3D1X_SRV_DIMENSION_TEXTURE2D D3D10_SRV_DIMENSION_TEXTURE2D
 #define D3D1X_SRV_DIMENSION_TEXTURE3D D3D10_SRV_DIMENSION_TEXTURE3D
 #define D3D1X_SHADER_ENABLE_STRICTNESS D3D10_SHADER_ENABLE_STRICTNESS

 typedef D3D10_STATE_BLOCK_MASK D3D1X_STATE_BLOCK_MASK;

 typedef D3D10_VIEWPORT    D3D1X_VIEWPORT;
 typedef ID3D10Blob     ID3D1XBlob;

 #define D3D1X_USAGE_DEFAULT   D3D10_USAGE_DEFAULT
 #define D3D1X_USAGE_IMMUTABLE  D3D10_USAGE_IMMUTABLE
 #define D3D1X_USAGE_DYNAMIC D3D10_USAGE_DYNAMIC
#define D3D1X_BIND_CONSTANT_BUFFER D3D10_BIND_CONSTANT_BUFFER
#define D3D1X_CPU_ACCESS_WRITE D3D10_CPU_ACCESS_WRITE

 typedef D3D10_SUBRESOURCE_DATA  D3D1X_SUBRESOURCE_DATA;

 #define D3D1X_COLOR_WRITE_ENABLE_ALL D3D10_COLOR_WRITE_ENABLE_ALL
 #define D3D1X_BLEND_SRC_ALPHA  D3D10_BLEND_SRC_ALPHA
    #define D3D1X_BLEND_INV_SRC_ALPHA   D3D10_BLEND_INV_SRC_ALPHA
    #define D3D1X_BLEND_OP_ADD   D3D10_BLEND_OP_ADD
    #define D3D1X_BLEND_ONE    D3D10_BLEND_ONE
    #define D3D1X_BLEND_ZERO   D3D10_BLEND_ZERO
    #define D3D1X_BLEND_OP_ADD   D3D10_BLEND_OP_ADD

 #define D3D1X_DEPTH_WRITE_MASK_ALL       D3D10_DEPTH_WRITE_MASK_ALL
    #define D3D1X_COMPARISON_LESS    D3D10_COMPARISON_LESS
    #define D3D1X_DEFAULT_STENCIL_READ_MASK  D3D10_DEFAULT_STENCIL_READ_MASK
    #define D3D1X_DEFAULT_STENCIL_WRITE_MASK D3D10_DEFAULT_STENCIL_WRITE_MASK
    #define D3D1X_STENCIL_OP_ZERO    D3D10_STENCIL_OP_ZERO
    #define D3D1X_COMPARISON_ALWAYS    D3D10_COMPARISON_ALWAYS

 #define D3D1X_FILL_SOLID   D3D10_FILL_SOLID
 #define D3D1X_CULL_BACK    D3D10_CULL_BACK


 #define D3D1X_CLEAR_DEPTH   D3D10_CLEAR_DEPTH
 #define D3D1X_PRIMITIVE_TOPOLOGY_TRIANGLELIST D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST

 
 #define D3D1XCreateDeviceAndSwapChain D3D10CreateDeviceAndSwapChain
 #define D3D1XCreateStateBlock  D3D10CreateStateBlock

 #define D3D1X_BYTES_FROM_BITS(X) D3D10_BYTES_FROM_BITS(X)
 #define D3D1X_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

 // Math
 typedef DirectX::XMFLOAT2  XVECTOR2;
 typedef DirectX::XMFLOAT3  XVECTOR3;
 typedef DirectX::XMFLOAT4  XVECTOR4;
 typedef DirectX::XMMATRIX  XMATRIX;
 #define XMatrixLookAtLH    DirectX::XMMatrixLookAtLH
 #define XMatrixOrthoLH     DirectX::XMMatrixOrthographicLH
 #define XMatrixPerspectiveOffCenterRH DirectX::XMMatrixPerspectiveOffCenterRH
 #define XMatrixPerspectiveFovRH DirectX::XMMatrixPerspectiveFovRH

    #define XMatrixIdentity    DirectX::XMMatrixIdentity
 #define XVector3Cross    DirectX::XMVector3Cross
 #define XMatrixRotationX   DirectX::XMMatrixRotationX
    #define XMatrixRotationY   DirectX::XMMatrixRotationY
    #define XMatrixRotationZ   DirectX::XMMatrixRotationZ
 #define XMatrixTranslation DirectX::XMMatrixTranslation
    #define XMatrixMultiply    DirectX::XMMatrixMultiply 

#ifndef D3DX_PI
 #define D3DX_PI DirectX::XM_PI
#endif // D3DX_PI
#elif EASYBLEND_DX_VER==11
 // DX11

//#include <d3d11.h>
// #include <d3dcompiler.h>
// #include <DirectXMath.h>  

 // Effects11 lib, from the Samples dir in the DX SDK (June 2010 say), just compiled and moved to local to this lib. Used to load .fx files
// #include <d3dx11effect.h>

 #define D3D1X_SDK_VERSION   D3D11_SDK_VERSION
    #define D3D1X_DRIVER_TYPE_HARDWARE D3D_DRIVER_TYPE_HARDWARE
    #define D3D1X_CREATE_DEVICE_DEBUG D3D11_CREATE_DEVICE_DEBUG
 
 typedef ID3D11Device    ID3D1XDevice;   
 typedef ID3D11DeviceContext   ID3D1XDeviceContext;
 typedef ID3D11RenderTargetView  ID3D1XRenderTargetView;
 typedef ID3D11Texture2D    ID3D1XTexture2D;  
 typedef ID3D11Texture3D    ID3D1XTexture3D;  
 typedef ID3D11Resource    ID3D1XResource;

#if 0
 typedef ID3D11DepthStencilView  ID3D1XDepthStencilView;
 typedef ID3D11InputLayout   ID3D1XInputLayout;
 typedef ID3D11Buffer    ID3D1XBuffer;            
 typedef ID3DX11Effect    ID3D1XEffect;    
 typedef ID3DX11EffectTechnique  ID3D1XEffectTechnique;
 typedef ID3D11BlendState   ID3D1XBlendState;  
 typedef ID3D11DepthStencilState  ID3D1XDepthStencilState;
 typedef ID3D11RasterizerState  ID3D1XRasterizerState;
 typedef ID3DX11EffectMatrixVariable ID3D1XEffectMatrixVariable;
 typedef ID3DX11EffectShaderResourceVariable ID3D1XEffectShaderResourceVariable;
 typedef ID3DX11EffectVectorVariable ID3D1XEffectVectorVariable;
 typedef ID3DX11EffectScalarVariable ID3D1XEffectScalarVariable;
 //typedef ID3DX11EffectStateBlock   ID3D1XStateBlock;
 typedef ID3D11ShaderResourceView ID3D1XShaderResourceView;
 typedef ID3D11PixelShader ID3D1XPixelShader;
 typedef ID3D11VertexShader ID3D1XVertexShader;

#define D3DX_DRIVER_TYPE D3D_DRIVER_TYPE
#define D3DX_DRIVER_TYPE_HARDWARE D3D_DRIVER_TYPE_HARDWARE
#define D3DX_DRIVER_TYPE_WARP D3D_DRIVER_TYPE_WARP
#define D3DX_DRIVER_TYPE_SOFTWARE D3D_DRIVER_TYPE_SOFTWARE
#define D3DX_DRIVER_TYPE_REFERENCE D3D_DRIVER_TYPE_REFERENCE
#define D3DX_DRIVER_TYPE_NULL D3D_DRIVER_TYPE_NULL

 typedef D3D11_INPUT_ELEMENT_DESC D3D1X_INPUT_ELEMENT_DESC;
 #define D3D1X_INPUT_PER_VERTEX_DATA D3D11_INPUT_PER_VERTEX_DATA
                
 #define D3D1X_BIND_DEPTH_STENCIL D3D11_BIND_DEPTH_STENCIL
 #define D3D1X_BIND_VERTEX_BUFFER D3D11_BIND_VERTEX_BUFFER
 #define D3D1X_BIND_INDEX_BUFFER  D3D11_BIND_INDEX_BUFFER
 #define D3D1X_BIND_RENDER_TARGET D3D11_BIND_RENDER_TARGET
 #define D3D1X_BIND_SHADER_RESOURCE D3D11_BIND_SHADER_RESOURCE

 typedef D3DX11_PASS_DESC   D3D1X_PASS_DESC;
 typedef D3D11_BUFFER_DESC   D3D1X_BUFFER_DESC;
 typedef D3D11_BLEND_DESC   D3D1X_BLEND_DESC;
 typedef D3D11_DEPTH_STENCIL_DESC D3D1X_DEPTH_STENCIL_DESC;
 typedef D3D11_RASTERIZER_DESC  D3D1X_RASTERIZER_DESC;

 typedef D3DX11_TECHNIQUE_DESC  D3D1X_TECHNIQUE_DESC;
 typedef D3D11_DEPTH_STENCIL_VIEW_DESC D3D1X_DEPTH_STENCIL_VIEW_DESC;
 typedef D3D11_SHADER_RESOURCE_VIEW_DESC D3D1X_SHADER_RESOURCE_VIEW_DESC;
 typedef D3D11_TEXTURE2D_DESC  D3D1X_TEXTURE2D_DESC;
 typedef CD3D11_TEXTURE2D_DESC  CD3D1X_TEXTURE2D_DESC;
 typedef D3D11_TEXTURE3D_DESC  D3D1X_TEXTURE3D_DESC;
 typedef CD3D11_TEXTURE3D_DESC  CD3D1X_TEXTURE3D_DESC;
  
 #define D3D1X_DSV_DIMENSION_TEXTURE2D D3D11_DSV_DIMENSION_TEXTURE2D
 #define D3D1X_SRV_DIMENSION_TEXTURE2D D3D11_SRV_DIMENSION_TEXTURE2D
 #define D3D1X_SRV_DIMENSION_TEXTURE3D D3D11_SRV_DIMENSION_TEXTURE3D
 #define D3D1X_SHADER_ENABLE_STRICTNESS 0

 typedef D3DX11_STATE_BLOCK_MASK D3D1X_STATE_BLOCK_MASK;

 typedef D3D11_VIEWPORT    D3D1X_VIEWPORT;
 typedef ID3DBlob     ID3D1XBlob;

 #define D3D1X_USAGE_DEFAULT   D3D11_USAGE_DEFAULT
 #define D3D1X_USAGE_IMMUTABLE  D3D11_USAGE_IMMUTABLE
 #define D3D1X_USAGE_DYNAMIC D3D11_USAGE_DYNAMIC
#define D3D1X_BIND_CONSTANT_BUFFER D3D11_BIND_CONSTANT_BUFFER
#define D3D1X_CPU_ACCESS_WRITE D3D11_CPU_ACCESS_WRITE

 typedef D3D11_SUBRESOURCE_DATA  D3D1X_SUBRESOURCE_DATA;

 #define D3D1X_COLOR_WRITE_ENABLE_ALL D3D11_COLOR_WRITE_ENABLE_ALL
 #define D3D1X_BLEND_SRC_ALPHA  D3D11_BLEND_SRC_ALPHA
    #define D3D1X_BLEND_INV_SRC_ALPHA   D3D11_BLEND_INV_SRC_ALPHA
    #define D3D1X_BLEND_OP_ADD   D3D11_BLEND_OP_ADD
    #define D3D1X_BLEND_ONE    D3D11_BLEND_ONE
    #define D3D1X_BLEND_ZERO   D3D11_BLEND_ZERO
    #define D3D1X_BLEND_OP_ADD   D3D11_BLEND_OP_ADD

 #define D3D1X_DEPTH_WRITE_MASK_ALL       D3D11_DEPTH_WRITE_MASK_ALL
    #define D3D1X_COMPARISON_LESS    D3D11_COMPARISON_LESS
    #define D3D1X_DEFAULT_STENCIL_READ_MASK  D3D11_DEFAULT_STENCIL_READ_MASK
    #define D3D1X_DEFAULT_STENCIL_WRITE_MASK D3D11_DEFAULT_STENCIL_WRITE_MASK
    #define D3D1X_STENCIL_OP_ZERO    D3D11_STENCIL_OP_ZERO
    #define D3D1X_COMPARISON_ALWAYS    D3D11_COMPARISON_ALWAYS

 #define D3D1X_FILL_SOLID   D3D11_FILL_SOLID
 #define D3D1X_CULL_BACK    D3D11_CULL_BACK

 #define D3D1X_CLEAR_DEPTH   D3D11_CLEAR_DEPTH
 #define D3D1X_PRIMITIVE_TOPOLOGY_TRIANGLELIST D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST

 
 #define D3D1XCreateDeviceAndSwapChain D3D11CreateDeviceAndSwapChain
 #define D3D1XCreateStateBlock  D3D11CreateStateBlock

 #ifndef D3D11_BYTES_FROM_BITS
  #define D3D11_BYTES_FROM_BITS(x) (((x) + 7) / 8)
 #endif 

 #define D3D1X_BYTES_FROM_BITS(X) D3D11_BYTES_FROM_BITS(X)
 #define D3D1X_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

 // Math
 typedef DirectX::XMFLOAT2  XVECTOR2;
 typedef DirectX::XMFLOAT3  XVECTOR3;
 typedef DirectX::XMFLOAT4  XVECTOR4;
    typedef DirectX::XMMATRIX  XMATRIX;
 #define XMatrixLookAtLH    DirectX::XMMatrixLookAtLH
    #define XMatrixOrthoLH     DirectX::XMMatrixOrthographicLH
 #define XMatrixPerspectiveOffCenterRH DirectX::XMMatrixPerspectiveOffCenterRH
 #define XMatrixPerspectiveFovRH DirectX::XMMatrixPerspectiveFovRH

    #define XMatrixIdentity    DirectX::XMMatrixIdentity
 #define XVector3Cross    DirectX::XMVector3Cross
 #define XMatrixRotationX   DirectX::XMMatrixRotationX
    #define XMatrixRotationY   DirectX::XMMatrixRotationY
    #define XMatrixRotationZ   DirectX::XMMatrixRotationZ
 #define XMatrixTranslation DirectX::XMMatrixTranslation
    #define XMatrixMultiply    DirectX::XMMatrixMultiply 
#endif

#ifndef D3DX_PI
 #define D3DX_PI DirectX::XM_PI
#endif // D3DX_PI
#else
 
#error Unsupported DirecX version. 11.1 

#endif // DX10/11

#endif // Header
