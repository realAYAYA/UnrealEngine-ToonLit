// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Delegates/DelegateCombinations.h"
#include "AssetRegistry/AssetData.h"
#include "Param/ParamTypeHandle.h"

namespace UE::AnimNext::Editor
{

struct FParameterToAdd
{
	FParameterToAdd() = default;

	FParameterToAdd(const FAnimNextParamType& InType, FName InName)
		: Type(InType)
		, Name(InName)
	{}

	bool IsValid() const
	{
		return Name != NAME_None && Type.IsValid(); 
	}

	bool IsValid(FText& OutReason) const;

	// Type
	FAnimNextParamType Type;

	// Name for parameter
	FName Name;
};

// A parameter asset, optionally bound in a block
struct FParameterBindingReference
{
	FParameterBindingReference() = default;

	FParameterBindingReference(FName InParameter, const FAnimNextParamType& InType, const FAssetData& InBlock = FAssetData())
		: Parameter(InParameter)
		, Type(InType)
		, Block(InBlock)
	{
	}

	// Parameter name
	FName Parameter;

	// Parameter type
	FAnimNextParamType Type;

	// Asset (first found in asset registry) that the parameter is used in
	FAssetData Asset;

	// Optional block asset that the parameter is bound in
	FAssetData Block;
};

// Delegate called when a parameter has been picked. Block argument is invalid when an unbound parameter is chosen.
DECLARE_DELEGATE_OneParam(FOnGetParameterBindings, TArray<FParameterBindingReference>& /*OutParameterBindings*/);

// Delegate called when a parameter has been picked. Block argument is invalid when an unbound parameter is chosen.
DECLARE_DELEGATE_OneParam(FOnParameterPicked, const FParameterBindingReference& /*InParameterBinding*/);

// Delegate called when a parameter is due to be added.
DECLARE_DELEGATE_OneParam(FOnAddParameter, const FParameterToAdd& /*InParameterToAdd*/);

// Result of a filter operation via FOnFilterParameter
enum class EFilterParameterResult : int32
{
	Include,
	Exclude
};

// Delegate called to filter parameters for display to the user
DECLARE_DELEGATE_RetVal_OneParam(EFilterParameterResult, FOnFilterParameter, const FParameterBindingReference& /*InParameterBinding*/);

// Delegate called to filter parameters by type for display to the user
DECLARE_DELEGATE_RetVal_OneParam(EFilterParameterResult, FOnFilterParameterType, const FAnimNextParamType& /*InParameterType*/);

struct FParameterPickerArgs
{
	FParameterPickerArgs() = default;

	// Ptr to existing called delegate to which the picker will register a function which returns the selected parameter
	// bindings
	FOnGetParameterBindings* OnGetParameterBindings = nullptr;

	// Delegate used to signal whether selection has changed
	FSimpleDelegate OnSelectionChanged;

	// Delegate called when a single parameter has been picked
	FOnParameterPicked OnParameterPicked;

	// Delegate called when a parameter, or set of parameters is added
	FOnAddParameter OnAddParameter;

	// Delegate called to filter parameters for display to the user
	FOnFilterParameter OnFilterParameter;

	// Delegate called to filter parameters by type for display to the user
	FOnFilterParameterType OnFilterParameterType;

	// Type to use for any new parameters generated through the picker
	FAnimNextParamType NewParameterType;

	// Whether we allow selecting multiple parameters or just one
	bool bMultiSelect = true;

	// Whether we should show parameters that are bound in a parameter block
	bool bShowBoundParameters = true;

	// Whether we should show parameters that are not bound in a parameter block (if bShowBoundParameters is false this will show all parameters)
	bool bShowUnboundParameters = true;	

	// Whether we should show parameters that are built in
	bool bShowBuiltInParameters = true;

	// Whether we should show the block alongside bound parameters
	bool bShowBlocks = true;

	// Whether we should allow new parameters to be created by this widget
	bool bAllowNew = true;

	// Whether the search box should be focussed on widget creation
	bool bFocusSearchWidget = true;

	// Whether 'none' can be selected
	bool bAllowNone = false;
};

}
