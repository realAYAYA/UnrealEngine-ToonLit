// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialX/MaterialXUtils/MaterialXManager.h"
#include "InterchangeImportModule.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "MaterialX/InterchangeMaterialXDefinitions.h"
#include "MaterialX/MaterialXUtils/MaterialXOpenPBRSurfaceShader.h"
#include "MaterialX/MaterialXUtils/MaterialXSurfaceShader.h"
#include "MaterialX/MaterialXUtils/MaterialXStandardSurfaceShader.h"
#include "MaterialX/MaterialXUtils/MaterialXUsdPreviewSurfaceShader.h"
#include "MaterialX/MaterialXUtils/MaterialXSurfaceUnlitShader.h"
#include "MaterialX/MaterialXUtils/MaterialXPointLightShader.h"
#include "MaterialX/MaterialXUtils/MaterialXDirectionalLightShader.h"
#include "MaterialX/MaterialXUtils/MaterialXSpotLightShader.h"
#include "MaterialX/MaterialXUtils/MaterialXSurfaceMaterial.h"

#include "InterchangeMaterialDefinitions.h"
#include "InterchangeImportLog.h"


namespace mx = MaterialX;

//not a good solution to use semicolon because of drive disk on Windows
const TCHAR FMaterialXManager::TexturePayloadSeparator = TEXT('{');

#define MX_MATERIALFUNCTION(AssetName) TEXT("/Interchange/") + FString{bIsSubstrateEnabled ? TEXT("Substrate/") : TEXT("Functions/")} + TEXT(AssetName) TEXT(".") TEXT(AssetName)

