// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "Engine/TextureDefines.h"
#include "InterchangeShaderGraphNode.h"
#include "InterchangeTranslatorBase.h"
#include "InterchangeTexture2DNode.h"
#include "MaterialXBase.h"
#include "MaterialXManager.h"

class FMaterialXSurfaceShaderAbstract : public FMaterialXBase
{
protected:

	static FString EmptyString;

	// MaterialX states the default output name of the different nodes is 'out'.
	static FString DefaultOutput;

	friend class FMaterialXSurfaceMaterial;

	struct FConnectNode
	{
		// The MaterialX node of a given type used to create the appropriate shader node.
		MaterialX::NodePtr UpstreamNode;

		// The shader node to connect to.
		UInterchangeShaderNode* ParentShaderNode;

		// The input of the ParentShaderNode to connect to.
		FString InputChannelName;

		// The output name of the MaterialX node. The default name is 'out' as stated by the standard library.
		FString OutputName{ DefaultOutput };
	};
		
	DECLARE_DELEGATE_OneParam(FOnConnectNodeOutputToInput, const FConnectNode&);

	FMaterialXSurfaceShaderAbstract(UInterchangeBaseNodeContainer & BaseNodeContainer);

	/**
	 * Add an attribute to a shader node from the given MaterialX input. Only floats and linear colors are supported.
	 *
	 * @param Input - The MaterialX input to retrieve and add the value from. Must be of type float/color/vector.
	 * @param InputChannelName - The name of the shader node's input to add the attribute.
	 * @param ShaderNode - The shader node to which we want to add the attribute.
	 *
	 * @return true if the attribute was successfully added.
	 */
	bool AddAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode);

	/**
	 * Add an attribute to a shader node from the given MaterialX input if that input has either a value or an interface name.
	 *
	 * @param Input - The MaterialX input to retrieve and add the value from. Must be of type float/color/vector.
	 * @param InputChannelName - The name of the shader node's input to add the attribute.
	 * @param ShaderNode - The shader node to which we want to add the attribute.
	 *
	 * @return true if the attribute was successfully added.
	 */
	bool AddAttributeFromValueOrInterface(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode);

	/**
	 * Add a bool attribute to a shader node only if its value taken from the input is not equal to its default value. Return false if the attribute does not exist or if we cannot add it
	 *
	 * @param Input - The MaterialX input to retrieve and add the value from, must be of type bool
	 * @param InputChannelName - The name of the shader node's input to add the attribute
	 * @param ShaderNode - The shader node to which we want to add the attribute
	 * @param DefaultValue - the default value to test the input against
	 *
	 * @return true if the attribute was successfully added
	 */
	bool AddBooleanAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode);

	/**
	 * Add a float attribute to a shader node only if its value taken from the input is not equal to its default value. Return false if the attribute does not exist or if we cannot add it
	 *
	 * @param Input - The MaterialX input to retrieve and add the value from. Must be of type float.
	 * @param InputChannelName - The name of the shader node's input to add the attribute.
	 * @param ShaderNode - The shader node to which we want to add the attribute.
	 * @param DefaultValue - the default value to test the input against.
	 *
	 * @return true if the attribute was successfully added.
	 */
	bool AddFloatAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, float DefaultValue = std::numeric_limits<float>::max());

	/**
	 * Add an FLinearColor attribute to a shader node only if its value taken from the input is not equal to its default value. Return false if the attribute does not exist or if we cannot add it.
	 *
	 * @param Input - The MaterialX input to retrieve and add the value from. Must be of type color.
	 * @param InputChannelName - The name of the shader node's input to add the attribute.
	 * @param ShaderNode - The shader node to which we want to add the attribute.
	 * @param DefaultValue - the default value to test the input against.
	 *
	 * @return true if the attribute was successfully added.
	 */
	bool AddLinearColorAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FLinearColor& DefaultValue = FLinearColor{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});

	/**
	 * Add an FLinearColor attribute to a shader node only if its value taken from the input is not equal to its default value. Return false if the attribute does not exist or if we cannot add it.
	 *
	 * @param Input - The MaterialX input to retrieve and add the value from. Must be of type vector.
	 * @param InputChannelName - The name of the shader node's input to add the attribute.
	 * @param ShaderNode - The shader node to which we want to add the attribute.
	 * @param DefaultValue - the default value to test the input against.
	 *
	 * @return true if the attribute was successfully added.
	 */
	bool AddVectorAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FVector4f& DefaultValue = FVector4f{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() });

	/**
	 * Connect an output either from a node name or a node graph from a MaterialX input to the shader node.
	 * 
	 * @param InputName - The name of the input of the SurfaceShaderNode to retrieve
	 * @param ShaderNode - The Interchange shader node to connect the MaterialX's node or node graph to
	 * @param InputShaderName - The name of the input of the shader node to connect to
	 * @param DefaultValue - The default value of the MaterialX input
	 * @param bIsTangentSpaceInput - Set the tangent space along the path of an input
	 */
	template<typename T>
	bool ConnectNodeOutputToInput(const char* InputName, UInterchangeShaderNode* ShaderNode, const FString& InputShaderName, T DefaultValue, bool bIsTangentSpaceInput = false)
	{
		MaterialX::DocumentPtr Document = SurfaceShaderNode->getDocument();
		MaterialX::InputPtr Input = GetInput(SurfaceShaderNode, InputName);

		TGuardValue<bool>InputTypeBeingProcessedGuard(bTangentSpaceInput, bIsTangentSpaceInput);

		bool bIsConnected = ConnectNodeGraphOutputToInput(Input, ShaderNode, InputShaderName);

		if(!bIsConnected)
		{
			bIsConnected = ConnectNodeNameOutputToInput(Input, ShaderNode, InputShaderName);
			if(!bIsConnected)
			{
				// only handle float, linear color and vector here, for other types, the child should handle them as it is most likely not an input but a parameter to set in Interchange
				if constexpr(std::is_same_v<decltype(DefaultValue), float>)
				{
					bIsConnected = AddFloatAttribute(Input, InputShaderName, ShaderNode, DefaultValue);
				}
				else if constexpr(std::is_same_v<decltype(DefaultValue), FLinearColor>)
				{
					bIsConnected = AddLinearColorAttribute(Input, InputShaderName, ShaderNode, DefaultValue);
				}
				else if constexpr(std::is_same_v<decltype(DefaultValue), FVector4f> || std::is_same_v<decltype(DefaultValue), FVector3f> || std::is_same_v<decltype(DefaultValue), FVector2f>)
				{					
					bIsConnected = AddVectorAttribute(Input, InputShaderName, ShaderNode, DefaultValue);
				}
			}
		}

		return bIsConnected;
	};

	/**
	 * Connect an ouput in the NodeGraph to the ShaderGraph.
	 *
	 * @param InputToNodeGraph - The input from the standard surface to retrieve the output in the NodeGraph.
	 * @param ShaderNode - The Interchange shader node to connect the MaterialX's node graph to.
	 * @param ParentInputName - The name of the input of the shader node that we want the node graph to be connected to.
	 *
	 * @return true if the given input is attached to one of the outputs of a node graph.
	 */
	bool ConnectNodeGraphOutputToInput(MaterialX::InputPtr InputToNodeGraph, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName);

	/**
	 * Create and connect the output of a MaterialX node that has already a matching in UE to a shader node.
	 * If not, search for a registered delegate.
	 *
	 * @param Node - The MaterialX node of a given type used to create the appropriate shader node.
	 * @param ParentShaderNode - The shader node to connect to.
	 * @param InputChannelName - The input of the ParentShaderNode to connect to.
	 *
	 * @return true if a shader node has been successfully created and is connected to the given input.
	 */
	bool ConnectMatchingNodeOutputToInput(const FConnectNode& Connect);

	/**
	 * Create and connect manually the output of a MaterialX node to a shader node.
	 *
	 * @param Edge - The MaterialX edge that has the current node, its parent and the input bridge between the two.
	 * @param ParentShaderNode - The shader node to connect to.
	 * @param InputChannelName - The input of the ParentShaderNode to connect to.
	 *
	 * @return true if a shader node has been successfully created and is connected to the given input.
	 */
	void ConnectNodeCategoryOutputToInput(const MaterialX::Edge& Edge, UInterchangeShaderNode* ParentShaderNode, const FString& InputChannelName, const FString& OutputName = TEXT("out"));

	/**
	 * Create and connect a node name directly connected from an input to a shader node.
	 *
	 * @param Node - The MaterialX node of a given type used to create the appropriate shader node.
	 * @param ParentShaderNode - The shader node to connect to.
	 * @param InputChannelName - The input of the ParentShaderNode to connect to.
	 *
	 * @return true if a shader node has been successfully created and is connected to the given input.
	 */
	bool ConnectNodeNameOutputToInput(MaterialX::InputPtr Input, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName);

	/** Begin Connect MaterialX nodes*/

	/** <constant> */
	void ConnectConstantInputToOutput(const FConnectNode& Connect);

	/** <extract> */
	void ConnectExtractInputToOutput(const FConnectNode& Connect);

	/** <dot> */
	void ConnectDotInputToOutput(const FConnectNode& Connect);

	/** <transformpoint> */
	void ConnectTransformPositionInputToOutput(const FConnectNode& Connect);

	/** <transformvector> */
	void ConnectTransformVectorInputToOutput(const FConnectNode& Connect);

	/** <rotate3d> */
	void ConnectRotate3DInputToOutput(const FConnectNode& Connect);

	/** <image> */
	void ConnectImageInputToOutput(const FConnectNode& Connect);

	/** <convert> */
	void ConnectConvertInputToOutput(const FConnectNode& Connect);

	/** <ifgreater> */
	void ConnectIfGreaterInputToOutput(const FConnectNode& Connect);

	/** <ifgreatereq> */
	void ConnectIfGreaterEqInputToOutput(const FConnectNode& Connect);

	/** <ifequal> */
	void ConnectIfEqualInputToOutput(const FConnectNode& Connect);

	/** <outside> */
	void ConnectOutsideInputToOutput(const FConnectNode& Connect);

	/** <position> */
	void ConnectPositionInputToOutput(const FConnectNode& Connect);

	/** <normal> */
	void ConnectNormalInputToOutput(const FConnectNode& Connect);

	/** <tangent> */
	void ConnectTangentInputToOutput(const FConnectNode& Connect);

	/** <bitangent> */
	void ConnectBitangentInputToOutput(const FConnectNode& Connect);

	/** <time> */
	void ConnectTimeInputToOutput(const FConnectNode& Connect);

	/** <noise3d> */
	void ConnectNoise3DInputToOutput(const FConnectNode& Connect);

	/** <cellnoise3d> */
	void ConnectCellNoise3DInputToOutput(const FConnectNode& Connect);

	/** <worleynoise3d> */
	void ConnectWorleyNoise3DInputToOutput(const FConnectNode& Connect);

	/** <heighttonormal> */
	void ConnectHeightToNormalInputToOutput(const FConnectNode& Connect);

	/** <blur> */
	void ConnectBlurInputToOutput(const FConnectNode& Connect);

	/** <texcoord> */
	void ConnectTexCoordInputToOutput(const FConnectNode& Connect);

	/** <separate2/3/4> */
	void ConnectSeparateInputToOutput(const FConnectNode& Connect);

	/** <swizzle> */
	void ConnectSwizzleInputToOutput(const FConnectNode& Connect);

	/** End Connect MaterialX nodes*/

	/**
	 * Create a ComponentMask shader node.
	 *
	 * @param RGBA - The mask component. For example: 0b1011 -> Only RBA are toggled
	 * @param NodeName - the name of the shader node.
	 * @param OutputName - the name of the output of the MaterialX node. The default name is 'out' as stated by the standard library.
	 * @return The ComponentMask node.
	 */
	UInterchangeShaderNode* CreateMaskShaderNode(uint8 RGBA, const FString& NodeName, const FString& OutputName = TEXT("out"));

	/**
	 * Helper function to create an InterchangeShaderNode.
	 *
	 * @param NodeName - The name of the shader node.
	 * @param ShaderType - The type of shader node we want to create.
	 * @param OutputName - The output name of the MaterialX node. The default name is 'out' as stated by the standard library.
	 *
	 * @return The shader node that was created.
	 */
	UInterchangeShaderNode* CreateShaderNode(const FString& NodeName, const FString& ShaderType, const FString& OutputName = TEXT("out"));

	/**
	 * Helper function to create an InterchangeFunctionCallShaderNode.
	 *
	 * @param NodeName - The name of the shader node.
	 * @param FunctionPath - The path to the Material Function we want to create.
	 * @param OutputName - The output name of the MaterialX node. The default name is 'out' as stated by the standard library.
	 *
	 * @return The shader node that was created.
	 */
	UInterchangeFunctionCallShaderNode* CreateFunctionCallShaderNode(const FString& NodeName, const FString& FunctionPath, const FString& OutputName = TEXT("out"));
	UInterchangeFunctionCallShaderNode* CreateFunctionCallShaderNode(const FString& NodeName, uint8 EnumType, uint8 EnumValue, const FString& OutputName = TEXT("out"));

	/**
	 * Helper function to create an InterchangeTextureNode.
	 *
	 * @param Node - The MaterialX node. This should be of the category <image>. No test is done on it.
	 *
	 * @return The texture node that was created.
	 */
	template<typename TextureTypeNode>
	UInterchangeTextureNode* CreateTextureNode(MaterialX::NodePtr Node) const
	{
		static_assert(std::is_convertible_v<TextureTypeNode*, UInterchangeTextureNode*>, "CreateTextureNode only accepts type that derived from UInterchangeTextureNode");
		UInterchangeTexture2DNode* TextureNode = nullptr;

		//A node image should have an input file otherwise the user should check its default value
		if(Node)
		{
			if(MaterialX::InputPtr InputFile = Node->getInput("file"); InputFile && InputFile->hasValue())
			{
				FString Filepath{ InputFile->getValueString().c_str() };
				const FString FilePrefix = GetFilePrefix(InputFile);
				Filepath = FPaths::Combine(FilePrefix, Filepath);
				const FString Filename = FPaths::GetCleanFilename(Filepath);
				const FString TextureNodeUID = TEXT("\\Texture\\") + Filename;

				//Only add the TextureNode once
				TextureNode = const_cast<UInterchangeTexture2DNode*>(Cast<UInterchangeTexture2DNode>(NodeContainer.GetNode(TextureNodeUID)));
				if(TextureNode == nullptr)
				{
					TextureNode = NewObject<TextureTypeNode>(&NodeContainer);
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

					auto GetAddressMode = [Node](const char* InputName)
					{
						EInterchangeTextureWrapMode WrapMode = EInterchangeTextureWrapMode::Wrap;
						if(MaterialX::InputPtr InputAddressMode = Node->getInput(InputName))
						{
							const std::string& AddressMode = InputAddressMode->getValueString();
							if(AddressMode == "clamp")
							{
								WrapMode = EInterchangeTextureWrapMode::Clamp;
							}
							else if(AddressMode == "mirror")
							{
								WrapMode = EInterchangeTextureWrapMode::Mirror;
							}
							else
							{
								WrapMode = EInterchangeTextureWrapMode::Wrap;
							}
						}

						return WrapMode;
					};

					EInterchangeTextureWrapMode WrapModeU = GetAddressMode("uaddressmode");
					EInterchangeTextureWrapMode WrapModeV = GetAddressMode("vaddressmode");

					TextureNode->SetCustomWrapU(WrapModeU);
					TextureNode->SetCustomWrapV(WrapModeV);

					// Encode the compression in the payloadKey
					if(bTangentSpaceInput)
					{
						TextureNode->SetPayLoadKey(TextureNode->GetPayLoadKey().GetValue() + FMaterialXManager::TexturePayloadSeparator + FString::FromInt(TextureCompressionSettings::TC_Normalmap));
					}
				}
			}
		}

		return TextureNode;
	}

	/**
	 * Get the UE corresponding name of a MaterialX Node category and input for a material.
	 *
	 * @param Input - MaterialX input.
	 *
	 * @return The matched name of the Node/Input, or an empty string.
	 */
	const FString& GetMatchedInputName(MaterialX::NodePtr Node, MaterialX::InputPtr Input) const;

	/**
	 * Get the input name. Use this function instead of getName() because this returns the name that will be used by UE inputs even if a renaming has occurred.
	 *
	 * @param Input - The input to retrieve the name from.
	 *
	 * @return The input name.
	 */
	FString GetInputName(MaterialX::InputPtr Input) const;

	/**
	 * Return the innermost file prefix of an element in the current scope. If it has none, it will take the one from its parents.
	 *
	 * @param Element - the Element to retrieve the file prefix from. This can be anything: an input, a node, a nodegraph, and so on.
	 *
	 * @return a file prefix or an empty string.
	 *
	 */
	FString GetFilePrefix(MaterialX::ElementPtr Element) const;

	/**
	 * Helper function that returns a vector. The function makes no assumption on the input, and it should have a value of vectorN type.
	 *
	 * @param Input - The input that has a vectorN value in it.
	 *
	 * @return The vector.
	 */
	FLinearColor GetVector(MaterialX::InputPtr Input) const;

	/**
	 * Retrieve the Interchange parent name of a MaterialX node. Useful when a node is a combination of several nodes connected to different inputs, such as Noise3D.
	 *
	 * @param Node - The node whose parent name is retrieved.
	 * @return The node of the parent.
	 */
	FString GetAttributeParentName(MaterialX::NodePtr Node) const;

	virtual void RegisterConnectNodeOutputToInputDelegates();

	/**
	 * Set the matching inputs names of a node to correspond to the one used by UE. The matching name is stored under the attribute UE::NewName.
	 *
	 * @param Node - Look up to all inputs of Node and set the matching name attribute.
	 */
	void SetMatchingInputsNames(MaterialX::NodePtr Node) const;

	/**
	 * Add the input new name under the attribute UE::NewName.
	 *
	 * @param Input - The input to add the proper attribute.
	 * @param NewName - the new name of the input.
	 */
	void SetAttributeNewName(MaterialX::InputPtr Input, const char* NewName) const;

private:
	
	UInterchangeShaderNode* ConnectGeometryInputToOutput(const FConnectNode& Connect, const FString& ShaderType, const FString& TransformShaderType, const FString& TransformInput, const FString& TransformSourceType, int32 TransformSource, const FString& TransformType, int32 TransformSDestination, bool bIsVector = true);

protected:

	/** 
	 * @param Get<0>: node
	 * @param Get<1>: output
	 */
	using FNodeOutput = TPair<FString, FString>;

	/** Store the shader nodes only when we create the shader graph node*/
	TMap<FNodeOutput, UInterchangeShaderNode*> ShaderNodes;

	/** Matching MaterialX category and Connect function*/
	TMap<FString, FOnConnectNodeOutputToInput> MatchingConnectNodeDelegates;

	/** The surface shader node processed during the Translate, up to the derived class to initialize it*/
	MaterialX::NodePtr SurfaceShaderNode;

	/** Initialized by the material shader (e.g: surfacematerial), the derived class should only set the ShaderType */
	UInterchangeShaderGraphNode* ShaderGraphNode;

	/** Used for texture compression and transform to tangent space nodes coming from inputs such as coat_normal  */
	bool bTangentSpaceInput;
};
#endif