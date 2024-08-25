// Copyright Epic Games, Inc. All Rights Reserved. 

#if WITH_EDITOR
#include "MaterialX/MaterialXUtils/MaterialXSurfaceShaderAbstract.h"

#include "InterchangeImportLog.h"
#include "InterchangeManager.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTextureBlurNode.h"
#include "InterchangeTranslatorBase.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "MaterialX/MaterialXUtils/MaterialXManager.h"
#include "MaterialX/MaterialExpressions/MaterialExpressionTextureSampleParameterBlur.h"

#define LOCTEXT_NAMESPACE "MaterialXSurfaceShaderAbstract"

namespace mx = MaterialX;

FString FMaterialXSurfaceShaderAbstract::EmptyString{};
FString FMaterialXSurfaceShaderAbstract::DefaultOutput{TEXT("out")};

FMaterialXSurfaceShaderAbstract::FMaterialXSurfaceShaderAbstract(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXBase{ BaseNodeContainer }
	, ShaderGraphNode{ nullptr }
	, bTangentSpaceInput{ false }
{}

bool FMaterialXSurfaceShaderAbstract::AddAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input)
	{
		if(Input->getType() == mx::Type::Boolean)
		{
			return AddBooleanAttribute(Input, InputChannelName, ShaderNode);
		}
		else if(Input->getType() == mx::Type::Float)
		{
			return AddFloatAttribute(Input, InputChannelName, ShaderNode);
		}
		else if(Input->getType() == mx::Type::Integer) //Let's add Float attribute, because Interchange doesn't create a scalar if it's an int
		{
			return ShaderNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), mx::fromValueString<int32>(Input->getValueString()));
		}
		else if(Input->getType() == mx::Type::Color3 || Input->getType() == mx::Type::Color4)
		{
			return AddLinearColorAttribute(Input, InputChannelName, ShaderNode);
		}
		else if(Input->getType() == mx::Type::Vector2)
		{
			FLinearColor Vector = GetVector(Input);
			return ShaderNode->AddVector2Attribute(UInterchangeShaderPortsAPI::MakeInputValueKey(InputChannelName), FVector2f{ Vector.R, Vector.G });
		}
		else if(Input->getType() == mx::Type::Vector3 || Input->getType() == mx::Type::Vector4)
		{
			return AddVectorAttribute(Input, InputChannelName, ShaderNode);
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddAttributeFromValueOrInterface(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode)
{
	bool bAttribute = false;

	if(Input)
	{
		UInterchangeShaderNode* ShaderNodeToConnectTo = ShaderNode;
		FString InputToConnectTo = InputChannelName;

		if(Input->hasChannels())
		{
			using namespace UE::Interchange::Materials::Standard::Nodes;

			UInterchangeShaderNode* SwizzleNode = CreateShaderNode(Input->getParent()->asA<mx::Node>()->getName().c_str() + FString{ TEXT("_Channels_") } + Input->getName().c_str(), Swizzle::Name.ToString());
			SwizzleNode->AddStringAttribute(Swizzle::Attributes::Channels.ToString(), Input->getChannels().c_str());

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, SwizzleNode->GetUniqueID());

			ShaderNodeToConnectTo = SwizzleNode;
			InputToConnectTo = TEXT("Input");
		}

		if(Input->hasValue())
		{
			bAttribute = AddAttribute(Input, InputToConnectTo, ShaderNodeToConnectTo);
		}
		else if(Input->hasInterfaceName())
		{
			if(mx::InputPtr InputInterface = Input->getInterfaceInput(); InputInterface->hasValue())
			{
				bAttribute = AddAttribute(InputInterface, InputToConnectTo, ShaderNodeToConnectTo);
			}
		}
	}

	return bAttribute;
}

bool FMaterialXSurfaceShaderAbstract::AddBooleanAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		bool Value = mx::fromValueString<bool>(Input->getValueString());

		// The parent is either a node, or it's an interfacename and we just take the name of the input
		mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
		FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
		NodeName += Input->getName().c_str();

		UInterchangeShaderNode* StaticBoolParameterNode = CreateShaderNode(NodeName, StaticBoolParameter::Name.ToString());
		StaticBoolParameterNode->AddBooleanAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(StaticBoolParameter::Attributes::DefaultValue.ToString()), Value);
		return UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, StaticBoolParameterNode->GetUniqueID());
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddFloatAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, float DefaultValue)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		float Value = mx::fromValueString<float>(Input->getValueString());

		if(!FMath::IsNearlyEqual(Value, DefaultValue))
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* ScalarParameterNode = CreateShaderNode(NodeName, ScalarParameter::Name.ToString());
			ScalarParameterNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(ScalarParameter::Attributes::DefaultValue.ToString()), Value);
			return UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, ScalarParameterNode->GetUniqueID());
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddLinearColorAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FLinearColor& DefaultValue)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		const FLinearColor Value = GetLinearColor(Input);

		if(!Value.Equals(DefaultValue))
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* VectorParameterNode = CreateShaderNode(NodeName, VectorParameter::Name.ToString());
			VectorParameterNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(VectorParameter::Attributes::DefaultValue.ToString()), Value);
			return UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, VectorParameterNode->GetUniqueID());
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddVectorAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FVector4f& DefaultValue)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		const FLinearColor Value = GetVector(Input);

		if(!Value.Equals(DefaultValue))
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* VectorParameterNode = CreateShaderNode(NodeName, VectorParameter::Name.ToString());
			VectorParameterNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(VectorParameter::Attributes::DefaultValue.ToString()), Value);
			return UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, VectorParameterNode->GetUniqueID());
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::ConnectNodeGraphOutputToInput(MaterialX::InputPtr InputToNodeGraph, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	bool bHasNodeGraph = false;

	if(InputToNodeGraph->hasNodeGraphString())
	{
		bHasNodeGraph = true;

		mx::OutputPtr Output = InputToNodeGraph->getConnectedOutput();

		if(!Output)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Couldn't find a connected output to (%s)."), *GetInputName(InputToNodeGraph));
			return false;
		}

		for(mx::Edge Edge : Output->traverseGraph())
		{
			Output->hasOutputString() ?
				ConnectNodeCategoryOutputToInput(Edge, ShaderNode, ParentInputName, Output->getOutputString().c_str()) :
				ConnectNodeCategoryOutputToInput(Edge, ShaderNode, ParentInputName);
		}
	}

	return bHasNodeGraph;
}

