// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"

struct FInstancedStruct;
class UCustomizableObjectResourceDataContainer;
struct FMutableGraphGenerationContext;

/**
 * An object that gets passed around during Customizable Object compilation to help set up
 * Extension Data.
 */
class CUSTOMIZABLEOBJECTEDITOR_API FExtensionDataCompilerInterface
{
public:
	FExtensionDataCompilerInterface(FMutableGraphGenerationContext& InGenerationContext);

	/**
	 * Register a new Extension Data compile-time constant that will be streamed in on demand.
	 * 
	 * This constant will be cooked out to its own package and will be loaded as needed.
	 * 
	 * Importantly, any UObjects that are referenced by the Extension Data that aren't in an asset
	 * package should have the container returned from this function as their Outer. This ensures
	 * that they get cooked into the correct package for streaming.
	 *
	 * For example, if the Extension Data has a dynamically generated USkeletalMesh that it
	 * references, which doesn't belong to another asset, the USkeletalMesh should have the given
	 * container as its Outer.
	 * 
	 * Assign the constant data to the FInstancedStruct within the container's Data member.
	 * 
	 * The provided FInstancedStruct will be visible to the garbage collector, so any object
	 * references from UPROPERTYs in the instanced struct will be treated as hard references from
	 * the Customizable Object.
	 *
	 * Don't save any references to objects under the container as string paths, e.g. using
	 * TSoftObjectPtr, because the container will be moved to a different package during cooking
	 * and your paths won't be automatically updated.
	 */
	mu::ExtensionDataPtrConst MakeStreamedExtensionData(UCustomizableObjectResourceDataContainer*& OutContainer);

	/**
	 * Register a new Extension Data compile-time constant that will always be loaded.
	 *
	 * This constant will be embedded in the Customizable Object and hence will be loaded in memory
	 * as long as the Customizable Object is loaded.
	 * 
	 * Any UObjects referenced by the provided instanced struct that aren't in an asset package
	 * should be created with the return value of GetOuterForAlwaysLoadedObjects as their Outer.
	 *
	 * Move the constant data into this function and use the resulting mu::ExtensionData in the
	 * mu::Node graph, e.g. set it as the value of a mu::NodeExtensionDataConstant.
	 * 
	 * As with MakeStreamedExtensionData, the provided instanced struct will be visible to the GC.
	 */
	mu::ExtensionDataPtrConst MakeAlwaysLoadedExtensionData(FInstancedStruct&& Data);

	/** The Outer to use for objects owned by always-loaded Extension Data constants. */
	UObject* GetOuterForAlwaysLoadedObjects();

	/**
	* Adds a node to the Generation Context list of generated nodes. This function is meant to be called
	* from classes that implement ICustomizableObjectExtensionNode::GenerateMutableNode for any generated nodes
	* so they are registered against the Mutable compiler
	*/
	void AddGeneratedNode(const class UCustomizableObjectNode* InNode);

	/** Adds a compiler log message to be displayed at the end of the compilation process */
	void CompilerLog(const FText& InLogText, const class UCustomizableObjectNode* InNode);

	FMutableGraphGenerationContext& GenerationContext;
};