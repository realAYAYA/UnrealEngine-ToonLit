// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "Modules/ModuleInterface.h"
#include "Templates/Tuple.h"
#include "UObject/ObjectMacros.h"
#include "USDMemory.h"

#include "UsdWrappers/ForwardDeclarations.h"

#include <string>
#include <vector>
#include <memory>

#if USE_USD_SDK
#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"
#endif // #if USE_USD_SDK

#if USE_USD_SDK
PXR_NAMESPACE_OPEN_SCOPE
	class GfMatrix4d;
	class SdfPath;
	class TfToken;
	class UsdAttribute;
	class UsdGeomMesh;
	class UsdPrim;
	class UsdStage;
	class UsdStageCache;

	template< typename T > class TfRefPtr;
PXR_NAMESPACE_CLOSE_SCOPE
#endif // #if USE_USD_SDK

class IUsdPrim;
class FUsdDiagnosticDelegate;

namespace UE
{
	class FUsdAttribute;
}

enum class EUsdInterpolationMethod
{
	/** Each element in a buffer maps directly to a specific vertex */
	Vertex,
	/** Each element in a buffer maps to a specific face/vertex pair */
	FaceVarying,
	/** Each vertex on a face is the same value */
	Uniform,
	/** Single value */
	Constant
};

enum class EUsdGeomOrientation
{
	/** Right handed coordinate system */
	RightHanded,
	/** Left handed coordinate system */
	LeftHanded,
};

enum class EUsdSubdivisionScheme
{
	None,
	CatmullClark,
	Loop,
	Bilinear,
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EUsdPurpose : int32
{
	Default = 0 UMETA(Hidden),
	Proxy = 1,
	Render = 2,
	Guide = 4
};
ENUM_CLASS_FLAGS(EUsdPurpose);

UENUM( meta = ( Bitflags, UseEnumValuesAsMaskValuesInEditor = "true" ) )
enum class EUsdDefaultKind : int32
{
	None = 0 UMETA( Hidden ),
	Model = 1,
	Component = 2,
	Group = 4,
	Assembly = 8,
	Subcomponent = 16
};
ENUM_CLASS_FLAGS( EUsdDefaultKind );


struct FUsdVector2Data
{
	UE_DEPRECATED( 5.0, "Not used anymore" )
	FUsdVector2Data(float InX = 0, float InY = 0)
		: X(InX)
		, Y(InY)
	{}

	float X;
	float Y;
};

struct FUsdVectorData
{
	UE_DEPRECATED( 5.0, "Not used anymore" )
	FUsdVectorData(float InX = 0, float InY = 0, float InZ = 0)
		: X(InX)
		, Y(InY)
		, Z(InZ)
	{}

	float X;
	float Y;
	float Z;
};

struct FUsdVector4Data
{
	UE_DEPRECATED( 5.0, "Not used anymore" )
	FUsdVector4Data(float InX = 0, float InY = 0, float InZ = 0, float InW = 0)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{}

	float X;
	float Y;
	float Z;
	float W;
};

struct FUsdUVData
{
	UE_DEPRECATED( 5.0, "Not used anymore" )
	FUsdUVData()
	{}

	/** Defines how UVs are mapped to faces */
	EUsdInterpolationMethod UVInterpMethod;

	/** Raw UVs */
	std::vector<FUsdVector2Data> Coords;
};

struct FUsdQuatData
{
	UE_DEPRECATED( 5.0, "Not used anymore" )
	FUsdQuatData(float InX=0, float InY = 0, float InZ = 0, float InW = 0)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{}

	float X;
	float Y;
	float Z;
	float W;
};

UENUM()
enum class EUsdInitialLoadSet : uint8
{
	LoadAll,
	LoadNone
};

UENUM()
enum class EUsdInterpolationType : uint8
{
	Held,
	Linear
};

UENUM()
enum class EUsdRootMotionHandling
{
	// Use for the root bone just its regular joint animation as described on the SkelAnimation prim.
	NoAdditionalRootMotion,

