// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "UnrealUSDWrapper.h"
#include "USDMemory.h"
#include "UsdWrappers/ForwardDeclarations.h"
#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
	#include "pxr/usd/usdShade/tokens.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class GfMatrix4d;
	class GfVec2f;
	class GfVec3f;
	class GfVec4f;
	class SdfPath;
	class TfToken;
	class TfType;
	class UsdAttribute;
	class UsdPrim;
	class UsdTimeCode;
	class UsdGeomPrimvar;
	class UsdGeomMesh;

	class UsdStage;
	template< typename T > class TfRefPtr;
	using UsdStageRefPtr = TfRefPtr< UsdStage >;

	class SdfPrimSpec;
	template <class T> class SdfHandle;
	using SdfPrimSpecHandle = SdfHandle< SdfPrimSpec >;
PXR_NAMESPACE_CLOSE_SCOPE

#endif // #if USE_USD_SDK

class USceneComponent;
class UUsdAssetImportData;
enum class EUsdDuplicateType : uint8;
enum class EUsdUpAxis : uint8;
struct FUsdUnrealAssetInfo;
namespace UE
{
	class FUsdPrim;
}
namespace UsdUtils
{
	struct FUsdPrimMaterialAssignmentInfo;
}

namespace UsdUtils
{
	template<typename T>
	T* FindOrCreateObject( UObject* InParent, const FString& InName, EObjectFlags Flags )
	{
		T* Object = FindObject<T>(InParent, *InName);

		if (!Object)
		{
			Object = NewObject<T>(InParent, FName(*InName), Flags);
		}

		return Object;
	}

	/** Case sensitive hashing function for TMap */
	template <typename ValueType>
	struct FCaseSensitiveStringMapFuncs : BaseKeyFuncs<ValueType, FString, /*bInAllowDuplicateKeys*/false>
	{
		static FORCEINLINE const FString& GetSetKey( const TPair<FString, ValueType>& Element )
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches( const FString& A, const FString& B )
		{
			return A.Equals( B, ESearchCase::CaseSensitive );
		}
		static FORCEINLINE uint32 GetKeyHash( const FString& Key )
		{
			return FCrc::StrCrc32<TCHAR>( *Key );
		}
	};

#if USE_USD_SDK
	template< typename ValueType >
	USDUTILITIES_API ValueType GetUsdValue( const pxr::UsdAttribute& Attribute, pxr::UsdTimeCode TimeCode );

	USDUTILITIES_API pxr::TfToken GetUsdStageUpAxis( const pxr::UsdStageRefPtr& Stage );
	USDUTILITIES_API EUsdUpAxis GetUsdStageUpAxisAsEnum( const pxr::UsdStageRefPtr& Stage );

	USDUTILITIES_API void SetUsdStageUpAxis( const pxr::UsdStageRefPtr& Stage, pxr::TfToken Axis );
	USDUTILITIES_API void SetUsdStageUpAxis( const pxr::UsdStageRefPtr& Stage, EUsdUpAxis Axis );

	USDUTILITIES_API float GetUsdStageMetersPerUnit( const pxr::UsdStageRefPtr& Stage );
	USDUTILITIES_API void SetUsdStageMetersPerUnit( const pxr::UsdStageRefPtr& Stage, float MetersPerUnit );

	USDUTILITIES_API int32 GetUsdStageNumFrames( const pxr::UsdStageRefPtr& Stage );

	USDUTILITIES_API bool HasCompositionArcs( const pxr::UsdPrim& Prim );
	USDUTILITIES_API bool HasCompositionArcs( const pxr::SdfPrimSpecHandle& PrimSpec );

	USDUTILITIES_API UClass* GetActorTypeForPrim( const pxr::UsdPrim& Prim );
	USDUTILITIES_API UClass* GetComponentTypeForPrim( const pxr::UsdPrim& Prim );

	/** Returns the USD schema name that should be used when exporting Component (e.g. "Xform", "Mesh", "Camera", etc.) */
	USDUTILITIES_API FString GetSchemaNameForComponent( const USceneComponent& Component );

