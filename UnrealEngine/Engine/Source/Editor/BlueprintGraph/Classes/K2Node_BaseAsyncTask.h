// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "K2Node.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_BaseAsyncTask.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class FMulticastDelegateProperty;
class UClass;
class UEdGraph;
class UEdGraphPin;
class UEdGraphSchema_K2;
class UFunction;
class UK2Node_CallFunction;
class UK2Node_CustomEvent;
class UK2Node_TemporaryVariable;
class UObject;

/** struct to remap pins for Async Tasks.
 * a single K2 node is shared by many proxy classes.
 * This allows redirecting pins by name per proxy class.
 * Add entries similar to this one in Engine.ini:
 * +K2AsyncTaskPinRedirects=(ProxyClassName="AbilityTask_PlayMontageAndWait", OldPinName="OnComplete", NewPinName="OnBlendOut")
 */

struct FAsyncTaskPinRedirectMapInfo
{
	TMap<FName, TArray<UClass*> > OldPinToProxyClassMap;
};

/** !!! The proxy object should have RF_StrongRefOnFrame flag. !!! */

UCLASS(Abstract)
class BLUEPRINTGRAPH_API UK2Node_BaseAsyncTask : public UK2Node
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
	virtual FString GetPinMetaData(FName InPinName, FName InKey) override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual FText GetToolTipHeading() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	/** Whether or not two pins match for purposes of reconnection after reconstruction.  This allows pins that may have had their names changed via reconstruction to be matched to their old values on a node-by-node basis, if needed*/
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	// End of UK2Node interface

protected:
	// Returns the factory function (checked)
	UFunction* GetFactoryFunction() const;

	/** Determines what the possible redirect pin names are **/
	virtual void GetRedirectPinNames(const UEdGraphPin& Pin, TArray<FString>& RedirectPinNames) const;

	/**
	 * If a the DefaultToSelf pin exists then it needs an actual connection to get properly casted
	 * during compilation.
	 *
	 * @param CompilerContext			The current compiler context used during expansion
	 * @param SourceGraph				The graph to place the expanded self node on
	 * @param IntermediateProxyNode		The spawned intermediate proxy node that has a DefaultToSelfPin
	 *
	 * @return	True if a successful connection was made to an intermediate node
	 *			or if one was not necessary. False if a connection was failed.
	 */
	bool ExpandDefaultToSelfPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* IntermediateProxyNode);
	
protected:
	// The name of the function to call to create a proxy object
	UPROPERTY()
	FName ProxyFactoryFunctionName;

	// The class containing the proxy object functions
	UPROPERTY()
	TObjectPtr<UClass> ProxyFactoryClass;

	// The type of proxy object that will be created
	UPROPERTY()
	TObjectPtr<UClass> ProxyClass;

	// The name of the 'go' function on the proxy object that will be called after delegates are in place, can be NAME_None
	UPROPERTY()
	FName ProxyActivateFunctionName;

	struct BLUEPRINTGRAPH_API FBaseAsyncTaskHelper
	{
		struct FOutputPinAndLocalVariable
		{
			UEdGraphPin* OutputPin;
			UK2Node_TemporaryVariable* TempVar;

			FOutputPinAndLocalVariable(UEdGraphPin* Pin, UK2Node_TemporaryVariable* Var) : OutputPin(Pin), TempVar(Var) {}
		};

		static bool ValidDataPin(const UEdGraphPin* Pin, EEdGraphPinDirection Direction);
		static bool CreateDelegateForNewFunction(UEdGraphPin* DelegateInputPin, FName FunctionName, UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);
		static bool CopyEventSignature(UK2Node_CustomEvent* CENode, UFunction* Function, const UEdGraphSchema_K2* Schema);
		static bool HandleDelegateImplementation(
			FMulticastDelegateProperty* CurrentProperty, const TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs,
			UEdGraphPin* ProxyObjectPin, UEdGraphPin*& InOutLastThenPin, UEdGraphPin*& OutLastActivatedThenPin,
			UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);

		static const FName GetAsyncTaskProxyName();
	};

	// Pin Redirector support
	static TMap<FName, FAsyncTaskPinRedirectMapInfo> AsyncTaskPinRedirectMap;
	static bool bAsyncTaskPinRedirectMapInitialized;

protected:
	/** Expand out the logic to handle the delegate output pins */
	virtual bool HandleDelegates(
		const TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs, UEdGraphPin* ProxyObjectPin,
		UEdGraphPin*& InOutLastThenPin, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);

private:
	/** Invalidates current pin tool tips, so that they will be refreshed before being displayed: */
	void InvalidatePinTooltips() { bPinTooltipsValid = false; }

	/**
	* Creates hover text for the specified pin.
	*
	* @param   Pin				The pin you want hover text for (should belong to this node)
	*/
	void GeneratePinTooltip(UEdGraphPin& Pin) const;

	/** Flag used to track validity of pin tooltips, when tooltips are invalid they will be refreshed before being displayed */
	mutable bool bPinTooltipsValid;
};
