// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Misc/StringBuilder.h"
#include "Misc/MemStack.h"
#include "Misc/GeneratedTypeName.h"
#include "RHIDefinitions.h"
#include "RHIShaderPlatform.h"
#include "RHIFeatureLevel.h"
#include "HLSLTree/HLSLTreeTypes.h"
#include "HLSLTree/HLSLTreeHash.h"
#include "MaterialShared.h"

class FMaterial;
class FMaterialCompilationOutput;
class ITargetPlatform;

namespace UE::Shader
{
class FPreshaderData;
}

namespace UE::HLSLTree
{

class FErrorHandlerInterface;
class FNode;
class FOwnedNode;
class FScope;
class FExpression;
class FFunction;
class FRequestedType;
class FPreparedType;
class FPrepareValueResult;
class FEmitScope;
class FEmitShaderExpression;
class FEmitShaderStatement;

struct FEmitPreshaderScope;
struct FEmitValuePreshaderResult;

struct FEmitShaderScopeEntry
{
	FEmitShaderScopeEntry() = default;
	FEmitShaderScopeEntry(FEmitScope* InScope, int32 InIndent, FStringBuilderBase& InCode) : Scope(InScope), Code(&InCode), Indent(InIndent) {}

	FEmitScope* Scope = nullptr;
	FStringBuilderBase* Code = nullptr;
	int32 Indent = 0;
};
using FEmitShaderScopeStack = TArray<FEmitShaderScopeEntry, TInlineAllocator<16>>;

class FEmitShaderNode
{
public:
	virtual void EmitShaderCode(FEmitShaderScopeStack& Stack, int32 Indent, FStringBuilderBase& OutString) = 0;
	virtual FEmitShaderExpression* AsExpression() { return nullptr; }
	virtual FEmitShaderStatement* AsStatement() { return nullptr; }

	FEmitShaderNode(FEmitScope& InScope, TArrayView<FEmitShaderNode*> InDependencies);

	FEmitScope* Scope = nullptr;
	FEmitShaderNode* NextScopedNode = nullptr;
	TArrayView<FEmitShaderNode*> Dependencies;
};
using FEmitShaderDependencies = TArray<FEmitShaderNode*, TInlineAllocator<8>>;

/**
 * Represents an HLSL expression
 */
class FEmitShaderExpression final : public FEmitShaderNode
{
public:
	FEmitShaderExpression(FEmitScope& InScope, TArrayView<FEmitShaderNode*> InDependencies, const Shader::FType& InType, FXxHash64 InHash)
		: FEmitShaderNode(InScope, InDependencies)
		, Type(InType)
		, Hash(InHash)
	{}

	virtual void EmitShaderCode(FEmitShaderScopeStack& Stack, int32 Indent, FStringBuilderBase& OutString) override;
	virtual FEmitShaderExpression* AsExpression() override { return this; }
	inline bool IsInline() const { return Value == nullptr; }

	/**
	 * String used to reference this expression.
	 * For inline epxressions, this will be the actual HLSL.  Otherwise it will be the name of a local variable
	 */
	const TCHAR* Reference = nullptr;

	/** For non-inline expressions, this holds the actual HLSL.  Otherwise it will be nullptr */
	const TCHAR* Value = nullptr;

	/** Type of the HLSL */
	Shader::FType Type;

	/** Hash of the string, used to deduplicate identical expressions */
	FXxHash64 Hash;
};

enum class EEmitScopeFormat : uint8
{
	None,
	Unscoped,
	Scoped,
};

class FEmitShaderStatement final : public FEmitShaderNode
{
public:
	FEmitShaderStatement(FEmitScope& InScope, TArrayView<FEmitShaderNode*> InDependencies)
		: FEmitShaderNode(InScope, InDependencies)
	{}

	virtual void EmitShaderCode(FEmitShaderScopeStack& Stack, int32 Indent, FStringBuilderBase& OutString) override;
	virtual FEmitShaderStatement* AsStatement() override { return this; }