bool FMaterialXSurfaceShaderAbstract::ConnectMatchingNodeOutputToInput(const FConnectNode& Connect)
{
	FMaterialXManager& Manager = FMaterialXManager::GetInstance();

	auto GetIndexOutput = [&Connect]()
	{
		mx::NodeDefPtr NodeDef = Connect.UpstreamNode->getNodeDef(mx::EMPTY_STRING, true);
		int Index = NodeDef->getChildIndex(TCHAR_TO_UTF8(*Connect.OutputName));
		return Index < 0 ? Index : Index % NodeDef->getInputCount();
	};

	bool bIsConnected = false;

	auto ConnectOutputToInputInternal = [&](UInterchangeShaderNode* OperatorNode, bool bFindMatchingInput = true)
	{
		for(mx::InputPtr Input : Connect.UpstreamNode->getInputs())
		{
			if(!bFindMatchingInput)
			{
				AddAttributeFromValueOrInterface(Input, Input->getName().c_str(), OperatorNode);
			}
			else if(const FString* InputNameFound = Manager.FindMaterialExpressionInput(GetInputName(Input)))
			{
				AddAttributeFromValueOrInterface(Input, *InputNameFound, OperatorNode);
			}
		}

		int IndexOutput = GetIndexOutput();

		bIsConnected = IndexOutput < 0 ?
			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, OperatorNode->GetUniqueID()) :
			UInterchangeShaderPortsAPI::ConnectOuputToInputByIndex(Connect.ParentShaderNode, Connect.InputChannelName, OperatorNode->GetUniqueID(), IndexOutput);
	};

	auto ConnectOutputToInput = [&](const FString* ShaderType, auto* (FMaterialXSurfaceShaderAbstract::* CreateFunctionCallOrShaderNode)(const FString&, const FString&, const FString&), bool bFindMatchingInput = true)
	{
		UInterchangeShaderNode* OperatorNode = nullptr;

		//We don't take the node output here because it would cause the creation of a new node (output is meaningful with ComponentMaskNode/separate where we have to create a new expression
		OperatorNode = (this->*CreateFunctionCallOrShaderNode)(Connect.UpstreamNode->getName().c_str(), *ShaderType, DefaultOutput);

		ConnectOutputToInputInternal(OperatorNode, bFindMatchingInput);
	};

	auto ConnectFunctionShaderNodeOutputToInput = [&](uint8 EnumType, uint8 EnumValue, auto* (FMaterialXSurfaceShaderAbstract::* CreateFunctionCallOrShaderNode)(const FString&, uint8, uint8, const FString&), bool bFindMatchingInput = true)
	{
		UInterchangeShaderNode* OperatorNode = nullptr;

		//We don't take the node output here because it would cause the creation of a new node (output is meaningful with ComponentMaskNode/separate where we have to create a new expression
		OperatorNode = (this->*CreateFunctionCallOrShaderNode)(Connect.UpstreamNode->getName().c_str(), EnumType, EnumValue, DefaultOutput);

		ConnectOutputToInputInternal(OperatorNode, bFindMatchingInput);
	};

	const FString* MaterialFunctionPath = nullptr;

	// First search a matching Material Expression
	// search for a Material Expression based on the node group (essentially used for Substrate Mix)
	if(const FString* ShaderType = Manager.FindMatchingMaterialExpression(Connect.UpstreamNode->getCategory().c_str(), Connect.UpstreamNode->getNodeDef(mx::EMPTY_STRING, true)->getNodeGroup().c_str()))
	{
		ConnectOutputToInput(ShaderType, &FMaterialXSurfaceShaderAbstract::CreateShaderNode);
	}
	else if((ShaderType = Manager.FindMatchingMaterialExpression(Connect.UpstreamNode->getCategory().c_str())))
	{
		ConnectOutputToInput(ShaderType, &FMaterialXSurfaceShaderAbstract::CreateShaderNode);
	}
	else if(FOnConnectNodeOutputToInput* Delegate = MatchingConnectNodeDelegates.Find(Connect.UpstreamNode->getCategory().c_str()))
	{
		bIsConnected = Delegate->ExecuteIfBound(Connect);
	}
	else if(uint8 EnumType, EnumValue; Manager.FindMatchingMaterialFunction(Connect.UpstreamNode->getCategory().c_str(), MaterialFunctionPath, EnumType, EnumValue))
	{
		MaterialFunctionPath ?
			ConnectOutputToInput(MaterialFunctionPath, &FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode, false) :
			ConnectFunctionShaderNodeOutputToInput(EnumType, EnumValue, &FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode, false);
	}

	return bIsConnected;
}

