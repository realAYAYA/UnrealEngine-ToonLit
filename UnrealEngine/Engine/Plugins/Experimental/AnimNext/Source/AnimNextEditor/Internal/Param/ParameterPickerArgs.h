// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Delegates/DelegateCombinations.h"
#include "AssetRegistry/AssetData.h"

namespace UE::AnimNext::Editor
{

// A parameter asset, optionally bound in a block
struct FParameterBindingReference
{
	FParameterBindingReference() = default;

	FParameterBindingReference(const FName& InParameter, const FAssetData& InLibrary, const FAssetData& InBlock = FAssetData())
		: Parameter(InParameter)
		, Library(InLibrary)
		, Block(InBlock)
	{
	}

	// Parameter name
	FName Parameter;

	// Library asset
	FAssetData Library;

	// Optional block asset that the parameter is bound in
	FAssetData Block;
};

// Delegate called when a parameter has been picked. Block argument is invalid when an unbound parameter is chosen.
DECLARE_DELEGATE_OneParam(FOnGetParameterBindings, TArray<FParameterBindingReference>& /*OutParameterBindings*/);

// Result of a filter operation via FOnFilterParameter
enum class EFilterParameterResult : int32
{
	Include,
	Exclude
};

// Delegate called to filter parameters for display to the user
DECLARE_DELEGATE_RetVal_OneParam(EFilterParameterResult, FOnFilterParameter, const FParameterBindingReference& /*InParameterBinding*/);

struct FParameterPickerArgs
{
	FParameterPickerArgs() = default;

	// Ptr to existing called delegate to which the picker will register a function which returns the selected parameter
	// bindings
	FOnGetParameterBindings* OnGetParameterBindings = nullptr;

	// Delegate used to signal whether selection has changed
	FSimpleDelegate OnSelectionChanged;

	// Delegate called to filter parameters for display to the user
	FOnFilterParameter OnFilterParameter;

	// Whether we should show parameters that are bound in a parameter block
	bool bShowBoundParameters = true;

	// Whether we should show parameters that are not bound in a parameter block
	bool bShowUnboundParameters = true;	
};

}
