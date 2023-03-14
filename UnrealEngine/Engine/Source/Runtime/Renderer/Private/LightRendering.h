// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightRendering.h: Light rendering declarations.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "LightSceneInfo.h"

/** Uniform buffer for rendering deferred lights. */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FDeferredLightUniformStruct,)
	SHADER_PARAMETER(FVector4f,ShadowMapChannelMask)
	SHADER_PARAMETER(FVector2f,DistanceFadeMAD)
	SHADER_PARAMETER(float, ContactShadowLength)
	SHADER_PARAMETER(float, ContactShadowNonShadowCastingIntensity)
	SHADER_PARAMETER(float, VolumetricScatteringIntensity)
	SHADER_PARAMETER(uint32,ShadowedBits)
	SHADER_PARAMETER(uint32,LightingChannelMask)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLightShaderParameters, LightParameters)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern uint32 GetShadowQuality();

extern float GetLightFadeFactor(const FSceneView& View, const FLightSceneProxy* Proxy);

extern FDeferredLightUniformStruct GetDeferredLightParameters(const FSceneView& View, const FLightSceneInfo& LightSceneInfo);

template<typename ShaderRHIParamRef>
void SetDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FLightSceneInfo* LightSceneInfo,
	const FSceneView& View)
{
	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, DeferredLightUniformBufferParameter, GetDeferredLightParameters(View, *LightSceneInfo));
}

extern FDeferredLightUniformStruct GetSimpleDeferredLightParameters(
	const FSceneView& View,
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry &SimpleLightPerViewData);

template<typename ShaderRHIParamRef>
void SetSimpleDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry &SimpleLightPerViewData,
	const FSceneView& View)
{
	FDeferredLightUniformStruct DeferredLightUniformsValue = GetSimpleDeferredLightParameters(View, SimpleLight, SimpleLightPerViewData);
	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, DeferredLightUniformBufferParameter, DeferredLightUniformsValue);
}

/** Shader parameters needed to render a light function. */
class FLightFunctionSharedParameters
{
	DECLARE_TYPE_LAYOUT(FLightFunctionSharedParameters, NonVirtual);
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		LightFunctionParameters.Bind(ParameterMap,TEXT("LightFunctionParameters"));
	}

	static FVector4f GetLightFunctionSharedParameters(const FLightSceneInfo* LightSceneInfo, float ShadowFadeFraction)
	{
		const bool bIsSpotLight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
		const bool bIsPointLight = LightSceneInfo->Proxy->GetLightType() == LightType_Point;
		const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo->Proxy->GetOuterConeAngle()) : 1.0f;
		return FVector4f(TanOuterAngle, ShadowFadeFraction, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f);
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FLightSceneInfo* LightSceneInfo, float ShadowFadeFraction) const
	{
		SetShaderValue( 
			RHICmdList, 
			ShaderRHI, 
			LightFunctionParameters, 
			GetLightFunctionSharedParameters(LightSceneInfo, ShadowFadeFraction));
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FLightFunctionSharedParameters& P)
	{
		Ar << P.LightFunctionParameters;
		return Ar;
	}

private:
	
		LAYOUT_FIELD(FShaderParameter, LightFunctionParameters)
	
};

/** Utility functions for drawing a sphere */
namespace StencilingGeometry
{
	/**
	* Draws a sphere using RHIDrawIndexedPrimitive, useful as approximate bounding geometry for deferred passes.
	* Note: The sphere will be of unit size unless transformed by the shader. 
	*/
	extern void DrawSphere(FRHICommandList& RHICmdList);
	/**
	 * Draws exactly the same as above, but uses FVector rather than FVector4f vertex data.
	 */
	extern void DrawVectorSphere(FRHICommandList& RHICmdList);
	/** Renders a cone with a spherical cap, used for rendering spot lights in deferred passes. */
	extern void DrawCone(FRHICommandList& RHICmdList);

	/** 
	* Vertex buffer for a sphere of unit size. Used for drawing a sphere as approximate bounding geometry for deferred passes.
	*/
	template<int32 NumSphereSides, int32 NumSphereRings, typename VectorType>
	class TStencilSphereVertexBuffer : public FVertexBuffer
	{
	public:
		static_assert(std::is_same_v<typename VectorType::FReal, float>, "Must be a float vector type");

		int32 GetNumRings() const
		{
			return NumSphereRings;
		}

		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			const int32 NumSides = NumSphereSides;
			const int32 NumRings = NumSphereRings;
			const int32 NumVerts = (NumSides + 1) * (NumRings + 1);

