// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealUSDWrapper.h"

#include "USDClassesModule.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDProjectSettings.h"

#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/SdfLayer.h"

#include "CineCameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Regex.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "UnrealUSDWrapper"

#if USE_USD_SDK

#include "USDIncludesStart.h"

#include "pxr/base/gf/rotation.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/tf/diagnosticMgr.h"
#include "pxr/base/tf/errorMark.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/setenv.h"
#include "pxr/usd/ar/defaultResolver.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/usd/kind/registry.h"
#include "pxr/usd/sdf/fileFormat.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/common.h"
#include "pxr/usd/usd/debugCodes.h"
#include "pxr/usd/usd/modelAPI.h"
#include "pxr/usd/usd/references.h"
#include "pxr/usd/usd/schemaBase.h"
#include "pxr/usd/usd/relationship.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/stageCacheContext.h"
#include "pxr/usd/usd/usdFileFormat.h"
#include "pxr/usd/usd/usdaFileFormat.h"
#include "pxr/usd/usd/usdcFileFormat.h"
#include "pxr/usd/usd/usdzFileFormat.h"
#include "pxr/usd/usd/variantSets.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/modelAPI.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xformCommonAPI.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "pxr/usd/usdShade/tokens.h"
#include "pxr/usd/usdUtils/stageCache.h"

#include "USDIncludesEnd.h"


using std::vector;
using std::string;

using namespace pxr;

namespace UnrealIdentifiers
{
	static const TfToken AssetPath("unrealAssetPath");

	static const TfToken ActorClass("unrealActorClass");

	static const TfToken PropertyPath("unrealPropertyPath");

	/**
	 * Identifies the LOD variant set on a primitive which means this primitive has child prims that LOD meshes
	 * named LOD0, LOD1, LOD2, etc
	 */
	const TfToken LOD("LOD");

	const TfToken MaterialAssignment = TfToken("unrealMaterial");
	const TfToken Unreal = TfToken("unreal");

	const TfToken UnrealNaniteOverride = TfToken("unrealNanite");
	const TfToken UnrealNaniteOverrideEnable = TfToken("enable");
	const TfToken UnrealNaniteOverrideDisable = TfToken("disable");

	const TfToken LiveLinkAPI = TfToken("LiveLinkAPI");
	const TfToken ControlRigAPI = TfToken("ControlRigAPI");
	const TfToken UnrealAnimBlueprintPath = TfToken("unreal:liveLink:animBlueprintPath");
	const TfToken UnrealLiveLinkSubjectName = TfToken("unreal:liveLink:subjectName");
	const TfToken UnrealLiveLinkEnabled = TfToken("unreal:liveLink:enabled");
	const TfToken UnrealControlRigPath = TfToken("unreal:controlRig:controlRigPath");
	const TfToken UnrealUseFKControlRig = TfToken("unreal:controlRig:useFKControlRig");
	const TfToken UnrealControlRigReduceKeys = TfToken("unreal:controlRig:reduceKeys");
	const TfToken UnrealControlRigReductionTolerance = TfToken("unreal:controlRig:reductionTolerance");
	const TfToken DiffuseColor = TfToken("diffuseColor");
	const TfToken EmissiveColor = TfToken("emissiveColor");
	const TfToken Metallic = TfToken("metallic");
	const TfToken Roughness = TfToken("roughness");
	const TfToken Opacity = TfToken("opacity");
	const TfToken Normal = TfToken("normal");
	const TfToken Specular = TfToken("specular");
	const TfToken Anisotropy = TfToken("anisotropy");
	const TfToken Tangent = TfToken("tangent");
	const TfToken SubsurfaceColor = TfToken("subsurfaceColor");
	const TfToken Occlusion = TfToken("occlusion");
	const TfToken Refraction = TfToken("ior");

	const TfToken Surface = TfToken("surface");
	const TfToken St = TfToken("st");
	const TfToken Varname = TfToken("varname");
	const TfToken Result = TfToken("result");
	const TfToken File = TfToken("file");
	const TfToken WrapT = TfToken( "wrapT" );
	const TfToken WrapS = TfToken( "wrapS" );
	const TfToken Repeat = TfToken( "repeat" );
	const TfToken Mirror = TfToken( "mirror" );
	const TfToken Clamp = TfToken( "clamp" );
	const TfToken Fallback = TfToken("fallback");
	const TfToken R = TfToken("r");
	const TfToken RGB = TfToken("rgb");

	const TfToken UsdPreviewSurface = TfToken( "UsdPreviewSurface" );
	const TfToken UsdPrimvarReader_float2 = TfToken( "UsdPrimvarReader_float2" );
	const TfToken UsdPrimvarReader_float3 = TfToken( "UsdPrimvarReader_float3" );
	const TfToken UsdUVTexture = TfToken( "UsdUVTexture" );

	const TfToken WorldSpaceNormals = TfToken( "worldSpaceNormals" );

	const TfToken GroomAPI = TfToken( "GroomAPI" );
	const TfToken GroomBindingAPI = TfToken( "GroomBindingAPI" );
	const TfToken UnrealGroomToBind = TfToken( "unreal:groomBinding:groom" );
	const TfToken UnrealGroomReferenceMesh = TfToken( "unreal:groomBinding:referenceMesh" );

	const TfToken UnrealContentPath = TfToken( "unreal:contentPath" );
	const TfToken UnrealAssetType = TfToken( "unreal:assetType" );
	const TfToken UnrealExportTime = TfToken( "unreal:exportTime" );
	const TfToken UnrealEngineVersion = TfToken( "unreal:engineVersion" );
}

std::string FUsdAttribute::GetUnrealPropertyPath( const pxr::UsdAttribute& Attribute )
{
	std::string UnrealPropertyPath;

	VtValue CustomData = Attribute.GetCustomDataByKey(UnrealIdentifiers::PropertyPath);

	if (CustomData.IsHolding<std::string>())
	{
		UnrealPropertyPath = CustomData.Get<std::string>();
	}

	return UnrealPropertyPath;
}

