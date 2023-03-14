// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDPrimConversion.h"

#include "UnrealUSDWrapper.h"
#include "USDAttributeUtils.h"
#include "USDConversionUtils.h"
#include "USDLayerUtils.h"
#include "USDLightConversion.h"
#include "USDLog.h"
#include "USDShadeConversion.h"
#include "USDSkeletalDataConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdGeomXformable.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "InstancedFoliageActor.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneColorSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"

#include "pxr/usd/sdf/changeBlock.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/timeCode.h"
#include "pxr/usd/usdGeom/camera.h"
#include "pxr/usd/usdGeom/imageable.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/pointInstancer.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/xformable.h"
#include "pxr/usd/usdGeom/xformCommonAPI.h"
#include "pxr/usd/usdLux/diskLight.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/lightAPI.h"
#include "pxr/usd/usdLux/rectLight.h"
#include "pxr/usd/usdLux/shapingAPI.h"
#include "pxr/usd/usdLux/sphereLight.h"
#include "pxr/usd/usdLux/tokens.h"
#include "pxr/usd/usdShade/connectableAPI.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "pxr/usd/usdShade/shader.h"
#include "pxr/usd/usdShade/tokens.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/root.h"

#include "USDIncludesEnd.h"

namespace UE
{
	namespace USDPrimConversionImpl
	{
		namespace Private
		{
			// Writes UEMaterialAssetPath as a material binding for MeshPrim, either by reusing a Material binding if
			// it already has an 'unreal' render context output and the expected structure, or by creating a new Material prim
			// that fulfills those requirements.
			// Doesn't write to 'unrealMaterial' at all, as we intend on deprecating it in the future.
			void AuthorMaterialOverride( pxr::UsdPrim& MeshPrim, const FString& UEMaterialAssetPath )
			{
				if ( !MeshPrim )
				{
					return;
				}

				FScopedUsdAllocs UsdAllocs;

				pxr::UsdShadeMaterialBindingAPI BindingAPI = pxr::UsdShadeMaterialBindingAPI::Apply( MeshPrim );

				// If this mesh prim already has a binding to a *child* material with the 'unreal' render context,
				// just write our material there and early out
				if ( pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial() )
				{
					// Only consider this material reusable if its within the same layer as the edit target, otherwise we'll
					// prefer to author something else that can be fully defined on the edit target. This is handy when we're
					// exporting a level, as we'll ensure we're making these prims on the MaterialOverrides layer
					pxr::SdfLayerRefPtr EditTarget = MeshPrim.GetStage()->GetEditTarget().GetLayer();
					pxr::SdfLayerRefPtr MaterialPrimLayer = static_cast< pxr::SdfLayerRefPtr >( UsdUtils::FindLayerForPrim( ShadeMaterial.GetPrim() ) );
					if ( EditTarget == MaterialPrimLayer )
					{
						// We need to try reusing these materials or else we'd write a new material prim every time we change
						// the override in UE, but we also run the risk of modifying a material that is used by multiple prims
						// (and here we just want to set the override for this Mesh prim). The compromise is to only reuse the
						// material if it is a child of MeshPrim already, and always to author our material prims as children
						std::string MaterialPath = ShadeMaterial.GetPrim().GetPath().GetString();
						std::string MeshPrimPath = MeshPrim.GetPath().GetString();
						if ( MaterialPath.rfind( MeshPrimPath, 0 ) == 0 )
						{
							if ( pxr::UsdPrim MaterialPrim = ShadeMaterial.GetPrim() )
							{
								UsdUtils::SetUnrealSurfaceOutput( MaterialPrim, UEMaterialAssetPath );
								return;
							}
						}
					}
				}

				// Find a unique name for our child material prim
				// Note how we'll always author these materials as children of the meshes themselves instead of emitting a common
				// Material prim to use for multiple overrides: This because in the future we'll want to have a separate material
				// bake for each mesh (to make sure we get vertex color effects, etc.), and so we'd have multiple baked .usda material
				// asset layers for each UE material, and we'd want each mesh/section/LOD to refer to its own anyway
				FString ChildMaterialName = TEXT( "UnrealMaterial" );
				if ( pxr::UsdPrim ExistingPrim = MeshPrim.GetChild( UnrealToUsd::ConvertToken( *ChildMaterialName ).Get() ) )
				{
					// Get a unique name for a new prim. Don't even try checking if this prim is usable as the material binding,
					// because if it was the material binding for this mesh we would have already used it above, when fetching the ExistingShader.
					// If we're here, we don't know what this prim is about
					TSet<FString> UsedNames;
					for ( pxr::UsdPrim Child : MeshPrim.GetFilteredChildren( pxr::UsdTraverseInstanceProxies( pxr::UsdPrimAllPrimsPredicate ) ) )
					{
						UsedNames.Add( UsdToUnreal::ConvertToken( Child.GetName() ) );
					}

					ChildMaterialName = UsdUtils::GetUniqueName( ChildMaterialName, UsedNames );
				}

				pxr::UsdStageRefPtr Stage = MeshPrim.GetStage();
				pxr::SdfPath MeshPath = MeshPrim.GetPath();
				pxr::SdfPath MaterialPath = MeshPath.AppendChild( UnrealToUsd::ConvertToken( *ChildMaterialName ).Get() );

				pxr::UsdShadeMaterial ChildMaterial = pxr::UsdShadeMaterial::Define( Stage, MaterialPath );
				if ( !ChildMaterial )
				{
					UE_LOG( LogUsd, Warning, TEXT( "Failed to author material prim '%s' when trying to write '%s's material override '%s' to USD" ),
						*UsdToUnreal::ConvertPath( MaterialPath ),
						*UsdToUnreal::ConvertPath( MeshPrim.GetPath() ),
						*UEMaterialAssetPath
					);
					return;
				}

				if ( pxr::UsdPrim MaterialPrim = ChildMaterial.GetPrim() )
				{
					UsdUtils::SetUnrealSurfaceOutput( MaterialPrim, UEMaterialAssetPath );

					BindingAPI.Bind( ChildMaterial );
				}
			}

			// On the current edit target, will set the Xformable's op order to a single "xformOp:transform",
			// create the corresponding attribute, and return the op
			pxr::UsdGeomXformOp ForceMatrixXform( pxr::UsdGeomXformable& Xformable )
			{
				FScopedUsdAllocs Allocs;

				// Note: We don't use Xformable.MakeMatrixXform() here because while it can clear the
				// xform op order on the current edit target just fine, it will later try to AddTransformOp(),
				// which calls AddXformOp. Internally, it will read the *composed* prim and if it finds that it already
				// has an op of that type it will early out and not author anything. This means that if our stage
				// has a strong opinion for an e.g. "xformOp:transform" already on the layer stack, it's not possible
				// to author that same op on a weaker layer. We want to do this here, to ensure this prim's transform
				// works as expected even if this weaker layer is used standalone, so we must do the analogous ourselves

				// References: private constructor for UsdGeomXformOp that can receive a UsdPrim and UsdGeomXformable::AddXformOp

				// Clear the existing xform op order for this prim on this layer
				Xformable.ClearXformOpOrder();

				// Find details about the transform attribute related to the default transform type xform op
				pxr::TfToken TransformAttrName = pxr::UsdGeomXformOp::GetOpName( pxr::UsdGeomXformOp::TypeTransform );
				const pxr::SdfValueTypeName& TransformAttrTypeName = pxr::UsdGeomXformOp::GetValueTypeName( pxr::UsdGeomXformOp::TypeTransform, pxr::UsdGeomXformOp::PrecisionDouble );
				if ( TransformAttrName.IsEmpty() || !TransformAttrTypeName )
				{
					return {};
				}

				// Create the transform attribute that would match the default transform type xform op
				const bool bCustom = false;
				pxr::UsdPrim UsdPrim = Xformable.GetPrim();
				pxr::UsdAttribute TransformAttr = UsdPrim.CreateAttribute( TransformAttrName, TransformAttrTypeName, bCustom );
				if ( !TransformAttr )
				{
					return {};
				}

				// Now that the attribute is created, use it to create the corresponding pxr::UsdGeomXformOp
				const bool bIsInverseOp = false;
				pxr::UsdGeomXformOp NewOp{ TransformAttr, bIsInverseOp };
				if ( !NewOp )
				{
					return {};
				}

				// Store the Op name on an array that will be our new op order value
				pxr::VtTokenArray NewOps;
				NewOps.push_back( NewOp.GetOpName() );
				Xformable.CreateXformOpOrderAttr().Set( NewOps );

				return NewOp;
			}

			// Turns OutTransform into the UE-space relative (local to parent) transform for Xformable, paying attention to if it
			// or any of its ancestors has the '!resetXformStack!' xformOp.
			void GetPrimConvertedRelativeTransform( pxr::UsdGeomXformable Xformable, double UsdTimeCode, FTransform& OutTransform, bool bIgnoreLocalTransform = false )
			{
				if ( !Xformable )
				{
					return;
				}

				FScopedUsdAllocs Allocs;

				pxr::UsdPrim UsdPrim = Xformable.GetPrim();
				pxr::UsdStageRefPtr UsdStage = UsdPrim.GetStage();

				bool bResetTransformStack = false;
				if( bIgnoreLocalTransform )
				{
					FTransform Dummy;
					UsdToUnreal::ConvertXformable( UsdStage, Xformable, Dummy, UsdTimeCode, &bResetTransformStack );

					OutTransform = FTransform::Identity;
				}
				else
				{
					UsdToUnreal::ConvertXformable( UsdStage, Xformable, OutTransform, UsdTimeCode, &bResetTransformStack );
				}

				// If we have the resetXformStack op on this prim's xformOpOrder we have to essentially use its transform
				// as the world transform (i.e. we have to discard the parent transforms). We won't do this here, and will instead
				// keep relative transforms everywhere for consistency, which means we must manually invert the ParentToWorld transform
				// and compute our relative transform ourselves.
				//
				// Ideally we'd query the components for this for performance reasons, but not only we don't have access to them here,
				// but neither the stage actor's PrimsToAnimate nor the sequencer guarantee a particular evaluation order anyway,
				// which means that if our parent is also animated, we could end up computing our relative transforms using the outdated
				// parent's transform instead. This means we must compute our relative transform using the actual prim hierarchy.
				//
				// Additionally, our parent prims may be animated, so we must query all of our ancestors for a new world matrix every frame.
				//
				// We could use UsdGeomXformCache for this, but given that we won't actually cache anything (since we'll have to resample
				// all ancestors every frame anyway) and that we would have to manually handle the camera/light compensation at least for
				// our immediate parent, it's simpler to just recursively call our own UsdToUnreal::ConvertXformable and concatenate the
				// results. Its not as fast, but we'll only do this on the initial read for prims with `resetXformStack`, so it should
				// be very rare. We don't ever write out the resetXformStack either, so after that initial read this op should just disappear.
				//
				// Note that, alternatively, we could also handle this whole situation by having the scene components specify their transforms
				// as absolute, and the Sequencer would work with that as well. However that would spread out the handling of
				// resetXformStack through all USD workflows, and mean we'd have to *write out* resetXformStack when writing/exporting
				// absolute transform components, and also convert between them when the user toggles between relative/absolute manually,
				// which is probably worse than just baking it as relative transforms on first read and forgetting about it.
				if ( bResetTransformStack )
				{
					FTransform ParentToWorld = FTransform::Identity;

					pxr::UsdPrim AncestorPrim = UsdPrim.GetParent();
					while ( AncestorPrim && !AncestorPrim.IsPseudoRoot() )
					{
						FTransform AncestorTransform = FTransform::Identity;
						bool bAncestorResetTransformStack = false;
						UsdToUnreal::ConvertXformable( UsdStage, pxr::UsdGeomXformable{ AncestorPrim }, AncestorTransform, UsdTimeCode, &bAncestorResetTransformStack );

						ParentToWorld = ParentToWorld * AncestorTransform;

						// If we find a parent that also has the resetXformStack, then we're in luck: That transform value will be its world
						// transform already, so we can stop concatenating stuff. Yes, on the component-side of things we'd have done the same
						// thing of making a fake relative transform for it, but the end result would have been the same final world transform
						if ( bAncestorResetTransformStack )
						{
							break;
						}

						AncestorPrim = AncestorPrim.GetParent();
					}

					const FVector& Scale = ParentToWorld.GetScale3D();
					if ( !FMath::IsNearlyEqual( Scale.X, Scale.Y ) || !FMath::IsNearlyEqual( Scale.X, Scale.Z ) )
					{
						UE_LOG( LogUsd, Warning, TEXT( "Inverting transform with non-uniform scaling '%s' when computing relative transform for prim '%s'! Result will likely be incorrect, since FTransforms can't invert non-uniform scalings. You can work around this by baking your non-uniform scaling transform into the vertices, or by not using the !resetXformStack! Xform op." ),
							*Scale.ToString(),
							*UsdToUnreal::ConvertPath( UsdPrim.GetPrimPath() )
						);
					}

					// Multiplying with matrices here helps mitigate the issues encountered with non-uniform scaling, however it will stil
					// never be perfect, as it is not possible to generate an FTransform that can properly invert a complex transform with non-uniform
					// scaling when just multiplying them (which is what downstream code within USceneComponent will do).
					OutTransform = FTransform{ OutTransform.ToMatrixWithScale() * ParentToWorld.ToInverseMatrixWithScale() };
				}
			}
		}
	}
}

bool UsdToUnreal::ConvertXformable( const pxr::UsdStageRefPtr& Stage, const pxr::UsdTyped& Schema, FTransform& OutTransform, double EvalTime, bool* bOutResetTransformStack )
{
	pxr::UsdGeomXformable Xformable( Schema );
	if ( !Xformable )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	// Transform
	pxr::GfMatrix4d UsdMatrix;
	bool bResetXformStack = false;
	bool* bResetXformStackPtr = bOutResetTransformStack ? bOutResetTransformStack : &bResetXformStack;
	Xformable.GetLocalTransformation( &UsdMatrix, bResetXformStackPtr, EvalTime );

	FUsdStageInfo StageInfo( Stage );
	OutTransform = UsdToUnreal::ConvertMatrix( StageInfo, UsdMatrix );

	const bool bPrimIsLight = Xformable.GetPrim().HasAPI< pxr::UsdLuxLightAPI >();

	// Extra rotation to match different camera facing direction convention
	// Note: The camera space is always Y-up, yes, but this is not what this is: This is the camera's transform wrt the stage,
	// which follows the stage up axis
	if ( Xformable.GetPrim().IsA< pxr::UsdGeomCamera >() || bPrimIsLight )
	{
		if ( StageInfo.UpAxis == EUsdUpAxis::YAxis )
		{
			OutTransform = FTransform( FRotator( 0.0f, -90.f, 0.0f ) ) * OutTransform;
		}
		else
		{
			OutTransform = FTransform( FRotator( -90.0f, -90.f, 0.0f ) ) * OutTransform;
		}
	}
	// Invert the compensation applied to our parents, in case they're a camera or a light
	if ( pxr::UsdPrim Parent = Xformable.GetPrim().GetParent() )
	{
		const bool bParentIsLight = Parent.HasAPI< pxr::UsdLuxLightAPI >();

		// If bResetXFormStack is true, then the prim's local transform will be used directly as the world transform, and we will
		// already invert the parent transform fully, regardless of what it is. This means it doesn't really matter if our parent
		// has a camera/light compensation or not, and so we don't have to have the explicit inverse compensation here anyway!
		if ( !(*bResetXformStackPtr) && ( Parent.IsA< pxr::UsdGeomCamera >() || bParentIsLight ) )
		{
			if ( StageInfo.UpAxis == EUsdUpAxis::YAxis )
			{
				OutTransform = OutTransform * FTransform( FRotator( 0.0f, -90.f, 0.0f ).GetInverse() );
			}
			else
			{
				OutTransform = OutTransform * FTransform( FRotator( -90.0f, -90.f, 0.0f ).GetInverse() );
			}
		}
	}

	return true;
}

bool UsdToUnreal::ConvertXformable( const pxr::UsdStageRefPtr& Stage, const pxr::UsdTyped& Schema, USceneComponent& SceneComponent, double EvalTime, bool bUsePrimTransform )
{
	pxr::UsdGeomXformable Xformable( Schema );
	if ( !Xformable )
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE( UsdToUnreal::ConvertXformable );

	FScopedUsdAllocs UsdAllocs;

	// Transform
	FTransform Transform;
	UE::USDPrimConversionImpl::Private::GetPrimConvertedRelativeTransform( Xformable, EvalTime, Transform, !bUsePrimTransform );
	SceneComponent.SetRelativeTransform( Transform );

	SceneComponent.Modify();

	// Computed (effective) visibility
	const bool bIsHidden = ( Xformable.ComputeVisibility( EvalTime ) == pxr::UsdGeomTokens->invisible );
	SceneComponent.SetHiddenInGame( bIsHidden );

	// Per-prim visibility
	bool bIsInvisible = false; // Default to 'inherited'
	if ( pxr::UsdAttribute VisibilityAttr = Xformable.GetVisibilityAttr() )
	{
		pxr::TfToken Value;
		if ( VisibilityAttr.Get( &Value, EvalTime ) )
		{
			bIsInvisible = Value == pxr::UsdGeomTokens->invisible;
		}
	}
	if ( bIsInvisible )
	{
		SceneComponent.ComponentTags.AddUnique( UnrealIdentifiers::Invisible );
		SceneComponent.ComponentTags.Remove( UnrealIdentifiers::Inherited );
	}
	else
	{
		SceneComponent.ComponentTags.Remove( UnrealIdentifiers::Invisible );
		SceneComponent.ComponentTags.AddUnique( UnrealIdentifiers::Inherited );
	}

	return true;
}

