// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeExecutionTypes.h"
#include "StateTreeNodeBase.h"

class FAvaTransitionNodeContext
{
public:
	explicit FAvaTransitionNodeContext(const FStateTreeDataView& InInstanceDataView)
		: InstanceDataView(InInstanceDataView)
	{
	}

	template<typename InNodeType
		UE_REQUIRES(std::is_base_of_v<FStateTreeNodeBase, InNodeType>)>
	const typename InNodeType::FInstanceDataType* GetInstanceData(const InNodeType* InNode) const
	{
		return InstanceDataView.GetPtr<typename InNodeType::FInstanceDataType>();
	}

private:
	const FStateTreeDataView InstanceDataView;
};
