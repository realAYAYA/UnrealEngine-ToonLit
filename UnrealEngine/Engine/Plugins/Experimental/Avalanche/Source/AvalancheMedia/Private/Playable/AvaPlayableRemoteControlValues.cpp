// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/AvaPlayableRemoteControlValues.h"

#include "AvaMediaSerializationUtils.h"
#include "Playable/AvaPlayableRemoteControl.h"
#include "Playable/AvaPlayableRemoteControlValuesPrivate.h"
#include "RCVirtualProperty.h"
#include "Serialization/CustomVersion.h"

namespace UE::AvaPlayableRemoteControlValues::Private
{
	bool PruneValues(const TMap<FGuid, FAvaPlayableRemoteControlValue>& InValues, TMap<FGuid, FAvaPlayableRemoteControlValue>& OutValues)
	{
		bool bModified = false;
		for (TMap<FGuid, FAvaPlayableRemoteControlValue>::TIterator ValueIterator(OutValues); ValueIterator; ++ValueIterator)
		{
			if (!InValues.Contains(ValueIterator->Key))
			{
				ValueIterator.RemoveCurrent();
				bModified = true;
			}
		}
		return bModified;
	}
	
	bool UpdateValues(const TMap<FGuid, FAvaPlayableRemoteControlValue>& InValues, TMap<FGuid, FAvaPlayableRemoteControlValue>& OutValues, bool bInUpdateDefaults)
	{
		// Remove property values that are no longer exposed.
		bool bModified = PruneValues(InValues, OutValues);
	
		// Add missing property values and optionally update the default values.
		for (const TPair<FGuid, FAvaPlayableRemoteControlValue>& SourceValue : InValues)
		{
			FAvaPlayableRemoteControlValue* ExistingValue = OutValues.Find(SourceValue.Key);
			if (!ExistingValue)
			{
				// Remark: IsDefault flag follows along.
				OutValues.Add(SourceValue.Key, SourceValue.Value);
				bModified = true;
			}
			else if (bInUpdateDefaults && ExistingValue->bIsDefault && !ExistingValue->IsSameValueAs(SourceValue.Value))
			{
				ExistingValue->SetValueFrom(SourceValue.Value);
				bModified = true;
			}
		}
		return bModified;
	}

	bool HasSameValues(const TMap<FGuid, FAvaPlayableRemoteControlValue>& InValues, const TMap<FGuid, FAvaPlayableRemoteControlValue>& InOtherValues)
	{
		// If values count differ, consider as different
		if (InValues.Num() != InOtherValues.Num())
		{
			return false;
		}

		// Both Value maps have the same count, so one cannot be a subset of another, therefore, a single find pass should determine equality 
		for (const TPair<FGuid, FAvaPlayableRemoteControlValue>& Pair : InValues)
		{
			const FAvaPlayableRemoteControlValue* FoundValue = InOtherValues.Find(Pair.Key);
			if (!FoundValue || !FoundValue->IsSameValueAs(Pair.Value))
			{
				// Other's Value wasn't found, or was different from the value of this
				return false;
			}
		}

		return true;
	}

	EAvaPlayableRemoteControlChanges ToRemoteControlChanges(bool bInModified, EAvaPlayableRemoteControlChanges InModifiedChanges)
	{
		return bInModified ? InModifiedChanges : EAvaPlayableRemoteControlChanges::None;
	}
}

const FGuid FAvaPlayableRemoteControlValueCustomVersion::Key(0x85218F83, 0xEDF141CA, 0x800EF947, 0x2F14CB06);
FCustomVersionRegistration GRegisterAvaPlayableRemoteControlValueCustomVersion(FAvaPlayableRemoteControlValueCustomVersion::Key
	, FAvaPlayableRemoteControlValueCustomVersion::LatestVersion
	, TEXT("AvaPlayableRemoteControlValueVersion"));

