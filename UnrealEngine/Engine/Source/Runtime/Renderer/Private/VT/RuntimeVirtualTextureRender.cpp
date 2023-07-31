// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/RuntimeVirtualTextureRender.h"

#include "Components/RuntimeVirtualTextureComponent.h"
#include "GlobalShader.h"
#include "GPUScene.h"
#include "MaterialShader.h"
#include "MeshPassProcessor.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderUtils.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ShaderBaseClasses.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureSceneProxy.h"
#include "MeshPassProcessor.inl"
#include "RenderCaptureInterface.h"

namespace RuntimeVirtualTexture
{
    int32 RenderCaptureNextRVTPagesDraws = 0;
    static FAutoConsoleVariableRef CVarRenderCaptureNextRVTPagesDraws(
	    TEXT("r.VT.RenderCaptureNextPagesDraws"),
	    RenderCaptureNextRVTPagesDraws,
	    TEXT("Trigger a render capture during the next RVT RenderPages draw calls."));

	BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FEtcParameters, )
		SHADER_PARAMETER_ARRAY(FVector4f, ALPHA_DISTANCE_TABLES, [16])
		SHADER_PARAMETER_ARRAY(FVector4f, RGB_DISTANCE_TABLES, [8])
	END_GLOBAL_SHADER_PARAMETER_STRUCT()

	IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FEtcParameters, "EtcParameters");

	class FEtcParametersUniformBuffer : public TUniformBuffer<FEtcParameters>
	{
		typedef TUniformBuffer<FEtcParameters> Super;
	public:
		FEtcParametersUniformBuffer()
		{
			FEtcParameters Parameters;
			Parameters.ALPHA_DISTANCE_TABLES[0] = FVector4f(2, 5, 8, 14);
			Parameters.ALPHA_DISTANCE_TABLES[1] = FVector4f(2, 6, 9, 12);
			Parameters.ALPHA_DISTANCE_TABLES[2] = FVector4f(1, 4, 7, 12);
			Parameters.ALPHA_DISTANCE_TABLES[3] = FVector4f(1, 3, 5, 12);
			Parameters.ALPHA_DISTANCE_TABLES[4] = FVector4f(2, 5, 7, 11);
			Parameters.ALPHA_DISTANCE_TABLES[5] = FVector4f(2, 6, 8, 10);
			Parameters.ALPHA_DISTANCE_TABLES[6] = FVector4f(3, 6, 7, 10);
			Parameters.ALPHA_DISTANCE_TABLES[7] = FVector4f(2, 4, 7, 10);
			Parameters.ALPHA_DISTANCE_TABLES[8] = FVector4f(1, 5, 7, 9);
			Parameters.ALPHA_DISTANCE_TABLES[9] = FVector4f(1, 4, 7, 9);
			Parameters.ALPHA_DISTANCE_TABLES[10] = FVector4f(1, 3, 7, 9);
			Parameters.ALPHA_DISTANCE_TABLES[11] = FVector4f(1, 4, 6, 9);
			Parameters.ALPHA_DISTANCE_TABLES[12] = FVector4f(2, 3, 6, 9);
			Parameters.ALPHA_DISTANCE_TABLES[13] = FVector4f(0, 1, 2, 9);
			Parameters.ALPHA_DISTANCE_TABLES[14] = FVector4f(3, 5, 7, 8);
			Parameters.ALPHA_DISTANCE_TABLES[15] = FVector4f(2, 4, 6, 8);

			Parameters.RGB_DISTANCE_TABLES[0] = FVector4f(-8, -2, 2, 8);
			Parameters.RGB_DISTANCE_TABLES[1] = FVector4f(-17, -5, 5, 17);
			Parameters.RGB_DISTANCE_TABLES[2] = FVector4f(-29, -9, 9, 29);
			Parameters.RGB_DISTANCE_TABLES[3] = FVector4f(-42, -13, 13, 42);
			Parameters.RGB_DISTANCE_TABLES[4] = FVector4f(-60, -18, 18, 60);
			Parameters.RGB_DISTANCE_TABLES[5] = FVector4f(-80, -24, 24, 80);
			Parameters.RGB_DISTANCE_TABLES[6] = FVector4f(-106, -33, 33, 106);
			Parameters.RGB_DISTANCE_TABLES[7] = FVector4f(-183, -47, 47, 183);

			SetContents(Parameters);
		}
	};

	const TUniformBufferRef<FEtcParameters>& GetEtcParametersUniformBufferRef()
	{
		check(IsInRenderingThread());
		static TGlobalResource<FEtcParametersUniformBuffer> EtcParametersUniformBuffer;
		return EtcParametersUniformBuffer.GetUniformBufferRef();
	}

	bool UseEtcProfile(EShaderPlatform ShaderPlatform)
	{
		switch (ShaderPlatform)
		{
		case SP_METAL:
		case SP_METAL_MRT:
		case SP_METAL_TVOS:
		case SP_METAL_MRT_TVOS:
		case SP_VULKAN_ES3_1_ANDROID:
		case SP_OPENGL_ES3_1_ANDROID:
		case SP_VULKAN_SM5_ANDROID:
			return true;
		default:
			break;
		}
		return false;
	}

	/** Mesh material shader for writing to the virtual texture. */
	class FShader_VirtualTextureMaterialDraw : public FMeshMaterialShader
	{
	public:
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
			SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
		{
			return UseVirtualTexturing(GetMaxSupportedFeatureLevel(Parameters.Platform)) &&
				(Parameters.MaterialParameters.bHasRuntimeVirtualTextureOutput || Parameters.MaterialParameters.bIsDefaultMaterial);
		}

		static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("VIRTUAL_TEXTURE_PAGE_RENDER"), 1);
			OutEnvironment.SetDefine(TEXT("IS_VIRTUAL_TEXTURE_MATERIAL"), 1);
		}

		FShader_VirtualTextureMaterialDraw()
		{}

		FShader_VirtualTextureMaterialDraw(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
			: FMeshMaterialShader(Initializer)
		{
		}

		template <typename TRHICmdList>
		void SetParameters(TRHICmdList& RHICmdList, FSceneView const& View, FMaterialRenderProxy const& MaterialProxy, FMaterial const& Material)
		{
			FMeshMaterialShader::SetParameters(
				RHICmdList,
				RHICmdList.GetBoundPixelShader(),
				&MaterialProxy,
				Material,
				View,
				View.ViewUniformBuffer,
				ESceneTextureSetupMode::All);
		}
	};


	/** Specialization for ERuntimeVirtualTextureMaterialType::BaseColor */
	class FMaterialPolicy_BaseColor
	{
	public:
		static void ModifyCompilationEnvironment(FShaderCompilerEnvironment& OutEnvironment)
		{
			OutEnvironment.SetDefine(TEXT("OUT_BASECOLOR"), 1);
		}

		static FRHIBlendState* GetBlendState(uint8 OutputAttributeMask)
		{
			return TStaticBlendState< CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One >::GetRHI();
		}
	};

	/** Specialization for ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular */
	class FMaterialPolicy_BaseColorNormalSpecular
	{
	private:
		/** Compile time helper to build blend state from the connected output attribute mask. */
		static constexpr EColorWriteMask GetColorMaskFromAttributeMask(uint8 AttributeMask, uint8 RenderTargetIndex)
		{
			// Color mask in the output render targets for each of the relevant attributes in ERuntimeVirtualTextureAttributeType
			const EColorWriteMask AttributeMasks[][3] = {
				{ CW_RGBA, CW_NONE, CW_NONE }, // BaseColor
				{ CW_NONE, EColorWriteMask(CW_RED | CW_GREEN | CW_ALPHA), EColorWriteMask(CW_BLUE | CW_ALPHA) }, // Normal
				{ CW_NONE, CW_NONE, EColorWriteMask(CW_GREEN | CW_ALPHA) }, // Roughness
				{ CW_NONE, CW_NONE, EColorWriteMask(CW_RED | CW_ALPHA) }, // Specular
				{ CW_NONE, EColorWriteMask(CW_BLUE | CW_ALPHA), CW_NONE }, // Mask
			};

			// Combine the color masks for this AttributeMask
			EColorWriteMask ColorWriteMask = CW_NONE;
			for (int32 i = 0; i < 5; ++i)
			{
				if (AttributeMask & (1 << i))
				{
					ColorWriteMask = EColorWriteMask(ColorWriteMask | AttributeMasks[i][RenderTargetIndex]);
				}
			}
			return ColorWriteMask;
		}

		/** Helper to convert the connected output attribute mask to a blend state with a color mask for these attributes. */
		template< uint32 AttributeMask >
		static FRHIBlendState* TGetBlendStateFromAttributeMask()
		{
			return TStaticBlendState<
				GetColorMaskFromAttributeMask(AttributeMask, 0), BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,
				GetColorMaskFromAttributeMask(AttributeMask, 1), BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,
				GetColorMaskFromAttributeMask(AttributeMask, 2), BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One	>::GetRHI();
		}

		/** Runtime conversion of attribute mask to static blend state. */
		static FRHIBlendState* GetBlendStateImpl(uint8 AttributeMask)
		{
			// We have 5 relevant bits in the attribute mask. Any more and this would get painful...
			switch (AttributeMask & 0x1f)
			{
			case 1: return TGetBlendStateFromAttributeMask<1>();
			case 2: return TGetBlendStateFromAttributeMask<2>();
			case 3: return TGetBlendStateFromAttributeMask<3>();
			case 4: return TGetBlendStateFromAttributeMask<4>();
			case 5: return TGetBlendStateFromAttributeMask<5>();
			case 6: return TGetBlendStateFromAttributeMask<6>();
			case 7: return TGetBlendStateFromAttributeMask<7>();
			case 8: return TGetBlendStateFromAttributeMask<8>();
			case 9: return TGetBlendStateFromAttributeMask<9>();
			case 10: return TGetBlendStateFromAttributeMask<10>();
			case 11: return TGetBlendStateFromAttributeMask<11>();
			case 12: return TGetBlendStateFromAttributeMask<12>();
			case 13: return TGetBlendStateFromAttributeMask<13>();
			case 14: return TGetBlendStateFromAttributeMask<14>();
			case 15: return TGetBlendStateFromAttributeMask<15>();
			case 16: return TGetBlendStateFromAttributeMask<16>();
			case 17: return TGetBlendStateFromAttributeMask<17>();
			case 18: return TGetBlendStateFromAttributeMask<18>();
			case 19: return TGetBlendStateFromAttributeMask<19>();
			case 21: return TGetBlendStateFromAttributeMask<21>();
			case 22: return TGetBlendStateFromAttributeMask<22>();
			case 23: return TGetBlendStateFromAttributeMask<23>();
			case 24: return TGetBlendStateFromAttributeMask<24>();
			case 25: return TGetBlendStateFromAttributeMask<25>();
			case 26: return TGetBlendStateFromAttributeMask<26>();
			case 27: return TGetBlendStateFromAttributeMask<27>();
			case 28: return TGetBlendStateFromAttributeMask<28>();
			case 29: return TGetBlendStateFromAttributeMask<29>();
			case 30: return TGetBlendStateFromAttributeMask<30>();
			default: return TGetBlendStateFromAttributeMask<31>();
			}
		}

	public:
		static void ModifyCompilationEnvironment(FShaderCompilerEnvironment& OutEnvironment)
		{
			OutEnvironment.SetDefine(TEXT("OUT_BASECOLOR_NORMAL_SPECULAR"), 1);
		}

		static FRHIBlendState* GetBlendState(uint8 OutputAttributeMask)
		{
			return GetBlendStateImpl(OutputAttributeMask);
		}
	};

	/** Specialization for ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness */
	class FMaterialPolicy_BaseColorNormalRoughness
	{
	private:
		/** Compile time helper to build blend state from the connected output attribute mask. */
		static constexpr EColorWriteMask GetColorMaskFromAttributeMask(uint8 AttributeMask, uint8 RenderTargetIndex)
		{
			// Color mask in the output render targets for each of the relevant attributes in ERuntimeVirtualTextureAttributeType
			const EColorWriteMask AttributeMasks[][2] = {
				{ CW_RGBA, CW_NONE}, // BaseColor
				{ CW_NONE, EColorWriteMask(CW_RED| CW_BLUE | CW_ALPHA)}, // Normal
				{ CW_NONE, EColorWriteMask(CW_GREEN | CW_ALPHA)}, // Roughness
				{ CW_NONE, CW_NONE}, // Specular
				{ CW_NONE, CW_NONE}, // Mask
			};

			// Combine the color masks for this AttributeMask
			EColorWriteMask ColorWriteMask = CW_NONE;
			for (int32 i = 0; i < 5; ++i)
			{
				if (AttributeMask & (1 << i))
				{
					ColorWriteMask = EColorWriteMask(ColorWriteMask | AttributeMasks[i][RenderTargetIndex]);
				}
			}
			return ColorWriteMask;
		}
		/** Helper to convert the connected output attribute mask to a blend state with a color mask for these attributes. */
		template< uint32 AttributeMask >
		static FRHIBlendState* TGetBlendStateFromAttributeMask()
		{
			return TStaticBlendState<
				GetColorMaskFromAttributeMask(AttributeMask, 0), BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One,
				// normal XY is stored in R and B channels, and the Sign of Z is considered always positive
				GetColorMaskFromAttributeMask(AttributeMask, 1), BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
		}
		/** Runtime conversion of attribute mask to static blend state. */
		static FRHIBlendState* GetBlendStateImpl(uint8 AttributeMask)
		{
			// We have 5 relevant bits in the attribute mask. Any more and this would get painful...
			switch (AttributeMask & 0x1f)
			{
			case 1: return TGetBlendStateFromAttributeMask<1>();
			case 2: return TGetBlendStateFromAttributeMask<2>();
			case 3: return TGetBlendStateFromAttributeMask<3>();
			case 4: return TGetBlendStateFromAttributeMask<4>();
			case 5: return TGetBlendStateFromAttributeMask<5>();
			case 6: return TGetBlendStateFromAttributeMask<6>();
			case 7: return TGetBlendStateFromAttributeMask<7>();
			case 8: return TGetBlendStateFromAttributeMask<8>();
			case 9: return TGetBlendStateFromAttributeMask<9>();
			case 10: return TGetBlendStateFromAttributeMask<10>();
			case 11: return TGetBlendStateFromAttributeMask<11>();
			case 12: return TGetBlendStateFromAttributeMask<12>();
			case 13: return TGetBlendStateFromAttributeMask<13>();
			case 14: return TGetBlendStateFromAttributeMask<14>();
			case 15: return TGetBlendStateFromAttributeMask<15>();
			case 16: return TGetBlendStateFromAttributeMask<16>();
			case 17: return TGetBlendStateFromAttributeMask<17>();
			case 18: return TGetBlendStateFromAttributeMask<18>();
			case 19: return TGetBlendStateFromAttributeMask<19>();
			case 21: return TGetBlendStateFromAttributeMask<21>();
			case 22: return TGetBlendStateFromAttributeMask<22>();
			case 23: return TGetBlendStateFromAttributeMask<23>();
			case 24: return TGetBlendStateFromAttributeMask<24>();
			case 25: return TGetBlendStateFromAttributeMask<25>();
			case 26: return TGetBlendStateFromAttributeMask<26>();
			case 27: return TGetBlendStateFromAttributeMask<27>();
			case 28: return TGetBlendStateFromAttributeMask<28>();
			case 29: return TGetBlendStateFromAttributeMask<29>();
			case 30: return TGetBlendStateFromAttributeMask<30>();
			default: return TGetBlendStateFromAttributeMask<31>();
			}
		}
	public:
		static void ModifyCompilationEnvironment(FShaderCompilerEnvironment& OutEnvironment)
		{
			OutEnvironment.SetDefine(TEXT("OUT_BASECOLOR_NORMAL_ROUGHNESS"), 1);
		}
		static FRHIBlendState* GetBlendState(uint8 OutputAttributeMask)
		{
			return GetBlendStateImpl(OutputAttributeMask);
		}
	};
	/** Specialization for ERuntimeVirtualTextureMaterialType::WorldHeight */
	class FMaterialPolicy_WorldHeight
	{
	public:
		static void ModifyCompilationEnvironment(FShaderCompilerEnvironment& OutEnvironment)
		{
			OutEnvironment.SetDefine(TEXT("OUT_WORLDHEIGHT"), 1);
			OutEnvironment.SetRenderTargetOutputFormat(0, PF_R32_FLOAT);
		}

		static FRHIBlendState* GetBlendState(uint8 OutputAttributeMask)
		{
			return TStaticBlendState< CW_RED, BO_Max, BF_One, BF_One, BO_Add, BF_One, BF_One >::GetRHI();
		}
	};


	/** Vertex shader derivation of material shader. Templated on policy for virtual texture layout. */
	template< class MaterialPolicy >
	class FShader_VirtualTextureMaterialDraw_VS : public FShader_VirtualTextureMaterialDraw
	{
	public:
		DECLARE_SHADER_TYPE(FShader_VirtualTextureMaterialDraw_VS, MeshMaterial);

		FShader_VirtualTextureMaterialDraw_VS()
		{}

		FShader_VirtualTextureMaterialDraw_VS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FShader_VirtualTextureMaterialDraw(Initializer)
		{}

		static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FShader_VirtualTextureMaterialDraw::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			MaterialPolicy::ModifyCompilationEnvironment(OutEnvironment);
		}
	};

	/** Pixel shader derivation of material shader. Templated on policy for virtual texture layout. */
	template< class MaterialPolicy >
	class FShader_VirtualTextureMaterialDraw_PS : public FShader_VirtualTextureMaterialDraw
	{
	public:
		DECLARE_SHADER_TYPE(FShader_VirtualTextureMaterialDraw_PS< MaterialPolicy >, MeshMaterial);

		FShader_VirtualTextureMaterialDraw_PS()
		{}

		FShader_VirtualTextureMaterialDraw_PS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FShader_VirtualTextureMaterialDraw(Initializer)
		{}

		static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FShader_VirtualTextureMaterialDraw::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			MaterialPolicy::ModifyCompilationEnvironment(OutEnvironment);
		}
	};

	// If we change this macro or add additional policy types then we need to update GetRuntimeVirtualTextureShaderTypes() in LandscapeRender.cpp
	// That code is used to filter out unnecessary shader variations
