// Copyright Epic Games, Inc. All Rights Reserved. 
#pragma once

// Interchange currently only brings in MaterialX when in the editor, so we
// define its namespace macros ourselves otherwise.
#if WITH_EDITOR

#include "Math/Color.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"

THIRD_PARTY_INCLUDES_START
#include "MaterialXCore/Library.h"
THIRD_PARTY_INCLUDES_END

#else

#define MATERIALX_NAMESPACE_BEGIN namespace MaterialX {
#define MATERIALX_NAMESPACE_END }

#endif


#include "InterchangeMaterialXDefinitions.generated.h"

UENUM(BlueprintType)
enum class EInterchangeMaterialXShaders : uint8
{
	/** Default settings for Open PBR Surface shader. */
	OpenPBRSurface,

	/** Open PBR Surface shader	used for translucency. */
	OpenPBRSurfaceTransmission,

	/** Default settings for Autodesk's Standard Surface shader. */
	StandardSurface,

	/** Standard Surface shader used for translucency. */
	StandardSurfaceTransmission,

	/** Shader used for unlit surfaces. */
	SurfaceUnlit,

	/** Default settings for USD's Surface shader. */
	UsdPreviewSurface,

	/** A surface shader constructed from scattering and emission distribution functions. */
	Surface,

	MaxShaderCount UMETA(hidden)
};

UENUM(BlueprintType)
/** Data type representing a Bidirectional Scattering Distribution Function. */
enum class EInterchangeMaterialXBSDF : uint8
{
	/** A BSDF node for diffuse reflections. */
	OrenNayarDiffuse,

	/** A BSDF node for Burley diffuse reflections. */
	BurleyDiffuse,

	/** A BSDF node for pure diffuse transmission. */
	Translucent,

	/** A reflection/transmission BSDF node based on a microfacet model and a Fresnel curve for dielectrics. */
	Dielectric,

	/** A reflection BSDF node based on a microfacet model and a Fresnel curve for conductors/metals. */
	Conductor,

	/** A reflection/transmission BSDF node based on a microfacet model and a generalized Schlick Fresnel curve. */
	GeneralizedSchlick,

	/** A subsurface scattering BSDF for true subsurface scattering. */
	Subsurface,

	/** A microfacet BSDF for the back-scattering properties of cloth-like materials. */
	Sheen,

	/** Adds an iridescent thin film layer over a microfacet base BSDF. */
	ThinFilm,

	MaxBSDFCount UMETA(hidden)
};

UENUM(BlueprintType)
/** Data type representing an Emission Distribution Function. */
enum class EInterchangeMaterialXEDF : uint8
{
	/** An EDF node for uniform emission. */
	Uniform,

	/** Constructs an EDF emitting light inside a cone around the normal direction. */
	Conical,

	/** Constructs an EDF emitting light according to a measured IES light profile. */
	Measured,

	MaxEDFCount UMETA(hidden)
};

UENUM(BlueprintType)
/** Data type representing a Volume Distribution Function. */
enum class EInterchangeMaterialXVDF : uint8
{
	/** Constructs a VDF for pure light absorption. */
	Absorption,

	/** Constructs a VDF scattering light for a participating medium, based on the Henyey-Greenstein phase function. */
	Anisotropic,

	MaxVDFCount UMETA(hidden)
};

namespace UE
{
	namespace Interchange
	{
		namespace MaterialX
		{
			static constexpr uint8 IndexSurfaceShaders = 0;
			static constexpr uint8 IndexBSDF = 1;
			static constexpr uint8 IndexEDF = 2;
			static constexpr uint8 IndexVDF = 3;

			namespace Attributes
			{
				constexpr const TCHAR* EnumType = TEXT("MaterialXEnumType");
				constexpr const TCHAR* EnumValue = TEXT("MaterialXEnumValue");
			}
		}
	}
}

