// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariantManagerBlueprintLibrary.h"

#include "CapturableProperty.h"
#include "LevelVariantSets.h"
#include "LevelVariantSetsActor.h"
#include "PropertyValue.h"
#include "Variant.h"
#include "VariantManager.h"
#include "VariantManagerContentEditorModule.h"
#include "VariantManagerLog.h"
#include "VariantManagerPropertyCapturer.h"
#include "VariantObjectBinding.h"
#include "VariantSet.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

TUniquePtr<FVariantManager> UVariantManagerBlueprintLibrary::VariantManager;

UVariantManagerBlueprintLibrary::UVariantManagerBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!VariantManager.IsValid())
	{
		VariantManager = MakeUnique<FVariantManager>();
	}
}

ULevelVariantSets* UVariantManagerBlueprintLibrary::CreateLevelVariantSetsAsset(const FString& AssetName, const FString& AssetPath)
{
	IVariantManagerContentEditorModule& ContentEditorModule = FModuleManager::LoadModuleChecked<IVariantManagerContentEditorModule>(VARIANTMANAGERCONTENTEDITORMODULE_MODULE_NAME);
	return Cast<ULevelVariantSets>(ContentEditorModule.CreateLevelVariantSetsAsset(AssetName, AssetPath));
}

ALevelVariantSetsActor* UVariantManagerBlueprintLibrary::CreateLevelVariantSetsActor(ULevelVariantSets* LevelVariantSetsAsset)
{
	if (LevelVariantSetsAsset == nullptr)
	{
		return nullptr;
	}

	IVariantManagerContentEditorModule& ContentEditorModule = FModuleManager::LoadModuleChecked<IVariantManagerContentEditorModule>(VARIANTMANAGERCONTENTEDITORMODULE_MODULE_NAME);
	return Cast<ALevelVariantSetsActor>(ContentEditorModule.GetOrCreateLevelVariantSetsActor(LevelVariantSetsAsset));
}




void UVariantManagerBlueprintLibrary::AddVariantSet(ULevelVariantSets* LevelVariantSets, UVariantSet* VariantSet)
{
	if (LevelVariantSets == nullptr || VariantSet == nullptr)
	{
		return;
	}

	VariantManager->AddVariantSets({VariantSet}, LevelVariantSets);
}

void UVariantManagerBlueprintLibrary::AddVariant(UVariantSet* VariantSet, UVariant* Variant)
{
	if (VariantSet == nullptr || Variant == nullptr)
	{
		return;
	}

	VariantManager->AddVariants({Variant}, VariantSet);
}

void UVariantManagerBlueprintLibrary::AddActorBinding(UVariant* Variant, AActor* Actor)
{
	if (Variant == nullptr || Actor == nullptr)
	{
		return;
	}

	VariantManager->CreateObjectBindings({Actor}, {Variant});
}




void UVariantManagerBlueprintLibrary::RemoveVariantSet(ULevelVariantSets* LevelVariantSets, UVariantSet* VariantSet)
{
	if (LevelVariantSets == nullptr || VariantSet == nullptr)
	{
		return;
	}

	VariantManager->RemoveVariantSetsFromParent({VariantSet});
}

void UVariantManagerBlueprintLibrary::RemoveVariant(UVariantSet* VariantSet, UVariant* Variant)
{
	if (VariantSet == nullptr || Variant == nullptr)
	{
		return;
	}

	VariantManager->RemoveVariantsFromParent({Variant});
}

void UVariantManagerBlueprintLibrary::RemoveActorBinding(UVariant* Variant, AActor* Actor)
{
	if (Variant == nullptr || Actor == nullptr)
	{
		return;
	}

	UVariantObjectBinding* TargetBinding = nullptr;

	const TArray<UVariantObjectBinding*>& Bindings = Variant->GetBindings();
	for (UVariantObjectBinding* Binding : Bindings)
	{
		if (Binding->GetObject() == Actor)
		{
			TargetBinding = Binding;
			break;
		}
	}

	VariantManager->RemoveObjectBindingsFromParent({TargetBinding});
}