	// Use the transform animation from the SkelRoot prim in addition to the root bone joint animation as
	// described on the SkelAnimation prim. Note that the SkelRoot prim's Sequencer transform track will no longer
	// contain the transform animation data used in this manner, so as to not apply the animation twice.
	UseMotionFromSkelRoot,

	// Use the transform animation from the Skeleton prim in addition to the root bone joint animation as
	// described on the SkelAnimation prim.
	UseMotionFromSkeleton
};

class IUnrealUSDWrapperModule : public IModuleInterface
{
};

class UnrealUSDWrapper
{
public:
#if USE_USD_SDK
	UNREALUSDWRAPPER_API static double GetDefaultTimeCode();
#endif  // #if USE_USD_SDK

	/**
	 * Returns the file extensions of all file formats supported by USD.
	 *
	 * These include the extensions for the file formats built into USD as
	 * well as those for any other file formats introduced through the USD
	 * plugin system (e.g. "abc" for the Alembic file format of the usdAbc
	 * plugin).
	 */
	UNREALUSDWRAPPER_API static TArray<FString> GetAllSupportedFileFormats();

	/**
	 * Returns the file extensions that are native to USD.
	 *
	 * These are the extensions for the file formats built into USD (i.e. "usd", "usda", "usdc", and "usdz").
	 */
	UNREALUSDWRAPPER_API static TArray<FString> GetNativeFileFormats();

	/**
	 * Opens a USD stage from a file on disk or existing layers, with a population mask or not.
	 * @param Identifier - Path to a file that the USD SDK can open (or the identifier of a root layer), which will become the root layer of the new stage
	 * @param RootLayer - Existing root layer to use for the new stage, instead of reading it from disk
	 * @param SessionLayer - Existing session layer to use for the new stage, instead of creating a new one
	 * @param InitialLoadSet - How to handle USD payloads when opening this stage
	 * @param PopulationMask - List of prim paths to import, following the USD population mask rules
	 * @param bUseStageCache - If true, and the stage is already opened in the stage cache (or the layers are already loaded in the registry) then
	 *						   the file reading may be skipped, and the existing stage returned. When false, the stage and all its referenced layers
	 *						   will be re-read anew, and the stage will not be added to the stage cache.
	 * @param bForceReloadLayersFromDisk - USD layers are always cached in the layer registry, so trying to reopen an
	 *                                     already opened layer will just fetch it from memory (potentially with
	 *                                     edits). If this is true, all local layers used by the stage will be force-
	 *                                     reloaded from disk
	 * @return The opened stage, which may be invalid.
	 */
	UNREALUSDWRAPPER_API static UE::FUsdStage OpenStage(
		const TCHAR* Identifier,
		EUsdInitialLoadSet InitialLoadSet,
		bool bUseStageCache = true,
		bool bForceReloadLayersFromDisk = false
	);
	UNREALUSDWRAPPER_API static UE::FUsdStage OpenStage(
		UE::FSdfLayer RootLayer,
		UE::FSdfLayer SessionLayer,
		EUsdInitialLoadSet InitialLoadSet,
		bool bUseStageCache = true,
		bool bForceReloadLayersFromDisk = false
	);
	UNREALUSDWRAPPER_API static UE::FUsdStage OpenMaskedStage(
		const TCHAR* Identifier,
		EUsdInitialLoadSet InitialLoadSet,
		const TArray<FString>& PopulationMask,
		bool bForceReloadLayersFromDisk = false
	);
	UNREALUSDWRAPPER_API static UE::FUsdStage OpenMaskedStage(
		UE::FSdfLayer RootLayer,
		UE::FSdfLayer SessionLayer,
		EUsdInitialLoadSet InitialLoadSet,
		const TArray<FString>& PopulationMask,
		bool bForceReloadLayersFromDisk = false
	);