	/**
	 * Returns a prim path to use when exporting a given ActorOrComponent, assuming we're exporting according to the component attachment hierarchy.
	 * e.g. We have a top-level actor with label "Actor" with an attached child actor "Child" that has a component "Comp1" attached to its root, and "Comp2"
	 * attached to "Comp1". If Comp2 is provided to this function, it will return "/Root/Actor/Child/Comp1/Comp2".
	 *
	 * The actor label is always used in place of the root component name.
	 *
	 * ParentPrimPath is optional, and without it the function will recurse upwards and build the full path all the way to the root. Actor folders
	 * are handled just like if they were actors/components
	 */
	USDUTILITIES_API FString GetPrimPathForObject( const UObject* ActorOrComponent, const FString& ParentPrimPath = TEXT(""), bool bUseActorFolders = false );

	USDUTILITIES_API TUsdStore< pxr::TfToken > GetUVSetName( int32 UVChannelIndex );

	/**
	 * Heuristic to try and guess what UV index we should assign this primvar to.
	 * We need something like this because one material may use st0, and another st_0 (both meaning the same thing),
	 * but a mesh that binds both materials may interpret these as targeting completely different UV sets
	 * @param PrimvarName - Name of the primvar that should be used as UV set
	 * @return UV index that should be used for this primvar
	 */
	USDUTILITIES_API int32 GetPrimvarUVIndex( FString PrimvarName );

	/**
	 * Gets the names of the primvars that should be used as UV sets, per index, for this mesh.
	 * (e.g. first item of array is primvar for UV set 0, second for UV set 1, etc).
	 * This overload will only return primvars with 'texcoord2f' role.	 *
	 * @param UsdMesh - Mesh that contains primvars that can be used as texture coordinates.
	 * @return Array where each index gives the primvar that should be used for that UV index
	 */
	USDUTILITIES_API TArray< TUsdStore< pxr::UsdGeomPrimvar > > GetUVSetPrimvars( const pxr::UsdGeomMesh& UsdMesh );

	/**
	 * Gets the names of the primvars that should be used as UV sets, per index, for this mesh.
	 * (e.g. first item of array is primvar for UV set 0, second for UV set 1, etc).
	 * This overload will only return primvars with 'texcoord2f' role.	 *
	 * @param UsdMesh - Mesh that contains primvars that can be used as texture coordinates.
	 * @param MaterialToPrimvarsUVSetNames - Maps from a material prim path, to pairs indicating which primvar names are used as 'st' coordinates, and which UVIndex the imported material will sample from (e.g. ["st0", 0], ["myUvSet2", 2], etc). These are supposed to be the materials used by the mesh, and we do this because it helps identify which primvars are valid/used as texture coordinates, as the user may have these named as 'myUvSet2' and still expect it to work
	 * @param UsdMeshMaterialAssignmentInfo - Result of calling GetPrimMaterialAssignments on UsdMesh's Prim. This can be provided or will be retrieved on-demand using RenderContext
	 * @param RenderContext - Render context to use when traversing through material shaders looking for used primvars
	 * @param MaterialPurpose - Specific material purpose to use when parsing the UsdMesh's material bindings
	 * @return Array where each index gives the primvar that should be used for that UV index
	 */
	USDUTILITIES_API TArray< TUsdStore< pxr::UsdGeomPrimvar > > GetUVSetPrimvars(
		const pxr::UsdGeomMesh& UsdMesh,
		const TMap< FString, TMap< FString, int32 > >& MaterialToPrimvarsUVSetNames,
		const pxr::TfToken& RenderContext = pxr::UsdShadeTokens->universalRenderContext,
		const pxr::TfToken& MaterialPurpose = pxr::UsdShadeTokens->allPurpose
	);
	USDUTILITIES_API TArray< TUsdStore< pxr::UsdGeomPrimvar > > GetUVSetPrimvars( const pxr::UsdGeomMesh& UsdMesh, const TMap< FString, TMap< FString, int32 > >& MaterialToPrimvarsUVSetNames, const UsdUtils::FUsdPrimMaterialAssignmentInfo& UsdMeshMaterialAssignmentInfo );

