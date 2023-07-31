// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowCoreNodes.h"
#include "BaseNodes/BinaryOpLambdaNode.h"


namespace UE
{
namespace GeometryFlow
{

	template<typename T, int StorageTypeIdentifier>
	class TBinaryOpAddNode : public TBinaryOpLambdaNode<T, StorageTypeIdentifier>
	{
	public:
		TBinaryOpAddNode() : TBinaryOpLambdaNode<T, StorageTypeIdentifier>([](const T& A, const T& B) { return A + B; })
		{}
	};

	typedef TBinaryOpAddNode<float, (int)EDataTypes::Float> FAddFloatNode;



}	// end namespace GeometryFlow
}	// end namespace UE