FMaterialXManager::FMaterialXManager()
	: MatchingInputNames {
		{{TEXT(""),                            TEXT("amplitude")},   TEXT("Amplitude")},
		{{TEXT(""),                            TEXT("bg")},          TEXT("B")},
		{{TEXT(""),                            TEXT("center")},      TEXT("Center")},
		{{TEXT(""),                            TEXT("diminish")},    TEXT("Diminish")},
		{{TEXT(""),                            TEXT("fg")},          TEXT("A")},
		{{TEXT(""),                            TEXT("high")},        TEXT("Max")},
		{{TEXT(""),                            TEXT("in")},          TEXT("Input")},
		{{TEXT(""),                            TEXT("in1")},         TEXT("A")},
		{{TEXT(""),                            TEXT("in2")},         TEXT("B")},
		{{TEXT(""),                            TEXT("in3")},         TEXT("C")},
		{{TEXT(""),                            TEXT("in4")},         TEXT("D")},
		{{TEXT(""),                            TEXT("inlow")},       TEXT("InputLow")},
		{{TEXT(""),                            TEXT("inhigh")},      TEXT("InputHigh")},
		{{TEXT(""),                            TEXT("lacunarity")},  TEXT("Lacunarity")},
		{{TEXT(""),                            TEXT("low")},         TEXT("Min")},
		{{TEXT(""),                            TEXT("lumacoeffs")},  TEXT("LuminanceFactors")}, // for the moment not yet handled by Interchange, because of the attribute being an advanced pin
		{{TEXT(""),                            TEXT("mix")},         TEXT("Alpha")},
		{{TEXT(""),                            TEXT("offset")},      TEXT("Offset")},
		{{TEXT(""),                            TEXT("position")},    TEXT("Position")},
		{{TEXT(""),                            TEXT("texcoord")},    TEXT("Coordinates")},
		{{TEXT(""),                            TEXT("octaves")},     TEXT("Octaves")},
		{{TEXT(""),                            TEXT("temperature")}, TEXT("Temp")},
		{{TEXT(""),                            TEXT("outlow")},      TEXT("TargetLow")},
		{{TEXT(""),                            TEXT("outhigh")},     TEXT("TargetHigh")},
		{{TEXT(""),                            TEXT("valuel")},      TEXT("A")},
		{{TEXT(""),                            TEXT("valuer")},      TEXT("B")},
		{{TEXT(""),                            TEXT("valuet")},      TEXT("A")},
		{{TEXT(""),                            TEXT("valueb")},      TEXT("B")},
		{{TEXT(""),                            TEXT("valuetl")},     TEXT("A")},
		{{TEXT(""),                            TEXT("valuetr")},     TEXT("B")},
		{{TEXT(""),                            TEXT("valuebl")},     TEXT("C")},
		{{TEXT(""),                            TEXT("valuebr")},     TEXT("D")},
		{{TEXT(""),                            TEXT("value1")},      TEXT("A")},
		{{TEXT(""),                            TEXT("value2")},      TEXT("B")},
		{{MaterialX::Category::Atan2,          TEXT("in1")},         TEXT("Y")},
		{{MaterialX::Category::Atan2,          TEXT("in2")},         TEXT("X")},
		{{MaterialX::Category::HeightToNormal, TEXT("scale")},       UE::Interchange::Materials::Standard::Nodes::NormalFromHeightMap::Inputs::Intensity.ToString()},
		{{MaterialX::Category::IfGreater,      TEXT("in1")},         TEXT("AGreaterThanB")},
		{{MaterialX::Category::IfGreater,      TEXT("in2")},         TEXT("ALessThanB")}, //another input is added for the case equal see ConnectIfGreater
		{{MaterialX::Category::IfGreaterEq,    TEXT("in1")},         TEXT("AGreaterThanB")}, //another is added for the case equal see ConnectIfGreaterEq
		{{MaterialX::Category::IfGreaterEq,    TEXT("in2")},         TEXT("ALessThanB")},
		{{MaterialX::Category::IfEqual,        TEXT("in1")},         TEXT("AEqualsB")},
		{{MaterialX::Category::IfEqual,        TEXT("in2")},         TEXT("ALessThanB")},  // another input is added for the case greater see ConnectIfEqual
		{{MaterialX::Category::Inside,         TEXT("in")},          TEXT("A")},			  // Inside is treated as a Multiply node
		{{MaterialX::Category::Inside,         TEXT("mask")},        TEXT("B")},			  // Inside is treated as a Multiply node
		{{MaterialX::Category::Invert,         TEXT("amount")},      TEXT("A")},
		{{MaterialX::Category::Invert,         TEXT("in")},          TEXT("B")},
		{{MaterialX::Category::Mix,            TEXT("fg")},          TEXT("B")},
		{{MaterialX::Category::Mix,            TEXT("bg")},          TEXT("A")},
		{{MaterialX::Category::Mix,            TEXT("mix")},		 TEXT("Factor")},
		{{MaterialX::Category::Noise3D,        TEXT("amplitude")},   TEXT("B")},            // The amplitude of the noise is connected to a multiply node
		{{MaterialX::Category::Noise3D,        TEXT("pivot")},       TEXT("B")},            // The pivot of the noise is connected to a add node
		{{MaterialX::Category::Normalize,      TEXT("in")},          TEXT("VectorInput")},
		{{MaterialX::Category::Outside,        TEXT("in")},          TEXT("A")},				// Outside is treated as Multiply node
		{{MaterialX::Category::Outside,        TEXT("mask")},        TEXT("B")},				// Outside is treated as Multiply node
		{{MaterialX::Category::Place2D,        TEXT("pivot")},       TEXT("Pivot")},
		{{MaterialX::Category::Place2D,        TEXT("rotate")},      TEXT("RotationAngle")},
		{{MaterialX::Category::Power,          TEXT("in1")},         TEXT("Base")},
		{{MaterialX::Category::Power,	       TEXT("in2")},         TEXT("Exponent")},
		{{MaterialX::Category::Rotate2D,       TEXT("amount")},      TEXT("RotationAngle")},
		{{MaterialX::Category::Rotate3D,       TEXT("amount")},      TEXT("RotationAngle")},
		{{MaterialX::Category::Rotate3D,       TEXT("axis")},		 TEXT("NormalizedRotationAxis")},
		{{MaterialX::Category::Rotate3D,       TEXT("in")},          TEXT("Position")},
		{{MaterialX::Category::Saturate,       TEXT("amount")},      TEXT("Fraction")},
		{{MaterialX::Category::Smoothstep,     TEXT("in")},          TEXT("Value")},
	}
	, MatchingMaterialExpressions {
		// Math nodes
		{MaterialX::Category::Absval,       TEXT("Abs")},
		{MaterialX::Category::Add,          TEXT("Add")},
		{MaterialX::Category::Acos,         TEXT("Arccosine")},
		{MaterialX::Category::Asin,         TEXT("Arcsine")},
		{MaterialX::Category::Atan2,        TEXT("Arctangent2")},
		{MaterialX::Category::Ceil,         TEXT("Ceil")},
		{MaterialX::Category::Clamp,        TEXT("Clamp")},
		{MaterialX::Category::Cos,          TEXT("Cosine")},
		{MaterialX::Category::CrossProduct, TEXT("Crossproduct")},
		{MaterialX::Category::Divide,       TEXT("Divide")},
		{MaterialX::Category::DotProduct,   TEXT("Dotproduct")},
		{MaterialX::Category::Exp,          TEXT("Exponential")},
		{MaterialX::Category::Floor,        TEXT("Floor")},
		{MaterialX::Category::Invert,       TEXT("Subtract")},
		{MaterialX::Category::Ln,           TEXT("Logarithm")},
		{MaterialX::Category::Magnitude,    TEXT("Length")},
		{MaterialX::Category::Max,          TEXT("Max")},
		{MaterialX::Category::Min,          TEXT("Min")},
		{MaterialX::Category::Modulo,       TEXT("Fmod")},
		{MaterialX::Category::Multiply,     TEXT("Multiply")},
		{MaterialX::Category::Normalize,    TEXT("Normalize")},
		{MaterialX::Category::Place2D,		TEXT("MaterialXPlace2D")},
		{MaterialX::Category::Power,        TEXT("Power")},
		{MaterialX::Category::Rotate2D,		TEXT("MaterialXRotate2D")},
		{MaterialX::Category::RampLR,       TEXT("MaterialXRampLeftRight")},
		{MaterialX::Category::RampTB,       TEXT("MaterialXRampTopBottom")},
		{MaterialX::Category::Sign,         TEXT("Sign")},
		{MaterialX::Category::Sin,          TEXT("Sine")},
		{MaterialX::Category::SplitLR,      TEXT("MaterialXSplitLeftRight")},
		{MaterialX::Category::SplitTB,      TEXT("MaterialXSplitTopBottom")},
		{MaterialX::Category::Sqrt,         TEXT("SquareRoot")},
		{MaterialX::Category::Sub,          TEXT("Subtract")},
		{MaterialX::Category::Tan,          TEXT("Tangent")},
		// Compositing nodes
		{MaterialX::Category::Burn,         TEXT("MaterialXBurn")},
		{MaterialX::Category::Difference,   TEXT("MaterialXDifference")},
		{MaterialX::Category::Disjointover, TEXT("MaterialXDisjointover")},
		{MaterialX::Category::Dodge,        TEXT("MaterialXDodge")},
		{MaterialX::Category::In,           TEXT("MaterialXIn")},
		{MaterialX::Category::Inside,       TEXT("Multiply")},
		{MaterialX::Category::Mask,         TEXT("MaterialXMask")},
		{MaterialX::Category::Matte,        TEXT("MaterialXMatte")},
		{MaterialX::Category::Minus,        TEXT("MaterialXMinus")},
		{MaterialX::Category::Mix,          TEXT("Lerp")},
		{MaterialX::Category::Out,          TEXT("MaterialXOut")},
		{MaterialX::Category::Over,         TEXT("MaterialXOver")},
		{MaterialX::Category::Overlay,      TEXT("MaterialXOverlay")},
		{MaterialX::Category::Plus,         TEXT("MaterialXPlus")},
		{MaterialX::Category::Premult,      TEXT("MaterialXPremult")},
		{MaterialX::Category::Screen,       TEXT("MaterialXScreen")},
		{MaterialX::Category::Unpremult,    TEXT("MaterialXUnpremult")},
		// Channel nodes
		{MaterialX::Category::Combine2,     TEXT("AppendVector")},
		{MaterialX::Category::Combine3,     TEXT("MaterialXAppend3Vector")},
		{MaterialX::Category::Combine4,     TEXT("MaterialXAppend4Vector")},
		// PBR
		{MaterialX::Category::Blackbody,    TEXT("BlackBody")},
		// Procedural2D nodes
		{MaterialX::Category::Ramp4,        TEXT("MaterialXRamp4")},
		{MaterialX::Category::RampLR,       TEXT("MaterialXRampLeftRight")},
		{MaterialX::Category::RampTB,       TEXT("MaterialXRampTopBottom")},
		{MaterialX::Category::SplitLR,      TEXT("MaterialXSplitLeftRight")},
		{MaterialX::Category::SplitTB,      TEXT("MaterialXSplitTopBottom")},
		// Procedural3D nodes
		{MaterialX::Category::Fractal3D,    TEXT("MaterialXFractal3D")},
		// Geometric nodes 
		{MaterialX::Category::GeomColor,	TEXT("VertexColor")},
		// Adjustment nodes,
		{MaterialX::Category::HsvToRgb,		TEXT("HsvToRgb")},
		{MaterialX::Category::Luminance,    TEXT("MaterialXLuminance")},
		{MaterialX::Category::Remap,        TEXT("MaterialXRemap")},
		{MaterialX::Category::RgbToHsv,		TEXT("RgbToHsv")},
		{MaterialX::Category::Saturate,		TEXT("Desaturation")},
		{MaterialX::Category::Smoothstep,   TEXT("SmoothStep")}
	}
	, MaterialExpressionInputs {
		TEXT("A"),
		TEXT("Alpha"),
		TEXT("Amplitude"),
		TEXT("B"),
		TEXT("Base"),
		TEXT("C"),
		TEXT("Center"),
		TEXT("Coordinates"),
		TEXT("D"),
		TEXT("Exponent"),
		TEXT("Factor"),
		TEXT("Fraction"),
		TEXT("Input"),
		TEXT("InputLow"),
		TEXT("InputHigh"),
		TEXT("Lacunarity"),
		TEXT("LuminanceFactors"),
		TEXT("Max"),
		TEXT("Min"),
		TEXT("Octaves"),
		TEXT("Offset"),
		TEXT("Pivot"),
		TEXT("Position"),
		TEXT("RotationAngle"),
		TEXT("Scale"),
		TEXT("TargetLow"),
		TEXT("TargetHigh"),
		TEXT("Temp"),
		TEXT("Value"),
		TEXT("VectorInput"),
		TEXT("X"),
		TEXT("Y")
	}
	, MaterialXContainerDelegates{
		{mx::Category::OpenPBRSurface, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXOpenPBRSurfaceShader::MakeInstance)},
		{mx::Category::Surface, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXSurfaceShader::MakeInstance)},
		{mx::Category::SurfaceUnlit, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXSurfaceUnlitShader::MakeInstance)},
		{mx::Category::StandardSurface, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXStandardSurfaceShader::MakeInstance)},
		{mx::Category::UsdPreviewSurface, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXUsdPreviewSurfaceShader::MakeInstance)},
		{mx::Category::PointLight, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXPointLightShader::MakeInstance)},
		{mx::Category::DirectionalLight, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXDirectionalLightShader::MakeInstance)},
		{mx::Category::SpotLight, FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXSpotLightShader::MakeInstance)},
		{mx::SURFACE_MATERIAL_NODE_STRING.c_str(), FMaterialXManager::FOnGetMaterialXInstance::CreateStatic(&FMaterialXSurfaceMaterial::MakeInstance)}
	}
	, bIsSubstrateEnabled{ IInterchangeImportModule::IsAvailable() ? IInterchangeImportModule::Get().IsSubstrateEnabled() : false }
{
		MatchingMaterialFunctions = {
			// BSDF Nodes
			{mx::Category::BurleyDiffuseBSDF,		FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::BurleyDiffuse}},
			{mx::Category::ConductorBSDF,			FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::Conductor}},
			{mx::Category::DielectricBSDF,			FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::Dielectric}},
			{mx::Category::GeneralizedSchlickBSDF,	FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::GeneralizedSchlick}},
			{mx::Category::OrenNayarDiffuseBSDF ,	FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::OrenNayarDiffuse}},
			{mx::Category::SheenBSDF,				FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::Sheen}},
			{mx::Category::SubsurfaceBSDF ,			FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::Subsurface}},
			{mx::Category::ThinFilmBSDF,			FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::ThinFilm}},
			{mx::Category::TranslucentBSDF,			FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXBSDF>{}, EInterchangeMaterialXBSDF::Translucent}},
			// EDF Nodes
			{mx::Category::ConicalEDF,				FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXEDF>{}, EInterchangeMaterialXEDF::Conical}},
			{mx::Category::MeasuredEDF,				FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXEDF>{}, EInterchangeMaterialXEDF::Measured}},
			{mx::Category::UniformEDF,				FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXEDF>{}, EInterchangeMaterialXEDF::Uniform}},
			// VDF Nodes
			{mx::Category::AbsorptionVDF,			FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXVDF>{}, EInterchangeMaterialXVDF::Absorption}},
			{mx::Category::AnisotropicVDF,			FMaterialXMaterialFunction{TInPlaceType<EInterchangeMaterialXVDF>{}, EInterchangeMaterialXVDF::Anisotropic}},
			// Utility nodes
			{mx::Category::ArtisticIOR,				FMaterialXMaterialFunction{TInPlaceType<FString>{}, TEXT("/Interchange/Functions/MX_Artistic_IOR.MX_Artistic_IOR")}},
			{mx::Category::RoughnessAnisotropy,		FMaterialXMaterialFunction{TInPlaceType<FString>{}, TEXT("/Interchange/Functions/MX_Roughness_Anisotropy.MX_Roughness_Anisotropy")}},
			{mx::Category::RoughnessDual,			FMaterialXMaterialFunction{TInPlaceType<FString>{}, TEXT("/Interchange/Functions/MX_Roughness_Dual.MX_Roughness_Dual")}}
		};

		if(bIsSubstrateEnabled)
		{
			// Vertical Layering
			MatchingInputNames.Add({ TEXT(""), TEXT("top") }, TEXT("Top"));
			MatchingInputNames.Add({ TEXT(""), TEXT("base") }, TEXT("Bottom"));
			MaterialExpressionInputs.Add(TEXT("Top"));
			MaterialExpressionInputs.Add(TEXT("Bottom"));
			MatchingMaterialExpressions.Add({ MaterialX::Category::Layer }, TEXT("SubstrateVerticalLayering"));

			// Horizontal Layering, MaterialX names it mix so we have to be careful not to take the lerp expression
			MatchingInputNames.Add({ FKeyCategoryNodegroup{MaterialX::Category::Mix, MaterialX::NodeGroup::PBR}, TEXT("bg") }, TEXT("Background"));
			MatchingInputNames.Add({ FKeyCategoryNodegroup{MaterialX::Category::Mix, MaterialX::NodeGroup::PBR}, TEXT("fg") }, TEXT("Foreground"));
			MatchingInputNames.Add({ FKeyCategoryNodegroup{MaterialX::Category::Mix, MaterialX::NodeGroup::PBR}, TEXT("mix") }, TEXT("Mix"));
			MaterialExpressionInputs.Add(TEXT("Background"));
			MaterialExpressionInputs.Add(TEXT("Foreground"));
			MaterialExpressionInputs.Add(TEXT("Mix"));
			MatchingMaterialExpressions.Add({ MaterialX::Category::Mix, MaterialX::NodeGroup::PBR }, TEXT("SubstrateHorizontalMixing"));

			// Add
			MatchingMaterialExpressions.Add({ MaterialX::Category::Add, MaterialX::NodeGroup::PBR }, TEXT("SubstrateAdd"));

			// Multiply
			MatchingInputNames.Add({ FKeyCategoryNodegroup{MaterialX::Category::Multiply, MaterialX::NodeGroup::PBR}, TEXT("in2") }, TEXT("Weight"));
			MaterialExpressionInputs.Add(TEXT("Weight"));
			MatchingMaterialExpressions.Add({ MaterialX::Category::Multiply, MaterialX::NodeGroup::PBR }, TEXT("SubstrateWeight"));
		}
	}