	USDUTILITIES_API bool IsAnimated( const pxr::UsdPrim& Prim );
	USDUTILITIES_API bool HasAnimatedVisibility( const pxr::UsdPrim& Prim );

	/** Returns whether Prim belongs to any of the default kinds, or a kind derived from them. The result can be a union of different kinds. */
	USDUTILITIES_API EUsdDefaultKind GetDefaultKind( const pxr::UsdPrim& Prim );

	/**
	 * Sets the prim kind for Prim to the provided NewKind.
	 * Differs from IUsdPrim::SetKind in that we don't have to provide the USD token.
	 * @param NewKind - New kind to set. Must be a single flag, and not a combination of multiple plags
	 * @param Prim - Prim to receive the new Kind
	 * @return Whether we managed to set the kind or not.
	 */
	USDUTILITIES_API bool SetDefaultKind( pxr::UsdPrim& Prim, EUsdDefaultKind NewKind );

	/**
	 * Returns all prims of type SchemaType (or a descendant type) in the subtree of prims rooted at StartPrim.
	 * Stops going down the subtrees when it hits a schema type to exclude.
	 */
	USDUTILITIES_API TArray< TUsdStore< pxr::UsdPrim > > GetAllPrimsOfType( const pxr::UsdPrim& StartPrim, const pxr::TfType& SchemaType, const TArray< TUsdStore< pxr::TfType > >& ExcludeSchemaTypes = {} );
	USDUTILITIES_API TArray< TUsdStore< pxr::UsdPrim > > GetAllPrimsOfType( const pxr::UsdPrim& StartPrim, const pxr::TfType& SchemaType, TFunction< bool( const pxr::UsdPrim& ) > PruneChildren, const TArray< TUsdStore< pxr::TfType > >& ExcludeSchemaTypes = {} );

	USDUTILITIES_API FString GetAssetPathFromPrimPath( const FString& RootContentPath, const pxr::UsdPrim& Prim );
#endif // #if USE_USD_SDK

	USDUTILITIES_API TArray< UE::FUsdPrim > GetAllPrimsOfType( const UE::FUsdPrim& StartPrim, const TCHAR* SchemaName );
	USDUTILITIES_API TArray< UE::FUsdPrim > GetAllPrimsOfType( const UE::FUsdPrim& StartPrim, const TCHAR* SchemaName, TFunction< bool( const UE::FUsdPrim& ) > PruneChildren, const TArray<const TCHAR*>& ExcludeSchemaNames = {} );

	/** Returns the time code for non-timesampled values. Usually a quiet NaN. */
	USDUTILITIES_API double GetDefaultTimeCode();

	/** Returns the earliest possible timecode. Use it to always fetch the first frame of an animated attribute */
	USDUTILITIES_API double GetEarliestTimeCode();

	USDUTILITIES_API UUsdAssetImportData* GetAssetImportData( UObject* Asset );

	/** Adds a reference on Prim to the layer at AbsoluteFilePath */
	USDUTILITIES_API void AddReference( UE::FUsdPrim& Prim, const TCHAR* AbsoluteFilePath, const UE::FSdfPath& TargetPrimPath = {}, double TimeCodeOffset = 0.0, double TimeCodeScale = 1.0 );

	/** Adds a payload on Prim pointing at the default prim of the layer at AbsoluteFilePath */
	USDUTILITIES_API void AddPayload( UE::FUsdPrim& Prim, const TCHAR* AbsoluteFilePath, const UE::FSdfPath& TargetPrimPath = {}, double TimeCodeOffset = 0.0, double TimeCodeScale = 1.0 );

	/**
	 * Renames a single prim to a new name
	 * WARNING: This will lead to issues if called from within a pxr::SdfChangeBlock. This because it needs to be able
	 * to send separate notices: One notice about the renaming, that the transactor can record on the current edit target,
	 * and one extra notice about the definition of an auxiliary prim on the session layer, that the transactor *must* record
	 * as having taken place on the session layer.
	 * @param Prim - Prim to rename
	 * @param NewPrimName - New name for the prim e.g. "MyNewName"
	 * @return True if we managed to rename
	 */
	USDUTILITIES_API bool RenamePrim( UE::FUsdPrim& Prim, const TCHAR* NewPrimName );

