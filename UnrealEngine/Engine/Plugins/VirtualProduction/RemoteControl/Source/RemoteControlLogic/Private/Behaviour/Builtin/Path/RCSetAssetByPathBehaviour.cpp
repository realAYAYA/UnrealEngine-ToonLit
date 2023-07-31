// Copyright Epic Games, Inc. All Rights Reserved.

#include "Behaviour/Builtin/Path/RCSetAssetByPathBehaviour.h"

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Controller/RCController.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2DDynamic.h"
#include "Engine/World.h"
#include "IRemoteControlModule.h"
#include "IRemoteControlPropertyHandle.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstance.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlLogger.h"
#include "RemoteControlPreset.h"

URCSetAssetByPathBehaviour::URCSetAssetByPathBehaviour()
{
	PropertyInContainer = CreateDefaultSubobject<URCVirtualPropertyContainerBase>(FName("VirtualPropertyInContainer"));
}

void URCSetAssetByPathBehaviour::Initialize()
{
	PropertyInContainer->AddProperty(SetAssetByPathBehaviourHelpers::DefaultInput, URCController::StaticClass(), EPropertyBagPropertyType::String);
	UpdateTargetEntity();
	
	Super::Initialize();
}

bool URCSetAssetByPathBehaviour::SetAssetByPath(const FString& AssetPath, const FString& DefaultString)
{
	const URCController* Controller = ControllerWeakPtr.Get();
	
	if (AssetPath.IsEmpty() || !Controller)
	{
		return false;
	}
	
	FString ControllerString;
	if (!Controller->GetValueString(ControllerString))
	{
		return false;
	}
	
	URemoteControlPreset* Preset = Controller->PresetWeakPtr.Get();
	if (!Preset)
	{
		return false;
	}
	
	const TSharedPtr<FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(TargetEntityId).Pin();
	if (!RemoteControlProperty)
	{
		return false;
	}

	FProperty* Property = RemoteControlProperty->GetProperty();
	if (!Property)
	{
		return false;
	}

	if (!bInternal)
	{
		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, [AssetPath, ControllerString]
		{
			return FText::FromString(FString("Path Behaviour attempts to set Asset to %s") + *AssetPath + *ControllerString);
		});

#if WITH_EDITOR
		FEditPropertyChain PropertyChain;
		PropertyChain.AddHead(Property);
		RemoteControlProperty->GetBoundObject()->PreEditChange(PropertyChain);
#endif
		if (SetExternalAsset(AssetPath + ControllerString))
		{
#if WITH_EDITOR
			FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
			RemoteControlProperty->GetBoundObject()->PostEditChangeProperty(ChangedEvent);
#endif
			
			return true;
		}

		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, [AssetPath, DefaultString]
		{
			return FText::FromString(FString("Path Behaviour attempts to set Asset to %s") + *AssetPath + *DefaultString);
		});
		if (!DefaultString.IsEmpty() && SetExternalAsset(AssetPath + DefaultString))
		{
			return true;
		}

		return false;
	}

	FSoftObjectPath MainObjectRef(AssetPath + ControllerString);
	
	if (UObject* Object = MainObjectRef.TryLoad())
	{
		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, [MainObjectRef]
		{
			return FText::FromString(FString("Path Behaviour attempts to set Asset to %s") + *MainObjectRef.GetAssetPathString());
		});

#if WITH_EDITOR
		FEditPropertyChain PropertyChain;
		PropertyChain.AddHead(Property);
		RemoteControlProperty->GetBoundObject()->PreEditChange(PropertyChain);
#endif
		if (SetInternalAsset(Object))
		{
#if WITH_EDITOR
			FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
			RemoteControlProperty->GetBoundObject()->PostEditChangeProperty(ChangedEvent);
#endif

			return true;
		}

		return false;
	}
	
	if (!DefaultString.IsEmpty())
	{
		FSoftObjectPath DefaultObjectRef(AssetPath + DefaultString);
		
		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, [DefaultObjectRef]
		{
			return FText::FromString(FString("Path Behaviour attempts to set Asset to %s") + *DefaultObjectRef.GetAssetPathString());
		});

#if WITH_EDITOR
		FEditPropertyChain PropertyChain;
		PropertyChain.AddHead(Property);
		RemoteControlProperty->GetBoundObject()->PreEditChange(PropertyChain);
