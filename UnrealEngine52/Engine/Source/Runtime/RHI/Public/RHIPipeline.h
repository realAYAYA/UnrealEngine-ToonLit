// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/EnumClassFlags.h"
#include "Containers/ArrayView.h"
#include "Containers/StaticArray.h"

enum class ERHIPipeline : uint8
{
	Graphics = 1 << 0,
	AsyncCompute = 1 << 1,

	None = 0,
	All = Graphics | AsyncCompute,
	Num = 2
};
ENUM_CLASS_FLAGS(ERHIPipeline)

inline constexpr uint32 GetRHIPipelineIndex(ERHIPipeline Pipeline)
{
	switch (Pipeline)
	{
	default:
	case ERHIPipeline::Graphics:
		return 0;
	case ERHIPipeline::AsyncCompute:
		return 1;
	}
}

inline constexpr uint32 GetRHIPipelineCount()
{
	return uint32(ERHIPipeline::Num);
}

inline TArrayView<const ERHIPipeline> GetRHIPipelines()
{
	static const ERHIPipeline Pipelines[] = { ERHIPipeline::Graphics, ERHIPipeline::AsyncCompute };
	return Pipelines;
}

template <typename FunctionType>
inline void EnumerateRHIPipelines(ERHIPipeline PipelineMask, FunctionType Function)
{
	for (ERHIPipeline Pipeline : GetRHIPipelines())
	{
		if (EnumHasAnyFlags(PipelineMask, Pipeline))
		{
			Function(Pipeline);
		}
	}
}

/** Array of pass handles by RHI pipeline, with overloads to help with enum conversion. */
template <typename ElementType>
class TRHIPipelineArray : public TStaticArray<ElementType, GetRHIPipelineCount()>
{
	using Base = TStaticArray<ElementType, GetRHIPipelineCount()>;
public:
	using Base::Base;

	FORCEINLINE ElementType& operator[](ERHIPipeline Pipeline)
	{
		return Base::operator[](GetRHIPipelineIndex(Pipeline));
	}

	FORCEINLINE const ElementType& operator[](ERHIPipeline Pipeline) const
	{
		return Base::operator[](GetRHIPipelineIndex(Pipeline));
	}
};
