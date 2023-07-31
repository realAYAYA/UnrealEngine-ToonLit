// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTypes)

DEFINE_LOG_CATEGORY(LogStateTree);

const FStateTreeStateHandle FStateTreeStateHandle::Invalid = FStateTreeStateHandle();
const FStateTreeStateHandle FStateTreeStateHandle::Succeeded = FStateTreeStateHandle(FStateTreeStateHandle::SucceededIndex);
const FStateTreeStateHandle FStateTreeStateHandle::Failed = FStateTreeStateHandle(FStateTreeStateHandle::FailedIndex);

const FStateTreeIndex16 FStateTreeIndex16::Invalid = FStateTreeIndex16();
const FStateTreeIndex8 FStateTreeIndex8::Invalid = FStateTreeIndex8();

const FStateTreeExternalDataHandle FStateTreeExternalDataHandle::Invalid = FStateTreeExternalDataHandle();
