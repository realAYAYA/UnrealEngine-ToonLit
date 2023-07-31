// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterfaceContext.h"

namespace UE::DataInterface
{

struct DATAINTERFACE_API FKernel
{
public:
	// Runs a 'kernel' predicate that iterates over the supplied (optionally chunked) params
	// Number and type of params must match between the predicate & param pack
	template<typename PredicateType, typename... ParamTypes>
	static void Run(const FContext& InContext, PredicateType InPredicate, ParamTypes&... InArgs)
	{
		// Cache offsets to apply each element for each parameter
		const uint32 Offsets[sizeof...(InArgs)] =
		{
			[](auto& InArg) -> uint32
			{
				if constexpr (TIsDerivedFrom<ParamTypes, FParam>::Value)
				{
					if(EnumHasAnyFlags(InArg.Flags, FParam::EFlags::Chunked))
					{
						return InArg.GetType().GetSize();
					}
				}

				return 0;
			}(InArgs)...
		};

		// Iterate per-element in the context, applying kernel predicate to all elements
		const uint32 NumElements = InContext.GetNum();
		for(uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
		{
			uint32 ParameterIndex = 0;

			// Call predicate, expanding InArgs parameter pack from TParam<ParamType> -> ParamType
			// and applying correct offset for each param
			InPredicate(
				[&Offsets, &ParameterIndex](uint32 InElementIndex, auto& InArg) -> auto&
				{
					auto GetBasePtr = [](auto& InArg)
					{
						if constexpr (TIsDerivedFrom<ParamTypes, FParam>::Value)
						{
							return static_cast<typename ParamTypes::TValueType*>(InArg.Data);
						}
						else
						{
							return static_cast<ParamTypes*>(&InArg);
						}
					};

					auto GetOffset = [&Offsets, &ParameterIndex](uint32 InElementIndex) -> uint32
					{
						return Offsets[ParameterIndex++] * InElementIndex;
					};
		
					return *(GetBasePtr(InArg) + GetOffset(InElementIndex));
				}(ElementIndex, InArgs)...
			);
		}
	}
};

}