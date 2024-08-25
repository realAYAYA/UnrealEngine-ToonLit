// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConvexVolume.h"
#include "DebugViewModeHelpers.h"
#include "DynamicRenderScaling.h"
#include "EngineDefines.h"
#include "FinalPostProcessSettings.h"
#include "GameTime.h"
#include "GlobalDistanceFieldConstants.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "Math/MirrorMatrix.h"
#include "PrimitiveComponentId.h"
#include "RendererInterface.h"
#include "RenderResource.h"
#include "ShowFlags.h"
#include "StereoRendering.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GlobalDistanceFieldParameters.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "SceneInterface.h"
#include "SceneTypes.h"
#include "UniformBuffer.h"
#endif

#define MAX_PHYSICS_FIELD_TARGETS 32

class FSceneView;
class FSceneViewFamily;
class FSceneViewStateInterface;
class FViewElementDrawer;
class ISceneViewExtension;
class FSceneViewFamily;
class FVolumetricFogViewResources;
class ISceneRenderer;
class ISpatialUpscaler;
struct FMinimalViewInfo;

namespace UE::Renderer::Private
{

class ITemporalUpscaler;

}

class FRenderTarget;

// Projection data for a FSceneView
struct FSceneViewProjectionData
{
	/** The view origin. */
	FVector ViewOrigin;

	/** Rotation matrix transforming from world space to view space. */
	FMatrix ViewRotationMatrix;

	/** UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix ProjectionMatrix;

	//The unconstrained (no aspect ratio bars applied) view rectangle (also unscaled)
	FIntRect ViewRect;

	//The vector (including distance) from the camera to it's viewtarget, if set. Primarily only used for Ortho views.
	FVector CameraToViewTarget;

protected:
	// The constrained view rectangle (identical to UnconstrainedUnscaledViewRect if aspect ratio is not constrained)
	FIntRect ConstrainedViewRect;

public:
	void SetViewRectangle(const FIntRect& InViewRect)
	{
		ViewRect = InViewRect;
		ConstrainedViewRect = InViewRect;
	}

	void SetConstrainedViewRectangle(const FIntRect& InViewRect)
	{
		ConstrainedViewRect = InViewRect;
	}

	bool IsValidViewRectangle() const
	{
		return (ConstrainedViewRect.Min.X >= 0) &&
			(ConstrainedViewRect.Min.Y >= 0) &&
			(ConstrainedViewRect.Width() > 0) &&
			(ConstrainedViewRect.Height() > 0);
	}

	bool IsPerspectiveProjection() const
	{
		return ProjectionMatrix.M[3][3] < 1.0f;
	}

	const FIntRect& GetViewRect() const { return ViewRect; }
	const FIntRect& GetConstrainedViewRect() const { return ConstrainedViewRect; }

	FMatrix ComputeViewProjectionMatrix() const
	{
		return FTranslationMatrix(-ViewOrigin) * ViewRotationMatrix * ProjectionMatrix;
	}

	//Function for retrieving the NearPlane from the existing projection matrix
	static ENGINE_API float GetNearPlaneFromProjectionMatrix(const FMatrix& ProjectionMatrix)
	{
		if (ProjectionMatrix.M[3][3] < 1.0f)
		{
			// Infinite projection with reversed Z.
			return static_cast<float>(ProjectionMatrix.M[3][2]);
		}
		else
		{
			// Ortho projection with reversed Z.
			return static_cast<float>((1.0f - ProjectionMatrix.M[3][2]) / (ProjectionMatrix.M[2][2] == 0.0f ? UE_DELTA : ProjectionMatrix.M[2][2]));
		}
	}

	float GetNearPlaneFromProjectionMatrix() const
	{
		return GetNearPlaneFromProjectionMatrix(ProjectionMatrix);
	}

	// Function for correcting Ortho camera near plane locations to avoid artifacts behind camera view origin
	static ENGINE_API bool UpdateOrthoPlanes(FSceneViewProjectionData* InOutProjectionData, float& NearPlane, float& FarPlane, float HalfOrthoWidth, bool bUseCameraHeightAsViewTarget);
	ENGINE_API inline bool UpdateOrthoPlanes(float& NearPlane, float& FarPlane, float HalfOrthoWidth, bool bUseCameraHeightAsViewTarget)
	{
		return UpdateOrthoPlanes(this, NearPlane, FarPlane, HalfOrthoWidth, bUseCameraHeightAsViewTarget);
	}
	ENGINE_API bool UpdateOrthoPlanes(FMinimalViewInfo& MinimalViewInfo);
	ENGINE_API bool UpdateOrthoPlanes(bool bUseCameraHeightAsViewTarget);
};

/** Method used for primary screen percentage method. */
enum class EPrimaryScreenPercentageMethod
{
	// Add spatial upscale pass at the end of post processing chain, before the secondary upscale.
	SpatialUpscale,

	// Let temporal AA's do the upscale.
	TemporalUpscale,

	// No upscaling or up sampling, just output the view rect smaller.
	// This is useful for VR's render thread dynamic resolution with MSAA.
	RawOutput,
};

/**
 * Method used for second screen percentage method, that is a second spatial upscale pass at the
 * very end, independent of screen percentage show flag.
 */
enum class ESecondaryScreenPercentageMethod
{
	// Helpful to work on aliasing issue on HighDPI monitors.
	NearestSpatialUpscale,

	// Upscale to simulate smaller pixel density on HighDPI monitors.
	LowerPixelDensitySimulation,

	// TODO: Same config as primary upscale?
};

// Construction parameters for a FSceneView
struct FSceneViewInitOptions : public FSceneViewProjectionData
{
	const FSceneViewFamily* ViewFamily;
	FSceneViewStateInterface* SceneViewStateInterface;
	const AActor* ViewActor;
	int32 PlayerIndex;
	FViewElementDrawer* ViewElementDrawer;

	FLinearColor BackgroundColor;
	FLinearColor OverlayColor;
	FLinearColor ColorScale;

	/** For stereoscopic rendering, whether or not this is a full pass, or a primary / secondary pass */
	EStereoscopicPass StereoPass;

	/** For stereoscopic rendering, a unique index to identify the view across view families */
	int32 StereoViewIndex;

	/** Conversion from world units (uu) to meters, so we can scale motion to the world appropriately */
	float WorldToMetersScale;

	/** The view origin and rotation without any stereo offsets applied to it */
	FVector ViewLocation;
	FRotator ViewRotation;

	TSet<FPrimitiveComponentId> HiddenPrimitives;

	/** The primitives which are visible for this view. If the array is not empty, all other primitives will be hidden. */
	TOptional<TSet<FPrimitiveComponentId>> ShowOnlyPrimitives;

	// -1,-1 if not setup
	FIntPoint CursorPos;

	float LODDistanceFactor;

	/** If > 0, overrides the view's far clipping plane with a plane at the specified distance. */
	float OverrideFarClippingPlaneDistance;

	/** World origin offset value. Non-zero only for a single frame when origin is rebased */
	FVector OriginOffsetThisFrame;

	/** Was there a camera cut this frame? */
	bool bInCameraCut;

	/** Whether to use FOV when computing mesh LOD. */
	bool bUseFieldOfViewForLOD;

	/** Actual field of view and that desired by the camera originally */
	float FOV;
	float DesiredFOV;

	/** Whether this view is being used to render a scene capture. */
	bool bIsSceneCapture;

	/** Whether the scene capture is a cube map (bIsSceneCapture will also be set). */
	bool bIsSceneCaptureCube;

	/** Whether this view uses ray tracing, for views that are used to render a scene capture. */
	bool bSceneCaptureUsesRayTracing;

	/** Whether this view is being used to render a reflection capture. */
	bool bIsReflectionCapture;

	/** Whether this view is being used to render a planar reflection. */
	bool bIsPlanarReflection;

#if WITH_EDITOR
	/** default to 0'th view index, which is a bitfield of 1 */
	uint64 EditorViewBitflag;

	/** Whether game screen percentage should be disabled. */
	bool bDisableGameScreenPercentage;
#endif

	FSceneViewInitOptions()
		: ViewFamily(NULL)
		, SceneViewStateInterface(NULL)
		, ViewActor(NULL)
		, PlayerIndex(INDEX_NONE)
		, ViewElementDrawer(NULL)
		, BackgroundColor(FLinearColor::Transparent)
		, OverlayColor(FLinearColor::Transparent)
		, ColorScale(FLinearColor::White)
		, StereoPass(EStereoscopicPass::eSSP_FULL)
		, StereoViewIndex(INDEX_NONE)
		, WorldToMetersScale(100.f)
		, ViewLocation(ForceInitToZero)
		, ViewRotation(ForceInitToZero)
		, CursorPos(-1, -1)
		, LODDistanceFactor(1.0f)
		, OverrideFarClippingPlaneDistance(-1.0f)
		, OriginOffsetThisFrame(ForceInitToZero)
		, bInCameraCut(false)
		, bUseFieldOfViewForLOD(true)
		, FOV(90.f)
		, DesiredFOV(90.f)
		, bIsSceneCapture(false)
		, bIsSceneCaptureCube(false)
		, bSceneCaptureUsesRayTracing(false)
		, bIsReflectionCapture(false)
		, bIsPlanarReflection(false)
#if WITH_EDITOR
		, EditorViewBitflag(1)
		, bDisableGameScreenPercentage(false)
		//@TODO: , const TBitArray<>& InSpriteCategoryVisibility=TBitArray<>()
#endif
	{
	}
};


//////////////////////////////////////////////////////////////////////////

struct FViewMatrices
{
	struct FMinimalInitializer
	{
		FMatrix ViewRotationMatrix = FMatrix::Identity;
		FMatrix ProjectionMatrix = FMatrix::Identity;
		FVector ViewOrigin = FVector::ZeroVector;
		FIntRect ConstrainedViewRect = FIntRect(0, 0, 0, 0);
		FVector CameraToViewTarget = FVector::ZeroVector;
		EStereoscopicPass StereoPass = EStereoscopicPass::eSSP_FULL;
	};

	FViewMatrices()
	{
		ProjectionMatrix.SetIdentity();
		ProjectionNoAAMatrix.SetIdentity();
		InvProjectionMatrix.SetIdentity();
		ViewMatrix.SetIdentity();
		InvViewMatrix.SetIdentity();
		ViewProjectionMatrix.SetIdentity();
		InvViewProjectionMatrix.SetIdentity();
		HMDViewMatrixNoRoll.SetIdentity();
		TranslatedViewMatrix.SetIdentity();
		InvTranslatedViewMatrix.SetIdentity();
		OverriddenTranslatedViewMatrix.SetIdentity();
		OverriddenInvTranslatedViewMatrix.SetIdentity();
		TranslatedViewProjectionMatrix.SetIdentity();
		InvTranslatedViewProjectionMatrix.SetIdentity();
		ScreenToClipMatrix.SetIdentity();
		PreViewTranslation = FVector::ZeroVector;
		ViewOrigin = FVector::ZeroVector;
		CameraToViewTarget = FVector::ZeroVector;
		ProjectionScale = FVector2D::ZeroVector;
		TemporalAAProjectionJitter = FVector2D::ZeroVector;
		ScreenScale = 1.f;
	}

	ENGINE_API FViewMatrices(const FMinimalInitializer& Initializer);
	ENGINE_API FViewMatrices(const FSceneViewInitOptions& InitOptions);

private:

	void Init(const FMinimalInitializer& Initializer);

	/** ViewToClip : UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix		ProjectionMatrix;
	/** ViewToClipNoAA : UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. Don't apply any AA jitter */
	FMatrix		ProjectionNoAAMatrix;
	/** ClipToView : UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix		InvProjectionMatrix;
	// WorldToView..
	FMatrix		ViewMatrix;
	// ViewToWorld..
	FMatrix		InvViewMatrix;
	// WorldToClip : UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix		ViewProjectionMatrix;
	// ClipToWorld : UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix		InvViewProjectionMatrix;
	// HMD WorldToView with roll removed
	FMatrix		HMDViewMatrixNoRoll;
	/** WorldToView with PreViewTranslation. */
	FMatrix		TranslatedViewMatrix;
	/** ViewToWorld with PreViewTranslation. */
	FMatrix		InvTranslatedViewMatrix;
	/** WorldToView with PreViewTranslation. */
	FMatrix		OverriddenTranslatedViewMatrix;
	/** ViewToWorld with PreViewTranslation. */
	FMatrix		OverriddenInvTranslatedViewMatrix;
	/** The view-projection transform, starting from world-space points translated by -ViewOrigin. */
	FMatrix		TranslatedViewProjectionMatrix;
	/** The inverse view-projection transform, ending with world-space points translated by -ViewOrigin. */
	FMatrix		InvTranslatedViewProjectionMatrix;
	/** The screen to clip matrix (defined depending on whether this is a perspective or ortho projection view)*/
	FMatrix		ScreenToClipMatrix;
	/** The translation to apply to the world before TranslatedViewProjectionMatrix. Usually it is -ViewOrigin but with rereflections this can differ */
	FVector		PreViewTranslation;
	/** The camera/viewport location in world space */
	FVector		ViewOrigin;
	/** The current view target's location proportional to the camera location */
	FVector		CameraToViewTarget;
	/** Scale applied by the projection matrix in X and Y. */
	FVector2D	ProjectionScale;
	/** TemporalAA jitter offset currently stored in the projection matrix */
	FVector2D	TemporalAAProjectionJitter;

