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
	struct FNodeParameters {
		FName Name;
		UObject* OwningObject = nullptr;
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
struct FDataflowNode
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
	DATAFLOWCORE_API virtual FString GetToolTip();
	DATAFLOWCORE_API FString GetPinToolTip(const FName& PropertyName);
	DATAFLOWCORE_API FText GetPinDisplayName(const FName& PropertyName);
	DATAFLOWCORE_API TArray<FString> GetPinMetaData(const FName& PropertyName);
	virtual TArray<Dataflow::FRenderingParameter> GetRenderParameters() const { return GetRenderParametersImpl(); }
	// Copy node property values from another node
	UE_DEPRECATED(5.4, "FDataflowNode::CopyNodeProperties is deprecated.")
	DATAFLOWCORE_API void CopyNodeProperties(const TSharedPtr<FDataflowNode> CopyFromDataflowNode);

	virtual bool IsDeprecated() { return false; }
	virtual bool IsExperimental() { return false; }

	//
	// Connections
	//

	DATAFLOWCORE_API TArray<Dataflow::FPin> GetPins() const;

	/** Override this function to add the AddOptionPin functionality to the node's context menu. */
	virtual Dataflow::FPin AddPin() { return { Dataflow::FPin::EDirection::NONE, NAME_None, NAME_None }; }
	/** Override this function to add the AddOptionPin functionality to the node's context menu. */
	virtual bool CanAddPin() const { return false; }
	/** Override this function to add the RemoveOptionPin functionality to the node's context menu. */
	virtual Dataflow::FPin GetPinToRemove() const { return { Dataflow::FPin::EDirection::NONE, NAME_None, NAME_None }; }
	UE_DEPRECATED(5.4, "Use GetPinToRemove and OnPinRemoved instead.")
	virtual Dataflow::FPin RemovePin() { return GetPinToRemove(); }
	/** 
	 * Override this to update any bookkeeping when a pin is being removed.
	 * This will be called before the pin is unregistered as an input.
	 */
	virtual void OnPinRemoved(const Dataflow::FPin& Pin) {}
	/** Override this function to add the RemoveOptionPin functionality to the node's context menu. */
	virtual bool CanRemovePin() const { return false; }

	DATAFLOWCORE_API virtual void AddInput(FDataflowInput* InPtr);
	DATAFLOWCORE_API TArray< FDataflowInput* > GetInputs() const;
	DATAFLOWCORE_API void ClearInputs();

	DATAFLOWCORE_API FDataflowInput* FindInput(FName Name);
	DATAFLOWCORE_API FDataflowInput* FindInput(void* Reference);
	DATAFLOWCORE_API const FDataflowInput* FindInput(const void* Reference) const;
	DATAFLOWCORE_API const FDataflowInput* FindInput(const FGuid& InGuid) const;

	DATAFLOWCORE_API virtual void AddOutput(FDataflowOutput* InPtr);
	DATAFLOWCORE_API int NumOutputs() const;
	DATAFLOWCORE_API TArray< FDataflowOutput* > GetOutputs() const;
	DATAFLOWCORE_API void ClearOutputs();

	DATAFLOWCORE_API FDataflowOutput* FindOutput(FName Name);
	DATAFLOWCORE_API FDataflowOutput* FindOutput(void* Reference);
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(FName Name) const;
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(const void* Reference) const;
	DATAFLOWCORE_API const FDataflowOutput* FindOutput(const FGuid& InGuid) const;

	/** Return a property's byte offset from the dataflow base node address using the full property name (must includes its parent struct property names). */
	uint32 GetPropertyOffset(const FName& PropertyFullName) const;

	static DATAFLOWCORE_API const FName DataflowInput;
	static DATAFLOWCORE_API const FName DataflowOutput;
	static DATAFLOWCORE_API const FName DataflowPassthrough;
	static DATAFLOWCORE_API const FName DataflowIntrinsic;

	static DATAFLOWCORE_API const FLinearColor DefaultNodeTitleColor;
	static DATAFLOWCORE_API const FLinearColor DefaultNodeBodyTintColor;

	/** Override this method to provide custom serialization for this node. */
	virtual void Serialize(FArchive& Ar) {}

	/** Called by editor toolkits when the node is selected, or already selected and invalidated. */
	virtual void OnSelected(Dataflow::FContext& Context) {}
	/** Called by editor toolkits when the node is deselected. */
	virtual void OnDeselected() {}

	//
	//  Struct Support
	//

	virtual void SerializeInternal(FArchive& Ar) { check(false); }
	virtual FStructOnScope* NewStructOnScope() { return nullptr; }
	virtual const UScriptStruct* TypedScriptStruct() const { return nullptr; }

	/** Register the Input and Outputs after the creation in the factory. Use PropertyName to disambiguate a struct name from its first property. */
	DATAFLOWCORE_API void RegisterInputConnection(const void* Property, const FName& PropertyName = NAME_None);
	DATAFLOWCORE_API void RegisterOutputConnection(
		const void* Property,
		const void* Passthrough = nullptr,
		const FName& PropertyName = NAME_None,
		const FName& PassthroughName = NAME_None);
	/** Unregister the input connection if one exists matching this property, and then invalidate the graph. */
	DATAFLOWCORE_API void UnregisterInputConnection(const void* Property, const FName& PropertyName = NAME_None);
	/** Unregister the connection if one exists matching this pin, then invalidate the graph. */
	DATAFLOWCORE_API void UnregisterPinConnection(const Dataflow::FPin& Pin);

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
	*   Set the value of the Reference output.
	* 
	*   Note: If the compiler errors out with "You cannot bind an lvalue to an rvalue reference", then simply remove
	*         the explicit template parameter from the function call to allow for a const reference type to be deducted.
	*         const int32 Value = 0; SetValue<int32>(Context, Value, &Reference);  // Error: You cannot bind an lvalue to an rvalue reference
	*         const int32 Value = 0; SetValue(Context, Value, &Reference);  // Fine
	*         const int32 Value = 0; SetValue<const int32&>(Context, Value, &Reference);  // Fine
	* 
	*   @param Context : The evaluation context that holds the data store.
	*   @param Value : The value to store in the contexts data store. 
	*   @param Reference : Pointer to a member of this node that corresponds with the output to set. 
	*/
	template<class T> void SetValue(Dataflow::FContext& Context, T&& Value, const typename TDecay<T>::Type* Reference) const
	{
		checkSlow(FindOutput(Reference));
		FindOutput(Reference)->SetValue(Forward<T>(Value), Context);
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

	DATAFLOWCORE_API void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current());

	virtual void OnInvalidate() {}

	DATAFLOWCORE_API virtual bool ValidateConnections();

	bool HasValidConnections() const { return bHasValidConnections; }

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
	static FString GetPropertyFullNameString(const TConstArrayView<const FProperty*>& PropertyChain);
	static FName GetPropertyFullName(const TArray<const FProperty*>& PropertyChain);
	static FText GetPropertyDisplayNameText(const TArray<const FProperty*>& PropertyChain);
	static uint32 GetPropertyOffset(const TArray<const FProperty*>& PropertyChain);

	/**
	* Find a property using the property address and name (not including its parent struct property names).
	* If NAME_None is used as the name, and the same address is shared by a parent structure property and
	* its first child property, then the parent will be returned.
	*/
	const FProperty* FindProperty(const UStruct* Struct, const void* Property, const FName& PropertyName, TArray<const FProperty*>* OutPropertyChain = nullptr) const;

	/** Find a property using the property full name (must includes its parent struct property names). */
	const FProperty* FindProperty(const UStruct* Struct, const FName& PropertyFullName, TArray<const FProperty*>* OutPropertyChain = nullptr) const;

	virtual TArray<Dataflow::FRenderingParameter> GetRenderParametersImpl() const { return TArray<Dataflow::FRenderingParameter>(); }


	bool bHasValidConnections = true;

protected:
	FOnNodeInvalidated OnNodeInvalidatedDelegate;

};