	FEmitScope* NestedScopes[2] = { nullptr };
	FStringView Code[2];
	EEmitScopeFormat ScopeFormat;
};

enum class EFormatArgType : uint8
{
	Void,
	ShaderValue,
	String,
	Int,
	Uint,
	Float,
	Bool,
};

struct FFormatArgVariant
{
	FFormatArgVariant() {}
	FFormatArgVariant(FEmitShaderExpression* InValue) : Type(EFormatArgType::ShaderValue), ShaderValue(InValue) { check(InValue); }
	FFormatArgVariant(const TCHAR* InValue) : Type(EFormatArgType::String), String(InValue) { check(InValue); }
	FFormatArgVariant(int32 InValue) : Type(EFormatArgType::Int), Int(InValue) {}
	FFormatArgVariant(uint32 InValue) : Type(EFormatArgType::Uint), Uint(InValue) {}
	FFormatArgVariant(float InValue) : Type(EFormatArgType::Float), Float(InValue) {}
	FFormatArgVariant(bool InValue) : Type(EFormatArgType::Bool), Bool(InValue) {}

	EFormatArgType Type = EFormatArgType::Void;
	union
	{
		FEmitShaderExpression* ShaderValue;
		const TCHAR* String;
		int32 Int;
		uint32 Uint;
		float Float;
		bool Bool;
	};
};

using FFormatArgList = TArray<FFormatArgVariant, TInlineAllocator<8>>;

namespace Private
{
inline void BuildFormatArgList(FFormatArgList&) {}

template<typename Type, typename... Types>
inline void BuildFormatArgList(FFormatArgList& OutList, Type Arg, Types... Args)
{
	OutList.Add(Arg);
	BuildFormatArgList(OutList, Forward<Types>(Args)...);
}

void InternalFormatStrings(FStringBuilderBase* OutString0,
	FStringBuilderBase* OutString1,
	FEmitShaderDependencies& OutDependencies,
	FStringView Format0,
	FStringView Format1,
	const FFormatArgList& ArgList);
} // namespace Private

/**
 * FormatString() provides printf-like functionality, except dependencies are automatically extracted from the list of Args
 * In addition, a single '%' is used for all format specifiers, since the Args list is strongly typed
 * Not all printf arg types are currently supported, but more can be added by extending FFormatArgVariant to support more types
 */
template<typename FormatType, typename... Types>
void FormatString(FStringBuilderBase& OutString, FEmitShaderDependencies& OutDependencies, const FormatType& Format, Types... Args)
{
	FFormatArgList ArgList;
	Private::BuildFormatArgList(ArgList, Forward<Types>(Args)...);
	Private::InternalFormatStrings(&OutString, nullptr, OutDependencies, Format, FStringView(), ArgList);
}

template<typename FormatType0, typename FormatType1, typename... Types>
void FormatStrings(FStringBuilderBase& OutString0, FStringBuilderBase& OutString1, FEmitShaderDependencies& OutDependencies, const FormatType0& Format0, const FormatType1& Format1, Types... Args)
{
	FFormatArgList ArgList;
	Private::BuildFormatArgList(ArgList, Forward<Types>(Args)...);
	Private::InternalFormatStrings(&OutString0, &OutString1, OutDependencies, Format0, Format1, ArgList);
}

enum class EEmitScopeState : uint8
{
	Uninitialized,
	Initializing,
	Live,
	Dead,
};

/**
 * FEmitScopes mostly track FScopes, except they hold transient state used while emitting HLSL
 * The exception is FFunction, which allows FScopes to be dynamically injected into the hierarchy, which will result in FEmitScopes matching this dynamic hierarchy
 */
class FEmitScope
{
public:
	static FEmitScope* FindSharedParent(FEmitScope* Lhs, FEmitScope* Rhs);
	bool HasParent(const FEmitScope* InParent) const;
	bool IsLoop() const;
	FEmitScope* FindLoop();

	void EmitShaderCode(FEmitShaderScopeStack& Stack);