	/**
	 * Scale factor to use when computing the size of a sphere in pixels.
	 * 
	 * A common calculation is to determine the size of a sphere in pixels when projected on the screen:
	 *		ScreenRadius = max(0.5 * ViewSizeX * ProjMatrix[0][0], 0.5 * ViewSizeY * ProjMatrix[1][1]) * SphereRadius / ProjectedSpherePosition.W
	 * Instead you can now simply use:
	 *		ScreenRadius = ScreenScale * SphereRadius / ProjectedSpherePosition.W
	 */
	float ScreenScale;

	/** Depth test scaling; differs between perspective and orthographic  */
	float PerProjectionDepthThicknessScale;

	//
	// World = TranslatedWorld - PreViewTranslation
	// TranslatedWorld = World + PreViewTranslation
	// 

	// ----------------

public:
	ENGINE_API void UpdateViewMatrix(const FVector& ViewLocation, const FRotator& ViewRotation);

	void UpdatePlanarReflectionViewMatrix(const FSceneView& SourceView, const FMirrorMatrix& MirrorMatrix);

	inline const FMatrix& GetProjectionMatrix() const
	{
		return ProjectionMatrix;
	}

	inline const FMatrix& GetProjectionNoAAMatrix() const
	{
		return ProjectionNoAAMatrix;
	}

	inline const FMatrix& GetInvProjectionMatrix() const
	{
		return InvProjectionMatrix;
	}

	inline const FMatrix& GetViewMatrix() const
	{
		return ViewMatrix;
	}

	inline const FMatrix& GetInvViewMatrix() const
	{
		return InvViewMatrix;
	}

	inline const FMatrix& GetViewProjectionMatrix() const
	{
		return ViewProjectionMatrix;
	}

	inline const FMatrix& GetInvViewProjectionMatrix() const
	{
		return InvViewProjectionMatrix;
	}
	
	inline const FMatrix& GetHMDViewMatrixNoRoll() const
	{
		return HMDViewMatrixNoRoll;
	}
	
	inline const FMatrix& GetTranslatedViewMatrix() const
	{
		return TranslatedViewMatrix;
	}

	inline const FMatrix& GetInvTranslatedViewMatrix() const
	{
		return InvTranslatedViewMatrix;
	}

	inline const FMatrix& GetOverriddenTranslatedViewMatrix() const
	{
		return OverriddenTranslatedViewMatrix;
	}

	inline const FMatrix& GetOverriddenInvTranslatedViewMatrix() const
	{
		return OverriddenInvTranslatedViewMatrix;
	}

	inline const FMatrix& GetTranslatedViewProjectionMatrix() const
	{
		return TranslatedViewProjectionMatrix;
	}

	inline const FMatrix& GetInvTranslatedViewProjectionMatrix() const
	{
		return InvTranslatedViewProjectionMatrix;
	}

	inline const FMatrix& GetScreenToClipMatrix() const
	{
		return ScreenToClipMatrix;
	}

	inline const FVector& GetPreViewTranslation() const
	{
		return PreViewTranslation;
	}
	
	inline const FVector& GetViewOrigin() const
	{
		return ViewOrigin;
	}

	inline const FVector& GetCameraToViewTarget() const
	{
		return CameraToViewTarget;
	}

	inline float GetScreenScale() const
	{
		return ScreenScale;
	}

	inline const FVector2D& GetProjectionScale() const
	{
		return ProjectionScale;
	} 

	/** @return true:perspective, false:orthographic */
	inline bool IsPerspectiveProjection() const
	{
		return ProjectionMatrix.M[3][3] < 1.0f;
	}

	inline FVector2f GetInvTanHalfFov() const
	{
		//No concept of FOV for orthographic projection so only return perspective related values or 1.0f
		if (IsPerspectiveProjection())
		{
			return FVector2f(static_cast<float>(ProjectionMatrix.M[0][0]), static_cast<float>(ProjectionMatrix.M[1][1]));
		}
		return  FVector2f(1.0f, 1.0f);
	}

	inline FVector2f GetTanHalfFov() const
	{
		//No concept of FOV for orthographic projection so only return perspective related values or 1.0f
		if (IsPerspectiveProjection())
		{
			return FVector2f(static_cast<float>(InvProjectionMatrix.M[0][0]), static_cast<float>(InvProjectionMatrix.M[1][1]));
		}
		return  FVector2f(1.0f,1.0f);
	}

	//Used for initializing the View Uniform Buffer TanAndInvTanHalfFOV variable without repeated calls to IsPerspectiveProjection
	inline FVector4f GetTanAndInvTanHalfFOV() const
	{
		//No concept of FOV for orthographic projection so only return perspective related values or 1.0f
		if (IsPerspectiveProjection())
		{
			return FVector4f(static_cast<float>(InvProjectionMatrix.M[0][0]), //ClipToView[0][0] - X axis
							 static_cast<float>(InvProjectionMatrix.M[1][1]), //ClipToView[1][1] - Y axis
							 static_cast<float>(ProjectionMatrix.M[0][0]), //ViewToClip[0][0] - 1/X axis
							 static_cast<float>(ProjectionMatrix.M[1][1])); //ViewToClip[1][1] - 1/Y axis
		}
		return FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
	}

	inline float GetPerProjectionDepthThicknessScale() const
	{
		return PerProjectionDepthThicknessScale;
	}

	FMatrix ScreenToClipProjectionMatrix() const;

	ENGINE_API void HackOverrideViewMatrixForShadows(const FMatrix& InViewMatrix);

	void SaveProjectionNoAAMatrix()
	{
		ProjectionNoAAMatrix = ProjectionMatrix;
	}

	void HackAddTemporalAAProjectionJitter(const FVector2D& InTemporalAAProjectionJitter)
	{
		ensure(TemporalAAProjectionJitter.X == 0.0f && TemporalAAProjectionJitter.Y == 0.0f);

		TemporalAAProjectionJitter = InTemporalAAProjectionJitter;

		if (IsPerspectiveProjection())
		{
			ProjectionMatrix.M[2][0] += TemporalAAProjectionJitter.X;
			ProjectionMatrix.M[2][1] += TemporalAAProjectionJitter.Y;
		}
		else
		{
			ProjectionMatrix.M[3][0] += TemporalAAProjectionJitter.X;
			ProjectionMatrix.M[3][1] += TemporalAAProjectionJitter.Y;
		}
		InvProjectionMatrix = InvertProjectionMatrix(ProjectionMatrix);

		RecomputeDerivedMatrices();
	}

	void HackRemoveTemporalAAProjectionJitter()
	{
		if (IsPerspectiveProjection())
		{
			ProjectionMatrix.M[2][0] -= TemporalAAProjectionJitter.X;
			ProjectionMatrix.M[2][1] -= TemporalAAProjectionJitter.Y;
		}
		else
		{
			ProjectionMatrix.M[3][0] -= TemporalAAProjectionJitter.X;
			ProjectionMatrix.M[3][1] -= TemporalAAProjectionJitter.Y;
		}
		InvProjectionMatrix = InvertProjectionMatrix(ProjectionMatrix);

		TemporalAAProjectionJitter = FVector2D::ZeroVector;
		RecomputeDerivedMatrices();
	}

	const FMatrix ComputeProjectionNoAAMatrix() const
	{
		FMatrix ProjNoAAMatrix = ProjectionMatrix;

		if (IsPerspectiveProjection())
		{
			ProjNoAAMatrix.M[2][0] -= TemporalAAProjectionJitter.X;
			ProjNoAAMatrix.M[2][1] -= TemporalAAProjectionJitter.Y;
		}
		else
		{
			ProjNoAAMatrix.M[3][0] -= TemporalAAProjectionJitter.X;
			ProjNoAAMatrix.M[3][1] -= TemporalAAProjectionJitter.Y;
		}

		return ProjNoAAMatrix;
	}

	inline const FVector2D GetTemporalAAJitter() const
	{
		return TemporalAAProjectionJitter;
	}

	const FMatrix ComputeViewRotationProjectionMatrix() const
	{
		return ViewMatrix.RemoveTranslation() * ProjectionMatrix;
	}
	
	const FMatrix ComputeInvProjectionNoAAMatrix() const
	{
		return InvertProjectionMatrix( ComputeProjectionNoAAMatrix() );
	}

	// @return in radians (horizontal,vertical)
	const FVector2D ComputeHalfFieldOfViewPerAxis() const
	{
		if(IsPerspectiveProjection())
		{
			const FMatrix ClipToView = ComputeInvProjectionNoAAMatrix();

			FVector VCenter = FVector(ClipToView.TransformPosition(FVector(0.0, 0.0, 0.0)));
			FVector VUp = FVector(ClipToView.TransformPosition(FVector(0.0, 1.0, 0.0)));
			FVector VRight = FVector(ClipToView.TransformPosition(FVector(1.0, 0.0, 0.0)));

			VCenter.Normalize();
			VUp.Normalize();
			VRight.Normalize();

			using ResultType = decltype(FVector2D::X);
			return FVector2D((ResultType)FMath::Acos(VCenter | VRight), (ResultType)FMath::Acos(VCenter | VUp));
		}
		return FVector2D::Zero();
	}

	FMatrix::FReal ComputeNearPlane() const
	{
		return ( ProjectionMatrix.M[3][3] - ProjectionMatrix.M[3][2] ) / ( ProjectionMatrix.M[2][2] - ProjectionMatrix.M[2][3] );
	}

	FMatrix::FReal ComputeFarPlane() const
	{
		return ComputeNearPlane() - InvProjectionMatrix.M[2][2];
	}

	void ApplyWorldOffset(const FVector& InOffset)
	{
		ViewOrigin+= InOffset;
		PreViewTranslation-= InOffset;
	
		ViewMatrix.SetOrigin(ViewMatrix.GetOrigin() + ViewMatrix.TransformVector(-InOffset));
		InvViewMatrix.SetOrigin(ViewOrigin);
		RecomputeDerivedMatrices();
	}

	FVector2f GetOrthoDimensions() const
	{
		if (!IsPerspectiveProjection())
		{
			return  FVector2f(static_cast<float>(InvProjectionMatrix.M[0][0]) * 2.0f, static_cast<float>(InvProjectionMatrix.M[1][1]) * 2.0f);
		}
		return FVector2f::ZeroVector;
	}

private:
	inline void RecomputeDerivedMatrices()
	{
		// Compute the view projection matrix and its inverse.
		ViewProjectionMatrix = GetViewMatrix() * GetProjectionMatrix();
		InvViewProjectionMatrix = GetInvProjectionMatrix() * GetInvViewMatrix();

		// Compute a transform from view origin centered world-space to clip space.
		TranslatedViewProjectionMatrix = GetTranslatedViewMatrix() * GetProjectionMatrix();
		InvTranslatedViewProjectionMatrix = GetInvProjectionMatrix() * GetInvTranslatedViewMatrix();
	}