void FMaterialXSurfaceShaderAbstract::ConnectNodeCategoryOutputToInput(const MaterialX::Edge& Edge, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName, const FString& OutputName)
{
	if(mx::NodePtr UpstreamNode = Edge.getUpstreamElement()->asA<mx::Node>())
	{
		// We need to connect the different descending nodes to all the outputs of a node
		// At least one output connected to the root shader node
		TArray<UInterchangeShaderNode*> ParentShaderNodeOutputs{ShaderNode};
		FString InputChannelName = ParentInputName;

		//Replace the input's name by the one used in UE
		SetMatchingInputsNames(UpstreamNode);

		FString OutputChannelName = OutputName;

		// Swizzle node for the attribute 'channels'
		UInterchangeShaderNode* ChannelsNode = nullptr;

		if(mx::ElementPtr DownstreamElement = Edge.getDownstreamElement())
		{
			if(mx::NodePtr DownstreamNode = DownstreamElement->asA<mx::Node>())
			{
				if(mx::InputPtr ConnectedInput = Edge.getConnectingElement()->asA<mx::Input>())
				{
					InputChannelName = GetInputName(ConnectedInput);
					if(ConnectedInput->hasOutputString())
					{
						OutputChannelName = ConnectedInput->getOutputString().c_str();
					}

					if(ConnectedInput->hasChannels())
					{
						using namespace UE::Interchange::Materials::Standard::Nodes;
						ChannelsNode = CreateShaderNode((UpstreamNode->getName() + ConnectedInput->getName()).c_str() + FString{ TEXT("_Channels") }, Swizzle::Name.ToString());
						ChannelsNode->AddStringAttribute(Swizzle::Attributes::Channels.ToString(), ConnectedInput->getChannels().c_str());
					}
				}

				std::vector<mx::OutputPtr> Outputs{ DownstreamNode->getOutputs()};
				if(Outputs.empty())
				{
					Outputs = DownstreamNode->getNodeDef(mx::EMPTY_STRING, true)->getOutputs();
				}

				ParentShaderNodeOutputs.Empty();
				for(std::size_t i = 0; i < Outputs.size(); ++i)
				{
					if(UInterchangeShaderNode** FoundNode = ShaderNodes.Find({ GetAttributeParentName(DownstreamNode), Outputs[i]->getName().c_str()}))
					{
						//Connect the swizzle node between the upstream and downstream node
						if(ChannelsNode)
						{
							UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(*FoundNode, InputChannelName, ChannelsNode->GetUniqueID());
							InputChannelName = TEXT("Input");
							FoundNode = &ChannelsNode;
						}

						ParentShaderNodeOutputs.Emplace(*FoundNode);
					}
				}
			}
		}

		for(UInterchangeShaderNode* ParentShaderNode: ParentShaderNodeOutputs)
		{
			if(!ConnectMatchingNodeOutputToInput({ UpstreamNode, ParentShaderNode, InputChannelName, OutputChannelName }))
			{
				UE_LOG(LogInterchangeImport, Warning, TEXT("<%s> is not supported."), ANSI_TO_TCHAR(UpstreamNode->getCategory().c_str()));
			}
		}
	}
}

bool FMaterialXSurfaceShaderAbstract::ConnectNodeNameOutputToInput(MaterialX::InputPtr InputToConnectedNode, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName)
{
	mx::NodePtr ConnectedNode = InputToConnectedNode->getConnectedNode();

	if(!ConnectedNode)
	{
		return false;
	}

	UInterchangeShaderNode* ParentShaderNode = ShaderNode;
	FString InputChannelName = ParentInputName;

	mx::Edge Edge(nullptr, InputToConnectedNode, ConnectedNode);

	TArray<mx::Edge> Stack{ Edge };

	while(!Stack.IsEmpty())
	{
		Edge = Stack.Pop();

		if(Edge.getUpstreamElement())
		{
			ConnectNodeCategoryOutputToInput(Edge, ShaderNode, ParentInputName);
			ConnectedNode = Edge.getUpstreamElement()->asA<mx::Node>();
			for(mx::InputPtr Input : ConnectedNode->getInputs())
			{
				Stack.Emplace(ConnectedNode, Input, Input->getConnectedNode());
			}
		}
	}

	return true;
}

void FMaterialXSurfaceShaderAbstract::ConnectConstantInputToOutput(const FConnectNode& Connect)
{
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("value"), Connect.InputChannelName, Connect.ParentShaderNode);
}