bool FAvaPlayableRemoteControlValue::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAvaPlayableRemoteControlValueCustomVersion::Key);
	
	if (Ar.CustomVer(FAvaPlayableRemoteControlValueCustomVersion::Key) >= FAvaPlayableRemoteControlValueCustomVersion::ValueAsString)
	{
		UScriptStruct* Struct = FAvaPlayableRemoteControlValue::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, reinterpret_cast<uint8*>(this), Struct, nullptr);
	}
	else
	{
		FAvaPlayableRemoteControlValueAsBytes_Legacy LegacyValue;
		UScriptStruct* Struct = FAvaPlayableRemoteControlValueAsBytes_Legacy::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, reinterpret_cast<uint8*>(&LegacyValue), Struct, nullptr);

		UE::AvaMediaSerializationUtils::JsonValueConversion::BytesToString(LegacyValue.Bytes, Value);
		bIsDefault = LegacyValue.bIsDefault;
	}

	return true;
}

void FAvaPlayableRemoteControlValues::RefreshControlledEntities(const URemoteControlPreset* InRemoteControlPreset)
{
	EntitiesControlledByController.Reset();

	if (IsValid(InRemoteControlPreset))
	{
		const TArray<URCVirtualPropertyBase*> Controllers = InRemoteControlPreset->GetControllers();
		for (const URCVirtualPropertyBase* PropertyBase : Controllers)
		{
			if (!UE::AvaPlayableRemoteControl::GetEntitiesControlledByController(InRemoteControlPreset, PropertyBase, EntitiesControlledByController))
			{
				UE_LOG(LogAvaPlayableRemoteControl, Warning, TEXT("Failed to get controlled entities for controller \"%s\" (id:%s)."),
					*PropertyBase->DisplayName.ToString(), *PropertyBase->Id.ToString());
			}
		}
	}
}

