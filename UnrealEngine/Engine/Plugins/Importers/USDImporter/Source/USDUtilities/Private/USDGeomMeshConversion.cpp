// Copyright Epic Games, Inc. All Rights Reserved.

#if USE_USD_SDK

#include "USDGeomMeshConversion.h"

#include "UnrealUSDWrapper.h"
#include "USDAssetImportData.h"
#include "USDAttributeUtils.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDPrimConversion.h"
#include "USDShadeConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#endif // WITH_EDITOR

#include "USDIncludesStart.h"
	#include "pxr/usd/ar/resolver.h"
	#include "pxr/usd/ar/resolverScopedCache.h"
	#include "pxr/usd/sdf/layer.h"
	#include "pxr/usd/sdf/layerUtils.h"
	#include "pxr/usd/sdf/types.h"
	#include "pxr/usd/usd/editContext.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/primRange.h"
	#include "pxr/usd/usdGeom/mesh.h"
	#include "pxr/usd/usdGeom/pointInstancer.h"
	#include "pxr/usd/usdGeom/primvarsAPI.h"
	#include "pxr/usd/usdGeom/subset.h"
	#include "pxr/usd/usdGeom/tokens.h"
	#include "pxr/usd/usdShade/material.h"
	#include "pxr/usd/usdShade/materialBindingAPI.h"
	#include "pxr/usd/usdShade/tokens.h"
#include "USDIncludesEnd.h"

#define LOCTEXT_NAMESPACE "USDGeomMeshConversion"

static int32 GMaxInstancesPerPointInstancer = -1;
static FAutoConsoleVariableRef CVarMaxInstancesPerPointInstancer(
	TEXT( "USD.MaxInstancesPerPointInstancer" ),
	GMaxInstancesPerPointInstancer,
	TEXT( "We will only parse up to this many instances from any point instancer when reading from USD to UE. Set this to -1 to disable this limit." ) );

namespace UE::UsdGeomMeshConversion::Private
{
	static const FString DisplayColorID = TEXT( "!DisplayColor" );

	int32 GetPrimValueIndex( const pxr::TfToken& InterpType, const int32 VertexIndex, const int32 VertexInstanceIndex, const int32 PolygonIndex )
	{
		if ( InterpType == pxr::UsdGeomTokens->vertex )
		{
			return VertexIndex;
		}
		else if ( InterpType == pxr::UsdGeomTokens->varying )
		{
			return VertexIndex;
		}
		else if ( InterpType == pxr::UsdGeomTokens->faceVarying )
		{
			return VertexInstanceIndex;
		}
		else if ( InterpType == pxr::UsdGeomTokens->uniform )
		{
			return PolygonIndex;
		}
		else /* if ( InterpType == pxr::UsdGeomTokens->constant ) */
		{
			return 0; // return index 0 for constant or any other unsupported cases
		}
	}

	int32 GetLODIndexFromName( const std::string& Name )
	{
		const std::string LODString = UnrealIdentifiers::LOD.GetString();

		// True if Name does not start with "LOD"
		if ( Name.rfind( LODString, 0 ) != 0 )
		{
			return INDEX_NONE;
		}

		// After LODString there should be only numbers
		if ( Name.find_first_not_of( "0123456789", LODString.size() ) != std::string::npos )
		{
			return INDEX_NONE;
		}

		const int Base = 10;
		char** EndPtr = nullptr;
		return std::strtol( Name.c_str() + LODString.size(), EndPtr, Base );
	}

	void ConvertStaticMeshLOD(
		int32 LODIndex,
		const FStaticMeshLODResources& LODRenderMesh,
		pxr::UsdGeomMesh& UsdMesh,
		const pxr::VtArray< std::string >& MaterialAssignments,
		const pxr::UsdTimeCode TimeCode,
		pxr::UsdPrim MaterialPrim
	)
	{
		pxr::UsdPrim MeshPrim = UsdMesh.GetPrim();
		pxr::UsdStageRefPtr Stage = MeshPrim.GetStage();
		if ( !Stage )
		{
			return;
		}
		const FUsdStageInfo StageInfo{ Stage };

		// Vertices
		{
			const int32 VertexCount = LODRenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();

			// Points
			{
				pxr::UsdAttribute Points = UsdMesh.CreatePointsAttr();
				if ( Points )
				{
					pxr::VtArray< pxr::GfVec3f > PointsArray;
					PointsArray.reserve( VertexCount );

					for ( int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex )
					{
						FVector VertexPosition = (FVector)LODRenderMesh.VertexBuffers.PositionVertexBuffer.VertexPosition( VertexIndex );
						PointsArray.push_back( UnrealToUsd::ConvertVector( StageInfo, VertexPosition ) );
					}

					Points.Set( PointsArray, TimeCode );
				}
			}

			// Normals
			{
				// We need to emit this if we're writing normals (which we always are) because any DCC that can
				// actually subdivide (like usdview) will just discard authored normals and fully recompute them
				// on-demand in case they have a valid subdivision scheme (which is the default state).
				// Reference: https://graphics.pixar.com/usd/release/api/class_usd_geom_mesh.html#UsdGeom_Mesh_Normals
				if ( pxr::UsdAttribute SubdivisionAttr = UsdMesh.CreateSubdivisionSchemeAttr() )
				{
					ensure( SubdivisionAttr.Set( pxr::UsdGeomTokens->none ) );
				}

				pxr::UsdAttribute NormalsAttribute = UsdMesh.CreateNormalsAttr();
				if ( NormalsAttribute )
				{
					pxr::VtArray< pxr::GfVec3f > Normals;
					Normals.reserve( VertexCount );

					for ( int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex )
					{
						FVector VertexNormal = (FVector4)LODRenderMesh.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( VertexIndex );
						Normals.push_back( UnrealToUsd::ConvertVector( StageInfo, VertexNormal ) );
					}

					NormalsAttribute.Set( Normals, TimeCode );
				}
			}

			// UVs
			{
				const int32 TexCoordSourceCount = LODRenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();

				for ( int32 TexCoordSourceIndex = 0; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex )
				{
					pxr::TfToken UsdUVSetName = UsdUtils::GetUVSetName( TexCoordSourceIndex ).Get();

					pxr::UsdGeomPrimvar PrimvarST = pxr::UsdGeomPrimvarsAPI(MeshPrim).CreatePrimvar( UsdUVSetName, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->vertex );

					if ( PrimvarST )
					{
						pxr::VtVec2fArray UVs;

						for ( int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex )
						{
							FVector2D TexCoord = FVector2D(LODRenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( VertexIndex, TexCoordSourceIndex ));
							TexCoord[ 1 ] = 1.f - TexCoord[ 1 ];

							UVs.push_back( UnrealToUsd::ConvertVector( TexCoord ) );
						}

						PrimvarST.Set( UVs, TimeCode );
					}
				}
			}

			// Vertex colors
			if ( LODRenderMesh.bHasColorVertexData )
			{
				pxr::UsdGeomPrimvar DisplayColorPrimvar = UsdMesh.CreateDisplayColorPrimvar( pxr::UsdGeomTokens->vertex );
				pxr::UsdGeomPrimvar DisplayOpacityPrimvar = UsdMesh.CreateDisplayOpacityPrimvar( pxr::UsdGeomTokens->vertex );

				if ( DisplayColorPrimvar )
				{
					pxr::VtArray< pxr::GfVec3f > DisplayColors;
					DisplayColors.reserve( VertexCount );

					pxr::VtArray< float > DisplayOpacities;
					DisplayOpacities.reserve( VertexCount );

					for ( int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex )
					{
						const FColor& VertexColor = LODRenderMesh.VertexBuffers.ColorVertexBuffer.VertexColor( VertexIndex );

						pxr::GfVec4f Color = UnrealToUsd::ConvertColor( VertexColor );
						DisplayColors.push_back( pxr::GfVec3f( Color[ 0 ], Color[ 1 ], Color[ 2 ] ) );
						DisplayOpacities.push_back( Color[ 3 ] );
					}

					DisplayColorPrimvar.Set( DisplayColors, TimeCode );
					DisplayOpacityPrimvar.Set( DisplayOpacities, TimeCode );
				}
			}
		}

		// Faces
		{
			const int32 FaceCount = LODRenderMesh.GetNumTriangles();

			// Face Vertex Counts
			{
				pxr::UsdAttribute FaceCountsAttribute = UsdMesh.CreateFaceVertexCountsAttr();

				if ( FaceCountsAttribute )
				{
					pxr::VtArray< int > FaceVertexCounts;
					FaceVertexCounts.reserve( FaceCount );

					for ( int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex )
					{
						FaceVertexCounts.push_back( 3 );
					}

					FaceCountsAttribute.Set( FaceVertexCounts, TimeCode );
				}
			}

			// Face Vertex Indices
			{
				pxr::UsdAttribute FaceVertexIndicesAttribute = UsdMesh.GetFaceVertexIndicesAttr();

				if ( FaceVertexIndicesAttribute )
				{
					FIndexArrayView Indices = LODRenderMesh.IndexBuffer.GetArrayView();
					ensure( Indices.Num() == FaceCount * 3 );

					pxr::VtArray< int > FaceVertexIndices;
					FaceVertexIndices.reserve( FaceCount * 3 );

					for ( int32 Index = 0; Index < FaceCount * 3; ++Index )
					{
						FaceVertexIndices.push_back( Indices[ Index ] );
					}

					FaceVertexIndicesAttribute.Set( FaceVertexIndices, TimeCode );
				}
			}
		}

		// Material assignments
		{
			bool bHasUEMaterialAssignements = false;

			pxr::VtArray< std::string > UnrealMaterialsForLOD;
			for ( const FStaticMeshSection& Section : LODRenderMesh.Sections )
			{
				if ( Section.MaterialIndex >= 0 && Section.MaterialIndex < MaterialAssignments.size() )
				{
					UnrealMaterialsForLOD.push_back( MaterialAssignments[ Section.MaterialIndex ] );
					bHasUEMaterialAssignements = true;
				}
				else
				{
					// Keep unrealMaterials with the same number of elements as our MaterialIndices expect
					UnrealMaterialsForLOD.push_back( "" );
				}
			}

			// This LOD has a single material assignment, just add an unrealMaterials attribute to the mesh prim
			if ( bHasUEMaterialAssignements && UnrealMaterialsForLOD.size() == 1 )
			{
				if ( pxr::UsdAttribute UEMaterialsAttribute = MaterialPrim.CreateAttribute( UnrealIdentifiers::MaterialAssignment, pxr::SdfValueTypeNames->String ) )
				{
					UEMaterialsAttribute.Set( UnrealMaterialsForLOD[ 0 ] );
				}
			}
			// Multiple material assignments to the same LOD (and so the same mesh prim). Need to create a GeomSubset for each UE mesh section
			else if ( UnrealMaterialsForLOD.size() > 1 )
			{
				// Need to fetch all triangles of a section, and add their indices
				for ( int32 SectionIndex = 0; SectionIndex < LODRenderMesh.Sections.Num(); ++SectionIndex )
				{
					const FStaticMeshSection& Section = LODRenderMesh.Sections[ SectionIndex ];

					// Note that we will continue on even if we have no material assignment, so as to satisfy the "partition" family condition (below)
					std::string SectionMaterial;
					if ( Section.MaterialIndex >= 0 && Section.MaterialIndex < MaterialAssignments.size() )
					{
						SectionMaterial = MaterialAssignments[ Section.MaterialIndex ];
					}

					pxr::UsdPrim GeomSubsetPrim = Stage->DefinePrim(
						MeshPrim.GetPath().AppendPath( pxr::SdfPath( "Section" + std::to_string( SectionIndex ) ) ),
						UnrealToUsd::ConvertToken( TEXT( "GeomSubset" ) ).Get()
					);

					// MaterialPrim may be in another stage, so we may need another GeomSubset there
					pxr::UsdPrim MaterialGeomSubsetPrim = GeomSubsetPrim;
					if ( MaterialPrim.GetStage() != MeshPrim.GetStage() )
					{
						MaterialGeomSubsetPrim = MaterialPrim.GetStage()->OverridePrim(
							MaterialPrim.GetPath().AppendPath( pxr::SdfPath( "Section" + std::to_string( SectionIndex ) ) )
						);
					}

					pxr::UsdGeomSubset GeomSubsetSchema{ GeomSubsetPrim };

					// Element type attribute
					pxr::UsdAttribute ElementTypeAttr = GeomSubsetSchema.CreateElementTypeAttr();
					ElementTypeAttr.Set( pxr::UsdGeomTokens->face, TimeCode );

					// Indices attribute
					const uint32 TriangleCount = Section.NumTriangles;
					const uint32 FirstTriangleIndex = Section.FirstIndex / 3; // FirstIndex is the first *vertex* instance index
					FIndexArrayView VertexInstances = LODRenderMesh.IndexBuffer.GetArrayView();
					pxr::VtArray<int> IndicesAttrValue;
					for ( uint32 TriangleIndex = FirstTriangleIndex; TriangleIndex - FirstTriangleIndex < TriangleCount; ++TriangleIndex )
					{
						// Note that we add VertexInstances in sequence to the usda file for the faceVertexInstances attribute, which
						// also constitutes our triangle order
						IndicesAttrValue.push_back( static_cast< int >( TriangleIndex ) );
					}

					pxr::UsdAttribute IndicesAttr = GeomSubsetSchema.CreateIndicesAttr();
					IndicesAttr.Set( IndicesAttrValue, TimeCode );

					// Family name attribute
					pxr::UsdAttribute FamilyNameAttr = GeomSubsetSchema.CreateFamilyNameAttr();
					FamilyNameAttr.Set( pxr::UsdShadeTokens->materialBind, TimeCode );

					// Family type
					pxr::UsdGeomSubset::SetFamilyType( UsdMesh, pxr::UsdShadeTokens->materialBind, pxr::UsdGeomTokens->partition );

					// unrealMaterial attribute
					if ( pxr::UsdAttribute UEMaterialsAttribute = MaterialGeomSubsetPrim.CreateAttribute( UnrealIdentifiers::MaterialAssignment, pxr::SdfValueTypeNames->String ) )
					{
						UEMaterialsAttribute.Set( UnrealMaterialsForLOD[ SectionIndex ] );
					}
				}
			}
		}
	}