void FMaterialXSurfaceShaderAbstract::ConnectExtractInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* MaskShaderNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), Mask::Name.ToString());

	if(mx::InputPtr InputIndex = Connect.UpstreamNode->getInput("index"))
	{
		const int32 Index = mx::fromValueString<int>(InputIndex->getValueString());
		switch(Index)
		{
		case 0: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::R.ToString(), true); break;
		case 1: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::G.ToString(), true); break;
		case 2: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::B.ToString(), true); break;
		case 3: MaskShaderNode->AddBooleanAttribute(Mask::Attributes::A.ToString(), true); break;
		default:
			UE_LOG(LogInterchangeImport, Warning, TEXT("Wrong index number for extracted node. Values are from [0-3]."));
			break;
		}
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, MaskShaderNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectDotInputToOutput(const FConnectNode& Connect)
{
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		SetAttributeNewName(Input, TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
		ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName}, Connect.ParentShaderNode);
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectTransformPositionInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("TransformPosition"));
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), TEXT("Input"), TransformNode);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TransformNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectTransformVectorInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("Transform"));
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), TEXT("Input"), TransformNode);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TransformNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectRotate3DInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* Rotate3DNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("RotateAboutAxis"));
	Rotate3DNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TEXT("PivotPoint")), FLinearColor(0.5, 0.5, 0));

	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), TEXT("Position"), Rotate3DNode);
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("axis"), TEXT("NormalizedRotationAxis"), Rotate3DNode);

	// we create a Divide node to convert MaterialX angle in degrees to UE's angle which is a value between [0,1]
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("amount"))
	{
		if(!AddAttributeFromValueOrInterface(Input, TEXT("RotationAngle"), Rotate3DNode))
		{
			//copy "in" input into "in1" and create "amount" input as "in2" with the value 360 because UE's angle is a value between [0,1]
			mx::NodePtr NewDivideNode = CreateNode(Connect.UpstreamNode->getParent()->asA<mx::NodeGraph>(),
												   Connect.UpstreamNode->getName().c_str(),
												   mx::Category::Divide,
												   { {"in1", Input} }, // rename input to match <divide> input "in1"
												   { {"in2", FAttributeValueArray{{"type", "float"}, {"value", "360"}}} });

			//We need to set the type otherwise we won't be able to find a nodedef
			NewDivideNode->setType(Input->getType());
			// Input now points to the new node
			Input->setNodeName(NewDivideNode->getName());
		}
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, Rotate3DNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectImageInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	if(UInterchangeTextureNode* TextureNode = CreateTextureNode<UInterchangeTexture2DNode>(Connect.UpstreamNode))
	{
		//By default set the output of a texture to RGB
		FString OutputChannel{ TEXT("RGB") };

		if(Connect.UpstreamNode->getType() == mx::Type::Vector4 || Connect.UpstreamNode->getType() == mx::Type::Color4)
		{
			OutputChannel = TEXT("RGBA");
		}
		else if(Connect.UpstreamNode->getType() == mx::Type::Float)
		{
			OutputChannel = TEXT("R");
		}

		UInterchangeShaderNode* TextureShaderNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TextureSample::Name.ToString());
		TextureShaderNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSample::Inputs::Texture.ToString()), TextureNode->GetUniqueID());
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(Connect.ParentShaderNode, Connect.InputChannelName, TextureShaderNode->GetUniqueID(), OutputChannel);
	}
	else
	{
		AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput(mx::NodeGroup::Texture2D::Inputs::Default), Connect.InputChannelName, Connect.ParentShaderNode);
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectConvertInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	//In case of an upwards conversion, let's do an append, downwards a mask, otherwise leave as is
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in");
	const std::string& NodeType = Connect.UpstreamNode->getType();
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
		UE_LOG(LogInterchangeImport, Warning, TEXT("<convert> node has unsupported types."));
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
			MaskShaderNode = CreateMaskShaderNode(0b1110, Connect.UpstreamNode->getName().c_str());
		}
		else if(NodeN == '2')
		{
			MaskShaderNode = CreateMaskShaderNode(0b1100, Connect.UpstreamNode->getName().c_str());
		}
		else
		{
			MaskShaderNode = CreateMaskShaderNode(0b1000, Connect.UpstreamNode->getName().c_str());
		}

		AddAttributeFromValueOrInterface(Input, TEXT("Input"), MaskShaderNode);

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, MaskShaderNode->GetUniqueID());
	}
	else // Append 
	{
		//same as dot, just connect the next output to this parent input
		SetAttributeNewName(Input, TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
		ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName }, Connect.ParentShaderNode);

		//No need to create a node, since the input and the node have the same channels, we just check if there's a value
		if(NodeN == InputN || (NodeN < '2' && InputN < '2'))
		{
			AddAttributeFromValueOrInterface(Input, Connect.InputChannelName, Connect.ParentShaderNode);
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
			InputsToCreate.Add({ "in2", FAttributeValueArray{{"type","float"}, {"value","1"}} });
		}

		//copy "in" input into "in1" and create "amount" input as "in2" with the value 360 because UE's angle is a value between [0,1]
		mx::NodePtr CombineNode = CreateNode(Connect.UpstreamNode->getParent()->asA<mx::NodeGraph>(),
											 Connect.UpstreamNode->getName().c_str(),
											 Category.c_str(),
											 InputsToCopy, // rename input to match <divide> input "in1"
											 InputsToCreate);

		//We set the type and an output to find it during connect node resolution phase
		//Because the case Combine2 vector2->vector3 has no nodedef in the standard library
		CombineNode->setType(Connect.UpstreamNode->getType());
		if(!CombineNode->getOutput("out"))
		{
			CombineNode->addOutput("out", Connect.UpstreamNode->getType());
		}

		// Input now points to the new node
		Input->setNodeName(CombineNode->getName());
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectIfGreaterInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("If"));
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AEqualsB
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in2");
	if(Input)
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, TEXT("AEqualsB"), NodeIf);

		//Let's add a new input that is a copy of in2 to connect it to the equal input
		mx::InputPtr Input3 = Connect.UpstreamNode->addInput("in3");
		Input3->copyContentFrom(Input);
		SetAttributeNewName(Input3, "AEqualsB");
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectIfGreaterEqInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("If"));
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AEqualsB
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in1");
	if(Input)
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, TEXT("AEqualsB"), NodeIf);

		//Let's add a new input that is a copy of in2 to connect it to the equal input
		mx::InputPtr Input3 = Connect.UpstreamNode->addInput("in3");
		Input3->copyContentFrom(Input);
		SetAttributeNewName(Input3, "AEqualsB");
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectIfEqualInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("If"));
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AGreaterThanB
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in2");
	if(Input)
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, TEXT("AGreaterThanB"), NodeIf);

		//Let's add a new input that is a copy of in2 to connect it to the equal input
		mx::InputPtr Input3 = Connect.UpstreamNode->addInput("in3");
		Input3->copyContentFrom(Input);
		SetAttributeNewName(Input3, "AGreaterThanB");
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectOutsideInputToOutput(const FConnectNode& Connect)
{
	//in * (1 - mask)
	UInterchangeShaderNode* NodeMultiply = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("Multiply"));
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), TEXT("A"), NodeMultiply);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeMultiply->GetUniqueID());

	UInterchangeShaderNode* NodeOneMinus = CreateShaderNode(Connect.UpstreamNode->getName().c_str() + FString(TEXT("_OneMinus")), TEXT("OneMinus"));
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("mask"), TEXT("Input"), NodeOneMinus);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeMultiply, TEXT("B"), NodeOneMinus->GetUniqueID());
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::ConnectGeometryInputToOutput(const FConnectNode& Connect, const FString& ShaderType, const FString& TransformShaderType, const FString& TransformInput, const FString& TransformSourceType, int32 TransformSource, const FString& TransformType, int32 TransformSDestination, bool bIsVector)
{
	// MaterialX defines the space as: object, model, world
	// model: The local coordinate space of the geometry, before any local deformations or global transforms have been applied.
	// object: The local coordinate space of the geometry, after local deformations have been applied, but before any global transforms.
	// world : The global coordinate space of the geometry, after local deformationsand global transforms have been applied.

	// In case of model/object we need to add a TransformVector from world to local space
	UInterchangeShaderNode* GeometryNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), ShaderType);

	UInterchangeShaderNode* NodeToConnectTo = Connect.ParentShaderNode;
	FString InputToConnectTo = Connect.InputChannelName;

	mx::InputPtr InputSpace = Connect.UpstreamNode->getInput("space");

	//the default space defined by the nodedef is "object"
	bool bIsObjectSpace = (InputSpace && InputSpace->getValueString() != "world") || !InputSpace;

	// We transform to Tangent Space only for Vector nodes
	if(bTangentSpaceInput && bIsVector)
	{
		using namespace UE::Interchange::Materials::Standard::Nodes;
		UInterchangeShaderNode* TransformTSNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str() + FString(TEXT("_TransformTS")), TransformShaderType);
		EMaterialVectorCoordTransformSource SpaceSource = bIsObjectSpace ? EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_Local : EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World;
		TransformTSNode->AddInt32Attribute(TransformSourceType, SpaceSource);
		TransformTSNode->AddInt32Attribute(TransformType, EMaterialVectorCoordTransform::TRANSFORM_Tangent);
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, TransformTSNode->GetUniqueID());
		NodeToConnectTo = TransformTSNode;
		InputToConnectTo = TransformInput; //Same a TransformVector
	}

	if(bIsObjectSpace)
	{
		UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Transform")), TransformShaderType);
		TransformNode->AddInt32Attribute(TransformSourceType, TransformSource);
		TransformNode->AddInt32Attribute(TransformType, TransformSDestination);
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, TransformNode->GetUniqueID());
		NodeToConnectTo = TransformNode;
		InputToConnectTo = TransformInput;
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, GeometryNode->GetUniqueID());

	return GeometryNode;
}

