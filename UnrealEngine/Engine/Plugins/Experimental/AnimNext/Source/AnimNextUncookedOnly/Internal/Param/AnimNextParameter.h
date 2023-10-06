// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamType.h"
#include "AnimNextParameter.generated.h"

class UAnimNextParameter;

namespace UE::AnimNext::Editor
{
struct FUtils;
struct FParameterPickerEntry;
class SParameterBlockView;
class SParameterLibraryViewRow;
}

namespace UE::AnimNext::UncookedOnly
{
// A delegate for subscribing / reacting to parameter modifications.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnParameterModified, UAnimNextParameter* /* InParameter */);
}

/** Defines a single parameter */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameter : public UObject
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	friend class UAnimNextParameterBlock_EditorData;
	friend class UAnimNextParameterLibrary;
	friend class UAnimNextParameterBlockBinding;
	friend class UAnimNextParameterBlockBindingReference;
	friend struct UE::AnimNext::Editor::FUtils;
	friend class UE::AnimNext::Editor::SParameterBlockView;
	friend struct UE::AnimNext::Editor::FParameterPickerEntry;
	friend class UE::AnimNext::Editor::SParameterLibraryViewRow;
	friend struct FRigVMDispatch_SetParameter;

	// Get the type of this parameter
	const FAnimNextParamType& GetType() const { return Type; }

	// Set the type of this parameter
	ANIMNEXTUNCOOKEDONLY_API void SetType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true);

	// Get the comment for this parameter
	FStringView GetComment() const { return Comment; }

	// Returns the modified event which can be used to subscribe to changes to this parameter
	ANIMNEXTUNCOOKEDONLY_API UE::AnimNext::UncookedOnly::FOnParameterModified& OnModified();

	// UObject interface 
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual bool IsAsset() const override;

	/** The parameter's type */
	UPROPERTY(EditAnywhere, Category = "Parameter", AssetRegistrySearchable)
	FAnimNextParamType Type = FAnimNextParamType::GetType<bool>();
	
	/** Comment to display in editor */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta=(MultiLine))
	FString Comment;

	// Delegate to subscribe to modifications
	UE::AnimNext::UncookedOnly::FOnParameterModified ModifiedDelegate;
#endif // #if WITH_EDITORONLY_DATA
};