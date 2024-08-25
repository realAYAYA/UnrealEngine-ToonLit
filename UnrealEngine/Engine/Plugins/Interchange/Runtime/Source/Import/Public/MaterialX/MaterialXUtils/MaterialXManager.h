// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "MaterialXCore/Node.h"
#include "Misc/TVariant.h"
#include "MaterialX/InterchangeMaterialXDefinitions.h"

class FMaterialXBase;
class UInterchangeBaseNodeContainer;
class UInterchangeShaderNode;
class FMaterialXSurfaceShaderAbstract;

class FMaterialXManager
{
public:

	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<FMaterialXBase>, FOnGetMaterialXInstance, UInterchangeBaseNodeContainer&);

	static FMaterialXManager& GetInstance();

	/** Find a corresponding Material Expression Input given a (Category [NodeGroup], Input) pair*/
	const FString* FindMatchingInput(const FString& CategoryKey, const FString& InputKey, const FString& NodeGroup = {}) const;

	/** Find a stored Material Expression Input */
	const FString* FindMaterialExpressionInput(const FString& InputKey) const;

	/** Find a matching Material Expression given a MaterialX category*/
	const FString* FindMatchingMaterialExpression(const FString& CategoryKey, const FString& NodeGroup = {}) const;

	/** Find a matching Material Function given a MaterialX category*/
	bool FindMatchingMaterialFunction(const FString& CategoryKey, const FString*& MaterialFunctionPath, uint8& EnumType, uint8& EnumValue) const;

	TSharedPtr<FMaterialXBase> GetShaderTranslator(const FString& CategoryShader, UInterchangeBaseNodeContainer& NodeContainer);

	void RegisterMaterialXInstance(const FString& CategoryShader, FOnGetMaterialXInstance MaterialXInstanceDelegate);
	
	bool IsSubstrateEnabled() const;


	FMaterialXManager(const FMaterialXManager&) = delete;
	FMaterialXManager& operator=(const FMaterialXManager&) = delete;

	static const TCHAR TexturePayloadSeparator;

private:

	/**
	* FString: Material Function path
	* Enums: data-driven BSDF nodes
	*/
	using FMaterialXMaterialFunction = TVariant<FString, EInterchangeMaterialXShaders, EInterchangeMaterialXBSDF, EInterchangeMaterialXEDF, EInterchangeMaterialXVDF>;

	struct FKeyCategoryNodegroup
	{
		template<typename CategoryString>
		FKeyCategoryNodegroup(CategoryString&& Category)
			: Category{ std::forward<CategoryString>(Category) }
		{}

		template<typename CategoryString, typename NodeGroupString>
		FKeyCategoryNodegroup(CategoryString&& Category, NodeGroupString&& NodeGroup)
			: Category{ std::forward<CategoryString>(Category) }
			, NodeGroup{ std::forward<CategoryString>(NodeGroup) }
		{}

		[[nodiscard]] FORCEINLINE bool operator==(const FKeyCategoryNodegroup& Rhs) const
		{
			return Category == Rhs.Category && NodeGroup == Rhs.NodeGroup;
		}

		friend FORCEINLINE uint32 GetTypeHash(const FKeyCategoryNodegroup& Key)
		{
			return HashCombine(GetTypeHash(Key.Category), GetTypeHash(Key.NodeGroup));
		}

		FString Category;
		FString NodeGroup; // node group is optional, some nodes in MaterialX have different material expressions in UE
	};

	FMaterialXManager();

	/** Given a MaterialX node (category (optionally a nodegroup) - input), return the UE/Interchange input name.*/
	TMap<TPair<FKeyCategoryNodegroup, FString>, FString> MatchingInputNames;

	/** Given a MaterialX node category, optionally with a node group, return the UE material expression class name*/
	TMap<FKeyCategoryNodegroup, FString> MatchingMaterialExpressions;

	/** Given a MaterialX node category, return the UE material function, used for BSDF nodes*/
	TMap<FString, FMaterialXMaterialFunction> MatchingMaterialFunctions;

	/** The different inputs of material expression that we may encounter, the MaterialX Document is modified consequently regarding those*/
	TSet<FString> MaterialExpressionInputs;

	/** Container of a MaterialX document, to translate the different nodes based on their category*/
	TMap<FString, FOnGetMaterialXInstance> MaterialXContainerDelegates;

	bool bIsSubstrateEnabled;
};
#endif

namespace UE::Interchange::MaterialX
{
	// Load necessary material functions, this function can only be called in the Game Thread
	bool AreMaterialFunctionPackagesLoaded();
}