#endif
		if (SetInternalAsset(DefaultObjectRef.TryLoad()))
		{
#if WITH_EDITOR
			FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
			RemoteControlProperty->GetBoundObject()->PostEditChangeProperty(ChangedEvent);
#endif
			
			return true;
		}
	}
	
	return false;
}

bool URCSetAssetByPathBehaviour::SetInternalAsset(UObject* SetterObject)
{
	const URCController* Controller = ControllerWeakPtr.Get();
	
	if (!SetterObject || !Controller || !TargetEntity)
	{
		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, [SetterObject]
		{
			return FText::FromString(FString("Path Behaviour fails to set Asset: %s") + *SetterObject->GetName());
		}, EVerbosityLevel::Error);
		return false;
	}
	
	const TWeakObjectPtr<URemoteControlPreset> PresetPtr = Controller->PresetWeakPtr;
	
	TArray<TWeakPtr<FRemoteControlProperty>> ExposedProperties = PresetPtr->GetExposedEntities<FRemoteControlProperty>();

	if (const TWeakPtr<FRemoteControlProperty>* ExposedPropertyPtr = ExposedProperties.FindByPredicate([this](const TWeakPtr<FRemoteControlProperty>& Property){ return Property.Pin()->GetLabel() == TargetEntity->GetLabel(); }))
	{
		const TSharedPtr<FRemoteControlProperty> ExposedProperty = (*ExposedPropertyPtr).Pin();
		if (!ExposedProperty)
		{
			ensureMsgf(false, TEXT("Exposed Property is not valid."));
			return false;
		}
		
		if (AssetClass == UStaticMesh::StaticClass() && SetterObject->IsA(UStaticMesh::StaticClass()))
		{
			UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(ExposedProperty->GetBoundObject());
			StaticMeshComponent->SetStaticMesh(Cast<UStaticMesh>(SetterObject));
			
			UE_LOG(LogRemoteControl, Display, TEXT("Path Behaviour sets Static Mesh Asset %s"), *SetterObject->GetName());
			return true;
		}
		
		if (AssetClass == UMaterial::StaticClass() && SetterObject->IsA(UMaterial::StaticClass()))
		{
			if (UMeshComponent* MaterialOwnerComponent = Cast<UMeshComponent>(ExposedProperty->GetBoundObject()))
			{
				MaterialOwnerComponent->SetMaterial(0, Cast<UMaterial>(SetterObject));
			}
			
			UE_LOG(LogRemoteControl, Display, TEXT("Path Behaviour sets Material Asset %s"), *SetterObject->GetName());
			return true;
		}

		// Only Texture runs through here
		if (GetSupportedClasses().Contains(AssetClass) && SetterObject->IsA(UTexture::StaticClass()))
		{
			if (const TSharedPtr<FRemoteControlProperty> RemoteControlProperty = ExposedProperty)
			{
				SetTextureAsset(RemoteControlProperty, Cast<UTexture>(SetterObject));
			}
		}
		
		return false;
	}
	
	if (TargetEntity)
	{
		const TWeakPtr<const FRemoteControlEntity> ExposedEntity = TargetEntity;
		if (AssetClass == UBlueprint::StaticClass() && SetterObject->IsA(UBlueprint::StaticClass()))
		{
			AActor* OldActor = Cast<AActor>(ExposedEntity.Pin()->GetBoundObject());
			UWorld* World = OldActor->GetWorld();
			
			OldActor->UnregisterAllComponents();
			
			const FVector OldLocation = OldActor->GetActorLocation();
			const FRotator OldRotation = OldActor->GetActorRotation();
			const FName OldActorName = OldActor->GetFName();
			
			FActorSpawnParameters SpawnParams;
			SpawnParams.Name = OldActorName;
			
			const FName OldActorReplacedNamed = MakeUniqueObjectName(OldActor->GetOuter(), OldActor->GetClass(), *FString::Printf(TEXT("%s_REPLACED"), *OldActor->GetFName().ToString()));
			OldActor->Rename(*OldActorReplacedNamed.ToString(), OldActor->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);

			if (AActor* NewActor = World->SpawnActor(Cast<UBlueprint>(SetterObject)->GeneratedClass, &OldLocation, &OldRotation, SpawnParams))
			{
				World->DestroyActor(OldActor);
				
				UE_LOG(LogRemoteControl, Display, TEXT("Path Behaviour sets Blueprint Asset %s"), *SetterObject->GetName());
				return true;
			}

			UE_LOG(LogRemoteControl, Error, TEXT("Path Behaviour unable to delete old Actor %s"), *OldActor->GetName());
			OldActor->UObject::Rename(*OldActorName.ToString(), OldActor->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
			OldActor->RegisterAllComponents();
		}
	}
	
	UE_LOG(LogRemoteControl, Error, TEXT("Path Behaviour fails to set Asset %s"), *SetterObject->GetName());
	return false;
}

TArray<UClass*> URCSetAssetByPathBehaviour::GetSupportedClasses() const
{
	static TArray<UClass*> SupportedClasses =
	{
		UStaticMesh::StaticClass(),
		UMaterial::StaticClass(),
		UTexture::StaticClass(),
		UBlueprint::StaticClass(),
	};

	return SupportedClasses;
}

FString URCSetAssetByPathBehaviour::GetCurrentPath()
{
	FString CurrentPath;
	URCController* RCController = ControllerWeakPtr.Get();
	if (!RCController)
	{
		ensureMsgf(false, TEXT("Controller is invalid."));
		return TEXT("Path is invalid!");
	}

	CurrentPath.Empty();
	FString ControllerString;
	RCController->GetValueString(ControllerString);

	// Add Path String Concat
	CurrentPath = bInternal ? SetAssetByPathBehaviourHelpers::ContentFolder : FString("");
	for (FString PathPart : PathStruct.PathArray)
	{
		if (PathPart.IsEmpty())
		{
			continue;
		}
		
		// Change *INPUT String Token to the correct Controller
		if (PathPart.Contains(SetAssetByPathBehaviourHelpers::InputToken, ESearchCase::CaseSensitive)
			&& PathPart.Find(SetAssetByPathBehaviourHelpers::InputToken) == 0)
		{
			const URemoteControlPreset* RemoteControlPreset = RCController->PresetWeakPtr.Get();
			if (!RemoteControlPreset)
			{
				ensureMsgf(false, TEXT("Remote Control Preset is invalid"));
				continue;
			}
			
			PathPart.RemoveFromStart(SetAssetByPathBehaviourHelpers::InputToken);
			int32 CharIndexStart;
			int32 CharIndexEnd;
			PathPart.FindChar('(', CharIndexStart);
			PathPart.FindChar(')', CharIndexEnd);
			if (CharIndexStart == INDEX_NONE || CharIndexEnd == INDEX_NONE)
			{
				// TODO: Add Message that Input token has been done wrongly.
				ensureMsgf(false, TEXT("Token has been input wrongly"));
				continue;
			}

			PathPart.RemoveAt(CharIndexStart);
			PathPart.RemoveAt(CharIndexEnd-1);
			const URCVirtualPropertyBase* TokenController = RemoteControlPreset->GetControllerByDisplayName(FName(PathPart));
			if (!TokenController)
			{
				ensureMsgf(false, TEXT("No Controller with given name found."));
				continue;
			}

			PathPart = TokenController->GetDisplayValueAsString();
		}

		// Check for certain chars.
		int32 IndexFound;
		if (PathPart.FindLastChar('_', IndexFound) && IndexFound == PathPart.Len() - 1)
		{
			// In the case there's underscore char in the at the end of one of the Path Strings, do nothing to facilitate more complex pathing behaviours.
		}
		else if (!PathPart.FindLastChar('/', IndexFound) || IndexFound < PathPart.Len() - 1)
		{
			PathPart = PathPart.AppendChar('/');
		}

		PathPart.ReplaceCharInline(TCHAR('\\'), TCHAR('/'), ESearchCase::IgnoreCase);
		
		CurrentPath += PathPart;
	}

	return CurrentPath;
}

void URCSetAssetByPathBehaviour::SetTargetEntity(const TSharedPtr<const FRemoteControlEntity>& InEntity)
{
	TargetEntityId = InEntity->GetId();
	TargetEntity = InEntity;
}

TWeakPtr<const FRemoteControlEntity> URCSetAssetByPathBehaviour::GetTargetEntity() const
{
	return TargetEntity;
}

