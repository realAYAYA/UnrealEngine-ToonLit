// Copyright Epic Games, Inc. All Rights Reserved. 

#include "MaterialX/InterchangeMaterialXTranslator.h"
#if WITH_EDITOR
#include "MaterialXFormat/Util.h"
#endif
#include "InterchangeImportLog.h"
#include "InterchangeLightNode.h"
#include "InterchangeManager.h"
#include "InterchangeMaterialDefinitions.h"
#include "InterchangeSceneNode.h"
#include "InterchangeMaterialXDefinitions.h"
#include "InterchangeTexture2DNode.h"
#include "Nodes/InterchangeSourceNode.h"
#include "UObject/GCObjectScopeGuard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMaterialXTranslator)

#define LOCTEXT_NAMESPACE "InterchangeMaterialXTranslator"

static bool GInterchangeEnableMaterialXImport = true;
static FAutoConsoleVariableRef CCvarInterchangeEnableMaterialXImport(
	TEXT("Interchange.FeatureFlags.Import.MTLX"),
	GInterchangeEnableMaterialXImport,
	TEXT("Whether MaterialX support is enabled."),
	ECVF_Default);


namespace mx = MaterialX;

UInterchangeMaterialXTranslator::UInterchangeMaterialXTranslator()
#if WITH_EDITOR
	:
InputNamesMaterialX2UE
{
	{{TEXT(""),                         TEXT("bg")},		TEXT("B")},
	{{TEXT(""),                         TEXT("center")},		TEXT("Center")},
	{{TEXT(""),                         TEXT("fg")},		TEXT("A")},
	{{TEXT(""),                         TEXT("high")},		TEXT("Max")},
	{{TEXT(""),                         TEXT("in")},		TEXT("Input")},
	{{TEXT(""),                         TEXT("in1")},		TEXT("A")},
	{{TEXT(""),                         TEXT("in2")},		TEXT("B")},
	{{TEXT(""),                         TEXT("in3")},		TEXT("C")},
	{{TEXT(""),                         TEXT("in4")},		TEXT("D")},
	{{TEXT(""),                         TEXT("inlow")},     TEXT("InputLow")},
	{{TEXT(""),                         TEXT("inhigh")},    TEXT("InputHigh")},
	{{TEXT(""),                         TEXT("low")},		TEXT("Min")},
	{{TEXT(""),                         TEXT("lumacoeffs")},TEXT("LuminanceFactors")}, // for the moment not yet handled by Interchange, because of the attribute being an advanced pin
	{{TEXT(""),                         TEXT("mix")},		TEXT("Factor")},
	{{TEXT(""),                         TEXT("texcoord")},  TEXT("Coordinates")},
	{{TEXT(""),                         TEXT("outlow")},    TEXT("TargetLow")},
	{{TEXT(""),                         TEXT("outhigh")},   TEXT("TargetHigh")},
	{{TEXT(""),                         TEXT("valuel")},    TEXT("A")},
	{{TEXT(""),                         TEXT("valuer")},    TEXT("B")},
	{{MaterialX::Category::Atan2,       TEXT("in1")},       TEXT("Y")},
	{{MaterialX::Category::Atan2,       TEXT("in2")},       TEXT("X")},
	{{MaterialX::Category::IfGreater,   TEXT("value1")},    TEXT("A")},
	{{MaterialX::Category::IfGreater,   TEXT("value2")},    TEXT("B")},
	{{MaterialX::Category::IfGreater,   TEXT("in1")},       TEXT("AGreaterThanB")},
	{{MaterialX::Category::IfGreater,   TEXT("in2")},       TEXT("ALessThanB")}, //another input is added for the case equal see ConnectIfGreater
	{{MaterialX::Category::IfGreaterEq, TEXT("value1")},    TEXT("A")},
	{{MaterialX::Category::IfGreaterEq, TEXT("value2")},    TEXT("B")},
	{{MaterialX::Category::IfGreaterEq, TEXT("in1")},       TEXT("AGreaterThanB")}, //another is added for the case equal see ConnectIfGreaterEq
	{{MaterialX::Category::IfGreaterEq, TEXT("in2")},       TEXT("ALessThanB")},
	{{MaterialX::Category::IfEqual,     TEXT("value1")},    TEXT("A")},
	{{MaterialX::Category::IfEqual,     TEXT("value2")},    TEXT("B")},
	{{MaterialX::Category::IfEqual,     TEXT("in1")},       TEXT("AEqualsB")},
	{{MaterialX::Category::IfEqual,     TEXT("in2")},       TEXT("ALessThanB")}, //another input is added for the case greater see ConnectIfEqual
	{{MaterialX::Category::Invert,      TEXT("amount")},    TEXT("A")},
	{{MaterialX::Category::Invert,      TEXT("in")},        TEXT("B")},
	{{MaterialX::Category::Magnitude,   TEXT("in")},        TEXT("A")},
	{{MaterialX::Category::Mix,         TEXT("fg")},        TEXT("B")},
	{{MaterialX::Category::Mix,         TEXT("bg")},        TEXT("A")},
	{{MaterialX::Category::Normalize,   TEXT("in")},        TEXT("VectorInput")},
	{{MaterialX::Category::Power,       TEXT("in1")},       TEXT("Base")},
	{{MaterialX::Category::Power,	    TEXT("in2")},       TEXT("Exponent")},
	{{MaterialX::Category::Rotate2D,    TEXT("amount")},    TEXT("RotationAngle")},
	{{MaterialX::Category::Rotate2D,    TEXT("in")},        TEXT("Position")},
	{{MaterialX::Category::Rotate3D,    TEXT("amount")},    TEXT("RotationAngle")},
	{{MaterialX::Category::Rotate3D,    TEXT("axis")},		TEXT("NormalizedRotationAxis")},
	{{MaterialX::Category::Rotate3D,    TEXT("in")},        TEXT("Position")},
	{{MaterialX::Category::Saturate,    TEXT("amount")},    TEXT("Fraction")},
	{{MaterialX::Category::Smoothstep,  TEXT("in")},        TEXT("Value")},
},
NodeNamesMaterialX2UE{
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
	{MaterialX::Category::Power,        TEXT("Power")},
	{MaterialX::Category::RampLR,       TEXT("RampLeftRight")},
	{MaterialX::Category::RampTB,       TEXT("RampTopBottom")},
	{MaterialX::Category::Sign,         TEXT("Sign")},
	{MaterialX::Category::Sin,          TEXT("Sine")},
	{MaterialX::Category::SplitLR,      TEXT("SplitLeftRight")},
	{MaterialX::Category::SplitTB,      TEXT("SplitTopBottom")},
	{MaterialX::Category::Sqrt,         TEXT("SquareRoot")},
	{MaterialX::Category::Sub,          TEXT("Subtract")},
	{MaterialX::Category::Tan,          TEXT("Tangent")},
	// Compositing nodes
	{MaterialX::Category::Mix,          TEXT("Lerp")},
	// Channel nodes
	{MaterialX::Category::Combine2,     TEXT("AppendVector")},
	{MaterialX::Category::Combine3,     TEXT("Append3Vector")},
	{MaterialX::Category::Combine4,     TEXT("Append4Vector")},
	// Geometric nodes 
	{MaterialX::Category::TexCoord,		TEXT("TextureCoordinate")},
	// Adjustment nodes,
	{MaterialX::Category::HsvToRgb,		TEXT("HsvToRgb")},
	{MaterialX::Category::Luminance,    TEXT("Luminance")},
	{MaterialX::Category::Remap,        TEXT("Remap")},
	{MaterialX::Category::RgbToHsv,		TEXT("RgbToHsv")},
	{MaterialX::Category::Saturate,		TEXT("Desaturation")},
	{MaterialX::Category::Smoothstep,   TEXT("SmoothStep")}
},
UEInputs
{
    TEXT("A"),
	TEXT("B"),
    TEXT("Base"),
    TEXT("C"),
	TEXT("Center"),
    TEXT("D"),
	TEXT("Exponent"),
    TEXT("Factor"),
	TEXT("Fraction"),
    TEXT("Input"),
	TEXT("InputLow"),
	TEXT("InputHigh"),
	TEXT("LuminanceFactors"),
    TEXT("Max"),
    TEXT("Min"),
	TEXT("TargetLow"),
	TEXT("TargetHigh"),
	TEXT("Value"),
    TEXT("VectorInput"),
    TEXT("X"),
    TEXT("Y")
}
#endif // WITH_EDITOR
{
}

EInterchangeTranslatorType UInterchangeMaterialXTranslator::GetTranslatorType() const
{
	return EInterchangeTranslatorType::Scenes;
}

EInterchangeTranslatorAssetType UInterchangeMaterialXTranslator::GetSupportedAssetTypes() const
{
	return EInterchangeTranslatorAssetType::Materials;
}

TArray<FString> UInterchangeMaterialXTranslator::GetSupportedFormats() const
{
	// Call to UInterchangeMaterialXTranslator::GetSupportedFormats is not supported out of game thread
	// A more global solution must be found for translators which require some initialization
	if(!IsInGameThread() || (!GInterchangeEnableMaterialXImport && !GIsAutomationTesting))
	{
		return TArray<FString>{};
	}

	return TArray<FString>{ TEXT("mtlx;MaterialX File Format") };
}