void UVariantManagerBlueprintLibrary::RemoveVariantSetByName(ULevelVariantSets* LevelVariantSets, const FString& VariantSetName)
{
	if (LevelVariantSets == nullptr)
	{
		return;
	}

	UVariantSet* VarSet = LevelVariantSets->GetVariantSetByName(VariantSetName);
	if (VarSet)
	{
		VariantManager->RemoveVariantSetsFromParent({VarSet});
	}
}

void UVariantManagerBlueprintLibrary::RemoveVariantByName(UVariantSet* VariantSet, const FString& VariantName)
{
	if (VariantSet == nullptr)
	{
		return;
	}

	UVariant* Var = VariantSet->GetVariantByName(VariantName);
	if (Var)
	{
		VariantManager->RemoveVariantsFromParent({Var});
	}
}

void UVariantManagerBlueprintLibrary::RemoveActorBindingByName(UVariant* Variant, const FString& ActorName)
{
	if (Variant == nullptr)
	{
		return;
	}

	UVariantObjectBinding* Binding = Variant->GetBindingByName(ActorName);

	if (Binding)
	{
		VariantManager->RemoveObjectBindingsFromParent({Binding});
	}
}




void UVariantManagerBlueprintLibrary::Record(UPropertyValue* PropVal)
{
	if (PropVal == nullptr)
	{
		return;
	}

	PropVal->RecordDataFromResolvedObject();
}

void UVariantManagerBlueprintLibrary::Apply(UPropertyValue* PropVal)
{
	if (PropVal == nullptr)
	{
		return;
	}

	PropVal->ApplyDataToResolvedObject();
}

using MapEntry = TPairInitializer<const UClass*&, const FString&>;

FString UVariantManagerBlueprintLibrary::GetPropertyTypeString(UPropertyValue* PropVal)
{
	if (PropVal == nullptr)
	{
		UE_LOG(LogVariantManager, Error, TEXT("Input UPropertyValue was nullptr!"));
		return FString();
	}

	FFieldClass* PropClass = PropVal->GetPropertyClass();
	if (PropClass->IsChildOf(FStructProperty::StaticClass()))
	{
		if (UStruct* Struct = PropVal->GetStructPropertyStruct())
		{
			if ( Struct->GetFName() == NAME_Rotator )
			{
				return TEXT("rotator");
			}
			else if ( Struct->GetFName() == NAME_Color )
			{
				return TEXT("color");
			}
			else if ( Struct->GetFName() == NAME_LinearColor )
			{
				return TEXT("linear_color");
			}
			else if ( Struct->GetFName() == NAME_Vector )
			{
				return TEXT("vector");
			}
			else if ( Struct->GetFName() == NAME_Quat )
			{
				return TEXT("quat");
			}
			else if ( Struct->GetFName() == NAME_Vector4 )
			{
				return TEXT("vector4");
			}
			else if ( Struct->GetFName() == NAME_Vector2D )
			{
				return TEXT("vector2d");
			}
			else if ( Struct->GetFName() == NAME_IntPoint )
			{
				return TEXT("int_point");
			}
		}
	}
	else if (PropClass->IsChildOf(FNumericProperty::StaticClass()))
	{
		if (PropVal->IsNumericPropertyFloatingPoint())
		{
			return TEXT("float");
		}
		else
		{
			return TEXT("int");
		}
	}
	else if (PropClass->IsChildOf(FBoolProperty::StaticClass()))
	{
		return TEXT("bool");
	}
	else if (PropClass->IsChildOf(FStrProperty::StaticClass()) ||
		     PropClass->IsChildOf(FTextProperty::StaticClass()) ||
		     PropClass->IsChildOf(FNameProperty::StaticClass()))
	{
		return TEXT("string");
	}
	else if (PropClass->IsChildOf(FObjectProperty::StaticClass()) || PropClass->IsChildOf(FInterfaceProperty::StaticClass()))
	{
		return TEXT("object");
	}

	UE_LOG(LogVariantManager, Error, TEXT("Invalid property type for UPropertyValue '%s'!"), *PropVal->GetFullDisplayString());
	return FString();
}