namespace Dataflow
{
	//
	// Use these macros to register dataflow nodes. 
	//

#define DATAFLOW_NODE_REGISTER_CREATION_FACTORY(A)									\
	::Dataflow::FNodeFactory::GetInstance()->RegisterNode(							\
		{A::StaticType(),A::StaticDisplay(),A::StaticCategory(),					\
			A::StaticTags(),A::StaticToolTip()},									\
		[](const ::Dataflow::FNewNodeParameters& InParam){							\
				TUniquePtr<A> Val = MakeUnique<A>(::Dataflow::FNodeParameters{		\
					InParam.Name, InParam.OwningObject}, InParam.Guid);				\
				Val->ValidateConnections(); return Val;});

#define DATAFLOW_NODE_RENDER_TYPE(A, B)												\
	virtual TArray<::Dataflow::FRenderingParameter> GetRenderParametersImpl() const {		\
		TArray<::Dataflow::FRenderingParameter> Array;								\
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
		Struct, nullptr);															\
		Serialize(Ar);}																\
	virtual FName GetDisplayName() const override { return TYPE::StaticDisplay(); }	\
	virtual FName GetCategory() const override { return TYPE::StaticCategory(); }	\
	virtual FString GetTags() const override { return TYPE::StaticTags(); }			\
	virtual const UScriptStruct* TypedScriptStruct() const override					\
		{return TYPE::StaticStruct();}												\
	TYPE() {}																		\
private:

#define DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY(A, C1, C2)	\
{																					\
	::Dataflow::FNodeColorsRegistry::Get().RegisterNodeColors(A, {C1, C2});			\
}																					\

}

