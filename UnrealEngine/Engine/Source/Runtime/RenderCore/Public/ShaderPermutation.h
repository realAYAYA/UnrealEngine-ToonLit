// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderPermutation.h: All shader permutation's compile time API.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"


/** Defines at compile time a boolean permutation dimension. */
struct FShaderPermutationBool
{
	/** Setup the dimension's type in permutation domain as boolean. */
	using Type = bool;

	/** Setup the dimension's number of permutation. */
	static constexpr int32 PermutationCount = 2;

	/** Setup the dimension as non multi-dimensional, so that the ModifyCompilationEnvironement's
	 * define can conventily be set up in SHADER_PERMUTATION_BOOL.
	 */
	static constexpr bool IsMultiDimensional = false;


	/** Converts dimension boolean value to dimension's value id. */
	static int32 ToDimensionValueId(Type E)
	{
		return E ? 1 : 0;
	}

	/** Pass down a boolean to FShaderCompilerEnvironment::SetDefine(). */
	static bool ToDefineValue(Type E)
	{
		return E;
	}

	/** Converts dimension's value id to dimension boolean value (exact reciprocal of ToDimensionValueId). */
	static Type FromDimensionValueId(int32 PermutationId)
	{
		checkf(PermutationId == 0 || PermutationId == 1, TEXT("Invalid shader permutation dimension id %i."), PermutationId);
		return PermutationId == 1;
	}
};


/** Defines at compile time a permutation dimension made of int32 from 0 to N -1. */
template <typename TType, int32 TDimensionSize, int32 TFirstValue=0>
struct TShaderPermutationInt
{
	/** Setup the dimension's type in permutation domain as integer. */
	using Type = TType;

	/** Setup the dimension's number of permutation. */
	static constexpr int32 PermutationCount = TDimensionSize;
	
	/** Setup the dimension as non multi-dimensional, so that the ModifyCompilationEnvironement's
	 * define can conventily be set up in SHADER_PERMUTATION_INT.
	 */
	static constexpr bool IsMultiDimensional = false;
	
	/** Min and max values. */
	static constexpr Type MinValue = static_cast<Type>(TFirstValue);
	static constexpr Type MaxValue = static_cast<Type>(TFirstValue + TDimensionSize - 1);


	/** Converts dimension's integer value to dimension's value id. */
	static int32 ToDimensionValueId(Type E)
	{
		int32 PermutationId = static_cast<int32>(E) - TFirstValue;
		checkf(PermutationId < PermutationCount && PermutationId >= 0, TEXT("Unknown shader permutation dimension value id %i."), PermutationId);
		return PermutationId;
	}

	/** Pass down a int32 to FShaderCompilerEnvironment::SetDefine() even for contiguous enum classes. */
	static int32 ToDefineValue(Type E)
	{
		return ToDimensionValueId(E) + TFirstValue;
	}

	/** Converts dimension's value id to dimension's integer value (exact reciprocal of ToDimensionValueId). */
	static Type FromDimensionValueId(int32 PermutationId)
	{
		checkf(PermutationId < PermutationCount && PermutationId >= 0, TEXT("Invalid shader permutation dimension value id %i."), PermutationId);
		return static_cast<Type>(PermutationId + TFirstValue);
	}
};


/** Defines at compile time a permutation dimension made of specific int32. */
template <int32... Ts>
struct TShaderPermutationSparseInt
{
	/** Setup the dimension's type in permutation domain as integer. */
	using Type = int32;

	/** Setup the dimension's number of permutation. */
	static constexpr int32 PermutationCount = 0;
	
	/** Setup the dimension as non multi-dimensional, so that the ModifyCompilationEnvironement's
	 * define can conventily be set up in SHADER_PERMUTATION_SPARSE_INT.
	 */
	static constexpr bool IsMultiDimensional = false;


	/** Converts dimension's integer value to dimension's value id, bu in this case fail because the dimension value was wrong. */
	static int32 ToDimensionValueId(Type E)
	{
		checkf(false, TEXT("Unknown shader permutation dimension value %i."), E);
		return int32(0);
	}

	/** Converts dimension's value id to dimension's integer value (exact reciprocal of ToDimensionValueId). */
	static Type FromDimensionValueId(int32 PermutationId)
	{
		checkf(false, TEXT("Invalid shader permutation dimension id %i."), PermutationId);
		return Type(0);
	}
};

template <int32 TUniqueValue, int32... Ts>
struct TShaderPermutationSparseInt<TUniqueValue, Ts...>
{
	/** Setup the dimension's type in permutation domain as integer. */
	using Type = int32;

