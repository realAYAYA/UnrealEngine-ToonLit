// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"

class IDetailTreeNode;

namespace FNiagaraStackObjectShared
{
	DECLARE_DELEGATE_TwoParams(FOnFilterDetailNodes, const TArray<TSharedRef<IDetailTreeNode>>& /* InSourceNodes */, TArray<TSharedRef<IDetailTreeNode>>& /* OutFilteredNodes */);
}