	bool ConvertMeshDescription( const FMeshDescription& MeshDescription, pxr::UsdGeomMesh& UsdMesh, const FMatrix& AdditionalTransform, const pxr::UsdTimeCode TimeCode )
	{
		pxr::UsdPrim MeshPrim = UsdMesh.GetPrim();
		pxr::UsdStageRefPtr Stage = MeshPrim.GetStage();
		if ( !Stage )
		{
			return false;
		}
		const FUsdStageInfo StageInfo{ Stage };

		FStaticMeshConstAttributes Attributes(MeshDescription);
		TVertexAttributesConstRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
		TPolygonGroupAttributesConstRef<FName> PolygonGroupImportedMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
		TVertexInstanceAttributesConstRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesConstRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesConstRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();

		const int32 VertexCount = VertexPositions.GetNumElements();
		const int32 VertexInstanceCount = VertexInstanceNormals.GetNumElements();
		const int32 FaceCount = MeshDescription.Polygons().Num();

		// Points
		{
			if ( pxr::UsdAttribute Points = UsdMesh.CreatePointsAttr() )
			{
				pxr::VtArray< pxr::GfVec3f > PointsArray;
				PointsArray.reserve( VertexCount );

				for ( const FVertexID VertexID : MeshDescription.Vertices().GetElementIDs() )
				{
					FVector UEPosition = AdditionalTransform.TransformPosition( (FVector)VertexPositions[ VertexID ] );
					PointsArray.push_back( UnrealToUsd::ConvertVector( StageInfo, UEPosition ) );
				}

				Points.Set( PointsArray, TimeCode );
			}
		}

		// Normals
		{
			if ( pxr::UsdAttribute NormalsAttribute = UsdMesh.CreateNormalsAttr() )
			{
				pxr::VtArray< pxr::GfVec3f > Normals;
				Normals.reserve( VertexInstanceCount );

				for ( const FVertexInstanceID InstanceID : MeshDescription.VertexInstances().GetElementIDs() )
				{
					FVector UENormal = (FVector)VertexInstanceNormals[ InstanceID ].GetSafeNormal();
					Normals.push_back( UnrealToUsd::ConvertVector( StageInfo, UENormal ) );
				}

				NormalsAttribute.Set( Normals, TimeCode );
				UsdMesh.SetNormalsInterpolation( pxr::UsdGeomTokens->faceVarying );
			}
		}

		// UVs
		{
			int32 NumUVs = VertexInstanceUVs.GetNumChannels();

			for ( int32 UVIndex = 0; UVIndex < NumUVs; ++UVIndex )
			{
				pxr::TfToken UsdUVSetName = UsdUtils::GetUVSetName( UVIndex ).Get();

				pxr::UsdGeomPrimvar PrimvarST = pxr::UsdGeomPrimvarsAPI(MeshPrim).CreatePrimvar( UsdUVSetName, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->vertex );
				if ( PrimvarST )
				{
					pxr::VtVec2fArray UVs;

					for ( const FVertexInstanceID InstanceID : MeshDescription.VertexInstances().GetElementIDs() )
					{
						FVector2D UV = FVector2D(VertexInstanceUVs.Get( InstanceID, UVIndex ));
						UV[ 1 ] = 1.f - UV[ 1 ];
						UVs.push_back( UnrealToUsd::ConvertVector( UV ) );
					}

					PrimvarST.Set( UVs, TimeCode );
					PrimvarST.SetInterpolation( pxr::UsdGeomTokens->faceVarying );
				}
			}
		}

		// Vertex colors
		if ( VertexInstanceColors.GetNumElements() > 0 )
		{
			pxr::UsdGeomPrimvar DisplayColorPrimvar = UsdMesh.CreateDisplayColorPrimvar( pxr::UsdGeomTokens->faceVarying );
			pxr::UsdGeomPrimvar DisplayOpacityPrimvar = UsdMesh.CreateDisplayOpacityPrimvar( pxr::UsdGeomTokens->faceVarying );
			if ( DisplayColorPrimvar && DisplayOpacityPrimvar )
			{
				pxr::VtArray< pxr::GfVec3f > DisplayColors;
				DisplayColors.reserve( VertexInstanceCount );

				pxr::VtArray< float > DisplayOpacities;
				DisplayOpacities.reserve( VertexInstanceCount );

				for ( const FVertexInstanceID InstanceID : MeshDescription.VertexInstances().GetElementIDs() )
				{
					pxr::GfVec4f Color = UnrealToUsd::ConvertColor( FLinearColor( VertexInstanceColors[ InstanceID ] ) );
					DisplayColors.push_back( pxr::GfVec3f( Color[ 0 ], Color[ 1 ], Color[ 2 ] ) );
					DisplayOpacities.push_back( Color[ 3 ] );
				}

				DisplayColorPrimvar.Set( DisplayColors, TimeCode );
				DisplayOpacityPrimvar.Set( DisplayOpacities, TimeCode );
			}
		}

		// Faces
		{
			pxr::UsdAttribute FaceCountsAttribute = UsdMesh.CreateFaceVertexCountsAttr();
			pxr::UsdAttribute FaceVertexIndicesAttribute = UsdMesh.GetFaceVertexIndicesAttr();

			pxr::VtArray< int > FaceVertexCounts;
			FaceVertexCounts.reserve( FaceCount );

			pxr::VtArray< int > FaceVertexIndices;

			for ( FPolygonID PolygonID : MeshDescription.Polygons().GetElementIDs() )
			{
				const TArray<FVertexInstanceID>& PolygonVertexInstances = MeshDescription.GetPolygonVertexInstances( PolygonID );
				FaceVertexCounts.push_back( static_cast< int >( PolygonVertexInstances.Num() ) );

				for ( FVertexInstanceID VertexInstanceID : PolygonVertexInstances )
				{
					int32 VertexIndex = MeshDescription.GetVertexInstanceVertex( VertexInstanceID ).GetValue();
					FaceVertexIndices.push_back( static_cast< int >( VertexIndex ) );
				}
			}

			FaceCountsAttribute.Set( FaceVertexCounts, TimeCode );
			FaceVertexIndicesAttribute.Set( FaceVertexIndices, TimeCode );
		}

		return true;
	}

	bool RecursivelyCollapseChildMeshes(
		const pxr::UsdPrim& Prim,
		FMeshDescription& OutMeshDescription,
		UsdUtils::FUsdPrimMaterialAssignmentInfo& OutMaterialAssignments,
		UsdToUnreal::FUsdMeshConversionOptions& Options,
		bool bIsFirstPrim
	)
	{
		// Ignore meshes from disabled purposes
		if ( !EnumHasAllFlags( Options.PurposesToLoad, IUsdPrim::GetPurpose( Prim ) ) )
		{
			return true;
		}

		FTransform ChildTransform = Options.AdditionalTransform;

		if ( !bIsFirstPrim )
		{
			// Ignore invisible child meshes.
			//
			// We used to compute visibility here and flat out ignore any invisible meshes. However, it could be that this mesh is invisible
			// due to the first prim (the parentmost prim of the recursive calls) being invisible. If the first is invisible but animated
			// then its possible it will become visible later, so if the child meshes are all invisible due to that fact alone then we should still
			// consider them. If the first is invisible but *not* animated then we should still consider it in the same way, because that's sort
			// of what you'd expect by calling ConvertGeomMeshHierarchy: We shouldn't just return nothing if the prim happens to be invisible.
			// Besides, it could be that first is invisible due to itself having a parent that is invisible but has visibility animations:
			// In that case we'd also want to generate meshes even if first is effectively invisible, since those parents can become visible
			// later as well. The only case left is if first is invisible due having parents that are invisible and not animated: Checking for
			// this would involve checking visibility and animations of all of its parents though, which is probably a bit too much, and like
			// in the case where first itself is invisible and not animated, the caller may still expect to receive a valid mesh even if the
			// prim's parents are invisible.
			//
			// The only case in which we'll truly discard invisible submeshes now is if they're invisible *by themselves*. If we're collapsing
			// them then we know they're not animated either, so they will basically never be visible at all, at any time code.
			//
			// Note that if we were to ever manually set any of these back to visible again via the editor, the visibility changes are
			// now resyncs and we'll reparse this entire asset, which will give us the chance to add them back to the collapsed mesh.
			if ( pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable( Prim ) )
			{
				if ( pxr::UsdAttribute VisibilityAttr = UsdGeomImageable.GetVisibilityAttr() )
				{
					pxr::TfToken VisibilityToken;
					if ( VisibilityAttr.Get( &VisibilityToken ) && VisibilityToken == pxr::UsdGeomTokens->invisible )
					{
						return true;
					}
				}
			}

			if ( pxr::UsdGeomXformable Xformable = pxr::UsdGeomXformable( Prim ) )
			{
				FTransform LocalChildTransform;
				UsdToUnreal::ConvertXformable( Prim.GetStage(), Xformable, LocalChildTransform, Options.TimeCode.GetValue() );

				ChildTransform = LocalChildTransform * Options.AdditionalTransform;
			}
		}

		bool bSuccess = true;
		bool bTraverseChildren = true;

		// Since ConvertGeomMesh and ConvertPointInstancerToMesh take the Options object by const ref and we traverse
		// children afterwards, its fine to overwrite Options.AdditionalTransform. We do have to put it back to our
		// original value after we're done though, as calls to sibling prims that would run after this call would need
		// the original AdditionalTransform in place. The alternative is to copy the entire options object...
		TGuardValue<FTransform> Guard{ Options.AdditionalTransform, ChildTransform };

		if ( pxr::UsdGeomMesh Mesh = pxr::UsdGeomMesh( Prim ) )
		{
			bSuccess = UsdToUnreal::ConvertGeomMesh( Mesh, OutMeshDescription, OutMaterialAssignments, Options );
		}
		else if ( pxr::UsdGeomPointInstancer PointInstancer = pxr::UsdGeomPointInstancer{ Prim } )
		{
			bSuccess = UsdToUnreal::ConvertPointInstancerToMesh(
				PointInstancer,
				OutMeshDescription,
				OutMaterialAssignments,
				Options
			);

			// We never want to step into point instancers when fetching prims for drawing
			bTraverseChildren = false;
		}

		if ( bTraverseChildren )
		{
			for ( const pxr::UsdPrim& ChildPrim : Prim.GetFilteredChildren( pxr::UsdTraverseInstanceProxies() ) )
			{
				if ( !bSuccess )
				{
					break;
				}

				const bool bChildIsFirstPrim = false;

				bSuccess &= RecursivelyCollapseChildMeshes(
					ChildPrim,
					OutMeshDescription,
					OutMaterialAssignments,
					Options,
					bChildIsFirstPrim
				);
			}
		}

		return bSuccess;
	}
}
namespace UsdGeomMeshImpl = UE::UsdGeomMeshConversion::Private;

namespace UsdToUnreal
{
	const FUsdMeshConversionOptions FUsdMeshConversionOptions::DefaultOptions;

	FUsdMeshConversionOptions::FUsdMeshConversionOptions()
		: AdditionalTransform( FTransform::Identity )
		, PurposesToLoad( EUsdPurpose::Render )
		, RenderContext( pxr::UsdShadeTokens->universalRenderContext )
		, MaterialPurpose( pxr::UsdShadeTokens->allPurpose )
		, TimeCode( pxr::UsdTimeCode::EarliestTime() )
		, MaterialToPrimvarToUVIndex( nullptr )
		, bMergeIdenticalMaterialSlots( true )
	{
	}
}