	/** Setup the dimension's number of permutation. */
	static constexpr int32 PermutationCount = TShaderPermutationSparseInt<Ts...>::PermutationCount + 1;
	
	/** Setup the dimension as non multi-dimensional, so that the ModifyCompilationEnvironement's
	 * define can conventily be set up in SHADER_PERMUTATION_SPARSE_INT.
	 */
	static constexpr bool IsMultiDimensional = false;


	/** Converts dimension's integer value to dimension's value id. */
	static int32 ToDimensionValueId(Type E)
	{
		if (E == TUniqueValue)
		{
			return PermutationCount - 1;
		}
		return TShaderPermutationSparseInt<Ts...>::ToDimensionValueId(E);
	}

	/** Pass down a int32 to FShaderCompilerEnvironment::SetDefine(). */
	static int32 ToDefineValue(Type E)
	{
		return int32(E);
	}

	/** Converts dimension's value id to dimension's integer value (exact reciprocal of ToDimensionValueId). */
	static Type FromDimensionValueId(int32 PermutationId)
	{
		if (PermutationId == PermutationCount - 1)
		{
			return TUniqueValue;
		}
		return TShaderPermutationSparseInt<Ts...>::FromDimensionValueId(PermutationId);
	}
};


/** Variadic template that defines an arbitrary multi-dimensional permutation domain, that can be instantiated to represent
 * a vector within the domain.
 *
 * // Defines a permutation domain with arbitrary number of dimensions. Dimensions can themselves be domains.
 * // It is totally legal to have a domain with no dimensions.
 * class FMyPermutationDomain = TShaderPermutationDomain<FMyDimensionA, FMyDimensionB, FMyDimensionC>;
 *
 * // ...
 *
 * // Create a permutation vector to be initialized. By default a vector is set at the origin of the domain.
 * // The origin of the domain is the ShaderPermutationId == 0.
 * FMyPermutationDomain PermutationVector;
 *
 * // Set the permutation vector's dimensions.
 * PermutationVector.Set<FMyDimensionA>(MyDimensionValueA);
 * PermutationVector.Set<FMyDimensionB>(MyDimensionValueB);
 * PermutationVector.Set<FMyDimensionC>(MyDimensionValueC);
 *
 * // Get the permutation id from the permutation vector for shader compiler.
 * int32 ShaderPermutationId = PermutationVector.ToDimensionValueId();
 *
 * // Reconstruct the permutation vector from shader permutation id.
 * FMyPermutationDomain PermutationVector2(ShaderPermutationId);
 *
 * // Get permutation vector's dimension.
 * if (PermutationVector2.Get<FMyDimensionA>())
 * { }
 */
template <typename... Ts>
struct TShaderPermutationDomain
{
	/** Setup the dimension's type in permutation domain as itself so that a permutation domain can be
	 * used as a dimension of another domain.
	 */
	using Type = TShaderPermutationDomain<Ts...>;

	/** Define a domain as a multidimensional dimension so that ModifyCompilationEnvironment() is getting used. */
	static constexpr bool IsMultiDimensional = true;

	/** Total number of permutation within the domain is one if no dimension at all. */
	static constexpr int32 PermutationCount = 1;


	/** Constructors. */
	TShaderPermutationDomain<Ts...>() {}
	explicit TShaderPermutationDomain<Ts...>(int32 PermutationId)
	{
		checkf(PermutationId == 0, TEXT("Invalid shader permutation id %i."), PermutationId);
	}


	/** Set dimension's value, but in this case emit compile time error if could not find the dimension to set. */
	template<class DimensionToSet>
	void Set(typename DimensionToSet::Type)
	{
		// On clang, we can't do static_assert(false), because is evaluated even when method is not used. So
		// we test sizeof(DimensionToSet::Type) == 0 to make the static assert depend on the DimensionToSet
		// template parameter.
		static_assert(sizeof(typename DimensionToSet::Type) == 0, "Unknown shader permutation dimension.");
	}

	/** get dimension's value, but in this case emit compile time error if could not find the dimension to get. */
	template<class DimensionToGet>
	const typename DimensionToGet::Type Get() const
	{
		// On clang, we can't do static_assert(false), because is evaluated even when method is not used. So
		// we test sizeof(DimensionToSet::Type) == 0 to make the static assert depend on the DimensionToGet
		// template parameter.
		static_assert(sizeof(typename DimensionToGet::Type) == 0, "Unknown shader permutation dimension.");
		return DimensionToGet::Type();
	}


