// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedActionKeyMapping.h"
#include "Engine/DataAsset.h"

#include "InputMappingContext.generated.h"

class UInputAction;

/**
* UInputMappingContext : A collection of key to action mappings for a specific input context
* Could be used to:
*	Store predefined controller mappings (allow switching between controller config variants). TODO: Build a system allowing redirects of UInputMappingContexts to handle this.
*	Define per-vehicle control mappings
*	Define context specific mappings (e.g. I switch from a gun (shoot action) to a grappling hook (reel in, reel out, disconnect actions).
*	Define overlay mappings to be applied on top of existing control mappings (e.g. Hero specific action mappings in a MOBA)
*/
UCLASS(BlueprintType, config = Input)
class ENHANCEDINPUT_API UInputMappingContext : public UDataAsset
{
	GENERATED_BODY()

protected:
	// List of key to action mappings.
	UPROPERTY(config, BlueprintReadOnly, EditAnywhere, Category = "Mappings")
	TArray<FEnhancedActionKeyMapping> Mappings;

public:

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif

	// Localized context descriptor
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Description", DisplayName = "Description")
	FText ContextDescription;

	friend class FInputContextDetails;
	friend class FActionMappingsNodeBuilderEx;

public:

	/**
	* Mapping accessors.
	* Note: Use UEnhancedInputLibrary::RequestRebuildControlMappingsForContext to invoke changes made to an FEnhancedActionKeyMapping
	*/
	const TArray<FEnhancedActionKeyMapping>& GetMappings() const { return Mappings; }
	FEnhancedActionKeyMapping& GetMapping(TArray<FEnhancedActionKeyMapping>::SizeType Index) { return Mappings[Index]; }

	// TODO: Don't want to encourage Map/Unmap calls here where context switches would be desirable. These are intended for use in the config/binding screen only.

	/**
	* Map a key to an action within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	FEnhancedActionKeyMapping& MapKey(const UInputAction* Action, FKey ToKey);

	/**
	* Unmap a key from an action within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	void UnmapKey(const UInputAction* Action, FKey Key);

	UE_DEPRECATED(5.1, "UnmapAction has been deprecated, please use UnmapAllKeysFromAction instead.")
	UFUNCTION(BlueprintCallable, Category = "Mapping", meta=(DeprecatedFunction, DeprecatedMessage="UnmapAction has been deprecated, please use UnmapAllKeysFromAction instead."))
	void UnmapAction(const UInputAction* Action) { UnmapAllKeysFromAction(Action); }
	
	/**
	* Unmap all key maps to an action within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	void UnmapAllKeysFromAction(const UInputAction* Action);

	/**
	* Unmap everything within the mapping context.
	*/
	UFUNCTION(BlueprintCallable, Category = "Mapping")
	void UnmapAll();
};

// ************************************************************************************************
// ************************************************************************************************
// ************************************************************************************************