	/** Creates a new USD root layer file, opens it as a new stage and returns that stage */
	UNREALUSDWRAPPER_API static UE::FUsdStage NewStage( const TCHAR* FilePath );

	/** Creates a new memory USD root layer, opens it as a new stage and returns that stage */
	UNREALUSDWRAPPER_API static UE::FUsdStage NewStage();

	/**
	 * Get the singleton, persistent stage used as a clipboard for prim cut/copy/paste operations.
	 * WARNING: This stage may remain open indefinitely! If you use this directly, be aware that there may be unintended
	 * consequences (e.g. a sublayer added to this stage may never fully close)
	 */
	UNREALUSDWRAPPER_API static UE::FUsdStage GetClipboardStage();

	/** Returns all the stages that are currently opened in the USD utils stage cache, shared between C++ and Python */
	UNREALUSDWRAPPER_API static TArray< UE::FUsdStage > GetAllStagesFromCache();

	/** Removes the stage from the stage cache. See UsdStageCache::Erase. */
	UNREALUSDWRAPPER_API static void EraseStageFromCache( const UE::FUsdStage& Stage );

	/** Starts listening to error/warning/log messages emitted by USD */
	UNREALUSDWRAPPER_API static void SetupDiagnosticDelegate();

	/** Stops listening to error/warning/log messages emitted by USD */
	UNREALUSDWRAPPER_API static void ClearDiagnosticDelegate();

private:
	static TUniquePtr<FUsdDiagnosticDelegate> Delegate;
};

class FUsdAttribute
{
public:
#if USE_USD_SDK

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static std::string GetUnrealPropertyPath(const pxr::UsdAttribute& Attribute);