	/** Modify compilation environment. */
	void ModifyCompilationEnvironment(FShaderCompilerEnvironment& OutEnvironment) const {}


	/** Converts domain permutation vector to domain's value id. */
	static int32 ToDimensionValueId(const Type& PermutationVector)
	{
		return 0;
	}

	int32 ToDimensionValueId() const
	{
		return ToDimensionValueId(*this);
	}


	/** Returns the permutation domain from the unique ID. */
	static Type FromDimensionValueId(const int32 PermutationId)
	{
		return Type(PermutationId);
	}


	/** Test if equal. */
	bool operator==(const Type& Other) const
	{
		return true;
	}
};


// C++11 doesn't allow partial specialization of templates method or function. So we spetialise class that have
// non spetialised static method, but leave templated static function.
template<bool BooleanSpetialization>
class TShaderPermutationDomainSpetialization
{
public:

	template<typename TPermutationVector, typename TDimension>
	static void ModifyCompilationEnvironment(const TPermutationVector& PermutationVector, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TDimension::DefineName, TDimension::ToDefineValue(PermutationVector.DimensionValue));
		return PermutationVector.Tail.ModifyCompilationEnvironment(OutEnvironment);
	}

	template<typename TPermutationVector, typename TDimensionToGet>
	static const typename TDimensionToGet::Type& GetDimension(const TPermutationVector& PermutationVector)
	{
		return PermutationVector.Tail.template Get<TDimensionToGet>();
	}

	template<typename TPermutationVector, typename TDimensionToSet>
	static void SetDimension(TPermutationVector& PermutationVector, const typename TDimensionToSet::Type& Value)
	{
		return PermutationVector.Tail.template Set<TDimensionToSet>(Value);
	}

};

template<>
class TShaderPermutationDomainSpetialization<true>
{
public:

	template<typename TPermutationVector, typename TDimension>
	static void ModifyCompilationEnvironment(const TPermutationVector& PermutationVector, FShaderCompilerEnvironment& OutEnvironment)
	{
		PermutationVector.DimensionValue.ModifyCompilationEnvironment(OutEnvironment);
		return PermutationVector.Tail.ModifyCompilationEnvironment(OutEnvironment);
	}

	template<typename TPermutationVector, typename TDimensionToGet>
	static const typename TDimensionToGet::Type& GetDimension(const TPermutationVector& PermutationVector)
	{
		return PermutationVector.DimensionValue;
	}

	template<typename TPermutationVector, typename TDimensionToSet>
	static void SetDimension(TPermutationVector& PermutationVector, const typename TDimensionToSet::Type& Value)
	{
		PermutationVector.DimensionValue = Value;
	}

};


template <typename TDimension, typename... Ts>
struct TShaderPermutationDomain<TDimension, Ts...>
{
	/** Setup the dimension's type in permutation domain as itself so that a permutation domain can be
	 * used as a dimension of another domain.
	 */
	using Type = TShaderPermutationDomain<TDimension, Ts...>;

	/** Define a domain as a multidimensional dimension so that ModifyCompilationEnvironment() is used. */
	static constexpr bool IsMultiDimensional = true;

	/** Parent type in the variadic template to reduce code. */
	using Super = TShaderPermutationDomain<Ts...>;

	/** Total number of permutation within the domain. */
	static constexpr int32 PermutationCount = Super::PermutationCount * TDimension::PermutationCount;


	/** Constructors. */
	TShaderPermutationDomain<TDimension, Ts...>()
		: DimensionValue(TDimension::FromDimensionValueId(0))
	{
	}

	explicit TShaderPermutationDomain<TDimension, Ts...>(int32 PermutationId)
		: DimensionValue(TDimension::FromDimensionValueId(PermutationId % TDimension::PermutationCount))
		, Tail(PermutationId / TDimension::PermutationCount)
	{
		checkf(PermutationId >= 0 && PermutationId < PermutationCount, TEXT("Invalid shader permutation id %i."), PermutationId);
	}


	/** Set dimension's value. */
	template<class DimensionToSet>
	void Set(typename DimensionToSet::Type Value)
	{
		return TShaderPermutationDomainSpetialization<TIsSame<TDimension, DimensionToSet>::Value>::template SetDimension<Type, DimensionToSet>(*this, Value);
	}


