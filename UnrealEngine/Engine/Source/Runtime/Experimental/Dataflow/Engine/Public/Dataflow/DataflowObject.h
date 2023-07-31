// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowCore.h"
#include "EdGraph/EdGraph.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "DataflowObject.generated.h"

class FArchive;
class UDataflow;
class UObject;
namespace Dataflow { class FGraph; }


/**
*	FFleshAssetEdit
*     Structured RestCollection access where the scope
*     of the object controls serialization back into the
*     dynamic collection
*
*/
class DATAFLOWENGINE_API FDataflowAssetEdit
{
public:
	typedef TFunctionRef<void()> FPostEditFunctionCallback;
	friend UDataflow;

	/**
	 * @param UDataflow				The "FAsset" to edit
	 */
	FDataflowAssetEdit(UDataflow *InAsset, FPostEditFunctionCallback InCallable);
	~FDataflowAssetEdit();

	Dataflow::FGraph* GetGraph();

private:
	FPostEditFunctionCallback PostEditCallback;
	UDataflow* Asset;
};

/**
* UDataflow (UObject)
*
* UObject wrapper for the Dataflow::FGraph
*
*/
UCLASS(BlueprintType, customconstructor)
class DATAFLOWENGINE_API UDataflow : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	TSharedPtr<Dataflow::FGraph, ESPMode::ThreadSafe> Dataflow;
	void PostEditCallback();

public:
	UDataflow(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, Category = "Evaluation")
	bool bActive = true;

	UPROPERTY(EditAnywhere, Category = "Evaluation", AdvancedDisplay )
	TArray<TObjectPtr<UObject>> Targets;

	/** UObject Interface */
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	/** End UObject Interface */

	void Serialize(FArchive& Ar);

	/** Accessors for internal geometry collection */
	const TSharedPtr<Dataflow::FGraph, ESPMode::ThreadSafe> GetDataflow() const { return Dataflow; }

	/**Editing the collection should only be through the edit object.*/
	FDataflowAssetEdit EditDataflow() const {
		//ThisNC is a by-product of editing through the component.
		UDataflow* ThisNC = const_cast<UDataflow*>(this);
		return FDataflowAssetEdit(ThisNC, [ThisNC]() {ThisNC->PostEditCallback(); });
	}
};