#define IMPLEMENT_VIRTUALTEXTURE_SHADER_TYPE(PolicyType, PolicyName) \
	typedef FShader_VirtualTextureMaterialDraw_VS<PolicyType> TVirtualTextureVS##PolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVirtualTextureVS##PolicyName, TEXT("/Engine/Private/VirtualTextureMaterial.usf"), TEXT("MainVS"), SF_Vertex); \
	typedef FShader_VirtualTextureMaterialDraw_PS<PolicyType> TVirtualTexturePS##PolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TVirtualTexturePS##PolicyName, TEXT("/Engine/Private/VirtualTextureMaterial.usf"), TEXT("MainPS"), SF_Pixel);

	IMPLEMENT_VIRTUALTEXTURE_SHADER_TYPE(FMaterialPolicy_BaseColor, BaseColor);
	IMPLEMENT_VIRTUALTEXTURE_SHADER_TYPE(FMaterialPolicy_BaseColorNormalSpecular, BaseColorNormalSpecular);
	IMPLEMENT_VIRTUALTEXTURE_SHADER_TYPE(FMaterialPolicy_WorldHeight, WorldHeight);
	IMPLEMENT_VIRTUALTEXTURE_SHADER_TYPE(FMaterialPolicy_BaseColorNormalRoughness, BaseColorNormalRoughness);

	/** Structure to localize the setup of our render graph based on the virtual texture setup. */
	struct FRenderGraphSetup
	{
		static void SetupRenderTargetsInfo(ERuntimeVirtualTextureMaterialType MaterialType, ERHIFeatureLevel::Type FeatureLevel, bool bLQFormat, FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo)
		{
			const ETextureCreateFlags VT_SRGB = FeatureLevel > ERHIFeatureLevel::ES3_1 ? TexCreate_SRGB : TexCreate_None;
			const EPixelFormat Compressed64BitFormat = PF_R16G16B16A16_UINT;
			const EPixelFormat Compressed128BitFormat = PF_R32G32B32A32_UINT;

			switch (MaterialType)
			{
			case ERuntimeVirtualTextureMaterialType::BaseColor:
				AddRenderTargetInfo(PF_B8G8R8A8, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness:
				AddRenderTargetInfo(bLQFormat ? PF_R5G6B5_UNORM : PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				AddRenderTargetInfo(bLQFormat ? PF_R5G6B5_UNORM : PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
				AddRenderTargetInfo(PF_B8G8R8A8, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				AddRenderTargetInfo(PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				AddRenderTargetInfo(PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
				AddRenderTargetInfo(PF_B8G8R8A8, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				AddRenderTargetInfo(PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				AddRenderTargetInfo(PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg:
				AddRenderTargetInfo(PF_B8G8R8A8, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				AddRenderTargetInfo(PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				AddRenderTargetInfo(PF_B8G8R8A8, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				break;
			case ERuntimeVirtualTextureMaterialType::WorldHeight:
				AddRenderTargetInfo(PF_G16, TexCreate_RenderTargetable | TexCreate_ShaderResource, RenderTargetsInfo);
				break;
			}
		}

		FRenderGraphSetup(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, ERuntimeVirtualTextureMaterialType MaterialType, FRHITexture2D* OutputTexture0, FIntPoint TextureSize, bool bIsThumbnails)
		{
			bRenderPass = OutputTexture0 != nullptr;
			bCopyThumbnailPass = bRenderPass && bIsThumbnails;
			EPixelFormat OutputFormat = (OutputTexture0 != nullptr ? OutputTexture0->GetFormat() : PF_Unknown);
			const bool bCompressedFormat = GPixelFormats[OutputFormat].BlockSizeX == 4 && GPixelFormats[OutputFormat].BlockSizeY == 4;
			const bool bLQFormat = OutputTexture0 != nullptr && OutputTexture0->GetFormat() == PF_R5G6B5_UNORM;
			bCompressPass = bRenderPass && !bCopyThumbnailPass && bCompressedFormat;
			bCopyPass = bRenderPass && !bCopyThumbnailPass && !bCompressPass && (MaterialType == ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular || MaterialType == ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg || MaterialType == ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg);
			// Not all mobile RHIs support sRGB texture views/aliasing, use only linear targets on mobile
			const ETextureCreateFlags VT_SRGB = FeatureLevel > ERHIFeatureLevel::ES3_1 ? TexCreate_SRGB : TexCreate_None;
			const EPixelFormat Compressed64BitFormat = PF_R16G16B16A16_UINT;
			const EPixelFormat Compressed128BitFormat = PF_R32G32B32A32_UINT;

			switch (MaterialType)
			{
			case ERuntimeVirtualTextureMaterialType::BaseColor:
				if (bRenderPass)
				{
					OutputAlias0 = RenderTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture0"));
				}
				if (bCompressPass)
				{
					OutputAlias0 = CompressTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed64BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture0"));
				}
				if (bCopyThumbnailPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
				}
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness:
				if (bRenderPass)
				{
					OutputAlias0 = RenderTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, bLQFormat ? PF_R5G6B5_UNORM : PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture0"));
					OutputAlias1 = RenderTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, bLQFormat ? PF_R5G6B5_UNORM : PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture1"));
				}
				if (bCompressPass)
				{
					OutputAlias0 = CompressTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed64BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture0"));
					OutputAlias1 = CompressTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture1"));
				}
				if (bCopyThumbnailPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, bLQFormat ? PF_R5G6B5_UNORM : PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
					OutputAlias1 = nullptr;
				}
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
				if (bRenderPass)
				{
					OutputAlias0 = RenderTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture0"));
					RenderTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::DefaultNormal8Bit, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture1"));
					RenderTexture2 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::DefaultNormal8Bit, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture2"));
				}
				if (bCompressPass)
				{
					OutputAlias0 = CompressTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture0"));
					OutputAlias1 = CompressTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture1"));
				}
				if (bCopyPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
					OutputAlias1 = CopyTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture1"));
				}
				if (bCopyThumbnailPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
				}
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
				if (bRenderPass)
				{
					OutputAlias0 = RenderTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture0"));
					RenderTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::DefaultNormal8Bit, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture1"));
					RenderTexture2 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::DefaultNormal8Bit, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture2"));
				}
				if (bCompressPass)
				{
					OutputAlias0 = CompressTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture0"));
					OutputAlias1 = CompressTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture1"));
					OutputAlias2 = CompressTexture2 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed64BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture2"));
				}
				if (bCopyPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
					OutputAlias1 = CopyTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture1"));
					OutputAlias2 = CopyTexture2 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture2"));
				}
				if (bCopyThumbnailPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
				}
				break;
			case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg:
				if (bRenderPass)
				{
					OutputAlias0 = RenderTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture0"));
					RenderTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::DefaultNormal8Bit, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture1"));
					RenderTexture2 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::DefaultNormal8Bit, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture2"));
				}
				if (bCompressPass)
				{
					OutputAlias0 = CompressTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture0"));
					OutputAlias1 = CompressTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture1"));
					OutputAlias2 = CompressTexture2 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 4, Compressed128BitFormat, FClearValueBinding::None, TexCreate_UAV), TEXT("CompressTexture2"));
				}
				if (bCopyPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
					OutputAlias1 = CopyTexture1 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture1"));
					OutputAlias2 = CopyTexture2 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture2"));
				}
				if (bCopyThumbnailPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, VT_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
				}
				break;
			case ERuntimeVirtualTextureMaterialType::WorldHeight:
				if (bRenderPass)
				{
					OutputAlias0 = RenderTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_G16, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RenderTexture0"));
				}
				if (bCopyThumbnailPass)
				{
					OutputAlias0 = CopyTexture0 = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize, PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("CopyTexture0"));
				}
				break;
			}
		}

		/** Flags to express what passes we need for this virtual texture layout. */
		bool bRenderPass = false;
		bool bCompressPass = false;
		bool bCopyPass = false;
		bool bCopyThumbnailPass = false;

		/** Render graph textures needed for this virtual texture layout. */
		FRDGTextureRef RenderTexture0 = nullptr;
		FRDGTextureRef RenderTexture1 = nullptr;
		FRDGTextureRef RenderTexture2 = nullptr;
		FRDGTextureRef CompressTexture0 = nullptr;
		FRDGTextureRef CompressTexture1 = nullptr;
		FRDGTextureRef CompressTexture2 = nullptr;
		FRDGTextureRef CopyTexture0 = nullptr;
		FRDGTextureRef CopyTexture1 = nullptr;
		FRDGTextureRef CopyTexture2 = nullptr;

		/** Aliases to one of the render/compress/copy textures. This is what we will Copy into the final physical texture. */
		//todo[vt]: On platforms that support direct aliasing we can not set these and compress direct to the final destination
		FRDGTextureRef OutputAlias0 = nullptr;
		FRDGTextureRef OutputAlias1 = nullptr;
		FRDGTextureRef OutputAlias2 = nullptr;
	};


	/** Mesh processor for rendering static meshes to the virtual texture */
	class FRuntimeVirtualTextureMeshProcessor : public FSceneRenderingAllocatorObject<FRuntimeVirtualTextureMeshProcessor>, public FMeshPassProcessor
	{
	public:
		FRuntimeVirtualTextureMeshProcessor(const FScene* InScene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InView, FMeshPassDrawListContext* InDrawListContext)
			: FMeshPassProcessor(EMeshPass::VirtualTexture, InScene, InFeatureLevel, InView, InDrawListContext)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		}

	private:
		bool TryAddMeshBatch(
			const FMeshBatch& RESTRICT MeshBatch,
			uint64 BatchElementMask,
			const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
			int32 StaticMeshId,
			const FMaterialRenderProxy* MaterialRenderProxy,
			const FMaterial* Material)
		{
			const uint8 OutputAttributeMask = Material->IsDefaultMaterial() ? 0xff : Material->GetRuntimeVirtualTextureOutputAttibuteMask_RenderThread();

			if (OutputAttributeMask != 0)
			{
				switch ((ERuntimeVirtualTextureMaterialType)MeshBatch.RuntimeVirtualTextureMaterialType)
				{
				case ERuntimeVirtualTextureMaterialType::BaseColor:
					return Process<FMaterialPolicy_BaseColor>(MeshBatch, BatchElementMask, StaticMeshId, OutputAttributeMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material);
					break;
				case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness:
					return Process<FMaterialPolicy_BaseColorNormalRoughness>(MeshBatch, BatchElementMask, StaticMeshId, OutputAttributeMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material);
					break;
				case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
				case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
				case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg:
					return Process<FMaterialPolicy_BaseColorNormalSpecular>(MeshBatch, BatchElementMask, StaticMeshId, OutputAttributeMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material);
					break;
				case ERuntimeVirtualTextureMaterialType::WorldHeight:
					return Process<FMaterialPolicy_WorldHeight>(MeshBatch, BatchElementMask, StaticMeshId, OutputAttributeMask, PrimitiveSceneProxy, *MaterialRenderProxy, *Material);
					break;
				default:
					break;
				}
			}

			return true;
		}

		template<class MaterialPolicy>
		bool Process(
			const FMeshBatch& MeshBatch,
			uint64 BatchElementMask,
			int32 StaticMeshId,
			uint8 OutputAttributeMask,
			const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
			const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
			const FMaterial& RESTRICT MaterialResource)
		{
			const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

			TMeshProcessorShaders<
				FShader_VirtualTextureMaterialDraw_VS< MaterialPolicy >,
				FShader_VirtualTextureMaterialDraw_PS< MaterialPolicy > > VirtualTexturePassShaders;

			FMaterialShaderTypes ShaderTypes;
			ShaderTypes.AddShaderType<FShader_VirtualTextureMaterialDraw_VS< MaterialPolicy>>();
			ShaderTypes.AddShaderType<FShader_VirtualTextureMaterialDraw_PS< MaterialPolicy>>();

			FMaterialShaders Shaders;
			if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
			{
				return false;
			}

			Shaders.TryGetVertexShader(VirtualTexturePassShaders.VertexShader);
			Shaders.TryGetPixelShader(VirtualTexturePassShaders.PixelShader);

			DrawRenderState.SetBlendState(MaterialPolicy::GetBlendState(OutputAttributeMask));

			const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
			ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MaterialResource, OverrideSettings);
			ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(MaterialResource, OverrideSettings);

			FMeshMaterialShaderElementData ShaderElementData;
			ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

			FMeshDrawCommandSortKey SortKey;
			SortKey.Translucent.MeshIdInPrimitive = MeshBatch.MeshIdInPrimitive;
			SortKey.Translucent.Distance = 0;
			SortKey.Translucent.Priority = (uint16)((int32)PrimitiveSceneProxy->GetTranslucencySortPriority() - (int32)SHRT_MIN);

			BuildMeshDrawCommands(
				MeshBatch,
				BatchElementMask,
				PrimitiveSceneProxy,
				MaterialRenderProxy,
				MaterialResource,
				DrawRenderState,
				VirtualTexturePassShaders,
				MeshFillMode,
				MeshCullMode,
				SortKey,
				EMeshPassFeatures::Default,
				ShaderElementData);

			return true;
		}

		template<class MaterialPolicy>
		void CollectPSOInitializers(
			const FVertexFactoryType* VertexFactoryType,
			const FMaterial& RESTRICT MaterialResource,
			const ERasterizerFillMode& MeshFillMode,
			const ERasterizerCullMode& MeshCullMode,
			uint8 OutputAttributeMask,
			ERuntimeVirtualTextureMaterialType MaterialType,
			TArray<FPSOPrecacheData>& PSOInitializers)
		{			
			FMaterialShaderTypes ShaderTypes;
			ShaderTypes.AddShaderType<FShader_VirtualTextureMaterialDraw_VS< MaterialPolicy>>();
			ShaderTypes.AddShaderType<FShader_VirtualTextureMaterialDraw_PS< MaterialPolicy>>();
			FMaterialShaders Shaders;
			if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
			{
				return;
			}

			TMeshProcessorShaders<
				FShader_VirtualTextureMaterialDraw_VS< MaterialPolicy >,
				FShader_VirtualTextureMaterialDraw_PS< MaterialPolicy > > VirtualTexturePassShaders;
			Shaders.TryGetVertexShader(VirtualTexturePassShaders.VertexShader);
			Shaders.TryGetPixelShader(VirtualTexturePassShaders.PixelShader);

			FMeshPassProcessorRenderState PSODrawRenderState(DrawRenderState);
			PSODrawRenderState.SetBlendState(MaterialPolicy::GetBlendState(OutputAttributeMask));

			const bool bLQQuality = false;
			FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
			RenderTargetsInfo.NumSamples = 1;
			FRenderGraphSetup::SetupRenderTargetsInfo(MaterialType, FeatureLevel, bLQQuality, RenderTargetsInfo);
			AddGraphicsPipelineStateInitializer(
				VertexFactoryType,
				MaterialResource,
				PSODrawRenderState,
				RenderTargetsInfo,
				VirtualTexturePassShaders,
				MeshFillMode,
				MeshCullMode,
				PT_TriangleList,
				EMeshPassFeatures::Default,
				PSOInitializers);
		}

	public:
		virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
		{
			if (MeshBatch.bRenderToVirtualTexture)
			{
				const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
				while (MaterialRenderProxy)
				{
					const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
					if (Material && Material->GetRenderingThreadShaderMap())
					{
						if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, MaterialRenderProxy, Material))
						{
							break;
						}
					}

					MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
				}
			}
		}

		virtual void CollectPSOInitializers(
			const FSceneTexturesConfig& SceneTexturesConfig, 
			const FMaterial& Material, 
			const FVertexFactoryType* VertexFactoryType, 
			const FPSOPrecacheParams& PreCacheParams, 
			TArray<FPSOPrecacheData>& PSOInitializers) override final
		{
			const uint8 OutputAttributeMask = Material.IsDefaultMaterial() ? 0xff : Material.GetRuntimeVirtualTextureOutputAttibuteMask_GameThread();

			if (OutputAttributeMask != 0)
			{
				const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
				const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
				const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

				// TODO: collect from component data & store in PreCacheParams to reduce PSO precache count
				CollectPSOInitializers<FMaterialPolicy_BaseColor>(VertexFactoryType, Material, MeshFillMode, MeshCullMode, OutputAttributeMask, ERuntimeVirtualTextureMaterialType::BaseColor, PSOInitializers);
				CollectPSOInitializers<FMaterialPolicy_BaseColorNormalRoughness>(VertexFactoryType, Material, MeshFillMode, MeshCullMode, OutputAttributeMask, ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness, PSOInitializers);
				CollectPSOInitializers<FMaterialPolicy_BaseColorNormalSpecular>(VertexFactoryType, Material, MeshFillMode, MeshCullMode, OutputAttributeMask, ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular, PSOInitializers);
				CollectPSOInitializers<FMaterialPolicy_WorldHeight>(VertexFactoryType, Material, MeshFillMode, MeshCullMode, OutputAttributeMask, ERuntimeVirtualTextureMaterialType::WorldHeight, PSOInitializers);
			}
		}

	private:
		FMeshPassProcessorRenderState DrawRenderState;
	};


	/** Registration for virtual texture command caching pass */
	FMeshPassProcessor* CreateRuntimeVirtualTexturePassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
	{
		return new FRuntimeVirtualTextureMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext);
	}

	REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(VirtualTexturePass, CreateRuntimeVirtualTexturePassProcessor, EShadingPath::Deferred, EMeshPass::VirtualTexture, EMeshPassFlags::CachedMeshCommands);
	FRegisterPassProcessorCreateFunction RegisterVirtualTexturePassMobile(&CreateRuntimeVirtualTexturePassProcessor, EShadingPath::Mobile, EMeshPass::VirtualTexture, EMeshPassFlags::CachedMeshCommands);


	/** Collect meshes to draw. */
	void GatherMeshesToDraw(FDynamicPassMeshDrawListContext* DynamicMeshPassContext, FScene const* Scene, FViewInfo* View, ERuntimeVirtualTextureMaterialType MaterialType, uint32 RuntimeVirtualTextureMask, uint8 vLevel, uint8 MaxLevel)
	{
		// Cached draw command collectors
		const FCachedPassMeshDrawList& SceneDrawList = Scene->CachedDrawLists[EMeshPass::VirtualTexture];

		// Uncached mesh processor
		FRuntimeVirtualTextureMeshProcessor MeshProcessor(Scene, Scene->GetFeatureLevel(), View, DynamicMeshPassContext);

		// Pre-calculate view factors used for culling
		const float RcpWorldSize = 1.f / (View->ViewMatrices.GetInvProjectionMatrix().M[0][0]);
		const float WorldToPixel = View->ViewRect.Width() * RcpWorldSize;

		// Iterate over scene and collect visible virtual texture draw commands for this view
		//todo: Consider a broad phase (quad tree etc?) here. (But only if running over PrimitiveVirtualTextureFlags shows up as a bottleneck.)
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); ++PrimitiveIndex)
		{
			const FPrimitiveVirtualTextureFlags Flags = Scene->PrimitiveVirtualTextureFlags[PrimitiveIndex];
			if (!Flags.bRenderToVirtualTexture)
			{
				continue;
			}
			if ((Flags.RuntimeVirtualTextureMask & RuntimeVirtualTextureMask) == 0)
			{
				continue;
			}

			//todo[vt]: In our case we know that frustum is an oriented box so investigate cheaper test for intersecting that
			const FSphere SphereBounds = Scene->PrimitiveBounds[PrimitiveIndex].BoxSphereBounds.GetSphere();
			if (!View->ViewFrustum.IntersectSphere(SphereBounds.Center, SphereBounds.W))
			{
				continue;
			}

			// Cull primitives according to mip level or pixel coverage
			const FPrimitiveVirtualTextureLodInfo LodInfo = Scene->PrimitiveVirtualTextureLod[PrimitiveIndex];
			if (LodInfo.CullMethod == 0)
			{
				if (MaxLevel - vLevel < LodInfo.CullValue)
				{
					continue;
				}
			}
			else
			{
				// Note that we use 2^MinPixelCoverage as that scales linearly with mip extents
				int32 PixelCoverage = FMath::FloorToInt(2.f * SphereBounds.W * WorldToPixel);
				if (PixelCoverage < (1 << LodInfo.CullValue))
				{
					continue;
				}
			}

			FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];
			FMeshDrawCommandPrimitiveIdInfo IdInfo(PrimitiveIndex, PrimitiveSceneInfo->GetInstanceSceneDataOffset());

			// Calculate Lod for current mip
			const float AreaRatio = 2.f * SphereBounds.W * RcpWorldSize;
			const int32 CurFirstLODIdx = PrimitiveSceneInfo->Proxy->GetCurrentFirstLODIdx_RenderThread();
			const int32 MinLODIdx = FMath::Max((int32)LodInfo.MinLod, CurFirstLODIdx);
			const int32 MaxLODIdx = FMath::Max((int32)LodInfo.MaxLod, CurFirstLODIdx);
			const int32 LodBias = (int32)LodInfo.LodBias - FPrimitiveVirtualTextureLodInfo::LodBiasOffset;
			const int32 LodIndex = FMath::Clamp<int32>(LodBias - FMath::FloorToInt(FMath::Log2(AreaRatio)), MinLODIdx, MaxLODIdx);

			// Process meshes
			for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); ++MeshIndex)
			{
				FStaticMeshBatchRelevance const& StaticMeshRelevance = PrimitiveSceneInfo->StaticMeshRelevances[MeshIndex];
				if (StaticMeshRelevance.bRenderToVirtualTexture && StaticMeshRelevance.LODIndex == LodIndex && StaticMeshRelevance.RuntimeVirtualTextureMaterialType == (uint32)MaterialType)
				{
					bool bCachedDraw = false;
					if (StaticMeshRelevance.bSupportsCachingMeshDrawCommands && !PrimitiveSceneInfo->NeedsUpdateStaticMeshes())
					{
						// Use cached draw command
						const int32 StaticMeshCommandInfoIndex = StaticMeshRelevance.GetStaticMeshCommandInfoIndex(EMeshPass::VirtualTexture);
						if (StaticMeshCommandInfoIndex >= 0)
						{
							FCachedMeshDrawCommandInfo& CachedMeshDrawCommand = PrimitiveSceneInfo->StaticMeshCommandInfos[StaticMeshCommandInfoIndex];

							const FMeshDrawCommand* MeshDrawCommand = CachedMeshDrawCommand.StateBucketId >= 0
								? &Scene->CachedMeshDrawCommandStateBuckets[EMeshPass::VirtualTexture].GetByElementId(CachedMeshDrawCommand.StateBucketId).Key
								: &SceneDrawList.MeshDrawCommands[CachedMeshDrawCommand.CommandIndex];

							FVisibleMeshDrawCommand NewVisibleMeshDrawCommand;
							NewVisibleMeshDrawCommand.Setup(
								MeshDrawCommand,
								IdInfo,
								CachedMeshDrawCommand.StateBucketId,
								CachedMeshDrawCommand.MeshFillMode,
								CachedMeshDrawCommand.MeshCullMode,
								CachedMeshDrawCommand.Flags,
								CachedMeshDrawCommand.SortKey);

							DynamicMeshPassContext->AddVisibleMeshDrawCommand(NewVisibleMeshDrawCommand);
							bCachedDraw = true;
						}
					}

					if (!bCachedDraw)
					{
						// No cached draw command was available. Process the mesh batch.
						uint64 BatchElementMask = ~0ull;
						MeshProcessor.AddMeshBatch(PrimitiveSceneInfo->StaticMeshes[MeshIndex], BatchElementMask, Scene->PrimitiveSceneProxies[PrimitiveIndex]);
					}
				}
			}
		}
	}

	/** BC Compression compute shader */
	class FShader_VirtualTextureCompress : public FGlobalShader
	{
	public:
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FIntVector4, DestRect)
			SHADER_PARAMETER_STRUCT_REF(FEtcParameters, EtcParameters)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RenderTexture0)
			SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler0)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RenderTexture1)
			SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler1)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RenderTexture2)
			SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler2)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint4>, OutCompressTexture0)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint4>, OutCompressTexture1)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint4>, OutCompressTexture2)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("ETC_PROFILE"), UseEtcProfile(Parameters.Platform) ? 1 : 0);
		}

		FShader_VirtualTextureCompress()
		{}

		FShader_VirtualTextureCompress(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			Bindings.BindForLegacyShaderParameters(this, Initializer.PermutationId, Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata());
		}
	};

	template< ERuntimeVirtualTextureMaterialType MaterialType >
	class FShader_VirtualTextureCompress_CS : public FShader_VirtualTextureCompress
	{
	public:
		typedef FShader_VirtualTextureCompress_CS< MaterialType > ClassName; // typedef is only so that we can use in DECLARE_SHADER_TYPE macro
		DECLARE_SHADER_TYPE( ClassName, Global );

		FShader_VirtualTextureCompress_CS()
		{}

		FShader_VirtualTextureCompress_CS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FShader_VirtualTextureCompress(Initializer)
		{}
	};

	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCompress_CS< ERuntimeVirtualTextureMaterialType::BaseColor >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CompressBaseColorCS"), SF_Compute);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCompress_CS< ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CompressBaseColorNormalSpecularCS"), SF_Compute);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCompress_CS< ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CompressBaseColorNormalRoughnessCS"), SF_Compute);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCompress_CS< ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CompressBaseColorNormalSpecularYCoCgCS"), SF_Compute);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCompress_CS< ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CompressBaseColorNormalSpecularMaskYCoCgCS"), SF_Compute);


	/** Add the BC compression pass to the graph. */
	template< ERuntimeVirtualTextureMaterialType MaterialType >
	void AddCompressPass(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FShader_VirtualTextureCompress::FParameters* Parameters, FIntVector GroupCount)
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef< FShader_VirtualTextureCompress_CS< MaterialType > > ComputeShader(GlobalShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("VirtualTextureCompress"),
			ComputeShader, Parameters, GroupCount);
	}

	/** Set up the BC compression pass for the given MaterialType. */
	void AddCompressPass(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FShader_VirtualTextureCompress::FParameters* Parameters, FIntPoint TextureSize, ERuntimeVirtualTextureMaterialType MaterialType)
	{
		const FIntVector GroupCount(((TextureSize.X / 4) + 7) / 8, ((TextureSize.Y / 4) + 7) / 8, 1);

		// Dispatch using the shader variation for our MaterialType
		switch (MaterialType)
		{
		case ERuntimeVirtualTextureMaterialType::BaseColor:
			AddCompressPass<ERuntimeVirtualTextureMaterialType::BaseColor>(GraphBuilder, FeatureLevel, Parameters, GroupCount);
			break;
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
			AddCompressPass<ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular>(GraphBuilder, FeatureLevel, Parameters, GroupCount);
			break;
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness:
			AddCompressPass<ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Roughness>(GraphBuilder, FeatureLevel, Parameters, GroupCount);
			break;
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
			AddCompressPass<ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg>(GraphBuilder, FeatureLevel, Parameters, GroupCount);
			break;
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg:
			AddCompressPass<ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg>(GraphBuilder, FeatureLevel, Parameters, GroupCount);
			break;
		}
	}


	/** Copy shaders are used when compression is disabled. These are used to ensure that the channel layout is the same as with compression. */
	class FShader_VirtualTextureCopy : public FGlobalShader
	{
	public:
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER(FIntVector4, DestRect)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RenderTexture0)
			SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler0)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RenderTexture1)
			SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler1)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, RenderTexture2)
			SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler2)
		END_SHADER_PARAMETER_STRUCT()

		FShader_VirtualTextureCopy()
		{}

		FShader_VirtualTextureCopy(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{
			Bindings.BindForLegacyShaderParameters(this, Initializer.PermutationId, Initializer.ParameterMap, *FParameters::FTypeInfo::GetStructMetadata());
		}
	};

	class FShader_VirtualTextureCopy_VS : public FShader_VirtualTextureCopy
	{
	public:
		DECLARE_SHADER_TYPE(FShader_VirtualTextureCopy_VS, Global);

		FShader_VirtualTextureCopy_VS()
		{}

		FShader_VirtualTextureCopy_VS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FShader_VirtualTextureCopy(Initializer)
		{}
	};

	IMPLEMENT_SHADER_TYPE(, FShader_VirtualTextureCopy_VS, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CopyVS"), SF_Vertex);

	template< ERuntimeVirtualTextureMaterialType MaterialType >
	class FShader_VirtualTextureCopy_PS : public FShader_VirtualTextureCopy
	{
	public:
		typedef FShader_VirtualTextureCopy_PS< MaterialType > ClassName; // typedef is only so that we can use in DECLARE_SHADER_TYPE macro
		DECLARE_SHADER_TYPE(ClassName, Global);

		FShader_VirtualTextureCopy_PS()
		{}

		FShader_VirtualTextureCopy_PS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FShader_VirtualTextureCopy(Initializer)
		{}
	};

	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCopy_PS< ERuntimeVirtualTextureMaterialType::BaseColor >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CopyBaseColorPS"), SF_Pixel);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCopy_PS< ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CopyBaseColorNormalSpecularPS"), SF_Pixel);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCopy_PS< ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CopyBaseColorNormalSpecularYCoCgPS"), SF_Pixel);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCopy_PS< ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CopyBaseColorNormalSpecularMaskYCoCgPS"), SF_Pixel);
	IMPLEMENT_SHADER_TYPE(template<>, FShader_VirtualTextureCopy_PS< ERuntimeVirtualTextureMaterialType::WorldHeight >, TEXT("/Engine/Private/VirtualTextureCompress.usf"), TEXT("CopyWorldHeightPS"), SF_Pixel);


	/** Add the copy pass to the graph. */
	template< ERuntimeVirtualTextureMaterialType MaterialType >
	void AddCopyPass(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FShader_VirtualTextureCopy::FParameters* Parameters, FIntPoint TextureSize)
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef< FShader_VirtualTextureCopy_VS > VertexShader(GlobalShaderMap);
		TShaderMapRef< FShader_VirtualTextureCopy_PS< MaterialType > > PixelShader(GlobalShaderMap);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("VirtualTextureCopy"),
			Parameters,
			ERDGPassFlags::Raster,
			[VertexShader, PixelShader, Parameters, TextureSize](FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *Parameters);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Parameters);

			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, TextureSize[0], TextureSize[1], 1.0f);
			RHICmdList.DrawIndexedPrimitive(GTwoTrianglesIndexBuffer.IndexBufferRHI, 0, 0, 4, 0, 2, 1);
		});
	}

	/** Set up the copy pass for the given MaterialType. */
	void AddCopyPass(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FShader_VirtualTextureCopy::FParameters* Parameters, FIntPoint TextureSize, ERuntimeVirtualTextureMaterialType MaterialType)
	{
		switch (MaterialType)
		{
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
			AddCopyPass<ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular>(GraphBuilder, FeatureLevel, Parameters, TextureSize);
			break;
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
			AddCopyPass<ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg>(GraphBuilder, FeatureLevel, Parameters, TextureSize);
			break;
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg:
			AddCopyPass<ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg>(GraphBuilder, FeatureLevel, Parameters, TextureSize);
			break;
		}
	}

	/** Set up the copy pass for the given MaterialType. */
	void AddCopyThumbnailPass(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FShader_VirtualTextureCopy::FParameters* Parameters, FIntPoint TextureSize, ERuntimeVirtualTextureMaterialType MaterialType)
	{
		switch (MaterialType)
		{
		case ERuntimeVirtualTextureMaterialType::BaseColor:
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular:
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_YCoCg:
		case ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg:
			AddCopyPass<ERuntimeVirtualTextureMaterialType::BaseColor>(GraphBuilder, FeatureLevel, Parameters, TextureSize);
			break;
		case ERuntimeVirtualTextureMaterialType::WorldHeight:
			AddCopyPass<ERuntimeVirtualTextureMaterialType::WorldHeight>(GraphBuilder, FeatureLevel, Parameters, TextureSize);
			break;
		}
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FCopyToOutputParameters, )
		RDG_TEXTURE_ACCESS(Input, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()

	/** Set up the copy to final output physical texture. */
	void AddCopyToOutputPass(FRDGBuilder& GraphBuilder, FRDGTextureRef InputTexture, FRHITexture2D* OutputTexture, FBox2D const& DestBox)
	{
		FRHICopyTextureInfo CopyInfo;
		CopyInfo.Size = InputTexture->Desc.GetSize();
		CopyInfo.DestPosition = FIntVector(DestBox.Min.X, DestBox.Min.Y, 0);

		FCopyToOutputParameters* Parameters = GraphBuilder.AllocParameters<FCopyToOutputParameters>();
		Parameters->Input = InputTexture;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("VirtualTextureCopyToOutput"),
			Parameters,
			ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
			[InputTexture, OutputTexture, CopyInfo](FRHICommandList& RHICmdList)
			{
				RHICmdList.Transition(FRHITransitionInfo(OutputTexture, ERHIAccess::SRVMask, ERHIAccess::CopyDest));
				RHICmdList.CopyTexture(InputTexture->GetRHI(), OutputTexture, CopyInfo);
				RHICmdList.Transition(FRHITransitionInfo(OutputTexture, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
			});
	}

	bool IsSceneReadyToRender(FSceneInterface* Scene)
	{
		return Scene != nullptr && Scene->GetRenderScene() != nullptr && Scene->GetRenderScene()->GPUScene.IsRendering();
	}

	void RenderPage(
		FRDGBuilder& GraphBuilder,
		FScene* Scene,
		uint32 RuntimeVirtualTextureMask,
		ERuntimeVirtualTextureMaterialType MaterialType,
		bool bClearTextures,
		bool bIsThumbnails,
		FRHITexture2D* OutputTexture0,
		IPooledRenderTarget* OutputTarget0,
		FBox2D const& DestBox0,
		FRHITexture2D* OutputTexture1,
		IPooledRenderTarget* OutputTarget1,
		FBox2D const& DestBox1,
		FRHITexture2D* OutputTexture2, 
		IPooledRenderTarget* OutputTarget2,
		FBox2D const& DestBox2,
		FTransform const& UVToWorld,
		FBox const& WorldBounds,
		FBox2D const& UVRange,
		uint8 vLevel,
		uint8 MaxLevel,
		ERuntimeVirtualTextureDebugType DebugType)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "VirtualTextureDynamicCache");

		// Initialize a temporary view required for the material render pass
		//todo[vt]: Some of this, such as ViewRotationMatrix, can be computed once in the Finalizer and passed down.
		//todo[vt]: Have specific shader variations and setup for different output texture configs
		FSceneViewFamily::ConstructionValues ViewFamilyInit(nullptr, Scene, FEngineShowFlags(ESFIM_Game));
		ViewFamilyInit.SetTime(FGameTime());
		FSceneViewFamily& ViewFamily = *GraphBuilder.AllocObject<FViewFamilyInfo>(ViewFamilyInit);

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;

		const FIntPoint TextureSize = (DestBox0.Max - DestBox0.Min).IntPoint();
		ViewInitOptions.SetViewRectangle(FIntRect(FIntPoint(0, 0), TextureSize));

		const FVector UVCenter = FVector(UVRange.GetCenter(), 0.f);
		const FVector CameraLookAt = UVToWorld.TransformPosition(UVCenter);
		const float BoundBoxZ = UVToWorld.GetScale3D().Z;
		const FVector CameraPos = CameraLookAt + BoundBoxZ * UVToWorld.GetUnitAxis(EAxis::Z);
		ViewInitOptions.ViewOrigin = CameraPos;

		const float OrthoWidth = UVToWorld.GetScaledAxis(EAxis::X).Size() * UVRange.GetExtent().X;
		const float OrthoHeight = UVToWorld.GetScaledAxis(EAxis::Y).Size() * UVRange.GetExtent().Y;

		const FTransform WorldToUVRotate(UVToWorld.GetRotation().Inverse());
		ViewInitOptions.ViewRotationMatrix = WorldToUVRotate.ToMatrixNoScale() * FMatrix(
			FPlane(1, 0, 0, 0),
			FPlane(0, -1, 0, 0),
			FPlane(0, 0, -1, 0),
			FPlane(0, 0, 0, 1));

		const float NearPlane = 0;
		const float FarPlane = BoundBoxZ;
		const float ZScale = 1.0f / (FarPlane - NearPlane);
		const float ZOffset = -NearPlane;
		ViewInitOptions.ProjectionMatrix = FReversedZOrthoMatrix(OrthoWidth, OrthoHeight, ZScale, ZOffset);

		// Ortho views need to set this to have a ViewOrigin that works with large world coordinates.
		ViewInitOptions.bUseFauxOrthoViewPos = true;

		const FVector4f MipLevelParameter = FVector4f((float)vLevel, (float)MaxLevel, OrthoWidth / (float)TextureSize.X, OrthoHeight / (float)TextureSize.Y);
		
		const float HeightRange = FMath::Max<float>(WorldBounds.Max.Z - WorldBounds.Min.Z, 1.f);
		const FVector2D WorldHeightPackParameter = FVector2D(1.f / HeightRange, -WorldBounds.Min.Z / HeightRange);

		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::White;

		FViewInfo* View = GraphBuilder.AllocObject<FViewInfo>(ViewInitOptions);
		ViewFamily.Views.Add(View);

		View->bIsVirtualTexture = true;
		View->ViewRect = View->UnconstrainedViewRect;
		View->CachedViewUniformShaderParameters = MakeUnique<FViewUniformShaderParameters>();
		View->SetupUniformBufferParameters(nullptr, 0, *View->CachedViewUniformShaderParameters);
		View->CachedViewUniformShaderParameters->RuntimeVirtualTextureMipLevel = MipLevelParameter;
		View->CachedViewUniformShaderParameters->RuntimeVirtualTexturePackHeight = FVector2f(WorldHeightPackParameter);	// LWC_TODO: Precision loss
		View->CachedViewUniformShaderParameters->RuntimeVirtualTextureDebugParams = FVector4f(DebugType == ERuntimeVirtualTextureDebugType::Debug ? 1.f : 0.f, 0.f, 0.f, 0.f);
		View->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(*View->CachedViewUniformShaderParameters, UniformBuffer_SingleFrame);

		FRDGExternalAccessQueue ExternalAccessQueue;
		Scene->GPUScene.UploadDynamicPrimitiveShaderDataForView(GraphBuilder, *Scene, *View, ExternalAccessQueue);
		ExternalAccessQueue.Submit(GraphBuilder);

		// Build graph
		FRenderGraphSetup GraphSetup(GraphBuilder, Scene->GetFeatureLevel(), MaterialType, OutputTexture0, TextureSize, bIsThumbnails);

		// Draw Pass
		if (GraphSetup.bRenderPass)
		{
			ERenderTargetLoadAction LoadAction = bClearTextures ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ENoAction;
			FShader_VirtualTextureMaterialDraw::FParameters* PassParameters = GraphBuilder.AllocParameters<FShader_VirtualTextureMaterialDraw::FParameters>();
			PassParameters->View = View->ViewUniformBuffer;
			PassParameters->RenderTargets[0] = GraphSetup.RenderTexture0 ? FRenderTargetBinding(GraphSetup.RenderTexture0, LoadAction) : FRenderTargetBinding();
			PassParameters->RenderTargets[1] = GraphSetup.RenderTexture1 ? FRenderTargetBinding(GraphSetup.RenderTexture1, LoadAction) : FRenderTargetBinding();
			PassParameters->RenderTargets[2] = GraphSetup.RenderTexture2 ? FRenderTargetBinding(GraphSetup.RenderTexture2, LoadAction) : FRenderTargetBinding();
    		
			AddSimpleMeshPass(GraphBuilder, PassParameters, Scene, *View, nullptr, RDG_EVENT_NAME("VirtualTextureDraw"), View->ViewRect,
	    	[Scene, View, MaterialType, RuntimeVirtualTextureMask, vLevel, MaxLevel](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
	    	{
	    		GatherMeshesToDraw(DynamicMeshPassContext, Scene, View, MaterialType, RuntimeVirtualTextureMask, vLevel, MaxLevel);
	    	});
		}

		// Compression Pass
		if (GraphSetup.bCompressPass)
		{
			FShader_VirtualTextureCompress::FParameters* PassParameters = GraphBuilder.AllocParameters<FShader_VirtualTextureCompress::FParameters>();
			PassParameters->DestRect = FIntVector4(0, 0, TextureSize.X, TextureSize.Y);
			PassParameters->EtcParameters = GetEtcParametersUniformBufferRef();
			PassParameters->RenderTexture0 = GraphSetup.RenderTexture0;
			PassParameters->TextureSampler0 = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->RenderTexture1 = GraphSetup.RenderTexture1;
			PassParameters->TextureSampler1 = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->RenderTexture2 = GraphSetup.RenderTexture2;
			PassParameters->TextureSampler2 = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->OutCompressTexture0 = GraphSetup.CompressTexture0 ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(GraphSetup.CompressTexture0)) : nullptr;
			PassParameters->OutCompressTexture1 = GraphSetup.CompressTexture1 ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(GraphSetup.CompressTexture1)) : nullptr;
			PassParameters->OutCompressTexture2 = GraphSetup.CompressTexture2 ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc(GraphSetup.CompressTexture2)) : nullptr;

			AddCompressPass(GraphBuilder, Scene->GetFeatureLevel(), PassParameters, TextureSize, MaterialType);
		}
		
		// Copy Pass
		if (GraphSetup.bCopyPass || GraphSetup.bCopyThumbnailPass)
		{
			FShader_VirtualTextureCopy::FParameters* PassParameters = GraphBuilder.AllocParameters<FShader_VirtualTextureCopy::FParameters>();
			PassParameters->RenderTargets[0] = GraphSetup.CopyTexture0 ? FRenderTargetBinding(GraphSetup.CopyTexture0, ERenderTargetLoadAction::ENoAction) : FRenderTargetBinding();
			PassParameters->RenderTargets[1] = GraphSetup.CopyTexture1 ? FRenderTargetBinding(GraphSetup.CopyTexture1, ERenderTargetLoadAction::ENoAction) : FRenderTargetBinding();
			PassParameters->RenderTargets[2] = GraphSetup.CopyTexture2 ? FRenderTargetBinding(GraphSetup.CopyTexture2, ERenderTargetLoadAction::ENoAction) : FRenderTargetBinding();
			PassParameters->DestRect = FIntVector4(0, 0, TextureSize.X, TextureSize.Y);
			PassParameters->RenderTexture0 = GraphSetup.RenderTexture0;
			PassParameters->TextureSampler0 = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->RenderTexture1 = GraphSetup.RenderTexture1;
			PassParameters->TextureSampler1 = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->RenderTexture2 = GraphSetup.RenderTexture2;
			PassParameters->TextureSampler2 = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			if (GraphSetup.bCopyPass)
			{
				AddCopyPass(GraphBuilder, Scene->GetFeatureLevel(), PassParameters, TextureSize, MaterialType);
			}
			else
			{
				AddCopyThumbnailPass(GraphBuilder, Scene->GetFeatureLevel(), PassParameters, TextureSize, MaterialType);
			}
		}

		// Copy to Output for each output texture
		if (GraphSetup.OutputAlias0 != nullptr && OutputTexture0 != nullptr)
		{
			AddCopyToOutputPass(GraphBuilder, GraphSetup.OutputAlias0, OutputTexture0, DestBox0);
		}
		if (GraphSetup.OutputAlias1 != nullptr && OutputTexture1 != nullptr)
		{
			AddCopyToOutputPass(GraphBuilder, GraphSetup.OutputAlias1, OutputTexture1, DestBox1);
		}
		if (GraphSetup.OutputAlias2 != nullptr && OutputTexture2 != nullptr)
		{
			AddCopyToOutputPass(GraphBuilder, GraphSetup.OutputAlias2, OutputTexture2, DestBox2);
		}
	}

	void RenderPagesInternal(FRDGBuilder& GraphBuilder, FRenderPageBatchDesc const& InDesc)
	{
		check(InDesc.NumPageDescs <= EMaxRenderPageBatch);

		if (InDesc.NumPageDescs > 0)
		{
			RenderCaptureInterface::FScopedCapture RenderCapture((RenderCaptureNextRVTPagesDraws != 0), GraphBuilder, TEXT("RenderRVTPages"));
			RenderCaptureNextRVTPagesDraws = FMath::Max(RenderCaptureNextRVTPagesDraws - 1, 0);

			for (int32 PageIndex = 0; PageIndex < InDesc.NumPageDescs; ++PageIndex)
			{
				FRenderPageDesc const& PageDesc = InDesc.PageDescs[PageIndex];

				RenderPage(
					GraphBuilder,
					InDesc.Scene,
					InDesc.RuntimeVirtualTextureMask,
					InDesc.MaterialType,
					InDesc.bClearTextures,
					InDesc.bIsThumbnails,
					InDesc.Targets[0].Texture, InDesc.Targets[0].PooledRenderTarget, PageDesc.DestBox[0],
					InDesc.Targets[1].Texture, InDesc.Targets[1].PooledRenderTarget, PageDesc.DestBox[1],
					InDesc.Targets[2].Texture, InDesc.Targets[2].PooledRenderTarget, PageDesc.DestBox[2],
					InDesc.UVToWorld,
					InDesc.WorldBounds,
					PageDesc.UVRange,
					PageDesc.vLevel,
					InDesc.MaxLevel,
					InDesc.DebugType);
			}
		}
	}

	void RenderPagesStandAlone(FRDGBuilder& GraphBuilder, FRenderPageBatchDesc const& InDesc)
	{
		// This is required to collect dynamic primitives from the views (not used here, but we must provide one).
		FGPUSceneDynamicContext GPUSceneDynamicContext(InDesc.Scene->GPUScene);

		InDesc.Scene->UpdateAllPrimitiveSceneInfos(GraphBuilder, true);

		// Call to let GPU-Scene determine if it is active and record scene primitive count
		FGPUSceneScopeBeginEndHelper GPUSceneScopeBeginEndHelper(InDesc.Scene->GPUScene, GPUSceneDynamicContext, InDesc.Scene);

		FRDGExternalAccessQueue ExternalAccessQueue;
		InDesc.Scene->GPUScene.Update(GraphBuilder, *InDesc.Scene, ExternalAccessQueue);
		ExternalAccessQueue.Submit(GraphBuilder);

		RenderPagesInternal(GraphBuilder, InDesc);
	}

	void RenderPages(FRDGBuilder& GraphBuilder, FRenderPageBatchDesc const& InDesc)
	{
		if (InDesc.Scene->GPUScene.IsRendering())
		{
			RenderPagesInternal(GraphBuilder, InDesc);
		}
		else
		{
			// We allow locked root pages to be rendered outside of their scene update.
			// We expect to hit this path very rarely. (One case is during material baking.)
			RenderPagesStandAlone(GraphBuilder, InDesc);
		}
	}

	void RenderPages(FRHICommandListImmediate& RHICmdList, FRenderPageBatchDesc const& InDesc)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		RenderPages(GraphBuilder, InDesc);
		GraphBuilder.Execute();
	}

	uint32 GetRuntimeVirtualTextureSceneIndex_GameThread(class URuntimeVirtualTextureComponent* InComponent)
	{
		int32 SceneIndex = 0;
		ENQUEUE_RENDER_COMMAND(GetSceneIndexCommand)(
			[&SceneIndex, InComponent](FRHICommandListImmediate& RHICmdList)
		{
			if (InComponent->GetScene() != nullptr)
			{
				FScene* Scene = InComponent->GetScene()->GetRenderScene();
				if (Scene != nullptr && InComponent->SceneProxy != nullptr)
				{
					SceneIndex = Scene->GetRuntimeVirtualTextureSceneIndex(InComponent->SceneProxy->ProducerId);
				}
			}
		});
		FlushRenderingCommands();
		return SceneIndex;
	}
}
