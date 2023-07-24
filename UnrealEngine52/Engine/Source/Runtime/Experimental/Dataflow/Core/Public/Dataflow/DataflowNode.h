// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosLog.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowConnection.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "UObject/StructOnScope.h"
#include "Dataflow/DataflowSettings.h"

#include "DataflowNode.generated.h"

struct FDataflowInput;
struct FDataflowOutput;
class UScriptStruct;

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
	Dataflow::FTimestamp LastModifiedTimestamp;

	TMap< int, FDataflowInput* > Inputs;
	TMap< int, FDataflowOutput* > Outputs;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	bool bActive = true;

	FDataflowNode()
		: Guid(FGuid())
		, Name("Invalid")
		, LastModifiedTimestamp(Dataflow::FTimestamp::Invalid)
	{
	}

	FDataflowNode(const Dataflow::FNodeParameters& Param, FGuid InGuid = FGuid::NewGuid())
		: Guid(InGuid)
		, Name(Param.Name)
		, LastModifiedTimestamp(Dataflow::FTimestamp::Invalid)
	{
	}

	virtual ~FDataflowNode() { ClearInputs(); ClearOutputs(); }

	FGuid GetGuid() const { return Guid; }
	FName GetName() const { return Name; }
	void SetName(FName InName) { Name = InName; }
	Dataflow::FTimestamp GetTimestamp() const { return LastModifiedTimestamp;  }

	static FName StaticType() { return FName("FDataflowNode"); }
	virtual FName GetType() const { return StaticType(); }
	virtual FName GetDisplayName() const { return ""; }
	virtual FName GetCategory() const { return ""; }
	virtual FString GetTags() const { return ""; }
	virtual FString GetToolTip();
	FString GetPinToolTip(const FName& PropertyName);
	TArray<FString> GetPinMetaData(const FName& PropertyName);
	virtual TArray<Dataflow::FRenderingParameter> GetRenderParameters() const { return GetRenderParametersImpl(); }

	//
	// Connections
	//

	TArray<Dataflow::FPin> GetPins() const;


	virtual void AddInput(FDataflowInput* InPtr);
	TArray< FDataflowInput* > GetInputs() const;
	void ClearInputs();

	FDataflowInput* FindInput(FName Name);
	FDataflowInput* FindInput(void* Reference);
	const FDataflowInput* FindInput(const void* Reference) const;


	virtual void AddOutput(FDataflowOutput* InPtr);
	int NumOutputs() const;
	TArray< FDataflowOutput* > GetOutputs() const;
	void ClearOutputs();

	FDataflowOutput* FindOutput(FName Name);
	FDataflowOutput* FindOutput(void* Reference);
	const FDataflowOutput* FindOutput(FName Name) const;
	const FDataflowOutput* FindOutput(const void* Reference) const;

	static const FName DataflowInput;
	static const FName DataflowOutput;
	static const FName DataflowPassthrough;
	static const FName DataflowIntrinsic;

	static const FLinearColor DefaultNodeTitleColor;
	static const FLinearColor DefaultNodeBodyTintColor;

	//
	//  Struct Support
	//

	virtual void SerializeInternal(FArchive& Ar) { check(false); }
	virtual FStructOnScope* NewStructOnScope() { return nullptr; }
	virtual const UScriptStruct* TypedScriptStruct() const { return nullptr; }

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

	/**
	*   IsConnected(...)
	*
	*	Checks if Reference input is connected.
	*
	*   @param Reference : Pointer to a member of this node that corresponds with the input.
	*/
	template<class T> bool IsConnected(const T* Reference) const
	{
		checkSlow(FindInput(Reference));
		return (FindInput(Reference)->GetConnection() != nullptr);
	}

	void Invalidate();

	virtual bool ValidateConnections();

	bool IsValid() const { return bValid; }

	virtual bool IsA(FName InType) const 
	{ 
		return InType.ToString().Equals(StaticType().ToString());
	}

	template<class T>
	const T* AsType() const
	{
		FName TargetType = T::StaticType();
		if (IsA(TargetType))
		{
			return (T*)this;
		}
		return nullptr;
	}

	template<class T>
	T* AsType()
	{
		FName TargetType = T::StaticType();
		if (IsA(TargetType))
		{
			return (T*)this;
		}
		return nullptr;
	}

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNodeInvalidated, FDataflowNode*);
	FOnNodeInvalidated& GetOnNodeInvalidatedDelegate() { return OnNodeInvalidatedDelegate; }

private:
	virtual TArray<Dataflow::FRenderingParameter> GetRenderParametersImpl() const { return TArray<Dataflow::FRenderingParameter>(); }


	bool bValid = true;

protected:
	FOnNodeInvalidated OnNodeInvalidatedDelegate;

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

#define DATAFLOW_NODE_RENDER_TYPE(A, B)												\
	virtual TArray<Dataflow::FRenderingParameter> GetRenderParametersImpl() const {		\
		TArray<Dataflow::FRenderingParameter> Array;								\
		Array.Add({ A, {B,} });														\
		return Array;}

#define DATAFLOW_NODE_DEFINE_INTERNAL(TYPE, DISPLAY_NAME, CATEGORY, TAGS)			\
public:																				\
	static FName StaticType() {return #TYPE;}										\
	static FName StaticDisplay() {return DISPLAY_NAME;}								\
	static FName StaticCategory() {return CATEGORY;}								\
	static FString StaticTags() {return TAGS;}										\
	static FString StaticToolTip() {return FString("Create a dataflow node.");}		\
	virtual FName GetType() const { return #TYPE; }									\
	virtual bool IsA(FName InType) const override									\
		{ return InType.ToString().Equals(StaticType().ToString()) || Super::IsA(InType); }	\
	virtual FStructOnScope* NewStructOnScope() override {							\
	   return new FStructOnScope(TYPE::StaticStruct(), (uint8*)this);}				\
	virtual void SerializeInternal(FArchive& Ar) override {							\
		UScriptStruct* const Struct = TYPE::StaticStruct();							\
		Struct->SerializeTaggedProperties(Ar, (uint8*)this,							\
		Struct, nullptr);}															\
	virtual FName GetDisplayName() const override { return TYPE::StaticDisplay(); }	\
	virtual FName GetCategory() const override { return TYPE::StaticCategory(); }	\
	virtual FString GetTags() const override { return TYPE::StaticTags(); }			\
	virtual const UScriptStruct* TypedScriptStruct() const override					\
		{return TYPE::StaticStruct();}												\
	TYPE() {}																		\
private:

#define DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY(A, C1, C2)	\
{																					\
	FNodeColorsRegistry::Get().RegisterNodeColors(A, {C1, C2});						\
}																					\

}