	static const FMatrix InvertProjectionMatrix( const FMatrix& M )
	{
		if( M.M[1][0] == 0.0f &&
			M.M[3][0] == 0.0f &&
			M.M[0][1] == 0.0f &&
			M.M[3][1] == 0.0f &&
			M.M[0][2] == 0.0f &&
			M.M[1][2] == 0.0f &&
			M.M[0][3] == 0.0f &&
			M.M[1][3] == 0.0f &&
			M.M[2][3] == 1.0f &&
			M.M[3][3] == 0.0f )
		{
			// Solve the common case directly with very high precision.
			/*
			M = 
			| a | 0 | 0 | 0 |
			| 0 | b | 0 | 0 |
			| s | t | c | 1 |
			| 0 | 0 | d | 0 |
			*/

			double a = M.M[0][0];
			double b = M.M[1][1];
			double c = M.M[2][2];
			double d = M.M[3][2];
			double s = M.M[2][0];
			double t = M.M[2][1];

			return FMatrix(
				FPlane( 1.0 / a, 0.0f, 0.0f, 0.0f ),
				FPlane( 0.0f, 1.0 / b, 0.0f, 0.0f ),
				FPlane( 0.0f, 0.0f, 0.0f, 1.0 / d ),
				FPlane( -s/a, -t/b, 1.0f, -c/d )
			);
		}
		else
		{
			return M.Inverse();
		}
	}
};

//////////////////////////////////////////////////////////////////////////

static const int MAX_MOBILE_SHADOWCASCADES = 4;

/** The uniform shader parameters for a mobile directional light and its shadow.
  * One uniform buffer will be created for the first directional light in each lighting channel.
  */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(FMobileDirectionalLightShaderParameters, ENGINE_API)
	SHADER_PARAMETER_EX(FLinearColor, DirectionalLightColor, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX(FVector4f, DirectionalLightDirectionAndShadowTransition, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX(FVector4f, DirectionalLightShadowSize, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_EX(FVector4f, DirectionalLightDistanceFadeMADAndSpecularScale, EShaderPrecisionModifier::Half) // .z is used for SpecularScale, .w is not used atm
	SHADER_PARAMETER_EX(FVector4f, DirectionalLightShadowDistances, EShaderPrecisionModifier::Half)
	SHADER_PARAMETER_ARRAY(FMatrix44f, DirectionalLightScreenToShadow, [MAX_MOBILE_SHADOWCASCADES])
	SHADER_PARAMETER(uint32, DirectionalLightNumCascades)
	SHADER_PARAMETER(uint32, DirectionalLightShadowMapChannelMask)
	SHADER_PARAMETER_TEXTURE(Texture2D, DirectionalLightShadowTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, DirectionalLightShadowSampler)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

//////////////////////////////////////////////////////////////////////////

/** 
 * Enumeration for currently used translucent lighting volume cascades 
 */
enum ETranslucencyVolumeCascade
{
	TVC_Inner,
	TVC_Outer,

	TVC_MAX,
};

#define SKY_IRRADIANCE_ENVIRONMENT_MAP_VEC4_COUNT 8

//VIEW_UNIFORM_BUFFER_MEMBER(FMatrix44f, PrevProjection)
//VIEW_UNIFORM_BUFFER_MEMBER(FMatrix44f, PrevViewProj)
//VIEW_UNIFORM_BUFFER_MEMBER(FMatrix44f, PrevViewRotationProj)

// View uniform buffer member declarations
#define VIEW_UNIFORM_BUFFER_MEMBER_TABLE \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, TranslatedWorldToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, RelativeWorldToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ClipToRelativeWorld)  \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, TranslatedWorldToView) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, TranslatedWorldToCameraView) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, CameraViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ViewToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ViewToClipNoAA) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ClipToView) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ClipToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, SVPositionToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ScreenToRelativeWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ScreenToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, MobileMultiviewShadowTransform) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, ViewOriginHigh) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector3f, ViewForward, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector3f, ViewUp, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector3f, ViewRight, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(FVector3f, HMDViewNoRollUp, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(FVector3f, HMDViewNoRollRight, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, InvDeviceZToWorldZTransform) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(FVector4f, ScreenPositionScaleBias, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, ViewOriginLow) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, TranslatedWorldCameraOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, WorldViewOriginHigh) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, WorldViewOriginLow) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PreViewTranslationHigh) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PreViewTranslationLow) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevViewToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevClipToView) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevTranslatedWorldToClip) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevTranslatedWorldToView) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevTranslatedWorldToCameraView) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevCameraViewToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevTranslatedWorldCameraOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevWorldCameraOriginHigh) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevWorldCameraOriginLow) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevWorldViewOriginHigh) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevWorldViewOriginLow) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevPreViewTranslationHigh) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevPreViewTranslationLow) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, ViewTilePosition) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, RelativeWorldCameraOriginTO) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, RelativeWorldViewOriginTO) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, RelativePreViewTranslationTO) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevRelativeWorldCameraOriginTO) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, PrevRelativeWorldViewOriginTO) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, RelativePrevPreViewTranslationTO) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevClipToRelativeWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, PrevScreenToTranslatedWorld) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ClipToPrevClip) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FMatrix44f, ClipToPrevClipWithAA) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, TemporalAAJitter) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, GlobalClippingPlane) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector2f, FieldOfViewWideAngles) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector2f, PrevFieldOfViewWideAngles) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(FVector4f, ViewRectMin, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, ViewSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FUintVector4, ViewRectMinAndSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, LightProbeSizeRatioAndInvSizeRatio) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, BufferSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, BufferBilinearUVMinMax) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, ScreenToViewSpace) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector2f, BufferToSceneTextureScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, ResolutionFractionAndInv) \
	VIEW_UNIFORM_BUFFER_MEMBER(int32, NumSceneColorMSAASamples) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(float, ProjectionDepthThicknessScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, PreExposure) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, OneOverPreExposure) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4f, DiffuseOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4f, SpecularOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector4f, NormalOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector2f, RoughnessOverrideParameter, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, PrevFrameGameTime) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, PrevFrameRealTime) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, OutOfBoundsMask, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector3f, WorldCameraMovementSinceLastFrame) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, CullingSign) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(float, NearPlane, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GameTime) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, RealTime) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DeltaTime) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MaterialTextureMipBias) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MaterialTextureDerivativeMultiply) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, Random) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, FrameNumber) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, FrameCounter) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, StateFrameIndexMod8) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, StateFrameIndex) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, DebugViewModeMask) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, WorldIsPaused) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, CameraCut, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, UnlitViewmodeMask, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FLinearColor, DirectionalLightColor, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(FVector3f, DirectionalLightDirection, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, TranslucencyLightingVolumeMin, TVC_MAX) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, TranslucencyLightingVolumeInvSize, TVC_MAX) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, TemporalAAParams) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, CircleDOFParams) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldSensorWidth) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFocalDistance) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFocalLength) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFocalRegion) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldNearTransitionRegion) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DepthOfFieldFarTransitionRegion) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MotionBlurNormalizedToPixel) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GeneralPurposeTweak) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GeneralPurposeTweak2) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, DemosaicVposOffset, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DecalDepthBias) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, IndirectLightingColorScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, PrecomputedIndirectLightingColorScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, PrecomputedIndirectSpecularColorScale) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, AtmosphereLightDirection, NUM_ATMOSPHERE_LIGHTS) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FLinearColor, AtmosphereLightIlluminanceOnGroundPostTransmittance, NUM_ATMOSPHERE_LIGHTS) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FLinearColor, AtmosphereLightIlluminanceOuterSpace, NUM_ATMOSPHERE_LIGHTS) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FLinearColor, AtmosphereLightDiscLuminance, NUM_ATMOSPHERE_LIGHTS) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, AtmosphereLightDiscCosHalfApexAngle_PPTrans, NUM_ATMOSPHERE_LIGHTS) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, SkyViewLutSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, SkyCameraTranslatedWorldOrigin) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, SkyPlanetTranslatedWorldCenterAndViewHeight) \
	VIEW_UNIFORM_BUFFER_MEMBER(FMatrix44f, SkyViewLutReferential) \
	VIEW_UNIFORM_BUFFER_MEMBER(FLinearColor, SkyAtmosphereSkyLuminanceFactor) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmospherePresentInScene) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereHeightFogContribution) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereBottomRadiusKm) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereTopRadiusKm) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, SkyAtmosphereCameraAerialPerspectiveVolumeSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereAerialPerspectiveStartDepthKm) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereCameraAerialPerspectiveVolumeDepthResolution) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereCameraAerialPerspectiveVolumeDepthResolutionInv) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereCameraAerialPerspectiveVolumeDepthSliceLengthKm) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereCameraAerialPerspectiveVolumeDepthSliceLengthKmInv) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyAtmosphereApplyCameraAerialPerspectiveVolume) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, NormalCurvatureToRoughnessScaleBias) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, RenderingReflectionCaptureMask) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, RealTimeReflectionCapture) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, RealTimeReflectionCapturePreExposure) \
	VIEW_UNIFORM_BUFFER_MEMBER(FLinearColor, AmbientCubemapTint) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, AmbientCubemapIntensity) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyLightApplyPrecomputedBentNormalShadowingFlag) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyLightAffectReflectionFlag) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyLightAffectGlobalIlluminationFlag) \
	VIEW_UNIFORM_BUFFER_MEMBER(FLinearColor, SkyLightColor) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SkyLightVolumetricScatteringIntensity) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, MobileSkyIrradianceEnvironmentMap, SKY_IRRADIANCE_ENVIRONMENT_MAP_VEC4_COUNT) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MobilePreviewMode) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(float, HMDEyePaddingOffset) \
	VIEW_UNIFORM_BUFFER_MEMBER_EX(float, ReflectionCubemapMaxMip, EShaderPrecisionModifier::Half) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, ShowDecalsMask) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, DistanceFieldAOSpecularOcclusionMode) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, IndirectCapsuleSelfShadowingIntensity) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, ReflectionEnvironmentRoughnessMixingScaleBiasAndLargestWeight) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(int32, StereoPassIndex) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, GlobalVolumeTranslatedCenterAndExtent, GlobalDistanceField::MaxClipmaps) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, GlobalVolumeTranslatedWorldToUVAddAndMul, GlobalDistanceField::MaxClipmaps) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, GlobalDistanceFieldMipTranslatedWorldToUVScale, GlobalDistanceField::MaxClipmaps) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, GlobalDistanceFieldMipTranslatedWorldToUVBias, GlobalDistanceField::MaxClipmaps) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GlobalDistanceFieldMipFactor) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GlobalDistanceFieldMipTransition) \
	VIEW_UNIFORM_BUFFER_MEMBER(int32, GlobalDistanceFieldClipmapSizeInPages) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, GlobalDistanceFieldInvPageAtlasSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, GlobalDistanceFieldInvCoverageAtlasSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GlobalVolumeDimension) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GlobalVolumeTexelSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MaxGlobalDFAOConeDistance) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, NumGlobalSDFClipmaps) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, CoveredExpandSurfaceScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, NotCoveredExpandSurfaceScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, NotCoveredMinStepScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DitheredTransparencyStepThreshold) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, DitheredTransparencyTraceThreshold) \
	VIEW_UNIFORM_BUFFER_MEMBER(FIntPoint, CursorPosition) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, bCheckerboardSubsurfaceProfileRendering) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, VolumetricFogInvGridSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, VolumetricFogGridZParams) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, VolumetricFogSVPosToVolumeUV) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, VolumetricFogViewGridUVToPrevViewRectUV) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, VolumetricFogPrevViewGridRectUVToResourceUV) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, VolumetricFogPrevUVMax) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, VolumetricFogPrevUVMaxForTemporalBlend) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, VolumetricFogScreenToResourceUV) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, VolumetricFogUVMax) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, VolumetricFogMaxDistance) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, VolumetricLightmapWorldToUVScale) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, VolumetricLightmapWorldToUVAdd) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, VolumetricLightmapIndirectionTextureSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, VolumetricLightmapBrickSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, VolumetricLightmapBrickTexelSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, IndirectLightingCacheShowFlag) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, EyeToPixelSpreadAngle) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FVector4f, XRPassthroughCameraUVs, 2) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, GlobalVirtualTextureMipBias) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, VirtualTextureFeedbackShift) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, VirtualTextureFeedbackMask) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, VirtualTextureFeedbackStride) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, VirtualTextureFeedbackJitterOffset) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, VirtualTextureFeedbackSampleOffset) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, RuntimeVirtualTextureMipLevel) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector2f, RuntimeVirtualTexturePackHeight) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, RuntimeVirtualTextureDebugParams) \
	VIEW_UNIFORM_BUFFER_MEMBER(int32, FarShadowStaticMeshLODBias) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, MinRoughness) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, HairRenderInfo) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, EnableSkyLight) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, HairRenderInfoBits) \
	VIEW_UNIFORM_BUFFER_MEMBER(uint32, HairComponents) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, bSubsurfacePostprocessEnabled) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, SSProfilesTextureSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, SSProfilesPreIntegratedTextureSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector4f, SpecularProfileTextureSizeAndInvSize) \
	VIEW_UNIFORM_BUFFER_MEMBER(FVector3f, PhysicsFieldClipmapCenter) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, PhysicsFieldClipmapDistance) \
	VIEW_UNIFORM_BUFFER_MEMBER(int, PhysicsFieldClipmapResolution) \
	VIEW_UNIFORM_BUFFER_MEMBER(int, PhysicsFieldClipmapExponent) \
	VIEW_UNIFORM_BUFFER_MEMBER(int, PhysicsFieldClipmapCount) \
	VIEW_UNIFORM_BUFFER_MEMBER(int, PhysicsFieldTargetCount) \
	VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(FIntVector4, PhysicsFieldTargets, MAX_PHYSICS_FIELD_TARGETS) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(uint32, GPUSceneViewId) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, ViewResolutionFraction) \
	VIEW_UNIFORM_BUFFER_MEMBER(float, SubSurfaceColorAsTransmittanceAtDistanceInMeters) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, TanAndInvTanHalfFOV) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, PrevTanAndInvTanHalfFOV) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector2f, WorldDepthToPixelWorldRadius) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, ScreenRayLengthMultiplier) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, GlintLUTParameters0) \
	VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(FVector4f, GlintLUTParameters1) \
	VIEW_UNIFORM_BUFFER_MEMBER(FIntVector4, EnvironmentComponentsFlags) \