bool UsdToUnreal::ConvertGeomMesh(
	const pxr::UsdGeomMesh& UsdMesh,
	FMeshDescription& OutMeshDescription,
	UsdUtils::FUsdPrimMaterialAssignmentInfo& OutMaterialAssignments,
	const FUsdMeshConversionOptions& Options
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE( UsdToUnreal::ConvertGeomMesh );

	if ( !UsdMesh )
	{
		return false;
	}

	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdPrim = UsdMesh.GetPrim();
	pxr::UsdStageRefPtr Stage = UsdPrim.GetStage();
	const FUsdStageInfo StageInfo( Stage );

	const double TimeCodeValue = Options.TimeCode.GetValue();

	const int32 MaterialIndexOffset = OutMaterialAssignments.Slots.Num();

	// Material assignments
	const bool bProvideMaterialIndices = true;
	UsdUtils::FUsdPrimMaterialAssignmentInfo LocalInfo = UsdUtils::GetPrimMaterialAssignments(
		UsdPrim,
		Options.TimeCode,
		bProvideMaterialIndices,
		Options.RenderContext,
		Options.MaterialPurpose
	);
	TArray< UsdUtils::FUsdPrimMaterialSlot >& LocalMaterialSlots = LocalInfo.Slots;
	TArray< int32 >& FaceMaterialIndices = LocalInfo.MaterialIndices;

	// Position 3 in this has the value 6 --> Local material slot #3 is actually the combined material slot #6
	TArray<int32> LocalToCombinedMaterialSlotIndices;
	LocalToCombinedMaterialSlotIndices.SetNumZeroed( LocalInfo.Slots.Num() );

	if ( Options.bMergeIdenticalMaterialSlots )
	{
		// Build a map of our existing slots since we can hash the entire slot, and our incoming mesh may have an arbitrary number of new slots
		TMap< UsdUtils::FUsdPrimMaterialSlot, int32 > CombinedMaterialSlotsToIndex;
		for ( int32 Index = 0; Index < OutMaterialAssignments.Slots.Num(); ++Index )
		{
			const UsdUtils::FUsdPrimMaterialSlot& Slot = OutMaterialAssignments.Slots[ Index ];
			CombinedMaterialSlotsToIndex.Add( Slot, Index );
		}

		for ( int32 LocalIndex = 0; LocalIndex < LocalInfo.Slots.Num(); ++LocalIndex )
		{
			const UsdUtils::FUsdPrimMaterialSlot& LocalSlot = LocalInfo.Slots[ LocalIndex ];
			if ( int32* ExistingCombinedIndex = CombinedMaterialSlotsToIndex.Find( LocalSlot ) )
			{
				LocalToCombinedMaterialSlotIndices[ LocalIndex ] = *ExistingCombinedIndex;
			}
			else
			{
				OutMaterialAssignments.Slots.Add( LocalSlot );
				LocalToCombinedMaterialSlotIndices[ LocalIndex ] = OutMaterialAssignments.Slots.Num() - 1;
			}
		}
	}
	else
	{
		// Just append our new local material slots at the end of MaterialAssignments
		OutMaterialAssignments.Slots.Append( LocalInfo.Slots );
		for ( int32 LocalIndex = 0; LocalIndex < LocalInfo.Slots.Num(); ++LocalIndex )
		{
			LocalToCombinedMaterialSlotIndices[ LocalIndex ] = LocalIndex + MaterialIndexOffset;
		}
	}

	const int32 VertexOffset = OutMeshDescription.Vertices().Num();
	const int32 VertexInstanceOffset = OutMeshDescription.VertexInstances().Num();
	const int32 PolygonOffset = OutMeshDescription.Polygons().Num();

	FStaticMeshAttributes StaticMeshAttributes( OutMeshDescription );

	// Vertex positions
	TVertexAttributesRef< FVector3f > MeshDescriptionVertexPositions = StaticMeshAttributes.GetVertexPositions();
	{
		pxr::UsdAttribute Points = UsdMesh.GetPointsAttr();
		if ( Points )
		{
			pxr::VtArray< pxr::GfVec3f > PointsArray;
			Points.Get( &PointsArray, TimeCodeValue );

			OutMeshDescription.ReserveNewVertices( PointsArray.size() );

			for ( int32 LocalPointIndex = 0; LocalPointIndex < PointsArray.size(); ++LocalPointIndex )
			{
				const pxr::GfVec3f& Point = PointsArray[ LocalPointIndex ];

				FVector Position = Options.AdditionalTransform.TransformPosition( UsdToUnreal::ConvertVector( StageInfo, Point ) );

				FVertexID AddedVertexId = OutMeshDescription.CreateVertex();
				MeshDescriptionVertexPositions[ AddedVertexId ] = ( FVector3f ) Position;
			}
		}
	}

	uint32 NumSkippedPolygons = 0;
	uint32 NumPolygons = 0;

	// Polygons
	{
		TMap<int32, FPolygonGroupID> PolygonGroupMapping;
		TArray<FVertexInstanceID> CornerInstanceIDs;
		TArray<FVertexID> CornerVerticesIDs;
		int32 CurrentVertexInstanceIndex = 0;
		TPolygonGroupAttributesRef<FName> MaterialSlotNames = StaticMeshAttributes.GetPolygonGroupMaterialSlotNames();

		// If we're going to share existing material slots, we'll need to share existing PolygonGroups in the mesh description,
		// so we need to traverse it and prefill PolygonGroupMapping
		if ( Options.bMergeIdenticalMaterialSlots )
		{
			for ( FPolygonGroupID PolygonGroupID : OutMeshDescription.PolygonGroups().GetElementIDs() )
			{
				// We always create our polygon groups in order with our combined material slots, so its easy to reconstruct this mapping here
				PolygonGroupMapping.Add( PolygonGroupID.GetValue(), PolygonGroupID );
			}
		}

		// Material slots
		// Note that we always create these in the order they show up in LocalInfo: If we created these on-demand when parsing polygons (like before)
		// we could run into polygons in a different order than the material slots and end up with different material assignments.
		// We could use the StaticMesh's SectionInfoMap to unswitch things, but that's not available at runtime so we better do this here
		for ( int32 LocalMaterialIndex = 0; LocalMaterialIndex < LocalInfo.Slots.Num(); ++LocalMaterialIndex )
		{
			const int32 CombinedMaterialIndex = LocalToCombinedMaterialSlotIndices[ LocalMaterialIndex ];
			if ( !PolygonGroupMapping.Contains( CombinedMaterialIndex ) )
			{
				FPolygonGroupID NewPolygonGroup = OutMeshDescription.CreatePolygonGroup();
				PolygonGroupMapping.Add( CombinedMaterialIndex, NewPolygonGroup );

				// This is important for runtime, where the material slots are matched to LOD sections based on their material slot name
				MaterialSlotNames[ NewPolygonGroup ] = *LexToString( NewPolygonGroup.GetValue() );
			}
		}

		bool bFlipThisGeometry = false;

		if ( IUsdPrim::GetGeometryOrientation( UsdMesh ) == EUsdGeomOrientation::LeftHanded )
		{
			bFlipThisGeometry = !bFlipThisGeometry;
		}

		// Face counts
		pxr::UsdAttribute FaceCountsAttribute = UsdMesh.GetFaceVertexCountsAttr();
		pxr::VtArray< int > FaceCounts;

		if ( FaceCountsAttribute )
		{
			FaceCountsAttribute.Get( &FaceCounts, TimeCodeValue );
			NumPolygons = FaceCounts.size();
		}

		// Face indices
		pxr::UsdAttribute FaceIndicesAttribute = UsdMesh.GetFaceVertexIndicesAttr();
		pxr::VtArray< int > FaceIndices;

		if ( FaceIndicesAttribute )
		{
			FaceIndicesAttribute.Get( &FaceIndices, TimeCodeValue );
		}

		// Normals
		pxr::UsdAttribute NormalsAttribute = UsdMesh.GetNormalsAttr();
		pxr::VtArray< pxr::GfVec3f > Normals;

		if ( NormalsAttribute )
		{
			NormalsAttribute.Get( &Normals, TimeCodeValue );
		}

		pxr::TfToken NormalsInterpType = UsdMesh.GetNormalsInterpolation();

		// UVs
		TVertexInstanceAttributesRef< FVector2f > MeshDescriptionUVs = StaticMeshAttributes.GetVertexInstanceUVs();

		struct FUVSet
		{
			int32 UVSetIndexUE; // The user may only have 'uv4' and 'uv5', so we can't just use array indices to find the target UV channel
			TOptional< pxr::VtIntArray > UVIndices; // UVs might be indexed or they might be flat (one per vertex)
			pxr::VtVec2fArray UVs;

			pxr::TfToken InterpType = pxr::UsdGeomTokens->faceVarying;
		};

		TArray< FUVSet > UVSets;

		TArray< TUsdStore< pxr::UsdGeomPrimvar > > PrimvarsByUVIndex = UsdUtils::GetUVSetPrimvars( UsdMesh, *Options.MaterialToPrimvarToUVIndex, LocalInfo );

		int32 HighestAddedUVChannel = 0;
		for ( int32 UVChannelIndex = 0; UVChannelIndex < PrimvarsByUVIndex.Num(); ++UVChannelIndex )
		{
			if ( !PrimvarsByUVIndex.IsValidIndex( UVChannelIndex ) )
			{
				break;
			}

			pxr::UsdGeomPrimvar& Primvar = PrimvarsByUVIndex[ UVChannelIndex ].Get();
			if ( !Primvar )
			{
				// The user may have name their UV sets 'uv4' and 'uv5', in which case we have no UV sets below 4, so just skip them
				continue;
			}

			FUVSet UVSet;
			UVSet.InterpType = Primvar.GetInterpolation();
			UVSet.UVSetIndexUE = UVChannelIndex;

			if ( Primvar.IsIndexed() )
			{
				UVSet.UVIndices.Emplace();

				if ( Primvar.GetIndices( &UVSet.UVIndices.GetValue(), Options.TimeCode ) && Primvar.Get( &UVSet.UVs, Options.TimeCode ) )
				{
					if ( UVSet.UVs.size() > 0 )
					{
						UVSets.Add( MoveTemp( UVSet ) );
						HighestAddedUVChannel = UVSet.UVSetIndexUE;
					}
				}
			}
			else
			{
				if ( Primvar.Get( &UVSet.UVs ) )
				{
					if ( UVSet.UVs.size() > 0 )
					{
						UVSets.Add( MoveTemp( UVSet ) );
						HighestAddedUVChannel = UVSet.UVSetIndexUE;
					}
				}
			}
		}

		// When importing multiple mesh pieces to the same static mesh.  Ensure each mesh piece has the same number of Uv's
		{
			int32 ExistingUVCount = MeshDescriptionUVs.GetNumChannels();
			int32 NumUVs = FMath::Max( HighestAddedUVChannel + 1, ExistingUVCount );
			NumUVs = FMath::Min<int32>( MAX_MESH_TEXTURE_COORDS_MD, NumUVs );
			// At least one UV set must exist.
			NumUVs = FMath::Max<int32>( 1, NumUVs );

			//Make sure all Vertex instance have the correct number of UVs
			MeshDescriptionUVs.SetNumChannels( NumUVs );
		}

		TVertexInstanceAttributesRef< FVector3f > MeshDescriptionNormals = StaticMeshAttributes.GetVertexInstanceNormals();

		OutMeshDescription.ReserveNewVertexInstances( FaceCounts.size() * 3 );
		OutMeshDescription.ReserveNewPolygons( FaceCounts.size() );
		OutMeshDescription.ReserveNewEdges( FaceCounts.size() * 2 );

		// Vertex color
		TVertexInstanceAttributesRef< FVector4f > MeshDescriptionColors = StaticMeshAttributes.GetVertexInstanceColors();

		pxr::UsdGeomPrimvar ColorPrimvar = UsdMesh.GetDisplayColorPrimvar();
		pxr::TfToken ColorInterpolation = pxr::UsdGeomTokens->constant;
		pxr::VtArray< pxr::GfVec3f > UsdColors;

		if ( ColorPrimvar )
		{
			ColorPrimvar.ComputeFlattened( &UsdColors, Options.TimeCode );
			ColorInterpolation = ColorPrimvar.GetInterpolation();
		}

		// Vertex opacity
		pxr::UsdGeomPrimvar OpacityPrimvar = UsdMesh.GetDisplayOpacityPrimvar();
		pxr::TfToken OpacityInterpolation = pxr::UsdGeomTokens->constant;
		pxr::VtArray< float > UsdOpacities;

		if ( OpacityPrimvar )
		{
			OpacityPrimvar.ComputeFlattened( &UsdOpacities );
			OpacityInterpolation = OpacityPrimvar.GetInterpolation();
		}

		for ( int32 PolygonIndex = 0; PolygonIndex < FaceCounts.size(); ++PolygonIndex )
		{
			int32 PolygonVertexCount = FaceCounts[ PolygonIndex ];
			CornerInstanceIDs.Reset( PolygonVertexCount );
			CornerVerticesIDs.Reset( PolygonVertexCount );

			for ( int32 CornerIndex = 0; CornerIndex < PolygonVertexCount; ++CornerIndex, ++CurrentVertexInstanceIndex )
			{
				int32 VertexInstanceIndex = VertexInstanceOffset + CurrentVertexInstanceIndex;
				const FVertexInstanceID VertexInstanceID( VertexInstanceIndex );
				const int32 ControlPointIndex = FaceIndices[ CurrentVertexInstanceIndex ];
				const FVertexID VertexID( VertexOffset + ControlPointIndex );
				const FVector VertexPosition = ( FVector ) MeshDescriptionVertexPositions[ VertexID ];

				// Make sure a face doesn't use the same vertex twice as MeshDescription doesn't like that
				if ( CornerVerticesIDs.Contains( VertexID ) )
				{
					continue;
				}

				CornerVerticesIDs.Add( VertexID );

				FVertexInstanceID AddedVertexInstanceId = OutMeshDescription.CreateVertexInstance( VertexID );
				CornerInstanceIDs.Add( AddedVertexInstanceId );

				if ( Normals.size() > 0 )
				{
					const int32 NormalIndex = UsdGeomMeshImpl::GetPrimValueIndex( NormalsInterpType, ControlPointIndex, CurrentVertexInstanceIndex, PolygonIndex );

					if ( NormalIndex < Normals.size() )
					{
						const pxr::GfVec3f& Normal = Normals[ NormalIndex ];
						FVector TransformedNormal = Options.AdditionalTransform.TransformVector( UsdToUnreal::ConvertVector( StageInfo, Normal ) ).GetSafeNormal();

						MeshDescriptionNormals[ AddedVertexInstanceId ] = ( FVector3f ) TransformedNormal.GetSafeNormal();
					}
				}

				for ( const FUVSet& UVSet : UVSets )
				{
					const int32 ValueIndex = UsdGeomMeshImpl::GetPrimValueIndex( UVSet.InterpType, ControlPointIndex, CurrentVertexInstanceIndex, PolygonIndex );

					pxr::GfVec2f UV( 0.f, 0.f );

					if ( UVSet.UVIndices.IsSet() )
					{
						if ( ensure( UVSet.UVIndices.GetValue().size() > ValueIndex ) )
						{
							UV = UVSet.UVs[ UVSet.UVIndices.GetValue()[ ValueIndex ] ];
						}
					}
					else if ( ensure( UVSet.UVs.size() > ValueIndex ) )
					{
						UV = UVSet.UVs[ ValueIndex ];
					}

					// Flip V for Unreal uv's which match directx
					FVector2f FinalUVVector( UV[ 0 ], 1.f - UV[ 1 ] );
					MeshDescriptionUVs.Set( AddedVertexInstanceId, UVSet.UVSetIndexUE, FinalUVVector );
				}

				// Vertex color
				{
					const int32 ValueIndex = UsdGeomMeshImpl::GetPrimValueIndex( ColorInterpolation, ControlPointIndex, CurrentVertexInstanceIndex, PolygonIndex );

					pxr::GfVec3f UsdColor( 1.f, 1.f, 1.f );

					if ( !UsdColors.empty() && ensure( UsdColors.size() > ValueIndex ) )
					{
						UsdColor = UsdColors[ ValueIndex ];
					}

					MeshDescriptionColors[ AddedVertexInstanceId ] = UsdToUnreal::ConvertColor( UsdColor );
				}

				// Vertex opacity
				{
					const int32 ValueIndex = UsdGeomMeshImpl::GetPrimValueIndex( OpacityInterpolation, ControlPointIndex, CurrentVertexInstanceIndex, PolygonIndex );

					if ( !UsdOpacities.empty() && ensure( UsdOpacities.size() > ValueIndex ) )
					{
						MeshDescriptionColors[ AddedVertexInstanceId ][ 3 ] = UsdOpacities[ ValueIndex ];
					}
				}
			}

			// This polygon was using the same vertex instance more than once and we removed too many
			// vertex indices, so now we're forced to skip the whole polygon. We'll show a warning about it though
			if ( CornerVerticesIDs.Num() < 3 )
			{
				++NumSkippedPolygons;
				continue;
			}

			// Polygon groups
			int32 LocalMaterialIndex = 0;
			if ( FaceMaterialIndices.IsValidIndex( PolygonIndex ) )
			{
				LocalMaterialIndex = FaceMaterialIndices[ PolygonIndex ];
				if ( !LocalMaterialSlots.IsValidIndex( LocalMaterialIndex ) )
				{
					LocalMaterialIndex = 0;
				}
			}

			const int32 CombinedMaterialIndex = LocalToCombinedMaterialSlotIndices[ LocalMaterialIndex ];

			if ( bFlipThisGeometry )
			{
				for ( int32 i = 0; i < CornerInstanceIDs.Num() / 2; ++i )
				{
					Swap( CornerInstanceIDs[ i ], CornerInstanceIDs[ CornerInstanceIDs.Num() - i - 1 ] );
				}
			}
			// Insert a polygon into the mesh
			FPolygonGroupID PolygonGroupID = PolygonGroupMapping[ CombinedMaterialIndex ];
			const FPolygonID NewPolygonID = OutMeshDescription.CreatePolygon( PolygonGroupID, CornerInstanceIDs );
		}
	}

	if ( NumPolygons > 0 && NumSkippedPolygons > 0 )
	{
		UE_LOG( LogUsd, Warning, TEXT( "Skipped %d out of %d faces when parsing the mesh for prim '%s', as those faces contained too many repeated vertex indices" ),
			NumSkippedPolygons,
			NumPolygons,
			*UsdToUnreal::ConvertPath( UsdPrim.GetPath() )
		);
	}

	return true;
}