	// Get the number of elements in the array if it is an array.  Otherwise -1
	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static int GetArraySize(const pxr::UsdAttribute& Attribute);

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsInt(int64_t& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsUnsignedInt(uint64_t& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsDouble(double& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsString(const char*& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsBool(bool& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsVector2(FUsdVector2Data& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsVector3(FUsdVectorData& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsVector4(FUsdVector4Data& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool AsColor(FUsdVector4Data& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex = -1, double Time = UnrealUSDWrapper::GetDefaultTimeCode());

	UE_DEPRECATED( 5.0, "Prefer using the utilities in USDConversionUtils.h or the actual pxr::UsdAttribute wrapper with the same name at UsdWrappers/UsdAttribute.h" )
	UNREALUSDWRAPPER_API static bool IsUnsigned(const pxr::UsdAttribute& Attribute);

#endif // #if USE_USD_SDK
};



class IUsdPrim
{
public:
#if USE_USD_SDK
	static UNREALUSDWRAPPER_API bool IsValidPrimName(const FString& Name, FText& OutReason);

	static UNREALUSDWRAPPER_API EUsdPurpose GetPurpose(const pxr::UsdPrim& Prim, bool bComputed = true);

	static UNREALUSDWRAPPER_API bool HasGeometryData(const pxr::UsdPrim& Prim);
	static UNREALUSDWRAPPER_API bool HasGeometryDataOrLODVariants(const pxr::UsdPrim& Prim);
	static UNREALUSDWRAPPER_API int GetNumLODs(const pxr::UsdPrim& Prim);

	static UNREALUSDWRAPPER_API bool IsKindChildOf(const pxr::UsdPrim& Prim, const std::string& InBaseKind);
	static UNREALUSDWRAPPER_API pxr::TfToken GetKind(const pxr::UsdPrim& Prim);
	static UNREALUSDWRAPPER_API bool SetKind(const pxr::UsdPrim& Prim, const pxr::TfToken& Kind);
	static UNREALUSDWRAPPER_API bool ClearKind(const pxr::UsdPrim& Prim);

	static UNREALUSDWRAPPER_API pxr::GfMatrix4d GetLocalTransform(const pxr::UsdPrim& Prim);
	static UNREALUSDWRAPPER_API pxr::GfMatrix4d GetLocalToWorldTransform(const pxr::UsdPrim& Prim );
	static UNREALUSDWRAPPER_API pxr::GfMatrix4d GetLocalToWorldTransform(const pxr::UsdPrim& Prim, double Time );
	static UNREALUSDWRAPPER_API pxr::GfMatrix4d GetLocalToWorldTransform(const pxr::UsdPrim& Prim, double Time, const pxr::SdfPath& AbsoluteRootPath);
	static UNREALUSDWRAPPER_API bool HasTransform(const pxr::UsdPrim& Prim);

	UE_DEPRECATED( 5.0, "Not used anymore" )
	static UNREALUSDWRAPPER_API bool IsUnrealProperty(const pxr::UsdPrim& Prim);

	UE_DEPRECATED( 5.0, "Not used anymore" )
	static UNREALUSDWRAPPER_API std::string GetUnrealPropertyPath(const pxr::UsdPrim& Prim);

	UE_DEPRECATED( 5.0, "Not used anymore" )
	static UNREALUSDWRAPPER_API TArray< UE::FUsdAttribute > GetUnrealPropertyAttributes(const pxr::UsdPrim& Prim);

	UE_DEPRECATED( 5.0, "Not used anymore" )
	static UNREALUSDWRAPPER_API std::string GetUnrealAssetPath(const pxr::UsdPrim& Prim);

	UE_DEPRECATED( 5.0, "Not used anymore" )
	static UNREALUSDWRAPPER_API std::string GetUnrealActorClass(const pxr::UsdPrim& Prim);

	static UNREALUSDWRAPPER_API bool SetActiveLODIndex(const pxr::UsdPrim& Prim, int LODIndex);

	static UNREALUSDWRAPPER_API EUsdGeomOrientation GetGeometryOrientation(const pxr::UsdGeomMesh& Mesh);
	static UNREALUSDWRAPPER_API EUsdGeomOrientation GetGeometryOrientation(const pxr::UsdGeomMesh& Mesh, double Time);
#endif // #if USE_USD_SDK
};

namespace UnrealIdentifiers
{
#if USE_USD_SDK
	extern UNREALUSDWRAPPER_API const pxr::TfToken LOD;

	/* Attribute name when assigning Unreal materials to UsdGeomMeshes */
	extern UNREALUSDWRAPPER_API const pxr::TfToken MaterialAssignment;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Unreal;

	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealNaniteOverride;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealNaniteOverrideEnable;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealNaniteOverrideDisable;

	extern UNREALUSDWRAPPER_API const pxr::TfToken LiveLinkAPI;
	extern UNREALUSDWRAPPER_API const pxr::TfToken ControlRigAPI;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealAnimBlueprintPath;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealLiveLinkSubjectName;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealLiveLinkEnabled;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealControlRigPath;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealUseFKControlRig;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealControlRigReduceKeys;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealControlRigReductionTolerance;

	extern UNREALUSDWRAPPER_API const pxr::TfToken DiffuseColor;
	extern UNREALUSDWRAPPER_API const pxr::TfToken EmissiveColor;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Metallic;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Roughness;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Opacity;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Normal;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Specular;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Anisotropy;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Tangent;
	extern UNREALUSDWRAPPER_API const pxr::TfToken SubsurfaceColor;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Occlusion;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Refraction;

	// Tokens used mostly for shade material conversion
	extern UNREALUSDWRAPPER_API const pxr::TfToken Surface;
	extern UNREALUSDWRAPPER_API const pxr::TfToken St;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Varname;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Result;
	extern UNREALUSDWRAPPER_API const pxr::TfToken File;
	extern UNREALUSDWRAPPER_API const pxr::TfToken WrapS;
	extern UNREALUSDWRAPPER_API const pxr::TfToken WrapT;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Repeat;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Mirror;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Clamp;
	extern UNREALUSDWRAPPER_API const pxr::TfToken Fallback;
	extern UNREALUSDWRAPPER_API const pxr::TfToken R;
	extern UNREALUSDWRAPPER_API const pxr::TfToken RGB;

	// Tokens copied from usdImaging, because at the moment it's all we need from it
	extern UNREALUSDWRAPPER_API const pxr::TfToken UsdPreviewSurface;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UsdPrimvarReader_float2;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UsdPrimvarReader_float3;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UsdUVTexture;

	// Token used to indicate that a material parsed from a material prim should use world space normals
	extern UNREALUSDWRAPPER_API const pxr::TfToken WorldSpaceNormals;

	extern UNREALUSDWRAPPER_API const pxr::TfToken GroomAPI;
	extern UNREALUSDWRAPPER_API const pxr::TfToken GroomBindingAPI;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealGroomToBind;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealGroomReferenceMesh;

	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealContentPath;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealAssetType;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealExportTime;
	extern UNREALUSDWRAPPER_API const pxr::TfToken UnrealEngineVersion;
#endif // #if USE_USD_SDK

	extern UNREALUSDWRAPPER_API const TCHAR* LayerSavedComment;

	extern UNREALUSDWRAPPER_API const TCHAR* Invisible;
	extern UNREALUSDWRAPPER_API const TCHAR* Inherited;
	extern UNREALUSDWRAPPER_API const TCHAR* IdentifierPrefix;

	// USceneComponent properties
	extern UNREALUSDWRAPPER_API FName TransformPropertyName;
	extern UNREALUSDWRAPPER_API FName HiddenInGamePropertyName;
	extern UNREALUSDWRAPPER_API FName HiddenPropertyName;

	// UCineCameraComponent properties
	extern UNREALUSDWRAPPER_API FName CurrentFocalLengthPropertyName;
	extern UNREALUSDWRAPPER_API FName ManualFocusDistancePropertyName;
	extern UNREALUSDWRAPPER_API FName CurrentAperturePropertyName;
	extern UNREALUSDWRAPPER_API FName SensorWidthPropertyName;
	extern UNREALUSDWRAPPER_API FName SensorHeightPropertyName;

	// LightComponentBase properties
	extern UNREALUSDWRAPPER_API FName IntensityPropertyName;
	extern UNREALUSDWRAPPER_API FName LightColorPropertyName;

	// LightComponent properties
	extern UNREALUSDWRAPPER_API FName UseTemperaturePropertyName;
	extern UNREALUSDWRAPPER_API FName TemperaturePropertyName;

	// URectLightComponent properties
	extern UNREALUSDWRAPPER_API FName SourceWidthPropertyName;
	extern UNREALUSDWRAPPER_API FName SourceHeightPropertyName;

	// UPointLightComponent properties
	extern UNREALUSDWRAPPER_API FName SourceRadiusPropertyName;

	// USpotLightComponent properties
	extern UNREALUSDWRAPPER_API FName OuterConeAnglePropertyName;
	extern UNREALUSDWRAPPER_API FName InnerConeAnglePropertyName;

	// UDirectionalLightComponent properties
	extern UNREALUSDWRAPPER_API FName LightSourceAnglePropertyName;

	// Material purpose tokens that we convert from USD, if available
	extern UNREALUSDWRAPPER_API FString MaterialAllPurpose;
	extern UNREALUSDWRAPPER_API FString MaterialAllPurposeText; // Text to show on UI for "allPurpose", as its value is actually the empty string
	extern UNREALUSDWRAPPER_API FString MaterialPreviewPurpose;
	extern UNREALUSDWRAPPER_API FString MaterialFullPurpose;
}

struct UNREALUSDWRAPPER_API FUsdDelegates
{
	DECLARE_MULTICAST_DELEGATE_OneParam( FUsdImportDelegate, FString /* FilePath */);
	static FUsdImportDelegate OnPreUsdImport;
	static FUsdImportDelegate OnPostUsdImport;
};