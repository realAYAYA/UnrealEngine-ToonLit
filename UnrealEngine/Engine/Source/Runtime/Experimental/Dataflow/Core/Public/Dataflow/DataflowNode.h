// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowConnection.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "UObject/StructOnScope.h"

#include "DataflowNode.generated.h"

struct FDataflowInput;
struct FDataflowOutput;

namespace Dataflow {
	struct DATAFLOWCORE_API FNodeParameters {
		FName Name;
	};
	class FGraph;
}


/**
* FNode
*		Base class for node based evaluation within the Dataflow graph. 
* 
* Note : Do NOT create mutable variables in the classes derived from FDataflowNode. The state
*        is stored on the FContext. The Evaluate is const to allow support for multithreaded
*        evaluation. 
*/
USTRUCT()
struct DATAFLOWCORE_API FDataflowNode
{
	GENERATED_USTRUCT_BODY()

	friend class Dataflow::FGraph;
	friend struct FDataflowConnection;

	FGuid Guid;
	FName Name;
	uint64 LastModifiedTimestamp;

	TMap< int, FDataflowInput* > Inputs;
	TMap< int, FDataflowOutput* > Outputs;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	bool bActive = true;

	FDataflowNode()
		: Guid(FGuid())
		, Name("Invalid")
		, LastModifiedTimestamp(0)
	{
	}

	FDataflowNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Guid(InGuid)
		, Name(Param.Name)
		, LastModifiedTimestamp(0)
	{
	}

	virtual ~FDataflowNode() { ClearInputs(); ClearOutputs(); }

	FGuid GetGuid() const { return Guid; }
	FName GetName() const { return Name; }
	void SetName(FName InName) { Name = InName; }

	virtual FName GetType() const { check(true); return FName("invalid"); }
	virtual FName GetDisplayName() const { return ""; }
	virtual FName GetCategory() const { return ""; }
	virtual FString GetTags() const { return ""; }
	virtual FString GetToolTip() const { return ""; }

	//
	// Connections
	//

	TArray<Dataflow::FPin> GetPins() const;


	void AddInput(FDataflowInput* InPtr);
	TArray< FDataflowInput* > GetInputs() const;
	void ClearInputs();

	FDataflowInput* FindInput(FName Name);
	FDataflowInput* FindInput(void* Reference);
	const FDataflowInput* FindInput(const void* Reference) const;


	void AddOutput(FDataflowOutput* InPtr);
	TArray< FDataflowOutput* > GetOutputs() const;
	void ClearOutputs();

	FDataflowOutput* FindOutput(FName Name);
	FDataflowOutput* FindOutput(void* Reference);
	const FDataflowOutput* FindOutput(const void* Reference) const;



	//
	//  Struct Support
	//

	virtual void SerializeInternal(FArchive& Ar) { check(false); }
	virtual FStructOnScope* NewStructOnScope() { return nullptr; }

	/** Register the Input and Outputs after the creation in the factory */
	void RegisterInputConnection(const void* Property);
	void RegisterOutputConnection(const void* Property, const void* Passthrough = nullptr);

	//
	// Evaluation
	//
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput*) const { ensure(false); }

	/**
	*   GetValue(...)
	*
	*	Get the value of the Reference output, invoking up stream evaluations if not 
	*   cached in the contexts data store. 
	* 
	*   @param Context : The evaluation context that holds the data store.
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set.
	*						*Reference will be used as the default if the input is not connected. 
	*/
	template<class T> const T& GetValue(Dataflow::FContext& Context, const T* Reference) const
	{
		checkSlow(FindInput(Reference));
		return FindInput(Reference)->template GetValue<T>(Context, *Reference);
	}
	
	template<class T>
	TFuture<const T&> GetValueParallel(Dataflow::FContext& Context, const T* Reference) const
	{
		checkSlow(FindInput(Reference));
		return FindInput(Reference)->template GetValueParallel<T>(Context, *Reference);
	}

	/**
	*   GetValue(...)
	*
	*	Get the value of the Reference output, invoking up stream evaluations if not
	*   cached in the contexts data store.
	*
	*   @param Context : The evaluation context that holds the data store.
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set.
	*   @param Default : Default value if the input is not connected.
	*/
	template<class T> const T& GetValue(Dataflow::FContext& Context, const T* Reference, const T& Default) const
	{
		checkSlow(FindInput(Reference));
		return FindInput(Reference)->template GetValue<T>(Context, Default);
	}

	/**
	*   SetValue(...)
	*
	*	Set the value of the Reference output.
	* 
	*   @param Context : The evaluation context that holds the data store.
	*   @param Value : The value to store in the contexts data store. 
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set. 
	*/
	template<class T> void SetValue(Dataflow::FContext& Context, const T& Value, const T* Reference) const
	{
		checkSlow(FindOutput(Reference));
		FindOutput(Reference)->template SetValue<T>(Value, Context);
	}

	void Invalidate();

	void InvalidateOutputs();

	bool ValidateConnections();

	bool IsValid() const { return bValid; }

private:

	bool bValid = true;

};

namespace Dataflow
{
	//
	// Use these macros to register dataflow nodes. 
	//

#define DATAFLOW_NODE_REGISTER_CREATION_FACTORY(A)									\
	FNodeFactory::GetInstance()->RegisterNode(										\
		{A::StaticType(),A::StaticDisplay(),A::StaticCategory(),					\
			A::StaticTags(),A::StaticToolTip()},									\
		[](const FNewNodeParameters& InParam){										\
				TUniquePtr<A> Val = MakeUnique<A>(FNodeParameters{InParam.Name}, InParam.Guid);    \
				Val->ValidateConnections(); return Val;});

#define DATAFLOW_NODE_DEFINE_INTERNAL(TYPE, DISPLAY_NAME, CATEGORY, TAGS)			\
public:																				\
	static FName StaticType() {return #TYPE;}										\
	static FName StaticDisplay() {return DISPLAY_NAME;}								\
	static FName StaticCategory() {return CATEGORY;}								\
	static FString StaticTags() {return TAGS;}										\
	static FString StaticToolTip() {return FString("Create a dataflow node.");}		\
	virtual FName GetType() const { return #TYPE; }									\
	virtual FStructOnScope* NewStructOnScope() override {							\
	   return new FStructOnScope(TYPE::StaticStruct(), (uint8*)this);}				\
	virtual void SerializeInternal(FArchive& Ar) override {							\
		UScriptStruct* const Struct = TYPE::StaticStruct();							\
		Struct->SerializeTaggedProperties(Ar, (uint8*)this,							\
		Struct, nullptr);}															\
	virtual FName GetDisplayName() const override { return TYPE::StaticDisplay(); }	\
	virtual FName GetCategory() const override { return TYPE::StaticCategory(); }	\
	virtual FString GetTags() const override { return TYPE::StaticTags(); }			\
	virtual FString GetToolTip() const override { return TYPE::StaticToolTip(); }	\
	TYPE() {}																		\
private:


}

