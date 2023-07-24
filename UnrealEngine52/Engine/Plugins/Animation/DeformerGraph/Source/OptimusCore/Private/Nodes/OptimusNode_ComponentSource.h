// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IOptimusComponentBindingProvider.h"
#include "OptimusNode.h"

#include "OptimusComponentSource.h"

#include "OptimusNode_ComponentSource.generated.h"


UCLASS(Hidden)
class UOptimusNode_ComponentSource :
	public UOptimusNode,
	public IOptimusComponentBindingProvider
{
	GENERATED_BODY()
public:
	void SetComponentSourceBinding(
		UOptimusComponentSourceBinding* InBinding
		);

	// UOptimusNode overrides
	FName GetNodeCategory() const override;

	// IOptimusComponentSourceBindingProvider implementation
	UOptimusComponentSourceBinding *GetComponentBinding() const override
	{
		return Binding;
	}

protected:
	friend class UOptimusDeformer;

	// UOptimusNode overrides
	void ConstructNode() override;

	/** Accessor to allow the UOptimusDeformer class to hook up data interface nodes to binding nodes automatically
	 *  for backcomp.
	 */
	UOptimusNodePin* GetComponentPin() const;

	// UObject overrides
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	
private:
	UPROPERTY()
	TObjectPtr<UOptimusComponentSourceBinding> Binding;
};