template<typename T>
bool GetValue(T& OutVal, const pxr::UsdAttribute& Attrib, int ArrayIndex, double Time)
{
	bool bResult = false;

	if (ArrayIndex != -1)
	{
		// Note: VtArray is copy on write so this is cheap
		VtArray<T> Array;
		if (Attrib.Get(&Array, Time))
		{
			OutVal = Array[ArrayIndex];
			bResult = true;
		}
	}
	else
	{
		bResult = Attrib.Get(&OutVal, Time);
	}

	return bResult;
}

template<typename T>
bool IsHolding(const VtValue& Value)
{
	return Value.IsHolding<T>() || Value.IsHolding<VtArray<T>>();
}

bool FUsdAttribute::AsInt(int64_t& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	// We test multiple types of ints here. int64 is always returned as it can hold all other types
	// Unreal expects this
	VtValue Value;
	bool bResult = Attribute.Get(&Value, Time);
	if (IsHolding<int8_t>(Value))
	{
		uint8_t Val = 0;
		bResult = GetValue(Val, Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<int32_t>(Value))
	{
		int32_t Val = 0;
		bResult = GetValue(Val, Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<int64_t>(Value))
	{
		int64_t Val = 0;
		bResult = GetValue(Val, Attribute, ArrayIndex, Time);
		OutVal = Val;
	}

	return bResult;
}

bool FUsdAttribute::AsUnsignedInt(uint64_t& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	// We test multiple types of ints here. uint64 is always returned as it can hold all other types
	// Unreal expects this
	VtValue Value;
	bool bResult = Attribute.Get(&Value, Time);
	if (IsHolding<uint8_t>(Value))
	{
		uint8_t Val;
		bResult = GetValue(Val, Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<uint32_t>(Value))
	{
		uint32_t Val;
		bResult = GetValue(Val, Attribute, ArrayIndex, Time);
		OutVal = Val;
	}
	else if (IsHolding<uint64_t>(Value))
	{
		uint64_t Val;
		bResult = GetValue(Val, Attribute, ArrayIndex, Time);
		OutVal = Val;
	}

	return bResult;
}

bool FUsdAttribute::AsDouble(double& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	bool bResult = false;

	bResult = GetValue<double>(OutVal, Attribute, ArrayIndex, Time);

	if (!bResult)
	{
		float Val = 0.0f;
		bResult = GetValue<float>(Val, Attribute, ArrayIndex, Time);
		OutVal = Val;
	}

	return bResult;
}

bool FUsdAttribute::AsString(const char*& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	// this method is very hacky to return temp strings
	// designed to have the string copied immediately
	bool bResult = false;

	VtValue Value;
	Attribute.Get(&Value);
	// mem leak
	static std::string Temp;
	if (IsHolding<std::string>(Value))
	{
		bResult = GetValue(Temp, Attribute, ArrayIndex, Time);

		OutVal = Temp.c_str();
	}
	else if (IsHolding<TfToken>(Value))
	{
		TfToken Token;
		bResult = GetValue(Token, Attribute, ArrayIndex, Time);

		Temp = Token.GetString();

		OutVal = Temp.c_str();
	}

	return bResult;
}

bool FUsdAttribute::AsBool(bool& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	return GetValue(OutVal, Attribute, ArrayIndex, Time);
}

bool FUsdAttribute::AsVector2(FUsdVector2Data& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	GfVec2f Value;
	const bool bResult = GetValue(Value, Attribute, ArrayIndex, Time);

	OutVal.X = Value[0];
	OutVal.Y = Value[1];

	return bResult;
}

bool FUsdAttribute::AsVector3(FUsdVectorData& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	GfVec3f Value;
	const bool bResult = GetValue(Value, Attribute, ArrayIndex, Time);

	OutVal.X = Value[0];
	OutVal.Y = Value[1];
	OutVal.Z = Value[2];

	return bResult;
}

bool FUsdAttribute::AsVector4(FUsdVector4Data& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	GfVec4f Value;
	const bool bResult = GetValue(Value, Attribute, ArrayIndex, Time);

	OutVal.X = Value[0];
	OutVal.Y = Value[1];
	OutVal.Z = Value[2];
	OutVal.W = Value[3];

	return bResult;
}

bool FUsdAttribute::AsColor(FUsdVector4Data& OutVal, const pxr::UsdAttribute& Attribute, int ArrayIndex, double Time)
{
	GfVec4f Value;
	bool bResult = GetValue(Value, Attribute, ArrayIndex, Time);

	if (bResult)
	{
		OutVal.X = Value[0];
		OutVal.Y = Value[1];
		OutVal.Z = Value[2];
		OutVal.W = Value[3];
	}
	else
	{
		// Try color 3 with a = 1;
		GfVec3f Value3;
		bResult = GetValue<GfVec3f>(Value3, Attribute, ArrayIndex, Time);
		OutVal.X = Value3[0];
		OutVal.Y = Value3[1];
		OutVal.Z = Value3[2];
		OutVal.W = 1;
	}

	return bResult;
}

bool FUsdAttribute::IsUnsigned(const pxr::UsdAttribute& Attribute)
{
	VtValue Value;
	Attribute.Get(&Value);

	return IsHolding<uint8_t>(Value)
		|| IsHolding<uint32_t>(Value)
		|| IsHolding<uint64_t>(Value);
}

int FUsdAttribute::GetArraySize( const pxr::UsdAttribute& Attribute )
{
	VtValue Value;
	Attribute.Get(&Value);

	return Value.IsArrayValued() ? (int)Value.GetArraySize() : -1;

}

bool IUsdPrim::IsValidPrimName(const FString& Name, FText& OutReason)
{
	if (Name.IsEmpty())
	{
		OutReason = LOCTEXT("EmptyStringInvalid", "Empty string is not a valid name!");
		return false;
	}

	const FString InvalidCharacters = TEXT("\\W");
	FRegexPattern RegexPattern( InvalidCharacters );
	FRegexMatcher RegexMatcher( RegexPattern, Name );
	if (RegexMatcher.FindNext())
	{
		OutReason = LOCTEXT("InvalidCharacter", "Can only use letters, numbers and underscore!");
		return false;
	}

	if (Name.Left(1).IsNumeric())
	{
		OutReason = LOCTEXT("InvalidFirstCharacter", "First character cannot be a number!");
		return false;
	}

	return true;
}

EUsdPurpose IUsdPrim::GetPurpose( const UsdPrim& Prim, bool bComputed )
{
	UsdGeomImageable Geom(Prim);
	if (Geom)
	{
		// Use compute purpose because it depends on the hierarchy:
		// "If the purpose of </RootPrim> is set to "render", then the effective purpose
		// of </RootPrim/ChildPrim> will be "render" even if that prim has a different
		// authored value for purpose."
		TfToken Purpose;
		if (bComputed)
		{
			Purpose = Geom.ComputePurpose();
		}
		else
		{
			pxr::UsdAttribute PurposeAttr = Prim.GetAttribute(pxr::UsdGeomTokens->purpose);

			pxr::VtValue Value;
			PurposeAttr.Get(&Value);

			Purpose = Value.Get<pxr::TfToken>();
		}

		if (Purpose == pxr::UsdGeomTokens->proxy)
		{
			return EUsdPurpose::Proxy;
		}
		else if (Purpose == pxr::UsdGeomTokens->render)
		{
			return EUsdPurpose::Render;
		}
		else if (Purpose == pxr::UsdGeomTokens->guide)
		{
			return EUsdPurpose::Guide;
		}
	}

	return EUsdPurpose::Default;
}

bool IUsdPrim::HasGeometryData(const UsdPrim& Prim)
{
	return UsdGeomMesh(Prim) ? true : false;
}

bool IUsdPrim::HasGeometryDataOrLODVariants(const UsdPrim& Prim)
{
	return HasGeometryData(Prim) || GetNumLODs(Prim) > 0;
}

int IUsdPrim::GetNumLODs(const UsdPrim& Prim)
{
	FScopedUsdAllocs UsdAllocs;

	// 0 indicates no variant or no lods in variant.
	int NumLODs = 0;
	if (Prim.HasVariantSets())
	{
		UsdVariantSet LODVariantSet = Prim.GetVariantSet(UnrealIdentifiers::LOD);
		if(LODVariantSet.IsValid())
		{
			vector<string> VariantNames = LODVariantSet.GetVariantNames();
			NumLODs = VariantNames.size();
		}
	}

	return NumLODs;
}

bool IUsdPrim::IsKindChildOf(const UsdPrim& Prim, const std::string& InBaseKind)
{
	TfToken BaseKind(InBaseKind);

	KindRegistry& Registry = KindRegistry::GetInstance();

	TfToken PrimKind( GetKind(Prim) );

	return Registry.IsA(PrimKind, BaseKind);

}

TfToken IUsdPrim::GetKind(const pxr::UsdPrim& Prim)
{
	TfToken KindType;

	UsdModelAPI Model(Prim);
	if (Model)
	{
		Model.GetKind(&KindType);
	}
	else
	{
		// Prim is not a model, read kind directly from metadata
		Prim.GetMetadata( SdfFieldKeys->Kind, &KindType );
	}

	return KindType;
}

bool IUsdPrim::SetKind(const pxr::UsdPrim& Prim, const pxr::TfToken& Kind)
{
	UsdModelAPI Model(Prim);
	if (Model)
	{
		if (!Model.SetKind(Kind))
		{
			return Prim.SetMetadata( SdfFieldKeys->Kind, Kind );
		}

		return true;
	}

	return false;
}

bool IUsdPrim::ClearKind( const pxr::UsdPrim& Prim )
{
	return Prim.ClearMetadata( SdfFieldKeys->Kind );
}

pxr::GfMatrix4d IUsdPrim::GetLocalTransform(const pxr::UsdPrim& Prim)
{
	pxr::GfMatrix4d USDMatrix(1);

	pxr::UsdGeomXformable XForm(Prim);
	if(XForm)
	{
		// Set transform
		bool bResetXFormStack = false;
		XForm.GetLocalTransformation(&USDMatrix, &bResetXFormStack);
	}

	return USDMatrix;
}

pxr::GfMatrix4d IUsdPrim::GetLocalToWorldTransform(const pxr::UsdPrim& Prim )
{
	return GetLocalToWorldTransform( Prim, pxr::UsdTimeCode::Default().GetValue() );
}

pxr::GfMatrix4d IUsdPrim::GetLocalToWorldTransform(const pxr::UsdPrim& Prim, double Time)
{
	pxr::SdfPath AbsoluteRootPath = pxr::SdfPath::AbsoluteRootPath();
	return GetLocalToWorldTransform(Prim, Time, AbsoluteRootPath);

}

pxr::GfMatrix4d IUsdPrim::GetLocalToWorldTransform(const pxr::UsdPrim& Prim, double Time, const pxr::SdfPath& AbsoluteRootPath)
{
	pxr::SdfPath PrimPath = Prim.GetPath();
	if (!Prim || PrimPath == AbsoluteRootPath)
	{
		return pxr::GfMatrix4d(1);
	}

	pxr::GfMatrix4d AccumulatedTransform(1.);
	bool bResetsXFormStack = false;
	pxr::UsdGeomXformable XFormable(Prim);
	// silently ignoring errors
	XFormable.GetLocalTransformation(&AccumulatedTransform, &bResetsXFormStack, Time);

	if (!bResetsXFormStack)
	{
		AccumulatedTransform = AccumulatedTransform * GetLocalToWorldTransform(Prim.GetParent(), Time, AbsoluteRootPath);
	}

	return AccumulatedTransform;
}

std::string IUsdPrim::GetUnrealPropertyPath(const pxr::UsdPrim& Prim)
{
	VtValue CustomData = Prim.GetCustomDataByKey(UnrealIdentifiers::PropertyPath);
	if (CustomData.IsHolding<std::string>())
	{
		return CustomData.Get<std::string>();
	}

	return {};
}

TArray< UE::FUsdAttribute > PrivateGetAttributes(const pxr::UsdPrim& Prim, const TfToken& ByMetadata)
{
	FScopedUsdAllocs UsdAllocs;

	std::vector<UsdAttribute> Attributes = Prim.GetAttributes();

	TArray<UE::FUsdAttribute> OutAttributes;
	OutAttributes.Reserve(Attributes.size());

	for (UsdAttribute& Attr : Attributes)
	{
		if (ByMetadata.IsEmpty() || Attr.HasCustomDataKey(ByMetadata))
		{
			OutAttributes.Emplace( Attr);
		}
	}

	return OutAttributes;
}

TArray< UE::FUsdAttribute > IUsdPrim::GetUnrealPropertyAttributes(const pxr::UsdPrim& Prim)
{
	return PrivateGetAttributes(Prim, UnrealIdentifiers::PropertyPath);
}

std::string IUsdPrim::GetUnrealAssetPath(const pxr::UsdPrim& Prim)
{
	std::string UnrealAssetPath;

	UsdAttribute UnrealAssetPathAttr = Prim.GetAttribute(UnrealIdentifiers::AssetPath);
	if (UnrealAssetPathAttr.HasValue())
	{
		UnrealAssetPathAttr.Get(&UnrealAssetPath);
	}

	return UnrealAssetPath;
}

std::string IUsdPrim::GetUnrealActorClass(const pxr::UsdPrim& Prim)
{
	std::string UnrealActorClass;

	UsdAttribute UnrealActorClassAttr = Prim.GetAttribute(UnrealIdentifiers::ActorClass);
	if (UnrealActorClassAttr.HasValue())
	{
		UnrealActorClassAttr.Get(&UnrealActorClass);
	}

	return UnrealActorClass;
}

namespace Internal
{
	TArray< FString > FillMaterialInfo(const SdfPath& Path, UsdStageWeakPtr Stage)
	{
		TArray< FString > MaterialNames;

		// load each material at the material path;
		UsdPrim MaterialPrim = Stage->Load(Path);

		if(MaterialPrim)
		{
			// Default to using the prim path name as the path for this material in Unreal
			FString MaterialName = ANSI_TO_TCHAR( MaterialPrim.GetName().GetString().c_str() ) ;

			std::string UsdMaterialName;

			// See if the material has an "unrealAssetPath" attribute.  This should be the full name of the material
			static const TfToken AssetPathToken = TfToken(UnrealIdentifiers::AssetPath);
			UsdAttribute UnrealAssetPathAttr = MaterialPrim.GetAttribute(AssetPathToken);
			if (UnrealAssetPathAttr && UnrealAssetPathAttr.HasValue())
			{
				UnrealAssetPathAttr.Get(&UsdMaterialName);


			}

			MaterialNames.Add( MoveTemp( MaterialName ) );
		}

		return MaterialNames;
	}
}

std::string DiscoverInformationAboutUsdMaterial(const UsdShadeMaterial& ShadeMaterial, const UsdGeomGprim& boundPrim)
{
	std::string ShadingEngineName = (ShadeMaterial ? ShadeMaterial.GetPrim() : boundPrim.GetPrim()).GetPrimPath().GetString();
	return ShadingEngineName;
}

bool IUsdPrim::IsUnrealProperty(const pxr::UsdPrim& Prim)
{
	return Prim.HasCustomDataKey(UnrealIdentifiers::PropertyPath);
}

bool IUsdPrim::HasTransform(const pxr::UsdPrim& Prim)
{
	return UsdGeomXformable(Prim) ? true : false;
}

bool IUsdPrim::SetActiveLODIndex(const pxr::UsdPrim& Prim, int LODIndex)
{
	FScopedUsdAllocs UsdAllocs;

	if (Prim.HasVariantSets())
	{
		UsdVariantSet LODVariantSet = Prim.GetVariantSet(UnrealIdentifiers::LOD);
		if (LODVariantSet.IsValid())
		{
			vector<string> VariantNames = LODVariantSet.GetVariantNames();

			bool bResult = false;
			if(LODIndex < VariantNames.size())
			{
				bResult = LODVariantSet.SetVariantSelection(VariantNames[LODIndex]);
			}
		}
	}

	return false;
}

EUsdGeomOrientation IUsdPrim::GetGeometryOrientation(const pxr::UsdGeomMesh& Mesh)
{
	return GetGeometryOrientation( Mesh, pxr::UsdTimeCode::Default().GetValue() );
}

EUsdGeomOrientation IUsdPrim::GetGeometryOrientation(const pxr::UsdGeomMesh& Mesh, double Time)
{
	EUsdGeomOrientation GeomOrientation = EUsdGeomOrientation::RightHanded;

	if (Mesh)
	{
		UsdAttribute Orientation = Mesh.GetOrientationAttr();
		if(Orientation)
		{
			static TfToken RightHanded("rightHanded");
			static TfToken LeftHanded("leftHanded");

			TfToken OrientationValue;
			Orientation.Get(&OrientationValue, Time);

			GeomOrientation = OrientationValue == LeftHanded ? EUsdGeomOrientation::LeftHanded : EUsdGeomOrientation::RightHanded;
		}
	}

	return GeomOrientation;
}
#endif // USE_USD_SDK

const TCHAR* UnrealIdentifiers::LayerSavedComment = TEXT("unreal:layerSaved");

const TCHAR* UnrealIdentifiers::Invisible = TEXT("invisible");
const TCHAR* UnrealIdentifiers::Inherited = TEXT("inherited");
const TCHAR* UnrealIdentifiers::IdentifierPrefix = TEXT("@identifier:");

FName UnrealIdentifiers::TransformPropertyName = TEXT( "Transform" ); // Fake FName because the transform is stored decomposed on the component
FName UnrealIdentifiers::HiddenInGamePropertyName = GET_MEMBER_NAME_CHECKED( USceneComponent, bHiddenInGame );
FName UnrealIdentifiers::HiddenPropertyName = AActor::GetHiddenPropertyName();

FName UnrealIdentifiers::CurrentFocalLengthPropertyName = GET_MEMBER_NAME_CHECKED( UCineCameraComponent, CurrentFocalLength );
FName UnrealIdentifiers::ManualFocusDistancePropertyName = GET_MEMBER_NAME_CHECKED( UCineCameraComponent, FocusSettings.ManualFocusDistance );
FName UnrealIdentifiers::CurrentAperturePropertyName = GET_MEMBER_NAME_CHECKED( UCineCameraComponent, CurrentAperture );
FName UnrealIdentifiers::SensorWidthPropertyName = GET_MEMBER_NAME_CHECKED( UCineCameraComponent, Filmback.SensorWidth );
FName UnrealIdentifiers::SensorHeightPropertyName = GET_MEMBER_NAME_CHECKED( UCineCameraComponent, Filmback.SensorHeight );

FName UnrealIdentifiers::IntensityPropertyName = GET_MEMBER_NAME_CHECKED( ULightComponentBase, Intensity );
FName UnrealIdentifiers::LightColorPropertyName = GET_MEMBER_NAME_CHECKED( ULightComponentBase, LightColor );
FName UnrealIdentifiers::UseTemperaturePropertyName = GET_MEMBER_NAME_CHECKED( ULightComponent, bUseTemperature );
FName UnrealIdentifiers::TemperaturePropertyName = GET_MEMBER_NAME_CHECKED( ULightComponent, Temperature );
FName UnrealIdentifiers::SourceWidthPropertyName = GET_MEMBER_NAME_CHECKED( URectLightComponent, SourceWidth );
FName UnrealIdentifiers::SourceHeightPropertyName = GET_MEMBER_NAME_CHECKED( URectLightComponent, SourceHeight );
FName UnrealIdentifiers::SourceRadiusPropertyName = GET_MEMBER_NAME_CHECKED( UPointLightComponent, SourceRadius );
FName UnrealIdentifiers::OuterConeAnglePropertyName = GET_MEMBER_NAME_CHECKED( USpotLightComponent, OuterConeAngle );
FName UnrealIdentifiers::InnerConeAnglePropertyName = GET_MEMBER_NAME_CHECKED( USpotLightComponent, InnerConeAngle );
FName UnrealIdentifiers::LightSourceAnglePropertyName = GET_MEMBER_NAME_CHECKED( UDirectionalLightComponent, LightSourceAngle );

FString UnrealIdentifiers::MaterialAllPurposeText = TEXT( "allPurpose" );
#if USE_USD_SDK
FString UnrealIdentifiers::MaterialAllPurpose = ANSI_TO_TCHAR( pxr::UsdShadeTokens->allPurpose.GetString().c_str() );
FString UnrealIdentifiers::MaterialPreviewPurpose = ANSI_TO_TCHAR( pxr::UsdShadeTokens->preview.GetString().c_str() );
FString UnrealIdentifiers::MaterialFullPurpose = ANSI_TO_TCHAR( pxr::UsdShadeTokens->full.GetString().c_str() );
#else
FString UnrealIdentifiers::MaterialAllPurpose = TEXT( "" );
FString UnrealIdentifiers::MaterialPreviewPurpose = TEXT( "preview" );
FString UnrealIdentifiers::MaterialFullPurpose = TEXT( "full" );
#endif // USE_USD_SDK

FUsdDelegates::FUsdImportDelegate FUsdDelegates::OnPreUsdImport;
FUsdDelegates::FUsdImportDelegate FUsdDelegates::OnPostUsdImport;

namespace UsdWrapperUtils
{
	void CheckIfForceDisabled()
	{
#if USD_FORCE_DISABLED
		UE_LOG( LogUsd, Error, TEXT( "The USD SDK is disabled because the executable is not forcing the ansi C allocator (you need to set 'FORCE_ANSI_ALLOCATOR=1' as a global definition on your project *.Target.cs file). Read the comments at the end of UnrealUSDWrapper.Build.cs for more details." ) );
#endif // USD_FORCE_DISABLED
	}
}

#if USE_USD_SDK
class FUsdDiagnosticDelegate : public pxr::TfDiagnosticMgr::Delegate
{
public:
	virtual ~FUsdDiagnosticDelegate() override {};
	virtual void IssueError(const pxr::TfError& Error) override
	{
		FScopedUsdAllocs Allocs;

		std::string Msg = Error.GetErrorCodeAsString();
		Msg += ": ";
		Msg += Error.GetCommentary();

		UE_LOG(LogUsd, Error, TEXT("%s"), ANSI_TO_TCHAR( Msg.c_str() ));
	}
	virtual void IssueFatalError(const pxr::TfCallContext& Context, const std::string& Msg) override
	{
		UE_LOG(LogUsd, Error, TEXT("%s"), ANSI_TO_TCHAR( Msg.c_str() ));
	}
	virtual void IssueStatus(const pxr::TfStatus& Status) override
	{
		FScopedUsdAllocs Allocs;

		std::string Msg = Status.GetDiagnosticCodeAsString();
		Msg += ": ";
		Msg += Status.GetCommentary();

		UE_LOG(LogUsd, Log, TEXT("%s"), ANSI_TO_TCHAR( Msg.c_str() ));
	}
	virtual void IssueWarning(const pxr::TfWarning& Warning) override
	{
		FScopedUsdAllocs Allocs;

		std::string Msg = Warning.GetDiagnosticCodeAsString();
		Msg += ": ";
		Msg += Warning.GetCommentary();

		UE_LOG(LogUsd, Warning, TEXT("%s"), ANSI_TO_TCHAR( Msg.c_str() ));
	}
};
#else
class FUsdDiagnosticDelegate { };
#endif // USE_USD_SDK

TUniquePtr<FUsdDiagnosticDelegate> UnrealUSDWrapper::Delegate = nullptr;

#if USE_USD_SDK

double UnrealUSDWrapper::GetDefaultTimeCode()
{
	return UsdTimeCode::Default().GetValue();
}
#endif // USE_USD_SDK

TArray<FString> UnrealUSDWrapper::GetAllSupportedFileFormats()
{
	TArray<FString> Result;

#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	std::set<std::string> Extensions = pxr::SdfFileFormat::FindAllFileFormatExtensions();
	for ( const std::string& Ext : Extensions )
	{
		// Ignore formats that don't target "usd"
		pxr::SdfFileFormatConstPtr Format = pxr::SdfFileFormat::FindByExtension(Ext, pxr::UsdUsdFileFormatTokens->Target);
		if ( Format == nullptr )
		{
			continue;
		}

		Result.Emplace( ANSI_TO_TCHAR( Ext.c_str() ) );
	}
#endif // #if USE_USD_SDK

	return Result;
}

TArray<FString> UnrealUSDWrapper::GetNativeFileFormats()
{
	TArray<FString> Result;

#if USE_USD_SDK
	FScopedUsdAllocs Allocs;

	const pxr::TfTokenVector NativeFormatIds{
		pxr::UsdUsdFileFormatTokens->Id,
		pxr::UsdUsdaFileFormatTokens->Id,
		pxr::UsdUsdcFileFormatTokens->Id,
		pxr::UsdUsdzFileFormatTokens->Id
	};

	std::set<std::string> FileExtensions;

	for ( const pxr::TfToken& FormatId : NativeFormatIds )
	{
		const pxr::SdfFileFormatConstPtr Format = pxr::SdfFileFormat::FindById( FormatId );
		if ( !Format )
		{
			continue;
		}

		for ( const std::string& FileExtension : Format->GetFileExtensions() )
		{
			FileExtensions.insert( FileExtension );
		}
	}

	for ( const std::string& FileExtension : FileExtensions )
	{
		Result.Emplace( ANSI_TO_TCHAR( FileExtension.c_str() ) );
	}
#endif // #if USE_USD_SDK

	return Result;
}

namespace UE::UnrealUSDWrapper::Private
{
	UE::FUsdStage OpenStageImpl(
		const TCHAR* RootIdentifier,
		const TCHAR* SessionIdentifier,
		EUsdInitialLoadSet InitialLoadSet,
		bool bUseStageCache,
		const TArray<FString>* PopulationMask,
		bool bForceReloadLayersFromDisk
	)
	{
		if ( !RootIdentifier || FCString::Strlen( RootIdentifier ) == 0 )
		{
			return UE::FUsdStage();
		}

#if USE_USD_SDK
		FScopedUsdAllocs UsdAllocs;

		pxr::SdfLayerHandleSet LoadedLayers = pxr::SdfLayer::GetLoadedLayers();
		pxr::UsdStageRefPtr Stage;

		TOptional<pxr::UsdStageCacheContext> StageCacheContext;
		if ( bUseStageCache )
		{
			StageCacheContext.Emplace( pxr::UsdUtilsStageCache::Get() );
		}

		pxr::UsdStagePopulationMask Mask;
		if( PopulationMask )
		{
			// The USD OpenMasked functions don't actually consult or populate the stage cache
			ensure( bUseStageCache == false );

			for ( const FString& AllowedPrimPath : *PopulationMask )
			{
				Mask.Add( pxr::SdfPath{ TCHAR_TO_ANSI( *AllowedPrimPath ) } );
			}
		}

		static_assert( ( int ) pxr::UsdStage::InitialLoadSet::LoadAll == ( int ) EUsdInitialLoadSet::LoadAll );
		static_assert( ( int ) pxr::UsdStage::InitialLoadSet::LoadNone == ( int ) EUsdInitialLoadSet::LoadNone );
		pxr::UsdStage::InitialLoadSet LoadSet = static_cast< pxr::UsdStage::InitialLoadSet >( InitialLoadSet );

		FString IdentifierStr = FString( RootIdentifier );
		if ( FPaths::FileExists( IdentifierStr ) )
		{
			if( PopulationMask )
			{
				Stage = pxr::UsdStage::OpenMasked( TCHAR_TO_ANSI( *IdentifierStr ), Mask, LoadSet );
			}
			else
			{
				Stage = pxr::UsdStage::Open( TCHAR_TO_ANSI( *IdentifierStr ), LoadSet );
			}
		}
		else
		{
			FString SessionIdentifierStr = FString{ SessionIdentifier };

			IdentifierStr.RemoveFromStart( UnrealIdentifiers::IdentifierPrefix );
			SessionIdentifierStr.RemoveFromStart( UnrealIdentifiers::IdentifierPrefix );

			pxr::SdfLayerRefPtr RootLayer = pxr::SdfLayer::Find( TCHAR_TO_ANSI( *IdentifierStr ) );
			pxr::SdfLayerRefPtr SessionLayer = pxr::SdfLayer::Find( TCHAR_TO_ANSI( *SessionIdentifierStr ) );
			if ( RootLayer )
			{
				if ( PopulationMask )
				{
					// We use this additional check so we don't have to replicate USD's "_CreateAnonymousSessionLayer"
					// Basically we can't pass an invalid session layer pointer here the stage will actually end up
					// with no session layer at all
					if ( SessionLayer )
					{
						Stage = pxr::UsdStage::OpenMasked( RootLayer, SessionLayer, Mask, LoadSet );
					}
					else
					{
						Stage = pxr::UsdStage::OpenMasked( RootLayer, Mask, LoadSet );
					}
				}
				else
				{
					if( SessionLayer )
					{
						Stage = pxr::UsdStage::Open( RootLayer, SessionLayer, LoadSet );
					}
					else
					{
						Stage = pxr::UsdStage::Open( RootLayer, LoadSet );
					}
				}
			}
		}

		if ( bForceReloadLayersFromDisk && Stage )
		{
			// Layers are cached in the layer registry independently of the stage cache. If the layer is already in
			// the registry by the time we try to open a stage, even if we're not using a stage cache at all the
			// layer will be reused and the file will *not* be re-read, so here we manually reload them.
			pxr::SdfLayerHandleVector StageLayers = Stage->GetLayerStack();
			for ( pxr::SdfLayerHandle StageLayer : StageLayers )
			{
				if ( LoadedLayers.count( StageLayer ) > 0 )
				{
					StageLayer->Reload();
				}
			}
		}

		return UE::FUsdStage( Stage );
#else
		UsdWrapperUtils::CheckIfForceDisabled();
		return UE::FUsdStage();
#endif // #if USE_USD_SDK
	}
}

UE::FUsdStage UnrealUSDWrapper::OpenStage(
	const TCHAR* Identifier,
	EUsdInitialLoadSet InitialLoadSet,
	bool bUseStageCache,
	bool bForceReloadLayersFromDisk
)
{
	const TArray<FString>* PopulationMask = nullptr;
	const TCHAR* SessionIdentifier = nullptr;
	return UE::UnrealUSDWrapper::Private::OpenStageImpl(
		Identifier,
		SessionIdentifier,
		InitialLoadSet,
		bUseStageCache,
		nullptr,
		bForceReloadLayersFromDisk
	);
}

 UE::FUsdStage UnrealUSDWrapper::OpenStage(
	 UE::FSdfLayer RootLayer,
	 UE::FSdfLayer SessionLayer,
	 EUsdInitialLoadSet InitialLoadSet,
	 bool bUseStageCache,
	 bool bForceReloadLayersFromDisk
 )
{
	 if( !RootLayer )
	 {
		 return {};
	 }

	 const TArray<FString>* PopulationMask = nullptr;
	 const TCHAR* SessionIdentifier = nullptr;
	 return UE::UnrealUSDWrapper::Private::OpenStageImpl(
		 *RootLayer.GetIdentifier(),
		 SessionLayer ? *SessionLayer.GetIdentifier() : nullptr,
		 InitialLoadSet,
		 bUseStageCache,
		 nullptr,
		 bForceReloadLayersFromDisk
	 );
}

UE::FUsdStage UnrealUSDWrapper::OpenMaskedStage(
	const TCHAR* Identifier,
	EUsdInitialLoadSet InitialLoadSet,
	const TArray<FString>& PopulationMask,
	bool bForceReloadLayersFromDisk
)
{
	const bool bUseStageCache = false;
	const TCHAR* SessionIdentifier = nullptr;
	return UE::UnrealUSDWrapper::Private::OpenStageImpl(
		Identifier,
		SessionIdentifier,
		InitialLoadSet,
		bUseStageCache,
		&PopulationMask,
		bForceReloadLayersFromDisk
	);
}

UE::FUsdStage UnrealUSDWrapper::OpenMaskedStage(
	UE::FSdfLayer RootLayer,
	UE::FSdfLayer SessionLayer,
	EUsdInitialLoadSet InitialLoadSet,
	const TArray<FString>& PopulationMask,
	bool bForceReloadLayersFromDisk
)
{
	if ( !RootLayer )
	{
		return {};
	}

	const bool bUseStageCache = false;
	const TCHAR* SessionIdentifier = nullptr;
	return UE::UnrealUSDWrapper::Private::OpenStageImpl(
		*RootLayer.GetIdentifier(),
		SessionLayer ? *SessionLayer.GetIdentifier() : nullptr,
		InitialLoadSet,
		bUseStageCache,
		&PopulationMask,
		bForceReloadLayersFromDisk
	);
}

UE::FUsdStage UnrealUSDWrapper::NewStage( const TCHAR* FilePath )
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	UE::FUsdStage UsdStage( pxr::UsdStage::CreateNew( TCHAR_TO_ANSI( FilePath ) ) );
	if ( UsdStage )
	{
		pxr::UsdGeomSetStageUpAxis( UsdStage, pxr::UsdGeomTokens->z );
	}

	return UsdStage;
#else
	UsdWrapperUtils::CheckIfForceDisabled();
	return UE::FUsdStage();
#endif // #if USE_USD_SDK
}

UE::FUsdStage UnrealUSDWrapper::NewStage()
{
#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	UE::FUsdStage UsdStage( pxr::UsdStage::CreateInMemory() );
	if ( UsdStage )
	{
		pxr::UsdGeomSetStageUpAxis( UsdStage, pxr::UsdGeomTokens->z );
	}

	return UsdStage;
#else
	UsdWrapperUtils::CheckIfForceDisabled();
	return UE::FUsdStage();
#endif // #if USE_USD_SDK
}

UE::FUsdStage UnrealUSDWrapper::GetClipboardStage()
{
	static UE::FUsdStage ClipboardStage;

#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	if ( !ClipboardStage )
	{
		// Use this specific name so that we can check for it on our automated tests
		pxr::UsdStageCacheContext StageCacheContext{ pxr::UsdUtilsStageCache::Get() };
		ClipboardStage = UE::FUsdStage{ pxr::UsdStage::CreateInMemory( TCHAR_TO_ANSI( TEXT( "unreal_clipboard_layer" ) ) ) };

		pxr::UsdGeomSetStageUpAxis( ClipboardStage, pxr::UsdGeomTokens->z );
	}
#endif // #if USE_USD_SDK

	return ClipboardStage;
}

TArray< UE::FUsdStage > UnrealUSDWrapper::GetAllStagesFromCache()
{
	TArray< UE::FUsdStage > StagesInCache;

#if USE_USD_SDK
	FScopedUsdAllocs UsdAllocs;

	for ( const pxr::UsdStageRefPtr& StageInCache : pxr::UsdUtilsStageCache::Get().GetAllStages() )
	{
		StagesInCache.Emplace( StageInCache );
	}
#endif // #if USE_USD_SDK

	return StagesInCache;
}

void UnrealUSDWrapper::EraseStageFromCache( const UE::FUsdStage& Stage )
{
#if USE_USD_SDK
	pxr::UsdUtilsStageCache::Get().Erase( Stage );
#endif // #if USE_USD_SDK
}

void UnrealUSDWrapper::SetupDiagnosticDelegate()
{
#if USE_USD_SDK
	if (Delegate.IsValid())
	{
		UnrealUSDWrapper::ClearDiagnosticDelegate();
	}

	Delegate = MakeUnique<FUsdDiagnosticDelegate>();

	pxr::TfDiagnosticMgr& DiagMgr = pxr::TfDiagnosticMgr::GetInstance();
	DiagMgr.AddDelegate(Delegate.Get());
#endif // USE_USD_SDK
}

void UnrealUSDWrapper::ClearDiagnosticDelegate()
{
#if USE_USD_SDK
	if (!Delegate.IsValid())
	{
		return;
	}

	pxr::TfDiagnosticMgr& DiagMgr = pxr::TfDiagnosticMgr::GetInstance();
	DiagMgr.RemoveDelegate(Delegate.Get());

	Delegate = nullptr;
#endif // USE_USD_SDK
}

class FUnrealUSDWrapperModule : public IUnrealUSDWrapperModule
{
public:
	virtual void StartupModule() override
	{
#if USE_USD_SDK
		LLM_SCOPE_BYTAG(Usd);

		// Path to USD base plugins
		FString UsdPluginsPath = FPaths::Combine( TEXT( ".." ), TEXT( "ThirdParty" ), TEXT( "USD" ), TEXT( "UsdResources" ) );
		UsdPluginsPath = FPaths::ConvertRelativePathToFull( UsdPluginsPath );
#if PLATFORM_WINDOWS
		UsdPluginsPath /= FPaths::Combine( TEXT( "Win64" ), TEXT( "plugins" ) );
#elif PLATFORM_LINUX
		UsdPluginsPath /= FPaths::Combine( TEXT( "Linux" ), TEXT( "plugins" ) );
#elif PLATFORM_MAC
		UsdPluginsPath /= FPaths::Combine( TEXT( "Mac" ), TEXT( "plugins" ) );
#endif // PLATFORM_WINDOWS

#ifdef USE_LIBRARIES_FROM_PLUGIN_FOLDER
		// e.g. "../../../Engine/Plugins/Importers/USDImporter/Source/ThirdParty"
		FString TargetDllFolder = FPaths::Combine( IPluginManager::Get().FindPlugin( TEXT( "USDImporter" ) )->GetBaseDir(), TEXT( "Source" ), TEXT( "ThirdParty" ) );

#if PLATFORM_WINDOWS
		TargetDllFolder /= FPaths::Combine( TEXT( "USD" ), TEXT( "bin" ) );
#elif PLATFORM_LINUX
		TargetDllFolder /= FPaths::Combine( TEXT( "Linux" ), TEXT( "bin" ), TEXT( "x86_64-unknown-linux-gnu" ) );
#elif PLATFORM_MAC
		TargetDllFolder /= FPaths::Combine( TEXT( "Mac" ), TEXT( "bin" ) );
#endif // PLATFORM_WINDOWS

#else
		FString TargetDllFolder = FPlatformProcess::BaseDir();
#endif // USE_LIBRARIES_FROM_PLUGIN_FOLDER

		// Path to the MaterialX standard data libraries.
		FString MaterialXStdDataLibsPath = FPaths::Combine( FPaths::EngineDir(), TEXT( "Binaries" ), TEXT("ThirdParty"), TEXT("MaterialX"), TEXT("libraries"));
		MaterialXStdDataLibsPath = FPaths::ConvertRelativePathToFull( MaterialXStdDataLibsPath );
		if (FPaths::DirectoryExists(MaterialXStdDataLibsPath))
		{
			FPlatformMisc::SetEnvironmentVar(TEXT("PXR_MTLX_STDLIB_SEARCH_PATHS"), *MaterialXStdDataLibsPath);
		}

		// Have to do this in USDClasses as we need the Json module, which is RTTI == false
		IUsdClassesModule::UpdatePlugInfoFiles(UsdPluginsPath, TargetDllFolder);

		// Combine our current plugins with any additional USD plugins the user may have set.
		TArray<FString> PluginDirectories;
		PluginDirectories.Add( UsdPluginsPath );
		for ( const FDirectoryPath& Directory : GetDefault<UUsdProjectSettings>()->AdditionalPluginDirectories )
		{
			if ( !Directory.Path.IsEmpty() )
			{
				PluginDirectories.Add( Directory.Path );
			}
		}

		{
			FScopedUsdAllocs UsdAllocs;

			std::vector< std::string > UsdPluginDirectories;
			UsdPluginDirectories.reserve( PluginDirectories.Num() );

			for ( const FString& Dir : PluginDirectories )
			{
				UsdPluginDirectories.push_back( TCHAR_TO_UTF8( *Dir ) );
			}

			PlugRegistry::GetInstance().RegisterPlugins( UsdPluginDirectories );
		}

#endif // USE_USD_SDK

		FUsdMemoryManager::Initialize();
		UnrealUSDWrapper::SetupDiagnosticDelegate();
	}

	virtual void ShutdownModule() override
	{
		UnrealUSDWrapper::ClearDiagnosticDelegate();
		FUsdMemoryManager::Shutdown();
	}
};

IMPLEMENT_MODULE_USD(FUnrealUSDWrapperModule, UnrealUSDWrapper);

#undef LOCTEXT_NAMESPACE