// Deprecated
bool UsdToUnreal::ConvertGeomMesh(
	const pxr::UsdTyped& UsdSchema,
	FMeshDescription& MeshDescription,
	UsdUtils::FUsdPrimMaterialAssignmentInfo& MaterialAssignments,
	const pxr::UsdTimeCode TimeCode,
	const pxr::TfToken& RenderContext
)
{
	FUsdMeshConversionOptions Options;
	Options.TimeCode = TimeCode;
	Options.RenderContext = RenderContext;

	return ConvertGeomMesh( pxr::UsdGeomMesh{ UsdSchema }, MeshDescription, MaterialAssignments, Options );
}

// Deprecated
bool UsdToUnreal::ConvertGeomMesh(
	const pxr::UsdTyped& UsdSchema,
	FMeshDescription& MeshDescription,
	UsdUtils::FUsdPrimMaterialAssignmentInfo& MaterialAssignments,
	const FTransform& AdditionalTransform,
	const pxr::UsdTimeCode TimeCode,
	const pxr::TfToken& RenderContext
)
{
	FUsdMeshConversionOptions Options;
	Options.AdditionalTransform = AdditionalTransform;
	Options.TimeCode = TimeCode;
	Options.RenderContext = RenderContext;

	return ConvertGeomMesh( pxr::UsdGeomMesh{ UsdSchema }, MeshDescription, MaterialAssignments, Options );
}

// Deprecated
bool UsdToUnreal::ConvertGeomMesh(
	const pxr::UsdTyped& UsdSchema,
	FMeshDescription& MeshDescription,
	UsdUtils::FUsdPrimMaterialAssignmentInfo& MaterialAssignments,
	const FTransform& AdditionalTransform,
	const TMap< FString, TMap< FString, int32 > >& MaterialToPrimvarsUVSetNames,
	const pxr::UsdTimeCode TimeCode,
	const pxr::TfToken& RenderContext,
	bool bMergeIdenticalMaterialSlots
)
{
	FUsdMeshConversionOptions Options;
	Options.AdditionalTransform = AdditionalTransform;
	Options.MaterialToPrimvarToUVIndex = &MaterialToPrimvarsUVSetNames;
	Options.TimeCode = TimeCode;
	Options.RenderContext = RenderContext;
	Options.bMergeIdenticalMaterialSlots = bMergeIdenticalMaterialSlots;

	return ConvertGeomMesh( pxr::UsdGeomMesh{ UsdSchema }, MeshDescription, MaterialAssignments, Options );
}

bool UsdToUnreal::ConvertPointInstancerToMesh(
	const pxr::UsdGeomPointInstancer& PointInstancer,
	FMeshDescription& OutMeshDescription,
	UsdUtils::FUsdPrimMaterialAssignmentInfo& OutMaterialAssignments,
	const FUsdMeshConversionOptions& Options
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE( UsdToUnreal::ConvertPointInstancerToMesh );

	if ( !PointInstancer )
	{
		return false;
	}

	FScopedUsdAllocs Allocs;

	pxr::UsdStageRefPtr Stage = PointInstancer.GetPrim().GetStage();
	if ( !Stage )
	{
		return false;
	}

	// Bake each prototype to a single mesh description and material assignment struct
	TArray<FMeshDescription> PrototypeMeshDescriptions;
	TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo> PrototypeMaterialAssignments;
	TArray<TMap<FPolygonGroupID, FPolygonGroupID>> PrototypePolygonGroupRemapping;
	uint32 NumPrototypes = 0;
	{
		const pxr::UsdRelationship& Prototypes = PointInstancer.GetPrototypesRel();

		pxr::SdfPathVector PrototypePaths;
		if ( !Prototypes.GetTargets( &PrototypePaths ) )
		{
			return false;
		}

		NumPrototypes = PrototypePaths.size();
		if ( NumPrototypes == 0 )
		{
			return true;
		}

		PrototypeMeshDescriptions.SetNum( NumPrototypes );
		PrototypeMaterialAssignments.SetNum( NumPrototypes );
		PrototypePolygonGroupRemapping.SetNum( NumPrototypes );

		// Our AdditionalTransform should be applied after even the instance transforms, we don't want to apply it
		// directly to our prototypes
		FUsdMeshConversionOptions OptionsCopy = Options;
		OptionsCopy.AdditionalTransform = FTransform::Identity;

		for ( uint32 PrototypeIndex = 0; PrototypeIndex < NumPrototypes; ++PrototypeIndex )
		{
			const pxr::SdfPath& PrototypePath = PrototypePaths[ PrototypeIndex ];

			pxr::UsdPrim PrototypeUsdPrim = Stage->GetPrimAtPath( PrototypePath );
			if ( !PrototypeUsdPrim )
			{
				UE_LOG( LogUsd, Warning, TEXT( "Failed to find prototype '%s' for PointInstancer '%s' within ConvertPointInstancerToMesh" ),
					*UsdToUnreal::ConvertPath( PrototypePath ),
					*UsdToUnreal::ConvertPath( PointInstancer.GetPrim().GetPrimPath() )
				);
				continue;
			}

			const bool bSkipRootPrimTransformAndVisibility = false;
			ConvertGeomMeshHierarchy(
				PrototypeUsdPrim,
				PrototypeMeshDescriptions[ PrototypeIndex ],
				PrototypeMaterialAssignments[ PrototypeIndex ],
				OptionsCopy,
				bSkipRootPrimTransformAndVisibility
			);
		}
	}

	// Handle combined prototype material slots.
	// Sets up PrototypePolygonGroupRemapping so that our new faces are remapped from the prototype's mesh description polygon groups to the
	// combined mesh description's polygon groups when AppendMeshDescription is called.
	// Note: We always setup our mesh description polygon groups in the same order as the material assignment slots, so this is not so complicated
	for ( uint32 PrototypeIndex = 0; PrototypeIndex < NumPrototypes; ++PrototypeIndex )
	{
		UsdUtils::FUsdPrimMaterialAssignmentInfo& PrototypeMaterialAssignment = PrototypeMaterialAssignments[ PrototypeIndex ];
		TMap<FPolygonGroupID, FPolygonGroupID>& PrototypeToCombinedMeshPolygonGroupMap = PrototypePolygonGroupRemapping[ PrototypeIndex ];

		if ( Options.bMergeIdenticalMaterialSlots )
		{
			// Build a map of our existing slots since we can hash the entire slot, and our incoming mesh may have an arbitrary number of new slots
			TMap< UsdUtils::FUsdPrimMaterialSlot, int32 > CombinedMaterialSlotsToIndex;
			for ( int32 Index = 0; Index < OutMaterialAssignments.Slots.Num(); ++Index )
			{
				const UsdUtils::FUsdPrimMaterialSlot& Slot = OutMaterialAssignments.Slots[ Index ];
				CombinedMaterialSlotsToIndex.Add( Slot, Index );
			}

			for ( int32 PrototypeMaterialSlotIndex = 0; PrototypeMaterialSlotIndex < PrototypeMaterialAssignment.Slots.Num(); ++PrototypeMaterialSlotIndex )
			{
				const UsdUtils::FUsdPrimMaterialSlot& LocalSlot = PrototypeMaterialAssignment.Slots[ PrototypeMaterialSlotIndex ];
				if ( int32* ExistingCombinedIndex = CombinedMaterialSlotsToIndex.Find( LocalSlot ) )
				{
					PrototypeToCombinedMeshPolygonGroupMap.Add( PrototypeMaterialSlotIndex, *ExistingCombinedIndex );
				}
				else
				{
					OutMaterialAssignments.Slots.Add( LocalSlot );
					PrototypeToCombinedMeshPolygonGroupMap.Add( PrototypeMaterialSlotIndex, OutMaterialAssignments.Slots.Num() - 1 );
				}
			}
		}
		else
		{
			const int32 NumExistingMaterialSlots = OutMaterialAssignments.Slots.Num();
			OutMaterialAssignments.Slots.Append( PrototypeMaterialAssignment.Slots );

			for ( int32 PrototypeMaterialSlotIndex = 0; PrototypeMaterialSlotIndex < PrototypeMaterialAssignment.Slots.Num(); ++PrototypeMaterialSlotIndex )
			{
				PrototypeToCombinedMeshPolygonGroupMap.Add( PrototypeMaterialSlotIndex, NumExistingMaterialSlots + PrototypeMaterialSlotIndex );
			}
		}
	}

	// Make sure we have the polygon groups we expect. Appending the mesh descriptions will not create new polygon groups if we're using a
	// PolygonGroupsDelegate, which we will
	const int32 NumExistingPolygonGroups = OutMeshDescription.PolygonGroups().Num();
	for ( int32 NumMissingPolygonGroups = OutMaterialAssignments.Slots.Num() - NumExistingPolygonGroups; NumMissingPolygonGroups > 0; --NumMissingPolygonGroups )
	{
		OutMeshDescription.CreatePolygonGroup();
	}

	// Double-check our target mesh description has the attributes we need
	FStaticMeshAttributes StaticMeshAttributes( OutMeshDescription );
	StaticMeshAttributes.Register();

	// Append mesh descriptions
	FUsdStageInfo StageInfo{ PointInstancer.GetPrim().GetStage() };
	for ( uint32 PrototypeIndex = 0; PrototypeIndex < NumPrototypes; ++PrototypeIndex )
	{
		const FMeshDescription& PrototypeMeshDescription = PrototypeMeshDescriptions[ PrototypeIndex ];
		UsdUtils::FUsdPrimMaterialAssignmentInfo& PrototypeMaterialAssignment = PrototypeMaterialAssignments[ PrototypeIndex ];

		// We may generate some empty meshes in case a prototype is invisible, for example
		if ( PrototypeMeshDescription.IsEmpty() )
		{
			continue;
		}

		TArray<FTransform> InstanceTransforms;
		bool bSuccess = UsdUtils::GetPointInstancerTransforms( StageInfo, PointInstancer, PrototypeIndex, Options.TimeCode, InstanceTransforms );
		if ( !bSuccess )
		{
			UE_LOG( LogUsd, Error, TEXT( "Failed to retrieve point instancer transforms for prototype index '%u' of point instancer '%s'" ),
				PrototypeIndex,
				*UsdToUnreal::ConvertPath( PointInstancer.GetPrim().GetPrimPath() )
			);

			continue;
		}

		const int32 NumInstances = InstanceTransforms.Num();

		OutMeshDescription.ReserveNewVertices( PrototypeMeshDescription.Vertices().Num() * NumInstances );
		OutMeshDescription.ReserveNewVertexInstances( PrototypeMeshDescription.VertexInstances().Num() * NumInstances );
		OutMeshDescription.ReserveNewEdges( PrototypeMeshDescription.Edges().Num() * NumInstances );
		OutMeshDescription.ReserveNewTriangles( PrototypeMeshDescription.Triangles().Num() * NumInstances );

		FStaticMeshOperations::FAppendSettings Settings;
		Settings.PolygonGroupsDelegate = FAppendPolygonGroupsDelegate::CreateLambda(
			[&PrototypePolygonGroupRemapping, PrototypeIndex]( const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, PolygonGroupMap& RemapPolygonGroups )
			{
				RemapPolygonGroups = PrototypePolygonGroupRemapping[ PrototypeIndex ];
			}
		);

		// TODO: Maybe we should make a new overload of AppendMeshDescriptions that can do this more efficiently, since all we need is to change the
		// transform repeatedly?
		for ( const FTransform& Transform : InstanceTransforms )
		{
			Settings.MeshTransform = Transform * Options.AdditionalTransform;
			FStaticMeshOperations::AppendMeshDescription( PrototypeMeshDescription, OutMeshDescription, Settings );
		}
	}

	return true;
}

bool UsdToUnreal::ConvertGeomMeshHierarchy(
	const pxr::UsdPrim& Prim,
	FMeshDescription& OutMeshDescription,
	UsdUtils::FUsdPrimMaterialAssignmentInfo& OutMaterialAssignments,
	const FUsdMeshConversionOptions& Options,
	bool bSkipRootPrimTransformAndVisibility
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE( UsdToUnreal::ConvertGeomMeshHierarchy );

	if ( !Prim )
	{
		return false;
	}

	FStaticMeshAttributes StaticMeshAttributes( OutMeshDescription );
	StaticMeshAttributes.Register();

	// Pass a copy down so that we can repeatedly overwrite the AdditionalTransform and still
	// provide the options object to ConvertGeomMesh and ConvertPointInstancerToMesh
	FUsdMeshConversionOptions OptionsCopy = Options;

	return UsdGeomMeshImpl::RecursivelyCollapseChildMeshes(
		Prim,
		OutMeshDescription,
		OutMaterialAssignments,
		OptionsCopy,
		bSkipRootPrimTransformAndVisibility
	);
}

// Deprecated
bool UsdToUnreal::ConvertGeomMeshHierarchy(
	const pxr::UsdPrim& Prim,
	const pxr::UsdTimeCode& TimeCode,
	const EUsdPurpose PurposesToLoad,
	const pxr::TfToken& RenderContext,
	const TMap< FString, TMap<FString, int32> >& MaterialToPrimvarToUVIndex,
	FMeshDescription& OutMeshDescription,
	UsdUtils::FUsdPrimMaterialAssignmentInfo& OutMaterialAssignments,
	bool bSkipRootPrimTransformAndVisibility,
	bool bMergeIdenticalMaterialSlots
)
{
	FUsdMeshConversionOptions Options;
	Options.TimeCode = TimeCode;
	Options.PurposesToLoad = PurposesToLoad;
	Options.RenderContext = RenderContext;
	Options.MaterialToPrimvarToUVIndex = &MaterialToPrimvarToUVIndex;
	Options.bMergeIdenticalMaterialSlots = bMergeIdenticalMaterialSlots;

	return ConvertGeomMeshHierarchy(
		Prim,
		OutMeshDescription,
		OutMaterialAssignments,
		Options,
		bSkipRootPrimTransformAndVisibility
	);
}