	FEmitScope* ParentScope = nullptr;
	FStatement* OwnerStatement = nullptr;
	FStatement* ContainedStatement = nullptr;
	FEmitShaderNode* FirstNode = nullptr;
	int32 NestedLevel = 0;
	EEmitScopeState State = EEmitScopeState::Uninitialized;
	EExpressionEvaluation Evaluation = EExpressionEvaluation::None;
};
inline bool IsScopeDead(const FEmitScope* Scope)
{
	return !Scope || Scope->State == EEmitScopeState::Dead;
}

struct FEmitCustomHLSLInput
{
	FStringView Name;
	FStringView ObjectDeclarationCode;
	FStringView ObjectForwardCode;
	Shader::FType Type;
};

class FEmitCustomHLSL
{
public:
	FStringView DeclarationCode;
	FStringView FunctionCode;
	TConstArrayView<FEmitCustomHLSLInput> Inputs;
	const Shader::FStructType* OutputType;
	uint32 ShaderFrequencyMask;
	int32 Index;
};

struct FPreshaderLocalPHIScope
{
	FPreshaderLocalPHIScope(const FExpression* InExpression, int32 InValueStackPosition) : ExpressionLocalPHI(InExpression), ValueStackPosition(InValueStackPosition) {}

	const FExpression* ExpressionLocalPHI;
	int32 ValueStackPosition;
};

enum class EEmitCastFlags : uint32
{
	None = 0u,
	// Scalars are replicated by default, this will zero-extend them instead
	ZeroExtendScalar = (1u << 0),
};
ENUM_CLASS_FLAGS(EEmitCastFlags)

struct FTargetParameters
{
	FTargetParameters() = default;

	FTargetParameters(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel, const ITargetPlatform* InTargetPlatform)
		: ShaderPlatform(InShaderPlatform)
		, FeatureLevel(InFeatureLevel)
		, TargetPlatform(InTargetPlatform)
	{}

	inline bool IsGenericTarget() const { return FeatureLevel == ERHIFeatureLevel::Num; }

	EShaderPlatform ShaderPlatform = SP_NumPlatforms;
	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::Num;
	const ITargetPlatform* TargetPlatform = nullptr;
};

/**
 * Used to associate owner objects (such as UMaterialExpression) with the FExpressions used to generate their input
 * This is currently only used to color pins/wires based on types in the material editor
 */
struct FConnectionKey
{
	FConnectionKey() = default;
	FConnectionKey(const UObject* InInputObject, const FExpression* InOutputExpression) : InputObject(InInputObject), OutputExpression(InOutputExpression) {}

	const UObject* InputObject = nullptr;
	const FExpression* OutputExpression = nullptr;
};
inline uint32 GetTypeHash(const FConnectionKey& Key)
{
	return HashCombine(::GetTypeHash(Key.InputObject), ::GetTypeHash(Key.OutputExpression));
}
inline bool operator==(const FConnectionKey& Lhs, const FConnectionKey& Rhs)
{
	return Lhs.InputObject == Rhs.InputObject &&
		Lhs.OutputExpression == Rhs.OutputExpression;
}
inline bool operator!=(const FConnectionKey& Lhs, const FConnectionKey& Rhs)
{
	return !operator==(Lhs, Rhs);
}

/** Tracks shared state while emitting HLSL code */
class FEmitContext
{
public:
	ENGINE_API explicit FEmitContext(FMemStackBase& InAllocator,
		const FTargetParameters& InTargetParameters,
		FErrorHandlerInterface& InErrors,
		const Shader::FStructTypeRegistry& InTypeRegistry);

	ENGINE_API ~FEmitContext();

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

	/** Finds or creates arbitrary data that's associated with this context. This can be used to provide custom inputs/outputs to FExpression/FStatement implementations */
	template<typename T>
	T& AcquireData()
	{
		const FXxHash64 TypeHash = HashValue(GetGeneratedTypeName<T>());
		TCustomDataWrapper<T>* Wrapper = static_cast<TCustomDataWrapper<T>*>(InternalFindData(TypeHash));
		if (!Wrapper)
		{
			Wrapper = new TCustomDataWrapper<T>;
			InternalRegisterData(TypeHash, Wrapper);
		}
		return Wrapper->Data;
	}

	template<typename T>
	const T& FindData() const
	{
		const FXxHash64 TypeHash = HashValue(GetGeneratedTypeName<T>());
		TCustomDataWrapper<T>* Wrapper = static_cast<TCustomDataWrapper<T>*>(InternalFindData(TypeHash));
		check(Wrapper);
		return Wrapper->Data;
	}

