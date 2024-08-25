// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphRenderLayerSubsystem.h"

#include "MovieGraphModifierNode.generated.h"

/** A container which allows an array of modifiers to be merged correctly. */
UCLASS()
class UMovieGraphMergeableModifierContainer final : public UObject, public IMovieGraphTraversableObject
{
	GENERATED_BODY()

public:
	// IMovieGraphTraversableObject interface
	virtual void Merge(const IMovieGraphTraversableObject* InSourceObject) override;
	virtual TArray<TPair<FString, FString>> GetMergedProperties() const override;
	// ~IMovieGraphTraversableObject interface

private:
	void MergeProperties(const TObjectPtr<UMovieGraphCollectionModifier>& InDestModifier, const TObjectPtr<UMovieGraphCollectionModifier>& InSourceModifier);

public:
	UPROPERTY()
	TArray<TObjectPtr<UMovieGraphCollectionModifier>> Modifiers;
};

/** 
* A collection node specifies an interface for doing dynamic scene queries for actors in the world. Collections work in tandem with
* UMovieGraphModifiers to select which actors the modifiers should be run on.
*/
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphModifierNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()

public:
	UMovieGraphModifierNode();

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
#endif

	virtual FString GetNodeInstanceName() const override { return ModifierName; }

	/** Gets the modifier of the specified type, or nullptr if one does not exist on this node. */
	UFUNCTION(BlueprintCallable, Category = "Modifiers")
	UMovieGraphCollectionModifier* GetModifier(TSubclassOf<UMovieGraphCollectionModifier> ModifierType) const;

	/** Gets all modifiers currently added to the node. */
	UFUNCTION(BlueprintCallable, Category = "Modifiers")
	const TArray<UMovieGraphCollectionModifier*>& GetModifiers() const;

	/**
	 * Adds a new modifier of the specified type. Returns a pointer to the new modifier, or nullptr if a modifier of the specified type already
	 * exists on this node (only one modifier of each type can be added to the node).
	 */
	UFUNCTION(BlueprintCallable, Category = "Modifiers")
	UMovieGraphCollectionModifier* AddModifier(TSubclassOf<UMovieGraphCollectionModifier> ModifierType);

	/** Removes the modifier of the specified type. Returns true on success, or false if a modifier of the specified type does not exist on the node. */
	UFUNCTION(BlueprintCallable, Category = "Modifiers")
	bool RemoveModifier(TSubclassOf<UMovieGraphCollectionModifier> ModifierType);

	/** Add a collection identified by the given name which will be affected by the modifiers on this node. */
	UFUNCTION(BlueprintCallable, Category = "Modifiers")
	void AddCollection(const FName& InCollectionName);

	/** Remove a collection identified by the given name. Returns true if the collection was found and removed successfully, else false. */
	UFUNCTION(BlueprintCallable, Category = "Modifiers")
	bool RemoveCollection(const FName& InCollectionName);

	/** Gets all collections that will be affected by the modifiers on this node. */
	UFUNCTION(BlueprintCallable, Category = "Modifiers")
	const TArray<FName>& GetCollections() const;

public:
	UPROPERTY()
	uint8 bOverride_ModifierName : 1 = 1;	// Always merge the modifier name, no need for the user to do this explicitly

	/** The name of this modifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FString ModifierName;

private:
	UPROPERTY()
	uint8 bOverride_Collections : 1 = 1; //-V570

	UPROPERTY()
	uint8 bOverride_ModifiersContainer : 1 = 1; //-V570
	
	/** The names of collections being modified. */
	UPROPERTY()
	TArray<FName> Collections;
	
	/** The modifiers this node should run. */
	UPROPERTY(meta=(DisplayName="Modifiers"))
	TObjectPtr<UMovieGraphMergeableModifierContainer> ModifiersContainer;
};