	/**
	 * Removes any numbered suffix, followed by any number of underscores (e.g. Asset_2, Asset__232_31 or Asset94 all become 'Asset'), making
	 * sure the string is kept at least one character long. Returns true if it removed anything.
	 */
	USDUTILITIES_API bool RemoveNumberedSuffix( FString& Prefix );

	/**
	 * Appends numbered suffixes to Name until the result is not contained in UsedNames, and returns it.
	 * Does not add the result to UsedNames before returning (as it is const).
	 * @param Name - Received string to make unique (e.g. "MyName")
	 * @param UsedNames - Strings that cannot be used for the result
	 * @return Modified Name so that it doesn't match anything in UsedNames (e.g. "MyName" again, or "MyName_0" or "MyName_423")
	 */
	USDUTILITIES_API FString GetUniqueName( FString Name, const TSet<FString>& UsedNames );

#if USE_USD_SDK
	/**
	 * Returns a sanitized, valid version of 'InName' that can be used as the name for a child prim of 'ParentPrim'.
	 * Returns the empty string in case of an error.
	 */
	USDUTILITIES_API FString GetValidChildName( FString InName, const pxr::UsdPrim& ParentPrim );
#endif // #if USE_USD_SDK

	/**
	 * Returns a modified version of InIdentifier that can be used as a USD prim or property name.
	 * This means only allowing letters, numbers and the underscore character. All others are replaced with underscores.
	 * Additionally, the first character cannot be a number.
	 * Note that this obviously doesn't check for a potential name collision.
	 */
	USDUTILITIES_API FString SanitizeUsdIdentifier( const TCHAR* InIdentifier );

	/** Will call UsdGeomImageable::MakeVisible/MakeInvisible if Prim is a UsdGeomImageable */
	USDUTILITIES_API void MakeVisible( UE::FUsdPrim& Prim, double TimeCode = UsdUtils::GetDefaultTimeCode() );
	USDUTILITIES_API void MakeInvisible( UE::FUsdPrim& Prim, double TimeCode = UsdUtils::GetDefaultTimeCode() );

	/** Returns if the ComputedVisibility for Prim says it should be visible */
	USDUTILITIES_API bool IsVisible( const UE::FUsdPrim& Prim, double TimeCode = UsdUtils::GetDefaultTimeCode() );

	/** Returns whether Prim has visibility set to 'inherited' */
	USDUTILITIES_API bool HasInheritedVisibility( const UE::FUsdPrim& Prim, double TimeCode = UsdUtils::GetDefaultTimeCode() );

	/**
	 * Travels up from Prim and returns true if we hit any invisible prim before we hit the stage pseudoroot.
	 * Does not check `Prim` itself (or `RootPrim`, so that you can just pass the PseudoRoot in most cases).
	 * Completely ignores whether non-UsdGeomImageable prims are visible or not and keeps travelling up.
	 */
	USDUTILITIES_API bool HasInvisibleParent( const UE::FUsdPrim& Prim, const UE::FUsdPrim& RootPrim, double TimeCode = UsdUtils::GetDefaultTimeCode() );

	/**
	 * Returns a path exactly like Prim.GetPrimPath(), except that if the prim is within variant sets, it will return the
	 * full path with variant selections in it (i.e. the spec path), like "/Root/Child{Varset=Var}Inner" instead of just
	 * "/Root/Child/Inner".
	 *
	 * It needs a layer because it is possible for a prim to be defined within a variant set in some layer, but then
	 * have an 'over' opinion defined in another layer without a variant, meaning the actual spec path depends on the layer.
	 *
	 * Note that stage operations that involve manipulating specs require this full path instead (like removing/renaming prims),
	 * while other operations need the path with the stripped variant selections (like getting/defining/overriding prims)
	 *
	 * Returns an empty path in case the layer doesn't have a spec for this prim.
	 */
	USDUTILITIES_API UE::FSdfPath GetPrimSpecPathForLayer( const UE::FUsdPrim& Prim, const UE::FSdfLayer& Layer );

