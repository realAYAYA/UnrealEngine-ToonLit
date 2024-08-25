// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "HLSLTree/HLSLTree.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#include "Misc/MemStack.h"

class UMaterial;
class UMaterialExpression;
class UMaterialExpressionCustomOutput;
class UMaterialParameterCollection;
struct FMaterialLayersFunctions;

namespace UE
{
namespace HLSLTree
{
class FEmitContext;
}
}

struct FMaterialConnectionKey
{
	const UObject* InputObject = nullptr;
	const UObject* OutputObject = nullptr;
	int32 InputIndex = INDEX_NONE;
	int32 OutputIndex = INDEX_NONE;
};
inline uint32 GetTypeHash(const FMaterialConnectionKey& Key)
{
	return HashCombine(HashCombine(HashCombine(GetTypeHash(Key.InputObject), GetTypeHash(Key.OutputObject)), GetTypeHash(Key.InputIndex)), GetTypeHash(Key.OutputIndex));
}
inline bool operator==(const FMaterialConnectionKey& Lhs, const FMaterialConnectionKey& Rhs)
{
	return Lhs.InputObject == Rhs.InputObject &&
		Lhs.OutputObject == Rhs.OutputObject &&
		Lhs.InputIndex == Rhs.InputIndex &&
		Lhs.OutputIndex == Rhs.OutputIndex;
}
inline bool operator!=(const FMaterialConnectionKey& Lhs, const FMaterialConnectionKey& Rhs)
{
	return !operator==(Lhs, Rhs);
}

class FMaterialCachedHLSLTree
{
public:
	static const FMaterialCachedHLSLTree EmptyTree;

	ENGINE_API FMaterialCachedHLSLTree();
	ENGINE_API ~FMaterialCachedHLSLTree();

	SIZE_T GetAllocatedSize() const;

	ENGINE_API bool GenerateTree(UMaterial* Material, const FMaterialLayersFunctions* LayerOverrides, UMaterialExpression* PreviewExpression);

	UE::Shader::FStructTypeRegistry& GetTypeRegistry() { return TypeRegistry; }
	//UE::HLSLTree::FTree& GetTree() { return *HLSLTree; }

	const UE::Shader::FStructTypeRegistry& GetTypeRegistry() const { return TypeRegistry; }
	UE::HLSLTree::FTree& GetTree() const { return *HLSLTree; }

	const UE::HLSLTree::FExpression* GetResultExpression() const { return ResultExpression; }
	UE::HLSLTree::FScope* GetResultScope() const { return ResultScope; }

	const UE::Shader::FStructType* GetMaterialAttributesType() const { return MaterialAttributesType; }
	const UE::Shader::FStructType* GetVTPageTableResultType() const { return VTPageTableResultType; }

	const TArray<UMaterialExpressionCustomOutput*>& GetMaterialCustomOutputs() const { return MaterialCustomOutputs; }

	const TMap<FMaterialConnectionKey, const UE::HLSLTree::FExpression*>& GetConnections() const { return ConnectionMap; }

	ENGINE_API void SetRequestedFields(const UE::HLSLTree::FEmitContext& Context, UE::HLSLTree::FRequestedType& OutRequestedType) const;
	void EmitSharedCode(FStringBuilderBase& OutCode) const;

	ENGINE_API bool IsAttributeUsed(UE::HLSLTree::FEmitContext& Context,
		UE::HLSLTree::FEmitScope& Scope,
		const UE::HLSLTree::FPreparedType& ResultType,
		EMaterialProperty Property) const;

private:
	const UE::Shader::FValue& GetMaterialAttributesDefaultValue() const { return MaterialAttributesDefaultValue; }

	FMemStackBase Allocator;
	UE::Shader::FStructTypeRegistry TypeRegistry;
	UE::HLSLTree::FTree* HLSLTree = nullptr;
	const UE::HLSLTree::FExpression* ResultExpression = nullptr;
	UE::HLSLTree::FScope* ResultScope = nullptr;

	TArray<UMaterialExpressionCustomOutput*> MaterialCustomOutputs;
	TMap<FMaterialConnectionKey, const UE::HLSLTree::FExpression*> ConnectionMap;
	const UE::Shader::FStructType* MaterialAttributesType = nullptr;
	const UE::Shader::FStructType* VTPageTableResultType = nullptr;
	UE::Shader::FValue MaterialAttributesDefaultValue;

	friend class FMaterialHLSLGenerator;
};

#endif // WITH_EDITOR