			const float RadiansPerRingSegment = UE_PI / (float)NumRings;
			float Radius = 1;

			TArray<VectorType, TInlineAllocator<NumRings + 1> > ArcVerts;
			ArcVerts.Empty(NumRings + 1);
			// Calculate verts for one arc
			for (int32 i = 0; i < NumRings + 1; i++)
			{
				const float Angle = i * RadiansPerRingSegment;
				ArcVerts.Add(FVector3f(0.0f, FMath::Sin(Angle), FMath::Cos(Angle)));
			}

			TResourceArray<VectorType, VERTEXBUFFER_ALIGNMENT> Verts;
			Verts.Empty(NumVerts);
			// Then rotate this arc NumSides + 1 times.
			const FVector3f Center = FVector3f(0,0,0);
			for (int32 s = 0; s < NumSides + 1; s++)
			{
				FRotator3f ArcRotator(0, 360.f * ((float)s / NumSides), 0);
				FRotationMatrix44f ArcRot( ArcRotator );

				for (int32 v = 0; v < NumRings + 1; v++)
				{
					const int32 VIx = (NumRings + 1) * s + v;
					Verts.Add(Center + Radius * ArcRot.TransformPosition(ArcVerts[v]));
				}
			}

			NumSphereVerts = Verts.Num();
			uint32 Size = Verts.GetResourceDataSize();

			// Create vertex buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(TEXT("TStencilSphereVertexBuffer"), &Verts);
			VertexBufferRHI = RHICreateVertexBuffer(Size,BUF_Static,CreateInfo);
		}

		int32 GetVertexCount() const { return NumSphereVerts; }

	/** 
	* Calculates the world transform for a sphere.
	* @param OutTransform - The output world transform.
	* @param Sphere - The sphere to generate the transform for.
	* @param PreViewTranslation - The pre-view translation to apply to the transform.
	* @param bConservativelyBoundSphere - when true, the sphere that is drawn will contain all positions in the analytical sphere,
	*		 Otherwise the sphere vertices will lie on the analytical sphere and the positions on the faces will lie inside the sphere.
	*/
	void CalcTransform(FVector4f& OutPosAndScale, const FSphere& Sphere, const FVector& PreViewTranslation, bool bConservativelyBoundSphere = true)
	{
		float Radius = Sphere.W; // LWC_TODO: Precision loss
		if (bConservativelyBoundSphere)
		{
			const int32 NumRings = NumSphereRings;
			const float RadiansPerRingSegment = UE_PI / (float)NumRings;

			// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
			Radius /= FMath::Cos(RadiansPerRingSegment);
		}

		const FVector3f Translate(Sphere.Center + PreViewTranslation);
		OutPosAndScale = FVector4f(Translate, Radius);
	}

