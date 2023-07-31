// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeDataInterface.h"

#include "Templates/SubclassOf.h"

#include "OptimusComputeDataInterface.generated.h"


class UActorComponent;


struct FOptimusCDIPinDefinition
{
	struct FDimensionInfo;
	
	// Singleton value read/write. The context name is implied as Optimus::ContextName::Singleton.
	FOptimusCDIPinDefinition(
		const FName InPinName,
		const FString InDataFunctionName
		) :
		PinName(InPinName),
		DataFunctionName(InDataFunctionName)
	{ }

	// A single level context lookup.
	FOptimusCDIPinDefinition(
		const FName InPinName,
		const FString InDataFunctionName,
		const FName InContextName,
		const FString InCountFunctionName
		) :
		PinName(InPinName),
		DataFunctionName(InDataFunctionName),
		DataDimensions{{InContextName, InCountFunctionName}}
	{ }

	FOptimusCDIPinDefinition(
		const FName InPinName,
		const FString InDataFunctionName,
		const FName InContextName,
		const int32 InMultiplier,
		const FString InCountFunctionName
		) :
		PinName(InPinName),
		DataFunctionName(InDataFunctionName),
		DataDimensions{{InContextName, InCountFunctionName}},
		DomainMultiplier(FMath::Max(1, InMultiplier))
	{ }
	
	FOptimusCDIPinDefinition(
		const FName InPinName,
		const FString InDataFunctionName,
		const std::initializer_list<FDimensionInfo> InContexts
		) :
		PinName(InPinName),
		DataFunctionName(InDataFunctionName)
	{
		for (FDimensionInfo ContextInfo: InContexts)
		{
			DataDimensions.Add(ContextInfo);
		}
	}

	
	// The name of the pin as seen by the user.
	FName PinName;

	// The name of the function that underlies the data access by the pin. The data functions
	// are used to either read or write to data interfaces, whether explicit or implicit.
	// The read functions take zero to N uint indices, determined by the number of count 
	// functions below, and return a value. The write functions take zero to N uint indices,
	// followed by the value, with no return value.
	// For example, for a pin that has two context levels, Vertex and Bone, the lookup function
	// would look something like this:
	//    float GetBoneWeight(uint VertexIndex, uint BoneIndex);
	//    
	// And the matching element count functions for this data function would look like:
	//    uint GetVertexCount();
	//    uint GetVertexBoneCount(uint VertexIndex);
	//
	// Using these examples, the indexes to teh GetBoneWeight function would be limited in range
	// like thus:
	//    0 <= VertexIndex < GetVertexCount()   and
	//    0 <= BoneIndex < GetVertexBoneCount(VertexIndex);
	FString DataFunctionName;

	struct FDimensionInfo
	{
		// The data context for a given context level. For pins to be connectable they need to
		// have identical set of contexts, in order.
		FName ContextName;
		
		// The function to calls to get the item count for the data. If there is no count function
		// name then the data is assumed to be a singleton and will be shown as a value pin rather
		// than a resource pin. Otherwise, the number of count functions defines the dimensionality
		// of the lookup. The first count function returns the count required for the context and
		// should accept no arguments. The second count function takes as index any number between
		// zero and the result of the first count function. For example:
		//   uint GetFirstDimCount();
		//   uint GetSecondDimCount(uint FirstDimIndex);
		// These two results then bound the indices used to call the data function.
		FString CountFunctionName;
	};

	// List of nested data contexts.
	TArray<FDimensionInfo> DataDimensions;

	// For single-level domains, how many values per element of that dimension's range. 
	int32 DomainMultiplier = 1;
};


UCLASS(Abstract, Const)
class OPTIMUSCORE_API UOptimusComputeDataInterface : public UComputeDataInterface
{
	GENERATED_BODY()
	
public:
	struct CategoryName
	{
		static const FName DataInterfaces;
		static const FName ExecutionDataInterfaces;
		static const FName OutputDataInterfaces;
	};

	/// Returns the name to show on the node that will proxy this interface in the graph view.
	virtual FString GetDisplayName() const PURE_VIRTUAL(UOptimusComputeDataInterface::GetDisplayName, return {};)

	/// Returns the category for the node.
	virtual FName GetCategory() const { return CategoryName::DataInterfaces; }

	/// Returns the list of pins that will map to the shader functions provided by this data interface.
	virtual TArray<FOptimusCDIPinDefinition> GetPinDefinitions() const PURE_VIRTUAL(UOptimusComputeDataInterface::GetDisplayName, return {};)

	/**
	 * @return Returns the component type that this data interface operates on.
	 */
	virtual TSubclassOf<UActorComponent> GetRequiredComponentClass() const PURE_VIRTUAL(UOptimusComputeDataInterface::GetRequiredComponent, return nullptr;)

	/**
	 * Register any additional data types provided by this data interface. 
	 */
	 virtual void RegisterTypes() {}

	/// Returns the list of top-level contexts from this data interface. These can be used to
	/// define driver contexts and resource contexts on a kernel. Each nested context will be
	/// non-empty.
	TSet<TArray<FName>> GetUniqueNestedContexts() const;
	
	virtual bool IsVisible() const
	{
		return true;
	}

	/// Returns all known UOptimusComputeDataInterface-derived classes.
	static TArray<TSubclassOf<UOptimusComputeDataInterface>> GetAllComputeDataInterfaceClasses();

	/// Returns the list of all nested contexts from all known data interfaces. These can be 
	/// used to define input/output pin contexts on a kernel.
	static TSet<TArray<FName>> GetUniqueDomainDimensions();

	// Registers types for all known data interfaces.
	static void RegisterAllTypes();
};