bool UsdToUnreal::ConvertXformable( const pxr::UsdTyped& Schema, UMovieScene3DTransformTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform )
{
	const UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter< UMovieScene >();

	if ( !MovieScene )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdGeomXformable Xformable( Schema );

	if ( !Xformable )
	{
		return false;
	}

	std::vector< double > UsdTimeSamples;
	Xformable.GetTimeSamples( &UsdTimeSamples );

	if ( UsdTimeSamples.empty() )
	{
		return false;
	}

	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	TArray< FFrameNumber > FrameNumbers;
	FrameNumbers.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > TranslationValuesX;
	TranslationValuesX.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > TranslationValuesY;
	TranslationValuesY.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > TranslationValuesZ;
	TranslationValuesZ.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > RotationValuesX;
	RotationValuesX.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > RotationValuesY;
	RotationValuesY.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > RotationValuesZ;
	RotationValuesZ.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > ScaleValuesX;
	ScaleValuesX.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > ScaleValuesY;
	ScaleValuesY.Reserve( UsdTimeSamples.size() );

	TArray< FMovieSceneDoubleValue > ScaleValuesZ;
	ScaleValuesZ.Reserve( UsdTimeSamples.size() );

	pxr::UsdStageRefPtr Stage = Schema.GetPrim().GetStage();
	const double StageTimeCodesPerSecond = Stage->GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	const ERichCurveInterpMode InterpMode = ( Stage->GetInterpolationType() == pxr::UsdInterpolationTypeLinear ) ? ERichCurveInterpMode::RCIM_Linear : ERichCurveInterpMode::RCIM_Constant;

	for ( double UsdTimeSample : UsdTimeSamples )
	{
		// Frame Number
		int32 FrameNumber = FMath::FloorToInt( UsdTimeSample );
		float SubFrameNumber = UsdTimeSample - FrameNumber;

		FFrameTime FrameTime( FrameNumber, SubFrameNumber );

		FFrameTime KeyFrameTime = FFrameRate::TransformTime( FrameTime, StageFrameRate, Resolution );
		KeyFrameTime *= SequenceTransform;
		FrameNumbers.Add( KeyFrameTime.GetFrame() );

		// Frame Values
		FTransform Transform;
		UsdToUnreal::ConvertXformable( Stage, Xformable, Transform, UsdTimeSample );

		// Location
		TranslationValuesX.Emplace_GetRef( Transform.GetLocation().X ).InterpMode = InterpMode;
		TranslationValuesY.Emplace_GetRef( Transform.GetLocation().Y ).InterpMode = InterpMode;
		TranslationValuesZ.Emplace_GetRef( Transform.GetLocation().Z ).InterpMode = InterpMode;

		// Rotation
		FRotator Rotator = Transform.Rotator();
		RotationValuesX.Emplace_GetRef( Rotator.Roll ).InterpMode = InterpMode;
		RotationValuesY.Emplace_GetRef( Rotator.Pitch ).InterpMode = InterpMode;
		RotationValuesZ.Emplace_GetRef( Rotator.Yaw ).InterpMode = InterpMode;

		// Scale
		ScaleValuesX.Emplace_GetRef( Transform.GetScale3D().X ).InterpMode = InterpMode;
		ScaleValuesY.Emplace_GetRef( Transform.GetScale3D().Y ).InterpMode = InterpMode;
		ScaleValuesZ.Emplace_GetRef( Transform.GetScale3D().Z ).InterpMode = InterpMode;
	}

	bool bSectionAdded = false;
	UMovieScene3DTransformSection* TransformSection = Cast< UMovieScene3DTransformSection >( MovieSceneTrack.FindOrAddSection( 0, bSectionAdded ) );
	TransformSection->EvalOptions.CompletionMode = EMovieSceneCompletionMode::KeepState;
	TransformSection->SetRange( TRange< FFrameNumber >::All() );

	TArrayView< FMovieSceneDoubleChannel* > Channels = TransformSection->GetChannelProxy().GetChannels< FMovieSceneDoubleChannel >();

	check( Channels.Num() >= 9 );

	// Translation
	Channels[0]->Set( FrameNumbers, TranslationValuesX );
	Channels[1]->Set( FrameNumbers, TranslationValuesY );
	Channels[2]->Set( FrameNumbers, TranslationValuesZ );

	// Rotation
	Channels[3]->Set( FrameNumbers, RotationValuesX );
	Channels[4]->Set( FrameNumbers, RotationValuesY );
	Channels[5]->Set( FrameNumbers, RotationValuesZ );

	// Scale
	Channels[6]->Set( FrameNumbers, ScaleValuesX );
	Channels[7]->Set( FrameNumbers, ScaleValuesY );
	Channels[8]->Set( FrameNumbers, ScaleValuesZ );

	return true;
}

bool UsdToUnreal::ConvertGeomCamera( const pxr::UsdStageRefPtr& Stage, const pxr::UsdGeomCamera& GeomCamera, UCineCameraComponent& CameraComponent, double EvalTime )
{
	return ConvertGeomCamera( UE::FUsdPrim{ GeomCamera.GetPrim() }, CameraComponent, EvalTime );
}

bool UsdToUnreal::ConvertGeomCamera( const UE::FUsdPrim& Prim, UCineCameraComponent& CameraComponent, double UsdTimeCode )
{
	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{ Prim };
	pxr::UsdGeomCamera GeomCamera{ UsdPrim };
	if ( !GeomCamera )
	{
		return false;
	}

	UE::FUsdStage Stage = Prim.GetStage();
	FUsdStageInfo StageInfo( Stage );

	CameraComponent.CurrentFocalLength = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( GeomCamera.GetFocalLengthAttr(), UsdTimeCode ) );

	CameraComponent.FocusSettings.ManualFocusDistance = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( GeomCamera.GetFocusDistanceAttr(), UsdTimeCode ) );

	if ( FMath::IsNearlyZero( CameraComponent.FocusSettings.ManualFocusDistance ) )
	{
		CameraComponent.FocusSettings.FocusMethod = ECameraFocusMethod::DoNotOverride;
	}

	CameraComponent.CurrentAperture = UsdUtils::GetUsdValue< float >( GeomCamera.GetFStopAttr(), UsdTimeCode );

	CameraComponent.Filmback.SensorWidth = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( GeomCamera.GetHorizontalApertureAttr(), UsdTimeCode ) );
	CameraComponent.Filmback.SensorHeight = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( GeomCamera.GetVerticalApertureAttr(), UsdTimeCode ) );

	return true;
}

bool UsdToUnreal::ConvertBoolTimeSamples( const UE::FUsdStage& Stage, const TArray<double>& UsdTimeSamples, const TFunction<bool( double )>& ReaderFunc, UMovieSceneBoolTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform )
{
	if ( !ReaderFunc )
	{
		return false;
	}

	const UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter< UMovieScene >();
	if ( !MovieScene )
	{
		return false;
	}

	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	FScopedUsdAllocs Allocs;

	pxr::UsdStageRefPtr UsdStage{ Stage };
	FUsdStageInfo StageInfo{ Stage };

	TArray< FFrameNumber > FrameNumbers;
	FrameNumbers.Reserve( UsdTimeSamples.Num() );

	TArray< bool > SectionValues;
	SectionValues.Reserve( UsdTimeSamples.Num() );

	const double StageTimeCodesPerSecond = UsdStage->GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	double LastTimeSample = TNumericLimits<double>::Lowest();
	for ( const double UsdTimeSample : UsdTimeSamples )
	{
		// We never want to evaluate the same time twice
		if ( FMath::IsNearlyEqual( UsdTimeSample, LastTimeSample ) )
		{
			continue;
		}
		LastTimeSample = UsdTimeSample;

		int32 FrameNumber = FMath::FloorToInt( UsdTimeSample );
		float SubFrameNumber = UsdTimeSample - FrameNumber;

		FFrameTime FrameTime( FrameNumber, SubFrameNumber );

		FFrameTime KeyFrameTime = FFrameRate::TransformTime( FrameTime, StageFrameRate, Resolution );
		KeyFrameTime *= SequenceTransform;
		FrameNumbers.Add( KeyFrameTime.GetFrame() );

		bool UEValue = ReaderFunc( UsdTimeSample );
		SectionValues.Emplace_GetRef( UEValue );
	}

	bool bSectionAdded = false;
	UMovieSceneBoolSection* Section = Cast< UMovieSceneBoolSection >( MovieSceneTrack.FindOrAddSection( 0, bSectionAdded ) );
	Section->EvalOptions.CompletionMode = EMovieSceneCompletionMode::KeepState;

	TMovieSceneChannelData<bool> Data = Section->GetChannel().GetData();
	Data.Reset();
	for ( int32 KeyIndex = 0; KeyIndex < FrameNumbers.Num(); ++KeyIndex )
	{
		Data.AddKey( FrameNumbers[ KeyIndex ], SectionValues[ KeyIndex ] );
	}

	Section->SetRange( Section->GetAutoSizeRange().Get( TRange<FFrameNumber>::Empty() ) );

	return true;
}

bool UsdToUnreal::ConvertFloatTimeSamples( const UE::FUsdStage& Stage, const TArray<double>& UsdTimeSamples, const TFunction<float( double )>& ReaderFunc, UMovieSceneFloatTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform )
{
	if ( !ReaderFunc )
	{
		return false;
	}

	const UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter< UMovieScene >();
	if ( !MovieScene )
	{
		return false;
	}

	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	FScopedUsdAllocs Allocs;

	pxr::UsdStageRefPtr UsdStage{ Stage };
	FUsdStageInfo StageInfo{ Stage };

	TArray< FFrameNumber > FrameNumbers;
	FrameNumbers.Reserve( UsdTimeSamples.Num() );

	TArray< FMovieSceneFloatValue > SectionValues;
	SectionValues.Reserve( UsdTimeSamples.Num() );

	const double StageTimeCodesPerSecond = UsdStage->GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	const ERichCurveInterpMode InterpMode = ( UsdStage->GetInterpolationType() == pxr::UsdInterpolationTypeLinear ) ? ERichCurveInterpMode::RCIM_Linear : ERichCurveInterpMode::RCIM_Constant;

	double LastTimeSample = TNumericLimits<double>::Lowest();
	for ( const double UsdTimeSample : UsdTimeSamples )
	{
		// We never want to evaluate the same time twice
		if ( FMath::IsNearlyEqual( UsdTimeSample, LastTimeSample ) )
		{
			continue;
		}
		LastTimeSample = UsdTimeSample;

		int32 FrameNumber = FMath::FloorToInt( UsdTimeSample );
		float SubFrameNumber = UsdTimeSample - FrameNumber;

		FFrameTime FrameTime( FrameNumber, SubFrameNumber );

		FFrameTime KeyFrameTime = FFrameRate::TransformTime( FrameTime, StageFrameRate, Resolution );
		KeyFrameTime *= SequenceTransform;
		FrameNumbers.Add( KeyFrameTime.GetFrame() );

		float UEValue = ReaderFunc( UsdTimeSample );
		SectionValues.Emplace_GetRef( UEValue ).InterpMode = InterpMode;
	}

	bool bSectionAdded = false;
	UMovieSceneFloatSection* Section = Cast< UMovieSceneFloatSection >( MovieSceneTrack.FindOrAddSection( 0, bSectionAdded ) );
	Section->EvalOptions.CompletionMode = EMovieSceneCompletionMode::KeepState;

	TArrayView< FMovieSceneFloatChannel* > Channels = Section->GetChannelProxy().GetChannels< FMovieSceneFloatChannel >();
	if ( Channels.Num() > 0 )
	{
		Channels[ 0 ]->Set( FrameNumbers, SectionValues );
	}

	Section->SetRange( Section->GetAutoSizeRange().Get( TRange<FFrameNumber>::Empty() ) );

	return true;
}

bool UsdToUnreal::ConvertColorTimeSamples( const UE::FUsdStage& Stage, const TArray<double>& UsdTimeSamples, const TFunction<FLinearColor( double )>& ReaderFunc, UMovieSceneColorTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform )
{
	if ( !ReaderFunc )
	{
		return false;
	}

	const UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter< UMovieScene >();
	if ( !MovieScene )
	{
		return false;
	}

	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	FScopedUsdAllocs Allocs;

	pxr::UsdStageRefPtr UsdStage{ Stage };
	FUsdStageInfo StageInfo{ Stage };

	TArray< FFrameNumber > FrameNumbers;
	FrameNumbers.Reserve( UsdTimeSamples.Num() );

	TArray< FMovieSceneFloatValue > RedValues;
	TArray< FMovieSceneFloatValue > GreenValues;
	TArray< FMovieSceneFloatValue > BlueValues;
	TArray< FMovieSceneFloatValue > AlphaValues;
	RedValues.Reserve( UsdTimeSamples.Num() );
	GreenValues.Reserve( UsdTimeSamples.Num() );
	BlueValues.Reserve( UsdTimeSamples.Num() );
	AlphaValues.Reserve( UsdTimeSamples.Num() );

	const double StageTimeCodesPerSecond = UsdStage->GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	const ERichCurveInterpMode InterpMode = ( UsdStage->GetInterpolationType() == pxr::UsdInterpolationTypeLinear ) ? ERichCurveInterpMode::RCIM_Linear : ERichCurveInterpMode::RCIM_Constant;

	double LastTimeSample = TNumericLimits<double>::Lowest();
	for ( const double UsdTimeSample : UsdTimeSamples )
	{
		// We never want to evaluate the same time twice
		if ( FMath::IsNearlyEqual( UsdTimeSample, LastTimeSample ) )
		{
			continue;
		}
		LastTimeSample = UsdTimeSample;

		int32 FrameNumber = FMath::FloorToInt( UsdTimeSample );
		float SubFrameNumber = UsdTimeSample - FrameNumber;

		FFrameTime FrameTime( FrameNumber, SubFrameNumber );

		FFrameTime KeyFrameTime = FFrameRate::TransformTime( FrameTime, StageFrameRate, Resolution );
		KeyFrameTime *= SequenceTransform;
		FrameNumbers.Add( KeyFrameTime.GetFrame() );

		FLinearColor UEValue = ReaderFunc( UsdTimeSample );
		RedValues.Emplace_GetRef( UEValue.R ).InterpMode = InterpMode;
		GreenValues.Emplace_GetRef( UEValue.G ).InterpMode = InterpMode;
		BlueValues.Emplace_GetRef( UEValue.B ).InterpMode = InterpMode;
		AlphaValues.Emplace_GetRef( UEValue.A ).InterpMode = InterpMode;
	}

	bool bSectionAdded = false;
	UMovieSceneColorSection* Section = Cast< UMovieSceneColorSection >( MovieSceneTrack.FindOrAddSection( 0, bSectionAdded ) );
	Section->EvalOptions.CompletionMode = EMovieSceneCompletionMode::KeepState;

	TArrayView< FMovieSceneFloatChannel* > Channels = Section->GetChannelProxy().GetChannels< FMovieSceneFloatChannel >();
	if ( Channels.Num() != 4 )
	{
		return false;
	}

	Channels[ 0 ]->Set( FrameNumbers, RedValues );
	Channels[ 1 ]->Set( FrameNumbers, GreenValues );
	Channels[ 2 ]->Set( FrameNumbers, BlueValues );
	Channels[ 3 ]->Set( FrameNumbers, AlphaValues );

	Section->SetRange( Section->GetAutoSizeRange().Get( TRange<FFrameNumber>::Empty() ) );

	return true;
}

bool UsdToUnreal::ConvertTransformTimeSamples( const UE::FUsdStage& Stage, const TArray<double>& UsdTimeSamples, const TFunction<FTransform( double )>& ReaderFunc, UMovieScene3DTransformTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform )
{
	if ( !ReaderFunc )
	{
		return false;
	}

	const UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter< UMovieScene >();
	if ( !MovieScene )
	{
		return false;
	}

	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	FScopedUsdAllocs Allocs;

	pxr::UsdStageRefPtr UsdStage{ Stage };
	FUsdStageInfo StageInfo{ Stage };

	TArray< FFrameNumber > FrameNumbers;
	FrameNumbers.Reserve( UsdTimeSamples.Num() );

	TArray< FMovieSceneDoubleValue > LocationXValues;
	TArray< FMovieSceneDoubleValue > LocationYValues;
	TArray< FMovieSceneDoubleValue > LocationZValues;

	TArray< FMovieSceneDoubleValue > RotationXValues;
	TArray< FMovieSceneDoubleValue > RotationYValues;
	TArray< FMovieSceneDoubleValue > RotationZValues;

	TArray< FMovieSceneDoubleValue > ScaleXValues;
	TArray< FMovieSceneDoubleValue > ScaleYValues;
	TArray< FMovieSceneDoubleValue > ScaleZValues;

	LocationXValues.Reserve( UsdTimeSamples.Num() );
	LocationYValues.Reserve( UsdTimeSamples.Num() );
	LocationZValues.Reserve( UsdTimeSamples.Num() );

	RotationXValues.Reserve( UsdTimeSamples.Num() );
	RotationYValues.Reserve( UsdTimeSamples.Num() );
	RotationZValues.Reserve( UsdTimeSamples.Num() );

	ScaleXValues.Reserve( UsdTimeSamples.Num() );
	ScaleYValues.Reserve( UsdTimeSamples.Num() );
	ScaleZValues.Reserve( UsdTimeSamples.Num() );

	const double StageTimeCodesPerSecond = UsdStage->GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	const ERichCurveInterpMode InterpMode = ( UsdStage->GetInterpolationType() == pxr::UsdInterpolationTypeLinear ) ? ERichCurveInterpMode::RCIM_Linear : ERichCurveInterpMode::RCIM_Constant;

	double LastTimeSample = TNumericLimits<double>::Lowest();
	for ( const double UsdTimeSample : UsdTimeSamples )
	{
		// We never want to evaluate the same time twice
		if ( FMath::IsNearlyEqual( UsdTimeSample, LastTimeSample ) )
		{
			continue;
		}
		LastTimeSample = UsdTimeSample;

		int32 FrameNumber = FMath::FloorToInt( UsdTimeSample );
		float SubFrameNumber = UsdTimeSample - FrameNumber;

		FFrameTime FrameTime( FrameNumber, SubFrameNumber );

		FFrameTime KeyFrameTime = FFrameRate::TransformTime( FrameTime, StageFrameRate, Resolution );
		KeyFrameTime *= SequenceTransform;
		FrameNumbers.Add( KeyFrameTime.GetFrame() );

		FTransform UEValue = ReaderFunc( UsdTimeSample );
		FVector Location = UEValue.GetLocation();
		FRotator Rotator = UEValue.Rotator();
		FVector Scale = UEValue.GetScale3D();

		LocationXValues.Emplace_GetRef( Location.X ).InterpMode = InterpMode;
		LocationYValues.Emplace_GetRef( Location.Y ).InterpMode = InterpMode;
		LocationZValues.Emplace_GetRef( Location.Z ).InterpMode = InterpMode;

		RotationXValues.Emplace_GetRef( Rotator.Roll ).InterpMode = InterpMode;
		RotationYValues.Emplace_GetRef( Rotator.Pitch ).InterpMode = InterpMode;
		RotationZValues.Emplace_GetRef( Rotator.Yaw ).InterpMode = InterpMode;

		ScaleXValues.Emplace_GetRef( Scale.X ).InterpMode = InterpMode;
		ScaleYValues.Emplace_GetRef( Scale.Y ).InterpMode = InterpMode;
		ScaleZValues.Emplace_GetRef( Scale.Z ).InterpMode = InterpMode;
	}

	bool bSectionAdded = false;
	UMovieScene3DTransformSection* Section = Cast< UMovieScene3DTransformSection >( MovieSceneTrack.FindOrAddSection( 0, bSectionAdded ) );
	Section->EvalOptions.CompletionMode = EMovieSceneCompletionMode::KeepState;
	Section->SetRange( TRange< FFrameNumber >::All() );

	TArrayView< FMovieSceneDoubleChannel* > Channels = Section->GetChannelProxy().GetChannels< FMovieSceneDoubleChannel >();
	if ( Channels.Num() < 9 )
	{
		return false;
	}

	Channels[ 0 ]->Set( FrameNumbers, LocationXValues );
	Channels[ 1 ]->Set( FrameNumbers, LocationYValues );
	Channels[ 2 ]->Set( FrameNumbers, LocationZValues );

	Channels[ 3 ]->Set( FrameNumbers, RotationXValues );
	Channels[ 4 ]->Set( FrameNumbers, RotationYValues );
	Channels[ 5 ]->Set( FrameNumbers, RotationZValues );

	Channels[ 6 ]->Set( FrameNumbers, ScaleXValues );
	Channels[ 7 ]->Set( FrameNumbers, ScaleYValues );
	Channels[ 8 ]->Set( FrameNumbers, ScaleZValues );

	return true;
}

