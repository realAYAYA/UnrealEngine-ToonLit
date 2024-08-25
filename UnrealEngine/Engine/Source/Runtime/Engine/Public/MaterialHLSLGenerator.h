// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Containers/Map.h"
#include "Misc/MemStack.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "RHIDefinitions.h"
#include "Materials/MaterialLayersFunctions.h"
#include "HLSLTree/HLSLTree.h"

class FMaterial;
class FMaterialCachedHLSLTree;
class FMaterialCompilationOutput;
struct FSharedShaderCompilerEnvironment;
struct FMaterialCompileTargetParameters;
struct FMaterialParameterMetadata;
class UMaterial;
class UMaterialFunctionInterface;
class UMaterialExpression;
class UMaterialExpressionFunctionInput;
class UMaterialExpressionFunctionOutput;
class UMaterialExpressionCustomOutput;
class UMaterialExpressionVertexInterpolator;
class UMaterialParameterCollection;
struct FFunctionExpressionInput;
struct FExpressionInput;
class ITargetPlatform;
enum class EMaterialParameterType : uint8;

namespace UE::HLSLTree
{
struct FSwizzleParameters;
}
namespace UE::HLSLTree::Material
{
enum class EExternalInput : uint8;
}
namespace UE::Shader
{
struct FStructType;
}

enum class EMaterialNewScopeFlag : uint8
{
	None = 0u,
	NoPreviousScope = (1u << 0),
};
ENUM_CLASS_FLAGS(EMaterialNewScopeFlag);

template<typename T>
struct TMaterialHLSLGeneratorType;

