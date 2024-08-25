// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/OverriddenPropertySet.h"

#include "UObject/OverridableManager.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"
#include "UObject/PropertyOptional.h"
#include "Misc/ScopeExit.h"
#include "Serialization/StructuredArchiveNameHelpers.h"

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 */

DEFINE_LOG_CATEGORY(LogOverridableObject);

//----------------------------------------------------------------------//
// FOverridableSerializationLogic
//----------------------------------------------------------------------//
thread_local bool FOverridableSerializationLogic::bUseOverridableSerialization = false;
thread_local FOverriddenPropertySet* FOverridableSerializationLogic::OverriddenProperties = nullptr;

EOverriddenPropertyOperation FOverridableSerializationLogic::GetOverriddenPropertyOperation(const FArchive& Ar, FProperty* Property /*= nullptr*/, uint8* DataPtr /*= nullptr*/, uint8* DefaultValue)
{
	checkf(bUseOverridableSerialization, TEXT("Nobody should use this method if it is not setup to use overridable serialization"));

	const FArchiveSerializedPropertyChain* CurrentPropertyChain = Ar.GetSerializedPropertyChain();
	const EOverriddenPropertyOperation OverriddenOperation = OverriddenProperties ? OverriddenProperties->GetOverriddenPropertyOperation(CurrentPropertyChain, Property) : EOverriddenPropertyOperation::None;
	if (OverriddenOperation != EOverriddenPropertyOperation::None)
	{
		return OverriddenOperation;
	}

	// It does not mean that if we have no record of an overriden operation that a subobject might have one, need to traverse all possible subobjects. 
	if (const FProperty* CurrentProperty = Property ? Property : (CurrentPropertyChain ? CurrentPropertyChain->GetPropertyFromStack(0) : nullptr) )
	{
		if (CurrentProperty->HasAnyPropertyFlags(CPF_ExperimentalAlwaysOverriden))
		{
			return EOverriddenPropertyOperation::Replace;
		}

		// In the case of a CDO owning default value, we might need to serialize it to keep its value.
		if (OverriddenProperties && OverriddenProperties->IsCDOOwningProperty(*CurrentProperty))
		{
			// Only need serialize this value if it is different from the default property value
			if (!CurrentProperty->Identical(DataPtr, DefaultValue, Ar.GetPortFlags()))
			{
				return 	EOverriddenPropertyOperation::Replace;
			}
		}
	}
	
	return EOverriddenPropertyOperation::None;
}

//----------------------------------------------------------------------//
// FOverridableSerializationScope
//----------------------------------------------------------------------//
FEnableOverridableSerializationScope::FEnableOverridableSerializationScope(bool bEnableOverridableSerialization, FOverriddenPropertySet* OverriddenProperties)
{
	if (bEnableOverridableSerialization)
	{
		if (FOverridableSerializationLogic::IsEnabled())
		{
			bWasOverridableSerializationEnabled = true;
			SavedOverriddenProperties = FOverridableSerializationLogic::GetOverriddenProperties();
			FOverridableSerializationLogic::Disable();
		}
		FOverridableSerializationLogic::Enable(OverriddenProperties);
		bOverridableSerializationEnabled = true;
	}
}

FEnableOverridableSerializationScope::~FEnableOverridableSerializationScope()
{
	if (bOverridableSerializationEnabled)
	{
		FOverridableSerializationLogic::Disable();
		if (bWasOverridableSerializationEnabled)
		{
			FOverridableSerializationLogic::Enable(SavedOverriddenProperties);
		}
	}
}

//----------------------------------------------------------------------//
// FOverriddenPropertyNodeID
//----------------------------------------------------------------------//
void FOverriddenPropertyNodeID::HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& Map)
{
	if (!Object)
	{
		return;
	}

	if (UObject*const* ReplacedObject = Map.Find(Object))
	{
		Object = *ReplacedObject;
	}
}