/** The uniform shader parameters associated with a view. */
#define VIEW_UNIFORM_BUFFER_MEMBER(type, identifier) \
	SHADER_PARAMETER(type, identifier)

#define VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(type, identifier) \
	SHADER_PARAMETER(type, identifier)

#define VIEW_UNIFORM_BUFFER_MEMBER_EX(type, identifier, precision) \
	SHADER_PARAMETER_EX(type, identifier, precision)

#define VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(type, identifier, precision) \
	SHADER_PARAMETER_EX(type, identifier, precision)

#define VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(type, identifier, dimension) \
	SHADER_PARAMETER_ARRAY(type, identifier, [dimension])

#define VIEW_UNIFORM_BUFFER_MEMBER_ARRAY_PER_VIEW(type, identifier, dimension) \
	SHADER_PARAMETER_ARRAY(type, identifier, [dimension])

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(FViewUniformShaderParameters, ENGINE_API)

	VIEW_UNIFORM_BUFFER_MEMBER_TABLE

	// Same as Wrap_WorldGroupSettings and Clamp_WorldGroupSettings, but with mipbias=MaterialTextureMipBias.
	SHADER_PARAMETER_SAMPLER(SamplerState, MaterialTextureBilinearWrapedSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, MaterialTextureBilinearClampedSampler)

	SHADER_PARAMETER_TEXTURE(Texture3D<uint4>, VolumetricLightmapIndirectionTexture) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, VolumetricLightmapBrickAmbientVector) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, VolumetricLightmapBrickSHCoefficients0) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, VolumetricLightmapBrickSHCoefficients1) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, VolumetricLightmapBrickSHCoefficients2) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, VolumetricLightmapBrickSHCoefficients3) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, VolumetricLightmapBrickSHCoefficients4) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, VolumetricLightmapBrickSHCoefficients5) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, SkyBentNormalBrickTexture) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_TEXTURE(Texture3D, DirectionalLightShadowingBrickTexture) // FPrecomputedVolumetricLightmapLightingPolicy

	SHADER_PARAMETER_SAMPLER(SamplerState, VolumetricLightmapBrickAmbientVectorSampler) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, VolumetricLightmapTextureSampler0) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, VolumetricLightmapTextureSampler1) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, VolumetricLightmapTextureSampler2) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, VolumetricLightmapTextureSampler3) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, VolumetricLightmapTextureSampler4) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, VolumetricLightmapTextureSampler5) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, SkyBentNormalTextureSampler) // FPrecomputedVolumetricLightmapLightingPolicy
	SHADER_PARAMETER_SAMPLER(SamplerState, DirectionalLightShadowingTextureSampler) // FPrecomputedVolumetricLightmapLightingPolicy

	SHADER_PARAMETER_TEXTURE(Texture3D, GlobalDistanceFieldPageAtlasTexture)
	SHADER_PARAMETER_TEXTURE(Texture3D, GlobalDistanceFieldCoverageAtlasTexture)
	SHADER_PARAMETER_TEXTURE(Texture3D<uint>, GlobalDistanceFieldPageTableTexture)
	SHADER_PARAMETER_TEXTURE(Texture3D, GlobalDistanceFieldMipTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, GlobalDistanceFieldPageAtlasTextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, GlobalDistanceFieldCoverageAtlasTextureSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, GlobalDistanceFieldMipTextureSampler)

	SHADER_PARAMETER_TEXTURE(Texture2D, AtmosphereTransmittanceTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, AtmosphereTransmittanceTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, AtmosphereIrradianceTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, AtmosphereIrradianceTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, AtmosphereInscatterTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, AtmosphereInscatterTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, PerlinNoiseGradientTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, PerlinNoiseGradientTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, PerlinNoise3DTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, PerlinNoise3DTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D<uint>, SobolSamplingTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SharedPointWrappedSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SharedPointClampedSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SharedBilinearWrappedSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SharedBilinearClampedSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SharedBilinearAnisoClampedSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SharedTrilinearWrappedSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SharedTrilinearClampedSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, PreIntegratedBRDF)
	SHADER_PARAMETER_SAMPLER(SamplerState, PreIntegratedBRDFSampler)

	// Change-begin
	SHADER_PARAMETER_TEXTURE(Texture2D, ToonRamp)
	SHADER_PARAMETER_SAMPLER(SamplerState, ToonRampSampler)
	// Change-end

	SHADER_PARAMETER_SRV(StructuredBuffer<float4>, SkyIrradianceEnvironmentMap)
	// Atmosphere
	SHADER_PARAMETER_TEXTURE(Texture2D, TransmittanceLutTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, TransmittanceLutTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, SkyViewLutTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SkyViewLutTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, DistantSkyLightLutTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, DistantSkyLightLutTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, CameraAerialPerspectiveVolume)
	SHADER_PARAMETER_SAMPLER(SamplerState, CameraAerialPerspectiveVolumeSampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, CameraAerialPerspectiveVolumeMieOnly)
	SHADER_PARAMETER_SAMPLER(SamplerState, CameraAerialPerspectiveVolumeMieOnlySampler)
	SHADER_PARAMETER_TEXTURE(Texture3D, CameraAerialPerspectiveVolumeRayOnly)
	SHADER_PARAMETER_SAMPLER(SamplerState, CameraAerialPerspectiveVolumeRayOnlySampler)
	// Hair
	SHADER_PARAMETER_TEXTURE(Texture3D, HairScatteringLUTTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, HairScatteringLUTSampler)
	// GGX/Sheen LTC textures
	SHADER_PARAMETER_TEXTURE(Texture2D, GGXLTCMatTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, GGXLTCMatSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, GGXLTCAmpTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, GGXLTCAmpSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, SheenLTCTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SheenLTCSampler)
	// Energy conservation
	SHADER_PARAMETER(uint32, bShadingEnergyConservation)
	SHADER_PARAMETER(uint32, bShadingEnergyPreservation)
	SHADER_PARAMETER_TEXTURE(Texture2D<float2>, ShadingEnergyGGXSpecTexture)
	SHADER_PARAMETER_TEXTURE(Texture3D<float2>, ShadingEnergyGGXGlassTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D<float2>, ShadingEnergyClothSpecTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D<float>, ShadingEnergyDiffuseTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, ShadingEnergySampler)
	// Glints
	SHADER_PARAMETER_TEXTURE(Texture2DArray<float4>, GlintTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, GlintSampler)
	// Simple volume texture
	SHADER_PARAMETER_TEXTURE(Texture3D<float>, SimpleVolumeTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SimpleVolumeTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture3D<float>, SimpleVolumeEnvTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SimpleVolumeEnvTextureSampler)
	// SSS
	SHADER_PARAMETER_TEXTURE(Texture2D, SSProfilesTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SSProfilesSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SSProfilesTransmissionSampler)
	SHADER_PARAMETER_TEXTURE(Texture2DArray, SSProfilesPreIntegratedTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SSProfilesPreIntegratedSampler)
	// Specular Profile
	SHADER_PARAMETER_TEXTURE(Texture2DArray, SpecularProfileTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SpecularProfileSampler)
	// Water
	SHADER_PARAMETER_SRV(Buffer<float4>, WaterIndirection)
	SHADER_PARAMETER_SRV(Buffer<float4>, WaterData)
	// Rect light atlas
	SHADER_PARAMETER(FVector4f, RectLightAtlasSizeAndInvSize)
	SHADER_PARAMETER(float, RectLightAtlasMaxMipLevel)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, RectLightAtlasTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, RectLightAtlasSampler)
	// IES atlas
	SHADER_PARAMETER(FVector4f, IESAtlasSizeAndInvSize)
	SHADER_PARAMETER_TEXTURE(Texture2DArray<float>, IESAtlasTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, IESAtlasSampler)
	// Landscape
	SHADER_PARAMETER_SAMPLER(SamplerState, LandscapeWeightmapSampler)
	SHADER_PARAMETER_SRV(Buffer<uint>, LandscapeIndirection)
	SHADER_PARAMETER_SRV(Buffer<float>, LandscapePerComponentData)

	SHADER_PARAMETER_UAV(RWStructuredBuffer<uint>, VTFeedbackBuffer)

	SHADER_PARAMETER_SRV(Buffer<float>, PhysicsFieldClipmapBuffer)

	// Ray tracing
	SHADER_PARAMETER(FVector3f, TLASPreViewTranslationHigh)
	SHADER_PARAMETER(FVector3f, TLASPreViewTranslationLow)

END_GLOBAL_SHADER_PARAMETER_STRUCT()

#undef VIEW_UNIFORM_BUFFER_MEMBER
#undef VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW
#undef VIEW_UNIFORM_BUFFER_MEMBER_EX
#undef VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX
#undef VIEW_UNIFORM_BUFFER_MEMBER_ARRAY
#undef VIEW_UNIFORM_BUFFER_MEMBER_ARRAY_PER_VIEW

/** Copy of the view uniform shader parameters associated with a view for instanced stereo. */
#define INSTANCED_VIEW_COUNT 2

#define VIEW_UNIFORM_BUFFER_MEMBER(type, identifier) \
	SHADER_PARAMETER(type, identifier)

#define VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(type, identifier) \
	SHADER_PARAMETER_ARRAY(TShaderParameterTypeInfo<type>::TInstancedType, identifier, [INSTANCED_VIEW_COUNT])

#define VIEW_UNIFORM_BUFFER_MEMBER_EX(type, identifier, precision) \
	SHADER_PARAMETER_EX(type, identifier, precision)

#define VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(type, identifier, precision) \
	SHADER_PARAMETER_ARRAY_EX(TShaderParameterTypeInfo<type>::TInstancedType, identifier, [INSTANCED_VIEW_COUNT], precision)

#define VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(type, identifier, dimension) \
	SHADER_PARAMETER_ARRAY(type, identifier, [dimension])

#define VIEW_UNIFORM_BUFFER_MEMBER_ARRAY_PER_VIEW(type, identifier, dimension) \
	SHADER_PARAMETER_ARRAY(type, identifier, [dimension * INSTANCED_VIEW_COUNT])

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT_WITH_CONSTRUCTOR(FInstancedViewUniformShaderParameters, ENGINE_API)
	VIEW_UNIFORM_BUFFER_MEMBER_TABLE
END_GLOBAL_SHADER_PARAMETER_STRUCT()

#undef VIEW_UNIFORM_BUFFER_MEMBER
#undef VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW
#undef VIEW_UNIFORM_BUFFER_MEMBER_EX
#undef VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX
#undef VIEW_UNIFORM_BUFFER_MEMBER_ARRAY
#undef VIEW_UNIFORM_BUFFER_MEMBER_ARRAY_PER_VIEW
#undef INSTANCED_VIEW_COUNT

#define VIEW_UNIFORM_BUFFER_MEMBER(type, identifier) \
	if ( CopyViewId == 0 ) \
	{ \
		FMemory::Memcpy(&InstancedViewParameters.identifier, &ViewParameters.identifier, sizeof(type)); \
	}

// since per-view attributes are of type type, but the backing structure is TInstancedType for 16 byte alignment, we'd need to mem0 the rest,
// but we rely on FInstancedViewUniformShaderParameters zeroing itself in the constructor
#define VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW(type, identifier) \
	FMemory::Memcpy(&InstancedViewParameters.identifier[CopyViewId], &ViewParameters.identifier, sizeof(type)); \
	check(FMemory::MemIsZero(((uint8*) &InstancedViewParameters.identifier[CopyViewId]) + sizeof(type), sizeof(TShaderParameterTypeInfo<type>::TInstancedType) - sizeof(type)));

#define VIEW_UNIFORM_BUFFER_MEMBER_EX(type, identifier, precision) \
	if ( CopyViewId == 0 ) \
	{ \
		FMemory::Memcpy(&InstancedViewParameters.identifier, &ViewParameters.identifier, sizeof(type)); \
	}

#define VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX(type, identifier, precision) \
	FMemory::Memcpy(&InstancedViewParameters.identifier[CopyViewId], &ViewParameters.identifier, sizeof(type)); \
	check(FMemory::MemIsZero(((uint8*)&InstancedViewParameters.identifier[CopyViewId]) + sizeof(type), sizeof(TShaderParameterTypeInfo<type>::TInstancedType) - sizeof(type)));