UMaterialInstanceDynamic* UsdUtils::CreateDisplayColorMaterialInstanceDynamic( const UsdUtils::FDisplayColorMaterial& DisplayColorDescription )
{
	FString ParentPath;
	if ( DisplayColorDescription.bHasOpacity )
	{
		if ( DisplayColorDescription.bIsDoubleSided )
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColorAndOpacityDoubleSided.DisplayColorAndOpacityDoubleSided'" );
		}
		else
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColorAndOpacity.DisplayColorAndOpacity'" );
		}
	}
	else
	{
		if ( DisplayColorDescription.bIsDoubleSided )
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColorDoubleSided.DisplayColorDoubleSided'" );
		}
		else
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColor.DisplayColor'" );
		}
	}

	if ( UMaterialInterface* ParentMaterial = Cast< UMaterialInterface >( FSoftObjectPath( ParentPath ).TryLoad() ) )
	{
		FName AssetName = MakeUniqueObjectName(
			GetTransientPackage(),
			UMaterialInstanceConstant::StaticClass(),
			*FString::Printf( TEXT( "DisplayColor_%s_%s" ),
				DisplayColorDescription.bHasOpacity ? TEXT( "Opacity" ) : TEXT( "NoOpacity" ),
				DisplayColorDescription.bIsDoubleSided ? TEXT( "DoubleSided" ) : TEXT( "SingleSided" )
			)
		);

		if ( UMaterialInstanceDynamic* NewMaterial = UMaterialInstanceDynamic::Create( ParentMaterial, GetTransientPackage(), AssetName ) )
		{
			return NewMaterial;
		}
	}

	return nullptr;
}

UMaterialInstanceConstant* UsdUtils::CreateDisplayColorMaterialInstanceConstant( const FDisplayColorMaterial& DisplayColorDescription )
{
#if WITH_EDITOR
	FString ParentPath;
	if ( DisplayColorDescription.bHasOpacity )
	{
		if ( DisplayColorDescription.bIsDoubleSided )
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColorAndOpacityDoubleSided.DisplayColorAndOpacityDoubleSided'" );
		}
		else
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColorAndOpacity.DisplayColorAndOpacity'" );
		}
	}
	else
	{
		if ( DisplayColorDescription.bIsDoubleSided )
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColorDoubleSided.DisplayColorDoubleSided'" );
		}
		else
		{
			ParentPath = TEXT( "Material'/USDImporter/Materials/DisplayColor.DisplayColor'" );
		}
	}

	if ( UMaterialInterface* ParentMaterial = Cast< UMaterialInterface >( FSoftObjectPath( ParentPath ).TryLoad() ) )
	{
		FName AssetName = MakeUniqueObjectName(
			GetTransientPackage(),
			UMaterialInstanceConstant::StaticClass(),
			*FString::Printf( TEXT( "DisplayColor_%s_%s" ),
				DisplayColorDescription.bHasOpacity ? TEXT( "Opacity" ) : TEXT( "NoOpacity" ),
				DisplayColorDescription.bIsDoubleSided ? TEXT( "DoubleSided" ) : TEXT( "SingleSided" )
			)
		);

		if ( UMaterialInstanceConstant* MaterialInstance = NewObject< UMaterialInstanceConstant >( GetTransientPackage(), AssetName, RF_NoFlags ) )
		{
			UMaterialEditingLibrary::SetMaterialInstanceParent( MaterialInstance, ParentMaterial );
			return MaterialInstance;
		}
	}
#endif // WITH_EDITOR
	return nullptr;
}

UsdUtils::FUsdPrimMaterialAssignmentInfo UsdUtils::GetPrimMaterialAssignments(
	const pxr::UsdPrim& UsdPrim,
	const pxr::UsdTimeCode TimeCode,
	bool bProvideMaterialIndices,
	const pxr::TfToken& RenderContext,
	const pxr::TfToken& MaterialPurpose
)
{
	if ( !UsdPrim )
	{
		return {};
	}

	auto FetchFirstUEMaterialFromAttribute = []( const pxr::UsdPrim& UsdPrim, const pxr::UsdTimeCode TimeCode ) -> TOptional<FString>
	{
		FString ValidPackagePath;
		if ( pxr::UsdAttribute MaterialAttribute = UsdPrim.GetAttribute( UnrealIdentifiers::MaterialAssignment ) )
		{
			std::string UEMaterial;
			if ( MaterialAttribute.Get( &UEMaterial, TimeCode ) && UEMaterial.size() > 0)
			{
				ValidPackagePath = UsdToUnreal::ConvertString( UEMaterial );
			}
		}

		if ( !ValidPackagePath.IsEmpty() )
		{
			// We can't TryLoad() this right now as we may be in an Async thread, so settle for checking with the asset registry module
			FSoftObjectPath SoftObjectPath{ ValidPackagePath };
			if ( SoftObjectPath.IsValid() )
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( "AssetRegistry" );
				FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath( SoftObjectPath );
				if ( AssetData.IsValid() && AssetData.GetClass()->IsChildOf( UMaterialInterface::StaticClass() ) )
				{
					return ValidPackagePath;
				}
			}

			UE_LOG( LogUsd, Warning, TEXT( "Could not find a valid material at path '%s', targetted by prim '%s's unrealMaterial attribute. Material assignment will fallback to USD materials and display color data." ),
				*ValidPackagePath, *UsdToUnreal::ConvertPath( UsdPrim.GetPath() ) );
		}

		return {};
	};

	auto FetchMaterialByComputingBoundMaterial = [ &RenderContext, &MaterialPurpose ]( const pxr::UsdPrim& UsdPrim ) -> TOptional<FString>
	{
		pxr::UsdShadeMaterialBindingAPI BindingAPI( UsdPrim );
		pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial( MaterialPurpose );
		if ( !ShadeMaterial )
		{
			return {};
		}

		// Ignore this material if UsdToUnreal::ConvertMaterial would as well
		pxr::UsdShadeShader SurfaceShader = ShadeMaterial.ComputeSurfaceSource( RenderContext );
		if ( !SurfaceShader )
		{
			return {};
		}

		pxr::UsdPrim ShadeMaterialPrim = ShadeMaterial.GetPrim();
		if ( ShadeMaterialPrim )
		{
			pxr::SdfPath Path = ShadeMaterialPrim.GetPath();
			std::string ShadingEngineName = ( ShadeMaterialPrim ? ShadeMaterialPrim.GetPrim() : UsdPrim.GetPrim() ).GetPrimPath().GetString();
			if(ShadingEngineName.size() > 0 )
			{
				return UsdToUnreal::ConvertString( ShadingEngineName );
			}
		}

		return {};
	};

	auto FetchMaterialByMaterialRelationship = [ &RenderContext ]( const pxr::UsdPrim& UsdPrim ) -> TOptional<FString>
	{
		if ( pxr::UsdRelationship Relationship = UsdPrim.GetRelationship( pxr::UsdShadeTokens->materialBinding ) )
		{
			pxr::SdfPathVector Targets;
			Relationship.GetTargets( &Targets );

			if ( Targets.size() > 0 )
			{
				const pxr::SdfPath& TargetMaterialPrimPath = Targets[0];
				pxr::UsdPrim MaterialPrim = UsdPrim.GetStage()->GetPrimAtPath( TargetMaterialPrimPath );
				pxr::UsdShadeMaterial UsdShadeMaterial{ MaterialPrim };
				if ( !UsdShadeMaterial )
				{
					FUsdLogManager::LogMessage(
						EMessageSeverity::Warning,
						FText::Format( LOCTEXT( "IgnoringMaterialInvalid", "Ignoring material '{0}' bound to prim '{1}' as it does not possess the UsdShadeMaterial schema" ),
							FText::FromString( UsdToUnreal::ConvertPath( TargetMaterialPrimPath ) ),
							FText::FromString( UsdToUnreal::ConvertPath( UsdPrim.GetPath() ) )
						)
					);
					return {};
				}

				// Ignore this material if UsdToUnreal::ConvertMaterial would as well
				pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource( RenderContext );
				if ( !SurfaceShader )
				{
					FUsdLogManager::LogMessage(
						EMessageSeverity::Warning,
						FText::Format( LOCTEXT( "IgnoringMaterialSurface", "Ignoring material '{0}' bound to prim '{1}' as it contains no valid surface shader source for render context '{2}'" ),
							FText::FromString( UsdToUnreal::ConvertPath( TargetMaterialPrimPath ) ),
							FText::FromString( UsdToUnreal::ConvertPath( UsdPrim.GetPath() ) ),
							FText::FromString( RenderContext == pxr::UsdShadeTokens->universalRenderContext ? TEXT( "universal" ) : UsdToUnreal::ConvertToken( RenderContext ) )
						)
					);
					return {};
				}

				FString MaterialPrimPath = UsdToUnreal::ConvertPath( TargetMaterialPrimPath );
				if ( Targets.size() > 1 )
				{
					FUsdLogManager::LogMessage(
						EMessageSeverity::Warning,
						FText::Format( LOCTEXT( "MoreThanOneMaterialBinding", "Found more than on material:binding targets on prim '{0}'. The first material ('{1}') will be used, and the rest ignored." ),
							FText::FromString( UsdToUnreal::ConvertPath( UsdPrim.GetPath() ) ),
							FText::FromString( MaterialPrimPath )
						)
					);
				}

				return MaterialPrimPath;
			}
		}

		return {};
	};

	FUsdPrimMaterialAssignmentInfo Result;

	uint64 NumFaces = 0;
	{
		pxr::UsdGeomMesh Mesh = pxr::UsdGeomMesh( UsdPrim );
		pxr::UsdAttribute FaceCounts = Mesh.GetFaceVertexCountsAttr();
		if ( !Mesh || !FaceCounts )
		{
			return Result;
		}

		pxr::VtArray<int> FaceVertexCounts;
		FaceCounts.Get( &FaceVertexCounts, TimeCode );
		NumFaces = FaceVertexCounts.size();
		if ( NumFaces < 1 )
		{
			return Result;
		}

		if ( bProvideMaterialIndices )
		{
			Result.MaterialIndices.SetNumZeroed( NumFaces );
		}
	}

	// Priority 1: Material is an unreal asset
	if ( RenderContext == UnrealIdentifiers::Unreal )
	{
		// Priority 1.1: unreal rendercontext material prim
		pxr::UsdShadeMaterialBindingAPI BindingAPI( UsdPrim );
		if ( BindingAPI )
		{
			if ( pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial( MaterialPurpose ) )
			{
				if ( TOptional<FString> UnrealMaterial = UsdUtils::GetUnrealSurfaceOutput( ShadeMaterial.GetPrim() ) )
				{
					FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
					Slot.MaterialSource = UnrealMaterial.GetValue();
					Slot.AssignmentType = UsdUtils::EPrimAssignmentType::UnrealMaterial;

					return Result;
				}
			}
		}

		// Priority 1.2: unrealMaterial attribute directly on the prim
		if ( TOptional<FString> UnrealMaterial = FetchFirstUEMaterialFromAttribute( UsdPrim, TimeCode ) )
		{
			FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
			Slot.MaterialSource = UnrealMaterial.GetValue();
			Slot.AssignmentType = UsdUtils::EPrimAssignmentType::UnrealMaterial;

			return Result;
		}
	}

	// Priority 2: material binding directly on the prim
	if ( TOptional<FString> BoundMaterial = FetchMaterialByComputingBoundMaterial( UsdPrim ) )
	{
		FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
		Slot.MaterialSource = BoundMaterial.GetValue();
		Slot.AssignmentType = UsdUtils::EPrimAssignmentType::MaterialPrim;

		return Result;
	}

	// Priority 3: material:binding relationship directly on the prim (not sure why this is a separate step, but it came from IUsdPrim::GetGeometryMaterials. I bumped it in priority as the GeomSubsets do the same)
	if ( TOptional<FString> TargetMaterial = FetchMaterialByMaterialRelationship( UsdPrim ) )
	{
		FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
		Slot.MaterialSource = TargetMaterial.GetValue();
		Slot.AssignmentType = UsdUtils::EPrimAssignmentType::MaterialPrim;

		return Result;
	}

	TOptional<FDisplayColorMaterial> DisplayColor = ExtractDisplayColorMaterial( pxr::UsdGeomMesh( UsdPrim ), TimeCode );

	// Priority 4: GeomSubset partitions
	std::vector<pxr::UsdGeomSubset> GeomSubsets = pxr::UsdShadeMaterialBindingAPI( UsdPrim ).GetMaterialBindSubsets();
	if ( GeomSubsets.size() > 0 )
	{
		// We need to do this even if we won't provide indices because we may create an additional slot for unassigned polygons
		pxr::VtIntArray UnassignedIndices;
		std::string ReasonWhyNotPartition;
		bool ValidPartition = pxr::UsdGeomSubset::ValidateSubsets( GeomSubsets, NumFaces, pxr::UsdGeomTokens->partition, &ReasonWhyNotPartition );
		if ( !ValidPartition )
		{
			UE_LOG( LogUsd, Warning, TEXT( "Found an invalid GeomSubsets partition in prim '%s': %s" ),
				*UsdToUnreal::ConvertPath( UsdPrim.GetPath() ), *UsdToUnreal::ConvertString( ReasonWhyNotPartition ));
			UnassignedIndices = pxr::UsdGeomSubset::GetUnassignedIndices( GeomSubsets, NumFaces );
		}

		for ( uint32 GeomSubsetIndex = 0; GeomSubsetIndex < GeomSubsets.size(); ++GeomSubsetIndex )
		{
			const pxr::UsdGeomSubset& GeomSubset = GeomSubsets[ GeomSubsetIndex ];
			bool bHasAssignment = false;

			// Priority 4.1: Material is an unreal asset
			if ( RenderContext == UnrealIdentifiers::Unreal )
			{
				// Priority 4.1.1: Partition has an unreal rendercontext material prim binding
				if ( !bHasAssignment )
				{
					pxr::UsdShadeMaterialBindingAPI BindingAPI( GeomSubset.GetPrim() );
					if ( BindingAPI )
					{
						if ( pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial( MaterialPurpose ) )
						{
							if ( TOptional<FString> UnrealMaterial = UsdUtils::GetUnrealSurfaceOutput( ShadeMaterial.GetPrim() ) )
							{
								FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
								Slot.MaterialSource = UnrealMaterial.GetValue();
								Slot.AssignmentType = UsdUtils::EPrimAssignmentType::UnrealMaterial;
								bHasAssignment = true;
							}
						}
					}
				}

				// Priority 4.1.2: Partitition has an unrealMaterial attribute directly on it
				if ( !bHasAssignment )
				{
					if ( TOptional<FString> UnrealMaterial = FetchFirstUEMaterialFromAttribute( GeomSubset.GetPrim(), TimeCode ) )
					{
						FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
						Slot.MaterialSource = UnrealMaterial.GetValue();
						Slot.AssignmentType = UsdUtils::EPrimAssignmentType::UnrealMaterial;
						bHasAssignment = true;
					}
				}
			}

			// Priority 4.2: computing bound material
			if ( !bHasAssignment )
			{
				if ( TOptional<FString> BoundMaterial = FetchMaterialByComputingBoundMaterial( GeomSubset.GetPrim() ) )
				{
					FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
					Slot.MaterialSource = BoundMaterial.GetValue();
					Slot.AssignmentType = UsdUtils::EPrimAssignmentType::MaterialPrim;
					bHasAssignment = true;
				}
			}

			// Priority 4.3: material:binding relationship
			if ( !bHasAssignment )
			{
				if ( TOptional<FString> TargetMaterial = FetchMaterialByMaterialRelationship( GeomSubset.GetPrim() ) )
				{
					FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
					Slot.MaterialSource = TargetMaterial.GetValue();
					Slot.AssignmentType = UsdUtils::EPrimAssignmentType::MaterialPrim;
					bHasAssignment = true;
				}
			}

			// Priority 4.4: Create a section anyway so it becomes its own slot. Assign displayColor if we have one
			if ( !bHasAssignment )
			{
				FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
				if ( DisplayColor )
				{
					Slot.MaterialSource = DisplayColor.GetValue().ToString();
					Slot.AssignmentType = UsdUtils::EPrimAssignmentType::DisplayColor;
				}
				bHasAssignment = true;
			}

			pxr::VtIntArray PolygonIndicesInSubset;
			GeomSubset.GetIndicesAttr().Get( &PolygonIndicesInSubset, TimeCode );

			if ( bProvideMaterialIndices )
			{
				int32 LastAssignmentIndex = Result.Slots.Num() - 1;
				for ( int PolygonIndex : PolygonIndicesInSubset )
				{
					Result.MaterialIndices[ PolygonIndex ] = LastAssignmentIndex;
				}
			}
		}

		// Extra slot for unassigned polygons
		if ( UnassignedIndices.size() > 0 )
		{
			FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
			if ( DisplayColor )
			{
				Slot.MaterialSource = DisplayColor.GetValue().ToString();
				Slot.AssignmentType = UsdUtils::EPrimAssignmentType::DisplayColor;
			}

			if ( bProvideMaterialIndices )
			{
				int32 LastAssignmentIndex = Result.Slots.Num() - 1;
				for ( int PolygonIndex : UnassignedIndices )
				{
					Result.MaterialIndices[ PolygonIndex ] = LastAssignmentIndex;
				}
			}
		}

		return Result;
	}

	// Priority 5: vertex color material using displayColor/displayOpacity information for the entire mesh
	if ( GeomSubsets.size() == 0 && DisplayColor )
	{
		FUsdPrimMaterialSlot& Slot = Result.Slots.Emplace_GetRef();
		Slot.MaterialSource = DisplayColor.GetValue().ToString();
		Slot.AssignmentType = UsdUtils::EPrimAssignmentType::DisplayColor;

		return Result;
	}

	// Priority 6: Make sure there is always at least one slot, even if empty
	if ( Result.Slots.Num() < 1 )
	{
		Result.Slots.Emplace();
	}
	return Result;
}

