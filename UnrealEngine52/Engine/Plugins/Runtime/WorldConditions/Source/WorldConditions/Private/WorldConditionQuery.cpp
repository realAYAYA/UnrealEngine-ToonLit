// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldConditionQuery.h"
#include "WorldConditionContext.h"
#include "UObject/UObjectThreadContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldConditionQuery)

//
// FWorldConditionResultInvalidationHandle
//

void FWorldConditionResultInvalidationHandle::InvalidateResult() const
{
	if (CachedResult && Item)
	{
		*CachedResult = EWorldConditionResultValue::Invalid;
		Item->CachedResult = EWorldConditionResultValue::Invalid;
	}
}


//
// FWorldConditionQueryState
//

FWorldConditionQueryState::~FWorldConditionQueryState()
{
	if (IsInitialized() && SharedDefinition)
	{
		UE_LOG(LogWorldCondition, Error, TEXT("World Condition: State %p is still active on destructor, calling Deactivate() without context data, might leak memory."), this);
		const FWorldConditionContextData ContextData(*SharedDefinition->SchemaClass.GetDefaultObject());
		const FWorldConditionContext Context(*this, ContextData);
		Context.Deactivate();
	}

	if (Memory)
	{
		UE_LOG(LogWorldCondition, Error, TEXT("World Condition: Expected state %p to have been freed, might leak memory."), Memory);
		FMemory::Free(Memory);
		Memory = nullptr;
	}
}
	
void FWorldConditionQueryState::Initialize(const UObject& InOwner, const FWorldConditionQueryDefinition& QueryDefinition)
{
	if (IsInitialized())
	{
		Free();
	}

	Owner = &InOwner;
	bHasPerConditionState = false;

	if (!QueryDefinition.IsValid())
	{
		// Empty condition
		SharedDefinition = nullptr;
		NumConditions = 0;
		bIsInitialized = true;
		return;
	}

	check(QueryDefinition.SharedDefinition);
	
	SharedDefinition = QueryDefinition.SharedDefinition;
	SharedDefinition->ActiveStates++;
	NumConditions = IntCastChecked<uint8>(SharedDefinition->Conditions.Num());
	
	int32 MinAlignment = 8;
	int32 Offset = 0;

	// Reserve space for cached result
	Offset += sizeof(EWorldConditionResultValue);
	
	// Reserve space for condition items.
	Offset = Align(Offset, alignof(FWorldConditionItem));
	check(Offset == ItemsOffset);
	Offset += sizeof(FWorldConditionItem) * static_cast<int32>(NumConditions); 

	// Reserve space for all runtime data.
	for (int32 Index = 0; Index < static_cast<int32>(NumConditions); Index++)
	{
		FWorldConditionBase& Condition = SharedDefinition->Conditions[Index].GetMutable<FWorldConditionBase>();
		if (const UStruct* StateStruct = Condition.GetRuntimeStateType())
		{
			int32 StructMinAlignment = 0;
			int32 StructSize = 0;

			if (const UScriptStruct* ScriptStruct = Cast<const UScriptStruct>(StateStruct))
			{
				StructMinAlignment = ScriptStruct->GetMinAlignment();
				StructSize = ScriptStruct->GetStructureSize();
				Condition.bIsStateObject = false;
			}
			else if (const UClass* Class = Cast<const UClass>(StateStruct))
			{
				StructMinAlignment = FWorldConditionStateObject::StaticStruct()->GetMinAlignment();
				StructSize = FWorldConditionStateObject::StaticStruct()->GetStructureSize();
				Condition.bIsStateObject = true;
			}
			
			check(StructMinAlignment > 0 && StructSize > 0);

			Offset = Align(Offset, StructMinAlignment);
			Condition.StateDataOffset = IntCastChecked<uint16>(Offset);

			Offset += StructSize;
			MinAlignment = FMath::Max(MinAlignment, StructMinAlignment);

			bHasPerConditionState = true;
		}
		else
		{
			Condition.StateDataOffset = 0;
		}
	}

	const int32 TotalSize = Offset;
	Memory = static_cast<uint8*>(FMemory::Malloc(TotalSize, MinAlignment));

	// Init cached result
	EWorldConditionResultValue& CachedResult = *reinterpret_cast<EWorldConditionResultValue*>(Memory + CachedResultOffset);
	CachedResult = EWorldConditionResultValue::Invalid;
	
	// Initialize items
	for (int32 Index = 0; Index < static_cast<int32>(NumConditions); Index++)
	{
		new (Memory + ItemsOffset + sizeof(FWorldConditionItem) * Index) FWorldConditionItem();
	}

	// Initialize state
	for (int32 Index = 0; Index < static_cast<int32>(NumConditions); Index++)
	{
		FWorldConditionBase& Condition = SharedDefinition->Conditions[Index].GetMutable<FWorldConditionBase>();
		if (Condition.StateDataOffset == 0)
		{
			continue;
		}
		uint8* StateMemory = Memory + Condition.StateDataOffset;
		if (Condition.bIsStateObject)
		{
			new (StateMemory) FWorldConditionStateObject();
			FWorldConditionStateObject& StateObject = *reinterpret_cast<FWorldConditionStateObject*>(StateMemory);
			const UClass* StateClass = Cast<UClass>(Condition.GetRuntimeStateType());
			StateObject.Object = NewObject<UObject>(const_cast<UObject*>(Owner.Get()), StateClass);  
		}
		else
		{
			const UScriptStruct* StateScriptStruct = Cast<UScriptStruct>(Condition.GetRuntimeStateType());
			StateScriptStruct->InitializeStruct(StateMemory);
		}
	}

	bIsInitialized = true;
}

void FWorldConditionQueryState::Free()
{
	if (Memory == nullptr)
	{
		NumConditions = 0;
		bHasPerConditionState = false;
		SharedDefinition = nullptr;
		bIsInitialized = false;
		return;
	}

	check(SharedDefinition);
	SharedDefinition->ActiveStates--;

	// Items don't need destructing.
	
	// Destroy state
	for (int32 Index = 0; Index < static_cast<int32>(NumConditions); Index++)
	{
		FWorldConditionBase& Condition = SharedDefinition->Conditions[Index].GetMutable<FWorldConditionBase>();
		if (Condition.StateDataOffset == 0)
		{
			continue;
		}
		uint8* StateMemory = Memory + Condition.StateDataOffset;
		if (Condition.bIsStateObject)
		{
			FWorldConditionStateObject& StateObject = *reinterpret_cast<FWorldConditionStateObject*>(Memory + Condition.StateDataOffset);
			StateObject.~FWorldConditionStateObject();
		}
		else
		{
			const UScriptStruct* StateScriptStruct = Cast<UScriptStruct>(Condition.GetRuntimeStateType());
			StateScriptStruct->DestroyStruct(StateMemory);
		}
	}

	FMemory::Free(Memory);
	
	Memory = nullptr;
	NumConditions = 0;
	bHasPerConditionState = false;
	SharedDefinition = nullptr;
	bIsInitialized = false;
}

void FWorldConditionQueryState::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Owner);
	Collector.AddReferencedObject(SharedDefinition);

	if (Memory == nullptr)
	{
		return;
	}

	check(SharedDefinition);
	check(NumConditions == SharedDefinition->Conditions.Num());

	for (int32 Index = 0, Num = SharedDefinition->Conditions.Num(); Index < Num; Index++)
	{
		const FWorldConditionBase& Condition = SharedDefinition->Conditions[Index].GetMutable<FWorldConditionBase>();
		if (Condition.StateDataOffset == 0)
		{
			continue;
		}
		
		uint8* StateMemory = Memory + Condition.StateDataOffset;
		if (Condition.bIsStateObject)
		{
			FWorldConditionStateObject& StateObject = *reinterpret_cast<FWorldConditionStateObject*>(StateMemory);
			Collector.AddReferencedObject(StateObject.Object);
		}
		else
		{
			if (const UScriptStruct* StateScriptStruct = Cast<UScriptStruct>(Condition.GetRuntimeStateType()))
			{
				Collector.AddReferencedObject(StateScriptStruct, Owner);
				Collector.AddPropertyReferencesWithStructARO(StateScriptStruct, StateMemory, Owner);
			}
		}
	}
}

FWorldConditionResultInvalidationHandle FWorldConditionQueryState::GetInvalidationHandle(const FWorldConditionBase& Condition) const
{
	check(bIsInitialized);
	check(Memory && Condition.ConditionIndex < NumConditions);

	EWorldConditionResultValue* CachedResult = reinterpret_cast<EWorldConditionResultValue*>(Memory + CachedResultOffset);
	FWorldConditionItem* Item = reinterpret_cast<FWorldConditionItem*>(Memory + ItemsOffset + Condition.ConditionIndex * sizeof(FWorldConditionItem));

	return FWorldConditionResultInvalidationHandle(CachedResult, Item);
}


//
// UWorldConditionQuerySharedDefinition
//

void UWorldConditionQuerySharedDefinition::PostLoad()
{
	Super::PostLoad();
	
	// Initialize conditions. This is done always on load so that any changes to the schema take affect.
	if (SchemaClass)
	{
		const UWorldConditionSchema* Schema = SchemaClass.GetDefaultObject();
		bool bResult = true;

		for (int32 Index = 0; Index < Conditions.Num(); Index++)
		{
			FWorldConditionBase& Condition = Conditions[Index].GetMutable<FWorldConditionBase>();
			bResult &= Condition.Initialize(*Schema);
		}

		if (!bResult)
		{
			UE_LOG(LogWorldCondition, Error, TEXT("World Condition: Failed to initialize query %s for %s."),
				*GetNameSafe(this), *GetFullNameSafe(GetOuter()));
		}
	}
	else
	{
		UE_LOG(LogWorldCondition, Error, TEXT("World Condition: shared definition %s for %s has empty schema, and %d conditions."),
			*GetNameSafe(this), *GetFullNameSafe(GetOuter()), Conditions.Num());
	}
}

//
// FWorldConditionQueryDefinition
//

void FWorldConditionQueryDefinition::SetSchemaClass(const TSubclassOf<UWorldConditionSchema> InSchema)
{
	SchemaClass = InSchema;
}

bool FWorldConditionQueryDefinition::IsValid() const
{
	return SharedDefinition != nullptr;
}

bool FWorldConditionQueryDefinition::Initialize(UObject& Outer)
{
	bool bResult = true;

#if WITH_EDITORONLY_DATA
	UWorldConditionQuerySharedDefinition* OldSharedDefinition = SharedDefinition; 
	SharedDefinition = nullptr;

	if (SchemaClass == nullptr)
	{
		UE_LOG(LogWorldCondition, Warning, TEXT("World Condition: Failed to initialize query for %s due to missing schema."), *GetFullNameSafe(&Outer));
		return false;
	}

	const UWorldConditionSchema* Schema = SchemaClass.GetDefaultObject();
	
	// Append only valid condition.
	TArray<FConstStructView> ValidConditions;
	ValidConditions.Reserve(EditableConditions.Num());
	for (FWorldConditionEditable& EditableCondition : EditableConditions)
	{
		if (EditableCondition.Condition.IsValid())
		{
			if (Schema->IsStructAllowed(EditableCondition.Condition.GetScriptStruct()))
			{
				FWorldConditionBase& Condition = EditableCondition.Condition.GetMutable<FWorldConditionBase>();
				// Store expression depth temporarily into NextExpressionDepth, it will be update below.
				Condition.NextExpressionDepth = EditableCondition.ExpressionDepth;
				Condition.Operator = EditableCondition.Operator;
				Condition.bInvert = EditableCondition.bInvert;

				ValidConditions.Add(EditableCondition.Condition);
			}
			else
			{
				UE_LOG(LogWorldCondition, Warning, TEXT("World Condition: Query for %s contains condition of type %s that is not allowed by schema %s."),
					*GetFullNameSafe(&Outer), *GetNameSafe(EditableCondition.Condition.GetScriptStruct()), *GetNameSafe(Schema));
			}
		}
	}
	
	if (ValidConditions.IsEmpty())
	{
		// Empty query, do not create definition.
		return true;
	}

	// We create a new shared definition if one does not exists, or the old one is already in use.
	// That allows the allocated states to uninitialize properly.
	if (OldSharedDefinition != nullptr && OldSharedDefinition->ActiveStates == 0)
	{
		SharedDefinition = OldSharedDefinition;
		SharedDefinition->SchemaClass = nullptr;
		SharedDefinition->Conditions.Reset();
	}
	else
	{
		SharedDefinition = NewObject<UWorldConditionQuerySharedDefinition>(&Outer);
	}
	
	SharedDefinition->SchemaClass = SchemaClass;
	SharedDefinition->Conditions.Append(ValidConditions);

	if (SharedDefinition->Conditions.Num() > 0)
	{
		FWorldConditionBase& Condition = SharedDefinition->Conditions[0].GetMutable<FWorldConditionBase>();
		Condition.Operator = EWorldConditionOperator::Copy;
	}

	for (int32 Index = 0; Index < SharedDefinition->Conditions.Num(); Index++)
	{
		uint8 NextExpressionDepth = 0;
		if ((Index + 1) < SharedDefinition->Conditions.Num())
		{
			const FWorldConditionBase& NextCondition = SharedDefinition->Conditions[Index + 1].GetMutable<FWorldConditionBase>();
			NextExpressionDepth = NextCondition.NextExpressionDepth;
		}
		
		FWorldConditionBase& Condition = SharedDefinition->Conditions[Index].GetMutable<FWorldConditionBase>();
		Condition.NextExpressionDepth = NextExpressionDepth;

		Condition.ConditionIndex = Index;
	}

	for (int32 Index = 0; Index < SharedDefinition->Conditions.Num(); Index++)
	{
		FWorldConditionBase& Condition = SharedDefinition->Conditions[Index].GetMutable<FWorldConditionBase>();
		bResult &= Condition.Initialize(*Schema);
	}
#endif
	
	return bResult;
}

void FWorldConditionQueryDefinition::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
#if WITH_EDITOR
		// If not initialized yet, but has data, initialize on load in editor.
		if (!SharedDefinition && EditableConditions.Num() > 0)
		{
			const FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
			if (LoadContext && LoadContext->SerializedObject)
			{
				Initialize(*LoadContext->SerializedObject);
			}
		}
#endif
	}
}


//
// FWorldConditionQuery
//

#if WITH_EDITORONLY_DATA
bool FWorldConditionQuery::DebugInitialize(UObject& Outer, const TSubclassOf<UWorldConditionSchema> InSchemaClass, const TConstArrayView<FWorldConditionEditable> InConditions)
{
	if (IsActive())
	{
		return false;
	}

	QueryDefinition.SchemaClass = InSchemaClass;
	QueryDefinition.EditableConditions = InConditions;
	return QueryDefinition.Initialize(Outer);
}
#endif // WITH_EDITORONLY_DATA

bool FWorldConditionQuery::Activate(const UObject& InOwner, const FWorldConditionContextData& ContextData) const
{
	QueryState.Initialize(InOwner, QueryDefinition);
	check(!QueryDefinition.SharedDefinition
			|| (QueryDefinition.SharedDefinition && QueryState.GetNumConditions() == QueryDefinition.SharedDefinition->Conditions.Num()));

	const FWorldConditionContext Context(QueryState, ContextData);
	return Context.Activate();
}

bool FWorldConditionQuery::IsTrue(const FWorldConditionContextData& ContextData) const
{
	const FWorldConditionContext Context(QueryState, ContextData);
	return Context.IsTrue();
}

void FWorldConditionQuery::Deactivate(const FWorldConditionContextData& ContextData) const
{
	const FWorldConditionContext Context(QueryState, ContextData);
	return Context.Deactivate();
}

bool FWorldConditionQuery::IsActive() const
{
	return QueryState.IsInitialized();
}