bool UInterchangeMaterialXTranslator::Translate(UInterchangeBaseNodeContainer& BaseNodeContainer) const
{
	bool bIsDocumentValid = false;
	bool bIsReferencesValid = true;

#if WITH_EDITOR
	FString Filename = GetSourceData()->GetFilename();

	if(!FPaths::FileExists(Filename))
	{
		return false;
	}
	try
	{
		mx::FileSearchPath MaterialXFolder{ TCHAR_TO_UTF8(*FPaths::Combine(
			FPaths::EngineDir(),
			TEXT("Binaries"),
			TEXT("ThirdParty"),
			TEXT("MaterialX"))) };

		mx::DocumentPtr MaterialXLibrary = mx::createDocument();

		mx::StringSet LoadedLibs = mx::loadLibraries({ mx::Library::Libraries }, MaterialXFolder, MaterialXLibrary);
		if(LoadedLibs.empty())
		{
			UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
			Message->Text = FText::Format(LOCTEXT("MaterialXLibrariesNotFound", "Couldn't load MaterialX libraries from {0}"),
										  FText::FromString(MaterialXFolder.asString().c_str()));
			return false;
		}

		mx::DocumentPtr Document = mx::createDocument();
		mx::readFromXmlFile(Document, TCHAR_TO_UTF8(*Filename));
		Document->importLibrary(MaterialXLibrary);

		std::string MaterialXMessage;
		bIsDocumentValid = Document->validate(&MaterialXMessage);
		if(!bIsDocumentValid)
		{
			UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
			Message->Text = FText::Format(LOCTEXT("MaterialXDocumentInvalid", "{0}"),
										  FText::FromString(MaterialXMessage.c_str()));
			return false;
		}

		for(mx::ElementPtr Elem : Document->traverseTree())
		{
			//make sure to read only the current file otherwise we'll process the entire library
			if(Elem->getActiveSourceUri() != Document->getActiveSourceUri())
			{
				continue;
			}

			mx::NodePtr Node = Elem->asA<mx::Node>();

			if(Node)
			{
				bool bIsMaterialShader = Node->getType() == mx::Type::Material;
				bool bIsLightShader = Node->getType() == mx::Type::LightShader;

				if(bIsMaterialShader || bIsLightShader)
				{
					if(!Node->getTypeDef())
					{
						UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
						Message->Text = FText::Format(LOCTEXT("TypeDefNotFound", "<{0}> has no matching TypeDef, aborting import..."),
													  FText::FromString(Node->getName().c_str()));
						bIsReferencesValid = false;
						break;
					}

					//The entry point for materials is only on a surfacematerial node
					if(bIsMaterialShader && Node->getCategory() == mx::SURFACE_MATERIAL_NODE_STRING)
					{
						bool bHasStandardSurface = false;

						for(mx::InputPtr Input : Node->getInputs())
						{
							//we only support standard_surface for now
							mx::NodePtr ConnectedNode = Input->getConnectedNode();

							if(ConnectedNode && ConnectedNode->getCategory() == mx::Category::StandardSurface)
							{
								ProcessStandardSurface(BaseNodeContainer, ConnectedNode, Document);
								bHasStandardSurface = true;
							}
						}

						if(!bHasStandardSurface)
						{
							UInterchangeResultWarning_Generic* Message = AddMessage<UInterchangeResultWarning_Generic>();
							Message->Text = FText::Format(LOCTEXT("StandardSurfaceNotFound", "<{0}> has no standard_surface inputs"),
														  FText::FromString(Node->getName().c_str()));
						}
					}
					else if(bIsLightShader)
					{
						bIsReferencesValid = true;
						ProcessLightShader(BaseNodeContainer, Node, Document);
					}
				}
			}
		}
	}
	catch(std::exception& Exception)
	{
		bIsDocumentValid = false;
		bIsReferencesValid = false;
		UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
		Message->Text = FText::Format(LOCTEXT("MaterialXException", "{0}"),
									  FText::FromString(Exception.what()));
	}

#endif // WITH_EDITOR

	if(bIsDocumentValid && bIsReferencesValid)
	{
		UInterchangeSourceNode* SourceNode = UInterchangeSourceNode::FindOrCreateUniqueInstance(&BaseNodeContainer);
		SourceNode->SetCustomImportUnusedMaterial(true);
	}

	return bIsDocumentValid && bIsReferencesValid;
}

TOptional<UE::Interchange::FImportImage> UInterchangeMaterialXTranslator::GetTexturePayloadData(const UInterchangeSourceData* InSourceData, const FString& PayloadKey) const
{
	UInterchangeSourceData* PayloadSourceData = UInterchangeManager::GetInterchangeManager().CreateSourceData(PayloadKey);
	FGCObjectScopeGuard ScopedSourceData(PayloadSourceData);

	if(!PayloadSourceData)
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	UInterchangeTranslatorBase* SourceTranslator = UInterchangeManager::GetInterchangeManager().GetTranslatorForSourceData(PayloadSourceData);
	FGCObjectScopeGuard ScopedSourceTranslator(SourceTranslator);
	const IInterchangeTexturePayloadInterface* TextureTranslator = Cast<IInterchangeTexturePayloadInterface>(SourceTranslator);

	if(!ensure(TextureTranslator))
	{
		return TOptional<UE::Interchange::FImportImage>();
	}

	return TextureTranslator->GetTexturePayloadData(PayloadSourceData, PayloadKey);
}