#define DECLARE_MATERIAL_HLSLGENERATOR_DATA(T) \
	template<> struct TMaterialHLSLGeneratorType<T> { static const FName& GetTypeName() { static const FName Name(TEXT(#T)); return Name; } }

/**
 * MaterialHLSLGenerator is a bridge between a material, and HLSLTree.  It facilitates generating HLSL source code for a given material, using HLSLTree
 */
class FMaterialHLSLGenerator
{
public:
	static const uint32 Version = 1;

	FMaterialHLSLGenerator(UMaterial* Material,
		const FMaterialLayersFunctions* InLayerOverrides,
		UMaterialExpression* InPreviewExpression,
		FMaterialCachedHLSLTree& OutCachedTree);

	const FMaterialLayersFunctions* GetLayerOverrides() const { return LayerOverrides; }

	const UMaterial* GetTargetMaterial() const;
	ENGINE_API UE::HLSLTree::FTree& GetTree() const;
	UE::Shader::FStructTypeRegistry& GetTypeRegistry() const;
	const UE::Shader::FStructType* GetMaterialAttributesType() const;
	const UE::HLSLTree::FExpression* GetMaterialAttributesDefaultExpression() const;

	UMaterialExpression* GetCurrentExpression() const;

	template<typename StringType>
	inline bool Error(const StringType& InError)
	{
		return InternalError(FStringView(InError));
	}

	template<typename FormatType, typename... Types>
	inline bool Errorf(const FormatType& Format, Types... Args)
	{
		TStringBuilder<1024> String;
		String.Appendf(Format, Forward<Types>(Args)...);
		return InternalError(FStringView(String.ToString(), String.Len()));
	}
	
	bool Generate();

	bool GenerateResult(UE::HLSLTree::FScope& Scope);

	UE::HLSLTree::FScope* NewScope(UE::HLSLTree::FScope& Scope, EMaterialNewScopeFlag Flags = EMaterialNewScopeFlag::None);
	UE::HLSLTree::FScope* NewOwnedScope(UE::HLSLTree::FStatement& Owner);

	UE::HLSLTree::FScope* NewJoinedScope(UE::HLSLTree::FScope& Scope);

	ENGINE_API const UE::HLSLTree::FExpression* NewConstant(const UE::Shader::FValue& Value);
	ENGINE_API const UE::HLSLTree::FExpression* NewTexCoord(int32 Index);
	const UE::HLSLTree::FExpression* NewExternalInput(UE::HLSLTree::Material::EExternalInput Input);
	const UE::HLSLTree::FExpression* NewSwizzle(const UE::HLSLTree::FSwizzleParameters& Params, const UE::HLSLTree::FExpression* Input);

	template<typename StringType>
	[[nodiscard]] inline const UE::HLSLTree::FExpression* NewErrorExpression(const StringType& InError)
	{
		return InternalNewErrorExpression(FStringView(InError));
	}

	template<typename FormatType, typename... Types>
	[[nodiscard]] const UE::HLSLTree::FExpression* NewErrorExpressionf(const FormatType& Format, Types... Args)
	{
		TStringBuilder<1024> String;
		String.Appendf(Format, Forward<Types>(Args)...);
		return InternalNewErrorExpression(String.ToView());
	}

	int32 FindInputIndex(const FExpressionInput* Input) const;

	const UE::HLSLTree::FExpression* NewDefaultInputConstant(int32 InputIndex, const UE::Shader::FValue& Value);
	const UE::HLSLTree::FExpression* NewDefaultInputExternal(int32 InputIndex, UE::HLSLTree::Material::EExternalInput Input);
	const UE::HLSLTree::FExpression* NewDefaultInputExpression(int32 InputIndex, const UE::HLSLTree::FExpression* Expression);

	/**
	 * Returns the appropriate HLSLNode representing the given UMaterialExpression.
	 * The node will be created if it doesn't exist. Otherwise, the tree will be updated to ensure the node is visible in the given scope
	 * Note that a given UMaterialExpression may only support 1 of these node types, attempting to access an invalid node type will generate an error
	 */
	const UE::HLSLTree::FExpression* AcquireExpression(UE::HLSLTree::FScope& Scope,
		int32 InputIndex,
		UMaterialExpression* MaterialExpression,
		int32 OutputIndex,
		const UE::HLSLTree::FSwizzleParameters& Swizzle);

	const UE::HLSLTree::FExpression* AcquireFunctionInputExpression(UE::HLSLTree::FScope& Scope, const UMaterialExpressionFunctionInput* MaterialExpression);

	bool GenerateStatements(UE::HLSLTree::FScope& Scope, UMaterialExpression* MaterialExpression);

	ENGINE_API const UE::HLSLTree::FExpression* GenerateMaterialParameter(FName InParameterName,
		const FMaterialParameterMetadata& InParameterMeta,
		EMaterialSamplerType InSamplerType = SAMPLERTYPE_Color,
		const FGuid& InExternalTextureGuid = FGuid());

	struct FConnectedInput
	{
		const UE::HLSLTree::FExpression* Expression;
		const FExpressionInput* Input;
		UE::HLSLTree::FScope* Scope;

		explicit FConnectedInput(const UE::HLSLTree::FExpression* InExpression)
			: Expression(InExpression)
			, Input(nullptr)
			, Scope(nullptr)
		{}

		FConnectedInput(const FExpressionInput* InInput, UE::HLSLTree::FScope* InScope)
			: Expression(nullptr)
			, Input(InInput)
			, Scope(InScope)
		{}
	};

	const UE::HLSLTree::FExpression* GenerateFunctionCall(UE::HLSLTree::FScope& Scope,
		UMaterialFunctionInterface* Function,
		EMaterialParameterAssociation ParameterAssociation,
		int32 ParameterIndex,
		TArrayView<FConnectedInput> ConnectedInputs,
		int32 OutputIndex);

	ENGINE_API const UE::HLSLTree::FExpression* GenerateBranch(UE::HLSLTree::FScope& Scope,
		const UE::HLSLTree::FExpression* ConditionExpression,
		const UE::HLSLTree::FExpression* TrueExpression,
		const UE::HLSLTree::FExpression* FalseExpression);

	template<typename T, typename... ArgTypes>
	T* NewExpressionData(const UMaterialExpression* MaterialExpression, ArgTypes... Args)
	{
		T* Data = new T(Forward<ArgTypes>(Args)...);
		InternalRegisterExpressionData(TMaterialHLSLGeneratorType<T>::GetTypeName(), MaterialExpression, Data);
		return Data;
	}

	template<typename T>
	T* FindExpressionData(const UMaterialExpression* MaterialExpression)
	{
		return static_cast<T*>(InternalFindExpressionData(TMaterialHLSLGeneratorType<T>::GetTypeName(), MaterialExpression));
	}

	template<typename T>
	T* AcquireGlobalData()
	{
		T* Data = static_cast<T*>(InternalFindExpressionData(TMaterialHLSLGeneratorType<T>::GetTypeName(), nullptr));
		if (!Data)
		{
			Data = new T();
			InternalRegisterExpressionData(TMaterialHLSLGeneratorType<T>::GetTypeName(), nullptr, Data);
		}
		return Data;
	}

	bool GetParameterOverrideValueForCurrentFunction(EMaterialParameterType ParameterType, FName ParameterName, FMaterialParameterMetadata& OutResult) const;
	FMaterialParameterInfo GetParameterInfo(const FName& ParameterName) const;

	int32 FindOrAddCustomExpressionOutputStructId(TArrayView<UE::Shader::FStructFieldInitializer> StructFields);

private:
	static constexpr int32 MaxNumPreviousScopes = UE::HLSLTree::MaxNumPreviousScopes;
	
	struct FExpressionDataKey
	{
		FExpressionDataKey(const FName& InTypeName, const UMaterialExpression* InMaterialExpression) : MaterialExpression(InMaterialExpression), TypeName(InTypeName) {}

		const UMaterialExpression* MaterialExpression;
		FName TypeName;

		friend inline uint32 GetTypeHash(const FExpressionDataKey& Value)
		{
			return HashCombine(GetTypeHash(Value.MaterialExpression), GetTypeHash(Value.TypeName));
		}

		friend inline bool operator==(const FExpressionDataKey& Lhs, const FExpressionDataKey& Rhs)
		{
			return Lhs.MaterialExpression == Rhs.MaterialExpression && Lhs.TypeName == Rhs.TypeName;
		}
	};

	using FFunctionInputArray = TArray<const UMaterialExpressionFunctionInput*, TInlineAllocator<4>>;
	using FFunctionOutputArray = TArray<UMaterialExpressionFunctionOutput*, TInlineAllocator<4>>;
	using FConnectedInputArray = TArray<FConnectedInput, TInlineAllocator<4>>;

	struct FFunctionCallEntry
	{
		UMaterialFunctionInterface* MaterialFunction = nullptr;
		UE::HLSLTree::FFunction* HLSLFunction = nullptr;
		FFunctionInputArray FunctionInputs;
		FFunctionOutputArray FunctionOutputs;
		FConnectedInputArray ConnectedInputs;
		uint64 GeneratingOutputMask = 0; // Do we need to support more than 64 outputs?
		EMaterialParameterAssociation ParameterAssociation = GlobalParameter;
		int32 ParameterIndex = INDEX_NONE;
		bool bGeneratedResult = false;

		void BeginGeneratingOutput(int32 OutputIndex)
		{
			check(OutputIndex < 64);
			GeneratingOutputMask |= (1llu << OutputIndex);
		}

		void EndGeneratingOutput(int32 OutputIndex)
		{
			check(IsGeneratingOutput(OutputIndex));
			GeneratingOutputMask ^= (1llu << OutputIndex);
		}

		bool IsGeneratingOutput(int32 OutputIndex) const
		{
			return GeneratingOutputMask & (1llu << OutputIndex);
		}
	};

	struct FScopedGenerateFunctionOutput
	{
		FScopedGenerateFunctionOutput(FFunctionCallEntry* InFunctionCall, int32 InOutputIndex)
			: FunctionCall(InFunctionCall)
			, OutputIndex(InOutputIndex)
		{
			FunctionCall->BeginGeneratingOutput(OutputIndex);
		}

		~FScopedGenerateFunctionOutput()
		{
			FunctionCall->EndGeneratingOutput(OutputIndex);
		}

	private:
		FFunctionCallEntry* FunctionCall;
		int32 OutputIndex;
	};

	struct FStatementEntry
	{
		UE::HLSLTree::FScope* PreviousScope[MaxNumPreviousScopes];
		int32 NumInputs = 0;
	};

	bool InternalGenerate();

	const UE::HLSLTree::FExpression* InternalNewErrorExpression(FStringView Error);
	ENGINE_API bool InternalError(FStringView ErrorMessage);

	void InternalRegisterExpressionData(const FName& Type, const UMaterialExpression* MaterialExpression, void* Data);
	void* InternalFindExpressionData(const FName& Type, const UMaterialExpression* MaterialExpression);

	FStringView AcquireError();

	bool NeedToPushOwnerExpression() const
	{
		// If we're inside a material function, don't change the owner to the current expression
		// Instead, we let the initial UMaterialExpressionFunctionCall own *all* the expressions generated within the function call
		// This allows errors generated within the function to be attributed back to the function call,
		// as well as allow wire types to be properly tracked for wires going into the function
		const FFunctionCallEntry* FunctionEntry = FunctionCallStack.Last();
		return !FunctionEntry->MaterialFunction;
	}

	UMaterial* TargetMaterial;
	const FMaterialLayersFunctions* LayerOverrides;
	UMaterialExpression* PreviewExpression;
	FMaterialCachedHLSLTree& CachedTree;
	FString CurrentErrorMessage;

	FFunctionCallEntry RootFunctionCallEntry;
	TArray<FFunctionCallEntry*, TInlineAllocator<8>> FunctionCallStack;
	TArray<UE::HLSLTree::FScope*> JoinedScopeStack;
	TMap<FXxHash64, TUniquePtr<FFunctionCallEntry>> FunctionCallMap;
	TMap<FXxHash64, const UE::HLSLTree::FExpression*> BranchMap;
	TMap<UMaterialFunctionInterface*, UE::HLSLTree::FFunction*> FunctionMap;
	TMap<UMaterialExpression*, FStatementEntry> StatementMap;
	TMap<FExpressionDataKey, void*> ExpressionDataMap;
	const UE::HLSLTree::FExpression* PreviewExpressionResult = nullptr;
	bool bGeneratedResult;

	TMap<FXxHash64, const UE::HLSLTree::FExpression*> GeneratedExpressionMap;
	TMap<FXxHash64, int32> CustomExpressionOutputStructIdMap;
};

ENGINE_API bool GenerateStaticTerrainLayerWeightExpression(
	FName LayerName,
	float PreviewWeight,
	bool bUseTextureArray, 
	FMaterialHLSLGenerator& Generator,
	const UE::HLSLTree::FExpression*& OutExpression);

UE_DEPRECATED(5.4, "GenerateStaticTerrainLayerWeightExpression(FName, float, FMaterialHLSLGenerator&, const UE::HLSLTree::FExpression*&) use version above")
ENGINE_API bool GenerateStaticTerrainLayerWeightExpression(
	FName LayerName,
	float PreviewWeight,
	FMaterialHLSLGenerator& Generator,
	const UE::HLSLTree::FExpression*& OutExpression);

#endif // WITH_EDITOR
