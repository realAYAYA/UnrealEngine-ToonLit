// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OptimusDataType.h"

#include "CoreMinimal.h"
#include "OptimusComponentSource.h"
#include "OptimusDataDomain.h"
#include "UObject/Object.h"

#include "OptimusResourceDescription.generated.h"


class UOptimusComponentSourceBinding;
class UOptimusDeformer;
class UOptimusPersistentBufferDataInterface;


UCLASS(BlueprintType)
class OPTIMUSCORE_API UOptimusResourceDescription :
	public UObject
{
	GENERATED_BODY()
public:

	UOptimusResourceDescription() = default;

	/** Returns the owning deformer to operate on this resource */
	// FIXME: Move to interface-based system.
	UOptimusDeformer* GetOwningDeformer() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = ResourceDescription)
	FName ResourceName;

	/** The the data type of each element of the resource */
	UPROPERTY(EditAnywhere, Category=ResourceDescription, meta=(UseInResource))
	FOptimusDataTypeRef DataType;

	/** The component binding that this resource description is bound to */
	UPROPERTY(VisibleAnywhere, Category=ResourceDescription)
	TWeakObjectPtr<UOptimusComponentSourceBinding> ComponentBinding;
	
	/** The data domain for this resource. */
	UPROPERTY(EditAnywhere, Category=ResourceDescription, meta=(EditCondition="ComponentBinding != nullptr", HideEditConditionToggle))
	FOptimusDataDomain DataDomain;

	UPROPERTY()
	TObjectPtr<UOptimusPersistentBufferDataInterface> DataInterface;

	// UObject overrides
	void PostLoad() override;
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void PreEditUndo() override;
	void PostEditUndo() override;
#endif

	bool IsValidComponentBinding() const;
	FOptimusDataDomain GetDataDomainFromComponentBinding() const;

private:
#if WITH_EDITORONLY_DATA
	FName ResourceNameForUndo;
#endif
};