FMaterialXManager& FMaterialXManager::GetInstance()
{
	static FMaterialXManager Instance;
	return Instance;
}

const FString* FMaterialXManager::FindMatchingInput(const FString& CategoryKey, const FString& InputKey, const FString& NodeGroup) const
{
	return MatchingInputNames.Find({ FKeyCategoryNodegroup{ CategoryKey, NodeGroup}, InputKey });
}

const FString* FMaterialXManager::FindMaterialExpressionInput(const FString& InputKey) const
{
	return MaterialExpressionInputs.Find(InputKey);
}

const FString* FMaterialXManager::FindMatchingMaterialExpression(const FString& CategoryKey, const FString& NodeGroup) const
{
	return MatchingMaterialExpressions.Find({ CategoryKey, NodeGroup });
}

bool FMaterialXManager::FindMatchingMaterialFunction(const FString& CategoryKey, const FString*& MaterialFunctionPath, uint8& EnumType, uint8& EnumValue) const
{
	if(const FMaterialXMaterialFunction* MaterialFunction = MatchingMaterialFunctions.Find(CategoryKey))
	{
		MaterialFunctionPath = MaterialFunction->TryGet<FString>();
		if(const EInterchangeMaterialXShaders* Shader = MaterialFunction->TryGet<EInterchangeMaterialXShaders>())
		{
			EnumType = UE::Interchange::MaterialX::IndexSurfaceShaders;
			EnumValue = static_cast<uint8>(*Shader);
		}
		else if(const EInterchangeMaterialXBSDF* Bsdf = MaterialFunction->TryGet<EInterchangeMaterialXBSDF>())
		{
			EnumType = UE::Interchange::MaterialX::IndexBSDF;
			EnumValue = static_cast<uint8>(*Bsdf);
		}
		else if(const EInterchangeMaterialXEDF* Edf = MaterialFunction->TryGet<EInterchangeMaterialXEDF>())
		{
			EnumType = UE::Interchange::MaterialX::IndexEDF;
			EnumValue = static_cast<uint8>(*Edf);
		}
		else if(const EInterchangeMaterialXVDF* Vdf = MaterialFunction->TryGet<EInterchangeMaterialXVDF>())
		{
			EnumType = UE::Interchange::MaterialX::IndexVDF;
			EnumValue = static_cast<uint8>(*Vdf);
		}
		return true;
	}

	return false;
}

