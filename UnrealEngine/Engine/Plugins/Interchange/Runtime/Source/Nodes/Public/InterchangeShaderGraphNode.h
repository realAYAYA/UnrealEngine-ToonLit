// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once
 
#include "Nodes/InterchangeBaseNode.h"
#include "UObject/Object.h"
 
#include "InterchangeShaderGraphNode.generated.h"

class UInterchangeBaseNodeContainer;

/**
 * The Shader Ports API manages a set of inputs and outputs attributes.
 * This API can be used over any InterchangeBaseNode that wants to support shader ports as attributes.
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeShaderPortsAPI : public UObject
{
	GENERATED_BODY()
 
public:
	/**
	 * Makes an attribute key to represent a node being connected to an input (ie: Inputs:InputName:Connect).
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static FName MakeInputConnectionKey(const FString& InputName);
 
	/**
	 * Makes an attribute key to represent a value being given to an input (ie: Inputs:InputName:Value).
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static FName MakeInputValueKey(const FString& InputName);
 
	/**
	 * From an attribute key associated with an input (ie: Inputs:InputName:Value), retrieves the input name from it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static FString MakeInputName(const FString& InputKey);
	
	/**
	 * Returns true if the attribute key is associated with an input (starts with "Inputs:").
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static bool IsAnInput(const FString& AttributeKey);
 
	/**
	 * Checks if a particular input exists on a given node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static bool HasInput(const UInterchangeBaseNode* InterchangeNode, const FName& InInputName);
 
	/**
	 * Retrieves the names of all the inputs for a given node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static void GatherInputs(const UInterchangeBaseNode* InterchangeNode, TArray<FString>& OutInputNames);
 
	/**
	 * Adds an input connection attribute.
	 * @param InterchangeNode	The Node to create the input on.
	 * @param InputName			The name to give to the input.
	 * @param ExpressionUid		The unique id of the node to connect to the input.
	 * @return					true if the input connection was successfully added to the node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static bool ConnectDefaultOuputToInput(UInterchangeBaseNode* InterchangeNode, const FString& InputName, const FString& ExpressionUid);
 
	/**
	 * Adds an input connection attribute.
	 * @param InterchangeNode	The Node to create the input on.
	 * @param InputName			The name to give to the input.
	 * @param ExpressionUid		The unique id of the node to connect to the input.
	 * @param OutputName		The name of the ouput from ExpressionUid to connect to the input.
	 * @return					true if the input connection was succesfully added to the node.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static bool ConnectOuputToInput(UInterchangeBaseNode* InterchangeNode, const FString& InputName, const FString& ExpressionUid, const FString& OutputName);
 
	/**
	 * Retrieves the node unique id and the ouputname connected to a given input, if any.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	static bool GetInputConnection(const UInterchangeBaseNode* InterchangeNode, const FString& InputName, FString& OutExpressionUid, FString& OutputName);
	
	/**
	 * For an input with a value, returns the type of the stored value.
	 */
	static UE::Interchange::EAttributeTypes GetInputType(const UInterchangeBaseNode* InterchangeNode, const FString& InputName);
 
private:
	static const TCHAR* InputPrefix;
	static const TCHAR* InputSeparator;
};

/**
 * A shader node is a named set of inputs and outputs. It can be connected to other shader nodes and finally to a shader graph input.
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeShaderNode : public UInterchangeBaseNode
{
	GENERATED_BODY()
 
public:
	/**
	 * Build and return a UID name for a shader node.
	 */
	static FString MakeNodeUid(const FStringView NodeName, const FStringView ParentNodeUid);

	/**
	 * Creates a new UInterchangeShaderNode and adds it to NodeContainer as a translated node.
	 */
	static UInterchangeShaderNode* Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView NodeName, const FStringView ParentNodeUid);

	virtual FString GetTypeName() const override;
 
public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomShaderType(FString& AttributeValue) const;
 
	/**
	 * Sets which type of shader this nodes represents. Can be arbitrary or one of the predefined shader types.
	 * The material pipeline handling the shader node should be aware of the shader type that is being set here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomShaderType(const FString& AttributeValue);
 
private:
	const UE::Interchange::FAttributeKey Macro_CustomShaderTypeKey = UE::Interchange::FAttributeKey(TEXT("ShaderType"));
};

/**
 * A function call shader node has a named set of inputs and outputs which corresponds to the inputs and outputs of the shader function it instances.
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeFunctionCallShaderNode : public UInterchangeShaderNode
{
	GENERATED_BODY()

public:
	virtual FString GetTypeName() const override;

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomMaterialFunction(FString& AttributeValue) const;

	/**
	 * Sets the unique id of the material function referenced by the function call expression.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomMaterialFunction(const FString& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomMaterialFunctionKey = UE::Interchange::FAttributeKey(TEXT("MaterialFunction"));
};

/**
 * A shader graph has its own set of inputs on which shader nodes can be connected to.
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeShaderGraphNode : public UInterchangeShaderNode
{
	GENERATED_BODY()
 
public:
	/**
	 * Build and return a UID name for a shader graph node.
	 */
	static FString MakeNodeUid(const FStringView NodeName);

	/**
	 * Creates a new UInterchangeShaderGraphNode and adds it to NodeContainer as a translated node.
	 */
	static UInterchangeShaderGraphNode* Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView NodeName);

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override;

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomTwoSided(bool& AttributeValue) const;
 
	/**
	 * Sets if this shader graph should be rendered two sided or not. Defaults to off.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomTwoSided(const bool& AttributeValue);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomOpacityMaskClipValue(float& AttributeValue) const;

	/**
	 * Shader is transparent or opaque if it's alpha is lower or higher than the clip value.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomOpacityMaskClipValue(const float& AttributeValue, bool bAddApplyDelegate = true);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool GetCustomIsAShaderFunction(bool& AttributeValue) const;

	/**
	 * Sets if this shader graph should be considered as a material, false, or a material function, true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Material")
	bool SetCustomIsAShaderFunction(const bool& AttributeValue);

private:
	const UE::Interchange::FAttributeKey Macro_CustomTwoSidedKey = UE::Interchange::FAttributeKey(TEXT("TwoSided"));
	const UE::Interchange::FAttributeKey Macro_CustomOpacityMaskClipValueKey = UE::Interchange::FAttributeKey(TEXT("OpacityMaskClipValue"));
	const UE::Interchange::FAttributeKey Macro_CustomIsAShaderFunctionKey = UE::Interchange::FAttributeKey(TEXT("IsAShaderFunction"));
};