	private:
		int32 NumSphereVerts;
	};

	/** 
	* Stenciling sphere index buffer
	*/
	template<int32 NumSphereSides, int32 NumSphereRings>
	class TStencilSphereIndexBuffer : public FIndexBuffer
	{
	public:
		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			const int32 NumSides = NumSphereSides;
			const int32 NumRings = NumSphereRings;
			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;

			// Add triangles for all the vertices generated
			for (int32 s = 0; s < NumSides; s++)
			{
				const int32 a0start = (s + 0) * (NumRings + 1);
				const int32 a1start = (s + 1) * (NumRings + 1);

				for (int32 r = 0; r < NumRings; r++)
				{
					Indices.Add(a0start + r + 0);
					Indices.Add(a1start + r + 0);
					Indices.Add(a0start + r + 1);
					Indices.Add(a1start + r + 0);
					Indices.Add(a1start + r + 1);
					Indices.Add(a0start + r + 1);
				}
			}

			NumIndices = Indices.Num();
			const uint32 Size = Indices.GetResourceDataSize();
			const uint32 Stride = sizeof(uint16);

			// Create index buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(TEXT("TStencilSphereIndexBuffer"), &Indices);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
		}

		int32 GetIndexCount() const { return NumIndices; }; 
	
	private:
		int32 NumIndices;
	};

	class FStencilConeIndexBuffer : public FIndexBuffer
	{
	public:
		// A side is a line of vertices going from the cone's origin to the edge of its SphereRadius
		static const int32 NumSides = 18;
		// A slice is a circle of vertices in the cone's XY plane
		static const int32 NumSlices = 12;

		static const uint32 NumVerts = NumSides * NumSlices * 2;

		void InitRHI() override
		{
			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;

			Indices.Empty((NumSlices - 1) * NumSides * 12);
			// Generate triangles for the vertices of the cone shape
			for (int32 SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
			{
				for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
				{
					const int32 CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides;
					const int32 NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides;
					const int32 NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides;
					const int32 NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides;

					Indices.Add(CurrentIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceAndSideIndex);
				}
			}

			// Generate triangles for the vertices of the spherical cap
			const int32 CapIndexStart = NumSides * NumSlices;

			for (int32 SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
			{
				for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
				{
					const int32 CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides + CapIndexStart;
					const int32 NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;
					const int32 NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides + CapIndexStart;
					const int32 NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;

					Indices.Add(CurrentIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSliceAndSideIndex);
				}
			}

			const uint32 Size = Indices.GetResourceDataSize();
			const uint32 Stride = sizeof(uint16);

			NumIndices = Indices.Num();

			// Create index buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(TEXT("FStencilConeIndexBuffer"), &Indices);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static,CreateInfo);
		}

		int32 GetIndexCount() const { return NumIndices; } 

	protected:
		int32 NumIndices;
	};

	/** 
	* Vertex buffer for a cone. It holds zero'd out data since the actual math is done on the shader
	*/
	class FStencilConeVertexBuffer : public FVertexBuffer
	{
	public:
		static const int32 NumVerts = FStencilConeIndexBuffer::NumSides * FStencilConeIndexBuffer::NumSlices * 2;

		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			TResourceArray<FVector4f, VERTEXBUFFER_ALIGNMENT> Verts;
			Verts.Empty(NumVerts);
			for (int32 s = 0; s < NumVerts; s++)
			{
				Verts.Add(FVector4f(0, 0, 0, 0));
			}

			uint32 Size = Verts.GetResourceDataSize();

			// Create vertex buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(TEXT("FStencilConeVertexBuffer"), &Verts);
			VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
		}

		int32 GetVertexCount() const { return NumVerts; }
	};

	extern TGlobalResource<TStencilSphereVertexBuffer<18, 12, FVector4f> >	GStencilSphereVertexBuffer;
	extern TGlobalResource<TStencilSphereVertexBuffer<18, 12, FVector3f> >	GStencilSphereVectorBuffer;
	extern TGlobalResource<TStencilSphereIndexBuffer<18, 12> >				GStencilSphereIndexBuffer;
	extern TGlobalResource<TStencilSphereVertexBuffer<4, 4, FVector4f> >		GLowPolyStencilSphereVertexBuffer;
	extern TGlobalResource<TStencilSphereIndexBuffer<4, 4> >				GLowPolyStencilSphereIndexBuffer;
	extern TGlobalResource<FStencilConeVertexBuffer>						GStencilConeVertexBuffer;
	extern TGlobalResource<FStencilConeIndexBuffer>							GStencilConeIndexBuffer;

}; //End StencilingGeometry

/** 
* Stencil geometry parameters used by multiple shaders. 
*/
class FStencilingGeometryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FStencilingGeometryShaderParameters, NonVirtual);