MATERIALX_NAMESPACE_BEGIN

	namespace OpenPBRSurface
	{
		namespace Input
		{
			static constexpr const char* BaseWeight = "base_weight";
			static constexpr const char* BaseColor = "base_color";
			static constexpr const char* BaseRoughness = "base_roughness";
			static constexpr const char* BaseMetalness = "base_metalness";
			static constexpr const char* SpecularWeight = "specular_weight";
			static constexpr const char* SpecularColor = "specular_color";
			static constexpr const char* SpecularRoughness = "specular_roughness";
			static constexpr const char* SpecularIOR = "specular_ior";
			static constexpr const char* SpecularIORLevel = "specular_ior_level";
			static constexpr const char* SpecularAnisotropy = "specular_anisotropy";
			static constexpr const char* SpecularRotation = "specular_rotation";
			static constexpr const char* TransmissionWeight = "transmission_weight";
			static constexpr const char* TransmissionColor = "transmission_color";
			static constexpr const char* TransmissionDepth = "transmission_depth";
			static constexpr const char* TransmissionScatter = "transmission_scatter";
			static constexpr const char* TransmissionScatterAnisotropy= "transmission_scatter_anisotropy";
			static constexpr const char* TransmissionDispersionScale = "transmission_dispersion_scale";
			static constexpr const char* TransmissionDispersionAbbeNumber= "transmission_dispersion_abbe_number";
			static constexpr const char* SubsurfaceWeight = "subsurface_weight";
			static constexpr const char* SubsurfaceColor = "subsurface_color";
			static constexpr const char* SubsurfaceRadius = "subsurface_radius";
			static constexpr const char* SubsurfaceRadiusScale = "subsurface_radius_scale";
			static constexpr const char* SubsurfaceAnisotropy = "subsurface_anisotropy";
			static constexpr const char* FuzzWeight = "fuzz_weight";
			static constexpr const char* FuzzColor = "fuzz_color";
			static constexpr const char* FuzzRoughness = "fuzz_roughness";
			static constexpr const char* CoatWeight = "coat_weight";
			static constexpr const char* CoatColor = "coat_color";
			static constexpr const char* CoatRoughness = "coat_roughness";
			static constexpr const char* CoatAnisotropy = "coat_anisotropy";
			static constexpr const char* CoatRotation = "coat_rotation";
			static constexpr const char* CoatIOR = "coat_ior";
			static constexpr const char* CoatIORLevel = "coat_ior_level";
			static constexpr const char* ThinFilmThickness = "thin_film_thickness";
			static constexpr const char* ThinFilmIOR = "thin_film_ior";
			static constexpr const char* EmissionLuminance = "emission_luminance";
			static constexpr const char* EmissionColor = "emission_color";
			static constexpr const char* GeometryOpacity = "geometry_opacity";
			static constexpr const char* GeometryThinWalled = "geometry_thin_walled";
			static constexpr const char* GeometryNormal = "geometry_normal";
			static constexpr const char* GeometryCoatNormal = "geometry_coat_normal";
			static constexpr const char* GeometryTangent = "geometry_tangent";
		}

		namespace DefaultValue
		{
			static constexpr float BaseWeight = 1.f;
			static constexpr FLinearColor BaseColor{ 0.8, 0.8, 0.8 };
			static constexpr float BaseRoughness = 0.f;
			static constexpr float BaseMetalness = 0.f;
			static constexpr float SpecularWeight = 1.f;
			static constexpr FLinearColor SpecularColor{ 1, 1, 1 };
			static constexpr float SpecularRoughness = 0.3f;
			static constexpr float SpecularIOR = 1.5f;
			static constexpr float SpecularIORLevel = 0.5f;
			static constexpr float SpecularAnisotropy = 0.f;
			static constexpr float SpecularRotation = 0.f;
			static constexpr float TransmissionWeight = 0.f;
			static constexpr FLinearColor TransmissionColor{ 1, 1, 1 };
			static constexpr float TransmissionDepth = 0.f;
			static constexpr FLinearColor TransmissionScatter{ 0, 0, 0 };
			static constexpr float TransmissionScatterAnisotropy = 0.f;
			static constexpr float TransmissionDispersionScale = 0.f;
			static constexpr float TransmissionDispersionAbbeNumber = 0.f;
			static constexpr float SubsurfaceWeight = 0.f;
			static constexpr FLinearColor SubsurfaceColor{ 0.8, 0.8, 0.8 };
			static constexpr float SubsurfaceRadius = 1.f;
			static constexpr FLinearColor SubsurfaceRadiusScale{1, 0.5, 0.25};
			static constexpr float SubsurfaceAnisotropy = 0.f;
			static constexpr float FuzzWeight = 0.f;
			static constexpr FLinearColor FuzzColor{ 1, 1, 1 };
			static constexpr float FuzzRoughness = 0.5f;
			static constexpr float CoatWeight = 0.f;
			static constexpr FLinearColor CoatColor{ 1, 1, 1 };
			static constexpr float CoatRoughness = 0.f;
			static constexpr float CoatAnisotropy = 0.f;
			static constexpr float CoatRotation = 0.f;
			static constexpr float CoatIOR = 1.6f;
			static constexpr float CoatIORLevel = 0.5f;
			static constexpr float ThinFilmThickness = 0.f;
			static constexpr float ThinFilmIOR = 1.5f;
			static constexpr float EmissionLuminance = 0.f;
			static constexpr FLinearColor EmissionColor{ 1, 1, 1 };
			static constexpr FLinearColor GeometryOpacity{ 1, 1, 1 };
			static constexpr bool GeometryThinWalled = false;
			static const FVector GeometryNormal{ 0, 0, 1 };
			static const FVector GeometryCoatNormal{ 0, 0, 1 };
			static const FVector GeometryTangent{ 0, 1, 0 };
		}
	}

	namespace StandardSurface
	{
		namespace Input
		{
			static constexpr const char* Base = "base";
			static constexpr const char* BaseColor = "base_color";
			static constexpr const char* DiffuseRoughness = "diffuse_roughness";
			static constexpr const char* Metalness = "metalness";
			static constexpr const char* Specular = "specular";
			static constexpr const char* SpecularColor = "specular_color";
			static constexpr const char* SpecularRoughness = "specular_roughness";
			static constexpr const char* SpecularIOR = "specular_IOR";
			static constexpr const char* SpecularAnisotropy = "specular_anisotropy";
			static constexpr const char* SpecularRotation = "specular_rotation";
			static constexpr const char* Transmission = "transmission";
			static constexpr const char* TransmissionColor = "transmission_color";
			static constexpr const char* TransmissionDepth = "transmission_depth";
			static constexpr const char* TransmissionScatter = "transmission_scatter";
			static constexpr const char* TransmissionScatterAnisotropy = "transmission_scatter_anisotropy";
			static constexpr const char* TransmissionDispersion = "transmission_dispersion";
			static constexpr const char* TransmissionExtraRoughness = "transmission_extra_roughness";
			static constexpr const char* Subsurface = "subsurface";
			static constexpr const char* SubsurfaceColor = "subsurface_color";
			static constexpr const char* SubsurfaceRadius = "subsurface_radius";
			static constexpr const char* SubsurfaceScale = "subsurface_scale";
			static constexpr const char* SubsurfaceAnisotropy = "subsurface_anisotropy";
			static constexpr const char* Sheen = "sheen";
			static constexpr const char* SheenColor = "sheen_color";
			static constexpr const char* SheenRoughness = "sheen_roughness";
			static constexpr const char* Coat = "coat";
			static constexpr const char* CoatColor = "coat_color";
			static constexpr const char* CoatRoughness = "coat_roughness";
			static constexpr const char* CoatAnisotropy = "coat_anisotropy";
			static constexpr const char* CoatRotation = "coat_rotation";
			static constexpr const char* CoatIOR = "coat_IOR";
			static constexpr const char* CoatNormal = "coat_normal";
			static constexpr const char* CoatAffectColor = "coat_affect_color";
			static constexpr const char* CoatAffectRoughness = "coat_affect_roughness";
			static constexpr const char* ThinFilmThickness = "thin_film_thickness";
			static constexpr const char* ThinFilmIOR = "thin_film_IOR";
			static constexpr const char* Emission = "emission";
			static constexpr const char* EmissionColor = "emission_color";
			static constexpr const char* Opacity = "opacity";
			static constexpr const char* ThinWalled = "thin_walled";
			static constexpr const char* Normal = "normal";
			static constexpr const char* Tangent = "tangent";
		}

		namespace DefaultValue
		{
			namespace Float
			{
				static float Base = 1.f;
				static float DiffuseRoughness = 0.f;
				static float Metalness = 0.f;
				static float Specular = 1.f;
				static float SpecularRoughness = 0.2f;
				static float SpecularIOR = 1.5f;
				static float SpecularAnisotropy = 0.f;
				static float SpecularRotation = 0.f;
				static float Transmission = 0.f;
				static float TransmissionDepth = 0.f;
				static float TransmissionScatterAnisotropy = 0.f;
				static float TransmissionDispersion = 0.f;
				static float TransmissionExtraRoughness = 0.f;
				static float Subsurface = 0.f;
				static float SubsurfaceScale = 1.f;
				static float SubsurfaceAnisotropy = 0.f;
				static float Sheen = 0.f;
				static float SheenRoughness = 0.3f;
				static float Coat = 0.f;
				static float CoatRoughness = 0.1f;
				static float CoatAnisotropy = 0.f;
				static float CoatRotation = 0.f;
				static float CoatIOR = 1.5f;
				static float CoatAffectColor = 0.f;
				static float CoatAffectRoughness = 0.f;
				static float ThinFilmThickness = 0.f;
				static float ThinFilmIOR = 1.5f;
				static float Emission = 0.f;
			}

			namespace Color3
			{
				constexpr FLinearColor BaseColor{ 0.8, 0.8, 0.8 };
				constexpr FLinearColor SpecularColor{ 1, 1, 1 };
				constexpr FLinearColor TransmissionColor{ 1, 1, 1 };
				constexpr FLinearColor TransmissionScatter{ 0, 0, 0 };
				constexpr FLinearColor SubsurfaceColor{ 1, 1, 1 };
				constexpr FLinearColor SubsurfaceRadius{ 1, 1, 1 };
				constexpr FLinearColor SheenColor{ 1, 1, 1 };
				constexpr FLinearColor CoatColor{ 1, 1, 1 };
				constexpr FLinearColor EmissionColor{ 1, 1, 1 };
				constexpr FLinearColor Opacity{ 1, 1, 1 };
			}
		}
	}

	namespace SurfaceUnlit
	{
		namespace Input
		{
			static constexpr const char* Emission = "emission";
			static constexpr const char* EmissionColor = "emission_color";
			static constexpr const char* Transmission = "transmission";
			static constexpr const char* TransmissionColor = "transmission_color";
			static constexpr const char* Opacity = "opacity";
		}

		namespace DefaultValue
		{
			namespace Float
			{
				constexpr float Emission = 1.f;
				constexpr float Transmission = 0.f;
				constexpr float Opacity = 1.f;
			}

			namespace Color3
			{
				constexpr FLinearColor EmissionColor{ 1, 1, 1 };
				constexpr FLinearColor TransmissionColor{ 1, 1, 1 };
			}
		}
	}

	namespace Surface
	{
		namespace Input
		{
			static constexpr const char* Bsdf = "bsdf";
			static constexpr const char* Edf = "edf";
			static constexpr const char* Opacity = "opacity";
		}

		namespace DefaultValue
		{
			namespace Float
			{
				constexpr float Opacity = 1.f;
			}
		}
	}

	namespace UsdPreviewSurface
	{
		namespace Input
		{
			static constexpr const char* DiffuseColor = "diffuseColor";
			static constexpr const char* EmissiveColor = "emissiveColor";
			static constexpr const char* SpecularColor = "specularColor";
			static constexpr const char* Metallic = "metallic";
			static constexpr const char* Roughness = "roughness";
			static constexpr const char* Clearcoat = "clearcoat";
			static constexpr const char* ClearcoatRoughness = "clearcoatRoughness";
			static constexpr const char* Opacity = "opacity";
			static constexpr const char* OpacityThreshold = "opacityThreshold";
			static constexpr const char* IOR = "ior";
			static constexpr const char* Normal = "normal";
			static constexpr const char* Displacement = "displacement";
			static constexpr const char* Occlusion = "occlusion";
		}

		namespace DefaultValue
		{
			namespace Float
			{
				constexpr float Metallic = 0.f;
				constexpr float Roughness = 0.5f;
				constexpr float Clearcoat = 0.f;
				constexpr float ClearcoatRoughness = 0.01f;
				constexpr float Opacity = 1.f;
				constexpr float OpacityThreshold = 0.f;
				constexpr float IOR = 1.5f;
				constexpr float Displacement = 0.f;
				constexpr float Occlusion = 1.f;
			}

			namespace Color3
			{
				constexpr FLinearColor DiffuseColor{ 0.18f, 0.18f, 0.18f };
				constexpr FLinearColor EmissiveColor{ 0, 0, 0 };
				constexpr FLinearColor SpecularColor{ 0, 0, 0 };
			}
			
			namespace Vector3
			{
				static const FVector3f Normal{ 0.f, 0.f, 1.f };
			}
		}
	}

	namespace Lights
	{
		//There's no input per se in a Light, but we can find some common inputs among those lights
		namespace Input
		{
			static constexpr const char* Color = "color";
			static constexpr const char* Intensity = "intensity";
		}

		namespace PointLight
		{
			namespace Input
			{
				using namespace Lights::Input;
				static constexpr const char* Position = "position";
				static constexpr const char* DecayRate = "decay_rate";
			}
		}

		namespace DirectionalLight
		{
			namespace Input
			{
				using namespace Lights::Input;
				static constexpr const char* Direction = "direction";
			}
		}

		namespace SpotLight
		{
			namespace Input
			{
				using namespace PointLight::Input;
				using namespace DirectionalLight::Input;
				static constexpr const char* InnerAngle = "inner_angle";
				static constexpr const char* OuterAngle = "outer_angle";
			}
		}
	}

	namespace Attributes
	{
		static constexpr const char* IsVisited = "UE:IsVisited";
		static constexpr const char* NewName = "UE:NewName";
		static constexpr const char* ParentName = "UE:ParentName";
	}

	namespace Category
	{
		// Math nodes
		static constexpr const char* Absval = "absval";
		static constexpr const char* Acos = "acos";
		static constexpr const char* Add = "add";
		static constexpr const char* ArrayAppend = "arrayappend";
		static constexpr const char* Asin = "asin";
		static constexpr const char* Atan2 = "atan2";
		static constexpr const char* Ceil = "ceil";
		static constexpr const char* Clamp = "clamp";
		static constexpr const char* Cos = "cos";
		static constexpr const char* CrossProduct = "crossproduct";
		static constexpr const char* Determinant = "determinant";
		static constexpr const char* Divide = "divide";
		static constexpr const char* DotProduct = "dotproduct";
		static constexpr const char* Exp = "exp";
		static constexpr const char* Floor = "floor";
		static constexpr const char* Invert = "invert";
		static constexpr const char* InvertMatrix = "invertmatrix";
		static constexpr const char* Ln = "ln";
		static constexpr const char* Magnitude = "magnitude";
		static constexpr const char* Max = "max";
		static constexpr const char* Min = "min";
		static constexpr const char* Modulo = "modulo";
		static constexpr const char* Multiply = "multiply";
		static constexpr const char* Normalize = "normalize";
		static constexpr const char* NormalMap = "normalmap";
		static constexpr const char* Place2D = "place2d";
		static constexpr const char* Power = "power";
		static constexpr const char* Rotate2D = "rotate2d";
		static constexpr const char* Rotate3D = "rotate3d";
		static constexpr const char* Sign = "sign";
		static constexpr const char* Sin = "sin";
		static constexpr const char* Sqrt = "sqrt";
		static constexpr const char* Sub = "subtract";
		static constexpr const char* Tan = "tan";
		static constexpr const char* TransformMatrix = "transformmatrix";
		static constexpr const char* TransformNormal = "transformnormal";
		static constexpr const char* TransformPoint = "transformpoint";
		static constexpr const char* TransformVector = "transformvector";
		static constexpr const char* Transpose = "transpose";		
		// Compositing nodes
		static constexpr const char* Burn = "burn";
		static constexpr const char* Difference = "difference";
		static constexpr const char* Disjointover = "disjointover";
		static constexpr const char* Dodge = "dodge";
		static constexpr const char* In = "in";
		static constexpr const char* Inside = "inside";
		static constexpr const char* Mask = "mask";
		static constexpr const char* Matte = "matte";
		static constexpr const char* Minus = "minus";
		static constexpr const char* Mix = "mix";
		static constexpr const char* Out = "out";
		static constexpr const char* Outside = "outside";
		static constexpr const char* Over = "over";
		static constexpr const char* Overlay = "overlay";
		static constexpr const char* Plus = "plus";
		static constexpr const char* Premult = "premult";
		static constexpr const char* Screen = "screen";
		static constexpr const char* Unpremult = "unpremult";
		// Conditional nodes
		static constexpr const char* IfGreater = "ifgreater";
		static constexpr const char* IfGreaterEq = "ifgreatereq";
		static constexpr const char* IfEqual = "ifequal";
		// Channel nodes
		static constexpr const char* Convert = "convert";
		static constexpr const char* Combine2 = "combine2";
		static constexpr const char* Combine3 = "combine3";
		static constexpr const char* Combine4 = "combine4";
		static constexpr const char* Extract = "extract";
		static constexpr const char* Separate2 = "separate2";
		static constexpr const char* Separate3 = "separate3";
		static constexpr const char* Separate4 = "separate4";
		static constexpr const char* Swizzle = "swizzle";
		// Procedural nodes 
		static constexpr const char* Constant = "constant";
		// Procedural2D nodes 
		static constexpr const char* Ramp4 = "ramp4";
		static constexpr const char* RampLR = "ramplr";
		static constexpr const char* RampTB = "ramptb";
		static constexpr const char* SplitLR = "splitlr";
		static constexpr const char* SplitTB = "splittb";
		// Procedural3D nodes 
		static constexpr const char* CellNoise3D = "cellnoise3d";
		static constexpr const char* Fractal3D = "fractal3d";
		static constexpr const char* Noise3D = "noise3d";
		static constexpr const char* WorleyNoise3D = "worleynoise3d";
		// Organization nodes 
		static constexpr const char* Dot = "dot";
		// Texture nodes 
		static constexpr const char* Image = "image";
		static constexpr const char* TiledImage = "tiledimage";
		// Geometric nodes
		static constexpr const char* Bitangent = "bitangent";
		static constexpr const char* GeomColor = "geomcolor";
		static constexpr const char* Normal = "normal";
		static constexpr const char* Position = "position";
		static constexpr const char* Tangent = "tangent";
		static constexpr const char* TexCoord = "texcoord";
		// Light nodes
		static constexpr const char* DirectionalLight = "directional_light";
		static constexpr const char* PointLight = "point_light";
		static constexpr const char* SpotLight = "spot_light";
		// Adjustment nodes
		static constexpr const char* HsvToRgb = "hsvtorgb";
		static constexpr const char* Luminance = "luminance";
		static constexpr const char* Remap = "remap";
		static constexpr const char* RgbToHsv = "rgbtohsv";
		static constexpr const char* Saturate = "saturate";
		static constexpr const char* Smoothstep = "smoothstep";
		// Application
		static constexpr const char* Time= "time";
		// PBR
		// BSDF
		static constexpr const char* BurleyDiffuseBSDF= "burley_diffuse_bsdf";
		static constexpr const char* ConductorBSDF = "conductor_bsdf";
		static constexpr const char* DielectricBSDF = "dielectric_bsdf";
		static constexpr const char* GeneralizedSchlickBSDF= "generalized_schlick_bsdf";
		static constexpr const char* OrenNayarDiffuseBSDF = "oren_nayar_diffuse_bsdf";
		static constexpr const char* SheenBSDF = "sheen_bsdf";
		static constexpr const char* SubsurfaceBSDF= "subsurface_bsdf";
		static constexpr const char* ThinFilmBSDF= "thin_film_bsdf";
		static constexpr const char* TranslucentBSDF = "translucent_bsdf";
		// EDF
		static constexpr const char* ConicalEDF = "conical_edf";
		static constexpr const char* MeasuredEDF = "measured_edf";
		static constexpr const char* UniformEDF = "uniform_edf";
		// VDF
		static constexpr const char* AbsorptionVDF = "absorption_vdf";
		static constexpr const char* AnisotropicVDF = "anisotropic_vdf";
		// PBR Utility Nodes
		static constexpr const char* ArtisticIOR = "artistic_ior";
		static constexpr const char* Blackbody = "blackbody";
		static constexpr const char* Layer = "layer";
		static constexpr const char* RoughnessAnisotropy = "roughness_anisotropy";
		static constexpr const char* RoughnessDual = "roughness_dual";
		//Surface Shaders
		static constexpr const char* GltfPbr = "gltf_pbr";
		static constexpr const char* DisneyBSDF2012 = "disney_brdf_2012";
		static constexpr const char* DisneyBSDF2015 = "disney_bsdf_2015";
		static constexpr const char* OpenPBRSurface = "open_pbr_surface";
		static constexpr const char* StandardSurface = "standard_surface";
		static constexpr const char* Surface = "surface";
		static constexpr const char* UsdPreviewSurface = "UsdPreviewSurface";
		// Shader
		static constexpr const char* SurfaceUnlit = "surface_unlit";
		// Convolution
		static constexpr const char* Blur = "blur";
		static constexpr const char* HeightToNormal = "heighttonormal";
	}

	namespace NodeDefinition
	{
		static constexpr const char* OpenPBRSurface = "ND_open_pbr_surface_surfaceshader";
		static constexpr const char* StandardSurface = "ND_standard_surface_surfaceshader";
		static constexpr const char* SurfaceUnlit = "ND_surface_unlit";
		static constexpr const char* UsdPreviewSurface = "ND_UsdPreviewSurface_surfaceshader";
		static constexpr const char* PointLight = "ND_point_light";
		static constexpr const char* DirectionalLight = "ND_directional_light";
		static constexpr const char* SpotLight = "ND_spot_light";
		static constexpr const char* Surface = "ND_surface";
	}

	namespace Library
	{
		static constexpr const char* Libraries = "libraries";
	}

	namespace Type
	{
		static constexpr const char* Boolean = "boolean";
		static constexpr const char* Integer = "integer";
		static constexpr const char* Float = "float";
		static constexpr const char* Color3 = "color3";
		static constexpr const char* Color4 = "color4";
		static constexpr const char* Vector2 = "vector2";
		static constexpr const char* Vector3 = "vector3";
		static constexpr const char* Vector4 = "vector4";
		static constexpr const char* Matrix33 = "matrix33";
		static constexpr const char* Matrix44 = "matrix44";
		static constexpr const char* String = "string";
		static constexpr const char* Filename = "filename";
		static constexpr const char* GeomName = "geomname";
		static constexpr const char* SurfaceShader = "surfaceshader";
		static constexpr const char* DisplacementShader = "displacementshader";
		static constexpr const char* VolumeShader = "volumeshader";
		static constexpr const char* LightShader = "lightshader";
		static constexpr const char* Material = "material";
		static constexpr const char* None = "none";
		static constexpr const char* IntegerArray = "integerarray";
		static constexpr const char* FloatArray = "floatarray";
		static constexpr const char* Color3Array = "color3array";
		static constexpr const char* Color4Array = "color4array";
		static constexpr const char* Vector2Array = "vector2array";
		static constexpr const char* Vector3Array = "vector3array";
		static constexpr const char* Vector4Array = "vector4array";
		static constexpr const char* StringArray = "stringarray";
		static constexpr const char* GeomNameArray = "geomnamearray";
	}

	namespace NodeGroup
	{
		namespace Texture2D
		{
			namespace Inputs
			{
				static constexpr const char* File = "file";
				static constexpr const char* Default = "default";
				static constexpr const char* TexCoord = "texcoord";
				static constexpr const char* FilterType = "filtertype";
				static constexpr const char* FrameRange = "framerange";
				static constexpr const char* FrameOffset = "frameoffset";
				static constexpr const char* FrameEndAction = "frameendaction";
			}
		}

		namespace Math
		{
			template<typename T>
			static const T NeutralZero = T{ 0 };

			template<typename T>
			static const T NeutralOne = T{ 1 };
		}

		static constexpr const char* PBR = "pbr";
	}

	namespace Image
	{
		namespace Inputs
		{
			using namespace NodeGroup::Texture2D::Inputs;

			static constexpr const char* Layer = "layer";
			static constexpr const char* UAddressMode = "uaddressmode";
			static constexpr const char* VAddressMode = "vaddressmode";
		}
	}

	namespace TiledImage
	{
		namespace Inputs
		{
			using namespace NodeGroup::Texture2D::Inputs;

			static constexpr const char* UVTiling = "uvtiling";
			static constexpr const char* UVOffset = "uvoffset";
			static constexpr const char* RealWorldImageSize = "realworldimagesize";
			static constexpr const char* RealWorldTileSize = "realworldtilesize";
		}
	}

MATERIALX_NAMESPACE_END