//----------------------------------------------------------------------//
// FOverriddenPropertySet
//----------------------------------------------------------------------//
FOverriddenPropertyNode& FOverriddenPropertySet::FindOrAddNode(FOverriddenPropertyNode& ParentNode, FOverriddenPropertyNodeID NodeID)
{
	FOverriddenPropertyNodeID& SubNodeID = ParentNode.SubPropertyNodeKeys.FindOrAdd(NodeID, FOverriddenPropertyNodeID());
	if (SubNodeID.IsValid())
	{
		FOverriddenPropertyNode* FoundNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(SubNodeID), SubNodeID);
		checkf(FoundNode, TEXT("Expecting a node"));
		return *FoundNode;
	}

	// We can safely assume that the parent node is at least modified from now on
	if (ParentNode.Operation == EOverriddenPropertyOperation::None)
	{
		ParentNode.Operation = EOverriddenPropertyOperation::Modified;
	}

	// Not found add the node
	FStringBuilderBase SubPropertyKeyBuilder;
	SubPropertyKeyBuilder = *ParentNode.NodeID.ToString();
	SubPropertyKeyBuilder.Append(TEXT("."));
	SubPropertyKeyBuilder.Append(*NodeID.ToString());
	SubNodeID.Path = FName(SubPropertyKeyBuilder.ToString());
	SubNodeID.Object = NodeID.Object;
	const FSetElementId NewID = OverriddenPropertyNodes.Emplace(SubNodeID);
	return OverriddenPropertyNodes.Get(NewID);
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(const FOverriddenPropertyNode& ParentPropertyNode, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, bool* bOutInheritedOperation, const void* Data) const
{
	FOverridableManager& OverridableManager = FOverridableManager::Get();

	const void* SubValuePtr = Data;
	const FEditPropertyChain::TDoubleLinkedListNode* PropertyIterator = PropertyNode;
	const FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	int32 ArrayIndex = INDEX_NONE;
	while (PropertyIterator && (!OverriddenPropertyNode || OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace))
	{
		ArrayIndex = INDEX_NONE;

		const FProperty* CurrentProperty = PropertyIterator->GetValue();
		SubValuePtr = CurrentProperty->ContainerPtrToValuePtr<void>(SubValuePtr, 0); //@todo support static arrays

		const FOverriddenPropertyNode* CurrentOverriddenPropertyNode = nullptr;
		if (OverriddenPropertyNode)
		{
			const FName CurrentPropID = CurrentProperty->GetFName();
			if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentPropID))
			{
				CurrentOverriddenPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(*CurrentPropKey), *CurrentPropKey);
				checkf(CurrentOverriddenPropertyNode, TEXT("Expecting a node"));
			}
		}


		// Special handling for for instanced subobjects 
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(CurrentProperty))
		{
			if (PropertyIterator->GetNextNode())
			{
				// Forward any sub queries to the subobject
				if (UObject* SubObject = ObjectProperty->GetObjectPropertyValue(SubValuePtr))
				{
					// This should not be needed in the property grid, as it should already been called on the subobject.
					return OverridableManager.GetOverriddenPropertyOperation(*SubObject, PropertyEvent, PropertyIterator->GetNextNode(), bOutInheritedOperation);
				}
			}
		}
		// Special handling for array of instanced subobjects 
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyEvent.GetArrayIndex(CurrentProperty->GetName());

			// Only special case is instanced subobjects, otherwise we fallback to full array override
			checkf(ArrayProperty->Inner, TEXT("Expecting an inner type for Arrays"));
			if (const FObjectProperty* InnerObjectProperty = ArrayProperty->Inner->HasAnyPropertyFlags(CPF_PersistentInstance) ? CastField<FObjectProperty>(ArrayProperty->Inner) : nullptr)
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, SubValuePtr);
				if(ArrayHelper.IsValidIndex(ArrayIndex))
				{
					if (UObject* SubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(ArrayIndex)))
					{
						if (PropertyIterator->GetNextNode())
						{
							// Forward any sub queries to the subobject
							return OverridableManager.GetOverriddenPropertyOperation(*SubObject, PropertyEvent, PropertyIterator->GetNextNode(), bOutInheritedOperation);
						}
						else if(CurrentOverriddenPropertyNode)
						{
							// Caller wants to know about any override state on the reference of the subobject itself
							const FOverriddenPropertyNodeID  SubObjectID(*SubObject);
							if (const FOverriddenPropertyNodeID* CurrentPropKey = CurrentOverriddenPropertyNode->SubPropertyNodeKeys.Find(SubObjectID))
							{
								const FOverriddenPropertyNode* SubObjectOverriddenPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(*CurrentPropKey), *CurrentPropKey);
								checkf(SubObjectOverriddenPropertyNode, TEXT("Expecting a node"));
								if (bOutInheritedOperation)
								{
									*bOutInheritedOperation = false;
								}
								return SubObjectOverriddenPropertyNode->Operation;
							}
						}
					}
				}
			}
		}
		// Special handling for maps and values of instance subobjects
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyEvent.GetArrayIndex(CurrentProperty->GetName());

			checkf(MapProperty->ValueProp, TEXT("Expecting a value type for Maps"));
			FScriptMapHelper MapHelper(MapProperty, SubValuePtr);

			const int32 InternalMapIndex = ArrayIndex != INDEX_NONE ? MapHelper.FindInternalIndex(ArrayIndex) : INDEX_NONE;
			if(MapHelper.IsValidIndex(InternalMapIndex))
			{
				if (PropertyIterator->GetNextNode())
				{
					// Forward any sub queries to the subobject
					if (const FObjectProperty* ValueInstancedObjectProperty = MapProperty->ValueProp->HasAnyPropertyFlags(CPF_PersistentInstance) ? CastField<FObjectProperty>(MapProperty->ValueProp) : nullptr)
					{
						if (UObject* ValueSubObject = ValueInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetValuePtr(InternalMapIndex)))
						{
							return OverridableManager.GetOverriddenPropertyOperation(*ValueSubObject, PropertyEvent, PropertyIterator->GetNextNode(), bOutInheritedOperation);
						}
					}
				}
				else if(CurrentOverriddenPropertyNode)
				{
					// Caller wants to know about any override state on the reference of the map pair itself
					checkf(MapProperty->KeyProp, TEXT("Expecting a key type for Maps"));
					FObjectProperty* KeyObjectProperty = CastField<FObjectProperty>(MapProperty->KeyProp);

					FOverriddenPropertyNodeID OverriddenKeyID;
					if (const UObject* OverriddenKeyObject = KeyObjectProperty ? KeyObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(InternalMapIndex)) : nullptr)
					{
						OverriddenKeyID = FOverriddenPropertyNodeID(*OverriddenKeyObject);
					}
					else
					{
						FString OverriddenKey;
						MapProperty->KeyProp->ExportTextItem_Direct(OverriddenKey, MapHelper.GetKeyPtr(InternalMapIndex), nullptr, nullptr, PPF_None);
						OverriddenKeyID = FName(OverriddenKey);
					}

					if (const FOverriddenPropertyNodeID* CurrentPropKey = CurrentOverriddenPropertyNode->SubPropertyNodeKeys.Find(OverriddenKeyID))
					{
						const FOverriddenPropertyNode* SubObjectOverriddenPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(*CurrentPropKey), *CurrentPropKey);
						checkf(SubObjectOverriddenPropertyNode, TEXT("Expecting a node"));
						if (bOutInheritedOperation)
						{
							*bOutInheritedOperation = false;
						}
						return SubObjectOverriddenPropertyNode->Operation;
					}
				}
			}
		}

		OverriddenPropertyNode = CurrentOverriddenPropertyNode;
		PropertyIterator = PropertyIterator->GetNextNode();
	}

	if (bOutInheritedOperation)
	{
		*bOutInheritedOperation = PropertyIterator != nullptr || ArrayIndex != INDEX_NONE;
	}
	return OverriddenPropertyNode ? OverriddenPropertyNode->Operation : EOverriddenPropertyOperation::None;
}

bool FOverriddenPropertySet::ClearOverriddenProperty(FOverriddenPropertyNode& ParentPropertyNode, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data)
{
	FOverridableManager& OverridableManager = FOverridableManager::Get();

	bool bClearedOverrides = false;
	const void* SubValuePtr = Data;
	const FEditPropertyChain::TDoubleLinkedListNode* PropertyIterator = PropertyNode;
	FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	int32 ArrayIndex = INDEX_NONE;
	while (PropertyIterator && OverriddenPropertyNode && OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace)
	{
		ArrayIndex = INDEX_NONE;

		const FProperty* CurrentProperty = PropertyIterator->GetValue();
		SubValuePtr = CurrentProperty->ContainerPtrToValuePtr<void>(SubValuePtr, 0); //@todo support static arrays

		FOverriddenPropertyNode* CurrentOverriddenPropertyNode = nullptr;
		if (OverriddenPropertyNode)
		{
			const FName CurrentPropID = CurrentProperty->GetFName();
			if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentPropID))
			{
				CurrentOverriddenPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(*CurrentPropKey), *CurrentPropKey);
				checkf(CurrentOverriddenPropertyNode, TEXT("Expecting a node"));
			}
		}

		// Special handling for for instanced subobjects 
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(CurrentProperty))
		{
			if (UObject* SubObject = ObjectProperty->GetObjectPropertyValue(SubValuePtr))
			{
				if (PropertyIterator->GetNextNode())
				{
					return OverridableManager.ClearOverriddenProperty(*SubObject, PropertyEvent, PropertyIterator->GetNextNode());
				}
				else
				{
					OverridableManager.ClearOverrides(*SubObject);
					bClearedOverrides = true;
				}
			}
		}
		// Special handling for array of instanced subobjects 
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyEvent.GetArrayIndex(CurrentProperty->GetName());

			// Only special case is instanced subobjects, otherwise we fallback to full array override
			if (FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner))
			{
				if (InnerObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance))
				{
					FScriptArrayHelper ArrayHelper(ArrayProperty, SubValuePtr);

					if(ArrayIndex == INDEX_NONE)
					{
						// This is a case of the entire array needs to be cleared
						// Need to loop through every sub object and clear them
						for (int i = 0; i < ArrayHelper.Num(); ++i)
						{
							if (UObject* SubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(i)))
							{
								OverridableManager.ClearInstancedSubObjectOverrides(*Owner, *SubObject);
							}
						}
						bClearedOverrides = true;
					}
					else if(ArrayHelper.IsValidIndex(ArrayIndex))
					{
						if (UObject* SubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(ArrayIndex)))
						{
							if (PropertyIterator->GetNextNode())
							{
								return OverridableManager.ClearOverriddenProperty(*SubObject, PropertyEvent, PropertyIterator->GetNextNode());
							}
							else if(CurrentOverriddenPropertyNode)
							{
								const FOverriddenPropertyNodeID  SubObjectID(*SubObject);
								FOverriddenPropertyNodeID CurrentPropKey;
								if (CurrentOverriddenPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(SubObjectID, CurrentPropKey))
								{
									verifyf(OverriddenPropertyNodes.RemoveByHash(GetTypeHash(CurrentPropKey), CurrentPropKey), TEXT("Expecting a node to be removed"));
									OverridableManager.ClearInstancedSubObjectOverrides(*Owner, *SubObject);
									return true;
								}
							}
						}
					}
				}
			}
		}
		// Special handling for maps and values of instance subobjects 
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyEvent.GetArrayIndex(CurrentProperty->GetName());

			FScriptMapHelper MapHelper(MapProperty, SubValuePtr);

			const int32 InternalMapIndex = ArrayIndex != INDEX_NONE ? MapHelper.FindInternalIndex(ArrayIndex) : INDEX_NONE;
			const FObjectProperty* ValueInstancedObjectProperty = MapProperty->ValueProp->HasAnyPropertyFlags(CPF_PersistentInstance) ? CastField<FObjectProperty>(MapProperty->ValueProp) : nullptr;

			// If there is a next node, it is probably because the map value is holding a instanced subobject and the user is changing value on it.
			// So forward the call to the instanced subobject
			if (PropertyIterator->GetNextNode())
			{
				if(MapHelper.IsValidIndex(InternalMapIndex))
				{
					checkf(MapProperty->ValueProp, TEXT("Expecting a value type for Maps"));
					if (UObject* ValueSubObject = ValueInstancedObjectProperty ? ValueInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetValuePtr(InternalMapIndex)) : nullptr)
					{
						return OverridableManager.ClearOverriddenProperty(*ValueSubObject, PropertyEvent, PropertyIterator->GetNextNode());
					}
				}
			}
			else if(InternalMapIndex == INDEX_NONE)
			{
				// Users want to clear all of the overrides on the array, but in the case of instanced subobject, we need to clear the overrides on them as well.
				if (ValueInstancedObjectProperty)
				{
					// This is a case of the entire array needs to be cleared
					// Need to loop through every sub object and clear them
					for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
					{
						if (UObject* ValueSubObject = ValueInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetValuePtr(It.GetInternalIndex())))
						{
							OverridableManager.ClearInstancedSubObjectOverrides(*Owner, *ValueSubObject);
						}
					}
				}
				bClearedOverrides = true;
			}
			else if (MapHelper.IsValidIndex(InternalMapIndex) && CurrentOverriddenPropertyNode)
			{
				checkf(MapProperty->KeyProp, TEXT("Expecting a key type for Maps"));
				FObjectProperty* KeyObjectProperty = CastField<FObjectProperty>(MapProperty->KeyProp);

				// Calculate the node from the key
				FOverriddenPropertyNodeID OverriddenKeyID;
				if (const UObject* OverriddenKeyObject = KeyObjectProperty ? KeyObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(InternalMapIndex)) : nullptr)
				{
					OverriddenKeyID = FOverriddenPropertyNodeID(*OverriddenKeyObject);
				}
				else
				{
					FString OverriddenKey;
					MapProperty->KeyProp->ExportTextItem_Direct(OverriddenKey, MapHelper.GetKeyPtr(InternalMapIndex), nullptr, nullptr, PPF_None);
					OverriddenKeyID = FName(OverriddenKey);
				}

				FOverriddenPropertyNodeID CurrentPropKey;
				if (CurrentOverriddenPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(OverriddenKeyID, CurrentPropKey))
				{
					verifyf(OverriddenPropertyNodes.RemoveByHash(GetTypeHash(CurrentPropKey), CurrentPropKey), TEXT("Expecting a node to be removed"));

					if (UObject* ValueSubObject = ValueInstancedObjectProperty ? ValueInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetValuePtr(InternalMapIndex)) : nullptr)
					{
						// In the case of a instanced subobject, clear all the overrides on the subobject as well
						OverridableManager.ClearInstancedSubObjectOverrides(*Owner, *ValueSubObject);
					}

					return true;
				}
			}
		}

		OverriddenPropertyNode = CurrentOverriddenPropertyNode;
		PropertyIterator = PropertyIterator->GetNextNode();
	}

	if (PropertyIterator != nullptr || OverriddenPropertyNode == nullptr)
	{
		return bClearedOverrides;
	}

	if (ArrayIndex != INDEX_NONE)
	{
		return false;
	}

	RemoveOverriddenSubProperties(*OverriddenPropertyNode);
	OverriddenPropertyNode->Operation = EOverriddenPropertyOperation::None;
	return true;
}

void FOverriddenPropertySet::NotifyPropertyChange(FOverriddenPropertyNode* ParentPropertyNode, const EPropertyNotificationType Notification, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data)
{
	checkf(IsValid(Owner), TEXT("Expecting a valid overridable owner"));

	FOverridableManager& OverridableManager = FOverridableManager::Get();
	if (!PropertyNode)
	{
		if (ParentPropertyNode && Notification == EPropertyNotificationType::PostEdit)
		{
			// Replacing this entire property
			ParentPropertyNode->Operation = EOverriddenPropertyOperation::Replace;

			// Sub-property overrides are not needed from now on, so clear them
			RemoveOverriddenSubProperties(*ParentPropertyNode);

			// If we are overriding the root node, need to propagate the overrides to all instanced sub object
			const FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(RootNodeID), RootNodeID);
			checkf(RootNode, TEXT("Expecting to always have a "));
			if (RootNode == ParentPropertyNode)
			{
				OverridableManager.PropagateOverrideToInstancedSubObjects(*Owner);
			}
		}
		return;
	}

	const FProperty* Property = PropertyNode->GetValue();
	checkf(Property, TEXT("Expecting a valid property"));

	const FOverriddenPropertyNodeID PropID(Property->GetFName());
	const void* SubValuePtr = Property->ContainerPtrToValuePtr<void>(Data, 0); //@todo support static arrays

	FOverriddenPropertyNode* SubPropertyNode = nullptr;
	if (ParentPropertyNode)
	{
		FOverriddenPropertyNode& SubPropertyNodeRef = FindOrAddNode(*ParentPropertyNode, PropID);
		SubPropertyNode = SubPropertyNodeRef.Operation != EOverriddenPropertyOperation::Replace ? &SubPropertyNodeRef : nullptr;
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		// Only special case is instanced subobjects, otherwise we fallback to full array override
		if (FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(ArrayProperty->Inner))
		{
			if (InnerObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, SubValuePtr);
				int32 ArrayIndex = PropertyEvent.GetArrayIndex(Property->GetName());
				if (!PropertyNode->GetNextNode())
				{
					checkf(ArrayProperty->Inner, TEXT("Expecting an inner type for Arrays"));

					static TArray<uint8> SavedPreEditSubObjects;
					FScriptArrayHelper PreEditSubObjectsArrayHelper(ArrayProperty, &SavedPreEditSubObjects);
					if (Notification == EPropertyNotificationType::PreEdit)
					{
						PreEditSubObjectsArrayHelper.EmptyAndAddValues(ArrayHelper.Num());
						for(int32 i = 0; i < ArrayHelper.Num(); i++)
						{
							InnerObjectProperty->SetObjectPropertyValue(PreEditSubObjectsArrayHelper.GetElementPtr(i), InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(i)));
						}
						return;
					}

					auto ArrayReplace = [&]
					{
						if (SubPropertyNode)
						{
							// Overriding all entry in the array
							SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
						}

						// This is a case of the entire array is overridden
						// Need to loop through every sub object and setup them up as overridden
						for (int i = 0; i < ArrayHelper.Num(); ++i)
						{
							if (UObject* SubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(i)))
							{
								if(SubPropertyNode)
								{
									const FOverriddenPropertyNodeID SubObjectID(*SubObject);
									FOverriddenPropertyNode& SubObjectNode = FindOrAddNode(*SubPropertyNode, SubObjectID);
									SubObjectNode.Operation = EOverriddenPropertyOperation::Replace;
								}

								OverridableManager.OverrideInstancedSubObject(*Owner, *SubObject);
							}
						}
					};

					auto ArrayAddImpl = [&]()
					{
						checkf(ArrayHelper.IsValidIndex(ArrayIndex), TEXT("ArrayAdd change type expected to have an valid index"));
						if (UObject* AddedSubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(ArrayIndex)))
						{
							if(SubPropertyNode)
							{
								const FOverriddenPropertyNodeID  AddedSubObjectID(*AddedSubObject);
								FOverriddenPropertyNode& AddedSubObjectNode = FindOrAddNode(*SubPropertyNode, AddedSubObjectID);
								AddedSubObjectNode.Operation = EOverriddenPropertyOperation::Add;

								// Check if this could be a readd
								UObject* RemovedSubObjectArchetype = AddedSubObject->GetArchetype();
								if(RemovedSubObjectArchetype && !RemovedSubObjectArchetype->HasAnyFlags(RF_ClassDefaultObject))
								{
									const FOverriddenPropertyNodeID RemovedSubObjectID(*RemovedSubObjectArchetype);
									SubPropertyNode->SubPropertyNodeKeys.Remove(RemovedSubObjectID);
								}
							}
						}
					};

					auto ArrayRemoveImpl = [&]()
					{
						checkf(PreEditSubObjectsArrayHelper.IsValidIndex(ArrayIndex), TEXT("ArrayRemove change type expected to have an valid index"));
						if (UObject* RemovedSubObject = InnerObjectProperty->GetObjectPropertyValue(PreEditSubObjectsArrayHelper.GetElementPtr(ArrayIndex)))
						{
							if(SubPropertyNode)
							{
								UObject* RemovedSubObjectArchetype = RemovedSubObject->GetArchetype();
								const FOverriddenPropertyNodeID RemovedSubObjectID (!RemovedSubObjectArchetype || RemovedSubObjectArchetype->HasAnyFlags(RF_ClassDefaultObject) ? *RemovedSubObject : *RemovedSubObjectArchetype);
								FOverriddenPropertyNode& RemovedSubObjectNode = FindOrAddNode(*SubPropertyNode, RemovedSubObjectID);

								if (RemovedSubObjectNode.Operation == EOverriddenPropertyOperation::Add)
								{
									// An add then a remove becomes no opt
									SubPropertyNode->SubPropertyNodeKeys.Remove(RemovedSubObjectID);
								}
								else
								{
									RemovedSubObjectNode.Operation = EOverriddenPropertyOperation::Remove;
								}
							}

							OverridableManager.ClearInstancedSubObjectOverrides(*Owner, *RemovedSubObject);
						}
					};

					// Only arrays flagged overridable logic can record deltas, for now just override entire array
					if (!ArrayProperty->HasAnyPropertyFlags(CPF_ExperimentalOverridableLogic))
					{
						if(PropertyEvent.ChangeType == EPropertyChangeType::Unspecified && ArrayIndex == INDEX_NONE)
						{
							// Overriding all entry in the array + override instanced sub obejects
							ArrayReplace();
						}
						else if (SubPropertyNode)
						{
							// Overriding all entry in the array
							SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
						}
						return;
					}

					switch(PropertyEvent.ChangeType)
					{
					case EPropertyChangeType::ValueSet:
						checkf(ArrayIndex != INDEX_NONE, TEXT("ValueSet change type should have associated indexes"));
						// Intentional fall thru
					case EPropertyChangeType::Unspecified:
						{
							if (ArrayIndex != INDEX_NONE)
							{
								// Overriding a single entry in the array
								ArrayRemoveImpl();
								ArrayAddImpl();
							}
							else
							{
								ArrayReplace();
							}
							return;
						}
					case EPropertyChangeType::ArrayAdd:
						{
							ArrayAddImpl();
							return;
						}
					case EPropertyChangeType::ArrayRemove:
						{
							ArrayRemoveImpl();
							return;
						}
					case EPropertyChangeType::ArrayClear:
						{
							checkf(ArrayIndex == INDEX_NONE, TEXT("ArrayClear change type should not have associated indexes"));

							for (int i = 0; i < PreEditSubObjectsArrayHelper.Num(); ++i)
							{
								ArrayIndex = i;
								ArrayRemoveImpl();
							}
							return;
						}
					case EPropertyChangeType::ArrayMove:
						{
							UE_LOG(LogOverridableObject, Warning, TEXT("ArrayMove change type is not going to change anything as ordering of object isn't supported yet"));
							return;
						}
					default:
						{
							UE_LOG(LogOverridableObject, Warning, TEXT("Property change type is not supported will default to full array override"));
							break;
						}
					}
				}
				else if (Notification == EPropertyNotificationType::PostEdit)
				{
					checkf(ArrayHelper.IsValidIndex(ArrayIndex), TEXT("Any sub operation is expected to have a valid index"));
					if (UObject* SubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(ArrayIndex)))
					{
						// This should not be needed in the property grid, as it should already been called on the subobject itself.
						OverridableManager.NotifyPropertyChange(Notification, *SubObject, PropertyEvent, PropertyNode->GetNextNode());
						return;
					}
				}
			}
		}
	}
	// @todo support set in the overridable serialization
	//else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	//{
	//	
	//}
	else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		// Special handling of instanced subobjects
		checkf(MapProperty->KeyProp, TEXT("Expecting a key type for Maps"));
		FObjectProperty* KeyObjectProperty = CastField<FObjectProperty>(MapProperty->KeyProp);

		// SubObjects
		checkf(!KeyObjectProperty || !MapProperty->KeyProp->HasAnyPropertyFlags(CPF_PersistentInstance) || CastField<FClassProperty>(MapProperty->KeyProp), TEXT("Keys as a instanced subobject is not supported yet"));

		checkf(MapProperty->ValueProp, TEXT("Expecting a value type for Maps"));
		FObjectProperty* ValueInstancedObjectProperty = MapProperty->ValueProp->HasAnyPropertyFlags(CPF_PersistentInstance) ? CastField<FObjectProperty>(MapProperty->ValueProp) : nullptr;

		FScriptMapHelper MapHelper(MapProperty, SubValuePtr);
		int32 LogicalMapIndex = PropertyEvent.GetArrayIndex(Property->GetName());
		int32 InternalMapIndex = LogicalMapIndex != INDEX_NONE ? MapHelper.FindInternalIndex(LogicalMapIndex) : INDEX_NONE;
		if (!PropertyNode->GetNextNode())
		{
			static const FProperty* SavedProp = nullptr;
			static uint8* SavedPreEditMap = nullptr;

			auto FreePreEditMap = []()
			{
				if (SavedPreEditMap)
				{
					checkf(SavedProp, TEXT("Expecting a matching property to the allocated memory"));
					SavedProp->DestroyValue(SavedPreEditMap);
					FMemory::Free(SavedPreEditMap);
					SavedPreEditMap = nullptr;
					SavedProp = nullptr;
				}
			};

			if (Notification == EPropertyNotificationType::PreEdit)
			{
				FreePreEditMap();

				SavedPreEditMap = (uint8*)FMemory::Malloc(MapProperty->GetSize(), MapProperty->GetMinAlignment());
				MapProperty->InitializeValue(SavedPreEditMap);
				SavedProp = MapProperty;

				FScriptMapHelper PreEditMapHelper(MapProperty, SavedPreEditMap);
				PreEditMapHelper.EmptyValues();
				for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
				{
					PreEditMapHelper.AddPair(MapHelper.GetKeyPtr(It.GetInternalIndex()), MapHelper.GetValuePtr(It.GetInternalIndex()));
				}
				return;
			}

			checkf(SavedProp == MapProperty, TEXT("Expecting the same property as the pre edit flow"));
			FScriptMapHelper PreEditMapHelper(MapProperty, SavedPreEditMap);
			// The logical should map directly to the pre edit map internal index as we skipped all of the invalid entries
			int32 InternalPreEditMapIndex = LogicalMapIndex;

			ON_SCOPE_EXIT
			{
				FreePreEditMap();
			};

			auto MapReplace = [&]()
			{
				// Overriding a all entries in the map
				if (SubPropertyNode)
				{
					SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
				}

				// This is a case of the entire array is overridden
				// Need to loop through every sub object and setup them up as overridden
				for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
				{
					if(SubPropertyNode)
					{
						FOverriddenPropertyNodeID OverriddenKeyID;
						if (const UObject* OverriddenKeyObject = KeyObjectProperty ? KeyObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(It.GetInternalIndex())) : nullptr)
						{
							OverriddenKeyID = FOverriddenPropertyNodeID(*OverriddenKeyObject);
						}
						else
						{
							FString OverriddenKey;
							MapProperty->KeyProp->ExportTextItem_Direct(OverriddenKey, MapHelper.GetKeyPtr(It.GetInternalIndex()), nullptr, nullptr, PPF_None);
							OverriddenKeyID = FName(OverriddenKey);
						}
						FOverriddenPropertyNode& OverriddenKeyNode = FindOrAddNode(*SubPropertyNode, OverriddenKeyID);
						OverriddenKeyNode.Operation = EOverriddenPropertyOperation::Replace;
					}

					// @todo support instanced object as a key in maps
					//if (UObject* KeySubObject = KeyInstancedObjectProperty ? KeyInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(*It)) : nullptr)
					//{
					//	OverridableManager.OverrideInstancedSubObject(*Owner, *KeySubObject);
					//}
					if (UObject* ValueSubObject = ValueInstancedObjectProperty ? ValueInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetValuePtr(It.GetInternalIndex())) : nullptr)
					{
						OverridableManager.OverrideInstancedSubObject(*Owner, *ValueSubObject);
					}
				}
			};

			auto MapAddImpl = [&]()
			{
				checkf(MapHelper.IsValidIndex(InternalMapIndex), TEXT("ArrayAdd change type expected to have an valid index"));

				if(SubPropertyNode)
				{
					FOverriddenPropertyNodeID AddedKeyID;
					if (const UObject* AddedKeyObject = KeyObjectProperty ? KeyObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(InternalMapIndex)) : nullptr)
					{
						AddedKeyID = FOverriddenPropertyNodeID(*AddedKeyObject);
					}
					else
					{
						FString AddedKey;
						MapProperty->KeyProp->ExportTextItem_Direct(AddedKey, MapHelper.GetKeyPtr(InternalMapIndex), nullptr, nullptr, PPF_None);
						AddedKeyID = FName(AddedKey);
					}

					FOverriddenPropertyNode& AddedKeyNode = FindOrAddNode(*SubPropertyNode, AddedKeyID);
					AddedKeyNode.Operation = EOverriddenPropertyOperation::Add;
				}
			};

			auto MapRemoveImpl = [&]()
			{
				checkf(PreEditMapHelper.IsValidIndex(InternalPreEditMapIndex), TEXT("ArrayRemove change type expected to have an valid index"));

				if(SubPropertyNode)
				{
					FOverriddenPropertyNodeID RemovedKeyID;
					if (const UObject* RemovedKeyObject = KeyObjectProperty ? KeyObjectProperty->GetObjectPropertyValue(PreEditMapHelper.GetKeyPtr(InternalPreEditMapIndex)) : nullptr)
					{
						RemovedKeyID = FOverriddenPropertyNodeID(*RemovedKeyObject);
					}
					else
					{
						FString RemovedKey;
						MapProperty->KeyProp->ExportTextItem_Direct(RemovedKey, PreEditMapHelper.GetKeyPtr(InternalPreEditMapIndex), nullptr, nullptr, PPF_None);
						RemovedKeyID = FName(RemovedKey);
					}
					FOverriddenPropertyNode& RemovedKeyNode = FindOrAddNode(*SubPropertyNode, RemovedKeyID);
					if (RemovedKeyNode.Operation == EOverriddenPropertyOperation::Add)
					{
						// @Todo support remove/add/remove
						SubPropertyNode->SubPropertyNodeKeys.Remove(RemovedKeyID);
					}
					else
					{
						RemovedKeyNode.Operation = EOverriddenPropertyOperation::Remove;
					}
				}

				// @todo support instanced object as a key in maps
				//if (UObject* RemovedKeySubObject = KeyInstancedObjectProperty ? KeyInstancedObjectProperty->GetObjectPropertyValue(PreEditMapHelper.GetKeyPtr(InternalPreEditMapIndex)) : nullptr)
				//{
				//	OverridableManager.ClearInstancedSubObjectOverrides(*Owner, *RemovedKeySubObject);
				//}
				if (UObject* RemovedValueSubObject = ValueInstancedObjectProperty ? ValueInstancedObjectProperty->GetObjectPropertyValue(PreEditMapHelper.GetValuePtr(InternalPreEditMapIndex)) : nullptr)
				{
					OverridableManager.ClearInstancedSubObjectOverrides(*Owner, *RemovedValueSubObject);
				}
			};

			// Only maps flagged overridable logic can be handled here
			if (!MapProperty->HasAnyPropertyFlags(CPF_ExperimentalOverridableLogic))
			{
				if (PropertyEvent.ChangeType == EPropertyChangeType::Unspecified && InternalMapIndex == INDEX_NONE)
				{
					// Overriding all entry in the array + override instanced sub obejects
					MapReplace();
				}
				else if(SubPropertyNode)
				{
					// Overriding all entry in the array
					SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
				}
				return;
			}

			switch (PropertyEvent.ChangeType)
			{
			case EPropertyChangeType::ValueSet:
				checkf(LogicalMapIndex != INDEX_NONE, TEXT("ValueSet change type should have associated indexes"));
				// Intentional fall thru
			case EPropertyChangeType::Unspecified:
				{
					if(LogicalMapIndex != INDEX_NONE)
					{
						// Overriding a single entry in the map
						MapRemoveImpl();
						MapAddImpl();
					}
					else
					{
						MapReplace();
					}
					return;
				}
			case EPropertyChangeType::ArrayAdd:
				{
					MapAddImpl();
					return;
				}
			case EPropertyChangeType::ArrayRemove:
				{
					MapRemoveImpl();
					return;
				}
			case EPropertyChangeType::ArrayClear:
				{
					checkf(InternalPreEditMapIndex == INDEX_NONE, TEXT("ArrayClear change type should not have associated indexes"));

					for (FScriptMapHelper::FIterator It(PreEditMapHelper); It; ++It)
					{
						InternalPreEditMapIndex = It.GetInternalIndex();
						MapRemoveImpl();
					}
					return;
				}
			case EPropertyChangeType::ArrayMove:
				{
					UE_LOG(LogOverridableObject, Warning, TEXT("ArrayMove change type is not going to change anything as ordering of object isn't supported yet"));
					return;
				}
			default:
				{
					UE_LOG(LogOverridableObject, Warning, TEXT("Property change type is not supported will default to full array override"));
					break;
				}
			}
		}
		else if (Notification == EPropertyNotificationType::PostEdit)
		{
			checkf(MapHelper.IsValidIndex(InternalMapIndex), TEXT("Any sub operation is expected to not have a valid index"));

			// @todo support instanced object as a key in maps
			//if (UObject* SubObject = KeyInstancedObjectProperty ? KeyInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetValuePtr(InternalMapIndex)) : nullptr)
			//{
			//	// This should not be needed in the property grid, as it should already been called on the subobject.
			//	OverridableManager.NotifyPropertyChange(Notification, *SubObject, PropertyEvent, PropertyNode->GetNextNode());
			//	return;
			//}

			if (UObject* SubObject = ValueInstancedObjectProperty ? ValueInstancedObjectProperty->GetObjectPropertyValue(MapHelper.GetValuePtr(InternalMapIndex)) : nullptr)
			{
				// This should not be needed in the property grid, as it should already been called on the subobject.
				OverridableManager.NotifyPropertyChange(Notification, *SubObject, PropertyEvent, PropertyNode->GetNextNode());
				return;
			}
		}
	}
	else if (Property->IsA<FStructProperty>())
	{
		if (!PropertyNode->GetNextNode())
		{
			if (Notification == EPropertyNotificationType::PostEdit && SubPropertyNode)
			{
				SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
			}
		}
		else
		{
			NotifyPropertyChange(SubPropertyNode, Notification, PropertyEvent, PropertyNode->GetNextNode(), SubValuePtr);
		}
		return;
	}
	else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		if (!PropertyNode->GetNextNode())
		{
			if (Notification == EPropertyNotificationType::PostEdit && SubPropertyNode)
			{
				SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
			}
		}
		else if (UObject* SubObject = ObjectProperty->GetObjectPropertyValue(SubValuePtr))
		{
			// This should not be needed in the property grid, as it should already been called on the subobject.
			OverridableManager.NotifyPropertyChange(Notification, *SubObject, PropertyEvent, PropertyNode->GetNextNode());
		}
		return;
	}
	else if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
	{
		if (!PropertyNode->GetNextNode())
		{
			if (Notification == EPropertyNotificationType::PostEdit && SubPropertyNode)
			{
				SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
			}
		}
		else if (OptionalProperty->IsSet(Data))
		{
			NotifyPropertyChange(SubPropertyNode, Notification, PropertyEvent, PropertyNode->GetNextNode(), OptionalProperty->GetValuePointerForRead(SubValuePtr));
		}
		return;
	}

	UE_CLOG(PropertyNode->GetNextNode(), LogOverridableObject, Warning, TEXT("Unsupported property type(%s), fallback to overriding entire property"), *Property->GetName());
	if (Notification == EPropertyNotificationType::PostEdit)
	{
		if (SubPropertyNode)
		{
			// Replacing this entire property
			SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
		}
	}
}

void FOverriddenPropertySet::RemoveOverriddenSubProperties(FOverriddenPropertyNode& PropertyNode)
{
	for (const auto& Pair : PropertyNode.SubPropertyNodeKeys)
	{
		FOverriddenPropertyNode* RemovedPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(Pair.Value), Pair.Value);
		checkf(RemovedPropertyNode, TEXT("Expecting a node"));
		RemoveOverriddenSubProperties(*RemovedPropertyNode);
		OverriddenPropertyNodes.RemoveByHash(GetTypeHash(Pair.Value), Pair.Value);
	}
	PropertyNode.SubPropertyNodeKeys.Empty();
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, bool* bOutInheritedOperation /*= nullptr*/) const
{
	if (const FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(RootNodeID), RootNodeID))
	{
		return GetOverriddenPropertyOperation(*RootNode, PropertyEvent, PropertyNode, bOutInheritedOperation, Owner);
	}
	return EOverriddenPropertyOperation::None;
}

bool FOverriddenPropertySet::ClearOverriddenProperty(const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode)
{
	if (FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(RootNodeID), RootNodeID))
	{
		return ClearOverriddenProperty(*RootNode, PropertyEvent, PropertyNode, Owner);
	}
	return false;
}

void FOverriddenPropertySet::OverrideProperty(const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data)
{
	FOverriddenPropertyNode& RootPropertyNode = OverriddenPropertyNodes.FindOrAddByHash(GetTypeHash(RootNodeID), RootNodeID);
	NotifyPropertyChange(&RootPropertyNode, EPropertyNotificationType::PreEdit, FPropertyChangedEvent(nullptr), PropertyNode, Data);
	NotifyPropertyChange(&RootPropertyNode, EPropertyNotificationType::PostEdit, PropertyEvent, PropertyNode, Data);
}

void FOverriddenPropertySet::NotifyPropertyChange(const EPropertyNotificationType Notification, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, const void* Data)
{
	NotifyPropertyChange(&OverriddenPropertyNodes.FindOrAddByHash(GetTypeHash(RootNodeID), RootNodeID), Notification, PropertyEvent, PropertyNode, Data);
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property) const
{
	if (const FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(RootNodeID), RootNodeID))
	{
		return GetOverriddenPropertyOperation(*RootNode, CurrentPropertyChain, Property);
	}
	return EOverriddenPropertyOperation::None;
}

FOverriddenPropertyNode* FOverriddenPropertySet::SetOverriddenPropertyOperation(EOverriddenPropertyOperation Operation, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property)
{
	return SetOverriddenPropertyOperation(Operation, OverriddenPropertyNodes.FindOrAddByHash(GetTypeHash(RootNodeID), RootNodeID), CurrentPropertyChain, Property);
}

const FOverriddenPropertyNode* FOverriddenPropertySet::GetOverriddenPropertyNode(const FArchiveSerializedPropertyChain* CurrentPropertyChain) const
{
	if (const FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(RootNodeID), RootNodeID))
	{
		return GetOverriddenPropertyNode(*RootNode, CurrentPropertyChain);
	}
	return nullptr;
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(const FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property) const
{
	// No need to look further
	// if it is the entire property is replaced or
	// if it is the FOverriddenPropertySet struct which is always Overridden
	if (ParentPropertyNode.Operation == EOverriddenPropertyOperation::Replace)
	{
		return EOverriddenPropertyOperation::Replace;
	}

	// @Todo optimize find a way to not have to copy the property chain here.
	FArchiveSerializedPropertyChain PropertyChain(CurrentPropertyChain ? *CurrentPropertyChain : FArchiveSerializedPropertyChain());
	if(Property)
	{
		PropertyChain.PushProperty(Property, Property->IsEditorOnlyProperty());
	}

	TArray<class FProperty*, TInlineAllocator<8>>::TConstIterator PropertyIterator = PropertyChain.GetRootIterator();
	const FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	while (PropertyIterator && OverriddenPropertyNode && OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace)
	{
		const FProperty* CurrentProperty = (*PropertyIterator);
		const FName CurrentPropID = CurrentProperty->GetFName();
		if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentPropID))
		{
			OverriddenPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(*CurrentPropKey), *CurrentPropKey);
			checkf(OverriddenPropertyNode, TEXT("Expecting a node"));
		}
		else
		{
			OverriddenPropertyNode = nullptr;
			break;
		}
		++PropertyIterator;
	}

	return OverriddenPropertyNode ? OverriddenPropertyNode->Operation : EOverriddenPropertyOperation::None;
}

FOverriddenPropertyNode* FOverriddenPropertySet::SetOverriddenPropertyOperation(EOverriddenPropertyOperation Operation, FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property)
{
	// No need to look further
	// if it is the entire property is replaced or
	// if it is the FOverriddenPropertySet struct which is always Overridden
	if (ParentPropertyNode.Operation == EOverriddenPropertyOperation::Replace)
	{
		return nullptr;
	}

	// @Todo optimize find a way to not have to copy the property chain here.
	FArchiveSerializedPropertyChain PropertyChain(CurrentPropertyChain ? *CurrentPropertyChain : FArchiveSerializedPropertyChain());
	if (Property)
	{
		PropertyChain.PushProperty(Property, Property->IsEditorOnlyProperty());
	}

	TArray<class FProperty*, TInlineAllocator<8>>::TConstIterator PropertyIterator = PropertyChain.GetRootIterator();
	FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	while (PropertyIterator && OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace)
	{
		const FProperty* CurrentProperty = (*PropertyIterator);
		const FName CurrentPropID = CurrentProperty->GetFName();
		OverriddenPropertyNode = &FindOrAddNode(*OverriddenPropertyNode, CurrentPropID);
		++PropertyIterator;
	}

	// Might have stop before as one of the parent property was completely replaced.
	if (!PropertyIterator)
	{
		OverriddenPropertyNode->Operation = Operation;
		return OverriddenPropertyNode;
	}

	return nullptr;
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetSubPropertyOperation(FOverriddenPropertyNodeID NodeID) const
{
	const FOverriddenPropertyNode* OverriddenPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(NodeID), NodeID);
	return OverriddenPropertyNode ? OverriddenPropertyNode->Operation : EOverriddenPropertyOperation::None;
}

FOverriddenPropertyNode* FOverriddenPropertySet::SetSubPropertyOperation(EOverriddenPropertyOperation Operation, FOverriddenPropertyNode& Node, FOverriddenPropertyNodeID NodeID)
{
	FOverriddenPropertyNode& OverriddenPropertyNode = FindOrAddNode(Node, NodeID);
	OverriddenPropertyNode.Operation = Operation;
	return &OverriddenPropertyNode;
}

bool FOverriddenPropertySet::IsCDOOwningProperty(const FProperty& Property) const
{
	checkf(Owner, TEXT("Expecting a valid overridable owner"));
	if (!Owner->HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	// We need to serialize only if the property owner is the current CDO class
	// Otherwise on derived class, this is done in parent CDO or it should be explicitly overridden if it is different than the parent value
	// This is sort of like saying it overrides the default property initialization value.
	return Property.GetOwnerClass() == Owner->GetClass();
}

void FOverriddenPropertySet::Reset()
{
	OverriddenPropertyNodes.Reset();
}

void FOverriddenPropertySet::HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& Map)
{
	for (FOverriddenPropertyNode& Node : OverriddenPropertyNodes)
	{
		Node.NodeID.HandleObjectsReInstantiated(Map);
		for (auto& Pair : Node.SubPropertyNodeKeys)
		{
			Pair.Key.HandleObjectsReInstantiated(Map);
			Pair.Value.HandleObjectsReInstantiated(Map);
		}
	}
}

const FOverriddenPropertyNode* FOverriddenPropertySet::GetOverriddenPropertyNode(const FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain) const
{
	if (!CurrentPropertyChain)
	{
		return &ParentPropertyNode;
	}

	TArray<class FProperty*, TInlineAllocator<8>>::TConstIterator PropertyIterator = CurrentPropertyChain->GetRootIterator();
	const FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	while (PropertyIterator && OverriddenPropertyNode)
	{
		const FProperty* CurrentProperty = (*PropertyIterator);
		const FName CurrentPropID = CurrentProperty->GetFName();
		if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentPropID))
		{
			OverriddenPropertyNode = OverriddenPropertyNodes.FindByHash(GetTypeHash(*CurrentPropKey), *CurrentPropKey);
			checkf(OverriddenPropertyNode, TEXT("Expecting a node"));
		}
		else
		{
			OverriddenPropertyNode = nullptr;
			break;
		}
		++PropertyIterator;
	}

	return OverriddenPropertyNode;
}
