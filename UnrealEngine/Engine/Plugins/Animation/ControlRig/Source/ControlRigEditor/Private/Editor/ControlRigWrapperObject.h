// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/RigVMDetailsViewWrapperObject.h"
#include "ControlRigWrapperObject.generated.h"

UCLASS()
class CONTROLRIGEDITOR_API UControlRigWrapperObject : public URigVMDetailsViewWrapperObject
{
public:
	GENERATED_BODY()

	virtual UClass* GetClassForStruct(UScriptStruct* InStruct, bool bCreateIfNeeded) const override;
};