	template<typename T>
	T& FindData()
	{
		const FXxHash64 TypeHash = HashValue(GetGeneratedTypeName<T>());
		TCustomDataWrapper<T>* Wrapper = static_cast<TCustomDataWrapper<T>*>(InternalFindData(TypeHash));
		check(Wrapper);
		return Wrapper->Data;
	}

	ENGINE_API void EmitDeclarationsCode(FStringBuilderBase& OutCode);

	ENGINE_API FPreparedType GetPreparedType(const FExpression* Expression, const FRequestedType& RequestedType) const;
	ENGINE_API Shader::FType GetResultType(const FExpression* Expression, const FRequestedType& RequestedType) const;
	ENGINE_API Shader::FType GetTypeForPinColoring(const FExpression* Expression) const;
	ENGINE_API EExpressionEvaluation GetEvaluation(const FExpression* Expression, const FEmitScope& Scope, const FRequestedType& RequestedType) const;

	ENGINE_API FPreparedType PrepareExpression(const FExpression* InExpression, FEmitScope& Scope, const FRequestedType& RequestedType);
	ENGINE_API void MarkInputType(const FExpression* InExpression, const Shader::FType& Type);

	ENGINE_API FEmitScope* InternalPrepareScope(FScope* Scope, FScope* ParentScope);
	ENGINE_API FEmitScope* PrepareScope(FScope* Scope);
	ENGINE_API FEmitScope* PrepareScopeWithParent(FScope* Scope, FScope* ParentScope);
	ENGINE_API void MarkScopeEvaluation(FEmitScope& EmitParentScope, FScope* Scope, EExpressionEvaluation Evaluation);
	ENGINE_API void MarkScopeDead(FEmitScope& EmitParentScope, FScope* Scope);

	ENGINE_API void EmitPreshaderScope(const FScope* Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> PreshaderScopes, Shader::FPreshaderData& OutPreshader);
	ENGINE_API void EmitPreshaderScope(FEmitScope& EmitScope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> PreshaderScopes, Shader::FPreshaderData& OutPreshader);

	ENGINE_API FEmitScope* AcquireEmitScopeWithParent(const FScope* Scope, FEmitScope* EmitParentScope);
	ENGINE_API FEmitScope* AcquireEmitScope(const FScope* Scope);
	ENGINE_API FEmitScope* FindEmitScope(const FScope* Scope);
	ENGINE_API FEmitScope* InternalEmitScope(const FScope* Scope);

	template<typename T>
	static TArrayView<FEmitShaderNode*> MakeDependencies(T*& Dependency)
	{
		return Dependency ? TArrayView<FEmitShaderNode*>(&Dependency, 1) : TArrayView<FEmitShaderNode*>();
	}

	ENGINE_API void Finalize();

	void ResetPastRequestedTypes();

	ENGINE_API FEmitShaderExpression* InternalEmitExpression(FEmitScope& Scope, TArrayView<FEmitShaderNode*> Dependencies, bool bInline, const Shader::FType& Type, FStringView Code);

	/**
	 * Generate a snippit of HLSL code, strings with identical contents will be deduplicated, and their dependencies will be merged
	 * @param Scope the scope where the code should be included
	 * @param Dependencies a list of explicit dependencies, dependencies are automatically extracted from the given vargs, so this is typically not needed
	 * @param Type the type of the HLSL expression
	 * @param Format printf-style format string, except all format specifiers use a single '%' character with no additional characters
	 * @param Args variable list of args to fill in the format string, 'FEmitShaderExpression*' is directly supported here to inject the expression's value, as well as include it as a dependency
	 */
	template<typename FormatType, typename... Types>
	FEmitShaderExpression* EmitExpressionWithDependencies(FEmitScope& Scope, TArrayView<FEmitShaderNode*> Dependencies, const Shader::FType& Type, const FormatType& Format, Types... Args)
	{
		TStringBuilder<2048> String;
		FEmitShaderDependencies LocalDependencies(Dependencies);
		FormatString(String, LocalDependencies, Format, Forward<Types>(Args)...);
		return InternalEmitExpression(Scope, LocalDependencies, false, Type, String.ToView());
	}

	template<typename FormatType, typename... Types>
	FEmitShaderExpression* EmitExpressionWithDependency(FEmitScope& Scope, FEmitShaderNode* Dependency, const Shader::FType& Type, const FormatType& Format, Types... Args)
	{
		return EmitExpressionWithDependencies(Scope, MakeDependencies(Dependency), Type, Format, Forward<Types>(Args)...);
	}