#if WITH_EDITOR
void UInterchangeMaterialXTranslator::ProcessStandardSurface(UInterchangeBaseNodeContainer& NodeContainer, MaterialX::NodePtr StandardSurfaceNode, MaterialX::DocumentPtr Document) const
{
	using namespace UE::Interchange::Materials;

	TMap<FString, UInterchangeShaderNode*> NamesToShaderNodes;

	UInterchangeShaderGraphNode* ShaderGraphNode = CreateShaderNode<UInterchangeShaderGraphNode>(StandardSurfaceNode->getName().c_str(), StandardSurface::Name.ToString(), NamesToShaderNodes, NodeContainer);

	//Base
	{
		//Weight
		{
			mx::InputPtr InputBase = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Base, Document);

			if(!ConnectNodeGraphOutputToInput(InputBase, ShaderGraphNode, StandardSurface::Parameters::Base.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputBase, StandardSurface::Parameters::Base.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Base);
			}
		}

		//Color
		{
			mx::InputPtr InputBaseColor = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::BaseColor, Document);

			if(!ConnectNodeGraphOutputToInput(InputBaseColor, ShaderGraphNode, StandardSurface::Parameters::BaseColor.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputBaseColor, StandardSurface::Parameters::BaseColor.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::BaseColor);
			}
		}
	}

	//DiffuseRoughness
	{
		mx::InputPtr InputDiffuseRoughness = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::DiffuseRoughness, Document);

		if(!ConnectNodeGraphOutputToInput(InputDiffuseRoughness, ShaderGraphNode, StandardSurface::Parameters::DiffuseRoughness.ToString(), NamesToShaderNodes, NodeContainer))
		{
			AddFloatAttribute(InputDiffuseRoughness, StandardSurface::Parameters::DiffuseRoughness.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::DiffuseRoughness);
		}
	}

	//Specular
	{
		//Weight
		{
			mx::InputPtr InputSpecular = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Specular, Document);

			if(!ConnectNodeGraphOutputToInput(InputSpecular, ShaderGraphNode, StandardSurface::Parameters::Specular.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSpecular, StandardSurface::Parameters::Specular.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Specular);
			}
		}

		//Roughness
		{
			mx::InputPtr InputSpecularRoughness = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SpecularRoughness, Document);

			if(!ConnectNodeGraphOutputToInput(InputSpecularRoughness, ShaderGraphNode, StandardSurface::Parameters::SpecularRoughness.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSpecularRoughness, StandardSurface::Parameters::SpecularRoughness.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::SpecularRoughness);
			}
		}

		//IOR
		{
			mx::InputPtr InputSpecularIOR = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SpecularIOR, Document);

			if(!ConnectNodeGraphOutputToInput(InputSpecularIOR, ShaderGraphNode, StandardSurface::Parameters::SpecularIOR.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSpecularIOR, StandardSurface::Parameters::SpecularIOR.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::SpecularIOR);
			}
		}

		//Anisotropy
		{
			mx::InputPtr InputSpecularAnisotropy = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SpecularAnisotropy, Document);

			if(!ConnectNodeGraphOutputToInput(InputSpecularAnisotropy, ShaderGraphNode, StandardSurface::Parameters::SpecularAnisotropy.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSpecularAnisotropy, StandardSurface::Parameters::SpecularAnisotropy.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::SpecularAnisotropy);
			}
		}

		//Rotation
		{
			mx::InputPtr InputSpecularRotation = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SpecularRotation, Document);

			if(!ConnectNodeGraphOutputToInput(InputSpecularRotation, ShaderGraphNode, StandardSurface::Parameters::SpecularRotation.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSpecularRotation, StandardSurface::Parameters::SpecularRotation.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::SpecularRotation);
			}
		}
	}

	//Metallic
	{
		mx::InputPtr InputMetalness = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Metalness, Document);

		if(!ConnectNodeGraphOutputToInput(InputMetalness, ShaderGraphNode, StandardSurface::Parameters::Metalness.ToString(), NamesToShaderNodes, NodeContainer))
		{
			AddFloatAttribute(InputMetalness, StandardSurface::Parameters::Metalness.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Metalness);
		}
	}

	//Subsurface
	{
		//Weight
		{
			mx::InputPtr InputSubsurface = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Subsurface, Document);

			if(!ConnectNodeGraphOutputToInput(InputSubsurface, ShaderGraphNode, StandardSurface::Parameters::Subsurface.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSubsurface, StandardSurface::Parameters::Subsurface.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Subsurface);
			}
		}

		//Color
		{
			mx::InputPtr InputSubsurfaceColor = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SubsurfaceColor, Document);

			if(!ConnectNodeGraphOutputToInput(InputSubsurfaceColor, ShaderGraphNode, StandardSurface::Parameters::SubsurfaceColor.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputSubsurfaceColor, StandardSurface::Parameters::SubsurfaceColor.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::SubsurfaceColor);
			}
		}

		//Radius
		{
			mx::InputPtr InputSubsurfaceRadius = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SubsurfaceRadius, Document);

			if(!ConnectNodeGraphOutputToInput(InputSubsurfaceRadius, ShaderGraphNode, StandardSurface::Parameters::SubsurfaceRadius.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputSubsurfaceRadius, StandardSurface::Parameters::SubsurfaceRadius.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::SubsurfaceRadius);
			}
		}

		//Scale
		{
			mx::InputPtr InputSubsurfaceScale = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SubsurfaceScale, Document);

			if(!ConnectNodeGraphOutputToInput(InputSubsurfaceScale, ShaderGraphNode, StandardSurface::Parameters::SubsurfaceScale.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSubsurfaceScale, StandardSurface::Parameters::SubsurfaceScale.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::SubsurfaceScale);
			}
		}
	}

	//Sheen
	{
		//Weight
		{
			mx::InputPtr InputSheen = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Sheen, Document);

			if(!ConnectNodeGraphOutputToInput(InputSheen, ShaderGraphNode, StandardSurface::Parameters::Sheen.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSheen, StandardSurface::Parameters::Sheen.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Sheen);
			}
		}

		//Color
		{
			mx::InputPtr InputSheenColor = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SheenColor, Document);

			if(!ConnectNodeGraphOutputToInput(InputSheenColor, ShaderGraphNode, StandardSurface::Parameters::SubsurfaceColor.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputSheenColor, StandardSurface::Parameters::SheenColor.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::SheenColor);
			}
		}

		//Roughness
		{
			mx::InputPtr InputSheenRoughness = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::SheenRoughness, Document);

			if(!ConnectNodeGraphOutputToInput(InputSheenRoughness, ShaderGraphNode, StandardSurface::Parameters::SheenRoughness.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputSheenRoughness, StandardSurface::Parameters::SheenRoughness.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::SheenRoughness);
			}
		}
	}

	//Coat
	{
		//Weight
		{
			mx::InputPtr InputCoat = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Coat, Document);

			if(!ConnectNodeGraphOutputToInput(InputCoat, ShaderGraphNode, StandardSurface::Parameters::Coat.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputCoat, StandardSurface::Parameters::Coat.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Coat);
			}
		}

		//Color
		{
			mx::InputPtr InputCoatColor = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::CoatColor, Document);

			if(!ConnectNodeGraphOutputToInput(InputCoatColor, ShaderGraphNode, StandardSurface::Parameters::CoatColor.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputCoatColor, StandardSurface::Parameters::CoatColor.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::CoatColor);
			}
		}

		//Roughness
		{
			mx::InputPtr InputCoatRoughness = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::CoatRoughness, Document);

			if(!ConnectNodeGraphOutputToInput(InputCoatRoughness, ShaderGraphNode, StandardSurface::Parameters::CoatRoughness.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputCoatRoughness, StandardSurface::Parameters::CoatRoughness.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::CoatRoughness);
			}
		}

		//Normal
		{
			//No need to take the default input if there is no Normal input
			mx::InputPtr InputCoatNormal = StandardSurfaceNode->getInput(mx::StandardSurface::Input::CoatNormal);

			if(InputCoatNormal)
			{
				ConnectNodeGraphOutputToInput(InputCoatNormal, ShaderGraphNode, StandardSurface::Parameters::CoatNormal.ToString(), NamesToShaderNodes, NodeContainer);
			}
		}
	}

	//ThinFilmThickness
	{
		mx::InputPtr InputThinFilmThickness = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::ThinFilmThickness, Document);

		if(!ConnectNodeGraphOutputToInput(InputThinFilmThickness, ShaderGraphNode, StandardSurface::Parameters::ThinFilmThickness.ToString(), NamesToShaderNodes, NodeContainer))
		{
			AddFloatAttribute(InputThinFilmThickness, StandardSurface::Parameters::ThinFilmThickness.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::ThinFilmThickness);
		}
	}

	//Emission
	{
		//Weight
		{
			mx::InputPtr InputEmission = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Emission, Document);

			if(!ConnectNodeGraphOutputToInput(InputEmission, ShaderGraphNode, StandardSurface::Parameters::Emission.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputEmission, StandardSurface::Parameters::Emission.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Emission);
			}
		}

		//Color
		{
			mx::InputPtr InputEmissionColor = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::EmissionColor, Document);

			if(!ConnectNodeGraphOutputToInput(InputEmissionColor, ShaderGraphNode, StandardSurface::Parameters::EmissionColor.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputEmissionColor, StandardSurface::Parameters::EmissionColor.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::EmissionColor);
			}
		}
	}

	//Normal
	{
		//No need to take the default input if there is no Normal input
		mx::InputPtr InputNormal = StandardSurfaceNode->getInput(mx::StandardSurface::Input::Normal);

		if(InputNormal)
		{
			ConnectNodeGraphOutputToInput(InputNormal, ShaderGraphNode, StandardSurface::Parameters::Normal.ToString(), NamesToShaderNodes, NodeContainer);
		}
	}

	//Tangent
	{
		//No need to take the default input if there is no Tangent input
		mx::InputPtr InputTangent = StandardSurfaceNode->getInput(mx::StandardSurface::Input::Tangent);

		if(InputTangent)
		{
			ConnectNodeGraphOutputToInput(InputTangent, ShaderGraphNode, StandardSurface::Parameters::Tangent.ToString(), NamesToShaderNodes, NodeContainer);
		}
	}

	//Transmission
	{
		//Weight
		{
			mx::InputPtr InputTransmission = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::Transmission, Document);

			if (!ConnectNodeGraphOutputToInput(InputTransmission, ShaderGraphNode, StandardSurface::Parameters::Transmission.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputTransmission, StandardSurface::Parameters::Transmission.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::Transmission);
			}
		}

		//Color
		{
			mx::InputPtr InputTransmissionColor = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::TransmissionColor, Document);

			if (!ConnectNodeGraphOutputToInput(InputTransmissionColor, ShaderGraphNode, StandardSurface::Parameters::TransmissionColor.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputTransmissionColor, StandardSurface::Parameters::TransmissionColor.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::TransmissionColor);
			}
		}

		//Depth
		{
			mx::InputPtr InputTransmissionDepth = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::TransmissionDepth, Document);

			if (!ConnectNodeGraphOutputToInput(InputTransmissionDepth, ShaderGraphNode, StandardSurface::Parameters::TransmissionDepth.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputTransmissionDepth, StandardSurface::Parameters::TransmissionDepth.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::TransmissionDepth);
			}
		}

		//Scatter
		{
			mx::InputPtr InputTransmissionScatter = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::TransmissionScatter, Document);

			if (!ConnectNodeGraphOutputToInput(InputTransmissionScatter, ShaderGraphNode, StandardSurface::Parameters::TransmissionScatter.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddLinearColorAttribute(InputTransmissionScatter, StandardSurface::Parameters::TransmissionScatter.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Color3::TransmissionScatter);
			}
		}

		//Scatter Anisotropy
		{
			mx::InputPtr InputTransmissionScatterAnisotropy = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::TransmissionScatterAnisotropy, Document);

			if (!ConnectNodeGraphOutputToInput(InputTransmissionScatterAnisotropy, ShaderGraphNode, StandardSurface::Parameters::TransmissionScatterAnisotropy.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputTransmissionScatterAnisotropy, StandardSurface::Parameters::TransmissionScatterAnisotropy.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::TransmissionScatterAnisotropy);
			}
		}

		//Dispersion
		{
			mx::InputPtr InputTransmissionDispersion = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::TransmissionDispersion, Document);

			if (!ConnectNodeGraphOutputToInput(InputTransmissionDispersion, ShaderGraphNode, StandardSurface::Parameters::TransmissionDispersion.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputTransmissionDispersion, StandardSurface::Parameters::TransmissionDispersion.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::TransmissionDispersion);
			}
		}

		//Extra Roughness
		{
			mx::InputPtr InputTransmissionExtraRoughness = GetStandardSurfaceInput(StandardSurfaceNode, mx::StandardSurface::Input::TransmissionExtraRoughness, Document);

			if (!ConnectNodeGraphOutputToInput(InputTransmissionExtraRoughness, ShaderGraphNode, StandardSurface::Parameters::TransmissionExtraRoughness.ToString(), NamesToShaderNodes, NodeContainer))
			{
				AddFloatAttribute(InputTransmissionExtraRoughness, StandardSurface::Parameters::TransmissionExtraRoughness.ToString(), ShaderGraphNode, mx::StandardSurface::DefaultValue::Float::TransmissionExtraRoughness);
			}
		}
	}
}