#define VIEW_UNIFORM_BUFFER_MEMBER_ARRAY(type, identifier, dimension) \
	if ( CopyViewId == 0 ) \
	{ \
		for (uint32 ElementIndex = 0; ElementIndex < dimension; ElementIndex++) \
		{ \
			FMemory::Memcpy(&InstancedViewParameters.identifier[CopyViewId * dimension + ElementIndex], &ViewParameters.identifier[ElementIndex], sizeof(type)); \
		} \
	}

#define VIEW_UNIFORM_BUFFER_MEMBER_ARRAY_PER_VIEW(type, identifier, dimension) \
	for (uint32 ElementIndex = 0; ElementIndex < dimension; ElementIndex++) \
	{ \
		FMemory::Memcpy(&InstancedViewParameters.identifier[CopyViewId * dimension + ElementIndex], &ViewParameters.identifier[ElementIndex], sizeof(type)); \
		check(FMemory::MemIsZero(((uint8*) &InstancedViewParameters.identifier[CopyViewId * dimension + ElementIndex]) + sizeof(type), sizeof(TShaderParameterTypeInfo<type>::TInstancedType) - sizeof(type))); \
	}

namespace InstancedViewParametersUtils
{
	static void CopyIntoInstancedViewParameters(FInstancedViewUniformShaderParameters& InstancedViewParameters, const FViewUniformShaderParameters& ViewParameters, uint32 CopyViewId)
	{
		VIEW_UNIFORM_BUFFER_MEMBER_TABLE
	}
}

#undef VIEW_UNIFORM_BUFFER_MEMBER
#undef VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW
#undef VIEW_UNIFORM_BUFFER_MEMBER_EX
#undef VIEW_UNIFORM_BUFFER_MEMBER_PER_VIEW_EX
#undef VIEW_UNIFORM_BUFFER_MEMBER_ARRAY
#undef VIEW_UNIFORM_BUFFER_MEMBER_TABLE