TArray<FString> UVariantManagerBlueprintLibrary::GetCapturableProperties(UObject* ActorOrClass)
{
	if (ActorOrClass == nullptr)
	{
		return {};
	}

	TArray<FString> Result;
	TArray<TSharedPtr<FCapturableProperty>> OutProps;
	const bool bCaptureAllArrayIndices = false;
	FString TargetPropertyPath;

	if (AActor* Actor = Cast<AActor>(ActorOrClass))
	{
		VariantManager->GetCapturableProperties({Actor}, OutProps, TargetPropertyPath, bCaptureAllArrayIndices);
	}
	else if (UClass* Class = Cast<UClass>(ActorOrClass))
	{
		VariantManager->GetCapturableProperties({Class}, OutProps, TargetPropertyPath, bCaptureAllArrayIndices);
	}

	Result.Reserve(OutProps.Num());
	for (const TSharedPtr<FCapturableProperty>& PropPtr : OutProps)
	{
		Result.Add(PropPtr->DisplayName);
	}

	return Result;
}

UPropertyValue* UVariantManagerBlueprintLibrary::CaptureProperty(UVariant* Variant, AActor* Actor, FString PropertyPath)
{
	if (Variant == nullptr)
	{
		UE_LOG(LogVariantManager, Error, TEXT("Variant was null!"));
		return nullptr;
	}

	if (Actor == nullptr)
	{
		UE_LOG(LogVariantManager, Error, TEXT("Actor was null!"));
		return nullptr;
	}

	if (PropertyPath.IsEmpty())
	{
		UE_LOG(LogVariantManager, Error, TEXT("PropertyPath was empty!"));
		return nullptr;
	}

	UVariantObjectBinding* TargetBinding = nullptr;

	const TArray<UVariantObjectBinding*>& Bindings = Variant->GetBindings();
	for (UVariantObjectBinding* Binding : Bindings)
	{
		if (Binding->GetObject() == Actor)
		{
			TargetBinding = Binding;
			break;
		}
	}

	if (TargetBinding == nullptr)
	{
		UE_LOG(LogVariantManager, Error, TEXT("Variant '%s' does not have a binding to actor '%s'. Use 'variant.add_actor_binding(actor)' to create one"), *Variant->GetDisplayText().ToString(), *Actor->GetActorLabel());
		return nullptr;
	}

	const bool bCaptureAllArrayIndices = false;
	TArray<TSharedPtr<FCapturableProperty>> OutProps;
	VariantManager->GetCapturableProperties({Actor}, OutProps, PropertyPath, bCaptureAllArrayIndices);

	if (OutProps.Num() < 1)
	{
		UE_LOG(LogVariantManager, Error, TEXT("Actor '%s' does not have a property with path '%s'!"), *Actor->GetActorLabel(), *PropertyPath);
		return nullptr;
	}

	TArray<UPropertyValue*> CreatedProps = VariantManager->CreatePropertyCaptures(OutProps, {TargetBinding});
	if (CreatedProps.Num() > 0)
	{
		return CreatedProps[0];
	}

	return nullptr;
}

int32 UVariantManagerBlueprintLibrary::AddDependency( UVariant* Variant, FVariantDependency& Dependency )
{
	if ( Variant )
	{
		return Variant->AddDependency(Dependency);
	}

	return INDEX_NONE;
}

void UVariantManagerBlueprintLibrary::SetDependency( UVariant* Variant, int32 Index, FVariantDependency& Dependency )
{
	if ( Variant )
	{
		Variant->SetDependency(Index, Dependency);
	}
}

void UVariantManagerBlueprintLibrary::DeleteDependency( UVariant* Variant, int32 Index )
{
	if ( Variant )
	{
		Variant->DeleteDependency(Index);
	}
}

TArray<UPropertyValue*> UVariantManagerBlueprintLibrary::GetCapturedProperties(UVariant* Variant, AActor* Actor)
{
	if (Variant == nullptr || Actor == nullptr)
	{
		return {};
	}

	TArray<UPropertyValue*> Result;
	for (UVariantObjectBinding* Binding : Variant->GetBindings())
	{
		if (AActor* ValidActor = Cast<AActor>(Binding->GetObject()))
		{
			if (ValidActor == Actor)
			{
				for (UPropertyValue* Prop : Binding->GetCapturedProperties())
				{
					Result.Add(Prop);
				}
				break;
			}
		}
	}

	return Result;
}

void UVariantManagerBlueprintLibrary::RemoveCapturedProperty(UVariant* Variant, AActor* Actor, UPropertyValue* Property)
{
	if (Variant == nullptr || Actor == nullptr || Property == nullptr)
	{
		return;
	}

	VariantManager->RemovePropertyCapturesFromParent({Property});
}

void UVariantManagerBlueprintLibrary::RemoveCapturedPropertyByName(UVariant* Variant, AActor* Actor, FString PropertyPath)
{
	if (Variant == nullptr || Actor == nullptr)
	{
		return;
	}

	UVariantObjectBinding* const* FoundBindingPtr = Variant->GetBindings().FindByPredicate([Actor](const UVariantObjectBinding* Binding)
	{
		AActor* ThisActor = Cast<AActor>(Binding->GetObject());
		return ThisActor && ThisActor == Actor;
	});

	if (FoundBindingPtr)
	{
		const TArray<UPropertyValue*>& Properties = (*FoundBindingPtr)->GetCapturedProperties();
		UPropertyValue* const* FoundPropPtr = Properties.FindByPredicate([&PropertyPath](const UPropertyValue* ThisProp)
		{
			return ThisProp && ThisProp->GetFullDisplayString() == PropertyPath;
		});

		if (FoundPropPtr)
		{
			VariantManager->RemovePropertyCapturesFromParent({*FoundPropPtr});
		}
	}
}

template<typename T>
void SetPropertyValueImpl(UPropertyValue* Property, T InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	Property->SetRecordedData((uint8*)&InValue, sizeof(T));
}

// Gets the value of Property as a T. This getter deep copies the bytes of the
// value object and returns it. It is necessary for value types and structs, as using
// the version for ref types below would (I think) try to return the object at the
// actual property recorded data bytes
template<typename T>
T GetPropertyValueImpl(UPropertyValue* Property, T InDefaultValue)
{
	if (Property == nullptr)
	{
		UE_LOG(LogVariantManager, Error, TEXT("Tried to access recorded data from an invalid property!"));
		return InDefaultValue;
	}

	if (!Property->HasRecordedData())
	{
		UE_LOG(LogVariantManager, Error, TEXT("Tried to access recorded data from property '%s', which does not have any!"), *Property->GetFullDisplayString());
		return InDefaultValue;
	}

	T Result;
	FMemory::Memcpy(&Result, Property->GetRecordedData().GetData(), sizeof(T));
	return Result;
}

// Gets the value of Property as a T. This getter interprets the stored data as
// a T and returns a copy of it calling its copy constructor. It is necessary for
// the string types (FString, FText, FName), which are reference types. Just
// deep-copying the bytes as in the value type version above would obviously not
// trigger the copying of resources these reference types manage (char arrays, etc)
template<typename T>
T GetPropertyValueRefTypeImpl(UPropertyValue* Property, T InDefaultValue)
{
	if (Property == nullptr)
	{
		UE_LOG(LogVariantManager, Error, TEXT("Tried to access recorded data from an invalid property!"));
		return InDefaultValue;
	}

	if (!Property->HasRecordedData())
	{
		UE_LOG(LogVariantManager, Error, TEXT("Tried to access recorded data from property '%s', which does not have any!"), *Property->GetFullDisplayString());
		return InDefaultValue;
	}

	return *((const T*)Property->GetRecordedData().GetData());
}

