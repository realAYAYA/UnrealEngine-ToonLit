// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMHost.h"
#include "AnimNextRigVMAsset.generated.h"

class UAnimNextRigVMAssetEditorData;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

/** Base class for all AnimNext assets that can host RigVM logic */
UCLASS(MinimalAPI, Abstract)
class UAnimNextRigVMAsset : public URigVMHost
{
	GENERATED_BODY()

protected:
	friend class UAnimNextRigVMAssetEditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	UAnimNextRigVMAsset(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override; 

	// The ExtendedExecuteContext object holds the common work data used by the RigVM internals. It is populated during the initial VM initialization.
	// Each instance of an AnimGraph requires a copy of this context and a call to initialize the VM instance with the context copy, 
	// so the cached memory handles are updated to the correct memory addresses.
	// This context is used as a reference to copy the common data for all instances created.
	UPROPERTY(Transient)
	FRigVMExtendedExecuteContext ExtendedExecuteContext;

	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Editor Data", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;
#endif
};
