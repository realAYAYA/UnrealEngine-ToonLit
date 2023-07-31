// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE
{
namespace Interchange
{
	namespace Materials
	{
		namespace Standard
		{
			namespace Nodes
			{
				namespace Add
				{
					const FName Name = TEXT("Add");

					namespace Inputs
					{
						const FName A = TEXT("A");
						const FName B = TEXT("B");
					}
				}

				namespace FlattenNormal
				{
					const FName Name = TEXT("FlattenNormal");

					namespace Inputs
					{
						const FName Normal = TEXT("Normal");
						const FName Flatness = TEXT("Flatness");
					}
				}

				namespace Lerp
				{
					const FName Name = TEXT("Lerp");

					namespace Inputs
					{
						const FName A = TEXT("A"); // Type: linear color
						const FName B = TEXT("B"); // Type: linear color
						const FName Factor = TEXT("Factor"); // Type: float
					}
				}

				namespace Mask
				{
					const FName Name = TEXT("ComponentMask");

					namespace Attributes
					{
						const FName R = TEXT("R");
						const FName G = TEXT("G");
						const FName B = TEXT("B");
						const FName A = TEXT("A");
					}

					namespace Inputs
					{
						const FName Input = TEXT("Input");
					}
				}

				namespace Multiply
				{
					const FName Name = TEXT("Multiply");

					namespace Inputs
					{
						const FName A = TEXT("A");
						const FName B = TEXT("B");
					}
				}

				namespace MakeFloat3
				{
					const FName Name = TEXT("MakeFloat3");
					namespace Inputs
					{
						const FName X = TEXT("X");
						const FName Y = TEXT("Y");
						const FName Z = TEXT("Z");
					}
				}

				namespace OneMinus
				{
					const FName Name = TEXT("OneMinus");

					namespace Inputs
					{
						const FName Input = TEXT("Input");
					}
				}

				namespace TextureCoordinate
				{
					const FName Name = TEXT("TextureCoordinate");

					namespace Inputs
					{
						const FName Index = TEXT("Index"); // Type: int
						const FName UTiling = TEXT("UTiling"); // Type: float
						const FName VTiling = TEXT("VTiling"); // Type: float
						const FName Offset = TEXT("Offset"); // Type: vec2
						const FName Scale = TEXT("Scale"); // Type: vec2
						const FName Rotate = TEXT("Rotate"); // Type: float, Range: 0-1
						const FName RotationCenter = TEXT("RotationCenter"); // Type: vec2
					}
				}

				namespace TextureSample
				{
					const FName Name = TEXT("TextureSample");

					namespace Inputs
					{
						const FName Coordinates = TEXT("Coordinates"); // Type: vec2
						const FName Texture = TEXT("TextureUid"); // Type: FString (unique id of a texture node)
					}

					namespace Outputs
					{
						const FName RGB = TEXT("RGB"); // Type: linear color
						const FName R = TEXT("R"); // Type: float
						const FName G = TEXT("G"); // Type: float
						const FName B = TEXT("B"); // Type: float
						const FName A = TEXT("A"); // Type: float
						const FName RGBA = TEXT("RGBA"); // Type: linear color
					}
				}
			}
		}

		namespace Common
		{
			namespace Parameters
			{
				const FName EmissiveColor = TEXT("EmissiveColor"); // Type: linear color
				const FName Normal = TEXT("Normal"); // Type: vector3f
				const FName Tangent = TEXT("Tangent"); // Type: vector3f
				const FName Opacity = TEXT("Opacity"); // Type: float
				const FName Occlusion = TEXT("Occlusion"); // Type: float
				const FName IndexOfRefraction = TEXT("IOR"); // Type: float
				const FName BxDF = TEXT("BxDF"); // input/output of BSDF or BRDF or BXDF or BTDF data
			}
		}

		namespace Lambert
		{
			namespace Parameters
			{
				using namespace Common::Parameters;

				const FName DiffuseColor = TEXT("DiffuseColor"); // Type: linear color
			}
		}

		namespace Phong
		{
			namespace Parameters
			{
				using namespace Lambert::Parameters;

				const FName SpecularColor = TEXT("SpecularColor"); // Type: linear color
				const FName Shininess = TEXT("Shininess"); // Type: float, this is the specular exponent, expected range: 2-100
			}
		}

		namespace PBR
		{
			namespace Parameters
			{
				using namespace Common::Parameters;

				const FName BaseColor = TEXT("BaseColor"); // Type: vector3
				const FName Metallic = TEXT("Metallic"); // Type: float
				const FName Specular = TEXT("Specular"); // Type: float
				const FName Roughness = TEXT("Roughness"); // Type: float
				const FName Anisotropy = TEXT("Anisotropy"); // Type: float
			}
		}

		namespace ClearCoat
		{
			namespace Parameters
			{
				using namespace PBR::Parameters;

				const FName ClearCoat = TEXT("ClearCoat"); // Type: float
				const FName ClearCoatRoughness = TEXT("ClearCoatRoughness"); // Type: float
				const FName ClearCoatNormal = TEXT("ClearCoatNormal"); // Type: vector3
			}
		}

		namespace ThinTranslucent
		{
			namespace Parameters
			{
				using namespace PBR::Parameters;

				const FName TransmissionColor = TEXT("TransmissionColor"); // Type: vector3
			}
		}

		namespace Sheen
		{
			namespace Parameters
			{
				using namespace PBR::Parameters;

				const FName SheenColor = TEXT("SheenColor"); // Type: vector3
				const FName SheenRoughness = TEXT("SheenRoughness"); // Type: float
			}
		}

		namespace Subsurface
		{
			namespace Parameters
			{
				using namespace PBR::Parameters;

				const FName SubsurfaceColor = TEXT("SubsurfaceColor"); // Type: linear color
			}
		}

		namespace StandardSurface
		{
			const FName Name = TEXT("standard_surface");

			namespace Parameters
			{
				const FName Base = TEXT("base");
				const FName BaseColor = TEXT("base_color");
				const FName DiffuseRoughness = TEXT("diffuse_roughness");
				const FName Metalness = TEXT("metalness");
				const FName Specular = TEXT("specular");
				const FName SpecularColor = TEXT("specular_color");
				const FName SpecularRoughness = TEXT("specular_roughness");
				const FName SpecularIOR = TEXT("specular_IOR");
				const FName SpecularAnisotropy = TEXT("specular_anisotropy");
				const FName SpecularRotation = TEXT("specular_rotation");
				const FName Transmission = TEXT("transmission");
				const FName TransmissionColor = TEXT("transmission_color");
				const FName TransmissionDepth = TEXT("transmission_depth");
				const FName TransmissionScatter = TEXT("transmission_scatter");
				const FName TransmissionScatterAnisotropy = TEXT("transmission_scatter_anisotropy");
				const FName TransmissionDispersion = TEXT("transmission_dispersion");
				const FName TransmissionExtraRoughness = TEXT("transmission_extra_roughness");
				const FName Subsurface = TEXT("subsurface");
				const FName SubsurfaceColor = TEXT("subsurface_color");
				const FName SubsurfaceRadius = TEXT("subsurface_radius");
				const FName SubsurfaceScale = TEXT("subsurface_scale");
				const FName SubsurfaceAnisotropy = TEXT("subsurface_anisotropy");
				const FName Sheen = TEXT("sheen");
				const FName SheenColor = TEXT("sheen_color");
				const FName SheenRoughness = TEXT("sheen_roughness");
				const FName Coat = TEXT("coat");
				const FName CoatColor = TEXT("coat_color");
				const FName CoatRoughness = TEXT("coat_roughness");
				const FName CoatAnisotropy = TEXT("coat_anisotropy");
				const FName CoatRotation = TEXT("coat_rotation");
				const FName CoatIOR = TEXT("coat_IOR");
				const FName CoatNormal = TEXT("coat_normal");
				const FName CoatAffectColor = TEXT("coat_affect_color");
				const FName CoatAffectRoughness = TEXT("coat_affect_roughness");
				const FName ThinFilmThickness = TEXT("thin_film_thickness");
				const FName ThinFilmIOR = TEXT("thin_film_IOR");
				const FName Emission = TEXT("emission");
				const FName EmissionColor = TEXT("emission_color");
				const FName Opacity = TEXT("opacity");
				const FName ThinWalled = TEXT("thin_walled");
				const FName Normal = TEXT("normal");
				const FName Tangent = TEXT("tangent");
			}
		}

		namespace Unlit
		{
			namespace Parameters
			{
				const FName UnlitColor = TEXT("UnlitColor"); // Type: linear color
			}
		}
	}
}
}