void UVariantManagerBlueprintLibrary::SetValueBool(UPropertyValue* Property, bool InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	int32 SizeInValue = sizeof(bool);
	int32 PropValSize = Property->GetValueSizeInBytes();

	if (SizeInValue == PropValSize && Property->GetPropertyClass()->IsChildOf(FBoolProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a bool as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

bool UVariantManagerBlueprintLibrary::GetValueBool(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, false);
}

void UVariantManagerBlueprintLibrary::SetValueInt(UPropertyValue* Property, int32 InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	FFieldClass* PropertyClass = Property->GetPropertyClass();

	// Technically blueprint-exposed properties can only be int32 or uint8, but this should handle all
	// cases. The two most common are first
	if (PropertyClass->IsChildOf(FIntProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else if (PropertyClass->IsChildOf(FByteProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<uint8>(InValue));
	}
	else if (PropertyClass->IsChildOf(FUInt64Property::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<uint64>(InValue));
	}
	else if (PropertyClass->IsChildOf(FUInt32Property::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<uint32>(InValue));
	}
	else if (PropertyClass->IsChildOf(FUInt16Property::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<uint16>(InValue));
	}

	else if (PropertyClass->IsChildOf(FInt64Property::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<int64>(InValue));
	}
	else if (PropertyClass->IsChildOf(FInt16Property::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<int16>(InValue));
	}
	else if (PropertyClass->IsChildOf(FInt8Property::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<int8>(InValue));
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set an integer as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *PropertyClass->GetName());
	}
}

int32 UVariantManagerBlueprintLibrary::GetValueInt(UPropertyValue* Property)
{
	if (Property == nullptr)
	{
		return 0;
	}

	FFieldClass* PropertyClass = Property->GetPropertyClass();

	// Technically blueprint-exposed properties can only be int32 or uint8, but this should handle all
	// cases. The two most common are first
	if (PropertyClass->IsChildOf(FIntProperty::StaticClass()))
	{
		return GetPropertyValueImpl<int32>(Property, 0);
	}
	else if (PropertyClass->IsChildOf(FByteProperty::StaticClass()))
	{
		return static_cast<int32>(GetPropertyValueImpl<uint8>(Property, 0));
	}
	else if (PropertyClass->IsChildOf(FUInt64Property::StaticClass()))
	{
		return static_cast<int32>(GetPropertyValueImpl<uint64>(Property, 0));
	}
	else if (PropertyClass->IsChildOf(FUInt32Property::StaticClass()))
	{
		return static_cast<int32>(GetPropertyValueImpl<uint32>(Property, 0));
	}
	else if (PropertyClass->IsChildOf(FUInt16Property::StaticClass()))
	{
		return static_cast<int32>(GetPropertyValueImpl<uint16>(Property, 0));
	}
	else if (PropertyClass->IsChildOf(FInt64Property::StaticClass()))
	{
		return static_cast<int32>(GetPropertyValueImpl<int64>(Property, 0));
	}
	else if (PropertyClass->IsChildOf(FInt16Property::StaticClass()))
	{
		return static_cast<int32>(GetPropertyValueImpl<int16>(Property, 0));
	}
	else if (PropertyClass->IsChildOf(FInt8Property::StaticClass()))
	{
		return static_cast<int32>(GetPropertyValueImpl<int8>(Property, 0));
	}

	return 0;
}

void UVariantManagerBlueprintLibrary::SetValueFloat(UPropertyValue* Property, float InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	FFieldClass* PropertyClass = Property->GetPropertyClass();

	if (PropertyClass->IsChildOf(FFloatProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else if (PropertyClass->IsChildOf(FDoubleProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, static_cast<double>(InValue));
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a float as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *PropertyClass->GetName());
	}
}

float UVariantManagerBlueprintLibrary::GetValueFloat(UPropertyValue* Property)
{
	if (Property == nullptr)
	{
		return 0.0f;
	}

	FFieldClass* PropertyClass = Property->GetPropertyClass();

	if (PropertyClass->IsChildOf(FFloatProperty::StaticClass()))
	{
		return GetPropertyValueImpl<float>(Property, 0.0f);
	}
	else if (PropertyClass->IsChildOf(FDoubleProperty::StaticClass()))
	{
		return static_cast<float>(GetPropertyValueImpl<double>(Property, 0.0));
	}

	return 0.0f;
}

void UVariantManagerBlueprintLibrary::SetValueObject(UPropertyValue* Property, UObject* InValue)
{
	if (Property == nullptr || InValue == nullptr)
	{
		return;
	}

	FFieldClass* PropClass = Property->GetPropertyClass();
	if (PropClass->IsChildOf(FObjectProperty::StaticClass()) || PropClass->IsChildOf(FInterfaceProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set an UObject as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

UObject* UVariantManagerBlueprintLibrary::GetValueObject(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, (UObject*)nullptr);
}

void UVariantManagerBlueprintLibrary::SetValueString(UPropertyValue* Property, const FString& InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	if (Property->GetPropertyClass()->IsChildOf(FStrProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else if (Property->GetPropertyClass()->IsChildOf(FTextProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, FText::FromString(InValue));
	}
	else if (Property->GetPropertyClass()->IsChildOf(FNameProperty::StaticClass()))
	{
		SetPropertyValueImpl(Property, FName(*InValue));
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a string as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FString UVariantManagerBlueprintLibrary::GetValueString(UPropertyValue* Property)
{
	if (Property == nullptr)
	{
		return FString();
	}

	if (Property->GetPropertyClass()->IsChildOf(FStrProperty::StaticClass()))
	{
		return GetPropertyValueRefTypeImpl(Property, FString());
	}
	else if (Property->GetPropertyClass()->IsChildOf(FTextProperty::StaticClass()))
	{
		return GetPropertyValueRefTypeImpl(Property, FText()).ToString();
	}
	else if (Property->GetPropertyClass()->IsChildOf(FNameProperty::StaticClass()))
	{
		return GetPropertyValueRefTypeImpl(Property, FName()).ToString();
	}

	return FString();
}

void UVariantManagerBlueprintLibrary::SetValueRotator(UPropertyValue* Property, FRotator InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_Rotator)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a Rotator as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FRotator UVariantManagerBlueprintLibrary::GetValueRotator(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FRotator());
}

void UVariantManagerBlueprintLibrary::SetValueColor(UPropertyValue* Property, FColor InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_Color)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a Color as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FColor UVariantManagerBlueprintLibrary::GetValueColor(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FColor());
}

void UVariantManagerBlueprintLibrary::SetValueLinearColor(UPropertyValue* Property, FLinearColor InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_LinearColor)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a LinearColor as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FLinearColor UVariantManagerBlueprintLibrary::GetValueLinearColor(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FLinearColor());
}

void UVariantManagerBlueprintLibrary::SetValueVector(UPropertyValue* Property, FVector InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_Vector)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a Vector as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FVector UVariantManagerBlueprintLibrary::GetValueVector(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FVector());
}

void UVariantManagerBlueprintLibrary::SetValueQuat(UPropertyValue* Property, FQuat InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_Quat)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a Quat as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FQuat UVariantManagerBlueprintLibrary::GetValueQuat(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FQuat());
}

void UVariantManagerBlueprintLibrary::SetValueVector4(UPropertyValue* Property, FVector4 InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_Vector4)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a Vector4 as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FVector4 UVariantManagerBlueprintLibrary::GetValueVector4(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FVector4());
}

void UVariantManagerBlueprintLibrary::SetValueVector2D(UPropertyValue* Property, FVector2D InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_Vector2D)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set a Vector2D as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FVector2D UVariantManagerBlueprintLibrary::GetValueVector2D(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FVector2D());
}

void UVariantManagerBlueprintLibrary::SetValueIntPoint(UPropertyValue* Property, FIntPoint InValue)
{
	if (Property == nullptr)
	{
		return;
	}

	UScriptStruct* Struct = Property->GetStructPropertyStruct();
	if (Struct && Struct->GetFName() == NAME_IntPoint)
	{
		SetPropertyValueImpl(Property, InValue);
	}
	else
	{
		UE_LOG(LogVariantManager, Error, TEXT("Cannot set an IntPoint as the value of '%s', which is a %s!"), *Property->GetFullDisplayString(), *Property->GetPropertyClass()->GetName());
	}
}

FIntPoint UVariantManagerBlueprintLibrary::GetValueIntPoint(UPropertyValue* Property)
{
	return GetPropertyValueImpl(Property, FIntPoint());
}