// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextInterfaceContext.h"

namespace UE::AnimNext::Interface
{

struct ANIMNEXTINTERFACE_API FKernel
{
public:
	// Runs a 'kernel' predicate that iterates over the supplied (optionally batched) params
	// Number and type of params must match between the predicate & param pack
	// The kernel supports passing a single element parameter where N are expected (as a compacted constant)
	template<typename PredicateType, typename... ParamTypes>
	static void Run(const FContext& InContext, PredicateType InPredicate, ParamTypes&... InArgs)
	{
		// Iterate per-element in the context, applying kernel predicate to all elements
		const uint32 NumElements = InContext.GetNum();
		for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
		{
			if (InContext.ShouldProcessThisElement(ElementIndex) == false)
				continue;

			uint32 ParameterIndex = 0;

			// Call predicate, expanding InArgs parameter pack from TParam<ParamType> -> ParamType
			// and applying correct offset for each param (using pointer arithmetic)
			InPredicate(
				[&ParameterIndex](uint32 InElementIndex, auto& InArg) -> auto&
				{
					auto GetBasePtr = [](auto& InArg)
					{
						if constexpr (TIsDerivedFrom<ParamTypes, FParam>::Value)
						{
							using Type = decltype(typename ParamTypes::TValueType{});
							return static_cast<Type*>(InArg.Data);
						}
						else
						{
							return static_cast<ParamTypes*>(&InArg);
						}
					};

					auto GetOffset = [&ParameterIndex](auto& InArg, int32 InElementIndex) -> int32
					{
						if constexpr (TIsDerivedFrom<ParamTypes, FParam>::Value)
						{
							if (InArg.NumElements > 1)
							{
								check(InElementIndex < InArg.NumElements);
								return InElementIndex;
							}
							else // single elements always use 0 offset, even if used on N element batch
							{
								check(InArg.NumElements > 0);
								return 0;
							}
						}
						else
						{
							return 0;
						}
					};

					// note that this uses pointer arithmetic, so an offset of +1 means adding the sizeof of the param type
					return *(GetBasePtr(InArg) + GetOffset(InArg, InElementIndex));
				}(ElementIndex, InArgs)...
			);
		}
	}
};

} // end namespace UE::AnimNext::Interface