BEGIN_SHADER_PARAMETER_STRUCT(FViewShaderParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_REF(FInstancedViewUniformShaderParameters, InstancedView)
END_SHADER_PARAMETER_STRUCT()

namespace EDrawDynamicFlags
{
	enum Type : int32
	{
		None = 0,
		ForceLowestLOD = 0x1,
		FarShadowCascade = 0x2,
	};
}

/**
 * A projection from scene space into a 2D screen region.
 */
class FSceneView
{
public:
	const FSceneViewFamily* Family;
	/** can be 0 (thumbnail rendering) */
	FSceneViewStateInterface* State;

	/** The uniform buffer for the view's parameters. This is only initialized in the rendering thread's copies of the FSceneView. */
	TUniformBufferRef<FViewUniformShaderParameters> ViewUniformBuffer;

private:
	/** During GetDynamicMeshElements this will be the correct cull volume for shadow stuff */
	const FConvexVolume* DynamicMeshElementsShadowCullFrustum;
	/** If the above is non-null, a translation that is applied to world-space before transforming by one of the shadow matrices. */
	FVector		PreShadowTranslation;

public:
	FSceneViewInitOptions SceneViewInitOptions;

	/** The actor which is being viewed from. */
	const AActor* ViewActor;
	 
	/** Player index this view is associated with or INDEX_NONE. */
	int32 PlayerIndex;

	/** An interaction which draws the view's interaction elements. */
	FViewElementDrawer* Drawer;

	/* Final position of the view in the final render target (in pixels), potentially constrained by an aspect ratio requirement (black bars) */
	const FIntRect UnscaledViewRect;

	/* Raw view size (in pixels), used for screen space calculations */
	FIntRect UnconstrainedViewRect;

	/** Maximum number of shadow cascades to render with. */
	int32 MaxShadowCascades;

	FViewMatrices ViewMatrices;

	/** Variables used to determine the view matrix */
	FVector		ViewLocation;
	FRotator	ViewRotation;
	FQuat		BaseHmdOrientation;
	FVector		BaseHmdLocation;
	float		WorldToMetersScale;
	TOptional<FTransform> PreviousViewTransform;

	// normally the same as ViewMatrices unless "r.Shadow.FreezeCamera" is activated
	FViewMatrices ShadowViewMatrices;

	FMatrix ProjectionMatrixUnadjustedForRHI;

	FLinearColor BackgroundColor;
	FLinearColor OverlayColor;

	/** Color scale multiplier used during post processing */
	FLinearColor ColorScale;

	/** For stereoscopic rendering, whether or not this is a full pass, or a left / right eye pass */
	EStereoscopicPass StereoPass;

	/** For stereoscopic rendering, unique index identifying the view across view families */
	int32 StereoViewIndex;

	/** For stereoscopic rendering, view family index of the primary view associated with this view */
	int32 PrimaryViewIndex;

	/** Allow cross GPU transfer for this view */
	bool bAllowCrossGPUTransfer;

	/** Use custom GPUmask */
	bool bOverrideGPUMask;

	/** The GPU nodes on which to render this view. */
	FRHIGPUMask GPUMask;

	/** Whether this view should render the first instance only of any meshes using instancing. */
	bool bRenderFirstInstanceOnly;

	// Whether to use FOV when computing mesh LOD.
	bool bUseFieldOfViewForLOD;

	// Whether this view should use an HMD hidden area mask where appropriate.
	bool bHMDHiddenAreaMaskActive = false;

	/** Actual field of view and that desired by the camera originally */
	float FOV;
	float DesiredFOV;

	EDrawDynamicFlags::Type DrawDynamicFlags;

	/** Current buffer visualization mode */
	FName CurrentBufferVisualizationMode;

	/** Current Nanite visualization mode */
	FName CurrentNaniteVisualizationMode;

	/** Current Lumen visualization mode */
	FName CurrentLumenVisualizationMode;

	/** Current Substrate visualization mode */
	FName CurrentSubstrateVisualizationMode;

	/** Current Groom visualization mode */
	FName CurrentGroomVisualizationMode;

	/** Current Virtual Shadow Map visualization mode */
	FName CurrentVirtualShadowMapVisualizationMode;

	/** Current visualize calibration color material name */
	FName CurrentVisualizeCalibrationColorMaterialName;

	/** Current visualize calibration grayscale material name */
	FName CurrentVisualizeCalibrationGrayscaleMaterialName;

	/** Current visualize calibration custom material name */
	FName CurrentVisualizeCalibrationCustomMaterialName;

	/** Current GPU Skin Cache visualization mode*/
	FName CurrentGPUSkinCacheVisualizationMode;

#if WITH_EDITOR
	/* Whether to use the pixel inspector */
	bool bUsePixelInspector;
#endif //WITH_EDITOR

	/**
	* These can be used to override material parameters across the scene without recompiling shaders.
	* The last component is how much to include of the material's value for that parameter, so 0 will completely remove the material's value.
	*/
	FVector4f DiffuseOverrideParameter;
	FVector4f SpecularOverrideParameter;
	FVector4f NormalOverrideParameter;
	FVector2D RoughnessOverrideParameter;

	/** Mip bias to apply in material's samplers. */
	float MaterialTextureMipBias;

	/** The primitives which are hidden for this view. */
	TSet<FPrimitiveComponentId> HiddenPrimitives;

	/** The primitives which are visible for this view. If the array is not empty, all other primitives will be hidden. */
	TOptional<TSet<FPrimitiveComponentId>> ShowOnlyPrimitives;

	// Derived members.

	bool bAllowTemporalJitter;

	FConvexVolume ViewFrustum;

	bool bHasNearClippingPlane;

	FPlane NearClippingPlane;

	float NearClippingDistance;

	/** Monoscopic culling frustum, same as ViewFrustum in case of non-stereo */
	FConvexVolume CullingFrustum;

	/** Monoscopic culling origin, same as the view matrix origin in case of non-stereo */
	FVector CullingOrigin;

	/** true if ViewMatrix.Determinant() is negative. */
	bool bReverseCulling;

	/* Vector used by shaders to convert depth buffer samples into z coordinates in world space */
	FVector4f InvDeviceZToWorldZTransform;

	/** World origin offset value. Non-zero only for a single frame when origin is rebased */
	FVector OriginOffsetThisFrame;

	/** Multiplier for cull distance on objects */
	float LODDistanceFactor;

	/** Whether we did a camera cut for this view this frame. */
	bool bCameraCut;
	
	// -1,-1 if not setup
	FIntPoint CursorPos;

	/** True if this scene was created from a game world. */
	bool bIsGameView;

	/** For sanity checking casts that are assumed to be safe. */
	bool bIsViewInfo;

	class FCustomRenderPassBase* CustomRenderPass = nullptr;

	/** Whether this view is being used to render a scene capture. */
	bool bIsSceneCapture;

	/** Whether the scene capture is a cube map (bIsSceneCapture will also be set). */
	bool bIsSceneCaptureCube;

	/** Whether this view uses ray tracing, for views that are used to render a scene capture. */
	bool bSceneCaptureUsesRayTracing;

	/** Whether this view is being used to render a reflection capture. */
	bool bIsReflectionCapture;

	/** Whether this view is being used to render a planar reflection. */
	bool bIsPlanarReflection;

	/** Whether this view is being used to render a runtime virtual texture. */
	bool bIsVirtualTexture;

	/** Whether this view is being used to render a high quality offline render */
	bool bIsOfflineRender;

	/** Whether to force two sided rendering for this view. */
	bool bRenderSceneTwoSided;

	/** Whether this view was created from a locked viewpoint. */
	bool bIsLocked;

	/** 
	 * Whether to only render static lights and objects.  
	 * This is used when capturing the scene for reflection captures, which aren't updated at runtime. 
	 */
	bool bStaticSceneOnly;

	/** True if instanced stereo is enabled. */
	bool bIsInstancedStereoEnabled;

	/** OLD variable governing multi-viewport rendering. */
	UE_DEPRECATED(5.1, "bIsMultiViewEnabled has been deprecated. Use bIsMultiViewportEnabled")
	bool bIsMultiViewEnabled;

	/** True if multi-viewport instanced stereo rendering is enabled. */
	bool bIsMultiViewportEnabled;

	/** True if mobile multi-view is enabled. */
	bool bIsMobileMultiViewEnabled;

	/** True if we need to bind the instanced view uniform buffer parameters. */
	bool bShouldBindInstancedViewUB;

	/** How far below the water surface this view is. -1 means the view is out of water. */
	float UnderwaterDepth;

	/** True if we need to force the camera to discard previous frames occlusion. Necessary for overlapped tile rendering
	 * where we discard previous frame occlusion because the projection matrix changes.
	 */
	bool bForceCameraVisibilityReset;

	/** True if we should force the path tracer to reset its internal accumulation state */
	bool bForcePathTracerReset;

	/** Whether we should disable distance-based fade transitions for this frame (usually after a large camera movement.) */
	bool bDisableDistanceBasedFadeTransitions;

	/** Global clipping plane being applied to the scene, or all 0's if disabled.  This is used when rendering the planar reflection pass. */
	FPlane GlobalClippingPlane;

	/** Aspect ratio constrained view rect. In the editor, when attached to a camera actor and the camera black bar showflag is enabled, the normal viewrect 
	  * remains as the full viewport, and the black bars are just simulated by drawing black bars. This member stores the effective constrained area within the
	  * bars.
	 **/
	FIntRect CameraConstrainedViewRect;

	/** Sort axis for when TranslucentSortPolicy is SortAlongAxis */
	FVector TranslucentSortAxis;

	/** Translucent sort mode */
	TEnumAsByte<ETranslucentSortPolicy::Type> TranslucentSortPolicy;
	
	/** The frame index to override, useful for keeping determinism when rendering sequences. **/
	TOptional<uint32> OverrideFrameIndexValue;

	/** In some cases, the principal point of the lens is not at the center of the screen, especially for overlapped tile
	 *  rendering. So given a UV in [-1,1] viewport space, convert it to the [-1,1] viewport space of the lens using
	 *  LensUV = LensPrincipalPointOffsetScale.xy ScreenUV * LensPrincipalPointOffsetScale.zw;
	 *  This value is FVector4(0,0,1,1) unless overridden.
	 */
	FVector4f LensPrincipalPointOffsetScale;

	/** Whether to enable motion blur caused by camera movements */
	TOptional<bool> bCameraMotionBlur;

	/** Matrix overrides PrevViewToClip in the view uniform buffer.
		Used for replacing motion blur caused by the current camera movements with a custom transform.*/
	TOptional<FMatrix> ClipToPrevClipOverride;

#if WITH_EDITOR
	/** The set of (the first 64) groups' visibility info for this view */
	uint64 EditorViewBitflag;

	/** True if we should draw translucent objects when rendering hit proxies */
	bool bAllowTranslucentPrimitivesInHitProxy;

	/** BitArray representing the visibility state of the various sprite categories in the editor for this view */
	TBitArray<> SpriteCategoryVisibility;
	/** Selection color for the editor (used by post processing) */
	FLinearColor SelectionOutlineColor;
	/** Selection color for use in the editor with inactive primitives */
	FLinearColor SubduedSelectionOutlineColor;
	/** Additional selection colors for the editor (used by post processing) */
	TStaticArray<FLinearColor, 6> AdditionalSelectionOutlineColors;
	/** True if any components are selected in isolation (independent of actor selection) */
	bool bHasSelectedComponents;
#endif

	/**
	 * The final settings for the current viewer position (blended together from many volumes).
	 * Setup by the main thread, passed to the render thread and never touched again by the main thread.
	 */
	FFinalPostProcessSettings FinalPostProcessSettings;
#if DEBUG_POST_PROCESS_VOLUME_ENABLE
	TArray<FPostProcessSettingsDebugInfo> FinalPostProcessDebugInfo;
#endif

	// The antialiasing method.
	EAntiAliasingMethod AntiAliasingMethod;

	// Primary screen percentage method to use.
	EPrimaryScreenPercentageMethod PrimaryScreenPercentageMethod;

	/** Parameters for atmospheric fog. */
	FTextureRHIRef AtmosphereTransmittanceTexture;
	FTextureRHIRef AtmosphereIrradianceTexture;
	FTextureRHIRef AtmosphereInscatterTexture;

	/** Water rendering related data */
	FShaderResourceViewRHIRef WaterIndirectionBuffer;
	FShaderResourceViewRHIRef WaterDataBuffer;

	struct FWaterInfoTextureRenderingParams
	{
		FRenderTarget* RenderTarget = nullptr;
		FVector ViewLocation = FVector::Zero();
		FMatrix ViewRotationMatrix = FMatrix(EForceInit::ForceInit);
		FMatrix ProjectionMatrix = FMatrix(EForceInit::ForceInit);
		TSet<FPrimitiveComponentId> TerrainComponentIds;
		TSet<FPrimitiveComponentId> WaterBodyComponentIds;
		TSet<FPrimitiveComponentId> DilatedWaterBodyComponentIds;
		FVector WaterZoneExtents = FVector::Zero();
		FVector2f WaterHeightExtents = FVector2f::ZeroVector;
		float GroundZMin = 0.0f;
		float CaptureZ = 0.0f;
		int32 VelocityBlurRadius = 0;
	};
	TArray<FWaterInfoTextureRenderingParams> WaterInfoTextureRenderingParams;

	/** Landscape rendering related data */
	FShaderResourceViewRHIRef LandscapeIndirectionBuffer;
	FShaderResourceViewRHIRef LandscapePerComponentDataBuffer;

	/** Enables a CoC offset whose offset value changes with pixel's scene depth */
	bool bEnableDynamicCocOffset = false;

	/** When dynamic CoC offset is enabled, this is the distance from camera at which objects will be in perfect focus (when the CoC offset is maximum so that the final CoC is 0) */
	float InFocusDistance = 0.0;

	/** Lookup table for the dynamic CoC offset */
	FTextureRHIRef DynamicCocOffsetLUT;

	/** Feature level for this scene */
	const ERHIFeatureLevel::Type FeatureLevel;

#if RHI_RAYTRACING
	/** Use to allow ray tracing on this view. */
	bool bAllowRayTracing = true;
#endif

protected:
	friend class FSceneRenderer;

	/** Some views get cloned for certain renders, like shadows (see FViewInfo::CreateSnapshot()). If that's the case, this will point to the view this one originates from */
	const FSceneView* SnapshotOriginView = nullptr;

public:

	/** Initialization constructor. */
	ENGINE_API FSceneView(const FSceneViewInitOptions& InitOptions);

	/** These are only needed because of deprecated members being accessed in them */
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSceneView(const FSceneView& Other) = default;
	FSceneView(FSceneView&& Other) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Default destructor */
	virtual ~FSceneView() = default;

#if DO_CHECK || USING_CODE_ANALYSIS
	/** Verifies all the assertions made on members. */
	ENGINE_API bool VerifyMembersChecks() const;
#endif

	FORCEINLINE bool AllowGPUParticleUpdate() const { return !bIsPlanarReflection && !bIsSceneCapture && !bIsReflectionCapture; }

	/** Transforms a point from world-space to the view's screen-space. */
	ENGINE_API FVector4 WorldToScreen(const FVector& WorldPoint) const;

	/** Transforms a point from the view's screen-space to world-space. */
	ENGINE_API FVector ScreenToWorld(const FVector4& ScreenPoint) const;

	/** Transforms a point from the view's screen-space into pixel coordinates relative to the view's X,Y. */
	ENGINE_API bool ScreenToPixel(const FVector4& ScreenPoint,FVector2D& OutPixelLocation) const;

	/** Transforms a point from pixel coordinates relative to the view's X,Y (left, top) into the view's screen-space. */
	ENGINE_API FVector4 PixelToScreen(float X,float Y,float Z) const;

	/** Transforms a cursor location in render target pixel coordinates into the view's screen-space, taking into account the viewport rectangle. */
	ENGINE_API FVector4 CursorToScreen(float X, float Y, float Z) const;

	/** Transforms a point from the view's world-space into pixel coordinates relative to the view's X,Y (left, top). */
	ENGINE_API bool WorldToPixel(const FVector& WorldPoint,FVector2D& OutPixelLocation) const;

	/** Transforms a point from pixel coordinates relative to the view's X,Y (left, top) into the view's world-space. */
	ENGINE_API FVector4 PixelToWorld(float X,float Y,float Z) const;

	/** 
	 * Transforms a point from the view's world-space into the view's screen-space. 
	 * Divides the resulting X, Y, Z by W before returning. 
	 */
	ENGINE_API FPlane Project(const FVector& WorldPoint) const;

	/** 
	 * Transforms a point from the view's screen-space into world coordinates
	 * multiplies X, Y, Z by W before transforming. 
	 */
	ENGINE_API FVector Deproject(const FPlane& ScreenPoint) const;

	/** 
	 * Transforms 2D screen coordinates into a 3D world-space origin and direction 
	 * @param ScreenPos - screen coordinates in pixels
	 * @param out_WorldOrigin (out) - world-space origin vector
	 * @param out_WorldDirection (out) - world-space direction vector
	 */
	ENGINE_API void DeprojectFVector2D(const FVector2D& ScreenPos, FVector& out_WorldOrigin, FVector& out_WorldDirection) const;

	/** 
	 * Transforms 2D screen coordinates into a 3D world-space origin and direction 
	 * @param ScreenPos - screen coordinates in pixels
	 * @param ViewRect - view rectangle
	 * @param InvViewMatrix - inverse view matrix
	 * @param InvProjMatrix - inverse projection matrix
	 * @param out_WorldOrigin (out) - world-space origin vector
	 * @param out_WorldDirection (out) - world-space direction vector
	 */
	static ENGINE_API void DeprojectScreenToWorld(const FVector2D& ScreenPos, const FIntRect& ViewRect, const FMatrix& InvViewMatrix, const FMatrix& InvProjMatrix, FVector& out_WorldOrigin, FVector& out_WorldDirection);

	/** Overload to take a single combined view projection matrix. */
	static ENGINE_API void DeprojectScreenToWorld(const FVector2D& ScreenPos, const FIntRect& ViewRect, const FMatrix& InvViewProjMatrix, FVector& out_WorldOrigin, FVector& out_WorldDirection);

	/** 
	 * Transforms 3D world-space origin into 2D screen coordinates
	 * @param WorldPosition - the 3d world point to transform
	 * @param ViewRect - view rectangle
	 * @param ViewProjectionMatrix - combined view projection matrix
	 * @param out_ScreenPos (out) - screen coordinates in pixels
	 * @param bShouldCalcOutsideViewPosition - if enabled, calculates the out_ScreenPos if the WorldPosition is outside of ViewProjectionMatrix
	 */
	static ENGINE_API bool ProjectWorldToScreen(const FVector& WorldPosition, const FIntRect& ViewRect, const FMatrix& ViewProjectionMatrix, FVector2D& out_ScreenPos, bool bShouldCalcOutsideViewPosition = false);

	inline FVector GetViewRight() const { return ViewMatrices.GetViewMatrix().GetColumn(0); }
	inline FVector GetViewUp() const { return ViewMatrices.GetViewMatrix().GetColumn(1); }
	inline FVector GetViewDirection() const { return ViewMatrices.GetViewMatrix().GetColumn(2); }

	inline const FConvexVolume* GetDynamicMeshElementsShadowCullFrustum() const { return DynamicMeshElementsShadowCullFrustum; }
	inline void SetDynamicMeshElementsShadowCullFrustum(const FConvexVolume* InDynamicMeshElementsShadowCullFrustum) { DynamicMeshElementsShadowCullFrustum = InDynamicMeshElementsShadowCullFrustum; }

	inline const FVector& GetPreShadowTranslation() const { return PreShadowTranslation; }
	inline void SetPreShadowTranslation(const FVector& InPreShadowTranslation) { PreShadowTranslation = InPreShadowTranslation; }

	/** @return true:perspective, false:orthographic */
	inline bool IsPerspectiveProjection() const { return ViewMatrices.IsPerspectiveProjection(); }

	bool IsUnderwater() const { return UnderwaterDepth > 0.0f; }

	/** Returns the location used as the origin for LOD computations
	 * @param Index, 0 or 1, which LOD origin to return
	 * @return LOD origin
	 */
	ENGINE_API FVector GetTemporalLODOrigin(int32 Index, bool bUseLaggedLODTransition = true) const;

	/** 
	 * Returns the blend factor between the last two LOD samples
	 */
	ENGINE_API float GetTemporalLODTransition() const;

	/**
	 * returns the distance field temporal sample index or 0 if there is no view state
	 */
	ENGINE_API uint32 GetDistanceFieldTemporalSampleIndex() const;

	/** 
	 * returns a unique key for the view state if one exists, otherwise returns zero
	 */
	ENGINE_API uint32 GetViewKey() const;

	/** 
	 * returns a the occlusion frame counter or MAX_uint32 if there is no view state
	 */
	ENGINE_API uint32 GetOcclusionFrameCounter() const;

	ENGINE_API void UpdateProjectionMatrix(const FMatrix& NewProjectionMatrix);

	/** Allow things like HMD displays to update the view matrix at the last minute, to minimize perceived latency */
	ENGINE_API void UpdateViewMatrix();

	/** If we late update a view, we need to also late update any planar reflection views derived from it */
	ENGINE_API void UpdatePlanarReflectionViewMatrix(const FSceneView& SourceView, const FMirrorMatrix& MirrorMatrix);

	/** Setup defaults and depending on view position (postprocess volumes) */
	ENGINE_API void StartFinalPostprocessSettings(FVector InViewLocation);

	/**
	 * custom layers can be combined with the existing settings
	 * @param Weight usually 0..1 but outside range is clamped
	 */
	ENGINE_API void OverridePostProcessSettings(const FPostProcessSettings& Src, float Weight);

	/** applied global restrictions from show flags */
	ENGINE_API void EndFinalPostprocessSettings(const FSceneViewInitOptions& ViewInitOptions);

	ENGINE_API void SetupAntiAliasingMethod();

	/** Configure post process settings for the buffer visualization system */
	ENGINE_API void ConfigureBufferVisualizationSettings();

#if !(UE_BUILD_SHIPPING)
	/** Configure post process settings for calibration material */
	ENGINE_API void ConfigureVisualizeCalibrationSettings();
#endif

	/** Get the feature level for this view (cached from the scene so this is not different per view) **/
	ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }

	/** Get the feature level for this view **/
	ENGINE_API EShaderPlatform GetShaderPlatform() const;

	/** True if the view should render as an instanced stereo pass */
	ENGINE_API bool IsInstancedStereoPass() const;

	/** Instance factor for a stereo pass (normally 2 for ISR views, but see IStereoRendering::GetDesiredNumberOfViews()). Returns 1 for non-instanced stereo views or regular (split screen etc) views. */
	ENGINE_API int32 GetStereoPassInstanceFactor() const;

	/** Sets up the view rect parameters in the view's uniform shader parameters */
	ENGINE_API void SetupViewRectUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters, 
		const FIntPoint& InBufferSize,
		const FIntRect& InEffectiveViewRect,
		const FViewMatrices& InViewMatrices,
		const FViewMatrices& InPrevViewMatrice) const;

	ENGINE_API FVector4f GetScreenPositionScaleBias(const FIntPoint& BufferSize, const FIntRect& ViewRect) const;

	/** 
	 * Populates the uniform buffer prameters common to all scene view use cases
	 * View parameters should be set up in this method if they are required for the view to render properly.
	 * This is to avoid code duplication and uninitialized parameters in other places that create view uniform parameters (e.g Slate) 
	 */
	ENGINE_API void SetupCommonViewUniformBufferParameters(FViewUniformShaderParameters& ViewUniformShaderParameters,
		const FIntPoint& InBufferSize,
		int32 NumMSAASamples,
		const FIntRect& InEffectiveViewRect,
		const FViewMatrices& InViewMatrices,
		const FViewMatrices& InPrevViewMatrices) const;

#if RHI_RAYTRACING
	/** Current ray tracing debug visualization mode */
	FName CurrentRayTracingDebugVisualizationMode;
#endif

	UE_DEPRECATED(5.2, "Use HasValidEyeAdaptationBuffer() instead.")
	ENGINE_API bool HasValidEyeAdaptationTexture() const;

	/** Tells if the eye adaptation buffer exists without attempting to allocate it. */
	ENGINE_API bool HasValidEyeAdaptationBuffer() const;

	UE_DEPRECATED(5.2, "Use GetEyeAdaptationBuffer() instead.")
	ENGINE_API IPooledRenderTarget* GetEyeAdaptationTexture() const;

	/** Returns the eye adaptation buffer or null if it doesn't exist. */
	ENGINE_API FRDGPooledBuffer* GetEyeAdaptationBuffer() const;

	/** Returns the eye adaptation exposure or 0.0f if it doesn't exist. */
	ENGINE_API float GetLastEyeAdaptationExposure() const;

	/** Get the primary view associated with the secondary view. */
	ENGINE_API const FSceneView* GetPrimarySceneView() const;

	/** Checks whether this is the primary view of a stereo pair (important in instanced stereo rendering). Will also be true for any view that isn't stereo. */
	inline bool IsPrimarySceneView() const
	{
		return GetPrimarySceneView() == this;
	}

	/** Get the first secondary view associated with the primary view. */
	ENGINE_API const FSceneView* GetInstancedSceneView() const;

	/** Checks whether this is the instanced view of a stereo pair. If the technique supports instanced rendering stereo and ISR is enabled, such views can be skipped. */
	inline bool IsInstancedSceneView() const
	{
		return GetInstancedSceneView() == this;
	}

	/** Get all secondary views associated with the primary view. */
	ENGINE_API TArray<const FSceneView*> GetSecondaryViews() const;

	/** Returns uniform buffer that is used to access (first) instanced view's properties. Note: it is not the same as that view's ViewUniformBuffer, albeit it contains all relevant data from it. */
	const TUniformBufferRef<FInstancedViewUniformShaderParameters>& GetInstancedViewUniformBuffer() const
	{
		return InstancedViewUniformBuffer;
	}

	/** Returns the load action to use when overwriting all pixels of a target that you intend to read from. Takes into account the HMD hidden area mesh. */
	inline ERenderTargetLoadAction GetOverwriteLoadAction() const
	{
		return bHMDHiddenAreaMaskActive ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ENoAction;
	}

	const FSceneView* GetSnapshotOriginView() const { return SnapshotOriginView; }

protected:
	FSceneViewStateInterface* EyeAdaptationViewState = nullptr;

	/** Instanced view uniform buffer held on the primary view. */
	TUniformBufferRef<FInstancedViewUniformShaderParameters> InstancedViewUniformBuffer;
};

//////////////////////////////////////////////////////////////////////////

// for r.DisplayInternals (allows for easy passing down data from main to render thread)
struct FDisplayInternalsData
{
	//
	int32 DisplayInternalsCVarValue;
	// -1 if not set, from IStreamingManager::Get().StreamAllResources(Duration) in FStreamAllResourcesLatentCommand
	uint32 NumPendingStreamingRequests;

	FDisplayInternalsData()
		: DisplayInternalsCVarValue(0)
		, NumPendingStreamingRequests(-1)
	{
		check(!IsValid());
	}

	// called on main thread
	// @param World may be 0
	void Setup(UWorld *World);

	bool IsValid() const { return DisplayInternalsCVarValue != 0; }
};

//////////////////////////////////////////////////////////////////////////

/**
 * Generic plugin extension that have a lifetime of the FSceneViewFamily
 */
class ISceneViewFamilyExtention
{
protected:
	/** 
	 * Called by the destructor of the view family.
	 * Can be called on game or rendering thread.
	 */
	virtual ~ISceneViewFamilyExtention() {};

	friend class FSceneViewFamily;
};

/**
 * Generic plugin extension that have a lifetime of the FSceneViewFamily that can contain arbitrary data to passdown from game thread to render thread.
 */
class ISceneViewFamilyExtentionData : public ISceneViewFamilyExtention
{
public:
	/** Returns a const TCHAR* to uniquely identify what implementation ISceneViewFamilyExtentionData is. */
	virtual const TCHAR* GetSubclassIdentifier() const = 0;
};


/*
 * Game thread and render thread interface that takes care of a FSceneViewFamily's screen percentage.
 *
 * The renderer reserves the right to delete and replace the view family's screen percentage interface
 * for testing purposes with the r.Test.OverrideScreenPercentageInterface CVar.
 */
class ISceneViewFamilyScreenPercentage : ISceneViewFamilyExtention
{
public:
	// Sets the minimal and max screen percentage.
	static constexpr float kMinResolutionFraction = 0.01f;
	static constexpr float kMaxResolutionFraction = 4.0f;

	// Sets the minimal and maximal screen percentage for TSR.
	static constexpr float kMinTSRResolutionFraction = 0.25f;
	static constexpr float kMaxTSRResolutionFraction = 2.0f;

	// Sets the minimal and maximal screen percentage for TAAU.
	static constexpr float kMinTAAUpsampleResolutionFraction = 0.5f;
	static constexpr float kMaxTAAUpsampleResolutionFraction = 2.0f;

#if DO_CHECK || USING_CODE_ANALYSIS
	static bool IsValidResolutionFraction(float ResolutionFraction)
	{
		return ResolutionFraction >= kMinResolutionFraction && ResolutionFraction <= kMaxResolutionFraction;
	}
#endif

	/** 
	 * Method to know the maximum value that can be returned by GetPrimaryResolutionFraction_RenderThread().
	 * Can be called on game or rendering thread. This should return >= 1 if screen percentage show flag is disabled.
	 */
	virtual DynamicRenderScaling::TMap<float> GetResolutionFractionsUpperBound() const = 0;

protected:
	/**
	 * Setup view family's view's screen percentage on rendering thread.
	 * This should leave ResolutionFraction == 1 if screen percentage show flag is disabled.
	 */
	virtual DynamicRenderScaling::TMap<float> GetResolutionFractions_RenderThread() const = 0;

	/** Create a new screen percentage interface for a new view family. */
	virtual ISceneViewFamilyScreenPercentage* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const = 0;

	friend class FSceneViewFamily;
	friend class FSceneRenderer;
};


//////////////////////////////////////////////////////////////////////////

/**
 * A set of views into a scene which only have different view transforms and owner actors.
 */
class FSceneViewFamily
{
public:
	/**
	* Helper struct for creating FSceneViewFamily instances
	* If created with specifying a time it will retrieve them from the world in the given scene.
	* 
	* @param InRenderTarget		The render target which the views are being rendered to.
	* @param InScene			The scene being viewed.
	* @param InShowFlags		The show flags for the views.
	*
	*/
	struct ConstructionValues
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ConstructionValues(ConstructionValues&&) = default;
		ConstructionValues(const ConstructionValues& Other) = default;
		ConstructionValues& operator=(ConstructionValues&&) = default;
		ConstructionValues& operator=(const ConstructionValues& Other) = default;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		ENGINE_API ConstructionValues(
			const FRenderTarget* InRenderTarget,
			FSceneInterface* InScene,
			const FEngineShowFlags& InEngineShowFlags
			);

		/** The views which make up the family. */
		const FRenderTarget* RenderTarget;

		/** The render target which the views are being rendered to. */
		FSceneInterface* Scene;

		/** The engine show flags for the views. */
		FEngineShowFlags EngineShowFlags;

		/** Additional view params related to the current viewmode (example : texcoord index) */
		int32 ViewModeParam;
		/** An name bound to the current viewmode param. (example : texture name) */
		FName ViewModeParamName;

		/** The current time. */
		FGameTime Time;

		/** Gamma correction used when rendering this family. Default is 1.0 */
		UE_DEPRECATED(5.4, "Unused gamma correction.")
		float GammaCorrection;

		/** Indicates whether the view family is additional. */
		uint32 bAdditionalViewFamily : 1;

		/** Indicates whether the view family is updated in real-time. */
		uint32 bRealtimeUpdate:1;
		
		/** Used to defer the back buffer clearing to just before the back buffer is drawn to */
		uint32 bDeferClear:1;
		
		/** If true then results of scene rendering are copied/resolved to the RenderTarget. */
		uint32 bResolveScene:1;		
		
		/** Safety check to ensure valid times are set either from a valid world/scene pointer or via the SetWorldTimes function */
		uint32 bTimesSet:1;

		/** True if scene color and depth should be multiview-allocated */
		uint32 bRequireMultiView:1;

		/** Set the world time and real time independently to handle time dilation. */
		ConstructionValues& SetTime(const FGameTime& InTime)
		{
			Time = InTime;
			bTimesSet = true;
			return *this;
		}

		UE_DEPRECATED(5.0, "Use FSceneViewFamily::ConstructionValues::SetTime()")
		ConstructionValues& SetWorldTimes(float InCurrentWorldTime, float InDeltaWorldTime, float InCurrentRealTime)
		{
			return SetTime(FGameTime::CreateDilated(InCurrentRealTime, /* InCurrentRealTime = */ InDeltaWorldTime, InCurrentWorldTime, InDeltaWorldTime));
		}

		/** Set  whether the view family is additional. */
		ConstructionValues& SetAdditionalViewFamily(const bool Value) { bAdditionalViewFamily = Value; return *this; }

		/** Set  whether the view family is updated in real-time. */
		ConstructionValues& SetRealtimeUpdate(const bool Value) { bRealtimeUpdate = Value; return *this; }
		
		/** Set whether to defer the back buffer clearing to just before the back buffer is drawn to */
		ConstructionValues& SetDeferClear(const bool Value) { bDeferClear = Value; return *this; }
		
		/** Setting to if true then results of scene rendering are copied/resolved to the RenderTarget. */
		ConstructionValues& SetResolveScene(const bool Value) { bResolveScene = Value; return *this; }

		/** Setting to true results in scene color and depth being multiview-allocated. */
		ConstructionValues& SetRequireMobileMultiView(const bool Value) { bRequireMultiView = Value; return *this; }
		
		/** Set Gamma correction used when rendering this family. */
		UE_DEPRECATED(5.4, "Unused gamma correction.")
		ConstructionValues& SetGammaCorrection(const float Value)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			GammaCorrection = Value; return *this;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		/** Set the view param. */
		ConstructionValues& SetViewModeParam(const int InViewModeParam, const FName& InViewModeParamName) { ViewModeParam = InViewModeParam; ViewModeParamName = InViewModeParamName; return *this; }		
	};
	
	/** The views which make up the family. */
	TArray<const FSceneView*> Views;

	/** All views include main camera views and scene capture views. */
	TArray<const FSceneView*> AllViews;

	/** View mode of the family. */
	EViewModeIndex ViewMode;

	/** The render target which the views are being rendered to. */
	const FRenderTarget* RenderTarget;

	/** The scene being viewed. */
	FSceneInterface* Scene;

	/** The new show flags for the views (meant to replace the old system). */
	FEngineShowFlags EngineShowFlags;

	/** The current time. */
	FGameTime Time;

	UE_DEPRECATED(5.0, "Use FSceneViewFamily::Time")
	float CurrentWorldTime;

	UE_DEPRECATED(5.0, "Use FSceneViewFamily::Time")
	float DeltaWorldTime;

	UE_DEPRECATED(5.0, "Use FSceneViewFamily::Time")
	float CurrentRealTime;

	/** Copy from main thread GFrameNumber to be accessible on render thread side. UINT_MAX before CreateSceneRenderer() or BeginRenderingViewFamily() was called */
	uint32 FrameNumber;

	/** Copy from main thread GFrameCounter to be accessible on render thread side. GFrameCounter is incremented once per engine tick, so multi views of the same frame have the same value. */
	uint64 FrameCounter = 0;

	// When deleted, remove PRAGMA_DISABLE_DEPRECATION_WARNINGS / PRAGMA_ENABLE_DEPRECATION_WARNINGS from ~FSceneViewFamily()
	UE_DEPRECATED(5.3, "Do not use, has no effect, and will be removed in a future release.")
	TOptional<uint64> OverrideFrameCounter;

	/** Indicates this view family is an additional one. */
	bool bAdditionalViewFamily;

	/** Indicates whether the view family is updated in realtime. */
	bool bRealtimeUpdate;

	/** Used to defer the back buffer clearing to just before the back buffer is drawn to */
	bool bDeferClear;

	/** if true then results of scene rendering are copied/resolved to the RenderTarget. */
	bool bResolveScene;

	/** if true then each view is not rendered using the same GPUMask. */
	bool bMultiGPUForkAndJoin;