	template<typename FormatType, typename... Types>
	FEmitShaderExpression* EmitInlineExpressionWithDependencies(FEmitScope& Scope, TArrayView<FEmitShaderNode*> Dependencies, const Shader::FType& Type, const FormatType& Format, Types... Args)
	{
		TStringBuilder<2048> String;
		FEmitShaderDependencies LocalDependencies(Dependencies);
		FormatString(String, LocalDependencies, Format, Forward<Types>(Args)...);
		return InternalEmitExpression(Scope, LocalDependencies, true, Type, String.ToView());
	}

	template<typename FormatType, typename... Types>
	FEmitShaderExpression* EmitInlineExpressionWithDependency(FEmitScope& Scope, FEmitShaderNode* Dependency, const Shader::FType& Type, const FormatType& Format, Types... Args)
	{
		return EmitInlineExpressionWithDependencies(Scope, MakeDependencies(Dependency), Type, Format, Forward<Types>(Args)...);
	}

	template<typename FormatType, typename... Types>
	FEmitShaderExpression* EmitExpression(FEmitScope& Scope, const Shader::FType& Type, const FormatType& Format, Types... Args)
	{
		return EmitExpressionWithDependencies(Scope, TArrayView<FEmitShaderNode*>(), Type, Format, Forward<Types>(Args)...);
	}

	template<typename FormatType, typename... Types>
	FEmitShaderExpression* EmitInlineExpression(FEmitScope& Scope, const Shader::FType& Type, const FormatType& Format, Types... Args)
	{
		return EmitInlineExpressionWithDependencies(Scope, TArrayView<FEmitShaderNode*>(), Type, Format, Forward<Types>(Args)...);
	}

	ENGINE_API FEmitShaderStatement* InternalEmitStatement(FEmitScope& Scope, TArrayView<FEmitShaderNode*> Dependencies, EEmitScopeFormat ScopeFormat, FEmitScope* NestedScope0, FEmitScope* NestedScope1, FStringView Code0, FStringView Code1);

	template<typename FormatType0, typename FormatType1, typename... Types>
	FEmitShaderStatement* EmitFormatStatementInternal(FEmitScope& Scope,
		TArrayView<FEmitShaderNode*> Dependencies,
		EEmitScopeFormat ScopeFormat,
		FEmitScope* NestedScope0,
		FEmitScope* NestedScope1,
		const FormatType0& Format0,
		const FormatType1& Format1,
		Types... Args)
	{
		TStringBuilder<1024> String0;
		TStringBuilder<1024> String1;
		FEmitShaderDependencies LocalDependencies(Dependencies);
		FormatStrings(String0, String1, LocalDependencies, Format0, Format1, Forward<Types>(Args)...);
		return InternalEmitStatement(Scope, LocalDependencies, ScopeFormat, NestedScope0, NestedScope1, String0.ToView(), String1.ToView());
	}

	template<typename FormatType, typename... Types>
	FEmitShaderStatement* EmitStatementWithDependencies(FEmitScope& Scope, TArrayView<FEmitShaderNode*> Dependencies, const FormatType& Format, Types... Args)
	{
		TStringBuilder<1024> String;
		FEmitShaderDependencies LocalDependencies(Dependencies);
		FormatString(String, LocalDependencies, Format, Forward<Types>(Args)...);
		return InternalEmitStatement(Scope, LocalDependencies, EEmitScopeFormat::None, nullptr, nullptr, String.ToView(), FStringView());
	}

	template<typename FormatType, typename... Types>
	FEmitShaderStatement* EmitStatementWithDependency(FEmitScope& Scope, FEmitShaderNode* Dependency, const FormatType& Format, Types... Args)
	{
		return EmitStatementWithDependencies(Scope, MakeDependencies(Dependency), Format, Forward<Types>(Args)...);
	}

	template<typename FormatType, typename... Types>
	FEmitShaderStatement* EmitStatement(FEmitScope& Scope, const FormatType& Format, Types... Args)
	{
		return EmitStatementWithDependencies(Scope, TArrayView<FEmitShaderNode*>(), Format, Forward<Types>(Args)...);
	}