void FMaterialXSurfaceShaderAbstract::ConnectPositionInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* UnitNode = ConnectGeometryInputToOutput(Connect, TEXT("Multiply"),
																	TransformPosition::Name.ToString(),
																	TransformPosition::Inputs::Input.ToString(),
																	TransformPosition::Attributes::TransformSourceType.ToString(), EMaterialPositionTransformSource::TRANSFORMPOSSOURCE_World,
																	TransformPosition::Attributes::TransformType.ToString(), EMaterialPositionTransformSource::TRANSFORMPOSSOURCE_Local,
																	false);

	// In case of the position node, it seems that the unit is different, we assume for now a conversion from mm -> m, even if UE by default is cm
	// See standard_surface_marble_solid file, especially on the fractal3d node
	UInterchangeShaderNode* PositionNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Position")), TEXT("WorldPosition"));
	UnitNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TEXT("B")), 0.001f);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(UnitNode, TEXT("A"), PositionNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectNormalInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	ConnectGeometryInputToOutput(Connect, TEXT("VertexNormalWS"),
								 TransformVector::Name.ToString(),
								 TransformVector::Inputs::Input.ToString(),
								 TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World,
								 TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Local);
}

void FMaterialXSurfaceShaderAbstract::ConnectTangentInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	ConnectGeometryInputToOutput(Connect, TEXT("VertexTangentWS"),
								 TransformVector::Name.ToString(),
								 TransformVector::Inputs::Input.ToString(),
								 TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World,
								 TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Local);
}