void UInterchangeMaterialXTranslator::ProcessLightShader(UInterchangeBaseNodeContainer& NodeContainer, MaterialX::NodePtr LightShaderNode, MaterialX::DocumentPtr Document) const
{
	const FString FileName{ FPaths::GetBaseFilename(LightShaderNode->getActiveSourceUri().c_str()) };
	const FString LightNodeLabel = FString(LightShaderNode->getName().c_str());
	UInterchangeSceneNode* SceneNode = NewObject<UInterchangeSceneNode>(&NodeContainer);
	const FString SceneNodeUID = TEXT("\\Light\\") + FileName + TEXT("\\") + FString(LightShaderNode->getName().c_str());
	SceneNode->InitializeNode(SceneNodeUID, LightNodeLabel, EInterchangeNodeContainerType::TranslatedScene);
	NodeContainer.AddNode(SceneNode);

	UInterchangeBaseLightNode* LightNode = nullptr;
	if(LightShaderNode->getCategory() == mx::Category::PointLight)
	{
		LightNode = CreatePointLightNode(LightShaderNode, SceneNode, NodeContainer, Document);
	}
	else if(LightShaderNode->getCategory() == mx::Category::DirectionalLight)
	{
		LightNode = CreateDirectionalLightNode(LightShaderNode, SceneNode, NodeContainer, Document);
	}
	else if(LightShaderNode->getCategory() == mx::Category::SpotLight)
	{
		LightNode = CreateSpotLightNode(LightShaderNode, SceneNode, NodeContainer, Document);
	}
	else // MaterialX has no standardized lights, these 3 are the most common ones though and serves as an example in the format
	{
		LightNode = CreatePointLightNode(LightShaderNode, SceneNode, NodeContainer, Document);
	}

	const FString LightNodeUid = TEXT("\\Light\\") + LightNodeLabel;
	LightNode->InitializeNode(LightNodeUid, LightNodeLabel, EInterchangeNodeContainerType::TranslatedAsset);
	NodeContainer.AddNode(LightNode);
	const FString Uid = LightNode->GetUniqueID();
	SceneNode->SetCustomAssetInstanceUid(Uid);

	// Color
	{
		mx::InputPtr LightColor = LightShaderNode->getInput(mx::Lights::Input::Color);
		const FLinearColor Color = GetLinearColor(LightColor);
		LightNode->SetCustomLightColor(Color);
	}

	// Intensity
	{
		mx::InputPtr LightIntensity = LightShaderNode->getInput(mx::Lights::Input::Intensity);
		LightNode->SetCustomIntensity(mx::fromValueString<float>(LightIntensity->getValueString()));
	}
}

static void GetLightDirection(mx::InputPtr DirectionInput, FTransform& Transform)
{
	mx::Vector3 Direction = mx::fromValueString<mx::Vector3>(DirectionInput->getValueString());

	//Get rotation to go from UE's default direction of directional light and the direction of the MX directional light node
	const FVector LightDirection{ Direction[2], Direction[0], Direction[1] };
	const FVector TransformDirection = Transform.GetUnitAxis(EAxis::X); // it's the default direction of a UE directional light
	Transform.SetRotation(FQuat::FindBetween(LightDirection, TransformDirection));
}

static void GetLightPosition(mx::InputPtr PositionInput, FTransform& Transform)
{
	const mx::Vector3 Position = mx::fromValueString<mx::Vector3>(PositionInput->getValueString());
	Transform.SetLocation(FVector{ Position[0] * 100, Position[1] * 100, Position[2] * 100 });
}

UInterchangeBaseLightNode* UInterchangeMaterialXTranslator::CreateDirectionalLightNode(MaterialX::NodePtr DirectionalLightShaderNode, UInterchangeSceneNode* SceneNode, UInterchangeBaseNodeContainer& NodeContainer, MaterialX::DocumentPtr Document) const
{
	UInterchangeDirectionalLightNode* LightNode = NewObject<UInterchangeDirectionalLightNode>(&NodeContainer);

	// Direction
	{
		mx::InputPtr DirectionInput = GetDirectionalLightInput(DirectionalLightShaderNode, mx::Lights::DirectionalLight::Input::Direction, Document);

		FTransform Transform;
		GetLightDirection(DirectionInput, Transform);

		SceneNode->SetCustomLocalTransform(&NodeContainer, Transform);
	}

	return LightNode;
}

UInterchangeBaseLightNode* UInterchangeMaterialXTranslator::CreatePointLightNode(MaterialX::NodePtr PointLightShaderNode, UInterchangeSceneNode* SceneNode, UInterchangeBaseNodeContainer& NodeContainer, MaterialX::DocumentPtr Document) const
{
	UInterchangePointLightNode* LightNode = NewObject<UInterchangePointLightNode>(&NodeContainer);
	LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Candelas);

	// Decay rate
	{
		mx::InputPtr DecayRateInput = GetPointLightInput(PointLightShaderNode, mx::Lights::PointLight::Input::DecayRate, Document);
		const float DecayRate = mx::fromValueString<float>(DecayRateInput->getValueString());
		LightNode->SetCustomUseInverseSquaredFalloff(false);
		LightNode->SetCustomLightFalloffExponent(DecayRate);
	}

	// Position
	{
		mx::InputPtr PositionInput = GetPointLightInput(PointLightShaderNode, mx::Lights::PointLight::Input::Position, Document);
		FTransform Transform;
		GetLightPosition(PositionInput, Transform);
		SceneNode->SetCustomLocalTransform(&NodeContainer, Transform);
	}

	return LightNode;
}

UInterchangeBaseLightNode* UInterchangeMaterialXTranslator::CreateSpotLightNode(MaterialX::NodePtr SpotLightShaderNode, UInterchangeSceneNode* SceneNode, UInterchangeBaseNodeContainer& NodeContainer, MaterialX::DocumentPtr Document) const
{
	UInterchangeSpotLightNode* LightNode = NewObject<UInterchangeSpotLightNode>(&NodeContainer);

	LightNode->SetCustomIntensityUnits(EInterchangeLightUnits::Candelas);

	// Decay rate
	{
		mx::InputPtr DecayRateInput = GetSpotLightInput(SpotLightShaderNode, mx::Lights::SpotLight::Input::DecayRate, Document);
		const float DecayRate = mx::fromValueString<float>(DecayRateInput->getValueString());
		LightNode->SetCustomUseInverseSquaredFalloff(false);
		LightNode->SetCustomLightFalloffExponent(DecayRate);
	}

	{
		FTransform Transform;
		// Position
		{
			mx::InputPtr PositionInput = GetSpotLightInput(SpotLightShaderNode, mx::Lights::SpotLight::Input::Position, Document);
			GetLightPosition(PositionInput, Transform);
		}

		// Direction
		{
			mx::InputPtr DirectionInput = GetSpotLightInput(SpotLightShaderNode, mx::Lights::DirectionalLight::Input::Direction, Document);
			GetLightDirection(DirectionInput, Transform);
		}
		SceneNode->SetCustomLocalTransform(&NodeContainer, Transform);
	}

	// Inner angle
	{
		mx::InputPtr InnerAngleInput = GetSpotLightInput(SpotLightShaderNode, mx::Lights::SpotLight::Input::InnerAngle, Document);
		const float InnerAngle = FMath::RadiansToDegrees(mx::fromValueString<float>(InnerAngleInput->getValueString()));
		LightNode->SetCustomInnerConeAngle(InnerAngle);
	}

	// Outer angle
	{
		mx::InputPtr OuterAngleInput = GetSpotLightInput(SpotLightShaderNode, mx::Lights::SpotLight::Input::OuterAngle, Document);
		const float OuterAngle = FMath::RadiansToDegrees(mx::fromValueString<float>(OuterAngleInput->getValueString()));
		LightNode->SetCustomOuterConeAngle(OuterAngle);
	}

	return LightNode;
}