	/**
	 * Removes all the prim specs for Prim on the given Layer, if the layer belongs to the stage's local layer stack.
	 *
	 * This function is useful in case the prim is inside a variant set: In that case, just calling FUsdStage::RemovePrim()
	 * will attempt to remove the "/Root/Example/Child", which wouldn't remove the "/Root{Varset=Var}Example/Child" spec,
	 * meaning the prim may still be left on the stage. Note that it's even possible to have both of those specs at the same time:
	 * for example when we have a prim inside a variant set, but outside of it we have overrides to the same prim. This function
	 * will remove both.
	 *
	 * @param Prim - Prim to remove
	 * @param Layer - Layer to remove prim specs from. This can be left with the invalid layer (default) in order to remove all
	 *				  specs from the entire stage's local layer stack.
	 */
	USDUTILITIES_API void RemoveAllLocalPrimSpecs( const UE::FUsdPrim& Prim, const UE::FSdfLayer& Layer = UE::FSdfLayer{} );

	/**
	 * Copies flattened versions of the input prims onto the clipboard stage and removes all the prim specs for Prims from their stages.
	 * These cut prims can then be pasted with PastePrims.
	 *
	 * @param Prims - Prims to cut
	 * @return True if we managed to cut
	 */
	USDUTILITIES_API bool CutPrims( const TArray<UE::FUsdPrim>& Prims );

	/**
	 * Copies flattened versions of the input prims onto the clipboard stage.
	 * These copied prims can then be pasted with PastePrims.
	 *
	 * @param Prims - Prims to copy
	 * @return True if we managed to copy
	 */
	USDUTILITIES_API bool CopyPrims( const TArray<UE::FUsdPrim>& Prims );

	/**
	 * Pastes the prims from the clipboard stage as children of ParentPrim.
	 *
	 * The pasted prims may be renamed in order to have valid names for the target location, which is why this function
	 * returns the pasted prim paths.
	 * This function returns just paths instead of actual prims because USD needs to respond to the notices about
	 * the created prim specs before the prims are fully created, which means we wouldn't be able to return the
	 * created prims yet, in case this function was called from within an SdfChangeBlock.
	 *
	 * @param ParentPrim - Prim that will become parent to the pasted prims
	 * @return Paths to the pasted prim specs, after they were added as children of ParentPrim
	 */
	USDUTILITIES_API TArray<UE::FSdfPath> PastePrims( const UE::FUsdPrim& ParentPrim );

	/** Returns true if we have prims that we can paste within our clipboard stage */
	USDUTILITIES_API bool CanPastePrims();

	/** Clears all prims from our clipboard stage */
	USDUTILITIES_API void ClearPrimClipboard();

	/**
	 * Duplicates all provided Prims one-by-one, performing the requested DuplicateType.
	 * See the documentation on EUsdDuplicateType for the different operation types.
	 *
	 * The duplicated prims may be renamed in order to have valid names for the target location, which is why this
	 * function returns the pasted prim paths.
	 * This function returns just paths instead of actual prims because USD needs to respond to the notices about
	 * the created prim specs before the prims are fully created, which means we wouldn't be able to return the
	 * created prims yet, in case this function was called from within an SdfChangeBlock.
	 *
	 * @param Prims - Prims to duplicate
	 * @param DuplicateType - Type of prim duplication to perform
	 * @param TargetLayer - Target layer to use when duplicating, if relevant for that duplication type
	 * @return Paths to the duplicated prim specs, after they were added as children of ParentPrim.
	 */
	USDUTILITIES_API TArray<UE::FSdfPath> DuplicatePrims(
		const TArray<UE::FUsdPrim>& Prims,
		EUsdDuplicateType DuplicateType,
		const UE::FSdfLayer& TargetLayer = UE::FSdfLayer{}
	);

	/** Adds to Prim the assetInfo metadata the values described in Info */
	USDUTILITIES_API void SetPrimAssetInfo( UE::FUsdPrim& Prim, const FUsdUnrealAssetInfo& Info );

	/** Retrieves from Prim the assetInfo metadata values that we use as export metadata, when exporting Unreal assets */
	USDUTILITIES_API FUsdUnrealAssetInfo GetPrimAssetInfo( const UE::FUsdPrim& Prim );
}