void FMaterialXSurfaceShaderAbstract::ConnectBitangentInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* BitangentNode = ConnectGeometryInputToOutput(Connect, TEXT("CrossProduct"),
																		 TransformVector::Name.ToString(),
																		 TransformVector::Inputs::Input.ToString(),
																		 TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World,
																		 TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Local);

	UInterchangeShaderNode* NormalNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Normal")), TEXT("VertexNormalWS"));
	UInterchangeShaderNode* TangentNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Tangent")), TEXT("VertexTangentWS"));

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(BitangentNode, TEXT("A"), NormalNode->GetUniqueID());
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(BitangentNode, TEXT("B"), TangentNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectTimeInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* TimeNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TEXT("Time"));
	TimeNode->AddBooleanAttribute(Time::Attributes::OverridePeriod.ToString(), true);

	float FPS;
	mx::InputPtr Input = Connect.UpstreamNode->getInput("fps");

	//Take the default value from the node definition
	if(!Input)
	{
		Input = Connect.UpstreamNode->getNodeDef(mx::EMPTY_STRING, true)->getInput("fps");
	}

	FPS = mx::fromValueString<float>(Input->getValueString());

	//UE is a period
	TimeNode->AddFloatAttribute(Time::Attributes::Period.ToString(), 1.f / FPS);

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TimeNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectNoise3DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// MaterialX defines the Noise3d as Perlin Noise which is multiplied by the Amplitude then Added to Pivot
	UInterchangeShaderNode* NoiseNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), Noise::Name.ToString());
	NoiseNode->AddInt32Attribute(Noise::Attributes::Function.ToString(), ENoiseFunction::NOISEFUNCTION_GradientTex);
	NoiseNode->AddBooleanAttribute(Noise::Attributes::Turbulence.ToString(), false);
	NoiseNode->AddFloatAttribute(Noise::Attributes::OutputMin.ToString(), 0);

	UInterchangeShaderNode* NodeToConnect = NoiseNode;

	// Multiply Node
	auto ConnectNodeToInput = [&](mx::InputPtr Input, UInterchangeShaderNode* NodeToConnectTo, const FString& ShaderType, int32 IndexAttrib) -> UInterchangeShaderNode*
	{
		if(!Input)
		{
			return nullptr;
		}

		const FString ShaderNodeName = Connect.UpstreamNode->getName().c_str() + FString{ TEXT("_") } + ShaderType;
		UInterchangeShaderNode* ShaderNode = CreateShaderNode(ShaderNodeName, ShaderType);

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, TEXT("A"), NodeToConnectTo->GetUniqueID());

		// Connect the amplitude node to the shader node not the noise
		// it will be handle during the upstream-downstream connection phase
		// The index is here for the unicity of the attribute
		Connect.UpstreamNode->setAttribute(mx::Attributes::ParentName + std::to_string(IndexAttrib), TCHAR_TO_ANSI(*ShaderNodeName));
		AddAttributeFromValueOrInterface(Input, TEXT("B"), ShaderNode);

		return ShaderNode;
	};

	if(UInterchangeShaderNode* MultiplyNode = ConnectNodeToInput(Connect.UpstreamNode->getInput("amplitude"), NoiseNode, TEXT("Multiply"), 0))
	{
		NodeToConnect = MultiplyNode;
	}


	if(UInterchangeShaderNode* AddNode = ConnectNodeToInput(Connect.UpstreamNode->getInput("pivot"), NodeToConnect, TEXT("Add"), 1))
	{
		NodeToConnect = AddNode;
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeToConnect->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectCellNoise3DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// Let's use a vector noise for this one, the only one that is close to MaterialX implementation
	UInterchangeShaderNode* NoiseNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), VectorNoise::Name.ToString());
	NoiseNode->AddInt32Attribute(VectorNoise::Attributes::Function.ToString(), EVectorNoiseFunction::VNF_CellnoiseALU);

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NoiseNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectWorleyNoise3DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	//Also called Voronoi, the implementation is a bit different in UE, especially we don't have access to the jitter
	UInterchangeShaderNode* NoiseNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), Noise::Name.ToString());
	NoiseNode->AddInt32Attribute(Noise::Attributes::Function.ToString(), ENoiseFunction::NOISEFUNCTION_VoronoiALU);

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NoiseNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectHeightToNormalInputToOutput(const FConnectNode& Connect)
{
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		// Image node will become this node
		if(mx::NodePtr ConnectedNode = Input->getConnectedNode();
		   ConnectedNode && ConnectedNode->getCategory() == mx::Category::Image)
		{
			//we need to copy the content of the image node to this node
			Connect.UpstreamNode->copyContentFrom(ConnectedNode);

			// the copy overwrite every attribute of the node, so we need to get them back, essentially the type and the renaming
			// the output is always a vec3
			Connect.UpstreamNode->setType(mx::Type::Vector3);

			SetMatchingInputsNames(Connect.UpstreamNode);

			mx::NodeGraphPtr Graph = Connect.UpstreamNode->getParent()->asA<mx::NodeGraph>();
			Graph->removeNode(ConnectedNode->getName());

			using namespace UE::Interchange::Materials::Standard::Nodes;

			if(UInterchangeTextureNode* TextureNode = CreateTextureNode<UInterchangeTexture2DNode>(Connect.UpstreamNode))
			{
				UInterchangeShaderNode* HeightMapNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), NormalFromHeightMap::Name.ToString());
				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, HeightMapNode->GetUniqueID());

				const FString TextureNodeName = Connect.UpstreamNode->getName().c_str() + FString{ "_texture" };
				UInterchangeShaderNode* TextureShaderNode = CreateShaderNode(TextureNodeName, TextureObject::Name.ToString());
				TextureShaderNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureObject::Inputs::Texture.ToString()), TextureNode->GetUniqueID());
				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(HeightMapNode, NormalFromHeightMap::Inputs::HeightMap.ToString(), TextureShaderNode->GetUniqueID());

				AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("scale"), NormalFromHeightMap::Inputs::Intensity.ToString(), HeightMapNode);
			}
			else
			{
				AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput(mx::NodeGroup::Texture2D::Inputs::Default), Connect.InputChannelName, Connect.ParentShaderNode);
			}
		}
		else
		{
			// For the moment it doesn't make sense to plug a value to it, so let's plug directly the child to the parent, in the future we could implement a Sobel and handle a multi output
			SetAttributeNewName(Input, TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
			ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName }, Connect.ParentShaderNode);
		}
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectBlurInputToOutput(const FConnectNode& Connect)
{
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		// Image node will become this node
		if(mx::NodePtr ConnectedNode = Input->getConnectedNode();
		   ConnectedNode && ConnectedNode->getCategory() == mx::Category::Image)
		{
			std::string NodeType = Connect.UpstreamNode->getType();

			//we need to copy the content of the image node to this node
			Connect.UpstreamNode->copyContentFrom(ConnectedNode);

			//the copy overwrites every attribute of the node, so we need to get them back, essentially the type and the renaming
			Connect.UpstreamNode->setType(NodeType);

			SetMatchingInputsNames(Connect.UpstreamNode);

			mx::NodeGraphPtr Graph = Connect.UpstreamNode->getParent()->asA<mx::NodeGraph>();
			Graph->removeNode(ConnectedNode->getName());

			using namespace UE::Interchange::Materials::Standard::Nodes;

			if(UInterchangeTextureNode* TextureNode = CreateTextureNode<UInterchangeTextureBlurNode>(Connect.UpstreamNode))
			{
				FString OutputChannel{ TEXT("RGB") };

				if(NodeType == mx::Type::Vector4 || NodeType == mx::Type::Color4)
				{
					OutputChannel = TEXT("RGBA");
				}
				else if(NodeType == mx::Type::Float)
				{
					OutputChannel = TEXT("R");
				}

				UInterchangeShaderNode* TextureShaderNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TextureSampleBlur::Name.ToString());
				TextureShaderNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSampleBlur::Inputs::Texture.ToString()), TextureNode->GetUniqueID());
				UInterchangeShaderPortsAPI::ConnectOuputToInputByName(Connect.ParentShaderNode, Connect.InputChannelName, TextureShaderNode->GetUniqueID(), OutputChannel);

				if(mx::InputPtr InputKernel = Connect.UpstreamNode->getInput("filtertype"))
				{
					// By default TextureSampleBox uses gaussian filter
					if(InputKernel->getValueString() == "box")
					{
						TextureShaderNode->AddInt32Attribute(TextureSampleBlur::Attributes::Filter.ToString(), int32(EMaterialXTextureSampleBlurFilter::Box));
					}
				}

				if(mx::InputPtr InputKernel = Connect.UpstreamNode->getInput("size"))
				{
					if(InputKernel->hasValueString())
					{
						float KernelSize = mx::fromValueString<float>(InputKernel->getValueString());
						constexpr float Kernel1x1 = 0.f / 3.f;
						constexpr float Kernel3x3 = 1.f / 3.f;
						constexpr float Kernel5x5 = 2.f / 3.f;
						constexpr float Kernel7x7 = 3.f / 3.f;
						TextureShaderNode->AddInt32Attribute(TextureSampleBlur::Attributes::KernelSize.ToString(),
															 FMath::IsNearlyEqual(KernelSize, Kernel1x1) ? int32(EMAterialXTextureSampleBlurKernel::Kernel1) :
															 KernelSize <= Kernel3x3 ? int32(EMAterialXTextureSampleBlurKernel::Kernel3) :
															 KernelSize <= Kernel5x5 ? int32(EMAterialXTextureSampleBlurKernel::Kernel5) :
															 KernelSize <= Kernel7x7 ? int32(EMAterialXTextureSampleBlurKernel::Kernel7) :
															 int32(EMAterialXTextureSampleBlurKernel::Kernel1));
					}
					else
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("<%s>: input 'size' must have a value."), ANSI_TO_TCHAR(Connect.UpstreamNode->getName().c_str()));
					}
				}
			}
			else
			{
				AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput(mx::NodeGroup::Texture2D::Inputs::Default), Connect.InputChannelName, Connect.ParentShaderNode);
			}
		}
		else
		{
			// For a blur it doesn't make sense if there's no image input
			SetAttributeNewName(Input, TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
			ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName }, Connect.ParentShaderNode);
		}
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectTexCoordInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* TexCoord = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), TextureCoordinate::Name.ToString());
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("index"))
	{
		TexCoord->AddInt32Attribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureCoordinate::Inputs::Index.ToString()), mx::fromValueString<int>(Input->getValueString()));
	}
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TexCoord->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput(const FConnectNode& Connect)
{
	if(Connect.OutputName == TEXT("outx") || Connect.OutputName == TEXT("outr"))
	{
		UInterchangeShaderNode* OutXNode = CreateMaskShaderNode(0b1000, Connect.UpstreamNode->getName().c_str(), Connect.OutputName);
		if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
		{
			AddAttributeFromValueOrInterface(Input, GetInputName(Input), OutXNode);
		}
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, OutXNode->GetUniqueID());
	}
	else if(Connect.OutputName == TEXT("outy") || Connect.OutputName == TEXT("outg"))
	{
		UInterchangeShaderNode* OutYNode = CreateMaskShaderNode(0b0100, Connect.UpstreamNode->getName().c_str(), Connect.OutputName);
		if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
		{
			AddAttributeFromValueOrInterface(Input, GetInputName(Input), OutYNode);
		}
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, OutYNode->GetUniqueID());
	}
	else if(Connect.OutputName == TEXT("outz") || Connect.OutputName == TEXT("outb"))
	{
		UInterchangeShaderNode* OutZNode = CreateMaskShaderNode(0b0010, Connect.UpstreamNode->getName().c_str(), Connect.OutputName);
		if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
		{
			AddAttributeFromValueOrInterface(Input, GetInputName(Input), OutZNode);
		}
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, OutZNode->GetUniqueID());
	}
	else if(Connect.OutputName == TEXT("outw") || Connect.OutputName == TEXT("outa"))
	{
		UInterchangeShaderNode* OutWNode = CreateMaskShaderNode(0b0001, Connect.UpstreamNode->getName().c_str(), Connect.OutputName);
		if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
		{
			AddAttributeFromValueOrInterface(Input, GetInputName(Input), OutWNode);
		}
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, OutWNode->GetUniqueID());
	}
	else
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("output <%s> not defined in <%s>."), *Connect.OutputName, ANSI_TO_TCHAR(Connect.UpstreamNode->getCategory().c_str()));
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectSwizzleInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* SwizzleNode = CreateShaderNode(Connect.UpstreamNode->getName().c_str(), Swizzle::Name.ToString());
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), SwizzleNode);
	}
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("channels"))
	{
		SwizzleNode->AddStringAttribute(Swizzle::Attributes::Channels.ToString(), Input->getValueString().c_str());
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, SwizzleNode->GetUniqueID());
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::CreateMaskShaderNode(uint8 RGBA, const FString& NodeName, const FString& OutputName)
{
	bool bR = (0b1000 & RGBA) >> 3;
	bool bG = (0b0100 & RGBA) >> 2;
	bool bB = (0b0010 & RGBA) >> 1;
	bool bA = (0b0001 & RGBA) >> 0;
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* MaskShaderNode = CreateShaderNode(NodeName, Mask::Name.ToString(), OutputName);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::R.ToString(), bR);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::G.ToString(), bG);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::B.ToString(), bB);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::A.ToString(), bA);

	return MaskShaderNode;
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::CreateShaderNode(const FString& NodeName, const FString& ShaderType, const FString& OutputName)
{
	UInterchangeShaderNode* Node;

	const FString NodeUID = UInterchangeShaderNode::MakeNodeUid(NodeName + TEXT('_') + OutputName, FStringView{});

	//Test directly in the NodeContainer, because the ShaderNodes can be altered during the node graph either by the parent (dot/normalmap),
	//or by putting an intermediary node between the child and the parent (tiledimage)
	if(Node = const_cast<UInterchangeShaderNode*>(Cast<UInterchangeShaderNode>(NodeContainer.GetNode(NodeUID))); !Node)
	{
		Node = NewObject<UInterchangeShaderNode>(&NodeContainer);
		Node->InitializeNode(NodeUID, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		NodeContainer.AddNode(Node);
		Node->SetCustomShaderType(ShaderType);

		ShaderNodes.Add({ NodeName, OutputName }, Node);
	}

	return Node;
}

UInterchangeFunctionCallShaderNode* FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode(const FString& NodeName, const FString& FunctionPath, const FString& OutputName)
{
	UInterchangeFunctionCallShaderNode* Node;

	const FString NodeUID = UInterchangeShaderNode::MakeNodeUid(NodeName  + TEXT('_') + OutputName, FStringView{});

	if(Node = const_cast<UInterchangeFunctionCallShaderNode*>(Cast<UInterchangeFunctionCallShaderNode>(NodeContainer.GetNode(NodeUID))); !Node)
	{
		Node = NewObject<UInterchangeFunctionCallShaderNode>(&NodeContainer);
		Node->InitializeNode(NodeUID, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		Node->SetCustomMaterialFunction(FunctionPath);
		NodeContainer.AddNode(Node);

		ShaderNodes.Add({ NodeName, OutputName}, Node);
	}

	return Node;
}

UInterchangeFunctionCallShaderNode* FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode(const FString& NodeName, uint8 EnumType, uint8 EnumValue, const FString& OutputName)
{
	UInterchangeFunctionCallShaderNode* Node;

	const FString NodeUID = UInterchangeShaderNode::MakeNodeUid(NodeName + TEXT('_') + OutputName, FStringView{});

	if(Node = const_cast<UInterchangeFunctionCallShaderNode*>(Cast<UInterchangeFunctionCallShaderNode>(NodeContainer.GetNode(NodeUID))); !Node)
	{
		Node = NewObject<UInterchangeFunctionCallShaderNode>(&NodeContainer);
		Node->InitializeNode(NodeUID, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		//this is just a dummy path name so the Generic Material Pipeline consider it as a FunctionCallShader but where in fact the path is given by an enum
		Node->SetCustomMaterialFunction(TEXT("/Game/Default.Default"));
		Node->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumType, EnumType);
		Node->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumValue, EnumValue);
		NodeContainer.AddNode(Node);

		ShaderNodes.Add({ NodeName, OutputName }, Node);
	}

	return Node;
}

const FString& FMaterialXSurfaceShaderAbstract::GetMatchedInputName(MaterialX::NodePtr Node, MaterialX::InputPtr Input) const
{
	FMaterialXManager& Manager = FMaterialXManager::GetInstance();

	if(Input)
	{
		const FString NodeCategory{ Node->getCategory().c_str() };
		const FString InputName{ GetInputName(Input) };	

		if(const FString* Result = Manager.FindMatchingInput(NodeCategory, InputName, Node->getNodeDef(mx::EMPTY_STRING, true)->getNodeGroup().c_str()))
		{
			return *Result;
		}
		else if((Result = Manager.FindMatchingInput(NodeCategory, InputName)))
		{
			return *Result;
		}
		else if((Result = Manager.FindMatchingInput(EmptyString, InputName)))
		{
			return *Result;
		}
	}

	return EmptyString;
}

FString FMaterialXSurfaceShaderAbstract::GetInputName(MaterialX::InputPtr Input) const
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

FString FMaterialXSurfaceShaderAbstract::GetFilePrefix(MaterialX::ElementPtr Element) const
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

FLinearColor FMaterialXSurfaceShaderAbstract::GetVector(MaterialX::InputPtr Input) const
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
		ensureMsgf(false, TEXT("Input type can only be a vectorN."));
	}

	return LinearColor;
}