bool UInterchangeMaterialXTranslator::ConnectNodeGraphOutputToInput(MaterialX::InputPtr InputToNodeGraph, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	bool bHasNodeGraph = false;

	if(InputToNodeGraph->hasNodeGraphString())
	{
		bHasNodeGraph = true;

		mx::OutputPtr Output = InputToNodeGraph->getConnectedOutput();

		if(!Output)
		{
			UInterchangeResultWarning_Generic* Message = AddMessage<UInterchangeResultWarning_Generic>();
			Message->Text = FText::Format(LOCTEXT("OutputNotFound", "Couldn't find a connected output to ({0})"),
										  FText::FromString(GetInputName(InputToNodeGraph)));
			return false;
		}
	
		//extract and separate nodes will be handled by ourselves, since they are defined with a swizzle node, which is not present in UE
		auto FilterNodes = [](mx::NodePtr Node) ->bool
		{
			const std::string& Category= Node->getCategory();
			return
				Category != mx::Category::Extract &&
				Category != mx::Category::Saturate &&
				Category != mx::Category::Separate2 &&
				Category != mx::Category::Separate3 &&
				Category != mx::Category::Separate4;
		};

		mx::NodeGraphPtr NodeGraph = Output->getParent()->asA<mx::NodeGraph>();

		AddTexCoordToTiledImageNodes(NodeGraph);

		//This function will replace all the nodes that are defined with a nodegraph with their equivalent node network
		//This allows us to handle nodes defined with a nodegraph, with nodes that we already support while traversing the current node graph
		//For example the node tiledimage is just a succession of the nodes image, multiply, add, etc.
		NodeGraph->flattenSubgraphs(mx::EMPTY_STRING, FilterNodes);

		ConvertNeutralNodesToDot(NodeGraph);

		for(mx::Edge Edge : Output->traverseGraph())
		{
			if(mx::NodePtr UpstreamNode = Edge.getUpstreamElement()->asA<mx::Node>())
			{
				UInterchangeShaderNode* ParentShaderNode = ShaderNode;
				FString InputChannelName = ParentInputName;

				//Replace the input's name by the one used in UE
				RenameNodeInputs(UpstreamNode);

				if(mx::NodePtr DownstreamNode = Edge.getDownstreamElement()->asA<mx::Node>())
				{
					if(UInterchangeShaderNode** FoundNode = NamesToShaderNodes.Find(DownstreamNode->getName().c_str()))
					{
						ParentShaderNode = *FoundNode;
					}

					if(mx::InputPtr ConnectedInput = Edge.getConnectingElement()->asA<mx::Input>())
					{
						InputChannelName = GetInputName(ConnectedInput);
					}
				}

				if(ConnectNodeOutputToInput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer))
				{
					continue;
				}
				else if(UpstreamNode->getCategory() == mx::Category::Constant)
				{
					ConnectConstantInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::Extract)
				{
					ConnectExtractInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				//dot means identity, input == output
				else if(UpstreamNode->getCategory() == mx::Category::Dot || UpstreamNode->getCategory() == mx::Category::NormalMap)
				{
					ConnectDotInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::TransformPoint)
				{
					ConnectTransformPositionInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::TransformVector || UpstreamNode->getCategory() == mx::Category::TransformNormal)
				{
					ConnectTransformVectorInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::Rotate2D)
				{
					ConnectRotate2DInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::Rotate3D)
				{
					ConnectRotate3DInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::Image)
				{
					ConnectImageInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::Convert)
				{
					ConnectConvertInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::IfGreater)
				{
					ConnectIfGreaterInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::IfGreaterEq)
				{
					ConnectIfGreaterEqInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else if(UpstreamNode->getCategory() == mx::Category::IfEqual)
				{
					ConnectIfEqualInputToOutput(UpstreamNode, ParentShaderNode, InputChannelName, NamesToShaderNodes, NodeContainer);
				}
				else
				{
					UInterchangeResultWarning_Generic* Message = AddMessage<UInterchangeResultWarning_Generic>();
					Message->Text = FText::Format(LOCTEXT("NodeCategoryNotSupported", "<{0}> is not supported yet"),
												  FText::FromString(UpstreamNode->getCategory().c_str()));
				}
			}
		}
	}

	return bHasNodeGraph;
}

bool UInterchangeMaterialXTranslator::ConnectNodeOutputToInput(MaterialX::NodePtr Node, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	bool bIsConnected = false;

	if(const FString* ShaderType = NodeNamesMaterialX2UE.Find(Node->getCategory().c_str()))
	{
		UInterchangeShaderNode* OperatorNode = nullptr;

		OperatorNode = CreateShaderNode<UInterchangeShaderNode>(Node->getName().c_str(), *ShaderType, NamesToShaderNodes, NodeContainer);

		for(mx::InputPtr Input : Node->getInputs())
		{
			if(const FString* InputNameFound = UEInputs.Find(GetInputName(Input)))
			{
				AddAttributeFromValueOrInterface(Input, *InputNameFound, OperatorNode);
			}
		}

		bIsConnected = UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, OperatorNode->GetUniqueID());
	}

	return bIsConnected;
}

UInterchangeTextureNode* UInterchangeMaterialXTranslator::CreateTextureNode(MaterialX::NodePtr Node, UInterchangeBaseNodeContainer& NodeContainer) const
{
	UInterchangeTextureNode* TextureNode = nullptr;

	//A node image should have an input file otherwise the user should check its default value
	if(Node)
	{
		if(mx::InputPtr InputFile = Node->getInput("file"); InputFile && InputFile->hasValue())
		{
			FString Filepath{ InputFile->getValueString().c_str() };
			const FString FilePrefix = GetFilePrefix(InputFile);
			Filepath = FPaths::Combine(FilePrefix, Filepath);
			const FString Filename = FPaths::GetCleanFilename(Filepath);
			const FString TextureNodeUID = TEXT("\\Texture\\") + Filename;

			//Only add the TextureNode once
			TextureNode = const_cast<UInterchangeTextureNode*>(Cast<UInterchangeTextureNode>(NodeContainer.GetNode(TextureNodeUID)));
			if(TextureNode == nullptr)
			{
				TextureNode = NewObject<UInterchangeTexture2DNode>(&NodeContainer);
				TextureNode->InitializeNode(TextureNodeUID, Filename, EInterchangeNodeContainerType::TranslatedAsset);
				NodeContainer.AddNode(TextureNode);

				if(FPaths::IsRelative(Filepath))
				{
					Filepath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(Node->getActiveSourceUri().c_str()), Filepath);
				}

				TextureNode->SetPayLoadKey(Filepath);

				const FString ColorSpace = GetColorSpace(InputFile);
				const bool bIsSRGB = ColorSpace == TEXT("srgb_texture");
				TextureNode->SetCustomSRGB(bIsSRGB);
			}
		}
	}

	return TextureNode;
}

const FString& UInterchangeMaterialXTranslator::GetMatchedInputName(MaterialX::NodePtr Node, MaterialX::InputPtr Input) const
{
	static FString EmptyString;

	if(Input)
	{
		const FString NodeCategory{ Node->getCategory().c_str() };
		const FString InputName{ GetInputName(Input) };
		TPair<FString, FString> Pair{ NodeCategory, InputName };
		if(const FString* Result = InputNamesMaterialX2UE.Find(Pair))
		{
			return *Result;
		}
		else if((Result = InputNamesMaterialX2UE.Find({ EmptyString, InputName })))
		{
			return *Result;
		}
	}

	return EmptyString;
}

void UInterchangeMaterialXTranslator::RenameNodeInputs(MaterialX::NodePtr Node) const
{
	if(Node)
	{
		if(const std::string& IsVisited = Node->getAttribute(mx::Attributes::IsVisited); IsVisited.empty())
		{
			Node->setAttribute(mx::Attributes::IsVisited, "true");

			for(mx::InputPtr Input : Node->getInputs())
			{
				if(const FString& Name = GetMatchedInputName(Node, Input); !Name.IsEmpty())
				{
					RenameInput(Input, TCHAR_TO_UTF8(*Name));
				}
			}
		}
	}
}

void UInterchangeMaterialXTranslator::RenameInput(MaterialX::InputPtr Input, const char* NewName) const
{
	Input->setAttribute(mx::Attributes::NewName, NewName);
}

FString UInterchangeMaterialXTranslator::GetInputName(MaterialX::InputPtr Input) const
{
	if(Input->hasAttribute(mx::Attributes::NewName))
	{
		return Input->getAttribute(mx::Attributes::NewName).c_str();
	}
	else
	{
		return Input->getName().c_str();
	}
}

MaterialX::InputPtr UInterchangeMaterialXTranslator::GetStandardSurfaceInput(MaterialX::NodePtr StandardSurface, const char* InputName, MaterialX::DocumentPtr Document) const
{
	mx::InputPtr Input = StandardSurface->getInput(InputName);

	if(!Input)
	{
		Input = Document->getNodeDef(mx::NodeDefinition::StandardSurface)->getActiveInput(InputName);
	}

	return Input;
}

MaterialX::InputPtr UInterchangeMaterialXTranslator::GetPointLightInput(MaterialX::NodePtr PointLight, const char* InputName, MaterialX::DocumentPtr Document) const
{
	mx::InputPtr Input = PointLight->getInput(InputName);

	if(!Input)
	{
		Input = Document->getNodeDef(mx::NodeDefinition::PointLight)->getActiveInput(InputName);
	}

	return Input;
}

