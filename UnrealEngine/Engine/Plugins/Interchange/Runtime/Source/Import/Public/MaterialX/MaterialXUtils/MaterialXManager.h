// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "MaterialXCore/Node.h"

class FMaterialXBase;
class UInterchangeBaseNodeContainer;
class UInterchangeShaderNode;
class FMaterialXSurfaceShaderAbstract;

class FMaterialXManager
{
public:

	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<FMaterialXBase>, FOnGetMaterialXInstance, UInterchangeBaseNodeContainer&);

	static FMaterialXManager& GetInstance();

	/** Find a corresponding Material Expression Input given a (Category, Input) pair*/
	const FString* FindMatchingInput(const TPair<FString, FString>& CategoryInputKey) const;

	/** Find a stored Material Expression Input */
	const FString* FindMaterialExpressionInput(const FString& InputKey) const;

	/** Find a matching Material Expression given a MaterialX category*/
	const FString* FindMatchingMaterialExpression(const FString& CategoryKey) const;

	TSharedPtr<FMaterialXBase> GetShaderTranslator(const FString& CategoryShader, UInterchangeBaseNodeContainer& NodeContainer);

	void RegisterMaterialXInstance(const FString& CategoryShader, FOnGetMaterialXInstance MaterialXInstanceDelegate);

	FMaterialXManager(const FMaterialXManager&) = delete;
	FMaterialXManager& operator=(const FMaterialXManager&) = delete;

	static const TCHAR TexturePayloadSeparator;

private:

	FMaterialXManager();

	/** Given a MaterialX node (category - input), return the UE/Interchange input name.*/
	TMap<TPair<FString, FString>, FString> MatchingInputNames;

	/** Given a MaterialX node category, return the UE material expression class name*/
	TMap<FString, FString> MatchingMaterialExpressions;

	/** The different inputs of material expression that we may encounter, the MaterialX Document is modified consequently regarding those*/
	TSet<FString> MaterialExpressionInputs;

	/** Container of a MaterialX document, to translate the different nodes based on their category, same reason as above concerning the use of STL*/
	TMap<FString, FOnGetMaterialXInstance> MaterialXContainerDelegates;
};
#endif