public:

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, StencilingGeometryPosAndScale)
		SHADER_PARAMETER(FVector4f, StencilingConeParameters)
		SHADER_PARAMETER(FMatrix44f, StencilingConeTransform)
	END_SHADER_PARAMETER_STRUCT()

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		StencilGeometryPosAndScale.Bind(ParameterMap, TEXT("StencilingGeometryPosAndScale"));
		StencilConeParameters.Bind(ParameterMap, TEXT("StencilingConeParameters"));
		StencilConeTransform.Bind(ParameterMap, TEXT("StencilingConeTransform"));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FVector4f& InStencilingGeometryPosAndScale) const
	{
		const FParameters P = GetParameters(InStencilingGeometryPosAndScale);
		SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), StencilGeometryPosAndScale, P.StencilingGeometryPosAndScale);
		SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), StencilConeParameters, P.StencilingConeParameters);
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FSceneView& View, const FLightSceneInfo* LightSceneInfo) const
	{
		const FParameters P = GetParameters(View, LightSceneInfo);
		if (LightSceneInfo->Proxy->GetLightType() == LightType_Point ||
			LightSceneInfo->Proxy->GetLightType() == LightType_Rect)
		{
			SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), StencilGeometryPosAndScale, P.StencilingGeometryPosAndScale);
			SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), StencilConeParameters, P.StencilingConeParameters);
		}
		else if (LightSceneInfo->Proxy->GetLightType() == LightType_Spot)
		{
			SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), StencilConeTransform, P.StencilingConeTransform);
			SetShaderValue(
				RHICmdList,
				RHICmdList.GetBoundVertexShader(),
				StencilConeParameters,
				P.StencilingConeParameters);
		}
	}

	static FParameters GetParameters(const FVector4f& InStencilingGeometryPosAndScale)
	{
		FParameters Out;
		Out.StencilingGeometryPosAndScale = InStencilingGeometryPosAndScale;
		Out.StencilingConeParameters = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		Out.StencilingConeTransform = FMatrix44f::Identity;
		return Out;
	}

	static FParameters GetParameters(const FSceneView& View, const FLightSceneInfo* LightSceneInfo)
	{
		FParameters Out;
		if (LightSceneInfo->Proxy->GetLightType() == LightType_Point ||
			LightSceneInfo->Proxy->GetLightType() == LightType_Rect)
		{
			StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(Out.StencilingGeometryPosAndScale, LightSceneInfo->Proxy->GetBoundingSphere(), View.ViewMatrices.GetPreViewTranslation());
			Out.StencilingConeParameters = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
			Out.StencilingConeTransform = FMatrix44f::Identity;
		}
		else if (LightSceneInfo->Proxy->GetLightType() == LightType_Spot)
		{
			const FMatrix WorldToTranslatedWorld = FTranslationMatrix(View.ViewMatrices.GetPreViewTranslation());
			Out.StencilingGeometryPosAndScale = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
			Out.StencilingConeTransform = FMatrix44f(LightSceneInfo->Proxy->GetLightToWorld() * WorldToTranslatedWorld);
			Out.StencilingConeParameters =
				FVector4f(
					StencilingGeometry::FStencilConeIndexBuffer::NumSides,
					StencilingGeometry::FStencilConeIndexBuffer::NumSlices,
					LightSceneInfo->Proxy->GetOuterConeAngle(),
					LightSceneInfo->Proxy->GetRadius());
		}
		return Out;
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FStencilingGeometryShaderParameters& P)
	{
		Ar << P.StencilGeometryPosAndScale;
		Ar << P.StencilConeParameters;
		Ar << P.StencilConeTransform;
		return Ar;
	}

private:
	
	LAYOUT_FIELD(FShaderParameter, StencilGeometryPosAndScale)
	LAYOUT_FIELD(FShaderParameter, StencilConeParameters)
	LAYOUT_FIELD(FShaderParameter, StencilConeTransform)
};


BEGIN_SHADER_PARAMETER_STRUCT(FDrawFullScreenRectangleParameters, )
	SHADER_PARAMETER(FVector4f, PosScaleBias)
	SHADER_PARAMETER(FVector4f, UVScaleBias)
	SHADER_PARAMETER(FVector4f, InvTargetSizeAndTextureSize)
END_SHADER_PARAMETER_STRUCT()

/** A vertex shader for rendering the light in a deferred pass. */
class FDeferredLightVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDeferredLightVS,Global);
	SHADER_USE_PARAMETER_STRUCT(FDeferredLightVS, FGlobalShader);

	class FRadialLight : SHADER_PERMUTATION_BOOL("SHADER_RADIAL_LIGHT");
	using FPermutationDomain = TShaderPermutationDomain<FRadialLight>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDrawFullScreenRectangleParameters, FullScreenRect)
		SHADER_PARAMETER_STRUCT_INCLUDE(FStencilingGeometryShaderParameters::FParameters, Geometry)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FRadialLight>())
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) || IsMobileDeferredShadingEnabled(Parameters.Platform);
		}
		// used with FPrefilterPlanarReflectionPS on mobile
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static FDrawFullScreenRectangleParameters GetFullScreenRectParameters(
		float X, float Y,
		float SizeX, float SizeY,
		float U, float V,
		float SizeU, float SizeV,
		FIntPoint InTargetSize,
		FIntPoint InTextureSize);

	static FParameters GetParameters(const FViewInfo& View,
		float X, float Y,
		float SizeX, float SizeY,
		float U, float V,
		float SizeU, float SizeV,
		FIntPoint TargetSize,
		FIntPoint TextureSize,
		bool bBindViewUniform = true);

	static FParameters GetParameters(const FViewInfo& View, bool bBindViewUniform = true);

	static FParameters GetParameters(const FViewInfo& View, const FSphere& LightBounds, bool bBindViewUniform = true);

	static FParameters GetParameters(const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, bool bBindViewUniform = true);
};

enum class FLightOcclusionType : uint8
{
	Shadowmap,
	Raytraced,
};
FLightOcclusionType GetLightOcclusionType(const FLightSceneProxy& Proxy);
FLightOcclusionType GetLightOcclusionType(const FLightSceneInfoCompact& LightInfo);