MaterialX::InputPtr UInterchangeMaterialXTranslator::GetDirectionalLightInput(MaterialX::NodePtr DirectionalLight, const char* InputName, MaterialX::DocumentPtr Document) const
{
	mx::InputPtr Input = DirectionalLight->getInput(InputName);

	if(!Input)
	{
		Input = Document->getNodeDef(mx::NodeDefinition::DirectionalLight)->getActiveInput(InputName);
	}

	return Input;
}

MaterialX::InputPtr UInterchangeMaterialXTranslator::GetSpotLightInput(MaterialX::NodePtr SpotLight, const char* InputName, MaterialX::DocumentPtr Document) const
{
	mx::InputPtr Input = SpotLight->getInput(InputName);

	if(!Input)
	{
		Input = Document->getNodeDef(mx::NodeDefinition::SpotLight)->getActiveInput(InputName);
	}

	return Input;
}

bool UInterchangeMaterialXTranslator::AddAttributeFromValueOrInterface(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode) const
{
	if(Input)
	{
		if(Input->hasValue())
		{
			return AddAttribute(Input, InputChannelName, ShaderNode);
		}
		else if(Input->hasInterfaceName())
		{
			if(mx::InputPtr InputInterface = Input->getInterfaceInput(); InputInterface->hasValue())
			{
				return AddAttribute(InputInterface, InputChannelName, ShaderNode);
			}
		}
	}

	return false;
}

bool UInterchangeMaterialXTranslator::AddAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode) const
{
	if(Input)
	{
		if(Input->getType() == mx::Type::Float)
		{
			return ShaderNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), mx::fromValueString<float>(Input->getValueString()));
		}

		if(Input->getType() == mx::Type::Color3 || Input->getType() == mx::Type::Color4)
		{
			FLinearColor LinearColor = GetLinearColor(Input);
			return ShaderNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), LinearColor);
		}
		else if(Input->getType() == mx::Type::Vector2)
		{
			FLinearColor Vector = GetVector(Input);
			return ShaderNode->AddVector2Attribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), FVector2f{ Vector.R, Vector.G });
		}
		else if(Input->getType() == mx::Type::Vector3 || Input->getType() == mx::Type::Vector4)
		{
			FLinearColor Vector = GetVector(Input);
			return ShaderNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), Vector);
		}
	}

	return false;
}

bool UInterchangeMaterialXTranslator::AddFloatAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, float DefaultValue) const
{
	if(Input)
	{
		if(Input->hasValue())
		{
			float Value = mx::fromValueString<float>(Input->getValueString());

			if(!FMath::IsNearlyEqual(Value, DefaultValue))
			{
				return ShaderNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), Value);
			}
		}
	}

	return false;
}

bool UInterchangeMaterialXTranslator::AddLinearColorAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FLinearColor& DefaultValue) const
{
	if(Input)
	{
		if(Input->hasValue())
		{
			const FLinearColor Value = GetLinearColor(Input);

			if(!Value.Equals(DefaultValue))
			{
				return ShaderNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), Value);
			}
		}
	}

	return false;
}

FString UInterchangeMaterialXTranslator::GetFilePrefix(MaterialX::ElementPtr Element) const
{
	FString FilePrefix;

	if(Element)
	{
		if(Element->hasFilePrefix())
		{
			return FString(Element->getFilePrefix().c_str());
		}
		else
		{
			return GetFilePrefix(Element->getParent());
		}
	}

	return FilePrefix;
}

FString UInterchangeMaterialXTranslator::GetColorSpace(MaterialX::ElementPtr Element) const
{
	FString ColorSpace;

	if(Element)
	{
		if(Element->hasColorSpace())
		{
			return FString(Element->getColorSpace().c_str());
		}
		else
		{
			return GetColorSpace(Element->getParent());
		}
	}

	return ColorSpace;
}

FLinearColor UInterchangeMaterialXTranslator::GetLinearColor(MaterialX::InputPtr Input) const
{
	//we assume that the default color space is linear
	FLinearColor LinearColor;

	if(Input->getType() == mx::Type::Color3)
	{
		mx::Color3 Color = mx::fromValueString<mx::Color3>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], Color[2] };
	}
	else if(Input->getType() == mx::Type::Color4)
	{
		mx::Color4 Color = mx::fromValueString<mx::Color4>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], Color[2], Color[3] };
	}
	else
	{
		ensureMsgf(false, TEXT("input type can only be color3 or color4"));
	}

	const FString ColorSpace = GetColorSpace(Input);

	if(ColorSpace.IsEmpty() || ColorSpace == TEXT("lin_rec709") || ColorSpace == TEXT("none"))
	{
		;//noop
	}
	else if(ColorSpace == TEXT("gamma22"))
	{
		LinearColor = FLinearColor::FromPow22Color(FColor(LinearColor.R / 255.f, LinearColor.G / 255.f, LinearColor.B / 255.f, LinearColor.A / 255.f));
	}
	else
	{
		UInterchangeResultWarning_Generic* Message = AddMessage<UInterchangeResultWarning_Generic>();
		Message->Text = FText::Format(LOCTEXT("ColorSpaceNotSupported", "<{0}>-<{1}>: Colorspace {2} is not supported yet, falling back to linear"),
									  FText::FromString(Input->getParent()->getName().c_str()),
									  FText::FromString(Input->getName().c_str()),
									  FText::FromString(ColorSpace));
	}

	return LinearColor;
}

void UInterchangeMaterialXTranslator::ConnectConstantInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	AddAttributeFromValueOrInterface(UpstreamNode->getInput("value"), InputChannelName, ParentShaderNode);
}

void UInterchangeMaterialXTranslator::ConnectExtractInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* MaskShaderNode = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), Mask::Name.ToString(), NamesToShaderNodes, NodeContainer);

	if(mx::InputPtr InputIndex = UpstreamNode->getInput("index"))
	{
		const int32 Index = mx::fromValueString<int>(InputIndex->getValueString());
		switch(Index)
		{
		case 0: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::R, true); break;
		case 1: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::G, true); break;
		case 2: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::B, true); break;
		case 3: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::A, true); break;
		default:
			UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
			Message->Text = FText::FromString(TEXT("Wrong index number for extract node, values are from [0-3]"));
			break;
		}
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, MaskShaderNode->GetUniqueID());
}

void UInterchangeMaterialXTranslator::ConnectDotInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	if(mx::InputPtr Input = UpstreamNode->getInput("in"))
	{
		RenameInput(Input, TCHAR_TO_UTF8(*InputChannelName)); //let's take the parent node's input name
		NamesToShaderNodes.Add(UpstreamNode->getName().c_str(), ParentShaderNode);
	}
}

void UInterchangeMaterialXTranslator::ConnectTransformPositionInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	UInterchangeShaderNode* TransformNode = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TEXT("TransformPosition"), NamesToShaderNodes, NodeContainer);
	AddAttributeFromValueOrInterface(UpstreamNode->getInput("in"), TEXT("Input"), TransformNode);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, TransformNode->GetUniqueID());
}

void UInterchangeMaterialXTranslator::ConnectTransformVectorInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	UInterchangeShaderNode* TransformNode = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TEXT("Transform"), NamesToShaderNodes, NodeContainer);
	AddAttributeFromValueOrInterface(UpstreamNode->getInput("in"), TEXT("Input"), TransformNode);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, TransformNode->GetUniqueID());
}

void UInterchangeMaterialXTranslator::ConnectRotate2DInputToOutput(mx::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	UInterchangeShaderNode* Rotate2DNode = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TEXT("RotateAboutAxis"), NamesToShaderNodes, NodeContainer);
	Rotate2DNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TEXT("NormalizedRotationAxis")), FLinearColor(0, 0, 1));
	Rotate2DNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TEXT("PivotPoint")), FLinearColor(0.5, 0.5, 0));

	{
		mx::InputPtr Input = UpstreamNode->getInput("in");
		AddAttributeFromValueOrInterface(UpstreamNode->getInput("in"), TEXT("Position"), Rotate2DNode);

		// Rotate2D "in" input takes a vector2 input, but RotateAboutAxis takes a vector3 so we convert
		if(Input)
		{
			mx::NodePtr ConvertNode = CreateNode(UpstreamNode->getParent()->asA<mx::NodeGraph>(),
												 UpstreamNode->getName().c_str(),
												 mx::Category::Convert,
												 { {"in", Input} },
												 {});
			ConvertNode->setType(mx::Type::Vector3);
			Input->setNodeName(ConvertNode->getName());
		}
	}

	{

		mx::InputPtr Input = UpstreamNode->getInput("amount");
		AddAttributeFromValueOrInterface(Input, TEXT("RotationAngle"), Rotate2DNode);

		// we create a Divide node to convert MaterialX angle in degrees to UE's angle which is a value between [0,1]
		if(Input)
		{
			//copy "in" input into "in1" and create "amount" input as "in2" with the value 360 because UE's angle is a value between [0,1]
			mx::NodePtr DivideNode = CreateNode(UpstreamNode->getParent()->asA<mx::NodeGraph>(),
												UpstreamNode->getName().c_str(),
												mx::Category::Divide,
												{ {"in1", Input} }, // rename input to match <divide> input "in1"
												{ {"in2", FAttributeValueArray{{"type", "float"}, {"value", "360"}}} });

			// Input now points to the new node
			Input->setNodeName(DivideNode->getName());
		}
	}

	//We insert a mask node because the output has to be a vector2
	UInterchangeShaderNode * MaskNode = CreateMaskShaderNode(0b1100, UpstreamNode->getName().c_str() + FString(TEXT("_Mask")), NamesToShaderNodes, NodeContainer);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MaskNode, TEXT("Input"), Rotate2DNode->GetUniqueID());

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, MaskNode->GetUniqueID());
}

void UInterchangeMaterialXTranslator::ConnectRotate3DInputToOutput(mx::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	UInterchangeShaderNode* Rotate3DNode = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TEXT("RotateAboutAxis"), NamesToShaderNodes, NodeContainer);
	Rotate3DNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TEXT("PivotPoint")), FLinearColor(0.5, 0.5, 0));

	AddAttributeFromValueOrInterface(UpstreamNode->getInput("in"), TEXT("Position"), Rotate3DNode);
	AddAttributeFromValueOrInterface(UpstreamNode->getInput("axis"), TEXT("NormalizedRotationAxis"), Rotate3DNode);

	// we create a Divide node to convert MaterialX angle in degrees to UE's angle which is a value between [0,1]
	if(mx::InputPtr Input = UpstreamNode->getInput("amount"); !AddAttributeFromValueOrInterface(Input, TEXT("RotationAngle"), Rotate3DNode))
	{
		//copy "in" input into "in1" and create "amount" input as "in2" with the value 360 because UE's angle is a value between [0,1]
		mx::NodePtr NewDivideNode = CreateNode(UpstreamNode->getParent()->asA<mx::NodeGraph>(),
											   UpstreamNode->getName().c_str(),
											   mx::Category::Divide,
											   { {"in1", Input} }, // rename input to match <divide> input "in1"
											   { {"in2", FAttributeValueArray{{"type", "float"}, {"value", "360"}}} });

		// Input now points to the new node
		Input->setNodeName(NewDivideNode->getName());
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, Rotate3DNode->GetUniqueID());
}

void UInterchangeMaterialXTranslator::ConnectImageInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	if(UInterchangeTextureNode* TextureNode = CreateTextureNode(UpstreamNode, NodeContainer))
	{
		//By default set the output of a texture to RGB
		FString OutputChannel{ TEXT("RGB") };

		if(UpstreamNode->getType() == mx::Type::Vector4 || UpstreamNode->getType() == mx::Type::Color4)
		{
			OutputChannel = TEXT("RGBA");
		}
		else if(UpstreamNode->getType() == mx::Type::Float)
		{
			OutputChannel = TEXT("R");
		}

		UInterchangeShaderNode* TextureShaderNode = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TextureSample::Name.ToString(), NamesToShaderNodes, NodeContainer);
		TextureShaderNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSample::Inputs::Texture.ToString()), TextureNode->GetUniqueID());
		UInterchangeShaderPortsAPI::ConnectOuputToInput(ParentShaderNode, InputChannelName, TextureShaderNode->GetUniqueID(), OutputChannel);
	}
	else
	{
		AddAttributeFromValueOrInterface(UpstreamNode->getInput(mx::NodeGroup::Texture2D::Inputs::Default), InputChannelName, ParentShaderNode);
	}
}

void UInterchangeMaterialXTranslator::ConnectConvertInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	//In case of an upwards conversion, let's do an append, downwards a mask, otherwise leave as is
	mx::InputPtr Input = UpstreamNode->getInput("in");
	const std::string& NodeType = UpstreamNode->getType();
	const std::string& InputType = Input->getType();

	//Ensure that the types are supported
	const bool bIsNodeTypeSupported =
		NodeType == mx::Type::Color4 ||
		NodeType == mx::Type::Color3 ||
		NodeType == mx::Type::Vector4 ||
		NodeType == mx::Type::Vector3 ||
		NodeType == mx::Type::Vector2 ||
		NodeType == mx::Type::Float ||
		NodeType == mx::Type::Integer ||
		NodeType == mx::Type::Boolean;

	const bool bIsInputTypeSupported =
		InputType == mx::Type::Color4 ||
		InputType == mx::Type::Color3 ||
		InputType == mx::Type::Vector4 ||
		InputType == mx::Type::Vector3 ||
		InputType == mx::Type::Vector2 ||
		InputType == mx::Type::Float ||
		InputType == mx::Type::Integer ||
		InputType == mx::Type::Boolean;

	if(!bIsNodeTypeSupported || !bIsInputTypeSupported)
	{
		UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
		Message->Text = FText::FromString("<convert> node has non supported types");
		return;
	}

	char NodeN = NodeType.back();
	char InputN = InputType.back();

	constexpr auto Remap = [](auto Value, auto Low1, auto High1, auto Low2, auto High2)
	{
		return Low2 + (Value - Low1) * (High2 - Low2) / (High1 - Low1);
	};

	//Remap NodeN and InputN in a range lower than '2' in case of integer/boolean/float
	if(NodeN > '4')
	{
		NodeN = Remap(NodeN, 'a', 'z', 0, '1');
	}
	if(InputN > '4')
	{
		InputN = Remap(InputN, 'a', 'z', 0, '1');
	}

	if(InputN > NodeN) // Mask
	{
		UInterchangeShaderNode* MaskShaderNode = nullptr;
		if(NodeN == '3')
		{
			MaskShaderNode = CreateMaskShaderNode(0b1110, UpstreamNode->getName().c_str(), NamesToShaderNodes, NodeContainer);
		}
		else if(NodeN == '2')
		{
			MaskShaderNode = CreateMaskShaderNode(0b1100, UpstreamNode->getName().c_str(), NamesToShaderNodes, NodeContainer);
		}
		else
		{
			MaskShaderNode = CreateMaskShaderNode(0b1000, UpstreamNode->getName().c_str(), NamesToShaderNodes, NodeContainer);
		}

		AddAttributeFromValueOrInterface(Input, TEXT("Input"), MaskShaderNode);

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, MaskShaderNode->GetUniqueID());
	}
	else // Append 
	{
		//same as dot, just connect the next output to this parent input
		RenameInput(Input, TCHAR_TO_UTF8(*InputChannelName)); //let's take the parent node's input name
		NamesToShaderNodes.Add(UpstreamNode->getName().c_str(), ParentShaderNode);

		//No need to create a node, since the input and the node have the same channels, we just check if there's a value
		if(NodeN == InputN || (NodeN < '2' && InputN < '2'))
		{
			AddAttributeFromValueOrInterface(Input, InputChannelName, ParentShaderNode);
			return;
		}
		std::string Category;
		TArray<FInputToCopy> InputsToCopy;
		TArray<FInputToCreate> InputsToCreate;

		// float to N
		if(InputN < '2')
		{
			if(NodeN == '2')
			{
				Category = mx::Category::Combine2;
				InputsToCopy.Add({ "in1", Input });
				InputsToCopy.Add({ "in2", Input });
			}
			else if(NodeN == '3')
			{
				Category = mx::Category::Combine3;
				InputsToCopy.Add({ "in1", Input });
				InputsToCopy.Add({ "in2", Input });
				InputsToCopy.Add({ "in3", Input });
			}
			else if(NodeN == '4')
			{
				Category = mx::Category::Combine4;
				InputsToCopy.Add({ "in1", Input });
				InputsToCopy.Add({ "in2", Input });
				InputsToCopy.Add({ "in3", Input });
				InputsToCopy.Add({ "in4", Input });
			}
		}
		else if((InputN == '2' && NodeN == '3') || (InputN == '3' && NodeN == '4'))
		{
			Category = mx::Category::Combine2;
			InputsToCopy.Add({ "in1", Input });
			InputsToCreate.Add({ "in2", FAttributeValueArray{{"type","float"}, {"value","1"}}});
		}

		//copy "in" input into "in1" and create "amount" input as "in2" with the value 360 because UE's angle is a value between [0,1]
		mx::NodePtr CombineNode = CreateNode(UpstreamNode->getParent()->asA<mx::NodeGraph>(),
											 UpstreamNode->getName().c_str(),
											 Category.c_str(),
											 InputsToCopy, // rename input to match <divide> input "in1"
											 InputsToCreate);

		// Input now points to the new node
		Input->setNodeName(CombineNode->getName());
	}
}

