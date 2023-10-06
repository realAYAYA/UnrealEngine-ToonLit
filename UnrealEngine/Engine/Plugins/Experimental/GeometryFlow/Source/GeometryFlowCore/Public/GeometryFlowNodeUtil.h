// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowNode.h"

namespace UE::GeometryFlow { template <typename T, int32 DataTypeIdentifier> class TMovableData; }
namespace UE::GeometryFlow { template <typename T, int32 StorageTypeIdentifier> class TBasicNodeInput; }
namespace UE::GeometryFlow { template <typename T, int32 StorageTypeIdentifier> class TBasicNodeOutput; }

namespace UE
{
namespace GeometryFlow
{



template<typename DataType>
TSafeSharedPtr<TMovableData<DataType, DataType::DataTypeIdentifier>> MakeMovableData(DataType&& Data)
{
	return MakeShared<TMovableData<DataType, DataType::DataTypeIdentifier>, ESPMode::ThreadSafe>(MoveTemp(Data));
}


template<typename DataType>
TUniquePtr<TBasicNodeInput<DataType, DataType::DataTypeIdentifier>> MakeBasicInput()
{
	return MakeUnique<TBasicNodeInput<DataType, DataType::DataTypeIdentifier>>();
}

template<typename DataType>
TUniquePtr<TBasicNodeOutput<DataType, DataType::DataTypeIdentifier>> MakeBasicOutput()
{
	return MakeUnique<TBasicNodeOutput<DataType, DataType::DataTypeIdentifier>>();
}

template<typename T>
EGeometryFlowResult ExtractData(TSafeSharedPtr<IData> Data,
								T& Storage,
								int32 StorageTypeIdentifier,
								bool bTryTakeResult)
{
	if (Data->GetPayloadType() != StorageTypeIdentifier)
	{
		return EGeometryFlowResult::UnmatchedTypes;
	}
	if (bTryTakeResult)
	{
		Data->GiveTo(Storage, StorageTypeIdentifier);
	}
	else
	{
		Data->GetDataCopy(Storage, StorageTypeIdentifier);
	}

	return EGeometryFlowResult::Ok;

}



}	// end namespace GeometryFlow
}	// end n

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "GeometryFlowCoreNodes.h"
#include "GeometryFlowMovableData.h"
#endif
