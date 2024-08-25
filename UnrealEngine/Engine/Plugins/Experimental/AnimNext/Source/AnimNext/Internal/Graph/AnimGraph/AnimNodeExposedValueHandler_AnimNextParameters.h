// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/ExposedValueHandler.h"
#include "PropertyAccess.h"
#include "Param/ParamId.h"
#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"
#include "AnimNodeExposedValueHandler_AnimNextParameters.generated.h"

USTRUCT()
struct FAnimNodeExposedValueHandler_AnimNextParameters_Entry
{
	GENERATED_BODY()

	// Parameter to get
	UPROPERTY()
	FName ParameterName;

	// Cached ID for the parameter
	UE::AnimNext::FParamId ParamId;

	// ParamType of the property that this entry will write to
	UPROPERTY()
	FAnimNextParamType PropertyParamType;

	// Cached type handle created from ParamType
	UE::AnimNext::FParamTypeHandle PropertyParamTypeHandle;

	// Property access index
	UPROPERTY()
	int32 AccessIndex = INDEX_NONE;

	// Access operation to perform
	UPROPERTY()
	EPropertyAccessCopyType AccessType = EPropertyAccessCopyType::None;
};

USTRUCT()
struct FAnimNodeExposedValueHandler_AnimNextParameters : public FAnimNodeExposedValueHandler_Base
{
	GENERATED_BODY()

	// FAnimNodeExposedValueHandler interface
	virtual void Initialize(const UClass* InClass) override;
	virtual void Execute(const FAnimationBaseContext& InContext) const override;

	UPROPERTY()
	TArray<FAnimNodeExposedValueHandler_AnimNextParameters_Entry> Entries;

	// Cached property access library ptr
	const FPropertyAccessLibrary* PropertyAccessLibrary;
};