TArray<FString> UsdUtils::GetMaterialUsers( const UE::FUsdPrim& MaterialPrim, FName MaterialPurpose )
{
	TArray<FString> Result;

	FScopedUsdAllocs Allocs;

	pxr::UsdPrim UsdMaterialPrim{ MaterialPrim };
	if ( !UsdMaterialPrim || !UsdMaterialPrim.IsA<pxr::UsdShadeMaterial>() )
	{
		return Result;
	}

	pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
	if ( !MaterialPurpose.IsNone() )
	{
		MaterialPurposeToken = UnrealToUsd::ConvertToken( *MaterialPurpose.ToString() ).Get();
	}

	pxr::UsdStageRefPtr UsdStage = UsdMaterialPrim.GetStage();

	pxr::UsdPrimRange PrimRange = pxr::UsdPrimRange::Stage( UsdStage, pxr::UsdTraverseInstanceProxies() );
	for ( pxr::UsdPrimRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt )
	{
		pxr::UsdPrim Prim = *PrimRangeIt;

		if ( !Prim.HasAPI<pxr::UsdShadeMaterialBindingAPI>() )
		{
			continue;
		}

		pxr::UsdShadeMaterialBindingAPI BindingAPI( Prim );
		pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial( MaterialPurposeToken );
		if ( !ShadeMaterial )
		{
			continue;
		}

		pxr::UsdPrim ShadeMaterialPrim = ShadeMaterial.GetPrim();
		if ( ShadeMaterialPrim == UsdMaterialPrim )
		{
			Result.Add( UsdToUnreal::ConvertPath( Prim.GetPrimPath() ) );
		}
	}

	return Result;
}

bool UnrealToUsd::ConvertStaticMesh( const UStaticMesh* StaticMesh, pxr::UsdPrim& UsdPrim, const pxr::UsdTimeCode TimeCode, UE::FUsdStage* StageForMaterialAssignments, int32 LowestMeshLOD, int32 HighestMeshLOD )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdStageRefPtr Stage = UsdPrim.GetStage();
	if ( !Stage )
	{
		return false;
	}

	const FUsdStageInfo StageInfo( Stage );

	int32 NumLODs = StaticMesh->GetNumLODs();
	if ( NumLODs < 1 )
	{
		return false;
	}

	// Make sure they're both >= 0 (the options dialog slider is clamped, but this may be called directly)
	LowestMeshLOD = FMath::Clamp( LowestMeshLOD, 0, NumLODs -1 );
	HighestMeshLOD = FMath::Clamp( HighestMeshLOD, 0, NumLODs - 1 );

	// Make sure Lowest <= Highest
	int32 Temp = FMath::Min( LowestMeshLOD, HighestMeshLOD );
	HighestMeshLOD = FMath::Max( LowestMeshLOD, HighestMeshLOD );
	LowestMeshLOD = Temp;

	// Make sure it's at least 1 LOD level
	NumLODs = FMath::Max( HighestMeshLOD - LowestMeshLOD + 1, 1 );

	// If exporting a Nanite mesh, just use the lowest LOD and write the unrealNanite override attribute,
	// this way we can guarantee it will have Nanite when it's imported back.
#if WITH_EDITOR
	if ( StaticMesh->NaniteSettings.bEnabled )
	{
		if ( NumLODs > 1 )
		{
			UE_LOG( LogUsd, Log, TEXT( "Not exporting multiple LODs for mesh '%s' onto prim '%s' since the mesh has Nanite enabled: LOD '%d' will be used and the '%s' attribute will be written out instead" ),
				*StaticMesh->GetName(),
				*UsdToUnreal::ConvertPath( UsdPrim.GetPrimPath() ),
				LowestMeshLOD,
				*UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealNaniteOverride )
			);
		}

		HighestMeshLOD = LowestMeshLOD;
		NumLODs = 1;

		if ( pxr::UsdAttribute Attr = UsdPrim.CreateAttribute( UnrealIdentifiers::UnrealNaniteOverride, pxr::SdfValueTypeNames->Token ) )
		{
			Attr.Set( UnrealIdentifiers::UnrealNaniteOverrideEnable );
			UsdUtils::NotifyIfOverriddenOpinion( UE::FUsdAttribute{ Attr } );
		}
	}
#endif // WITH_EDITOR

	pxr::UsdVariantSets VariantSets = UsdPrim.GetVariantSets();
	if ( NumLODs > 1 && VariantSets.HasVariantSet( UnrealIdentifiers::LOD ) )
	{
		UE_LOG( LogUsd, Error, TEXT("Failed to export higher LODs for mesh '%s', as the target prim already has a variant set named '%s'!"), *StaticMesh->GetName(), *UsdToUnreal::ConvertToken( UnrealIdentifiers::LOD ) );
		NumLODs = 1;
	}

	bool bExportMultipleLODs = NumLODs > 1;

	pxr::SdfPath ParentPrimPath = UsdPrim.GetPath();
	std::string LowestLODAdded = "";

	// Collect all material assignments, referenced by the sections' material indices
	bool bHasMaterialAssignments = false;
	pxr::VtArray< std::string > MaterialAssignments;
	for(const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
	{
		FString AssignedMaterialPathName;
		if ( UMaterialInterface* Material = StaticMaterial.MaterialInterface )
		{
			if ( Material->GetOutermost() != GetTransientPackage() )
			{
				AssignedMaterialPathName = Material->GetPathName();
				bHasMaterialAssignments = true;
			}
		}

		MaterialAssignments.push_back( UnrealToUsd::ConvertString( *AssignedMaterialPathName ).Get() );
	}
	if ( !bHasMaterialAssignments )
	{
		// Prevent creation of the unrealMaterials attribute in case we don't have any assignments at all
		MaterialAssignments.clear();
	}

	for ( int32 LODIndex = LowestMeshLOD; LODIndex <= HighestMeshLOD; ++LODIndex )
	{
		const FStaticMeshLODResources& RenderMesh = StaticMesh->GetLODForExport( LODIndex );

		// Verify the integrity of the static mesh.
		if ( RenderMesh.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0 )
		{
			continue;
		}

		if ( RenderMesh.Sections.Num() == 0 )
		{
			continue;
		}

		// LOD0, LOD1, etc
		std::string VariantName = UnrealIdentifiers::LOD.GetString() + UnrealToUsd::ConvertString( *LexToString( LODIndex ) ).Get();
		if ( LowestLODAdded.size() == 0 )
		{
			LowestLODAdded = VariantName;
		}

		pxr::SdfPath LODPrimPath = ParentPrimPath.AppendPath(pxr::SdfPath(VariantName));

		// Enable the variant edit context, if we are creating variant LODs
		TOptional< pxr::UsdEditContext > EditContext;
		if ( bExportMultipleLODs )
		{
			pxr::UsdVariantSet VariantSet = VariantSets.GetVariantSet( UnrealIdentifiers::LOD );

			if ( !VariantSet.AddVariant( VariantName ) )
			{
				continue;
			}

			VariantSet.SetVariantSelection( VariantName );
			EditContext.Emplace( VariantSet.GetVariantEditContext() );
		}

		// Author material bindings on the dedicated stage if we have one
		pxr::UsdStageRefPtr MaterialStage;
		if ( StageForMaterialAssignments )
		{
			MaterialStage = static_cast< pxr::UsdStageRefPtr >( *StageForMaterialAssignments );
		}
		else
		{
			MaterialStage = Stage;
		}

		pxr::UsdGeomMesh TargetMesh;
		pxr::UsdPrim MaterialPrim = UsdPrim;
		if ( bExportMultipleLODs )
		{
			// Add the mesh data to a child prim with the Mesh schema
			pxr::UsdPrim UsdLODPrim = Stage->DefinePrim( LODPrimPath, UnrealToUsd::ConvertToken( TEXT("Mesh") ).Get() );
			TargetMesh = pxr::UsdGeomMesh{ UsdLODPrim };

			MaterialPrim = MaterialStage->OverridePrim( LODPrimPath );
		}
		else
		{
			// Make sure the parent prim has the Mesh schema and add the mesh data directly to it
			UsdPrim = Stage->DefinePrim( UsdPrim.GetPath(), UnrealToUsd::ConvertToken( TEXT("Mesh") ).Get() );
			TargetMesh = pxr::UsdGeomMesh{ UsdPrim };

			MaterialPrim = MaterialStage->OverridePrim( UsdPrim.GetPath() );
		}

		UsdGeomMeshImpl::ConvertStaticMeshLOD( LODIndex, RenderMesh, TargetMesh, MaterialAssignments, TimeCode, MaterialPrim );
	}

	// Reset variant set to start with the lowest lod selected
	if ( bExportMultipleLODs )
	{
		VariantSets.GetVariantSet(UnrealIdentifiers::LOD).SetVariantSelection(LowestLODAdded);
	}

	return true;
}

bool UnrealToUsd::ConvertMeshDescriptions( const TArray<FMeshDescription>& LODIndexToMeshDescription, pxr::UsdPrim& UsdPrim, const FMatrix& AdditionalTransform, const pxr::UsdTimeCode TimeCode )
{
	FScopedUsdAllocs UsdAllocs;

	pxr::UsdStageRefPtr Stage = UsdPrim.GetStage();
	if ( !Stage )
	{
		return false;
	}

	const FUsdStageInfo StageInfo( Stage );

	int32 NumLODs = LODIndexToMeshDescription.Num();
	if ( NumLODs < 1 )
	{
		return false;
	}

	pxr::UsdVariantSets VariantSets = UsdPrim.GetVariantSets();
	if ( NumLODs > 1 && VariantSets.HasVariantSet( UnrealIdentifiers::LOD ) )
	{
		UE_LOG( LogUsd, Error, TEXT( "Failed to convert higher mesh description LODs for prim '%s', as the target prim already has a variant set named '%s'!" ),
			*UsdToUnreal::ConvertPath( UsdPrim.GetPath() ),
			*UsdToUnreal::ConvertToken( UnrealIdentifiers::LOD )
		);
		NumLODs = 1;
	}

	bool bExportMultipleLODs = NumLODs > 1;

	pxr::SdfPath ParentPrimPath = UsdPrim.GetPath();
	std::string LowestLODAdded = "";

	for ( int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex )
	{
		const FMeshDescription& MeshDescription = LODIndexToMeshDescription[ LODIndex ];

		// LOD0, LOD1, etc
		std::string VariantName = UnrealIdentifiers::LOD.GetString() + UnrealToUsd::ConvertString( *LexToString( LODIndex ) ).Get();
		if ( LowestLODAdded.size() == 0 )
		{
			LowestLODAdded = VariantName;
		}

		pxr::SdfPath LODPrimPath = ParentPrimPath.AppendPath( pxr::SdfPath( VariantName ) );

		// Enable the variant edit context, if we are creating variant LODs
		TOptional< pxr::UsdEditContext > EditContext;
		if ( bExportMultipleLODs )
		{
			pxr::UsdVariantSet VariantSet = VariantSets.GetVariantSet( UnrealIdentifiers::LOD );
			if ( !VariantSet.AddVariant( VariantName ) )
			{
				continue;
			}

			VariantSet.SetVariantSelection( VariantName );
			EditContext.Emplace( VariantSet.GetVariantEditContext() );
		}

		pxr::UsdGeomMesh TargetMesh;
		if ( bExportMultipleLODs )
		{
			// Add the mesh data to a child prim with the Mesh schema
			pxr::UsdPrim UsdLODPrim = Stage->DefinePrim( LODPrimPath, UnrealToUsd::ConvertToken( TEXT( "Mesh" ) ).Get() );
			TargetMesh = pxr::UsdGeomMesh{ UsdLODPrim };
		}
		else
		{
			// Make sure the parent prim has the Mesh schema and add the mesh data directly to it
			UsdPrim = Stage->DefinePrim( UsdPrim.GetPath(), UnrealToUsd::ConvertToken( TEXT( "Mesh" ) ).Get() );
			TargetMesh = pxr::UsdGeomMesh{ UsdPrim };
		}

		if ( !UsdGeomMeshImpl::ConvertMeshDescription( MeshDescription, TargetMesh, AdditionalTransform, TimeCode ) )
		{
			return false;
		}
	}

	// Reset variant set to start with the lowest lod selected
	if ( bExportMultipleLODs )
	{
		VariantSets.GetVariantSet( UnrealIdentifiers::LOD ).SetVariantSelection( LowestLODAdded );
	}

	return true;
}

FString UsdUtils::FDisplayColorMaterial::ToString()
{
	return FString::Printf( TEXT("%s_%d_%d"), *UsdGeomMeshImpl::DisplayColorID, bHasOpacity, bIsDoubleSided );
}

TOptional<UsdUtils::FDisplayColorMaterial> UsdUtils::FDisplayColorMaterial::FromString( const FString& DisplayColorString )
{
	TArray<FString> Tokens;
	DisplayColorString.ParseIntoArray( Tokens, TEXT( "_" ) );

	if ( Tokens.Num() != 3 || Tokens[ 0 ] != UsdGeomMeshImpl::DisplayColorID )
	{
		return {};
	}

	UsdUtils::FDisplayColorMaterial Result;
	Result.bHasOpacity = static_cast< bool >( FCString::Atoi( *Tokens[ 1 ] ) );
	Result.bIsDoubleSided = static_cast< bool >( FCString::Atoi( *Tokens[ 2 ] ) );
	return Result;
}

TOptional<UsdUtils::FDisplayColorMaterial> UsdUtils::ExtractDisplayColorMaterial( const pxr::UsdGeomMesh& UsdMesh, const pxr::UsdTimeCode TimeCode )
{
	if ( !UsdMesh )
	{
		return {};
	}

	if ( !UsdMesh.GetDisplayOpacityAttr().IsDefined() && !UsdMesh.GetDisplayColorAttr().IsDefined() )
	{
		return {};
	}

	UsdUtils::FDisplayColorMaterial Desc;

	// Opacity
	pxr::VtArray< float > UsdOpacities = UsdUtils::GetUsdValue< pxr::VtArray< float > >( UsdMesh.GetDisplayOpacityAttr(), TimeCode );
	for ( float Opacity : UsdOpacities )
	{
		Desc.bHasOpacity = !FMath::IsNearlyEqual( Opacity, 1.f );
		if ( Desc.bHasOpacity )
		{
			break;
		}
	}

	// Double-sided
	if ( UsdMesh.GetDoubleSidedAttr().IsDefined() )
	{
		Desc.bIsDoubleSided = UsdUtils::GetUsdValue< bool >( UsdMesh.GetDoubleSidedAttr(), TimeCode );
	}

	return Desc;
}

namespace UE::UsdGeomMeshConversion::Private
{
	bool DoesPrimContainMeshLODsInternal( const pxr::UsdPrim& Prim )
	{
		FScopedUsdAllocs Allocs;

		if ( !Prim )
		{
			return false;
		}

		const std::string LODString = UnrealIdentifiers::LOD.GetString();

		pxr::UsdVariantSets VariantSets = Prim.GetVariantSets();
		if ( !VariantSets.HasVariantSet( LODString ) )
		{
			return false;
		}

		std::string Selection = VariantSets.GetVariantSet( LODString ).GetVariantSelection();
		int32 LODIndex = UsdGeomMeshImpl::GetLODIndexFromName( Selection );
		if ( LODIndex == INDEX_NONE )
		{
			return false;
		}

		return true;
	}
}

bool UsdUtils::DoesPrimContainMeshLODs( const pxr::UsdPrim& Prim )
{
	const bool bHasValidVariantSetup = UE::UsdGeomMeshConversion::Private::DoesPrimContainMeshLODsInternal( Prim );
	if ( bHasValidVariantSetup )
	{
		FScopedUsdAllocs Allocs;

		// Check if it has at least one mesh too
		pxr::UsdPrimSiblingRange PrimRange = Prim.GetChildren();
		for ( pxr::UsdPrimSiblingRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt )
		{
			const pxr::UsdPrim& Child = *PrimRangeIt;
			if ( pxr::UsdGeomMesh ChildMesh{ Child } )
			{
				return true;
				break;
			}
		}
	}

	return false;
}

bool UsdUtils::IsGeomMeshALOD( const pxr::UsdPrim& UsdMeshPrim )
{
	FScopedUsdAllocs Allocs;

	pxr::UsdGeomMesh UsdMesh{ UsdMeshPrim };
	if ( !UsdMesh )
	{
		return false;
	}

	return UE::UsdGeomMeshConversion::Private::DoesPrimContainMeshLODsInternal( UsdMeshPrim.GetParent() );
}

int32 UsdUtils::GetNumberOfLODVariants( const pxr::UsdPrim& Prim )
{
	FScopedUsdAllocs Allocs;

	const std::string LODString = UnrealIdentifiers::LOD.GetString();

	pxr::UsdVariantSets VariantSets = Prim.GetVariantSets();
	if ( !VariantSets.HasVariantSet( LODString ) )
	{
		return 1;
	}

	return VariantSets.GetVariantSet( LODString ).GetVariantNames().size();
}

bool UsdUtils::IterateLODMeshes( const pxr::UsdPrim& ParentPrim, TFunction<bool( const pxr::UsdGeomMesh & LODMesh, int32 LODIndex )> Func )
{
	if ( !ParentPrim )
	{
		return false;
	}

	FScopedUsdAllocs Allocs;

	const std::string LODString = UnrealIdentifiers::LOD.GetString();

	pxr::UsdVariantSets VariantSets = ParentPrim.GetVariantSets();
	if ( !VariantSets.HasVariantSet( LODString ) )
	{
		return false;
	}

	pxr::UsdVariantSet LODVariantSet = VariantSets.GetVariantSet( LODString );
	const std::string OriginalVariant = LODVariantSet.GetVariantSelection();

	pxr::UsdStageRefPtr Stage = ParentPrim.GetStage();
	pxr::UsdEditContext( Stage, Stage->GetRootLayer() );

	bool bHasValidVariant = false;
	for ( const std::string& LODVariantName : VariantSets.GetVariantSet( LODString ).GetVariantNames() )
	{
		int32 LODIndex = UsdGeomMeshImpl::GetLODIndexFromName( LODVariantName );
		if ( LODIndex == INDEX_NONE )
		{
			continue;
		}

		LODVariantSet.SetVariantSelection( LODVariantName );

		pxr::UsdGeomMesh LODMesh;
		pxr::TfToken TargetChildNameToken{ LODVariantName };

		// Search for our LOD child mesh
		pxr::UsdPrimSiblingRange PrimRange = ParentPrim.GetChildren();
		for ( pxr::UsdPrimSiblingRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt )
		{
			const pxr::UsdPrim& Child = *PrimRangeIt;
			if ( pxr::UsdGeomMesh ChildMesh{ Child } )
			{
				if ( Child.GetName() == TargetChildNameToken )
				{
					LODMesh = ChildMesh;
					// Don't break here so we can show warnings if the user has other prims here (that we may end up ignoring)
					// USD doesn't allow name collisions anyway, so there won't be any other prim named TargetChildNameToken
				}
				else
				{
					UE_LOG(LogUsd, Warning, TEXT("Unexpected prim '%s' inside LOD variant '%s'. For automatic parsing of LODs, each LOD variant should contain only a single Mesh prim named the same as the variant!"),
						*UsdToUnreal::ConvertPath( Child.GetPath() ),
						*UsdToUnreal::ConvertString( LODVariantName )
					);
				}
			}
		}
		if ( !LODMesh )
		{
			continue;
		}

		bHasValidVariant = true;

		bool bContinue = Func(LODMesh, LODIndex);
		if ( !bContinue )
		{
			break;
		}
	}

	LODVariantSet.SetVariantSelection( OriginalVariant );
	return bHasValidVariant;
}

void UsdUtils::ReplaceUnrealMaterialsWithBaked(
	const UE::FUsdStage& Stage,
	const UE::FSdfLayer& LayerToAuthorIn,
	const TMap<FString, FString>& BakedMaterials,
	bool bIsAssetLayer,
	bool bUsePayload,
	bool bRemoveUnrealMaterials
)
{
	FScopedUsdAllocs Allocs;

	struct FMaterialScopePrim
	{
		FMaterialScopePrim( pxr::UsdStageRefPtr ScopeStage, pxr::UsdPrim ParentPrim )
		{
			pxr::SdfPath Path = ParentPrim.GetPrimPath().AppendPath( pxr::SdfPath{ "Materials" } );
			Prim = ScopeStage->DefinePrim( Path, UnrealToUsd::ConvertToken( TEXT( "Scope" ) ).Get() );

			// Initialize our UsedPrimNames correctly, so we can guarantee we'll never have name collisions
			if ( Prim )
			{
				for ( pxr::UsdPrim Child : Prim.GetFilteredChildren( pxr::UsdTraverseInstanceProxies( pxr::UsdPrimAllPrimsPredicate ) ) )
				{
					UsedPrimNames.Add( UsdToUnreal::ConvertToken( Child.GetName() ) );
				}
			}
		}

		pxr::UsdPrim Prim;
		TSet<FString> UsedPrimNames;
		TMap<FString, pxr::UsdPrim> BakedFileNameToMatPrim;
	};
	TOptional<FMaterialScopePrim> StageMatScope;

	pxr::UsdStageRefPtr UsdStage{ Stage };

	// UsedLayers here instead of layer stack because we may be exporting using payloads, and payload layers
	// don't show up on the layer stack list but do show up on the UsedLayers list
	std::vector<pxr::SdfLayerHandle> LayersToTraverse = UsdStage->GetUsedLayers();

	// Recursively traverses the stage, doing the material assignment replacements.
	// This handles Mesh prims as well as GeomSubset prims.
	// Note how we receive the stage as an argument instead of capturing it from the outer scope:
	// This ensures the inner function doesn't hold a reference to the stage
	TFunction<void( pxr::UsdStageRefPtr StageToTraverse, pxr::UsdPrim Prim, TOptional<FMaterialScopePrim>& MatPrimScope, TOptional<pxr::UsdVariantSet> OuterVariantSet )> TraverseForMaterialReplacement;
	TraverseForMaterialReplacement =
		[
			&TraverseForMaterialReplacement,
			&LayersToTraverse,
			&LayerToAuthorIn,
			&BakedMaterials,
			bIsAssetLayer,
			bUsePayload,
			bRemoveUnrealMaterials,
			&StageMatScope
		]
		(
			pxr::UsdStageRefPtr StageToTraverse,
			pxr::UsdPrim Prim,
			TOptional<FMaterialScopePrim>& MatPrimScope,
			TOptional<pxr::UsdVariantSet> OuterVariantSet
		)
		{
			// Recurse into children before doing anything as we may need to parse LODs
			pxr::UsdVariantSet VarSet = Prim.GetVariantSet( UnrealIdentifiers::LOD );
			std::vector<std::string> LODs = VarSet.GetVariantNames();
			if ( LODs.size() > 0 )
			{
				TOptional<std::string> OriginalSelection = VarSet.HasAuthoredVariantSelection() ? VarSet.GetVariantSelection() : TOptional<std::string>{};

				// Prims within variant sets can't have relationships to prims outside the scope of the prim that
				// contains the variant set itself. This means we'll need a new material scope prim if we're stepping
				// into a variant within an asset layer, so that any material proxy prims we author are contained within it.
				// Note that we only do this for asset layers: If we're parsing the root layer, any LOD variant sets we can step into
				// are brought in via references to asset files, and we know that referenced subtree only has relationships to
				// things within that same subtree ( which will be entirely brought in to the root layer ). This means we can
				// just keep inner_mat_prim_scope as None and default to using the layer's mat scope prim if we need one
				TOptional<FMaterialScopePrim> InnerMatPrimScope = bIsAssetLayer ? FMaterialScopePrim{ StageToTraverse, Prim } : TOptional<FMaterialScopePrim>{};

				// Switch into each of the LOD variants the prim has, and recurse into the child prims
				for ( const std::string& Variant : LODs )
				{
					{
						pxr::UsdEditContext Context{ StageToTraverse, StageToTraverse->GetSessionLayer() };
						VarSet.SetVariantSelection( Variant );
					}

					for ( const pxr::UsdPrim& Child : Prim.GetChildren() )
					{
						TraverseForMaterialReplacement( StageToTraverse, Child, InnerMatPrimScope, VarSet );
					}
				}

				// Restore the variant selection to what it originally was
				pxr::UsdEditContext Context{ StageToTraverse, StageToTraverse->GetSessionLayer() };
				if ( OriginalSelection.IsSet() )
				{
					VarSet.SetVariantSelection( OriginalSelection.GetValue() );
				}
				else
				{
					VarSet.ClearVariantSelection();
				}
			}
			else
			{
				for ( const pxr::UsdPrim& Child : Prim.GetChildren() )
				{
					TraverseForMaterialReplacement( StageToTraverse, Child, MatPrimScope, OuterVariantSet );
				}
			}

			// Don't try fetching attributes from the pseudoroot as we'll obviously never have a material binding here
			// and we may get some USD warnings
			if ( Prim.IsPseudoRoot() )
			{
				return;
			}

			std::string UnrealMaterialAttrAssetPath;
			FString UnrealMaterialPrimAssetPath;

			pxr::UsdAttribute UnrealMaterialAttr = Prim.GetAttribute( UnrealIdentifiers::MaterialAssignment );
			pxr::UsdShadeMaterial UnrealMaterial;

			pxr::UsdShadeMaterialBindingAPI MaterialBindingAPI{ Prim };
			if ( MaterialBindingAPI )
			{
				// We always emit UnrealMaterials with allpurpose bindings, so we can use default arguments for
				// ComputeBoundMaterial
				if ( pxr::UsdShadeMaterial BoundMaterial = MaterialBindingAPI.ComputeBoundMaterial() )
				{
					UnrealMaterial = BoundMaterial;

					TOptional<FString> ExistingUEAssetReference = UsdUtils::GetUnrealSurfaceOutput( UnrealMaterial.GetPrim() );
					if ( ExistingUEAssetReference.IsSet() )
					{
						UnrealMaterialPrimAssetPath = MoveTemp( ExistingUEAssetReference.GetValue() );
					}
				}
			}

			if ( !UnrealMaterial && ( !UnrealMaterialAttr || !UnrealMaterialAttr.Get<std::string>( &UnrealMaterialAttrAssetPath ) ) )
			{
				return;
			}

			pxr::UsdPrim UnrealMaterialPrim = UnrealMaterial.GetPrim();

			// Prioritize the Unreal material since import will do so too
			FString UnrealMaterialAssetPath = UnrealMaterialPrimAssetPath.IsEmpty()
				? UsdToUnreal::ConvertString( UnrealMaterialAttrAssetPath )
				: UnrealMaterialPrimAssetPath;

			FString BakedFilename = BakedMaterials.FindRef( UnrealMaterialAssetPath );

			// If we have a valid UE asset but just haven't baked it, something went wrong: Just leave everything alone and abort
			if ( !UnrealMaterialAssetPath.IsEmpty() && BakedFilename.IsEmpty() )
			{
				return;
			}

			pxr::SdfPath UnrealMaterialAttrPath = UnrealMaterialAttr ? UnrealMaterialAttr.GetPath() : pxr::SdfPath{};
			pxr::SdfPath UnrealMaterialPrimPath = UnrealMaterial ? UnrealMaterialPrim.GetPrimPath() : pxr::SdfPath{};

			// Find out if we need to remove / author material bindings within an actual variant or outside of it, as an over.
			// We don't do this when using payloads because our override prims aren't inside the actual LOD variants : They just
			// directly override a mesh called e.g. 'LOD3' as if it's a child prim, so that the override automatically only
			// does anything when we happen to have the variant that enables the LOD3 Mesh
			const bool bAuthorInsideVariants = OuterVariantSet.IsSet() && bIsAssetLayer && !bUsePayload;

			if ( bAuthorInsideVariants )
			{
				pxr::UsdVariantSet& OuterVariantSetValue = OuterVariantSet.GetValue();
				pxr::SdfPath VarPrimPath = OuterVariantSetValue.GetPrim().GetPath();
				pxr::SdfPath VarPrimPathWithVar = VarPrimPath.AppendVariantSelection( OuterVariantSetValue.GetName(), OuterVariantSetValue.GetVariantSelection() );

				if ( UnrealMaterialAttrPath.HasPrefix( VarPrimPath ) )
				{
					// This builds a path like '/MyMesh{LOD=LOD0}LOD0.unrealMaterial',
					// or '/MyMesh{LOD=LOD0}LOD0/Section1.unrealMaterial'.This is required because we'll query the layer
					// for a spec path below, and this path must contain the variant selection in it, which the path returned
					// from attr.GetPath() doesn't contain
					UnrealMaterialAttrPath = UnrealMaterialAttrPath.ReplacePrefix( VarPrimPath, VarPrimPathWithVar );
				}

				if ( UnrealMaterialPrimPath.HasPrefix( VarPrimPath ) )
				{
					UnrealMaterialPrimPath = UnrealMaterialPrimPath.ReplacePrefix( VarPrimPath, VarPrimPathWithVar );
				}
			}

			// We always want to replace things in whatever layer they were authored, and not just override with
			// a stronger opinion, so search through all sublayers to find the ones with the specs we are targeting
			for ( pxr::SdfLayerHandle Layer : LayersToTraverse )
			{
				pxr::SdfAttributeSpecHandle UnrealMaterialAttrSpec = Layer->GetAttributeAtPath( UnrealMaterialAttrPath );
				pxr::SdfPrimSpecHandle UnrealMaterialPrimSpec = Layer->GetPrimAtPath( UnrealMaterialPrimPath );
				if ( !UnrealMaterialAttrSpec && !UnrealMaterialPrimSpec )
				{
					continue;
				}

				pxr::UsdEditContext Context{ StageToTraverse, Layer };

				if ( bRemoveUnrealMaterials )
				{
					TOptional<pxr::UsdEditContext> VarContext;
					if ( bAuthorInsideVariants )
					{
						VarContext.Emplace( OuterVariantSet.GetValue().GetVariantEditContext() );
					}

					if ( UnrealMaterialAttrSpec )
					{
						Prim.RemoveProperty( UnrealIdentifiers::MaterialAssignment );
					}

					if ( UnrealMaterialPrimSpec )
					{
						UsdUtils::RemoveUnrealSurfaceOutput( UnrealMaterialPrim, UE::FSdfLayer{ Layer } );
					}
				}

				// It was just an empty UE asset path, so just cancel now as our BakedFilename can't possibly be useful
				if ( UnrealMaterialAssetPath.IsEmpty() )
				{
					continue;
				}

				// Get the proxy prim for the material within this layer
				// (or create one outside the variant edit context)
				pxr::UsdPrim MatPrim;
				{
					pxr::UsdEditContext MatContext( StageToTraverse, pxr::SdfLayerRefPtr{ LayerToAuthorIn } );

					// We are already referencing an unreal material prim: Let's just augment it with a reference to the baked
					// material usd asset layer.
					// Note how this will likely not be within MatPrimScope but instead will be a child of the Mesh/GeomSubset.
					// This is fine, and in the future we'll likely exclusively do this since it will handle mesh-specific
					// material baking much better, as it will allow even having separate bakes for each LOD
					if ( UnrealMaterial && UnrealMaterialPrimSpec )
					{
						MatPrim = UnrealMaterialPrim;

						bool bAlreadyHasReference = false;

						// Make sure we don't reference it more than once. This shouldn't be needed since we'll only ever run into
						// these unreal material prims once per Mesh/GeomSubset, but when creating MatScopePrims we can guarantee we
						// add a reference only once by adding it along with the Material prim creation, so it would be nice to be able to
						// guarantee it here as well
						pxr::SdfReferencesProxy ReferencesProxy = UnrealMaterialPrimSpec->GetReferenceList();
						for ( const pxr::SdfReference& UsdReference : ReferencesProxy.GetAddedOrExplicitItems() )
						{
							FString ReferencedFilePath = UsdToUnreal::ConvertString( UsdReference.GetAssetPath() );
							FString LayerPath = UsdToUnreal::ConvertString( Layer->GetRealPath() );

							if ( !LayerPath.IsEmpty() )
							{
								ReferencedFilePath = FPaths::ConvertRelativePathToFull( LayerPath, ReferencedFilePath );
							}

							if ( FPaths::IsSamePath( ReferencedFilePath, BakedFilename ) )
							{
								bAlreadyHasReference = true;
								break;
							}
						}

						if ( !bAlreadyHasReference )
						{
							UE::FUsdPrim UEMatPrim{ MatPrim };
							UsdUtils::AddReference( UEMatPrim, *BakedFilename );
						}
					}
					// Need a MatScopePrim authored somewhere within this layer
					else
					{
						FMaterialScopePrim* MatPrimScopePtr = nullptr;

						if ( MatPrimScope.IsSet() )
						{
							MatPrimScopePtr = &MatPrimScope.GetValue();
						}
						else
						{
							// On-demand create a *single* material scope prim for the stage, if we're not inside a variant set
							if ( !StageMatScope.IsSet() )
							{
								// If a prim from a stage references another layer, USD's composition will effectively
								// paste the default prim of the referenced layer over the referencing prim. Because of
								// this, the subprims within the hierarchy of that default prim can't ever have
								// relationships to other prims outside that of that same hierarchy, as those prims
								// will not be present on the referencing stage at all. This is why we author our stage
								// materials scope under the default prim, and not the pseudoroot
								StageMatScope = FMaterialScopePrim{ StageToTraverse, StageToTraverse->GetDefaultPrim() };
							}
							MatPrimScopePtr = &StageMatScope.GetValue();
						}

						// This should never happen
						if ( !ensure( MatPrimScopePtr ) )
						{
							continue;
						}

						// We already have a material proxy prim for this UE material within MatPrimScope, so just reuse it
						if ( pxr::UsdPrim* FoundPrim = MatPrimScopePtr->BakedFileNameToMatPrim.Find( BakedFilename ) )
						{
							MatPrim = *FoundPrim;
						}
						// Create a new material proxy prim for this UE material within MatPrimScope
						else
						{
							FString MatName = FPaths::GetBaseFilename( UnrealMaterialAssetPath );
							MatName = UsdToUnreal::ConvertString( pxr::TfMakeValidIdentifier( UnrealToUsd::ConvertString( *MatName ).Get() ) );
							FString MatPrimName = UsdUtils::GetUniqueName( MatName, MatPrimScopePtr->UsedPrimNames );
							MatPrimScopePtr->UsedPrimNames.Add( MatPrimName );

							MatPrim = StageToTraverse->DefinePrim(
								MatPrimScopePtr->Prim.GetPath().AppendChild( UnrealToUsd::ConvertToken( *MatPrimName ).Get() ),
								UnrealToUsd::ConvertToken( TEXT( "Material" ) ).Get()
							);

							// We should only keep track and reuse the material proxy prims that we create within the MatPrimScope, not
							// the ones we have appropriated from within Mesh/GeomSubset from being UnrealPrims
							MatPrimScopePtr->BakedFileNameToMatPrim.Add( BakedFilename, MatPrim );

							UE::FUsdPrim UEMatPrim{ MatPrim };
							UsdUtils::AddReference( UEMatPrim, *BakedFilename );
						}
					}
				}

				// Make sure we have a binding to the material prim and the material binding API
				if( pxr::UsdShadeMaterial MaterialToBind{ MatPrim } )
				{
					TOptional<pxr::UsdEditContext> VarContext;
					if ( bAuthorInsideVariants )
					{
						VarContext.Emplace( OuterVariantSet.GetValue().GetVariantEditContext() );
					}

					if ( pxr::UsdShadeMaterialBindingAPI AppliedMaterialBindingAPI = pxr::UsdShadeMaterialBindingAPI::Apply( Prim ) )
					{
						AppliedMaterialBindingAPI.Bind( MaterialToBind );
					}
				}
			}
		};

	pxr::UsdPrim Root = Stage.GetPseudoRoot();
	TOptional<FMaterialScopePrim> Empty;
	TraverseForMaterialReplacement( UsdStage, Root, Empty, {} );
}

FString UsdUtils::HashGeomMeshPrim( const UE::FUsdStage& Stage, const FString& PrimPath, double TimeCode )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( UsdUtils::HashGeomMeshPrim );

	using namespace pxr;

	FScopedUsdAllocs Allocs;

	UsdPrim UsdPrim = pxr::UsdPrim( Stage.GetPrimAtPath( UE::FSdfPath( *PrimPath ) ) );
	if ( !UsdPrim )
	{
		return {};
	}

	UsdGeomMesh UsdMesh( UsdPrim );
	if ( !UsdMesh )
	{
		return {};
	}

	FMD5 MD5;

	if ( UsdAttribute Points = UsdMesh.GetPointsAttr() )
	{
		VtArray< GfVec3f > PointsArray;
		Points.Get( &PointsArray, TimeCode );
		MD5.Update( ( uint8* ) PointsArray.cdata(), PointsArray.size() * sizeof( GfVec3f ) );
	}

	if ( UsdAttribute NormalsAttribute = UsdMesh.GetNormalsAttr() )
	{
		VtArray< GfVec3f > Normals;
		NormalsAttribute.Get( &Normals, TimeCode );
		MD5.Update( ( uint8* ) Normals.cdata(), Normals.size() * sizeof( GfVec3f ) );
	}

	if ( UsdGeomPrimvar ColorPrimvar = UsdMesh.GetDisplayColorPrimvar() )
	{
		VtArray< GfVec3f > UsdColors;
		ColorPrimvar.ComputeFlattened( &UsdColors, TimeCode );
		MD5.Update( ( uint8* ) UsdColors.cdata(), UsdColors.size() * sizeof( GfVec3f ) );
	}

	if ( UsdGeomPrimvar OpacityPrimvar = UsdMesh.GetDisplayOpacityPrimvar() )
	{
		VtArray< float > UsdOpacities;
		OpacityPrimvar.ComputeFlattened( &UsdOpacities );
		MD5.Update( ( uint8* ) UsdOpacities.cdata(), UsdOpacities.size() * sizeof( float ) );
	}

	// TODO: This is not providing render context or material purpose, so it will never consider float2f primvars
	// for the hash, which could be an issue in very exotic cases
	TArray< TUsdStore< UsdGeomPrimvar > > PrimvarsByUVIndex = UsdUtils::GetUVSetPrimvars( UsdMesh );
	for ( int32 UVChannelIndex = 0; UVChannelIndex < PrimvarsByUVIndex.Num(); ++UVChannelIndex )
	{
		if ( !PrimvarsByUVIndex.IsValidIndex( UVChannelIndex ) )
		{
			break;
		}

		UsdGeomPrimvar& Primvar = PrimvarsByUVIndex[ UVChannelIndex ].Get();
		if ( !Primvar )
		{
			continue;
		}

		VtArray< GfVec2f > UsdUVs;
		Primvar.Get( &UsdUVs, TimeCode );
		MD5.Update( ( uint8* ) UsdUVs.cdata(), UsdUVs.size() * sizeof( GfVec2f ) );
	}

	uint8 Digest[ 16 ];
	MD5.Final( Digest );

	FString Hash;
	for ( int32 i = 0; i < 16; ++i )
	{
		Hash += FString::Printf( TEXT( "%02x" ), Digest[ i ] );
	}
	return Hash;
}

bool UsdUtils::GetPointInstancerTransforms( const FUsdStageInfo& StageInfo, const pxr::UsdGeomPointInstancer& PointInstancer, const int32 ProtoIndex, pxr::UsdTimeCode EvalTime, TArray<FTransform>& OutInstanceTransforms )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( GetPointInstancerTransforms );

	if ( !PointInstancer )
	{
		return false;
	}

	FScopedUsdAllocs UsdAllocs;

	pxr::VtArray< int > ProtoIndices = UsdUtils::GetUsdValue< pxr::VtArray< int > >( PointInstancer.GetProtoIndicesAttr(), EvalTime );

	pxr::VtMatrix4dArray UsdInstanceTransforms;

	// We don't want the prototype root prim's transforms to be included in these, as they'll already be baked into the meshes themselves
	if ( !PointInstancer.ComputeInstanceTransformsAtTime( &UsdInstanceTransforms, EvalTime, EvalTime, pxr::UsdGeomPointInstancer::ExcludeProtoXform ) )
	{
		return false;
	}

	int32 Index = 0;

	FScopedUnrealAllocs UnrealAllocs;

	const int32 NumInstances = GMaxInstancesPerPointInstancer >= 0
		? FMath::Min( static_cast< int32 >( UsdInstanceTransforms.size() ), GMaxInstancesPerPointInstancer )
		: static_cast< int32 >( UsdInstanceTransforms.size() );

	OutInstanceTransforms.Reset( NumInstances );

	for ( pxr::GfMatrix4d& UsdMatrix : UsdInstanceTransforms )
	{
		if ( Index == NumInstances )
		{
			break;
		}

		if ( ProtoIndices[ Index ] == ProtoIndex )
		{
			OutInstanceTransforms.Add( UsdToUnreal::ConvertMatrix( StageInfo, UsdMatrix ) );
		}

		++Index;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK
