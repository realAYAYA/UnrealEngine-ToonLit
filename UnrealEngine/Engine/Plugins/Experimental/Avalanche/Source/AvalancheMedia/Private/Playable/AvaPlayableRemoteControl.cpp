// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/AvaPlayableRemoteControl.h"

#include "Action/RCAction.h"
#include "Action/RCActionContainer.h"
#include "Action/RCPropertyIdAction.h"
#include "AvaMediaSerializationUtils.h"
#include "Backends/JsonStructDeserializerBackend.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "Behaviour/RCBehaviour.h"
#include "Controller/RCController.h"
#include "IRemoteControlModule.h"
#include "RCVirtualProperty.h"
#include "RemoteControlPropertyIdRegistry.h"
#include "Templates/CopyQualifiersFromTo.h"

DEFINE_LOG_CATEGORY(LogAvaPlayableRemoteControl);

namespace UE::AvaPlayableRemoteControl::Private
{
	/**
	 * Safely casts a Remote Control Entity from one Type to the other
	 * @param InEntity the entity to cast
	 * @return the entity casted to the desired type if it was derived from it. nullptr otherwise
	 */
	template<typename To, typename From>
	TSharedPtr<typename TCopyQualifiersFromTo<From, To>::Type> CastEntity(const TSharedPtr<From>& InEntity)
	{
		const UScriptStruct* EntityStruct = InEntity.IsValid() ? InEntity->GetStruct() : nullptr;
		if (EntityStruct && EntityStruct->IsChildOf(To::StaticStruct()))
		{
			return StaticCastSharedPtr<typename TCopyQualifiersFromTo<From, To>::Type>(InEntity);
		}
		return nullptr;
	}

	bool ResolveObjectPropertyForReadOnly(UObject* Object, FRCFieldPathInfo PropertyPath, FRCObjectReference& OutObjectRef, FString& OutErrorText)
	{
		if (!Object)
		{
			OutErrorText = FString::Printf(TEXT("Invalid object to resolve property '%s'"), *PropertyPath.GetFieldName().ToString());
			return false;
		}

		bool bSuccess = true;
		
		if (PropertyPath.GetSegmentCount() != 0)
		{
			if (PropertyPath.Resolve(Object))
			{
				OutObjectRef = FRCObjectReference{ERCAccess::READ_ACCESS, Object, MoveTemp(PropertyPath)};
			}
			else
			{
				OutErrorText = FString::Printf(TEXT("Object property: %s could not be resolved on object: %s"), *PropertyPath.GetFieldName().ToString(), *Object->GetPathName());
				bSuccess = false;
			}
		}
		else
		{
			OutObjectRef = FRCObjectReference{ERCAccess::READ_ACCESS, Object};
		}
		return bSuccess;
	}
	
	inline bool GetObjectRef(const TSharedPtr<const FRemoteControlProperty>& InField, const ERCAccess InAccess, FRCObjectReference& OutObjectRef)
	{
		if (!InField.IsValid())
		{
			return false;
		}
		if (UObject* FieldBoundObject = InField->GetBoundObject(); IsValid(FieldBoundObject))
		{
			FString ErrorText;
			bool bSuccess = false;
			
			if (InAccess == ERCAccess::READ_ACCESS)
			{
				// Fix for private/protected properties: we allow reading the property even if private/protected.
				// This is necessary for the case of private/protect props that have a setter.
				// RC doesn't have a path to allow reading those (it doesn't support getters).
				bSuccess = ResolveObjectPropertyForReadOnly(FieldBoundObject, InField->FieldPathInfo, OutObjectRef, ErrorText);
			}
			else
			{
				bSuccess = IRemoteControlModule::Get().ResolveObjectProperty(InAccess, FieldBoundObject, InField->FieldPathInfo, OutObjectRef, &ErrorText);
			}

			if (bSuccess)
			{
				return true;
			}
			
			UE_LOG(LogAvaPlayableRemoteControl, Error,
				TEXT("Couldn\'t resolve object property \"%s\" in object \"%s\": %s"),
				*InField->FieldName.ToString(), *FieldBoundObject->GetPathName(), *ErrorText);
		}
		else
		{
			UE_LOG(LogAvaPlayableRemoteControl, Error,
				TEXT("Couldn\'t resolve object property \"%s\": Invalid Field Bound Object."),
				*InField->FieldName.ToString());
		}
		return false;
	}
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::GetValueOfEntity(const TSharedPtr<const FRemoteControlEntity>& RemoteControlEntity, TArray<uint8>& OutValue)
{
	using namespace UE::AvaPlayableRemoteControl::Private;
	OutValue.Reset();

	TSharedPtr<const FRemoteControlProperty> Field = CastEntity<FRemoteControlProperty>(RemoteControlEntity);
	if (!Field.IsValid())
	{
		return EAvaPlayableRemoteControlResult::InvalidParameter;
	}

	FRCObjectReference ObjectRef;
	if (!GetObjectRef(Field, ERCAccess::READ_ACCESS, ObjectRef))
	{
		return EAvaPlayableRemoteControlResult::ReadAccessDenied;
	}
	FMemoryWriter Writer = FMemoryWriter(OutValue);
	FJsonStructSerializerBackend WriterBackend = FJsonStructSerializerBackend(Writer, EStructSerializerBackendFlags::Default);
	return IRemoteControlModule::Get().GetObjectProperties(ObjectRef, WriterBackend) ? EAvaPlayableRemoteControlResult::Completed : EAvaPlayableRemoteControlResult::ReadPropertyFailed;
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::GetValueOfEntity(const TSharedPtr<const FRemoteControlEntity>& InRemoteControlEntity, FString& OutValue)
{
	TArray<uint8> ValueAsBytes;
	const EAvaPlayableRemoteControlResult Result = GetValueOfEntity(InRemoteControlEntity, ValueAsBytes);
	AvaMediaSerializationUtils::JsonValueConversion::BytesToString(ValueAsBytes, OutValue);
	return Result;
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::SetValueOfEntity(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, const TArrayView<const uint8>& InValue)
{
	using namespace UE::AvaPlayableRemoteControl::Private;

	TSharedPtr<FRemoteControlProperty> Field = CastEntity<FRemoteControlProperty>(RemoteControlEntity);
	if (!Field.IsValid())
	{
		return EAvaPlayableRemoteControlResult::InvalidParameter;
	}
	
	FRCObjectReference ObjectRefRead;
	if (GetObjectRef(Field, ERCAccess::READ_ACCESS, ObjectRefRead))
	{
		TArray<uint8> CurrentValueAsBytes;
		FMemoryWriter Writer(CurrentValueAsBytes);
		FJsonStructSerializerBackend WriterBackend = FJsonStructSerializerBackend(Writer, EStructSerializerBackendFlags::Default);
		if (IRemoteControlModule::Get().GetObjectProperties(ObjectRefRead, WriterBackend))
		{
			if (CurrentValueAsBytes == InValue)
			{
				// if the given value is already set, don't do anything
				return EAvaPlayableRemoteControlResult::UpToDate;
			}
		}
	}

	FRCObjectReference ObjectRefWrite;
	if (!GetObjectRef(Field, ERCAccess::WRITE_ACCESS, ObjectRefWrite))
	{
		return EAvaPlayableRemoteControlResult::WriteAccessDenied;
	}

	FMemoryReaderView Reader(InValue);
	FJsonStructDeserializerBackend ReaderBackend = FJsonStructDeserializerBackend(Reader);

	// Notes:
	// - if RemoteControl.EnableOngoingChangeOptimization is enabled, PostEditChangeProperty is not called right away
	// there might be a delay (of 0.2 seconds) before it is called.
	// - OnPropertyChangedDelegate (OnExposedPropertiesModified()) is a "per frame" event and is broadcast from URemoteControlPreset::OnEndFrame().
	const bool bDeserializationSucceeded = IRemoteControlModule::Get().SetObjectProperties(ObjectRefWrite, ReaderBackend, ERCPayloadType::Json);

#if WITH_EDITOR
	UObject* const Object = ObjectRefWrite.Object.Get();
	if (bDeserializationSucceeded && IsValid(Object))
	{
		FEditPropertyChain EditPropertyChain;
		ObjectRefWrite.PropertyPathInfo.ToEditPropertyChain(EditPropertyChain);

		// Note: Only PostEditChangeChainProperty is called here because PostEditChangeProperty is already dealt with in RC.
		// Ideally, this should be in RC so that PostEditChangeProperty does not get called twice.
		if (!EditPropertyChain.IsEmpty())
		{
			FPropertyChangedEvent PropertyEvent = ObjectRefWrite.PropertyPathInfo.ToPropertyChangedEvent(EPropertyChangeType::ValueSet);

			FPropertyChangedChainEvent ChainEvent(EditPropertyChain, PropertyEvent);

			TArray<TMap<FString, int32>> ArrayIndicesPerObject;
			{
				TMap<FString, int32> ArrayIndices;
				ArrayIndices.Reserve(ObjectRefWrite.PropertyPathInfo.Segments.Num());

				for (FRCFieldPathSegment& Segment : ObjectRefWrite.PropertyPathInfo.Segments)
				{
					ArrayIndices.Add(Segment.Name.ToString(), Segment.ArrayIndex);
				}

				ArrayIndicesPerObject.Add(MoveTemp(ArrayIndices));
			}

			ChainEvent.ObjectIteratorIndex = 0;
			ChainEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);

			Object->PostEditChangeChainProperty(ChainEvent);
		}
	}
#endif

	return bDeserializationSucceeded ? EAvaPlayableRemoteControlResult::Completed : EAvaPlayableRemoteControlResult::WritePropertyFailed;
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::SetValueOfEntity(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity, const FString& InValue)
{
	return SetValueOfEntity(InRemoteControlEntity, AvaMediaSerializationUtils::JsonValueConversion::ValueToConstBytesView(InValue));
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::GetValueOfController(URCVirtualPropertyBase* InController, TArray<uint8>& OutValue)
{
	OutValue.Reset();

	if (!InController)
	{
		return EAvaPlayableRemoteControlResult::InvalidParameter;
	}

	FMemoryWriter Writer = FMemoryWriter(OutValue);
	FJsonStructSerializerBackend WriterBackend = FJsonStructSerializerBackend(Writer, EStructSerializerBackendFlags::Default);
	InController->SerializeToBackend(WriterBackend);
	return EAvaPlayableRemoteControlResult::Completed;
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::GetValueOfController(URCVirtualPropertyBase* InController, FString& OutValue)
{
	TArray<uint8> ValueAsBytes;
	const EAvaPlayableRemoteControlResult Result = GetValueOfController(InController, ValueAsBytes);

	AvaMediaSerializationUtils::JsonValueConversion::BytesToString(ValueAsBytes, OutValue);
	return Result;
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::SetValueOfController(URCVirtualPropertyBase* InController, const TArrayView<const uint8>& InValue)
{
	if (!InController)
	{
		return EAvaPlayableRemoteControlResult::InvalidParameter;
	}

	// TODO: Use the RC error callback to catch errors in the controller actions.
	FMemoryReaderView Reader(InValue);
	FJsonStructDeserializerBackend ReaderBackend = FJsonStructDeserializerBackend(Reader);
	return InController->DeserializeFromBackend(ReaderBackend) ? EAvaPlayableRemoteControlResult::Completed : EAvaPlayableRemoteControlResult::WritePropertyFailed;
}

EAvaPlayableRemoteControlResult UE::AvaPlayableRemoteControl::SetValueOfController(URCVirtualPropertyBase* InController, const FString& InValue)
{
	return SetValueOfController(InController, AvaMediaSerializationUtils::JsonValueConversion::ValueToConstBytesView(InValue));
}

bool UE::AvaPlayableRemoteControl::GetEntitiesControlledByController(const URemoteControlPreset* InRemoteControlPreset, const URCVirtualPropertyBase* InVirtualProperty, TSet<FGuid>& OutEntityIds)
{
	const URCController* Controller = Cast<URCController>(InVirtualProperty);
	if (!IsValid(Controller))
	{
		return false;
	}

	for (const TObjectPtr<URCBehaviour>& Behaviour : Controller->Behaviours)
	{
		const TSet<TObjectPtr<URCAction>>& Actions = Behaviour->ActionContainer->GetActions();
		for (const TObjectPtr<URCAction>& Action : Actions)
		{
			if (Action)
			{
				if (const URCPropertyIdAction* IdentityAction = Cast<URCPropertyIdAction>(Action.Get()))
				{
					if (const URemoteControlPropertyIdRegistry* IdentityRegistry = InRemoteControlPreset->GetPropertyIdRegistry())
					{
						OutEntityIds.Append(IdentityRegistry->GetEntityIdsForPropertyId(IdentityAction->PropertyId));

						// Explicit support for Sub-PropertyId
						for (const TPair<FPropertyIdContainerKey, TObjectPtr<URCVirtualPropertySelfContainer>>& PropertyContainer : IdentityAction->PropertySelfContainer)
						{
							OutEntityIds.Append(IdentityRegistry->GetEntityIdsForPropertyId(PropertyContainer.Key.PropertyId));
						}
					}
				}
				else
				{
					OutEntityIds.Add(Action->ExposedFieldId);
				}
			}
		}
	}
	return true;
}

FString UE::AvaPlayableRemoteControl::EnumToString(EAvaPlayableRemoteControlResult InValue)
{
	return StaticEnum<EAvaPlayableRemoteControlResult>()->GetNameStringByValue(static_cast<int64>(InValue));
}

namespace UE::AvaPlayableRemoteControl
{
	FScopedPushControllerBehavioursEnable::FScopedPushControllerBehavioursEnable(URCVirtualPropertyBase* InVirtualProperty, bool bInBehavioursEnabled)
		: VirtualProperty(InVirtualProperty)
	{
		if (URCController* const Controller = Cast<URCController>(InVirtualProperty))
		{
			PreviousBehavioursEnabled.Reserve(Controller->Behaviours.Num());			
			for (URCBehaviour* const Behavior : Controller->Behaviours)
			{
				PreviousBehavioursEnabled.Add(Behavior->bIsEnabled);
				Behavior->bIsEnabled = bInBehavioursEnabled;
			}
		}
	}

	FScopedPushControllerBehavioursEnable::~FScopedPushControllerBehavioursEnable()
	{
		if (URCController* const Controller = Cast<URCController>(VirtualProperty))
		{
			int32 BehaviourIndex = 0;
			for (URCBehaviour* const Behavior : Controller->Behaviours)
			{
				Behavior->bIsEnabled = PreviousBehavioursEnabled[BehaviourIndex++];
			}
		}
	}
}