bool URCSetAssetByPathBehaviour::SetExternalAsset(FString InExternalPath)
{
	URCController* Controller = ControllerWeakPtr.Get();
	if (!Controller || !TargetEntity)
	{
		return false;
	}
	
	URemoteControlPreset* PresetPtr = Controller->PresetWeakPtr.Get();
	if (!PresetPtr)
	{
		return false;
	}
	
	TArray<TWeakPtr<FRemoteControlProperty>> ExposedProperties = PresetPtr->GetExposedEntities<FRemoteControlProperty>();
	const TWeakPtr<FRemoteControlProperty>* ExposedPropertyPtr = ExposedProperties.FindByPredicate([this](const TWeakPtr<FRemoteControlProperty>& Property)
	{
		if (const TSharedPtr<FRemoteControlProperty> PinnedProperty = Property.Pin())
		{
			return PinnedProperty->GetLabel() == TargetEntity->GetLabel();
		}
		
		return false;
	});
	const TSharedPtr<FRemoteControlProperty> RemoteControlProperty = ExposedPropertyPtr->Pin();

	if (!RemoteControlProperty)
	{
		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, []
		{
			return FText::FromString(TEXT("Path Behaviour Exposed Property not found"));
		});
		return false;
	}

	UTexture2D* ImportedTexture = UKismetRenderingLibrary::ImportFileAsTexture2D(this, InExternalPath);
	if (!ImportedTexture)
	{
		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, [InExternalPath]
		{
			return FText::FromString(FString("Path Behaviour File not found: ") + *InExternalPath);
		});
		return false;
	}

	// Apply Texture
	if (SetTextureAsset(RemoteControlProperty, ImportedTexture))
	{
		FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, [InExternalPath]
		{
			return FText::FromString(FString("Path Behaviour Set external Asset: ") + *InExternalPath);
		});
		return true;
	}

	FRemoteControlLogger::Get().Log(SetAssetByPathBehaviourHelpers::SetAssetByPathBehaviour, []
	{
		return FText::FromString(TEXT("Path Behaviour Set external Asset failed"));
	});
	return false;
}

bool URCSetAssetByPathBehaviour::SetTextureAsset(TSharedPtr<FRemoteControlProperty> InRemoteControlPropertyPtr, UTexture* InObject)
{
	if (!InObject)
	{
		return false;
	}
	
	FRCObjectReference ObjectRef;
	ObjectRef.Property = InRemoteControlPropertyPtr->GetProperty();
	ObjectRef.Access = InRemoteControlPropertyPtr->GetPropertyHandle()->ShouldGenerateTransaction() ? ERCAccess::WRITE_TRANSACTION_ACCESS : ERCAccess::WRITE_ACCESS;
	ObjectRef.PropertyPathInfo = InRemoteControlPropertyPtr->FieldPathInfo.ToString();

	for (UObject* Object : InRemoteControlPropertyPtr->GetBoundObjects())
	{
		if (IRemoteControlModule::Get().ResolveObjectProperty(ObjectRef.Access, Object, ObjectRef.PropertyPathInfo, ObjectRef))
		{
			/**
			 * Setting with Pointer and using Serialization of itself afterwards as a workaround with the Texture Asset not updating in the world.
			 */
			FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InRemoteControlPropertyPtr->GetProperty());
			if (!ObjectProperty)
			{
				return false;
			}

			FProperty* Property = InRemoteControlPropertyPtr->GetProperty();
			uint8* ValueAddress = Property->ContainerPtrToValuePtr<uint8>(ObjectRef.ContainerAdress);
			ObjectProperty->SetObjectPropertyValue(ValueAddress, InObject);

			TArray<uint8> Buffer;
			FMemoryReader Reader(Buffer);
			FCborStructDeserializerBackend DeserializerBackend(Reader);

			IRemoteControlModule::Get().SetObjectProperties(ObjectRef, DeserializerBackend, ERCPayloadType::Cbor, Buffer);
			return true;
		}
	}
	return false;	
}

void URCSetAssetByPathBehaviour::UpdateTargetEntity()
{
	const URCController* Controller = ControllerWeakPtr.Get();
	if (!Controller)
	{
		return;
	}
	
	const URemoteControlPreset* PresetPtr = Controller->PresetWeakPtr.Get();
	if (!PresetPtr)
	{
		return;
	}

	TWeakPtr<const FRemoteControlEntity> ExposedEntity = PresetPtr->GetExposedEntity<FRemoteControlEntity>(TargetEntityId);
	if (!ExposedEntity.Pin())
	{
		return;
	}
	
	TargetEntity = ExposedEntity.Pin();
}

void URCSetAssetByPathBehaviour::RefreshPathArray()
{
	if (PathStruct.PathArray.Num() < 1)
	{
		PathStruct.PathArray.Add("");
	}
}