UsdToUnreal::FPropertyTrackReader UsdToUnreal::CreatePropertyTrackReader( const UE::FUsdPrim& Prim, const FName& PropertyPath, bool bIgnorePrimLocalTransform )
{
	UsdToUnreal::FPropertyTrackReader Reader;

	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{ Prim };
	pxr::UsdStageRefPtr UsdStage = UsdPrim.GetStage();
	FUsdStageInfo StageInfo{ UsdStage };

	if ( pxr::UsdGeomXformable Xformable{ UsdPrim } )
	{
		if ( PropertyPath == UnrealIdentifiers::TransformPropertyName )
		{
			FTransform Default = FTransform::Identity;
			UE::USDPrimConversionImpl::Private::GetPrimConvertedRelativeTransform( Xformable, UsdUtils::GetDefaultTimeCode(), Default, bIgnorePrimLocalTransform );

			Reader.TransformReader = [UsdStage, Xformable, Default, bIgnorePrimLocalTransform]( double UsdTimeCode )
			{
				FTransform Result = Default;
				UE::USDPrimConversionImpl::Private::GetPrimConvertedRelativeTransform( Xformable, UsdTimeCode, Result, bIgnorePrimLocalTransform );
				return Result;
			};
			return Reader;
		}
	}

	if ( pxr::UsdGeomImageable Imageable{ UsdPrim } )
	{
		if ( PropertyPath == UnrealIdentifiers::HiddenInGamePropertyName )
		{
			if ( pxr::UsdAttribute Attr = Imageable.GetVisibilityAttr() )
			{
				pxr::TfToken Default = pxr::UsdGeomTokens->inherited;
				Attr.Get<pxr::TfToken>( &Default );

				Reader.BoolReader = [Imageable]( double UsdTimeCode )
				{
					// The property is "HiddenInGame" but it will end up in a visibility track, which is just a bool track,
					// where true means visible
					return Imageable.ComputeVisibility( UsdTimeCode ) == pxr::UsdGeomTokens->inherited;
				};
				return Reader;
			}
		}
	}

	if ( pxr::UsdGeomCamera Camera{ UsdPrim } )
	{
		bool bConvertDistance = true;
		pxr::UsdAttribute Attr;

		if ( PropertyPath == UnrealIdentifiers::CurrentFocalLengthPropertyName )
		{
			Attr = Camera.GetFocalLengthAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::ManualFocusDistancePropertyName )
		{
			Attr = Camera.GetFocusDistanceAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::CurrentAperturePropertyName )
		{
			bConvertDistance = false;
			Attr = Camera.GetFStopAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::SensorWidthPropertyName )
		{
			Attr = Camera.GetHorizontalApertureAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::SensorHeightPropertyName )
		{
			Attr = Camera.GetVerticalApertureAttr();
		}

		if ( Attr )
		{
			if ( bConvertDistance )
			{
				float Default = 0.0;
				Attr.Get<float>( &Default );
				Default = UsdToUnreal::ConvertDistance( StageInfo, Default );

				Reader.FloatReader = [Attr, Default, StageInfo]( double UsdTimeCode )
				{
					float Result = Default;
					if ( Attr.Get<float>( &Result, UsdTimeCode ) )
					{
						Result = UsdToUnreal::ConvertDistance( StageInfo, Result );
					}

					return Result;
				};
				return Reader;
			}
			else
			{
				float Default = 0.0;
				Attr.Get<float>( &Default );

				Reader.FloatReader = [Attr, Default]( double UsdTimeCode )
				{
					float Result = Default;
					Attr.Get<float>( &Result, UsdTimeCode );
					return Result;
				};
				return Reader;
			}
		}
	}
	else if ( const pxr::UsdLuxLightAPI LightAPI{ Prim } )
	{
		if ( PropertyPath == UnrealIdentifiers::LightColorPropertyName )
		{
			if ( pxr::UsdAttribute Attr = LightAPI.GetColorAttr() )
			{
				pxr::GfVec3f UsdDefault;
				Attr.Get<pxr::GfVec3f>( &UsdDefault );
				FLinearColor Default = UsdToUnreal::ConvertColor( UsdDefault );

				Reader.ColorReader = [Attr, Default]( double UsdTimeCode )
				{
					FLinearColor Result = Default;

					pxr::GfVec3f Value;
					if ( Attr.Get<pxr::GfVec3f>( &Value, UsdTimeCode ) )
					{
						Result = UsdToUnreal::ConvertColor( Value );
					}

					return Result;
				};
				return Reader;
			}
		}
		else if ( PropertyPath == UnrealIdentifiers::UseTemperaturePropertyName )
		{
			if ( pxr::UsdAttribute Attr = LightAPI.GetEnableColorTemperatureAttr() )
			{
				bool Default;
				Attr.Get<bool>( &Default );

				Reader.BoolReader = [Attr, Default]( double UsdTimeCode )
				{
					bool Result = Default;
					Attr.Get<bool>( &Result, UsdTimeCode );
					return Result;
				};
				return Reader;
			}
		}
		else if ( PropertyPath == UnrealIdentifiers::TemperaturePropertyName )
		{
			if ( pxr::UsdAttribute Attr = LightAPI.GetColorTemperatureAttr() )
			{
				float Default;
				Attr.Get<float>( &Default );

				Reader.FloatReader = [Attr, Default]( double UsdTimeCode )
				{
					float Result = Default;
					Attr.Get<float>( &Result, UsdTimeCode );
					return Result;
				};
				return Reader;
			}
		}

		else if ( pxr::UsdLuxSphereLight SphereLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::SourceRadiusPropertyName )
			{
				if ( pxr::UsdAttribute Attr = SphereLight.GetRadiusAttr() )
				{
					float Default = 0.0;
					Attr.Get<float>( &Default );
					Default = UsdToUnreal::ConvertDistance( StageInfo, Default );

					Reader.FloatReader = [Attr, Default, StageInfo]( double UsdTimeCode )
					{
						float Result = Default;
						if ( Attr.Get<float>( &Result, UsdTimeCode ) )
						{
							Result = UsdToUnreal::ConvertDistance( StageInfo, Result );
						}

						return Result;
					};
					return Reader;
				}
			}

			// Spot light
			else if ( UsdPrim.HasAPI< pxr::UsdLuxShapingAPI >() )
			{
				pxr::UsdLuxShapingAPI ShapingAPI{ UsdPrim };

				if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
				{
					pxr::UsdAttribute IntensityAttr = SphereLight.GetIntensityAttr();
					pxr::UsdAttribute ExposureAttr = SphereLight.GetExposureAttr();
					pxr::UsdAttribute RadiusAttr = SphereLight.GetRadiusAttr();
					pxr::UsdAttribute ConeAngleAttr = ShapingAPI.GetShapingConeAngleAttr();
					pxr::UsdAttribute ConeSoftnessAttr = ShapingAPI.GetShapingConeSoftnessAttr();

					if ( IntensityAttr && ExposureAttr && RadiusAttr && ConeAngleAttr && ConeSoftnessAttr )
					{
						// Default values directly from pxr/usd/usdLux/schema.usda
						float DefaultUsdIntensity = 1.0f;
						float DefaultUsdExposure = 0.0f;
						float DefaultUsdRadius = 0.5f;
						float DefaultUsdConeAngle = 90.0f;
						float DefaultUsdConeSoftness = 0.0f;

						IntensityAttr.Get<float>( &DefaultUsdIntensity );
						ExposureAttr.Get<float>( &DefaultUsdExposure );
						RadiusAttr.Get<float>( &DefaultUsdRadius );
						ConeAngleAttr.Get<float>( &DefaultUsdConeAngle );
						ConeSoftnessAttr.Get<float>( &DefaultUsdConeSoftness );

						float Default = UsdToUnreal::ConvertLuxShapingAPIIntensityAttr(
							DefaultUsdIntensity,
							DefaultUsdExposure,
							DefaultUsdRadius,
							DefaultUsdConeAngle,
							DefaultUsdConeSoftness,
							StageInfo
						);

						Reader.FloatReader = [IntensityAttr, ExposureAttr, RadiusAttr, ConeAngleAttr, ConeSoftnessAttr, Default, StageInfo]( double UsdTimeCode )
						{
							float Result = Default;

							float UsdIntensity = 1.0f;
							float UsdExposure = 0.0f;
							float UsdRadius = 0.5f;
							float UsdConeAngle = 90.0f;
							float UsdConeSoftness = 0.0f;
							if ( IntensityAttr.Get<float>( &UsdIntensity, UsdTimeCode ) &&
								 ExposureAttr.Get<float>( &UsdExposure, UsdTimeCode ) &&
								 RadiusAttr.Get<float>( &UsdRadius, UsdTimeCode ) &&
								 ConeAngleAttr.Get<float>( &UsdConeAngle, UsdTimeCode ) &&
								 ConeSoftnessAttr.Get<float>( &UsdConeSoftness, UsdTimeCode ) )
							{
								Result = UsdToUnreal::ConvertLuxShapingAPIIntensityAttr(
									UsdIntensity,
									UsdExposure,
									UsdRadius,
									UsdConeAngle,
									UsdConeSoftness,
									StageInfo
								);
							}

							return Result;
						};
						return Reader;
					}
				}
				else if ( PropertyPath == UnrealIdentifiers::OuterConeAnglePropertyName )
				{
					if ( pxr::UsdAttribute Attr = ShapingAPI.GetShapingConeAngleAttr() )
					{
						float Default;
						Attr.Get<float>( &Default );

						Reader.FloatReader = [Attr, Default]( double UsdTimeCode )
						{
							float Result = Default;
							Attr.Get<float>( &Result, UsdTimeCode );
							return Result;
						};
						return Reader;
					}
				}
				else if ( PropertyPath == UnrealIdentifiers::InnerConeAnglePropertyName )
				{
					pxr::UsdAttribute ConeAngleAttr = ShapingAPI.GetShapingConeAngleAttr();
					pxr::UsdAttribute ConeSoftnessAttr = ShapingAPI.GetShapingConeSoftnessAttr();

					if ( ConeAngleAttr && ConeSoftnessAttr )
					{
						// Default values directly from pxr/usd/usdLux/schema.usda
						float DefaultUsdConeAngle = 90.0f;
						float DefaultUsdConeSoftness = 0.0f;

						ConeAngleAttr.Get<float>( &DefaultUsdConeAngle );
						ConeSoftnessAttr.Get<float>( &DefaultUsdConeSoftness );

						float Default = 0.0f;
						UsdToUnreal::ConvertConeAngleSoftnessAttr( DefaultUsdConeAngle, DefaultUsdConeSoftness, Default );

						Reader.FloatReader = [ConeAngleAttr, ConeSoftnessAttr, Default]( double UsdTimeCode )
						{
							float Result = Default;

							float UsdConeAngle = 90.0f;
							float UsdConeSoftness = 0.0f;
							if ( ConeAngleAttr.Get<float>( &UsdConeAngle, UsdTimeCode ) && ConeSoftnessAttr.Get<float>( &UsdConeSoftness, UsdTimeCode ) )
							{
								UsdToUnreal::ConvertConeAngleSoftnessAttr( UsdConeAngle, UsdConeSoftness, Result );
							}

							return Result;
						};
						return Reader;
					}
				}
			}
			// Just a point light
			else
			{
				if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
				{
					pxr::UsdAttribute IntensityAttr = SphereLight.GetIntensityAttr();
					pxr::UsdAttribute ExposureAttr = SphereLight.GetExposureAttr();
					pxr::UsdAttribute RadiusAttr = SphereLight.GetRadiusAttr();

					if ( IntensityAttr && ExposureAttr && RadiusAttr )
					{
						// Default values directly from pxr/usd/usdLux/schema.usda
						float DefaultUsdIntensity = 1.0f;
						float DefaultUsdExposure = 0.0f;
						float DefaultUsdRadius = 0.5f;

						IntensityAttr.Get<float>( &DefaultUsdIntensity );
						ExposureAttr.Get<float>( &DefaultUsdExposure );
						RadiusAttr.Get<float>( &DefaultUsdRadius );

						float Default = UsdToUnreal::ConvertSphereLightIntensityAttr(
							DefaultUsdIntensity,
							DefaultUsdExposure,
							DefaultUsdRadius,
							StageInfo
						);

						Reader.FloatReader = [IntensityAttr, ExposureAttr, RadiusAttr, Default, StageInfo]( double UsdTimeCode )
						{
							float Result = Default;

							float UsdIntensity = 1.0f;
							float UsdExposure = 0.0f;
							float UsdRadius = 0.5f;
							if ( IntensityAttr.Get<float>( &UsdIntensity, UsdTimeCode ) &&
								 ExposureAttr.Get<float>( &UsdExposure, UsdTimeCode ) &&
								 RadiusAttr.Get<float>( &UsdRadius, UsdTimeCode ) )
							{
								Result = UsdToUnreal::ConvertSphereLightIntensityAttr(
									UsdIntensity,
									UsdExposure,
									UsdRadius,
									StageInfo
								);
							}

							return Result;
						};
						return Reader;
					}
				}
			}
		}
		else if ( pxr::UsdLuxRectLight RectLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::SourceWidthPropertyName )
			{
				if ( pxr::UsdAttribute Attr = RectLight.GetWidthAttr() )
				{
					float Default = 0.0;
					Attr.Get<float>( &Default );
					Default = UsdToUnreal::ConvertDistance( StageInfo, Default );

					Reader.FloatReader = [Attr, Default, StageInfo]( double UsdTimeCode )
					{
						float Result = Default;
						if ( Attr.Get<float>( &Result, UsdTimeCode ) )
						{
							Result = UsdToUnreal::ConvertDistance( StageInfo, Result );
						}

						return Result;
					};
					return Reader;
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::SourceHeightPropertyName )
			{
				if ( pxr::UsdAttribute Attr = RectLight.GetHeightAttr() )
				{
					float Default = 0.0;
					Attr.Get<float>( &Default );
					Default = UsdToUnreal::ConvertDistance( StageInfo, Default );

					Reader.FloatReader = [Attr, Default, StageInfo]( double UsdTimeCode )
					{
						float Result = Default;
						if ( Attr.Get<float>( &Result, UsdTimeCode ) )
						{
							Result = UsdToUnreal::ConvertDistance( StageInfo, Result );
						}

						return Result;
					};
					return Reader;
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
			{
				pxr::UsdAttribute IntensityAttr = RectLight.GetIntensityAttr();
				pxr::UsdAttribute ExposureAttr = RectLight.GetExposureAttr();
				pxr::UsdAttribute WidthAttr = RectLight.GetWidthAttr();
				pxr::UsdAttribute HeightAttr = RectLight.GetHeightAttr();

				if ( IntensityAttr && ExposureAttr && WidthAttr && HeightAttr )
				{
					// Default values directly from pxr/usd/usdLux/schema.usda
					float DefaultUsdIntensity = 1.0f;
					float DefaultUsdExposure = 0.0f;
					float DefaultUsdWidth = 1.0f;
					float DefaultUsdHeight = 1.0f;

					IntensityAttr.Get<float>( &DefaultUsdIntensity );
					ExposureAttr.Get<float>( &DefaultUsdExposure );
					WidthAttr.Get<float>( &DefaultUsdWidth );
					HeightAttr.Get<float>( &DefaultUsdHeight );

					float Default = UsdToUnreal::ConvertRectLightIntensityAttr(
						DefaultUsdIntensity,
						DefaultUsdExposure,
						DefaultUsdWidth,
						DefaultUsdHeight,
						StageInfo
					);

					Reader.FloatReader = [IntensityAttr, ExposureAttr, WidthAttr, HeightAttr, Default, StageInfo]( double UsdTimeCode )
					{
						float Result = Default;

						float UsdIntensity = 1.0f;
						float UsdExposure = 0.0f;
						float UsdWidth = 1.0f;
						float UsdHeight = 1.0f;
						if ( IntensityAttr.Get<float>( &UsdIntensity, UsdTimeCode ) &&
							 ExposureAttr.Get<float>( &UsdExposure, UsdTimeCode ) &&
							 WidthAttr.Get<float>( &UsdWidth, UsdTimeCode ) &&
							 HeightAttr.Get<float>( &UsdHeight, UsdTimeCode ) )
						{
							Result = UsdToUnreal::ConvertRectLightIntensityAttr(
								UsdIntensity,
								UsdExposure,
								UsdWidth,
								UsdHeight,
								StageInfo
							);
						}

						return Result;
					};
					return Reader;
				}
			}
		}
		else if ( pxr::UsdLuxDiskLight DiskLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::SourceWidthPropertyName || PropertyPath == UnrealIdentifiers::SourceHeightPropertyName )
			{
				if ( pxr::UsdAttribute Attr = DiskLight.GetRadiusAttr() )
				{
					// Our conversion is that Width == Height == 2 * Radius

					float Default = 0.0;
					Attr.Get<float>( &Default );
					Default = 2.0f * UsdToUnreal::ConvertDistance( StageInfo, Default );

					Reader.FloatReader = [Attr, Default, StageInfo]( double UsdTimeCode )
					{
						float Result = Default;
						if ( Attr.Get<float>( &Result, UsdTimeCode ) )
						{
							Result = 2.0f * UsdToUnreal::ConvertDistance( StageInfo, Result );
						}

						return Result;
					};
					return Reader;
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
			{
				pxr::UsdAttribute IntensityAttr = DiskLight.GetIntensityAttr();
				pxr::UsdAttribute ExposureAttr = DiskLight.GetExposureAttr();
				pxr::UsdAttribute RadiusAttr = DiskLight.GetRadiusAttr();

				if ( IntensityAttr && ExposureAttr && RadiusAttr )
				{
					// Default values directly from pxr/usd/usdLux/schema.usda
					float DefaultUsdIntensity = 1.0f;
					float DefaultUsdExposure = 0.0f;
					float DefaultUsdRadius = 0.5f;

					IntensityAttr.Get<float>( &DefaultUsdIntensity );
					ExposureAttr.Get<float>( &DefaultUsdExposure );
					RadiusAttr.Get<float>( &DefaultUsdRadius );

					float Default = UsdToUnreal::ConvertDiskLightIntensityAttr(
						DefaultUsdIntensity,
						DefaultUsdExposure,
						DefaultUsdRadius,
						StageInfo
					);

					Reader.FloatReader = [IntensityAttr, ExposureAttr, RadiusAttr, Default, StageInfo]( double UsdTimeCode )
					{
						float Result = Default;

						float UsdIntensity = 1.0f;
						float UsdExposure = 0.0f;
						float UsdRadius = 0.5f;
						if ( IntensityAttr.Get<float>( &UsdIntensity, UsdTimeCode ) &&
							 ExposureAttr.Get<float>( &UsdExposure, UsdTimeCode ) &&
							 RadiusAttr.Get<float>( &UsdRadius, UsdTimeCode ) )
						{
							Result = UsdToUnreal::ConvertDiskLightIntensityAttr(
								UsdIntensity,
								UsdExposure,
								UsdRadius,
								StageInfo
							);
						}

						return Result;
					};
					return Reader;
				}
			}
		}
		else if ( pxr::UsdLuxDistantLight DistantLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::LightSourceAnglePropertyName )
			{
				if ( pxr::UsdAttribute Attr = DistantLight.GetAngleAttr() )
				{
					float Default;
					Attr.Get<float>( &Default );

					Reader.FloatReader = [Attr, Default]( double UsdTimeCode )
					{
						float Result = Default;
						Attr.Get<float>( &Result, UsdTimeCode );
						return Result;
					};
					return Reader;
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
			{
				pxr::UsdAttribute IntensityAttr = SphereLight.GetIntensityAttr();
				pxr::UsdAttribute ExposureAttr = SphereLight.GetExposureAttr();

				if ( IntensityAttr && ExposureAttr )
				{
					// Default values directly from pxr/usd/usdLux/schema.usda
					float DefaultUsdIntensity = 1.0f;
					float DefaultUsdExposure = 0.0f;

					IntensityAttr.Get<float>( &DefaultUsdIntensity );
					ExposureAttr.Get<float>( &DefaultUsdExposure );

					float Default = UsdToUnreal::ConvertDistantLightIntensityAttr( DefaultUsdIntensity, DefaultUsdExposure );

					Reader.FloatReader = [IntensityAttr, ExposureAttr, Default, StageInfo]( double UsdTimeCode )
					{
						float Result = Default;

						float UsdIntensity = 1.0f;
						float UsdExposure = 0.0f;
						if ( IntensityAttr.Get<float>( &UsdIntensity, UsdTimeCode ) && ExposureAttr.Get<float>( &UsdExposure, UsdTimeCode ) )
						{
							Result = UsdToUnreal::ConvertDistantLightIntensityAttr( UsdIntensity, UsdExposure );
						}

						return Result;
					};
					return Reader;
				}
			}
		}
	}

	return Reader;
}

bool UnrealToUsd::ConvertCameraComponent( const UCineCameraComponent& CameraComponent, pxr::UsdPrim& Prim, double UsdTimeCode )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdGeomCamera GeomCamera{ Prim };
	if ( !GeomCamera )
	{
		return false;
	}

	FUsdStageInfo StageInfo( Prim.GetStage() );

	if ( pxr::UsdAttribute Attr = GeomCamera.CreateFocalLengthAttr() )
	{
		Attr.Set<float>( UnrealToUsd::ConvertDistance( StageInfo, CameraComponent.CurrentFocalLength ), UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( pxr::UsdAttribute Attr = GeomCamera.CreateFocusDistanceAttr() )
	{
		Attr.Set<float>( UnrealToUsd::ConvertDistance( StageInfo, CameraComponent.FocusSettings.ManualFocusDistance ), UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( pxr::UsdAttribute Attr = GeomCamera.CreateFStopAttr() )
	{
		Attr.Set<float>( CameraComponent.CurrentAperture, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( pxr::UsdAttribute Attr = GeomCamera.CreateHorizontalApertureAttr() )
	{
		Attr.Set<float>( UnrealToUsd::ConvertDistance( StageInfo, CameraComponent.Filmback.SensorWidth ), UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( pxr::UsdAttribute Attr = GeomCamera.CreateVerticalApertureAttr() )
	{
		Attr.Set<float>( UnrealToUsd::ConvertDistance( StageInfo, CameraComponent.Filmback.SensorHeight ), UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	return true;
}

bool UnrealToUsd::ConvertBoolTrack( const UMovieSceneBoolTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform, const TFunction<void( bool, double )>& WriterFunc, UE::FUsdPrim& Prim )
{
	if ( !WriterFunc || !Prim )
	{
		return false;
	}

	UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter<UMovieScene>();
	if ( !MovieScene )
	{
		return false;
	}

	UE::FUsdStage Stage = Prim.GetStage();

	const TRange< FFrameNumber > PlaybackRange = MovieScene->GetPlaybackRange();
	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	const double StageTimeCodesPerSecond = Stage.GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	auto EvaluateChannel = [&Resolution, &DisplayRate]( const FMovieSceneBoolChannel& Channel, bool InDefaultValue ) -> TArray< TPair< FFrameNumber, bool > >
	{
		TArray< TPair< FFrameNumber, bool > > Values;

		bool DefaultValue = Channel.GetDefault().Get( InDefaultValue );

		TArrayView<const FFrameNumber> KeyTimes = Channel.GetTimes();
		TArrayView<const bool> KeyValues = Channel.GetValues();

		for ( int32 KeyIndex = 0; KeyIndex < KeyTimes.Num(); ++KeyIndex )
		{
			const FFrameNumber KeyTime = KeyTimes[ KeyIndex ];
			bool KeyValue = KeyValues[ KeyIndex ];

			FFrameTime SnappedKeyTime{ FFrameRate::Snap( KeyTime, Resolution, DisplayRate ).FloorToFrame() };

			// We never need to bake bool tracks
			Values.Emplace( SnappedKeyTime.GetFrame(), KeyValue );
		}

		return Values;
	};

	for ( UMovieSceneSection* Section : MovieSceneTrack.GetAllSections() )
	{
		if ( UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>( Section ) )
		{
			for ( const TPair< FFrameNumber, bool >& Pair : EvaluateChannel( BoolSection->GetChannel(), false ) )
			{
				FFrameTime TransformedBakedKeyTime{ Pair.Key };
				TransformedBakedKeyTime *= SequenceTransform.InverseLinearOnly();
				FFrameTime UsdFrameTime = FFrameRate::TransformTime( TransformedBakedKeyTime, Resolution, StageFrameRate );

				bool UEValue = Pair.Value;

				WriterFunc( UEValue, UsdFrameTime.AsDecimal() );
			}
		}
	}

	return true;
}

bool UnrealToUsd::ConvertFloatTrack( const UMovieSceneFloatTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform, const TFunction<void( float, double )>& WriterFunc, UE::FUsdPrim& Prim )
{
	if ( !WriterFunc || !Prim )
	{
		return false;
	}

	UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter<UMovieScene>();
	if ( !MovieScene )
	{
		return false;
	}

	UE::FUsdStage Stage = Prim.GetStage();

	ERichCurveInterpMode StageInterpMode;
	{
		FScopedUsdAllocs Allocs;
		pxr::UsdStageRefPtr UsdStage{ Stage };
		StageInterpMode = ( UsdStage->GetInterpolationType() == pxr::UsdInterpolationTypeLinear ) ? ERichCurveInterpMode::RCIM_Linear : ERichCurveInterpMode::RCIM_Constant;
	}

	const TRange< FFrameNumber > PlaybackRange = MovieScene->GetPlaybackRange();
	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	const double StageTimeCodesPerSecond = Stage.GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	auto EvaluateChannel = [&Resolution, &DisplayRate, StageInterpMode]( const FMovieSceneFloatChannel& Channel, float InDefaultValue ) -> TArray< TPair< FFrameNumber, float > >
	{
		TArray< TPair< FFrameNumber, float > > Values;

		const FFrameTime BakeInterval = FFrameRate::TransformTime( 1, DisplayRate, Resolution );

		float DefaultValue = Channel.GetDefault().Get( InDefaultValue );

		TMovieSceneChannelData<const FMovieSceneFloatValue> ChannelData = Channel.GetData();
		TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();
		TArrayView<const FMovieSceneFloatValue> KeyValues = ChannelData.GetValues();

		for ( int32 KeyIndex = 0; KeyIndex < KeyTimes.Num(); ++KeyIndex )
		{
			const FFrameNumber KeyTime = KeyTimes[ KeyIndex ];
			const FMovieSceneFloatValue& KeyValue = KeyValues[ KeyIndex ];

			// If the channel has the same interpolation type as the stage (or we're the last key),
			// we don't need to bake anything: Just write out the keyframe as is
			if ( KeyValue.InterpMode == StageInterpMode || KeyIndex == ( KeyTimes.Num() - 1 ) )
			{
				FFrameTime SnappedKeyTime( FFrameRate::Snap( KeyTime, Resolution, DisplayRate ).FloorToFrame() );
				Values.Emplace( SnappedKeyTime.GetFrame(), KeyValue.Value );
			}
			// We need to bake: Start from this key up until the next key (non-inclusive). We always want to put a keyframe at
			// KeyTime, but then snap the other ones to the stage framerate
			else
			{
				// Don't use the snapped key time for the end of the bake range, because if the snapping moves it
				// later we may end up stepping back again when it's time to bake from that key onwards
				const FFrameNumber NextKey = KeyTimes[ KeyIndex + 1 ];
				const FFrameTime NextKeyTime{ NextKey };

				for ( FFrameTime EvalTime = KeyTime; EvalTime < NextKeyTime; EvalTime += BakeInterval )
				{
					FFrameNumber BakedKeyTime = FFrameRate::Snap( EvalTime, Resolution, DisplayRate ).FloorToFrame();

					float Value = DefaultValue;
					Channel.Evaluate( BakedKeyTime, Value );

					Values.Emplace( BakedKeyTime, Value );
				}
			}
		}

		return Values;
	};

	for ( UMovieSceneSection* Section : MovieSceneTrack.GetAllSections() )
	{
		if ( UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>( Section ) )
		{
			for ( const TPair< FFrameNumber, float >& Pair : EvaluateChannel( FloatSection->GetChannel(), 0.0f ) )
			{
				FFrameTime TransformedBakedKeyTime{ Pair.Key };
				TransformedBakedKeyTime *= SequenceTransform.InverseLinearOnly();
				FFrameTime UsdFrameTime = FFrameRate::TransformTime( TransformedBakedKeyTime, Resolution, StageFrameRate );

				float UEValue = Pair.Value;

				WriterFunc( UEValue, UsdFrameTime.AsDecimal() );
			}
		}
	}

	return true;
}

bool UnrealToUsd::ConvertColorTrack( const UMovieSceneColorTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform, const TFunction<void( const FLinearColor&, double )>& WriterFunc, UE::FUsdPrim& Prim )
{
	if ( !WriterFunc || !Prim )
	{
		return false;
	}

	UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter<UMovieScene>();
	if ( !MovieScene )
	{
		return false;
	}

	UE::FUsdStage Stage = Prim.GetStage();

	ERichCurveInterpMode StageInterpMode;
	{
		FScopedUsdAllocs Allocs;
		pxr::UsdStageRefPtr UsdStage{ Stage };
		StageInterpMode = ( UsdStage->GetInterpolationType() == pxr::UsdInterpolationTypeLinear ) ? ERichCurveInterpMode::RCIM_Linear : ERichCurveInterpMode::RCIM_Constant;
	}

	const TRange< FFrameNumber > PlaybackRange = MovieScene->GetPlaybackRange();
	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	const double StageTimeCodesPerSecond = Stage.GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	auto AppendChannelBakeTimes = [&Resolution, &DisplayRate, StageInterpMode]( const FMovieSceneFloatChannel& Channel, TSet<FFrameNumber>& OutBakeTimes )
	{
		const FFrameTime BakeInterval = FFrameRate::TransformTime( 1, DisplayRate, Resolution );

		TMovieSceneChannelData<const FMovieSceneFloatValue> ChannelData = Channel.GetData();
		TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();
		TArrayView<const FMovieSceneFloatValue> KeyValues = ChannelData.GetValues();

		for ( int32 KeyIndex = 0; KeyIndex < KeyTimes.Num(); ++KeyIndex )
		{
			const FFrameNumber KeyTime = KeyTimes[ KeyIndex ];
			const FMovieSceneFloatValue& KeyValue = KeyValues[ KeyIndex ];

			// If the channel has the same interpolation type as the stage (or we're the last key),
			// we don't need to bake anything: Just write out the keyframe as is
			if ( KeyValue.InterpMode == StageInterpMode || KeyIndex == ( KeyTimes.Num() - 1 ) )
			{
				FFrameNumber SnappedKeyTime = FFrameRate::Snap( KeyTime, Resolution, DisplayRate ).FloorToFrame();
				OutBakeTimes.Emplace( SnappedKeyTime );
			}
			// We need to bake: Start from this key up until the next key (non-inclusive). We always want to put a keyframe at
			// KeyTime, but then snap the other ones to the stage framerate
			else
			{
				// Don't use the snapped key time for the end of the bake range, because if the snapping moves it
				// later we may end up stepping back again when it's time to bake from that key onwards
				const FFrameNumber NextKey = KeyTimes[ KeyIndex + 1 ];
				const FFrameTime NextKeyTime{ NextKey };

				for ( FFrameTime EvalTime = KeyTime; EvalTime < NextKeyTime; EvalTime += BakeInterval )
				{
					FFrameNumber BakedKeyTime = FFrameRate::Snap( EvalTime, Resolution, DisplayRate ).FloorToFrame();
					OutBakeTimes.Emplace( BakedKeyTime );
				}
			}
		}
	};

	for ( UMovieSceneSection* Section : MovieSceneTrack.GetAllSections() )
	{
		if ( UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>( Section ) )
		{
			const FMovieSceneFloatChannel& RedChannel   = ColorSection->GetRedChannel();
			const FMovieSceneFloatChannel& GreenChannel = ColorSection->GetGreenChannel();
			const FMovieSceneFloatChannel& BlueChannel  = ColorSection->GetBlueChannel();
			const FMovieSceneFloatChannel& AlphaChannel = ColorSection->GetAlphaChannel();

			// Get the baked FFrameNumbers for each channel (without evaluating the channels yet),
			// because they may have independent keys
			TSet<FFrameNumber> ChannelBakeTimes;
			AppendChannelBakeTimes( RedChannel, ChannelBakeTimes );
			AppendChannelBakeTimes( GreenChannel, ChannelBakeTimes );
			AppendChannelBakeTimes( BlueChannel, ChannelBakeTimes );
			AppendChannelBakeTimes( AlphaChannel, ChannelBakeTimes );

			TArray<FFrameNumber> BakeTimeUnion = ChannelBakeTimes.Array();
			BakeTimeUnion.Sort();

			// Sample all channels at the union of bake times, construct the value and write it out
			for ( const FFrameNumber UntransformedBakeTime : BakeTimeUnion )
			{
				float RedValue = 0.0f;
				float GreenValue = 0.0f;
				float BlueValue = 0.0f;
				float AlphaValue = 1.0f;

				RedChannel.Evaluate( UntransformedBakeTime, RedValue );
				GreenChannel.Evaluate( UntransformedBakeTime, GreenValue );
				BlueChannel.Evaluate( UntransformedBakeTime, BlueValue );
				AlphaChannel.Evaluate( UntransformedBakeTime, AlphaValue );

				FLinearColor Color{ RedValue, GreenValue, BlueValue, AlphaValue };

				FFrameTime TransformedBakedKeyTime{ UntransformedBakeTime };
				TransformedBakedKeyTime *= SequenceTransform.InverseLinearOnly();
				FFrameTime UsdFrameTime = FFrameRate::TransformTime( TransformedBakedKeyTime, Resolution, StageFrameRate );

				WriterFunc( Color, UsdFrameTime.AsDecimal() );
			}
		}
	}

	return true;
}

bool UnrealToUsd::Convert3DTransformTrack( const UMovieScene3DTransformTrack& MovieSceneTrack, const FMovieSceneSequenceTransform& SequenceTransform, const TFunction<void( const FTransform&, double )>& WriterFunc, UE::FUsdPrim& Prim )
{
	if ( !WriterFunc || !Prim )
	{
		return false;
	}

	UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter<UMovieScene>();
	if ( !MovieScene )
	{
		return false;
	}

	UE::FUsdStage Stage = Prim.GetStage();

	ERichCurveInterpMode StageInterpMode;
	{
		FScopedUsdAllocs Allocs;
		pxr::UsdStageRefPtr UsdStage{ Stage };
		StageInterpMode = ( UsdStage->GetInterpolationType() == pxr::UsdInterpolationTypeLinear ) ? ERichCurveInterpMode::RCIM_Linear : ERichCurveInterpMode::RCIM_Constant;
	}

	const TRange< FFrameNumber > PlaybackRange = MovieScene->GetPlaybackRange();
	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	const double StageTimeCodesPerSecond = Stage.GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	auto EvaluateChannelTimes = [&Resolution, &DisplayRate, StageInterpMode]( const FMovieSceneDoubleChannel* Channel ) -> TSet<FFrameNumber>
	{
		TSet<FFrameNumber> BakeTimes;

		if ( !Channel )
		{
			return BakeTimes;
		}

		const FFrameTime BakeInterval = FFrameRate::TransformTime( 1, DisplayRate, Resolution );

		TMovieSceneChannelData<const FMovieSceneDoubleValue> ChannelData = Channel->GetData();
		TArrayView<const FFrameNumber> KeyTimes = ChannelData.GetTimes();
		TArrayView<const FMovieSceneDoubleValue> KeyValues = ChannelData.GetValues();

		for ( int32 KeyIndex = 0; KeyIndex < KeyTimes.Num(); ++KeyIndex )
		{
			const FFrameNumber KeyTime = KeyTimes[ KeyIndex ];
			const FMovieSceneDoubleValue& KeyValue = KeyValues[ KeyIndex ];

			// If the channel has the same interpolation type as the stage (or we're the last key),
			// we don't need to bake anything: Just write out the keyframe as is
			if ( KeyValue.InterpMode == StageInterpMode || KeyIndex == ( KeyTimes.Num() - 1 ) )
			{
				FFrameNumber SnappedKeyTime = FFrameRate::Snap( KeyTime, Resolution, DisplayRate ).FloorToFrame();
				BakeTimes.Emplace( SnappedKeyTime );
			}
			// We need to bake: Start from this key up until the next key (non-inclusive). We always want to put a keyframe at
			// KeyTime, but then snap the other ones to the stage framerate
			else
			{
				// Don't use the snapped key time for the end of the bake range, because if the snapping moves it
				// later we may end up stepping back again when it's time to bake from that key onwards
				const FFrameNumber NextKey = KeyTimes[ KeyIndex + 1 ];
				const FFrameTime NextKeyTime{ NextKey };

				for ( FFrameTime EvalTime = KeyTime; EvalTime < NextKeyTime; EvalTime += BakeInterval )
				{
					FFrameNumber BakedKeyTime = FFrameRate::Snap( EvalTime, Resolution, DisplayRate ).FloorToFrame();
					BakeTimes.Emplace( BakedKeyTime );
				}
			}
		}

		return BakeTimes;
	};

	for ( UMovieSceneSection* Section : MovieSceneTrack.GetAllSections() )
	{
		if ( UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>( Section ) )
		{
			TArrayView< FMovieSceneDoubleChannel* > Channels = TransformSection->GetChannelProxy().GetChannels< FMovieSceneDoubleChannel >();
			if ( Channels.Num() < 9 )
			{
				UE_LOG( LogUsd, Error, TEXT( "Unexpected number of double tracks (%d) in transform section '%s'" ), Channels.Num(), *TransformSection->GetPathName() );
				continue;
			}

			FMovieSceneDoubleChannel* LocationXChannel = Channels[ 0 ];
			FMovieSceneDoubleChannel* LocationYChannel = Channels[ 1 ];
			FMovieSceneDoubleChannel* LocationZChannel = Channels[ 2 ];

			FMovieSceneDoubleChannel* RotationXChannel = Channels[ 3 ];
			FMovieSceneDoubleChannel* RotationYChannel = Channels[ 4 ];
			FMovieSceneDoubleChannel* RotationZChannel = Channels[ 5 ];

			FMovieSceneDoubleChannel* ScaleXChannel = Channels[ 6 ];
			FMovieSceneDoubleChannel* ScaleYChannel = Channels[ 7 ];
			FMovieSceneDoubleChannel* ScaleZChannel = Channels[ 8 ];

			TSet< FFrameNumber > LocationValuesX = EvaluateChannelTimes( LocationXChannel );
			TSet< FFrameNumber > LocationValuesY = EvaluateChannelTimes( LocationYChannel );
			TSet< FFrameNumber > LocationValuesZ = EvaluateChannelTimes( LocationZChannel );

			TSet< FFrameNumber > RotationValuesX = EvaluateChannelTimes( RotationXChannel );
			TSet< FFrameNumber > RotationValuesY = EvaluateChannelTimes( RotationYChannel );
			TSet< FFrameNumber > RotationValuesZ = EvaluateChannelTimes( RotationZChannel );

			TSet< FFrameNumber > ScaleValuesX = EvaluateChannelTimes( ScaleXChannel );
			TSet< FFrameNumber > ScaleValuesY = EvaluateChannelTimes( ScaleYChannel );
			TSet< FFrameNumber > ScaleValuesZ = EvaluateChannelTimes( ScaleZChannel );

			LocationValuesX.Append( LocationValuesY );
			LocationValuesX.Append( LocationValuesZ );

			LocationValuesX.Append( RotationValuesX );
			LocationValuesX.Append( RotationValuesY );
			LocationValuesX.Append( RotationValuesZ );

			LocationValuesX.Append( ScaleValuesX );
			LocationValuesX.Append( ScaleValuesY );
			LocationValuesX.Append( ScaleValuesZ );

			TArray<FFrameNumber> BakeTimeUnion = LocationValuesX.Array();
			BakeTimeUnion.Sort();

			// Sample all channels at the union of bake times, construct the value and write it out
			for ( const FFrameNumber UntransformedBakeTime : BakeTimeUnion )
			{
				double LocX = 0.0f;
				double LocY = 0.0f;
				double LocZ = 0.0f;

				double RotX = 0.0f;
				double RotY = 0.0f;
				double RotZ = 0.0f;

				double ScaleX = 1.0f;
				double ScaleY = 1.0f;
				double ScaleZ = 1.0f;

				if ( LocationXChannel )
				{
					LocationXChannel->Evaluate( UntransformedBakeTime, LocX );
				}
				if ( LocationYChannel )
				{
					LocationYChannel->Evaluate( UntransformedBakeTime, LocY );
				}
				if ( LocationZChannel )
				{
					LocationZChannel->Evaluate( UntransformedBakeTime, LocZ );
				}

				if ( RotationXChannel )
				{
					RotationXChannel->Evaluate( UntransformedBakeTime, RotX );
				}
				if ( RotationYChannel )
				{
					RotationYChannel->Evaluate( UntransformedBakeTime, RotY );
				}
				if ( RotationZChannel )
				{
					RotationZChannel->Evaluate( UntransformedBakeTime, RotZ );
				}

				if ( ScaleXChannel )
				{
					ScaleXChannel->Evaluate( UntransformedBakeTime, ScaleX );
				}
				if ( ScaleYChannel )
				{
					ScaleYChannel->Evaluate( UntransformedBakeTime, ScaleY );
				}
				if ( ScaleZChannel )
				{
					ScaleZChannel->Evaluate( UntransformedBakeTime, ScaleZ );
				}

				// Casting this to float right now because depending on the build and the LWC status FVectors contain FLargeWorldCoordinatesReal,
				// which can be floats and turn these into narrowing conversions, which require explicit casts.
				// TODO: Replace these casts with the underlying FVector type later
				FVector Location{ ( float ) LocX, ( float ) LocY, ( float ) LocZ };
				FRotator Rotation{ ( float ) RotY, ( float ) RotZ, ( float ) RotX };
				FVector Scale{ ( float ) ScaleX, ( float ) ScaleY, ( float ) ScaleZ };
				FTransform Transform{ Rotation, Location, Scale };

				FFrameTime TransformedBakedKeyTime{ UntransformedBakeTime };
				TransformedBakedKeyTime *= SequenceTransform.InverseLinearOnly();
				FFrameTime UsdFrameTime = FFrameRate::TransformTime( TransformedBakedKeyTime, Resolution, StageFrameRate );

				WriterFunc( Transform, UsdFrameTime.AsDecimal() );
			}
		}
	}

	return true;
}

bool UnrealToUsd::ConvertSceneComponent( const pxr::UsdStageRefPtr& Stage, const USceneComponent* SceneComponent, pxr::UsdPrim& UsdPrim )
{
	if ( !UsdPrim || !SceneComponent )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	// Transform
	pxr::UsdGeomXformable XForm( UsdPrim );
	if ( !XForm )
	{
		return false;
	}

	FTransform RelativeTransform = SceneComponent->GetRelativeTransform();

	// Compensate different orientation for light or camera components:
	// In USD cameras shoot towards local - Z, with + Y up.Lights also emit towards local - Z, with + Y up
	// In UE cameras shoot towards local + X, with + Z up.Lights also emit towards local + X, with + Z up
	// Note that this wouldn't have worked in case we collapsed light and camera components, but these always get their own
	// actors, so we know that we don't have a single component that represents a large collapsed prim hierarchy
	if ( UsdPrim.IsA<pxr::UsdGeomCamera>() || UsdPrim.HasAPI<pxr::UsdLuxLightAPI>() )
	{
		FTransform AdditionalRotation = FTransform( FRotator( 0.0f, 90.f, 0.0f ) );

		if ( UsdUtils::GetUsdStageUpAxis( Stage ) == pxr::UsdGeomTokens->z )
		{
			AdditionalRotation *= FTransform( FRotator( 90.0f, 0.f, 0.0f ) );
		}

		RelativeTransform = AdditionalRotation * RelativeTransform;
	}

	// Invert compensation applied to parent if it's a light or camera component
	if ( pxr::UsdPrim ParentPrim = UsdPrim.GetParent() )
	{
		if ( ParentPrim.IsA<pxr::UsdGeomCamera>() || ParentPrim.HasAPI<pxr::UsdLuxLightAPI>() )
		{
			FTransform AdditionalRotation = FTransform( FRotator( 0.0f, 90.f, 0.0f ) );

			if ( UsdUtils::GetUsdStageUpAxis( Stage ) == pxr::UsdGeomTokens->z )
			{
				AdditionalRotation *= FTransform( FRotator( 90.0f, 0.f, 0.0f ) );
			}

			RelativeTransform = RelativeTransform * AdditionalRotation.Inverse();
		}
	}

	// Transform
	ConvertXformable( RelativeTransform, UsdPrim, UsdUtils::GetDefaultTimeCode() );

	// Per-prim visibility
	if ( pxr::UsdAttribute VisibilityAttr = XForm.CreateVisibilityAttr() )
	{
		pxr::TfToken Value = pxr::UsdGeomTokens->inherited;

		if ( SceneComponent->ComponentTags.Contains( UnrealIdentifiers::Invisible ) )
		{
			Value = pxr::UsdGeomTokens->invisible;
		}
		else if ( !SceneComponent->ComponentTags.Contains( UnrealIdentifiers::Inherited ) )
		{
			// We don't have visible nor inherited tags: We're probably exporting a pure UE component, so write out component visibility instead
			Value = SceneComponent->bHiddenInGame ? pxr::UsdGeomTokens->invisible : pxr::UsdGeomTokens->inherited;
		}

		VisibilityAttr.Set<pxr::TfToken>( Value );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ VisibilityAttr } );
	}

	return true;
}

bool UnrealToUsd::ConvertMeshComponent( const pxr::UsdStageRefPtr& Stage, const UMeshComponent* MeshComponent, pxr::UsdPrim& UsdPrim )
{
	if ( !UsdPrim || !MeshComponent )
	{
		return false;
	}

	FScopedUsdAllocs Allocs;

	if ( const UGeometryCacheComponent* GeometryCacheComponent = Cast<const UGeometryCacheComponent>( MeshComponent ) )
	{
		const TArray<UMaterialInterface*>& Overrides = MeshComponent->OverrideMaterials;
		for ( int32 MatIndex = 0; MatIndex < Overrides.Num(); ++MatIndex )
		{
			UMaterialInterface* Override = Overrides[ MatIndex ];
			if ( !Override )
			{
				continue;
			}

			pxr::SdfPath OverridePrimPath = UsdPrim.GetPath();

			pxr::UsdPrim MeshPrim = Stage->OverridePrim( OverridePrimPath );
			UE::USDPrimConversionImpl::Private::AuthorMaterialOverride( MeshPrim, Override->GetPathName() );
		}
	}
	else if ( const UStaticMeshComponent* StaticMeshComponent = Cast<const UStaticMeshComponent>( MeshComponent ) )
	{
#if WITH_EDITOR
		if ( UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh() )
		{
			int32 NumLODs = Mesh->GetNumLODs();
			const bool bHasLODs = NumLODs > 1;

			const TArray<UMaterialInterface*>& Overrides = MeshComponent->OverrideMaterials;
			for ( int32 MatIndex = 0; MatIndex < Overrides.Num(); ++MatIndex )
			{
				UMaterialInterface* Override = Overrides[MatIndex];
				if ( !Override )
				{
					continue;
				}

				for ( int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex )
				{
					int32 NumSections = Mesh->GetNumSections( LODIndex );
					const bool bHasSubsets = NumSections > 1;

					for ( int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex )
					{
						int32 SectionMatIndex = Mesh->GetSectionInfoMap().Get( LODIndex, SectionIndex ).MaterialIndex;
						if ( SectionMatIndex != MatIndex )
						{
							continue;
						}

						pxr::SdfPath OverridePrimPath = UsdPrim.GetPath();

						// If we have only 1 LOD, the asset's DefaultPrim will be the Mesh prim directly.
						// If we have multiple, the default prim won't have any schema, but will contain separate
						// Mesh prims for each LOD named "LOD0", "LOD1", etc., switched via a "LOD" variant set
						if ( bHasLODs )
						{
							OverridePrimPath = OverridePrimPath.AppendPath( UnrealToUsd::ConvertPath( *FString::Printf( TEXT( "LOD%d" ), LODIndex ) ).Get() );
						}

						// If our LOD has only one section, its material assignment will be authored directly on the Mesh prim.
						// If it has more than one material slot, we'll author UsdGeomSubset for each LOD Section, and author the material
						// assignment there
						if ( bHasSubsets )
						{
							// Assume the UE sections are in the same order as the USD ones
							std::vector<pxr::UsdGeomSubset> GeomSubsets = pxr::UsdShadeMaterialBindingAPI( UsdPrim ).GetMaterialBindSubsets();
							if ( SectionIndex < GeomSubsets.size() )
							{
								OverridePrimPath = OverridePrimPath.AppendChild( GeomSubsets[ SectionIndex ].GetPrim().GetName() );
							}
							else
							{
								OverridePrimPath = OverridePrimPath.AppendPath( UnrealToUsd::ConvertPath( *FString::Printf( TEXT( "Section%d" ), SectionIndex ) ).Get() );
							}
						}

						pxr::UsdPrim MeshPrim = Stage->OverridePrim( OverridePrimPath );
						UE::USDPrimConversionImpl::Private::AuthorMaterialOverride( MeshPrim, Override->GetPathName() );
					}
				}
			}
		}
#endif // WITH_EDITOR
	}
	else if ( const USkinnedMeshComponent* SkinnedMeshComponent = Cast<const USkinnedMeshComponent>( MeshComponent ) )
	{
		pxr::UsdSkelRoot SkelRoot{ UsdPrim };
		if ( !SkelRoot )
		{
			return false;
		}

		if ( const USkinnedAsset* SkinnedAsset = SkinnedMeshComponent->GetSkinnedAsset())
		{
			FSkeletalMeshRenderData* RenderData = SkinnedAsset->GetResourceForRendering();
			if ( !RenderData )
			{
				return false;
			}

			TIndirectArray<FSkeletalMeshLODRenderData>& LodRenderData = RenderData->LODRenderData;
			if ( LodRenderData.Num() == 0 )
			{
				return false;
			}

			int32 NumLODs = SkinnedAsset->GetLODNum();
			const bool bHasLODs = NumLODs > 1;

			FString MeshName;
			if ( !bHasLODs )
			{
				for ( const pxr::UsdPrim& Child : UsdPrim.GetChildren() )
				{
					if ( pxr::UsdGeomMesh Mesh{ Child } )
					{
						MeshName = UsdToUnreal::ConvertToken( Child.GetName() );
					}
				}
			}

			const TArray<UMaterialInterface*>& Overrides = MeshComponent->OverrideMaterials;
			for ( int32 MatIndex = 0; MatIndex < Overrides.Num(); ++MatIndex )
			{
				UMaterialInterface* Override = Overrides[ MatIndex ];
				if ( !Override )
				{
					continue;
				}

				for ( int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex )
				{
					if ( !LodRenderData.IsValidIndex( LODIndex ) )
					{
						continue;
					}

					const FSkeletalMeshLODInfo* LODInfo = SkinnedAsset->GetLODInfo( LODIndex );

					const TArray<FSkelMeshRenderSection>& Sections = LodRenderData[ LODIndex ].RenderSections;
					int32 NumSections = Sections.Num();
					const bool bHasSubsets = NumSections > 1;

					for ( int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex )
					{
						int32 SectionMatIndex = Sections[ SectionIndex ].MaterialIndex;

						// If we have a LODInfo map, we need to reroute the material index through it
						if ( LODInfo && LODInfo->LODMaterialMap.IsValidIndex( SectionIndex ) )
						{
							SectionMatIndex = LODInfo->LODMaterialMap[ SectionIndex ];
						}

						if ( SectionMatIndex != MatIndex )
						{
							continue;
						}

						pxr::SdfPath OverridePrimPath = UsdPrim.GetPath();

						// If we have only 1 LOD, the asset's DefaultPrim will be a SkelRoot, and the Mesh will be a subprim
						// with the same name.If we have the default prim is also the SkelRoot, but will contain separate
						// Mesh prims for each LOD named "LOD0", "LOD1", etc., switched via a "LOD" variant set
						if ( bHasLODs )
						{
							OverridePrimPath = OverridePrimPath.AppendPath( UnrealToUsd::ConvertPath( *FString::Printf( TEXT( "LOD%d" ), LODIndex ) ).Get() );
						}
						else
						{
							OverridePrimPath = OverridePrimPath.AppendElementString( UnrealToUsd::ConvertString( *MeshName ).Get() );
						}

						// If our LOD has only one section, its material assignment will be authored directly on the Mesh prim.
						// If it has more than one material slot, we'll author UsdGeomSubset for each LOD Section, and author the material
						// assignment there
						if ( bHasSubsets )
						{
							// Assume the UE sections are in the same order as the USD ones
							std::vector<pxr::UsdGeomSubset> GeomSubsets = pxr::UsdShadeMaterialBindingAPI( UsdPrim ).GetMaterialBindSubsets();
							if ( SectionIndex < GeomSubsets.size() )
							{
								OverridePrimPath = OverridePrimPath.AppendChild( GeomSubsets[ SectionIndex ].GetPrim().GetName() );
							}
							else
							{
								OverridePrimPath = OverridePrimPath.AppendPath( UnrealToUsd::ConvertPath( *FString::Printf( TEXT( "Section%d" ), SectionIndex ) ).Get() );
							}
						}

						pxr::UsdPrim MeshPrim = Stage->OverridePrim( OverridePrimPath );
						UE::USDPrimConversionImpl::Private::AuthorMaterialOverride( MeshPrim, Override->GetPathName() );
					}
				}
			}
		}
	}

	return true;
}

bool UnrealToUsd::ConvertHierarchicalInstancedStaticMeshComponent( const UHierarchicalInstancedStaticMeshComponent* HISMComponent, pxr::UsdPrim& UsdPrim, double TimeCode )
{
	using namespace pxr;

	FScopedUsdAllocs Allocs;

	UsdGeomPointInstancer PointInstancer{ UsdPrim };
	if ( !PointInstancer || !HISMComponent )
	{
		return false;
	}

	UsdStageRefPtr Stage = UsdPrim.GetStage();
	FUsdStageInfo StageInfo{ Stage };

	VtArray<int> ProtoIndices;
	VtArray<GfVec3f> Positions;
	VtArray<GfQuath> Orientations;
	VtArray<GfVec3f> Scales;

	const int32 NumInstances = HISMComponent->GetInstanceCount();
	ProtoIndices.reserve( ProtoIndices.size() + NumInstances );
	Positions.reserve( Positions.size() + NumInstances );
	Orientations.reserve( Orientations.size() + NumInstances );
	Scales.reserve( Scales.size() + NumInstances );

	for( const FInstancedStaticMeshInstanceData& InstanceData : HISMComponent->PerInstanceSMData )
	{
		// Convert axes
		FTransform UETransform{ InstanceData.Transform };
		FTransform USDTransform = UsdUtils::ConvertAxes( StageInfo.UpAxis == EUsdUpAxis::ZAxis, UETransform );

		FVector Translation = USDTransform.GetTranslation();
		FQuat Rotation = USDTransform.GetRotation();
		FVector Scale = USDTransform.GetScale3D();

		// Compensate metersPerUnit
		const float UEMetersPerUnit = 0.01f;
		if ( !FMath::IsNearlyEqual( UEMetersPerUnit, StageInfo.MetersPerUnit ) )
		{
			Translation *= ( UEMetersPerUnit / StageInfo.MetersPerUnit );
		}

		ProtoIndices.push_back( 0 ); // We will always export a single prototype per PointInstancer, since HISM components handle only 1 mesh at a time
		Positions.push_back( GfVec3f( Translation.X, Translation.Y, Translation.Z ) );
		Orientations.push_back( GfQuath( Rotation.W, Rotation.X, Rotation.Y, Rotation.Z ) );
		Scales.push_back( GfVec3f( Scale.X, Scale.Y, Scale.Z ) );
	}

	const pxr::UsdTimeCode UsdTimeCode( TimeCode );

	if ( UsdAttribute Attr = PointInstancer.CreateProtoIndicesAttr() )
	{
		Attr.Set( ProtoIndices, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( UsdAttribute Attr = PointInstancer.CreatePositionsAttr() )
	{
		Attr.Set( Positions, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( UsdAttribute Attr = PointInstancer.CreateOrientationsAttr() )
	{
		Attr.Set( Orientations, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( UsdAttribute Attr = PointInstancer.CreateScalesAttr() )
	{
		Attr.Set( Scales, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	return true;
}

bool UnrealToUsd::ConvertCameraComponent( const pxr::UsdStageRefPtr& Stage, const UCineCameraComponent* CameraComponent, pxr::UsdPrim& UsdPrim, double TimeCode )
{
	if ( CameraComponent )
	{
		return ConvertCameraComponent( *CameraComponent, UE::FUsdPrim{ UsdPrim }, TimeCode );
	}

	return false;
}

bool UnrealToUsd::ConvertXformable( const FTransform& RelativeTransform, pxr::UsdPrim& UsdPrim, double TimeCode )
{
	if ( !UsdPrim )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	// Transform
	pxr::UsdGeomXformable XForm( UsdPrim );
	if ( !XForm )
	{
		return false;
	}

	FUsdStageInfo StageInfo( UsdPrim.GetStage() );
	pxr::GfMatrix4d UsdTransform = UnrealToUsd::ConvertTransform( StageInfo, RelativeTransform );

	const pxr::UsdTimeCode UsdTimeCode( TimeCode );

	if ( pxr::UsdGeomXformOp MatrixXform = UE::USDPrimConversionImpl::Private::ForceMatrixXform( XForm ) )
	{
		MatrixXform.Set( UsdTransform, UsdTimeCode );

		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ MatrixXform.GetAttr() } );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ XForm.GetXformOpOrderAttr() } );
	}

	return true;
}

#if WITH_EDITOR
namespace UE
{
	namespace USDPrimConversionImpl
	{
		namespace Private
		{
			void ConvertFoliageInstances(
				const FFoliageInfo& Info,
				const TSet<int32>& UEInstances,
				const FTransform& UEWorldToFoliageActor,
				const FUsdStageInfo& StageInfo,
				int PrototypeIndex,
				pxr::VtArray<int>& ProtoIndices,
				pxr::VtArray<pxr::GfVec3f>& Positions,
				pxr::VtArray<pxr::GfQuath>& Orientations,
				pxr::VtArray<pxr::GfVec3f>& Scales
			)
			{
				FScopedUsdAllocs Allocs;

				const int32 NumInstances = UEInstances.Num();

				ProtoIndices.reserve( ProtoIndices.size() + NumInstances );
				Positions.reserve( Positions.size() + NumInstances );
				Orientations.reserve( Orientations.size() + NumInstances );
				Scales.reserve( Scales.size() + NumInstances );

				for ( int32 InstanceIndex : UEInstances )
				{
					const FFoliageInstancePlacementInfo* Instance = &Info.Instances[ InstanceIndex ];

					// Convert axes
					FTransform UEWorldTransform{ Instance->Rotation, (FVector)Instance->Location, (FVector)Instance->DrawScale3D };
					FTransform USDTransform = UsdUtils::ConvertAxes( StageInfo.UpAxis == EUsdUpAxis::ZAxis, UEWorldTransform * UEWorldToFoliageActor );

					FVector Translation = USDTransform.GetTranslation();
					FQuat Rotation = USDTransform.GetRotation();
					FVector Scale = USDTransform.GetScale3D();

					// Compensate metersPerUnit
					const float UEMetersPerUnit = 0.01f;
					if ( !FMath::IsNearlyEqual( UEMetersPerUnit, StageInfo.MetersPerUnit ) )
					{
						Translation *= ( UEMetersPerUnit / StageInfo.MetersPerUnit );
					}

					ProtoIndices.push_back( PrototypeIndex );
					Positions.push_back( pxr::GfVec3f( Translation.X, Translation.Y, Translation.Z ) );
					Orientations.push_back( pxr::GfQuath( Rotation.W, Rotation.X, Rotation.Y, Rotation.Z ) );
					Scales.push_back( pxr::GfVec3f( Scale.X, Scale.Y, Scale.Z ) );
				}
			}
		}
	}
}
#endif // WITH_EDITOR

bool UnrealToUsd::ConvertInstancedFoliageActor( const AInstancedFoliageActor& Actor, pxr::UsdPrim& UsdPrim, double TimeCode, ULevel* InstancesLevel )
{
#if WITH_EDITOR
	using namespace pxr;

	FScopedUsdAllocs Allocs;

	UsdGeomPointInstancer PointInstancer{ UsdPrim };
	if ( !PointInstancer )
	{
		return false;
	}

	UsdStageRefPtr Stage = UsdPrim.GetStage();
	FUsdStageInfo StageInfo{ Stage };

	VtArray<int> ProtoIndices;
	VtArray<GfVec3f> Positions;
	VtArray<GfQuath> Orientations;
	VtArray<GfVec3f> Scales;

	TSet<FFoliageInstanceBaseId> HandledComponents;

	// It seems like the foliage instance transforms are actually world transforms, so to get them into the coordinate space of the generated
	// point instancer, we'll have to concatenate with the inverse the foliage actor's ActorToWorld transform
	FTransform UEWorldToFoliageActor = Actor.GetTransform().Inverse();

	int PrototypeIndex = 0;
	for ( const TPair<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliagePair : Actor.GetFoliageInfos() )
	{
		const UFoliageType* FoliageType = FoliagePair.Key;
		const FFoliageInfo& Info = FoliagePair.Value.Get();

		// Traverse valid foliage instances: Those that are being tracked to belonging to a particular component
		for ( const TPair<FFoliageInstanceBaseId, FFoliageInstanceBaseInfo>& FoliageInstancePair : Actor.InstanceBaseCache.InstanceBaseMap )
		{
			const FFoliageInstanceBaseId& ComponentId = FoliageInstancePair.Key;
			HandledComponents.Add( ComponentId );

			UActorComponent* Comp = FoliageInstancePair.Value.BasePtr.Get();
			if ( !Comp || ( InstancesLevel && ( Comp->GetComponentLevel() != InstancesLevel ) ) )
			{
				continue;
			}

			if ( const TSet<int32>* InstanceSet = Info.ComponentHash.Find( ComponentId ) )
			{
				UE::USDPrimConversionImpl::Private::ConvertFoliageInstances(
					Info,
					*InstanceSet,
					UEWorldToFoliageActor,
					StageInfo,
					PrototypeIndex,
					ProtoIndices,
					Positions,
					Orientations,
					Scales
				);
			}
		}

		// Do another pass to grab invalid foliage instances (not assigned to any particular component)
		// Only export these when we're not given a particular level to export, or if that level is the actor's level (essentially
		// pretending the invalid instances belong to the actor's level). This mostly helps prevent it from exporting the invalid instances
		// multiple times in case we're calling this function repeatedly for each individual sublevel
		if ( !InstancesLevel || InstancesLevel == Actor.GetLevel() )
		{
			for ( const TPair<FFoliageInstanceBaseId, TSet<int32>>& Pair : Info.ComponentHash )
			{
				const FFoliageInstanceBaseId& ComponentId = Pair.Key;
				if ( HandledComponents.Contains( ComponentId ) )
				{
					continue;
				}

				const TSet<int32>& InstanceSet = Pair.Value;
				UE::USDPrimConversionImpl::Private::ConvertFoliageInstances(
					Info,
					InstanceSet,
					UEWorldToFoliageActor,
					StageInfo,
					PrototypeIndex,
					ProtoIndices,
					Positions,
					Orientations,
					Scales
				);
			}
		}

		++PrototypeIndex;
	}

	const pxr::UsdTimeCode UsdTimeCode( TimeCode );

	if ( UsdAttribute Attr = PointInstancer.CreateProtoIndicesAttr() )
	{
		Attr.Set( ProtoIndices, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( UsdAttribute Attr = PointInstancer.CreatePositionsAttr() )
	{
		Attr.Set( Positions, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( UsdAttribute Attr = PointInstancer.CreateOrientationsAttr() )
	{
		Attr.Set( Orientations, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	if ( UsdAttribute Attr = PointInstancer.CreateScalesAttr() )
	{
		Attr.Set( Scales, UsdTimeCode );
		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	return true;
#else
	return false;
#endif // WITH_EDITOR
}

bool UnrealToUsd::CreateComponentPropertyBaker( UE::FUsdPrim& Prim, const USceneComponent& Component, const FString& PropertyPath, FComponentBaker& OutBaker )
{
	EBakingType BakerType = EBakingType::None;
	TFunction<void(double)> BakerFunction;

	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{ Prim };
	pxr::UsdStageRefPtr UsdStage = UsdPrim.GetStage();
	FUsdStageInfo StageInfo{ UsdStage };

	// SceneComponent
	{
		if ( PropertyPath == TEXT( "Transform" ) )
		{
			pxr::UsdGeomXformable Xformable( UsdPrim );
			if ( !Xformable )
			{
				return false;
			}

			Xformable.CreateXformOpOrderAttr();

			// Clear existing transform data and leave just one Transform op there
			pxr::UsdGeomXformOp TransformOp = UE::USDPrimConversionImpl::Private::ForceMatrixXform( Xformable );
			if ( !TransformOp )
			{
				return false;
			}

			pxr::UsdAttribute Attr = TransformOp.GetAttr();
			if ( !Attr )
			{
				return false;
			}

			Attr.Clear();

			// Compensate different orientation for light or camera components
			FTransform AdditionalRotation = FTransform::Identity;
			if ( UsdPrim.IsA< pxr::UsdGeomCamera >() || UsdPrim.HasAPI< pxr::UsdLuxLightAPI >() )
			{
				AdditionalRotation = FTransform( FRotator( 0.0f, 90.0f, 0.0f ) );

				if ( StageInfo.UpAxis == EUsdUpAxis::ZAxis )
				{
					AdditionalRotation *= FTransform( FRotator( 90.0f, 0.0f, 0.0f ) );
				}
			}

			// Invert compensation applied to parent if it's a light or camera component
			if ( const USceneComponent* AttachParent = Component.GetAttachParent() )
			{
				if ( AttachParent->IsA( UCineCameraComponent::StaticClass() ) ||
					AttachParent->IsA( ULightComponent::StaticClass() ) )
				{
					FTransform InverseCompensation = FTransform( FRotator( 0.0f, 90.f, 0.0f ) );

					if ( UsdUtils::GetUsdStageUpAxis( UsdStage ) == pxr::UsdGeomTokens->z )
					{
						InverseCompensation *= FTransform( FRotator( 90.0f, 0.f, 0.0f ) );
					}

					AdditionalRotation = AdditionalRotation * InverseCompensation.Inverse();
				}
			}

			BakerType = EBakingType::Transform;
			BakerFunction = [&Component, AdditionalRotation, StageInfo, Attr]( double UsdTimeCode )
			{
				FScopedUsdAllocs Allocs;

				FTransform FinalUETransform = AdditionalRotation * Component.GetRelativeTransform();
				pxr::GfMatrix4d UsdTransform = UnrealToUsd::ConvertTransform( StageInfo, FinalUETransform );
				Attr.Set< pxr::GfMatrix4d>( UsdTransform, UsdTimeCode );
			};
		}
		// bHidden is for the actor, and bHiddenInGame is for a component
		// A component is only visible when it's not hidden and its actor is not hidden
		// A bHidden is just handled like a bHiddenInGame for the actor's root component
		// Whenever we handle a bHiddenInGame, we always combine it with the actor's bHidden
		else if ( PropertyPath == TEXT( "bHidden" ) || PropertyPath == TEXT( "bHiddenInGame" ) )
		{
			pxr::UsdGeomImageable Imageable( UsdPrim );
			if ( !Imageable )
			{
				return false;
			}

			pxr::UsdAttribute Attr = Imageable.CreateVisibilityAttr();
			Attr.Clear();

			BakerType = EBakingType::Visibility;
			BakerFunction = [&Component, Imageable]( double UsdTimeCode )
			{
				if ( Component.bHiddenInGame || Component.GetOwner()->IsHidden() )
				{
					Imageable.MakeInvisible( UsdTimeCode );

					if ( pxr::UsdAttribute Attr = Imageable.CreateVisibilityAttr() )
					{
						if ( !Attr.HasAuthoredValue() )
						{
							Attr.Set<pxr::TfToken>( pxr::UsdGeomTokens->invisible, UsdTimeCode );
						}
					}
				}
				else
				{
					Imageable.MakeVisible( UsdTimeCode );

					// Imagine our visibility track has a single key that switches to hidden at frame 60.
					// If our prim is visible by default, MakeVisible will author absolutely nothing, and we'll end up
					// with a timeSamples that just has '60: "invisible"'. Weirdly enough, in USD that means the prim
					// will be invisible throughout *the entire duration of the animation* though, which is not what we want.
					// This check will ensure that if we're visible we should have a value here and not rely on the
					// fallback value of 'visible', as that doesn't behave how we want.
					if ( pxr::UsdAttribute Attr = Imageable.CreateVisibilityAttr() )
					{
						if ( !Attr.HasAuthoredValue() )
						{
							Attr.Set<pxr::TfToken>( pxr::UsdGeomTokens->inherited, UsdTimeCode );
						}
					}
				}
			};
		}
	}

	if ( const UCineCameraComponent* CameraComponent = Cast<UCineCameraComponent>( &Component ) )
	{
		static TSet<FString> RelevantProperties = {
			TEXT( "CurrentFocalLength" ),
			TEXT( "FocusSettings.ManualFocusDistance" ),
			TEXT( "CurrentAperture" ),
			TEXT( "Filmback.SensorWidth" ),
			TEXT( "Filmback.SensorHeight" )
		};

		if ( RelevantProperties.Contains( PropertyPath ) )
		{
			BakerType = EBakingType::Camera;
			BakerFunction = [UsdStage, CameraComponent, UsdPrim]( double UsdTimeCode ) mutable
			{
				UnrealToUsd::ConvertCameraComponent( *CameraComponent, UsdPrim, UsdTimeCode );
			};
		}
	}
	else if ( const ULightComponentBase* LightComponentBase = Cast<ULightComponentBase>( &Component ) )
	{
		if ( const URectLightComponent* RectLightComponent = Cast<URectLightComponent>( LightComponentBase ) )
		{
			static TSet<FString> RelevantProperties = {
				TEXT( "SourceHeight" ),
				TEXT( "SourceWidth" ),
				TEXT( "Temperature" ),
				TEXT( "bUseTemperature" ),
				TEXT( "LightColor" ),
				TEXT( "Intensity" )
			};

			if ( RelevantProperties.Contains( PropertyPath ) )
			{
				BakerType = EBakingType::Light;
				BakerFunction = [UsdStage, RectLightComponent, UsdPrim]( double UsdTimeCode ) mutable
				{
					UnrealToUsd::ConvertLightComponent( *RectLightComponent, UsdPrim, UsdTimeCode );
					UnrealToUsd::ConvertRectLightComponent( *RectLightComponent, UsdPrim, UsdTimeCode );
				};
			}
		}
		else if ( const USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>( LightComponentBase ) )
		{
			static TSet<FString> RelevantProperties = {
				TEXT( "OuterConeAngle" ),
				TEXT( "InnerConeAngle" ),
				TEXT( "Temperature" ),
				TEXT( "bUseTemperature" ),
				TEXT( "LightColor" ),
				TEXT( "Intensity" )
			};

			if ( RelevantProperties.Contains( PropertyPath ) )
			{
				BakerType = EBakingType::Light;
				BakerFunction = [UsdStage, SpotLightComponent, UsdPrim]( double UsdTimeCode ) mutable
				{
					UnrealToUsd::ConvertLightComponent( *SpotLightComponent, UsdPrim, UsdTimeCode );
					UnrealToUsd::ConvertPointLightComponent( *SpotLightComponent, UsdPrim, UsdTimeCode );
					UnrealToUsd::ConvertSpotLightComponent( *SpotLightComponent, UsdPrim, UsdTimeCode );
				};
			}
		}
		else if ( const UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>( LightComponentBase ) )
		{
			static TSet<FString> RelevantProperties = {
				TEXT( "SourceRadius" ),
				TEXT( "Temperature" ),
				TEXT( "bUseTemperature" ),
				TEXT( "LightColor" ),
				TEXT( "Intensity" )
			};

			if ( RelevantProperties.Contains( PropertyPath ) )
			{
				BakerType = EBakingType::Light;
				BakerFunction = [UsdStage, PointLightComponent, UsdPrim]( double UsdTimeCode ) mutable
				{
					UnrealToUsd::ConvertLightComponent( *PointLightComponent, UsdPrim, UsdTimeCode );
					UnrealToUsd::ConvertPointLightComponent( *PointLightComponent, UsdPrim, UsdTimeCode );
				};
			}
		}
		else if ( const UDirectionalLightComponent* DirectionalLightComponent = Cast<UDirectionalLightComponent>( LightComponentBase ) )
		{
			static TSet<FString> RelevantProperties = {
				TEXT( "LightSourceAngle" ),
				TEXT( "Temperature" ),
				TEXT( "bUseTemperature" ),
				TEXT( "LightColor" ),
				TEXT( "Intensity" )
			};

			if ( RelevantProperties.Contains( PropertyPath ) )
			{
				BakerType = EBakingType::Light;
				BakerFunction = [UsdStage, DirectionalLightComponent, UsdPrim]( double UsdTimeCode ) mutable
				{
					UnrealToUsd::ConvertLightComponent( *DirectionalLightComponent, UsdPrim, UsdTimeCode );
					UnrealToUsd::ConvertDirectionalLightComponent( *DirectionalLightComponent, UsdPrim, UsdTimeCode );
				};
			}
		}
	}

	if ( BakerType != EBakingType::None && BakerFunction)
	{
		OutBaker.BakerType = BakerType;
		OutBaker.BakerFunction = BakerFunction;
		return true;
	}

	return false;
}

bool UnrealToUsd::CreateSkeletalAnimationBaker( UE::FUsdPrim& SkelRoot, UE::FUsdPrim& SkelAnimation, USkeletalMeshComponent& Component, FComponentBaker& OutBaker )
{
#if WITH_EDITOR
	USkeletalMesh* SkeletalMesh = Component.GetSkeletalMeshAsset();
	if ( !SkeletalMesh )
	{
		return false;
	}

	FScopedUsdAllocs Allocs;

	pxr::UsdSkelRoot UsdSkelRoot{ SkelRoot };
	pxr::UsdSkelAnimation UsdSkelAnimation{ SkelAnimation };
	if ( !SkelRoot || !SkelAnimation )
	{
		return false;
	}

	// Make sure that the skel root is using our animation
	pxr::UsdPrim SkelRootPrim = UsdSkelRoot.GetPrim();
	pxr::UsdPrim SkelAnimPrim = UsdSkelAnimation.GetPrim();
	UsdUtils::BindAnimationSource( SkelRootPrim, SkelAnimPrim );

	FUsdStageInfo StageInfo{ SkelRoot.GetStage() };

	pxr::UsdAttribute JointsAttr			= UsdSkelAnimation.CreateJointsAttr();
	pxr::UsdAttribute TranslationsAttr		= UsdSkelAnimation.CreateTranslationsAttr();
	pxr::UsdAttribute RotationsAttr			= UsdSkelAnimation.CreateRotationsAttr();
	pxr::UsdAttribute ScalesAttr			= UsdSkelAnimation.CreateScalesAttr();

	// Joints
	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const int32 NumBones = RefSkeleton.GetRefBoneInfo().Num();
	UnrealToUsd::ConvertJointsAttribute( RefSkeleton, JointsAttr );

	// Build active morph targets array if it isn't setup already
	TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->GetMorphTargets();
	if ( Component.ActiveMorphTargets.Num() != Component.MorphTargetWeights.Num() && MorphTargets.Num() != 0 )
	{
		for ( int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargets.Num(); ++MorphTargetIndex )
		{
			Component.ActiveMorphTargets.Add( MorphTargets[ MorphTargetIndex ], MorphTargetIndex );
		}
	}

	// Blend shape names
	// Here we have to export UMorphTarget FNames in some order, then the weights in that same order. That is all.
	// Those work out as "channels", and USD will resolve those to match the right thing on each mesh.
	// We sort them in weight index order so that within the Baker we just write out weights in the order they are in.
	pxr::UsdAttribute BlendShapeWeightsAttr;
	pxr::UsdAttribute BlendShapesAttr;
	const int32 NumMorphTargets = Component.MorphTargetWeights.Num();
	if ( NumMorphTargets > 0 )
	{
		BlendShapeWeightsAttr = UsdSkelAnimation.CreateBlendShapeWeightsAttr();
		BlendShapesAttr = UsdSkelAnimation.CreateBlendShapesAttr();

		TArray<FMorphTargetWeightMap::ElementType> SortedMorphTargets = Component.ActiveMorphTargets.Array();
		SortedMorphTargets.Sort([](const FMorphTargetWeightMap::ElementType& Left, const FMorphTargetWeightMap::ElementType& Right)
		{
			return Left.Value < Right.Value;
		});

		pxr::VtArray< pxr::TfToken > BlendShapeNames;
		BlendShapeNames.reserve(SortedMorphTargets.Num());

		for ( const FMorphTargetWeightMap::ElementType& ActiveMorphTarget : SortedMorphTargets )
		{
			FString BlendShapeName;
			if ( const UMorphTarget* MorphTarget = ActiveMorphTarget.Key )
			{
				BlendShapeName = MorphTarget->GetFName().ToString();
			}

			BlendShapeNames.push_back( UnrealToUsd::ConvertToken( *BlendShapeName ).Get() );
		}

		BlendShapesAttr.Set( BlendShapeNames );
	}

	pxr::VtVec3fArray Translations;
	pxr::VtQuatfArray Rotations;
	pxr::VtVec3hArray Scales;
	pxr::VtArray< float > BlendShapeWeights;

	OutBaker.BakerType = EBakingType::Skeletal;
	OutBaker.BakerFunction =
		[&Component, StageInfo, Translations, Rotations, Scales, BlendShapeWeights, TranslationsAttr, RotationsAttr, ScalesAttr, BlendShapeWeightsAttr, NumBones, NumMorphTargets]
		( double UsdTimeCode ) mutable
		{
			FScopedUsdAllocs InnerAllocs;

			Translations.resize( NumBones );
			Rotations.resize( NumBones );
			Scales.resize( NumBones );

			// This whole incantation is required or else the component will really not update until the next frame.
			// Note: This will also cause the update of morph target weights.
			Component.TickAnimation( 0.f, false );
			Component.UpdateLODStatus();
			Component.RefreshBoneTransforms();
			Component.RefreshFollowerComponents();
			Component.UpdateComponentToWorld();
			Component.FinalizeBoneTransform();
			Component.MarkRenderTransformDirty();
			Component.MarkRenderDynamicDataDirty();

			const TArray< FTransform >& LocalBoneTransforms = Component.GetBoneSpaceTransforms();
			for ( int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex )
			{
				FTransform BoneTransform = LocalBoneTransforms[ BoneIndex ];
				BoneTransform = UsdUtils::ConvertAxes( StageInfo.UpAxis == EUsdUpAxis::ZAxis, BoneTransform );

				Translations[ BoneIndex ] = UnrealToUsd::ConvertVector( BoneTransform.GetTranslation() ) * ( 0.01f / StageInfo.MetersPerUnit );
				Rotations[ BoneIndex ] = UnrealToUsd::ConvertQuat( BoneTransform.GetRotation() ).GetNormalized();
				Scales[ BoneIndex ] = pxr::GfVec3h( UnrealToUsd::ConvertVector( BoneTransform.GetScale3D() ) );
			}

			if ( Translations.size() > 0 )
			{
				TranslationsAttr.Set( Translations, UsdTimeCode );
				RotationsAttr.Set( Rotations, UsdTimeCode );
				ScalesAttr.Set( Scales, UsdTimeCode );
			}

			if ( NumMorphTargets > 0 && BlendShapeWeightsAttr )
			{
				BlendShapeWeights.resize( NumMorphTargets );

				for ( int32 MorphTargetIndex = 0; MorphTargetIndex < NumMorphTargets; ++MorphTargetIndex )
				{
					BlendShapeWeights[ MorphTargetIndex ] = Component.MorphTargetWeights[ MorphTargetIndex ];
				}

				BlendShapeWeightsAttr.Set( BlendShapeWeights, UsdTimeCode );
			}
		};

	return true;
#else
	return false;
#endif // WITH_EDITOR
}

UnrealToUsd::FPropertyTrackWriter UnrealToUsd::CreatePropertyTrackWriter( const USceneComponent& Component, const UMovieScenePropertyTrack& Track, UE::FUsdPrim& Prim, TSet<FName>& OutPropertyPathsToRefresh )
{
	UnrealToUsd::FPropertyTrackWriter Result;

	if ( !Prim )
	{
		return Result;
	}

	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim{ Prim };
	pxr::UsdStageRefPtr UsdStage = UsdPrim.GetStage();
	FUsdStageInfo StageInfo{ UsdStage };

	pxr::SdfChangeBlock ChangeBlock;

	pxr::UsdAttribute Attr;

	const FName& PropertyPath = Track.GetPropertyPath();

	// SceneComponent
	{
		if ( PropertyPath == UnrealIdentifiers::TransformPropertyName )
		{
			if ( pxr::UsdGeomXformable Xformable{ UsdPrim } )
			{
				Xformable.CreateXformOpOrderAttr();

				if ( pxr::UsdGeomXformOp TransformOp = UE::USDPrimConversionImpl::Private::ForceMatrixXform( Xformable ) )
				{
					Attr = TransformOp.GetAttr();

					// Compensate different orientation for light or camera components
					FTransform AdditionalRotation = FTransform::Identity;
					if ( UsdPrim.IsA< pxr::UsdGeomCamera >() || UsdPrim.HasAPI< pxr::UsdLuxLightAPI >() )
					{
						AdditionalRotation = FTransform( FRotator( 0.0f, 90.0f, 0.0f ) );

						if ( StageInfo.UpAxis == EUsdUpAxis::ZAxis )
						{
							AdditionalRotation *= FTransform( FRotator( 90.0f, 0.0f, 0.0f ) );
						}
					}

					// Invert compensation applied to parent if it's a light or camera component
					if ( const USceneComponent* AttachParent = Component.GetAttachParent() )
					{
						if ( AttachParent->IsA( UCineCameraComponent::StaticClass() ) ||
							AttachParent->IsA( ULightComponent::StaticClass() ) )
						{
							FTransform InverseCompensation = FTransform( FRotator( 0.0f, 90.f, 0.0f ) );

							if ( UsdUtils::GetUsdStageUpAxis( UsdStage ) == pxr::UsdGeomTokens->z )
							{
								InverseCompensation *= FTransform( FRotator( 90.0f, 0.f, 0.0f ) );
							}

							AdditionalRotation = AdditionalRotation * InverseCompensation.Inverse();
						}
					}

					Result.TransformWriter = [&Component, AdditionalRotation, StageInfo, Attr]( const FTransform& UEValue, double UsdTimeCode )
					{
						FTransform FinalUETransform = AdditionalRotation * UEValue;
						pxr::GfMatrix4d UsdTransform = UnrealToUsd::ConvertTransform( StageInfo, FinalUETransform );
						Attr.Set< pxr::GfMatrix4d >( UsdTransform, UsdTimeCode );
					};
				}
			}
		}
		// bHidden is for the actor, and bHiddenInGame is for a component
		// A component is only visible when it's not hidden and its actor is not hidden
		// A bHidden is just handled like a bHiddenInGame for the actor's root component
		// Whenever we handle a bHiddenInGame, we always combine it with the actor's bHidden
		else if ( PropertyPath == UnrealIdentifiers::HiddenPropertyName || PropertyPath == UnrealIdentifiers::HiddenInGamePropertyName )
		{
			if ( pxr::UsdGeomImageable Imageable{ UsdPrim } )
			{
				Attr = Imageable.CreateVisibilityAttr();
				if ( Attr )
				{
					Result.BoolWriter = [Imageable, Attr]( bool UEValue, double UsdTimeCode )
					{
						if ( UEValue )
						{
							// We have to do both here as MakeVisible will ensure we also flip any parent prims,
							// and setting the attribute will ensure we write a timeSample. Otherwise if MakeVisible
							// finds that the prim should already be visible due to a stronger opinion, it won't write anything
							Attr.Set<pxr::TfToken>( pxr::UsdGeomTokens->inherited, UsdTimeCode );
							Imageable.MakeVisible( UsdTimeCode );
						}
						else
						{
							Attr.Set<pxr::TfToken>( pxr::UsdGeomTokens->invisible, UsdTimeCode );
							Imageable.MakeInvisible( UsdTimeCode );
						}
					};
				}
			}
		}
	}

	if ( pxr::UsdGeomCamera Camera{ UsdPrim } )
	{
		bool bConvertDistance = true;

		if ( PropertyPath == UnrealIdentifiers::CurrentFocalLengthPropertyName )
		{
			Attr = Camera.CreateFocalLengthAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::ManualFocusDistancePropertyName )
		{
			Attr = Camera.CreateFocusDistanceAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::CurrentAperturePropertyName )
		{
			bConvertDistance = false;
			Attr = Camera.CreateFStopAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::SensorWidthPropertyName )
		{
			Attr = Camera.CreateHorizontalApertureAttr();
		}
		else if ( PropertyPath == UnrealIdentifiers::SensorHeightPropertyName )
		{
			Attr = Camera.CreateVerticalApertureAttr();
		}

		if ( Attr )
		{
			if ( bConvertDistance )
			{
				Result.FloatWriter = [Attr, StageInfo]( float UEValue, double UsdTimeCode ) mutable
				{
					Attr.Set( UnrealToUsd::ConvertDistance( StageInfo, UEValue ), UsdTimeCode );
				};
			}
			else
			{
				Result.FloatWriter = [Attr]( float UEValue, double UsdTimeCode ) mutable
				{
					Attr.Set( UEValue, UsdTimeCode );
				};
			}
		}
	}
	else if ( pxr::UsdLuxLightAPI LightAPI{ Prim } )
	{
		if ( PropertyPath == UnrealIdentifiers::LightColorPropertyName )
		{
			Attr = LightAPI.GetColorAttr();
			if ( Attr )
			{
				Result.ColorWriter = [Attr]( const FLinearColor& UEValue, double UsdTimeCode )
				{
					pxr::GfVec4f Vec4 = UnrealToUsd::ConvertColor( UEValue );
					Attr.Set( pxr::GfVec3f{ Vec4[ 0 ], Vec4[ 1 ], Vec4[ 2 ] }, UsdTimeCode );
				};
			}
		}
		else if ( PropertyPath == UnrealIdentifiers::UseTemperaturePropertyName )
		{
			Attr = LightAPI.GetEnableColorTemperatureAttr();
			if ( Attr )
			{
				Result.BoolWriter = [Attr]( bool UEValue, double UsdTimeCode )
				{
					Attr.Set( UEValue, UsdTimeCode );
				};
			}
		}
		else if ( PropertyPath == UnrealIdentifiers::TemperaturePropertyName )
		{
			Attr = LightAPI.GetColorTemperatureAttr();
			if ( Attr )
			{
				Result.FloatWriter = [Attr]( float UEValue, double UsdTimeCode )
				{
					Attr.Set( UEValue, UsdTimeCode );
				};
			}
		}

		else if ( pxr::UsdLuxSphereLight SphereLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::SourceRadiusPropertyName )
			{
				OutPropertyPathsToRefresh.Add( UnrealIdentifiers::IntensityPropertyName );

				Attr = SphereLight.GetRadiusAttr();
				if ( Attr )
				{
					Result.FloatWriter = [Attr, StageInfo]( float UEValue, double UsdTimeCode )
					{
						Attr.Set( UnrealToUsd::ConvertDistance( StageInfo, UEValue ), UsdTimeCode );
					};
				}
			}

			// Spot light
			else if ( UsdPrim.HasAPI< pxr::UsdLuxShapingAPI >() )
			{
				pxr::UsdLuxShapingAPI ShapingAPI{ UsdPrim };

				if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
				{
					Attr = SphereLight.GetIntensityAttr();
					pxr::UsdAttribute RadiusAttr = SphereLight.GetRadiusAttr();
					pxr::UsdAttribute ConeAngleAttr = ShapingAPI.GetShapingConeAngleAttr();
					pxr::UsdAttribute ConeSoftnessAttr = ShapingAPI.GetShapingConeSoftnessAttr();

					// Always clear exposure because we'll put all of our "light intensity" on the intensity attr and assume exposure
					// is zero, as we can't manipulate something like that exposure directly from UE anyway
					if ( pxr::UsdAttribute ExposureAttr = SphereLight.GetExposureAttr() )
					{
						ExposureAttr.Clear();
					}

					// For now we'll assume the light intensity units are constant and the user doesn't have any light intensity unit tracks...
					ELightUnits Units = ELightUnits::Lumens;
					if ( const ULocalLightComponent* LightComponent = Cast<const ULocalLightComponent>( &Component ) )
					{
						Units = LightComponent->IntensityUnits;
					}

					if ( Attr && RadiusAttr && ConeAngleAttr && ConeSoftnessAttr )
					{
						Result.FloatWriter = [Attr, RadiusAttr, ConeAngleAttr, ConeSoftnessAttr, StageInfo, Units]( float UEValue, double UsdTimeCode )
						{
							const float UsdConeAngle = UsdUtils::GetUsdValue< float >( ConeAngleAttr, UsdTimeCode );
							const float UsdConeSoftness = UsdUtils::GetUsdValue< float >( ConeSoftnessAttr, UsdTimeCode );
							const float UsdRadius = UsdUtils::GetUsdValue< float >( RadiusAttr, UsdTimeCode );

							float InnerConeAngle = 0.0f;
							const float OuterConeAngle = UsdToUnreal::ConvertConeAngleSoftnessAttr( UsdConeAngle, UsdConeSoftness, InnerConeAngle );
							const float SourceRadius = UsdToUnreal::ConvertDistance( StageInfo, UsdRadius );

							Attr.Set(
								UnrealToUsd::ConvertSpotLightIntensityProperty( UEValue, OuterConeAngle, InnerConeAngle, SourceRadius, StageInfo, Units ),
								UsdTimeCode
							);
						};
					}
				}
				else if ( PropertyPath == UnrealIdentifiers::OuterConeAnglePropertyName )
				{
					Attr = ShapingAPI.GetShapingConeAngleAttr();
					if ( Attr )
					{
						// InnerConeAngle is calculated based on ConeAngleAttr, so we need to refresh it
						OutPropertyPathsToRefresh.Add( UnrealIdentifiers::InnerConeAnglePropertyName );

						Result.FloatWriter = [Attr]( float UEValue, double UsdTimeCode )
						{
							Attr.Set( UEValue, UsdTimeCode );
						};
					}
				}
				else if ( PropertyPath == UnrealIdentifiers::InnerConeAnglePropertyName )
				{
					Attr = ShapingAPI.GetShapingConeSoftnessAttr();
					pxr::UsdAttribute ConeAngleAttr = ShapingAPI.GetShapingConeAngleAttr();

					if ( ConeAngleAttr && Attr )
					{
						Result.FloatWriter = [Attr, ConeAngleAttr]( float UEValue, double UsdTimeCode )
						{
							const float UsdConeAngle = UsdUtils::GetUsdValue< float >( ConeAngleAttr, UsdTimeCode );
							const float OuterConeAngle = UsdConeAngle;

							const float OutNewSoftness = UnrealToUsd::ConvertInnerConeAngleProperty( UEValue, OuterConeAngle );
							Attr.Set( OutNewSoftness, UsdTimeCode );
						};
					}
				}
			}
			// Just a point light
			else
			{
				if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
				{
					Attr = SphereLight.GetIntensityAttr();
					pxr::UsdAttribute RadiusAttr = SphereLight.GetRadiusAttr();

					// Always clear exposure because we'll put all of our "light intensity" on the intensity attr and assume exposure
					// is zero, as we can't manipulate something like that exposure directly from UE anyway
					if ( pxr::UsdAttribute ExposureAttr = SphereLight.GetExposureAttr() )
					{
						ExposureAttr.Clear();
					}

					// For now we'll assume the light intensity units are constant and the user doesn't have any light intensity unit tracks...
					ELightUnits Units = ELightUnits::Lumens;
					if ( const ULocalLightComponent* LightComponent = Cast<const ULocalLightComponent>( &Component ) )
					{
						Units = LightComponent->IntensityUnits;
					}

					if ( Attr && RadiusAttr )
					{
						Result.FloatWriter = [Attr, RadiusAttr, StageInfo, Units]( float UEValue, double UsdTimeCode )
						{
							const float SourceRadius = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( RadiusAttr, UsdTimeCode ) );
							Attr.Set( UnrealToUsd::ConvertPointLightIntensityProperty( UEValue, SourceRadius, StageInfo, Units ), UsdTimeCode );
						};
					}
				}
			}
		}
		else if ( pxr::UsdLuxRectLight RectLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::SourceWidthPropertyName )
			{
				Attr = RectLight.GetWidthAttr();
				if ( Attr )
				{
					OutPropertyPathsToRefresh.Add( UnrealIdentifiers::IntensityPropertyName );

					Result.FloatWriter = [Attr, StageInfo]( float UEValue, double UsdTimeCode )
					{
						Attr.Set( UnrealToUsd::ConvertDistance( StageInfo, UEValue ), UsdTimeCode );
					};
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::SourceHeightPropertyName )
			{
				Attr = RectLight.GetHeightAttr();
				if ( Attr )
				{
					OutPropertyPathsToRefresh.Add( UnrealIdentifiers::IntensityPropertyName );

					Result.FloatWriter = [Attr, StageInfo]( float UEValue, double UsdTimeCode )
					{
						Attr.Set( UnrealToUsd::ConvertDistance( StageInfo, UEValue ), UsdTimeCode );
					};
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
			{
				Attr = RectLight.GetIntensityAttr();
				pxr::UsdAttribute WidthAttr = RectLight.GetWidthAttr();
				pxr::UsdAttribute HeightAttr = RectLight.GetHeightAttr();

				// Always clear exposure because we'll put all of our "light intensity" on the intensity attr and assume exposure
				// is zero, as we can't manipulate something like that exposure directly from UE anyway
				if ( pxr::UsdAttribute ExposureAttr = RectLight.GetExposureAttr() )
				{
					ExposureAttr.Clear();
				}

				// For now we'll assume the light intensity units are constant and the user doesn't have any light intensity unit tracks...
				ELightUnits Units = ELightUnits::Lumens;
				if ( const ULocalLightComponent* LightComponent = Cast<const ULocalLightComponent>( &Component ) )
				{
					Units = LightComponent->IntensityUnits;
				}

				if ( Attr && WidthAttr && HeightAttr )
				{
					Result.FloatWriter = [Attr, WidthAttr, HeightAttr, StageInfo, Units]( float UEValue, double UsdTimeCode )
					{
						const float Width = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( WidthAttr, UsdTimeCode ) );
						const float Height = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( HeightAttr, UsdTimeCode ) );

						Attr.Set( UnrealToUsd::ConvertRectLightIntensityProperty( UEValue, Width, Height, StageInfo, Units ), UsdTimeCode );
					};
				}
			}
		}
		else if ( pxr::UsdLuxDiskLight DiskLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::SourceWidthPropertyName || PropertyPath == UnrealIdentifiers::SourceHeightPropertyName )
			{
				Attr = DiskLight.GetRadiusAttr();
				if ( Attr )
				{
					OutPropertyPathsToRefresh.Add( UnrealIdentifiers::IntensityPropertyName );

					// Resync the other to match this one after we bake it, effectively always enforcing the UE rect light into a square shape
					OutPropertyPathsToRefresh.Add( PropertyPath == UnrealIdentifiers::SourceWidthPropertyName
						? UnrealIdentifiers::SourceHeightPropertyName
						: UnrealIdentifiers::SourceWidthPropertyName
					);

					Result.FloatWriter = [Attr, StageInfo]( float UEValue, double UsdTimeCode )
					{
						Attr.Set( UnrealToUsd::ConvertDistance( StageInfo, UEValue * 0.5f ), UsdTimeCode );
					};
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
			{
				Attr = RectLight.GetIntensityAttr();
				pxr::UsdAttribute RadiusAttr = DiskLight.GetRadiusAttr();

				// Always clear exposure because we'll put all of our "light intensity" on the intensity attr and assume exposure
				// is zero, as we can't manipulate something like that exposure directly from UE anyway
				if ( pxr::UsdAttribute ExposureAttr = RectLight.GetExposureAttr() )
				{
					ExposureAttr.Clear();
				}

				// For now we'll assume the light intensity units are constant and the user doesn't have any light intensity unit tracks...
				ELightUnits Units = ELightUnits::Lumens;
				if ( const ULocalLightComponent* LightComponent = Cast<const ULocalLightComponent>( &Component ) )
				{
					Units = LightComponent->IntensityUnits;
				}

				if ( Attr && RadiusAttr )
				{
					Result.FloatWriter = [Attr, RadiusAttr, StageInfo, Units]( float UEValue, double UsdTimeCode )
					{
						const float Radius = UsdToUnreal::ConvertDistance( StageInfo, UsdUtils::GetUsdValue< float >( RadiusAttr, UsdTimeCode ) );

						Attr.Set( UnrealToUsd::ConvertRectLightIntensityProperty( UEValue, Radius, StageInfo, Units ), UsdTimeCode );
					};
				}
			}
		}
		else if ( pxr::UsdLuxDistantLight DistantLight{ UsdPrim } )
		{
			if ( PropertyPath == UnrealIdentifiers::LightSourceAnglePropertyName )
			{
				Attr = DistantLight.GetAngleAttr();
				if ( Attr )
				{
					Result.FloatWriter = [Attr]( float UEValue, double UsdTimeCode )
					{
						Attr.Set( UEValue, UsdTimeCode );
					};
				}
			}
			else if ( PropertyPath == UnrealIdentifiers::IntensityPropertyName )
			{
				Attr = DistantLight.GetIntensityAttr();

				// Always clear exposure because we'll put all of our "light intensity" on the intensity attr and assume exposure
				// is zero, as we can't manipulate something like that exposure directly from UE anyway
				if ( pxr::UsdAttribute ExposureAttr = RectLight.GetExposureAttr() )
				{
					ExposureAttr.Clear();
				}

				if ( Attr )
				{
					Result.FloatWriter = [Attr]( float UEValue, double UsdTimeCode )
					{
						Attr.Set( UnrealToUsd::ConvertLightIntensityProperty( UEValue ), UsdTimeCode );
					};
				}
			}
		}
	}

	if ( Attr )
	{
		std::vector<double> TimeSamples;
		Attr.GetTimeSamples( &TimeSamples );
		for ( double TimeSample : TimeSamples )
		{
			Attr.ClearAtTime( TimeSample );
		}

		UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
	}

	return Result;
}

bool UnrealToUsd::ConvertXformable( const UMovieScene3DTransformTrack& MovieSceneTrack, pxr::UsdPrim& UsdPrim, const FMovieSceneSequenceTransform& SequenceTransform )
{
	if ( !UsdPrim )
	{
		return false;
	}

	const FUsdStageInfo StageInfo( UsdPrim.GetStage() );

	const UMovieScene* MovieScene = MovieSceneTrack.GetTypedOuter< UMovieScene >();
	if ( !MovieScene )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdGeomXformable Xformable( UsdPrim );
	if ( !Xformable )
	{
		return false;
	}

	UMovieScene3DTransformSection* TransformSection = Cast< UMovieScene3DTransformSection >( const_cast< UMovieScene3DTransformTrack& >( MovieSceneTrack ).FindSection( 0 ) );
	if ( !TransformSection )
	{
		return false;
	}

	const TRange< FFrameNumber > PlaybackRange = MovieScene->GetPlaybackRange();
	const FFrameRate Resolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();

	const double StageTimeCodesPerSecond = UsdPrim.GetStage()->GetTimeCodesPerSecond();
	const FFrameRate StageFrameRate( StageTimeCodesPerSecond, 1 );

	auto EvaluateChannel = [ &PlaybackRange, &Resolution, &DisplayRate, &SequenceTransform ]( const FMovieSceneDoubleChannel* Channel, double DefaultValue ) -> TArray< TPair< FFrameNumber, float > >
	{
		TArray< TPair< FFrameNumber, float > > Values;

		if ( PlaybackRange.HasLowerBound() && PlaybackRange.HasUpperBound() )
		{
			const FFrameTime Interval = FFrameRate::TransformTime( 1, DisplayRate, Resolution );
			const FFrameNumber StartFrame = UE::MovieScene::DiscreteInclusiveLower( PlaybackRange );
			const FFrameNumber EndFrame = UE::MovieScene::DiscreteExclusiveUpper( PlaybackRange );

			for ( FFrameTime EvalTime = StartFrame; EvalTime < EndFrame; EvalTime += Interval )
			{
				FFrameNumber KeyTime = FFrameRate::Snap( EvalTime, Resolution, DisplayRate ).FloorToFrame();

				double Result = DefaultValue;
				if ( Channel )
				{
					Result = Channel->GetDefault().Get( DefaultValue );
					Channel->Evaluate( KeyTime, Result );
				}

				FFrameTime GlobalEvalTime( KeyTime );
				GlobalEvalTime *= SequenceTransform.InverseLinearOnly();

				Values.Emplace( GlobalEvalTime.GetFrame(), Result );
			}
		}

		return Values;
	};

	TArrayView< FMovieSceneDoubleChannel* > Channels = TransformSection->GetChannelProxy().GetChannels< FMovieSceneDoubleChannel >();
	check( Channels.Num() >= 9 );

	auto GetChannel = [ &Channels ]( const int32 ChannelIndex ) -> const FMovieSceneDoubleChannel*
	{
		if ( Channels.IsValidIndex( ChannelIndex ) )
		{
			return Channels[ ChannelIndex ];
		}
		else
		{
			return  nullptr;
		}
	};

	// Translation
	TArray< TPair< FFrameNumber, float > > LocationValuesX = EvaluateChannel( GetChannel(0), 0.0 );
	TArray< TPair< FFrameNumber, float > > LocationValuesY = EvaluateChannel( GetChannel(1), 0.0 );
	TArray< TPair< FFrameNumber, float > > LocationValuesZ = EvaluateChannel( GetChannel(2), 0.0 );

	// Rotation
	TArray< TPair< FFrameNumber, float > > RotationValuesX = EvaluateChannel( GetChannel(3), 0.0 );
	TArray< TPair< FFrameNumber, float > > RotationValuesY = EvaluateChannel( GetChannel(4), 0.0 );
	TArray< TPair< FFrameNumber, float > > RotationValuesZ = EvaluateChannel( GetChannel(5), 0.0 );

	// Scale
	TArray< TPair< FFrameNumber, float > > ScaleValuesX = EvaluateChannel( GetChannel(6), 1.0 );
	TArray< TPair< FFrameNumber, float > > ScaleValuesY = EvaluateChannel( GetChannel(7), 1.0 );
	TArray< TPair< FFrameNumber, float > > ScaleValuesZ = EvaluateChannel( GetChannel(8), 1.0 );

	bool bIsDataOutOfSync = false;
	{
		int32 ValueIndex = 0;

		FFrameTime UsdStartTime = FFrameRate::TransformTime( PlaybackRange.GetLowerBoundValue(), Resolution, StageFrameRate );
		FFrameTime UsdEndTime = FFrameRate::TransformTime( PlaybackRange.GetUpperBoundValue(), Resolution, StageFrameRate );

		std::vector< double > UsdTimeSamples;
		if ( LocationValuesX.Num() > 0 || ( Xformable.GetTimeSamples( &UsdTimeSamples ) && UsdTimeSamples.size() > 0 ) )
		{
			bIsDataOutOfSync = ( UsdTimeSamples.size() != LocationValuesX.Num() );

			if ( !bIsDataOutOfSync )
			{
				for ( const TPair< FFrameNumber, float >& Value : LocationValuesX )
				{
					FFrameTime UsdFrameTime = FFrameRate::TransformTime( Value.Key, Resolution, StageFrameRate );

					FVector Location( LocationValuesX[ ValueIndex ].Value, LocationValuesY[ ValueIndex ].Value, LocationValuesZ[ ValueIndex ].Value );
					FRotator Rotation( RotationValuesY[ ValueIndex ].Value, RotationValuesZ[ ValueIndex ].Value, RotationValuesX[ ValueIndex ].Value );
					FVector Scale( ScaleValuesX[ ValueIndex ].Value, ScaleValuesY[ ValueIndex ].Value, ScaleValuesZ[ ValueIndex ].Value );

					FTransform Transform( Rotation, Location, Scale );
					pxr::GfMatrix4d UsdTransform = UnrealToUsd::ConvertTransform( StageInfo, Transform );

					pxr::GfMatrix4d UsdMatrix;
					bool bResetXFormStack = false;
					Xformable.GetLocalTransformation( &UsdMatrix, &bResetXFormStack, UsdFrameTime.AsDecimal() );

					if ( !pxr::GfIsClose( UsdMatrix, UsdTransform, THRESH_POINTS_ARE_NEAR ) )
					{
						bIsDataOutOfSync = true;
						break;
					}

					++ValueIndex;
				}
			}
		}
	}

	if ( bIsDataOutOfSync )
	{
		if ( pxr::UsdGeomXformOp TransformOp = UE::USDPrimConversionImpl::Private::ForceMatrixXform( Xformable ) )
		{
			TransformOp.GetAttr().Clear(); // Clear existing transform data
		}

		pxr::SdfChangeBlock ChangeBlock;

		// Compensate different orientation for light or camera components
		FTransform CameraCompensation = FTransform::Identity;
		if ( UsdPrim.IsA< pxr::UsdGeomCamera >() || UsdPrim.HasAPI< pxr::UsdLuxLightAPI >() )
		{
			CameraCompensation = FTransform( FRotator( 0.0f, 90.0f, 0.0f ) );

			if ( StageInfo.UpAxis == EUsdUpAxis::ZAxis )
			{
				CameraCompensation *= FTransform( FRotator( 90.0f, 0.0f, 0.0f ) );
			}
		}

		// Invert compensation applied to parent if it's a light or camera component
		FTransform InverseCameraCompensation = FTransform::Identity;
		if ( pxr::UsdPrim ParentPrim = UsdPrim.GetParent() )
		{
			if ( ParentPrim.IsA<pxr::UsdGeomCamera>() || ParentPrim.HasAPI<pxr::UsdLuxLightAPI>() )
			{
				InverseCameraCompensation = FTransform( FRotator( 0.0f, 90.f, 0.0f ) );

				if ( StageInfo.UpAxis == EUsdUpAxis::ZAxis )
				{
					InverseCameraCompensation *= FTransform( FRotator( 90.0f, 0.f, 0.0f ) );
				}
			}
		}

		int32 ValueIndex = 0;
		for ( const TPair< FFrameNumber, float >& Value : LocationValuesX )
		{
			FFrameTime UsdFrameTime = FFrameRate::TransformTime( Value.Key, Resolution, StageFrameRate );

			FVector Location( LocationValuesX[ ValueIndex ].Value, LocationValuesY[ ValueIndex ].Value, LocationValuesZ[ ValueIndex ].Value );
			FRotator Rotation( RotationValuesY[ ValueIndex ].Value, RotationValuesZ[ ValueIndex ].Value, RotationValuesX[ ValueIndex ].Value );
			FVector Scale( ScaleValuesX[ ValueIndex ].Value, ScaleValuesY[ ValueIndex ].Value, ScaleValuesZ[ ValueIndex ].Value );

			FTransform Transform( Rotation, Location, Scale );
			ConvertXformable( CameraCompensation * Transform * InverseCameraCompensation.Inverse(), UsdPrim, UsdFrameTime.AsDecimal() );

			++ValueIndex;
		}
	}

	return true;
}

TArray<UE::FUsdAttribute> UnrealToUsd::GetAttributesForProperty( const UE::FUsdPrim& Prim, const FName& PropertyPath )
{
	using namespace UnrealIdentifiers;

	FScopedUsdAllocs Allocs;
	pxr::UsdPrim UsdPrim{ Prim };

	// Common attributes
	if ( PropertyPath == TransformPropertyName )
	{
		if ( pxr::UsdAttribute Attr = UsdPrim.GetAttribute( UnrealToUsd::ConvertToken( TEXT( "xformOp:transform" ) ).Get() ) )
		{
			return { UE::FUsdAttribute{ Attr } };
		}

		if ( pxr::UsdGeomXformable Xformable{ UsdPrim } )
		{
			return { UE::FUsdAttribute{ Xformable.GetXformOpOrderAttr() } };
		}
	}
	if ( PropertyPath == HiddenInGamePropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdGeomTokens->visibility ) } };
	}

	// Camera attributes
	else if ( PropertyPath == CurrentFocalLengthPropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdGeomTokens->focalLength ) } };
	}
	else if ( PropertyPath == ManualFocusDistancePropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdGeomTokens->focusDistance ) } };
	}
	else if ( PropertyPath == CurrentAperturePropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdGeomTokens->fStop ) } };
	}
	else if ( PropertyPath == SensorWidthPropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdGeomTokens->horizontalAperture ) } };
	}
	else if ( PropertyPath == SensorHeightPropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdGeomTokens->verticalAperture ) } };
	}

	// Light attributes
	else if ( PropertyPath == IntensityPropertyName )
	{
		if ( UsdPrim.IsA<pxr::UsdLuxRectLight>() )
		{
			return {
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsIntensity ) },
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsExposure ) },
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsWidth ) },
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsHeight ) }
			};
		}
		else if ( UsdPrim.IsA<pxr::UsdLuxDiskLight>() )
		{
			return {
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsIntensity ) },
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsExposure ) },
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsRadius ) }
			};
		}
		else if ( UsdPrim.IsA<pxr::UsdLuxDistantLight>() )
		{
			return {
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsIntensity ) },
				UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsExposure ) }
			};
		}
		else if ( UsdPrim.IsA<pxr::UsdLuxSphereLight>() )
		{
			if ( UsdPrim.HasAPI<pxr::UsdLuxShapingAPI>() )
			{
				return {
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsIntensity ) },
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsExposure ) },
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsRadius ) },
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsShapingConeAngle ) },
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsShapingConeSoftness ) }
				};
			}
			else
			{
				return {
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsIntensity ) },
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsExposure ) },
					UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsRadius ) }
				};
			}
		}
	}
	else if ( PropertyPath == LightColorPropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsColor ) } };
	}
	else if ( PropertyPath == UseTemperaturePropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsEnableColorTemperature ) } };
	}
	else if ( PropertyPath == TemperaturePropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsColorTemperature ) } };
	}
	else if ( PropertyPath == SourceWidthPropertyName )
	{
		if ( UsdPrim.IsA<pxr::UsdLuxDiskLight>() )
		{
			return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsRadius ) } };
		}
		else
		{
			return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsWidth ) } };
		}
	}
	else if ( PropertyPath == SourceHeightPropertyName )
	{
		if ( UsdPrim.IsA<pxr::UsdLuxDiskLight>() )
		{
			return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsRadius ) } };
		}
		else
		{
			return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsHeight ) } };
		}
	}
	else if ( PropertyPath == SourceRadiusPropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsRadius ) } };
	}
	else if ( PropertyPath == OuterConeAnglePropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsShapingConeAngle ) } };
	}
	else if ( PropertyPath == InnerConeAnglePropertyName )
	{
		return {
			UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsShapingConeAngle ) },
			UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsShapingConeSoftness ) }
		};
	}
	else if ( PropertyPath == LightSourceAnglePropertyName )
	{
		return { UE::FUsdAttribute{ UsdPrim.GetAttribute( pxr::UsdLuxTokens->inputsAngle ) } };
	}

	return {};
}

#endif // #if USE_USD_SDK