#if WITH_MGPU
	/** Force a cross-GPU copy of all persistent resources after rendering this view family (except those marked MultiGPUGraphIgnore) */
	bool bForceCopyCrossGPU = false;
#endif

	/** Whether this view is one of multiple view families rendered in a single frame.  Affects occlusion query synchronization logic. */
	bool bIsMultipleViewFamily = false;
	
	/** 
	* Whether this view is the first of multiple view families rendered in a single frame.
	* Setting this correctly helps make better and more consistent streaming decisions.
	* This should be a temporary workaround until we have an API for executing code before/after all view rendering.
	*/
	bool bIsFirstViewInMultipleViewFamily = true;

	bool bIsSceneTexturesInitialized = false;
	bool bIsViewFamilyInfo = false;

	/** 
	 * Which component of the scene rendering should be output to the final render target.
	 * If SCS_FinalColorLDR this indicates do nothing.
	 */
	ESceneCaptureSource SceneCaptureSource;
	

	/** When enabled, the scene capture will composite into the render target instead of overwriting its contents. */
	ESceneCaptureCompositeMode SceneCaptureCompositeMode;

	/** Whether this view is for thumbnail rendering */
	bool bThumbnailRendering = false;

	/**
	 * GetWorld->IsPaused() && !Simulate
	 * Simulate is excluded as the camera can move which invalidates motionblur
	 */
	bool bWorldIsPaused;

	/** When enabled, the post processing will output in HDR space */
	bool bIsHDR;

	/** True if scenecolor and depth should be multiview-allocated */
	bool bRequireMultiView;

	/** Gamma correction used when rendering this family. Default is 1.0 */
	UE_DEPRECATED(5.4, "Unused gamma correction.")
	float GammaCorrection;
	
	/** DPI scale to be used for debugging font. */
	float DebugDPIScale = 1.0f;

	/** Editor setting to allow designers to override the automatic expose. 0:Automatic, following indices: -4 .. +4 */
	FExposureSettings ExposureSettings;

	/** Extensions that can modify view parameters on the render thread. */
	TArray<TSharedRef<class ISceneViewExtension, ESPMode::ThreadSafe> > ViewExtensions;

	// for r.DisplayInternals (allows for easy passing down data from main to render thread)
	FDisplayInternalsData DisplayInternalsData;

	/**
	 * Secondary view fraction to support High DPI monitor still with same primary screen percentage range for temporal
	 * upscale to test content consistently in editor no mater of the HighDPI scale. 
	 */
	float SecondaryViewFraction;
	ESecondaryScreenPercentageMethod SecondaryScreenPercentageMethod;

	// Override the LOD of landscape in this viewport
	int8 LandscapeLODOverride;

	/** Whether the scene is currently being edited, which can be used to speed up lighting propagation. */
	bool bCurrentlyBeingEdited;

	/** Whether to disable virtual texture update throttling. */
	bool bOverrideVirtualTextureThrottle;

	/** Size in pixels of a virtual texture feedback tile. */
	int32 VirtualTextureFeedbackFactor;

#if WITH_EDITOR
	/** Indicates whether, or not, the base attachment volume should be drawn. */
	bool bDrawBaseInfo;

	/**
	 * Indicates whether the shader world space position should be forced to 0. Also sets the view vector to (0,0,1) for all pixels.
	 * This is used in the texture streaming build when computing material tex coords scale.
	 * Because the material are rendered in tiles, there is no actual valid mapping for world space position.
	 * World space mapping would require to render mesh with the level transforms to be valid.
	 */
	bool bNullifyWorldSpacePosition;
#endif

	// Optional description of view family for Unreal Insights profiling
	FString ProfileDescription;

	/**
	 * Optional tracking of scene render time in seconds, useful when rendering multiple scenes (such as via UDisplayClusterViewportClient)
	 * where the user may want a live breakdown of where render time is going per scene, which the stat system otherwise doesn't provide.
	 * The destination memory location needs to be persistent and safe to access in any thread (for example, a standalone allocated buffer). 
	 */
	float* ProfileSceneRenderTime;

	/** Initialization constructor. */
	ENGINE_API FSceneViewFamily( const ConstructionValues& CVS );
	ENGINE_API virtual ~FSceneViewFamily();

	ENGINE_API ERHIFeatureLevel::Type GetFeatureLevel() const;

	EShaderPlatform GetShaderPlatform() const { return GShaderPlatformForFeatureLevel[GetFeatureLevel()]; }

#if WITH_DEBUG_VIEW_MODES
	EDebugViewShaderMode DebugViewShaderMode;
	int32 ViewModeParam;
	FName ViewModeParamName;

	bool bUsedDebugViewVSDSHS;
	FORCEINLINE EDebugViewShaderMode GetDebugViewShaderMode() const { return DebugViewShaderMode; }
	FORCEINLINE int32 GetViewModeParam() const { return ViewModeParam; }
	FORCEINLINE const FName& GetViewModeParamName() const { return ViewModeParamName; }
	ENGINE_API EDebugViewShaderMode ChooseDebugViewShaderMode() const;
	FORCEINLINE bool UseDebugViewVSDSHS() const { return bUsedDebugViewVSDSHS; }
	FORCEINLINE bool UseDebugViewPS() const { return DebugViewShaderMode != DVSM_None; }
#else
	FORCEINLINE EDebugViewShaderMode GetDebugViewShaderMode() const { return DVSM_None; }
	FORCEINLINE int32 GetViewModeParam() const { return -1; }
	FORCEINLINE FName GetViewModeParamName() const { return NAME_None; }
	FORCEINLINE bool UseDebugViewVSDSHS() const { return false; }
	FORCEINLINE bool UseDebugViewPS() const { return false; }
#endif

	/** Returns whether the screen percentage show flag is supported or not for this view family. */
	ENGINE_API bool SupportsScreenPercentage() const;

	FORCEINLINE bool AllowTranslucencyAfterDOF() const { return bAllowTranslucencyAfterDOF; }

	FORCEINLINE bool AllowStandardTranslucencySeparated() const { return bAllowStandardTranslucencySeparated; }

	FORCEINLINE const ISceneViewFamilyScreenPercentage* GetScreenPercentageInterface() const
	{
		return ScreenPercentageInterface;
	}

	/**
	 * Safely sets the view family's screen percentage interface.
	 * This is meant to be set by one of the ISceneViewExtension::BeginRenderViewFamily(). And collision will
	 * automatically be detected. If no extension sets it, that is fine since the renderer is going to use an
	 * internal default one.
	 *
	 * The renderer reserves the right to delete and replace the view family's screen percentage interface
	 * for testing purposes with the r.Test.OverrideScreenPercentageInterface CVar.
	 */
	FORCEINLINE void SetScreenPercentageInterface(ISceneViewFamilyScreenPercentage* InScreenPercentageInterface)
	{
		check(InScreenPercentageInterface);
		checkf(ScreenPercentageInterface == nullptr, TEXT("View family already had a screen percentage interface assigned."));
		ScreenPercentageInterface = InScreenPercentageInterface;
	}

	// View family assignment operator is not allowed because of ScreenPercentageInterface lifetime.
	void operator = (const FSceneViewFamily&) = delete;

	// Allow moving view family as long as no screen percentage interface are set.
	FSceneViewFamily(const FSceneViewFamily&& InViewFamily)
		: FSceneViewFamily(static_cast<const FSceneViewFamily&>(InViewFamily))
	{
		check(ScreenPercentageInterface == nullptr);
		check(TemporalUpscalerInterface == nullptr);
		check(PrimarySpatialUpscalerInterface == nullptr);
		check(SecondarySpatialUpscalerInterface == nullptr);
	}

	template<typename TExtensionData> TExtensionData* GetExtentionData()
	{
		static_assert(TIsDerivedFrom<TExtensionData, ISceneViewFamilyExtentionData>::Value, "TExtensionData is not derived from ISceneViewFamilyExtentionData.");

		for (TSharedRef<class ISceneViewFamilyExtentionData, ESPMode::ThreadSafe>& ViewExtensionData : ViewExtentionDatas)
		{
			if (ViewExtensionData->GetSubclassIdentifier() == TExtensionData::GSubclassIdentifier)
			{
				return static_cast<TExtensionData*>(&ViewExtensionData.Get());
			}
		}
		return nullptr;
	}

	template<typename TExtensionData> TExtensionData* GetOrCreateExtentionData()
	{
		TExtensionData* ViewExtensionData = GetExtentionData<TExtensionData>();
		if (!ViewExtensionData)
		{
			ViewExtentionDatas.Push(MakeShared<TExtensionData>());
			ViewExtensionData = static_cast<TExtensionData*>(&ViewExtentionDatas.Last().Get());
			check(ViewExtensionData->GetSubclassIdentifier() == TExtensionData::GSubclassIdentifier);
		}
		return ViewExtensionData;
	}

	FORCEINLINE void SetTemporalUpscalerInterface(UE::Renderer::Private::ITemporalUpscaler* InTemporalUpscalerInterface)
	{
		check(InTemporalUpscalerInterface);
		checkf(TemporalUpscalerInterface == nullptr, TEXT("View family already had a temporal upscaler assigned."));
		TemporalUpscalerInterface = InTemporalUpscalerInterface;
	}

	FORCEINLINE const UE::Renderer::Private::ITemporalUpscaler* GetTemporalUpscalerInterface() const
	{
		return TemporalUpscalerInterface;
	}

	FORCEINLINE void SetPrimarySpatialUpscalerInterface(ISpatialUpscaler* InSpatialUpscalerInterface)
	{
		check(InSpatialUpscalerInterface);
		checkf(PrimarySpatialUpscalerInterface == nullptr, TEXT("View family already had a primary spatial upscaler assigned."));
		PrimarySpatialUpscalerInterface = InSpatialUpscalerInterface;
	}

	FORCEINLINE const ISpatialUpscaler* GetPrimarySpatialUpscalerInterface() const
	{
		return PrimarySpatialUpscalerInterface;
	}

	FORCEINLINE void SetSecondarySpatialUpscalerInterface(ISpatialUpscaler* InSpatialUpscalerInterface)
	{
		check(InSpatialUpscalerInterface);
		checkf(SecondarySpatialUpscalerInterface == nullptr, TEXT("View family already had a secondary spatial upscaler assigned."));
		SecondarySpatialUpscalerInterface = InSpatialUpscalerInterface;
	}

	FORCEINLINE const ISpatialUpscaler* GetSecondarySpatialUpscalerInterface() const
	{
		return SecondarySpatialUpscalerInterface;
	}

	inline bool GetIsInFocus() const				{ return bIsInFocus; }
	inline void SetIsInFocus(bool bInIsInFocus)		{ bIsInFocus = bInIsInFocus; }

	void SetSceneRenderer(ISceneRenderer* NewSceneRenderer) { SceneRenderer = NewSceneRenderer; }
	ISceneRenderer* GetSceneRenderer() const { check(SceneRenderer); return SceneRenderer; }

private:
	/** The scene renderer that is rendering this view family. This is only initialized in the rendering thread's copies of the FSceneViewFamily. */
	ISceneRenderer* SceneRenderer;

	/** Interface to handle screen percentage of the views of the family. */
	ISceneViewFamilyScreenPercentage* ScreenPercentageInterface;

	/** Renderer private interfaces, automatically have same lifetime as FSceneViewFamily. */
	UE::Renderer::Private::ITemporalUpscaler* TemporalUpscalerInterface;
	ISpatialUpscaler* PrimarySpatialUpscalerInterface;
	ISpatialUpscaler* SecondarySpatialUpscalerInterface;

	/** Arrays of standalone data that have safe lifetime as FSceneViewFamily. */
	TArray<TSharedRef<class ISceneViewFamilyExtentionData, ESPMode::ThreadSafe> > ViewExtentionDatas;

	/** whether the translucency are allowed to render after DOF, if not they will be rendered in standard translucency. */
	bool bAllowTranslucencyAfterDOF = false;

	/** whether the pre DOF translucency are allowed to be rendered in separated target from scene to allow for better composition with distortion.*/
	bool bAllowStandardTranslucencySeparated = false;

	/** True if this view is the current editing view or the active game view */
	bool bIsInFocus = true;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Only FSceneRenderer can copy a view family.
	FSceneViewFamily(const FSceneViewFamily&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	friend class FSceneRenderer;
	friend class FViewFamilyInfo;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

/**
 * A view family which deletes its views when it goes out of scope.
 */
class FSceneViewFamilyContext : public FSceneViewFamily
{
public:
	/** Initialization constructor. */
	FSceneViewFamilyContext( const ConstructionValues& CVS)
		:	FSceneViewFamily(CVS)
	{}

	/** Destructor. */
	ENGINE_API virtual ~FSceneViewFamilyContext();
};