TSharedPtr<FMaterialXBase> FMaterialXManager::GetShaderTranslator(const FString& CategoryShader, UInterchangeBaseNodeContainer & NodeContainer)
{
	if(FOnGetMaterialXInstance* InstanceDelegate = MaterialXContainerDelegates.Find(CategoryShader))
	{
		if(InstanceDelegate->IsBound())
		{
			return InstanceDelegate->Execute(NodeContainer);
		}
	}

	return nullptr;
}

void FMaterialXManager::RegisterMaterialXInstance(const FString& Category, FOnGetMaterialXInstance MaterialXInstanceDelegate)
{
	if(!Category.IsEmpty())
	{
		MaterialXContainerDelegates.Add(Category, MaterialXInstanceDelegate);
	}
}

bool FMaterialXManager::IsSubstrateEnabled() const
{
	return bIsSubstrateEnabled;
}
#endif

namespace UE::Interchange::MaterialX
{
	bool AreMaterialFunctionPackagesLoaded()
	{
#if WITH_EDITOR
		auto ArePackagesLoaded = [](const TArray<FString>& TextPaths) -> bool
		{
			bool bAllLoaded = true;
			for(const FString& TextPath : TextPaths)
			{
				const FString FunctionPath{ FPackageName::ExportTextPathToObjectPath(TextPath) };
				if(FPackageName::DoesPackageExist(FunctionPath))
				{
					if(!FSoftObjectPath(FunctionPath).TryLoad())
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("Couldn't load %s"), *FunctionPath);
						bAllLoaded = false;
					}
				}
				else
				{
					UE_LOG(LogInterchangeImport, Warning, TEXT("Couldn't find %s"), *FunctionPath);
					bAllLoaded = false;
				}
			}

			return bAllLoaded;
		};

		static const bool bPackagesLoaded =	ArePackagesLoaded({ TEXT("MaterialFunction'/Engine/Functions/Engine_MaterialFunctions03/Procedurals/NormalFromHeightmap.NormalFromHeightmap'"),
															    TEXT("MaterialFunction'/Interchange/Functions/MX_Artistic_IOR.MX_Artistic_IOR'"),
															    TEXT("MaterialFunction'/Interchange/Functions/MX_Roughness_Anisotropy.MX_Roughness_Anisotropy'"), 
															    TEXT("MaterialFunction'/Interchange/Functions/MX_Roughness_Dual.MX_Roughness_Dual'")});

		return bPackagesLoaded;
#else
		return false;
#endif
	}
}

#undef MX_MATERIALFUNCTION