FString FMaterialXSurfaceShaderAbstract::GetAttributeParentName(MaterialX::NodePtr Node) const
{
	FString ParentName;
	const mx::StringVec& Attributes = Node->getAttributeNames();

	// For consistency the parent attribute has an index attach to it to ensure unicity
	// Attributes are set in order, we only need to take the first one (it will be remove after that)
	for(const std::string& Attrib : Attributes)
	{
		if(Attrib.find(mx::Attributes::ParentName) != std::string::npos)
		{
			ParentName = Node->getAttribute(Attrib).c_str();
			Node->removeAttribute(Attrib);
			break;
		}
	}

	return ParentName.IsEmpty() ? Node->getName().c_str() : ParentName;
}

void FMaterialXSurfaceShaderAbstract::RegisterConnectNodeOutputToInputDelegates()
{
	MatchingConnectNodeDelegates.Add(mx::Category::Constant,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectConstantInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Extract,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectExtractInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Dot,				FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectDotInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::NormalMap,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectDotInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TransformPoint,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTransformPositionInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TransformVector, FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTransformVectorInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TransformNormal, FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTransformVectorInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Rotate3D,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectRotate3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Image,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectImageInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Convert,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectConvertInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::IfGreater,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectIfGreaterInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::IfGreaterEq,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectIfGreaterEqInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::IfEqual,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectIfEqualInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Outside,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectOutsideInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Position,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectPositionInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Normal,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectNormalInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Tangent,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTangentInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Bitangent,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectBitangentInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Time,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTimeInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Noise3D,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectNoise3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::CellNoise3D,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectCellNoise3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::WorleyNoise3D,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectWorleyNoise3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Blur,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectBlurInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::HeightToNormal,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectHeightToNormalInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Separate2,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Separate3,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Separate4,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TexCoord,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTexCoordInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Swizzle,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSwizzleInputToOutput));
}

void FMaterialXSurfaceShaderAbstract::SetMatchingInputsNames(MaterialX::NodePtr Node) const
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
					SetAttributeNewName(Input, TCHAR_TO_UTF8(*Name));
				}
			}
		}
	}
}

void FMaterialXSurfaceShaderAbstract::SetAttributeNewName(MaterialX::InputPtr Input, const char* NewName) const
{
	Input->setAttribute(mx::Attributes::NewName, NewName);
}

#undef LOCTEXT_NAMESPACE
#endif