// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "InstancedStruct.h"
#include "StructView.h"
#include "IObjectChooser.generated.h"

#if UE_TRACE_ENABLED && !IS_PROGRAM && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#define CHOOSER_TRACE_ENABLED 1
#else
#define CHOOSER_TRACE_ENABLED 0
#endif

#define CHOOSER_DEBUGGING_ENABLED ((CHOOSER_TRACE_ENABLED) || (WITH_EDITOR))

UINTERFACE(NotBlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class CHOOSER_API UObjectChooser : public UInterface
{
	GENERATED_BODY()
};

class CHOOSER_API IObjectChooser
{
	GENERATED_BODY()
public:
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct) const { }
};

#if CHOOSER_DEBUGGING_ENABLED
struct CHOOSER_API FChooserDebuggingInfo
{
	const UObject* CurrentChooser = nullptr;
	bool bCurrentDebugTarget = false;
};
#endif


USTRUCT()
struct CHOOSER_API FChooserEvaluationInputObject
{
	GENERATED_BODY()
	FChooserEvaluationInputObject() {}
	FChooserEvaluationInputObject (UObject* InObject) : Object(InObject) {}
	TObjectPtr<UObject> Object;
};

USTRUCT(BlueprintType)
struct CHOOSER_API FChooserEvaluationContext
{
	FChooserEvaluationContext() {}
	FChooserEvaluationContext(UObject* ContextObject)
	{
		AddObjectParam(ContextObject);
	}

	// Add a UObject Parameter to the context
	void AddObjectParam(UObject* Param)
	{
		ObjectParams.Add(Param);
		AddStructParam(ObjectParams.Last());
	}

	// Add a struct Parameter to the Context
	// the struct will be referred to by reference, and so must have a lifetime that is longer than this context
	template <class T>
	void AddStructParam(T& Param)
	{
		Params.Add(FStructView::Make(Param));
	}

	#if CHOOSER_DEBUGGING_ENABLED
    	FChooserDebuggingInfo DebuggingInfo;
    #endif
	
	GENERATED_BODY()
	TArray<FStructView, TInlineAllocator<4>> Params;

	// storage for Object Params, call AddObjectParam to allocate one FChooserEvaluationInputObject in this array and then add a StructView of it to the Params array
	TArray<FChooserEvaluationInputObject, TFixedAllocator<4>> ObjectParams;
};

USTRUCT()
struct CHOOSER_API FObjectChooserBase
{
	GENERATED_BODY()

public:
	virtual ~FObjectChooserBase() {}

	enum class EIteratorStatus { Continue, ContinueWithOutputs, Stop };

	DECLARE_DELEGATE_RetVal_OneParam( EIteratorStatus, FObjectChooserIteratorCallback, UObject*);

	virtual void Compile(class IHasContextClass* HasContext, bool bForce) {};

	virtual UObject* ChooseObject(FChooserEvaluationContext& Context) const { return nullptr; };
	virtual EIteratorStatus ChooseMulti(FChooserEvaluationContext& ContextData, FObjectChooserIteratorCallback Callback) const
	{
		// fallback implementation just calls the single version
		if (UObject* Result = ChooseObject(ContextData))
		{
			return Callback.Execute(Result);
		}
		return EIteratorStatus::Continue;
	}

	virtual void GetDebugName(FString& OutDebugName) const {};
};