void FAvaPlayableRemoteControlValues::CopyFrom(const URemoteControlPreset* InRemoteControlPreset, bool bInIsDefault)
{
	using namespace UE::AvaPlayableRemoteControl;

	EntityValues.Reset();
	ControllerValues.Reset();

	RefreshControlledEntities(InRemoteControlPreset);	// will reset ids if invalid.
	
	if (IsValid(InRemoteControlPreset))
	{
		FString ValueAsString;
		for (const TWeakPtr<const FRemoteControlEntity>& EntityWeakPtr : InRemoteControlPreset->GetExposedEntities<FRemoteControlEntity>())
		{
			const TSharedPtr<const FRemoteControlEntity> Entity = EntityWeakPtr.Pin();
			if (!Entity.IsValid())
			{
				continue;
			}

			const EAvaPlayableRemoteControlResult Result = GetValueOfEntity(Entity, ValueAsString);

			if (Failed(Result))
			{
				UE_LOG(LogAvaPlayableRemoteControl, Error,
					TEXT("Failed to read value of entity \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
					*Entity->GetLabel().ToString(), *Entity->GetId().ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
				continue;
			}
			
			EntityValues.Add(Entity->GetId(), FAvaPlayableRemoteControlValue(ValueAsString, bInIsDefault));
		}

		TArray<URCVirtualPropertyBase*> Controllers = InRemoteControlPreset->GetControllers();
		for (URCVirtualPropertyBase* Controller : Controllers)
		{
			const EAvaPlayableRemoteControlResult Result = GetValueOfController(Controller, ValueAsString);
	
			if (Failed(Result))
			{
				UE_LOG(LogAvaPlayableRemoteControl, Error,
					TEXT("Failed to read value of controller \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
					*Controller->DisplayName.ToString(), *Controller->Id.ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
				continue;
			}
			
			ControllerValues.Add(Controller->Id, FAvaPlayableRemoteControlValue(ValueAsString, bInIsDefault));
		}
	}
}

bool FAvaPlayableRemoteControlValues::HasSameEntityValues(const FAvaPlayableRemoteControlValues& InOther) const
{
	return UE::AvaPlayableRemoteControlValues::Private::HasSameValues(EntityValues, InOther.EntityValues);
}

bool FAvaPlayableRemoteControlValues::HasSameControllerValues(const FAvaPlayableRemoteControlValues& InOther) const
{
	return UE::AvaPlayableRemoteControlValues::Private::HasSameValues(ControllerValues, InOther.ControllerValues);
}

EAvaPlayableRemoteControlChanges FAvaPlayableRemoteControlValues::PruneRemoteControlValues(const FAvaPlayableRemoteControlValues& InRemoteControlValues)
{
	using namespace UE::AvaPlayableRemoteControlValues::Private;
	return ToRemoteControlChanges(PruneValues(InRemoteControlValues.EntityValues, EntityValues), EAvaPlayableRemoteControlChanges::EntityValues) 
		| ToRemoteControlChanges(PruneValues(InRemoteControlValues.ControllerValues, ControllerValues), EAvaPlayableRemoteControlChanges::ControllerValues); 
}

EAvaPlayableRemoteControlChanges FAvaPlayableRemoteControlValues::UpdateRemoteControlValues(const FAvaPlayableRemoteControlValues& InRemoteControlValues, bool bInUpdateDefaults)
{
	using namespace UE::AvaPlayableRemoteControlValues::Private;
	return ToRemoteControlChanges(UpdateValues(InRemoteControlValues.EntityValues, EntityValues, bInUpdateDefaults), EAvaPlayableRemoteControlChanges::EntityValues) 
		| ToRemoteControlChanges(UpdateValues(InRemoteControlValues.ControllerValues, ControllerValues, bInUpdateDefaults), EAvaPlayableRemoteControlChanges::ControllerValues); 
}

bool FAvaPlayableRemoteControlValues::SetEntityValue(const FGuid& InId, const URemoteControlPreset* InRemoteControlPreset, bool bInIsDefault)
{
	const TSharedPtr<const FRemoteControlEntity> Entity = InRemoteControlPreset->GetExposedEntity<FRemoteControlEntity>(InId).Pin();

	if (!Entity)
	{
		UE_LOG(LogAvaPlayableRemoteControl, Error,
			TEXT("Requested entity id \"%s\" was not found in RemoteControlPreset \"%s\"."),
			*InId.ToString(), *InRemoteControlPreset->GetName());
		return false;
	}

	using namespace UE::AvaPlayableRemoteControl;
	FAvaPlayableRemoteControlValue Value;
	Value.bIsDefault = bInIsDefault;

	const EAvaPlayableRemoteControlResult Result = GetValueOfEntity(Entity, Value.Value);

	if (Failed(Result))
	{
		UE_LOG(LogAvaPlayableRemoteControl, Error,
			TEXT("Failed to read value of entity \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
			*Entity->GetLabel().ToString(), *InId.ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
		return false;
	}

	EntityValues.Add(Entity->GetId(), MoveTemp(Value));
	return true;
}

bool FAvaPlayableRemoteControlValues::SetControllerValue(const FGuid& InId, const URemoteControlPreset* InRemoteControlPreset, bool bInIsDefault)
{
	URCVirtualPropertyBase* Controller = InRemoteControlPreset->GetController(InId);

	if (!Controller)
	{
		UE_LOG(LogAvaPlayableRemoteControl, Error,
			TEXT("Requested controller id \"%s\" was not found in RemoteControlPreset \"%s\"."),
			*InId.ToString(), *InRemoteControlPreset->GetName());
		return false;
	}

	using namespace UE::AvaPlayableRemoteControl;
	FAvaPlayableRemoteControlValue Value;
	Value.bIsDefault = bInIsDefault;

	const EAvaPlayableRemoteControlResult Result = GetValueOfController(Controller, Value.Value);
	
	if (Failed(Result))
	{
		UE_LOG(LogAvaPlayableRemoteControl, Error,
			TEXT("Failed to read value of controller \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
			*Controller->DisplayName.ToString(), *InId.ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
		return false;
	}
		
	ControllerValues.Add(InId, MoveTemp(Value));
	return true;
}

void FAvaPlayableRemoteControlValues::ApplyEntityValuesToRemoteControlPreset(URemoteControlPreset* InRemoteControlPreset) const
{
	if (!InRemoteControlPreset)
	{
		return;
	}
	using namespace UE::AvaPlayableRemoteControl;
	for (const TWeakPtr<FRemoteControlEntity>& EntityWeakPtr : InRemoteControlPreset->GetExposedEntities<FRemoteControlEntity>())
	{
		if (const TSharedPtr<FRemoteControlEntity> Entity = EntityWeakPtr.Pin())
		{
			if (const FAvaPlayableRemoteControlValue* Value = GetEntityValue(Entity->GetId()))
			{
				const EAvaPlayableRemoteControlResult Result = SetValueOfEntity(Entity, Value->Value); 
				if (Failed(Result))
				{
					UE_LOG(LogAvaPlayableRemoteControl, Error, TEXT("Failed to set value of exposed entity \"%s\" (id:%s): %s."),
						*Entity->GetLabel().ToString(), *Entity->GetId().ToString(), *EnumToString(Result));
				}
			}
			else
			{
				UE_LOG(LogAvaPlayableRemoteControl, Error, TEXT("Exposed entity \"%s\" (id:%s): value not found in page."),
					*Entity->GetLabel().ToString(), *Entity->GetId().ToString());
			}
		}
	}
}

void FAvaPlayableRemoteControlValues::ApplyControllerValuesToRemoteControlPreset(URemoteControlPreset* InRemoteControlPreset, bool bInForceDisableBehaviors) const
{
	if (!InRemoteControlPreset)
	{
		return;
	}
	TArray<URCVirtualPropertyBase*> Controllers = InRemoteControlPreset->GetControllers();
	for (URCVirtualPropertyBase* Controller : Controllers)
	{
		if (const FAvaPlayableRemoteControlValue* Value = GetControllerValue(Controller->Id))
		{
			using namespace UE::AvaPlayableRemoteControl;
			EAvaPlayableRemoteControlResult Result;
			if (bInForceDisableBehaviors)
			{
				FScopedPushControllerBehavioursEnable PushBehavioursEnable(Controller, false);
				Result = SetValueOfController(Controller, Value->Value);
			}
			else
			{
				Result = SetValueOfController(Controller, Value->Value);
			}
			if (Failed(Result))
			{
				UE_LOG(LogAvaPlayableRemoteControl, Error, TEXT("Failed to set virtual value of controller \"%s\" (id:%s): %s."),
					*Controller->DisplayName.ToString(), *Controller->Id.ToString(), *EnumToString(Result));
			}
		}
		else
		{
			UE_LOG(LogAvaPlayableRemoteControl, Error, TEXT("Controller \"%s\" (id:%s): value not found in page."),
				*Controller->DisplayName.ToString(), *Controller->Id.ToString());
		}
	}
}

bool FAvaPlayableRemoteControlValues::HasIdCollisions(const FAvaPlayableRemoteControlValues& InOtherValues) const
{
	const bool bHasControllerIdCollisions = HasIdCollisions(ControllerValues, InOtherValues.ControllerValues);
	const bool bHasEntityIdCollisions = HasIdCollisions(EntityValues, InOtherValues.EntityValues);
	return bHasControllerIdCollisions || bHasEntityIdCollisions; 
}

bool FAvaPlayableRemoteControlValues::Merge(const FAvaPlayableRemoteControlValues& InOtherValues)
{
	const bool bHasIdCollisions = HasIdCollisions(InOtherValues);

	ControllerValues.Append(InOtherValues.ControllerValues);
	EntityValues.Append(InOtherValues.EntityValues);		
	EntitiesControlledByController.Append(InOtherValues.EntitiesControlledByController);

	return !bHasIdCollisions;
}

bool FAvaPlayableRemoteControlValues::HasIdCollisions(const TMap<FGuid, FAvaPlayableRemoteControlValue>& InValues, const TMap<FGuid, FAvaPlayableRemoteControlValue>& InOtherValues)
{
	for (const TPair<FGuid, FAvaPlayableRemoteControlValue>& ValueEntry : InValues)
	{
		if (InOtherValues.Contains(ValueEntry.Key))
		{
			return true;
		}
	}
	return false;
}

const FAvaPlayableRemoteControlValues& FAvaPlayableRemoteControlValues::GetDefaultEmpty()
{
	const static FAvaPlayableRemoteControlValues Empty;
	return Empty;
}