	/** Get dimension's value. */
	template<class DimensionToGet>
	const typename DimensionToGet::Type& Get() const
	{
		return TShaderPermutationDomainSpetialization<TIsSame<TDimension, DimensionToGet>::Value>::template GetDimension<Type, DimensionToGet>(*this);
	}


	/** Modify the shader's compilation environment. */
	void ModifyCompilationEnvironment(FShaderCompilerEnvironment& OutEnvironment) const
	{
		TShaderPermutationDomainSpetialization<TDimension::IsMultiDimensional>::template ModifyCompilationEnvironment<Type, TDimension>(*this, OutEnvironment);
	}


	/** Converts domain permutation vector to domain's value id. */
	static int32 ToDimensionValueId(const Type& PermutationVector)
	{
		return PermutationVector.ToDimensionValueId();
	}

	int32 ToDimensionValueId() const
	{
		return TDimension::ToDimensionValueId(DimensionValue) + TDimension::PermutationCount * Tail.ToDimensionValueId();
	}


	/** Returns the permutation domain from the unique ID. */
	static Type FromDimensionValueId(const int32 PermutationId)
	{
		return Type(PermutationId);
	}


	/** Test if equal. */
	bool operator==(const Type& Other) const
	{
		return DimensionValue == Other.DimensionValue && Tail == Other.Tail;
	}

	/** Test if not equal. */
	bool operator!=(const Type& Other) const
	{
		return !(*this == Other);
	}

private:
	template<bool BooleanSpetialization>
	friend class TShaderPermutationDomainSpetialization;

	typename TDimension::Type DimensionValue;
	Super Tail;
};


/** Global shader permutation domain with no dimension. */
using FShaderPermutationNone = TShaderPermutationDomain<>;


// Internal implementation of non multi-dimensional shader permutation dimension.
#define DECLARE_SHADER_PERMUTATION_IMPL(InDefineName,PermutationMetaType,...) \
	public PermutationMetaType<__VA_ARGS__> { \
	public: \
		static constexpr const TCHAR* DefineName = TEXT(InDefineName); \
	}


/** Implements a boolean shader permutation dimensions. Meant to be used like so:
 *
 * class FMyShaderDim : SHADER_PERMUTATION_BOOL("MY_SHADER_DEFINE_NAME");
 */
#define SHADER_PERMUTATION_BOOL(InDefineName) \
	public FShaderPermutationBool { \
	public: \
		static constexpr const TCHAR* DefineName = TEXT(InDefineName); \
	}
	
/** Implements an integer shader permutation dimensions with N permutation values from [[0; N[[. Meant to be used like so:
 *
 * class FMyShaderDim : SHADER_PERMUTATION_INT("MY_SHADER_DEFINE_NAME", N);
 */
#define SHADER_PERMUTATION_INT(InDefineName, Count) \
	DECLARE_SHADER_PERMUTATION_IMPL(InDefineName, TShaderPermutationInt, int32, Count)
	
/** Implements an integer shader permutation dimensions with N permutation values from [[X; X+N[[. Meant to be used like so:
 *
 * class FMyShaderDim : SHADER_PERMUTATION_RANGE_INT("MY_SHADER_DEFINE_NAME", X, N);
 */
#define SHADER_PERMUTATION_RANGE_INT(InDefineName, Start, Count) \
	DECLARE_SHADER_PERMUTATION_IMPL(InDefineName, TShaderPermutationInt, int32, Count, Start)

/** Implements an integer shader permutation dimensions with non contiguous permutation values. Meant to be used like so:
 *
 * class FMyShaderDim : SHADER_PERMUTATION_SPARSE_INT("MY_SHADER_DEFINE_NAME", 1, 2, 4, 8);
 */
#define SHADER_PERMUTATION_SPARSE_INT(InDefineName,...) \
	DECLARE_SHADER_PERMUTATION_IMPL(InDefineName, TShaderPermutationSparseInt, __VA_ARGS__)
	
/** Implements an shader permutation dimensions with an enum class assumed to have contiguous integer values. Meant to be used like so:
 *
 * enum class EMyEnum
 * {
 *		Hello,
 *		World,
 *		// [...]
 *		MAX
 * };
 *
 * class FMyShaderDim : SHADER_PERMUTATION_ENUM_CLASS("MY_SHADER_DEFINE_NAME", EMyEnum);
 */
#define SHADER_PERMUTATION_ENUM_CLASS(InDefineName, EnumName) \
	DECLARE_SHADER_PERMUTATION_IMPL(InDefineName, TShaderPermutationInt, EnumName, static_cast<int32>(EnumName::MAX))
	