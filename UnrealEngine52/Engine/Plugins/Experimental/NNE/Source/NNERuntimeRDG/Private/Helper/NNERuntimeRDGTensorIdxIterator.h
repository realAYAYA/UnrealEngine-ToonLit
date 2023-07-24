// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECoreTensor.h"

namespace UE::NNERuntimeRDG::Private
{
	class TensorIdxIterator
	{
		const NNECore::FTensorShape& TensorShape;
		TArray<uint32, TInlineAllocator<NNECore::FTensorShape::MaxRank>> CurrentPosition;

	public:
		TensorIdxIterator(const NNECore::FTensorShape& InTensorShape);
		bool Advance();
		int32 GetIndexToBroadcastedShape(const NNECore::FTensorShape& InTensorShape) const;
		int32 GetIndex() const;
		TConstArrayView<uint32> GetPositions() const;
		int32 GetIndexFromPosition(TConstArrayView<uint32> Position) const;
	};

} // UE::NNERuntimeRDG::Private