	FEmitShaderStatement* EmitNextScopeWithDependency(FEmitScope& Scope, FEmitShaderNode* Dependency, FScope* NextScope)
	{
		FEmitScope* EmitScope = InternalEmitScope(NextScope);
		if (EmitScope)
		{
			return InternalEmitStatement(Scope, MakeDependencies(Dependency), EEmitScopeFormat::Unscoped, EmitScope, nullptr, FStringView(), FStringView());
		}
		return nullptr;
	}

	FEmitShaderStatement* EmitNextScope(FEmitScope& Scope, FScope* NextScope)
	{
		return EmitNextScopeWithDependency(Scope, nullptr, NextScope);
	}

	template<typename FormatType, typename... Types>
	FEmitShaderStatement* EmitNestedScope(FEmitScope& Scope, FScope* NestedScope, const FormatType& Format, Types... Args)
	{
		FEmitScope* EmitScope = InternalEmitScope(NestedScope);
		if (EmitScope)
		{
			TStringBuilder<1024> String;
			FEmitShaderDependencies LocalDependencies;
			FormatString(String, LocalDependencies, Format, Forward<Types>(Args)...);
			return InternalEmitStatement(Scope, LocalDependencies, EEmitScopeFormat::Scoped, EmitScope, nullptr, String.ToView(), FStringView());
		}
		return nullptr;
	}

	template<typename FormatType0, typename FormatType1, typename... Types>
	FEmitShaderStatement* EmitNestedScopes(FEmitScope& Scope, FScope* NestedScope0, FScope* NestedScope1, const FormatType0& Format0, const FormatType1& Format1, Types... Args)
	{
		FEmitScope* EmitScope0 = InternalEmitScope(NestedScope0);
		FEmitScope* EmitScope1 = InternalEmitScope(NestedScope1);
		if (EmitScope1)
		{
			TStringBuilder<1024> String0;
			TStringBuilder<1024> String1;
			FEmitShaderDependencies LocalDependencies;
			FormatStrings(String0, String1, LocalDependencies, Format0, Format1, Forward<Types>(Args)...);
			return InternalEmitStatement(Scope, LocalDependencies, EEmitScopeFormat::Scoped, EmitScope0, EmitScope1, String0.ToView(), String1.ToView());
		}
		else if (EmitScope0)
		{
			TStringBuilder<1024> String;
			FEmitShaderDependencies LocalDependencies;
			FormatString(String, LocalDependencies, Format0, Forward<Types>(Args)...);
			return InternalEmitStatement(Scope, LocalDependencies, EEmitScopeFormat::Scoped, EmitScope0, nullptr, String.ToView(), FStringView());
		}

		return nullptr;
	}

	ENGINE_API FEmitShaderExpression* EmitPreshaderOrConstant(FEmitScope& Scope, const FRequestedType& RequestedType, const Shader::FType& ResultType, const FExpression* Expression);
	ENGINE_API FEmitShaderExpression* EmitConstantZero(FEmitScope& Scope, const Shader::FType& Type);
	ENGINE_API FEmitShaderExpression* EmitCast(FEmitScope& Scope, FEmitShaderExpression* ShaderValue, const Shader::FType& DestType, EEmitCastFlags Flags = EEmitCastFlags::None);

	/** FStringViews passed to this method are expected to reference persistent memory */
	ENGINE_API FEmitShaderExpression* EmitCustomHLSL(FEmitScope& Scope, FStringView DeclarationCode, FStringView FunctionCode, TConstArrayView<FCustomHLSLInput> Inputs, const Shader::FStructType* OutputType);

	class FCustomDataWrapper
	{
	public:
		virtual ~FCustomDataWrapper() {}
	};

	template<typename T>
	class TCustomDataWrapper : public FCustomDataWrapper
	{
	public:
		T Data;
	};

	ENGINE_API bool InternalError(FStringView ErrorMessage);
	ENGINE_API void InternalRegisterData(FXxHash64 Hash, FCustomDataWrapper* Data);
	ENGINE_API FCustomDataWrapper* InternalFindData(FXxHash64 Hash) const;

	FMemStackBase* Allocator = nullptr;
	FErrorHandlerInterface* Errors = nullptr;
	const Shader::FStructTypeRegistry* TypeRegistry = nullptr;
	FTargetParameters TargetParameters;
	EShaderFrequency ShaderFrequency = SF_Pixel;
	bool bMarkLiveValues = false;
	bool bUseAnalyticDerivatives = false;

	bool bCompiledShadingModels = false;
	bool bUsesSkyAtmosphere = false;
	bool bUsesSpeedTree = false;
	bool bUsesSphericalParticleOpacity = false;
	bool bUsesWorldPositionExcludingShaderOffsets = false;

	uint32 DynamicParticleParameterMask = 0u;

	FActiveStructFieldStack ActiveStructFieldStack;

	TArray<const FOwnedNode*, TInlineAllocator<32>> OwnerStack;
	TArray<FEmitShaderNode*> EmitNodes;
	TMap<const FScope*, FEmitScope*> EmitScopeMap;
	TMap<FXxHash64, FPrepareValueResult*> PrepareValueMap;
	TMap<FXxHash64, FRequestedType*> RequestedTypeTracker;
	TMap<FMaterialParameterInfo, FMaterialParameterValue> SeenStaticParameterValues;
	TMap<const FExpression*, FEmitScope*> PrepareLocalPHIMap;
	TMap<FXxHash64, FEmitShaderExpression*> EmitLocalPHIMap;
	TMap<FXxHash64, FEmitShaderExpression*> EmitExpressionMap;
	TMap<FXxHash64, FEmitShaderExpression*> EmitPreshaderMap;
	TMap<FXxHash64, FEmitShaderExpression*> EmitValueMap;
	TMap<const FFunction*, FEmitShaderNode*> EmitFunctionMap;
	TMap<FXxHash64, FEmitCustomHLSL> EmitCustomHLSLMap;
	TArray<struct FPreshaderLoopScope*> PreshaderLoopScopes;
	TArray<const FPreshaderLocalPHIScope*> PreshaderLocalPHIScopes;
	TMap<FXxHash64, FCustomDataWrapper*> CustomDataMap;
	TMap<FConnectionKey, Shader::FType> ConnectionMap;
	int32 PreshaderStackPosition = 0;
	int32 NumErrors = 0;

	int32 NumExpressionLocals = 0;
	int32 NumExpressionLocalPHIs = 0;

	/**
	 * TODO - Material values required for preshaders, need to decouple preshaders from material system
	 * Current plan is to implement support for externally registered preshader opcodes, and move the preshader opcodes related to material parameters into a separate module.
	 * This would require a runtime parallel to CustomDataMap, in order to pass the relevant material data to the preshader VM in a generic way.
	 * Would also need some generic interface between the preshaders generated here, and the preshaders stored in FMaterialCompilationOutput
	 */
	const FMaterial* Material = nullptr;
	const UMaterialInterface* MaterialInterface = nullptr;
	FMaterialCompilationOutput* MaterialCompilationOutput = nullptr;
	uint32 UniformPreshaderOffset = 0u;
	uint32 CurrentBoolUniformOffset = ~0u;
	uint32 CurrentNumBoolComponents = 32u;
};

struct FEmitOwnerScope
{
	FEmitOwnerScope(FEmitContext& InContext, const FOwnedNode* InNode) : Context(InContext), Node(InNode)
	{
		InContext.OwnerStack.Add(InNode);
	}

	~FEmitOwnerScope()
	{
		verify(Context.OwnerStack.Pop(EAllowShrinking::No) == Node);
	}

	FEmitContext& Context;
	const FOwnedNode* Node;
};

namespace Private
{
void MoveToScope(FEmitShaderNode* EmitNode, FEmitScope& Scope);

void EmitPreshaderField(
	FEmitContext& Context,
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& UniformPreshaders,
	TMemoryImageArray<FMaterialUniformPreshaderField>& UniformPreshaderFields,
	Shader::FPreshaderData& UniformPreshaderData,
	FMaterialUniformPreshaderHeader*& PreshaderHeader,
	TFunction<void (FEmitValuePreshaderResult&)> EmitPreshaderOpcode,
	const Shader::FValueTypeDescription& TypeDesc,
	int32 ComponentIndex,
	FStringBuilderBase& FormattedCode);
}

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