void UInterchangeMaterialXTranslator::ConnectIfGreaterInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TEXT("If"), NamesToShaderNodes, NodeContainer);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}
	
	if(mx::InputPtr Input = UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}
	
	if(mx::InputPtr Input = UpstreamNode->getInput("in1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AEqualsB
	mx::InputPtr Input = UpstreamNode->getInput("in2");
	if(Input)
	{
		bool bHasValue = AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, TEXT("AEqualsB"), NodeIf);

		if(bHasValue)
		{
			//Let's add a new input that is a copy of in2 to connect it to the equal input
			mx::InputPtr Input3 = UpstreamNode->addInput("in3");
			Input3->copyContentFrom(Input);
			RenameInput(Input3, "AEqualsB");
		}
	}
}

void UInterchangeMaterialXTranslator::ConnectIfGreaterEqInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode * ParentShaderNode, const FString & InputChannelName, TMap<FString, UInterchangeShaderNode*>&NamesToShaderNodes, UInterchangeBaseNodeContainer & NodeContainer) const
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TEXT("If"), NamesToShaderNodes, NodeContainer);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = UpstreamNode->getInput("in2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AEqualsB
	mx::InputPtr Input = UpstreamNode->getInput("in1");
	if(Input)
	{
		bool bHasValue = AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, TEXT("AEqualsB"), NodeIf);

		if(bHasValue)
		{
			//Let's add a new input that is a copy of in2 to connect it to the equal input
			mx::InputPtr Input3 = UpstreamNode->addInput("in3");
			Input3->copyContentFrom(Input);
			RenameInput(Input3, "AEqualsB");
		}
	}
}

void UInterchangeMaterialXTranslator::ConnectIfEqualInputToOutput(MaterialX::NodePtr UpstreamNode, UInterchangeShaderNode * ParentShaderNode, const FString & InputChannelName, TMap<FString, UInterchangeShaderNode*>&NamesToShaderNodes, UInterchangeBaseNodeContainer & NodeContainer) const
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode<UInterchangeShaderNode>(UpstreamNode->getName().c_str(), TEXT("If"), NamesToShaderNodes, NodeContainer);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ParentShaderNode, InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = UpstreamNode->getInput("value1"); Input && Input->hasValue())
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = UpstreamNode->getInput("value2"); Input && Input->hasValue())
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = UpstreamNode->getInput("in1"); Input && Input->hasValue())
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AGreaterThanB
	mx::InputPtr Input = UpstreamNode->getInput("in2");
	if(Input)
	{
		bool bHasValue = AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, TEXT("AGreaterThanB"), NodeIf);

		if(bHasValue)
		{
			//Let's add a new input that is a copy of in2 to connect it to the equal input
			mx::InputPtr Input3 = UpstreamNode->addInput("in3");
			Input3->copyContentFrom(Input);
			RenameInput(Input3, "AGreaterThanB");
		}
	}
}

UInterchangeShaderNode* UInterchangeMaterialXTranslator::CreateMaskShaderNode(uint8 RGBA, const FString& NodeName, TMap<FString, UInterchangeShaderNode*>& NamesToShaderNodes, UInterchangeBaseNodeContainer& NodeContainer) const
{
	bool bR = (0b1000 & RGBA) >> 3;
	bool bG = (0b0100 & RGBA) >> 2;
	bool bB = (0b0010 & RGBA) >> 1;
	bool bA = (0b0001 & RGBA) >> 0;
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* MaskShaderNode = CreateShaderNode<UInterchangeShaderNode>(NodeName, Mask::Name.ToString(), NamesToShaderNodes, NodeContainer);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::R, bR);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::G, bG);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::B, bB);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::A, bA);

	return MaskShaderNode;
}

FLinearColor UInterchangeMaterialXTranslator::GetVector(MaterialX::InputPtr Input) const
{
	FLinearColor LinearColor;

	if(Input->getType() == mx::Type::Vector2)
	{
		mx::Vector2 Color = mx::fromValueString<mx::Vector2>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], 0 };
	}
	else if(Input->getType() == mx::Type::Vector3)
	{
		mx::Vector3 Color = mx::fromValueString<mx::Vector3>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], Color[2] };
	}
	else if(Input->getType() == mx::Type::Vector4)
	{
		mx::Vector4 Color = mx::fromValueString<mx::Vector4>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], Color[2], Color[3] };
	}
	else
	{
		ensureMsgf(false, TEXT("input type can only be a vectorN"));
	}

	return LinearColor;
}

MaterialX::NodePtr UInterchangeMaterialXTranslator::CreateNode(MaterialX::NodeGraphPtr NodeGraph, const char* NodeName, const char* Category, TArray<FInputToCopy> InputsToCopy, TArray<FInputToCreate> InputsToCreate) const
{
	std::string UniqueNodeName{ NodeName + std::string{"_"} + Category };

	mx::NodePtr Node = NodeGraph->getNode(UniqueNodeName);

	if(!Node)
	{
		Node = NodeGraph->addChildOfCategory(Category, UniqueNodeName)->asA<mx::Node>();

		for(const FInputToCopy& Pair : InputsToCopy)
		{
			const char* NewInputName = Pair.Get<0>();
			mx::InputPtr Input = Pair.Get<1>();

			mx::InputPtr InputCopy = Node->addInput();
			InputCopy->copyContentFrom(Input);
			//remove the attribute NewName since when the input will be copied, the name will be the one from the spec (before a renaming)
			InputCopy->removeAttribute(mx::Attributes::NewName);

			if(NewInputName)
			{
				InputCopy->setName(NewInputName);
			}
		}

		for(const FInputToCreate& Input : InputsToCreate)
		{
			const char* InputName = Input.Get<0>();
			const FAttributeValueArray& Attributes = Input.Get<1>();

			mx::InputPtr NewInput = Node->addInput();
			NewInput->setName(InputName);
			for(const FAttributeValue& AttributeValue : Attributes)
			{
				const char* Attribute = AttributeValue.Get<0>();
				const char* Value = AttributeValue.Get<1>();
				NewInput->setAttribute(Attribute, Value);
			}
		}
	}

	return Node;
}

void UInterchangeMaterialXTranslator::ConvertNeutralNodesToDot(MaterialX::NodeGraphPtr NodeGraph) const
{
	const std::vector<mx::NodePtr> Nodes = NodeGraph->getNodes();
	for(mx::NodePtr Node : Nodes)
	{
		const std::string& Category = Node->getCategory();
		if(Category == mx::Category::Multiply || Category == mx::Category::Divide || Category == mx::Category::Add || Category == mx::Category::Sub)
		{
			mx::InputPtr Input2 = Node->getInput("in2");
			bool bIsNeutral = false;

			if(!Input2)
			{
				bIsNeutral = true;
				if(mx::InputPtr Input1 = Node->getInput("in1"))
				{
					Input1->setName("in");
				}
			}
			else
			{
				if(!Input2->hasValue() && !Input2->hasNodeName() && !Input2->hasInterfaceName())
				{
					bIsNeutral = true;
					Node->removeInput("in2");
					if(mx::InputPtr Input1 = Node->getInput("in1"))
					{
						Input1->setName("in");
					}
				}
				else if(Input2->hasValue())
				{
					using namespace mx::NodeGroup::Math;

					auto IsValueStringNeutral = [&bIsNeutral, &Category, Input2](auto T)
					{
						bIsNeutral = (Category == mx::Category::Multiply || Category == mx::Category::Divide) ?
							mx::fromValueString<decltype(T)>(Input2->getValueString()) == NeutralOne<decltype(T)> :
							mx::fromValueString<decltype(T)>(Input2->getValueString()) == NeutralZero<decltype(T)>;
					};

					if(Input2->getType() == mx::Type::Float)
					{
						IsValueStringNeutral(float());
					}
					else if(Input2->getType() == mx::Type::Vector2)
					{
						IsValueStringNeutral(mx::Vector2());
					}
					else if(Input2->getType() == mx::Type::Vector3)
					{
						IsValueStringNeutral(mx::Vector3());
					}
					else if(Input2->getType() == mx::Type::Vector4)
					{
						IsValueStringNeutral(mx::Vector4());
					}
					else if(Input2->getType() == mx::Type::Color3)
					{
						IsValueStringNeutral(mx::Color3());
					}
					else if(Input2->getType() == mx::Type::Color4)
					{
						IsValueStringNeutral(mx::Color4());
					}
				}
			}

			if(bIsNeutral)
			{
				Node->setCategory(mx::Category::Dot);
			}
		}
	}
}

void UInterchangeMaterialXTranslator::AddTexCoordToTiledImageNodes(MaterialX::NodeGraphPtr NodeGraph) const
{
	for(mx::NodePtr Node : NodeGraph->getNodes(mx::Category::TiledImage))
	{
		mx::InputPtr InputTexCoord = Node->getInput("texcoord");

		if(!InputTexCoord)
		{
			mx::NodePtr NodeTexCoord = CreateNode(Node->getParent()->asA<mx::NodeGraph>(),
												  "node_ue_texcoord",
												  mx::Category::TexCoord,
												  {},
												  { {"index", FAttributeValueArray{{"type", "integer"}, {"value", "0"}}} });

			InputTexCoord = Node->addInput(mx::NodeGroup::Texture2D::Inputs::TexCoord, mx::Type::Vector2);
			InputTexCoord->setNodeName(NodeTexCoord->getName());
		}
	}
}

#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE
