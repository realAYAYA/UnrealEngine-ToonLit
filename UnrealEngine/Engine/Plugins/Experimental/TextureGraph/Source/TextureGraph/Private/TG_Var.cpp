// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_Var.h"

#include <functional>

#include "TG_Texture.h"
#include "TG_OutputSettings.h"
#include "TG_Variant.h"
#include "Model/ModelObject.h"

#include "Expressions/TG_Expression.h"

template <> FString TG_Var_LogValue(uint8& Value)
{
	
	FString LogMessage = FString::FromInt(Value);
	return LogMessage;
}

template <> FString TG_Var_LogValue(bool& Value)
{
	FString LogMessage = (Value ? TEXT("true") : TEXT("false"));
	return LogMessage;
}

template <> FString TG_Var_LogValue(int& Value)
{
	FString LogMessage = FString::FromInt(Value);
	return LogMessage;
}

template <> FString TG_Var_LogValue(float& Value)
{
	FString LogMessage = FString::SanitizeFloat(Value);
	return LogMessage;
}

template <> FString TG_Var_LogValue(FName& Value)
{
	FString LogMessage = Value.ToString();
	return LogMessage;
}

template <> FString TG_Var_LogValue(FLinearColor& Value)
{
	FString LogMessage = Value.ToString();
	return LogMessage;
}
template <> FString TG_Var_LogValue(FVector4f& Value)
{
	//Formating the string into comma seperated numbers
	FString LogMessage = FString::Printf(TEXT("%3.3f,%3.3f,%3.3f,%3.3f"), Value.X, Value.Y, Value.Z, Value.W);
	return LogMessage;
}
template <> FString TG_Var_LogValue(FVector2f& Value)
{
	FString LogMessage = Value.ToString();
	return LogMessage;
}
template <> FString TG_Var_LogValue(TObjectPtr<UObject>& Value)
{
	if (Value.Get())
	{
		auto Name = Value->GetClass()->GetName();
		return FString::Printf(TEXT("%s <0x%0*x>"), *Name, 8, Value.Get());
	}
	else
		return TEXT("nullptr");
}

template <> FString TG_Var_LogValue(FTG_OutputSettings& Value)
{
	FString LogMessage = Value.ToString();
	return LogMessage;
}

template <> void TG_Var_SetValueFromString(int& Value, const FString& StrVal)
{
	Value = FCString::Atoi(*StrVal);
}
template <> void TG_Var_SetValueFromString(bool& Value, const FString& StrVal)
{
	Value = FCString::ToBool(*StrVal);
}

template <> void TG_Var_SetValueFromString(float& Value, const FString& StrVal)
{
	Value = FCString::Atof(*StrVal);
}
template <> void TG_Var_SetValueFromString(uint8& Value, const FString& StrVal)
{
	Value = static_cast<uint8>(FCString::Atoi(*StrVal));
}
template <> void TG_Var_SetValueFromString(FName& Value, const FString& StrVal)
{
	Value = FName(StrVal);
}

template <> void TG_Var_SetValueFromString(FLinearColor& Value, const FString& StrVal)
{
	Value.InitFromString(StrVal);
}
template <> void TG_Var_SetValueFromString(FVector4f& Value, const FString& StrVal)
{
	TArray<FString> StringArray;
	StrVal.ParseIntoArray(StringArray, TEXT(","), true);

	//should get the 4 comma seperated values from string
	check(StringArray.Num() == 4);

	Value.X = FCString::Atof(*StringArray[0]);
	Value.Y = FCString::Atof(*StringArray[1]);
	Value.Z = FCString::Atof(*StringArray[2]);
	Value.W = FCString::Atof(*StringArray[3]);
}
template <> void TG_Var_SetValueFromString(FVector2f& Value, const FString& StrVal)
{
	Value.InitFromString(StrVal);
}
template <> void TG_Var_SetValueFromString(TObjectPtr<UObject>& Value, const FString& StrVal)
{
	FSoftObjectPath objRef(StrVal);
	Value = Cast<UObject>(objRef.TryLoad());
}


template <> void TG_Var_SetValueFromString(FTG_OutputSettings& Value, const FString& StrVal)
{
	Value.InitFromString(StrVal);
}


FString FTG_Var::LogHead() const
{
	return FString::Printf(TEXT("v%s<0x%0*x>"), *GetId().ToString(), 8, Concept.Get());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define VAR_PROPERTY_SERIALIZER(name, function)	{ TEXT(name), &function }
#define VAR_PROPERTY_SERIALIZER_DEF(name)		VAR_PROPERTY_SERIALIZER(#name, VarPropertySerializer_##name)

template <typename T_PropertyType, typename T_ValueType>
void Generic_Simple_Serializer(FTG_Var::VarPropertySerialInfo& Info)
{
	FProperty* Property = Info.Owner->GetClass()->FindPropertyByName(Info.Arg.GetName());

	const T_PropertyType* TProperty = CastField<T_PropertyType>(Property);
	const T_ValueType VarValue = Info.Var->GetAs<T_ValueType>();

	if (Info.CopyVarToProperty)
		TProperty->SetPropertyValue(TProperty->template ContainerPtrToValuePtr<T_ValueType>(Info.Owner, Info.ClampedIndex()), VarValue);
	else
		Info.Var->EditAs<T_ValueType>() = TProperty->GetPropertyValue(TProperty->template ContainerPtrToValuePtr<T_ValueType>(Info.Owner, Info.ClampedIndex()));
}

template <typename T_ValueType>
void Generic_Struct_Serializer(FTG_Var::VarPropertySerialInfo& Info)
{
	FProperty* Property = Info.Owner->GetClass()->FindPropertyByName(Info.Arg.GetName());


	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	T_ValueType* PropertyValue = StructProperty->ContainerPtrToValuePtr<T_ValueType>(Info.Owner, Info.ClampedIndex());

	if (Info.CopyVarToProperty)
	{
		if (!Info.Var->IsEmpty() && Info.Var->IsValid())
			*PropertyValue = Info.Var->GetAs<T_ValueType>();
		else
		{
			T_ValueType* DefaultValue = StructProperty->ContainerPtrToValuePtrForDefaults<T_ValueType>(StructProperty->Struct, Info.Owner, Info.ClampedIndex());
			if (DefaultValue)
				*PropertyValue = *DefaultValue;
		}
	}
	else
	{
		Info.Var->EditAs<T_ValueType>() = *PropertyValue;
	}
}

void VarPropertySerializer_FTG_Texture(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Struct_Serializer<FTG_Texture>(Info);
}

void VarPropertySerializer_FVector4f(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Struct_Serializer<FVector4f>(Info);
}

void VarPropertySerializer_FVector2f(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Struct_Serializer<FVector2f>(Info);
}

void VarPropertySerializer_FLinearColor(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Struct_Serializer<FLinearColor>(Info);
}

void VarPropertySerializer_int32(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Simple_Serializer<FIntProperty, int32>(Info);
}

void VarPropertySerializer_uint32(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Simple_Serializer<FUInt32Property, int32>(Info);
}

void VarPropertySerializer_float(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Simple_Serializer<FFloatProperty, float>(Info);
}

void VarPropertySerializer_bool(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Simple_Serializer<FBoolProperty, bool>(Info);
}

void VarPropertySerializer_FName(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Simple_Serializer<FNameProperty, FName>(Info);
}

void VarPropertySerializer_UObjectPtr(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Struct_Serializer<FTG_Texture>(Info);
}

void VarPropertySerializer_FTG_Variant(FTG_Var::VarPropertySerialInfo& Info)
{
	FProperty* Property = Info.Owner->GetClass()->FindPropertyByName(Info.Arg.GetName());
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	FTG_Variant* PropertyValue = StructProperty->ContainerPtrToValuePtr<FTG_Variant>(Info.Owner, Info.ClampedIndex());

	if (Info.CopyVarToProperty)
	{
		if (!Info.Var->IsEmpty() && Info.Var->IsValid())
		{
			*PropertyValue = Info.Var->GetAs<FTG_Variant>();
		}
	}
	else
	{
		Info.Var->SetAs<FTG_Variant>(*PropertyValue);
	}
}

void VarPropertySerializer_ObjectProperty(FTG_Var::VarPropertySerialInfo& Info)
{
	FProperty* Property = Info.Owner->GetClass()->FindPropertyByName(Info.Arg.GetName());

	FObjectProperty* ObjectPtrProperty = CastField<FObjectProperty>(Property);
	if (ObjectPtrProperty)
	{
		if (Info.CopyVarToProperty)
		{
			TObjectPtr<UObject> ObjectPtr = Info.Var->GetAs<TObjectPtr<UObject>>();
			ObjectPtrProperty->SetObjectPropertyValue_InContainer(Info.Owner, ObjectPtr.Get(), Info.ClampedIndex());

			// If UObject is going through a setter then make sure to feedback the true end value in the var
			if (Property->HasSetter())
			{
				Info.Var->EditAs<TObjectPtr<UObject>>() = ObjectPtrProperty->GetObjectPropertyValue_InContainer(Info.Owner, Info.ClampedIndex());
			}
		}
		else
		{
			Info.Var->EditAs<TObjectPtr<UObject>>() = ObjectPtrProperty->GetObjectPropertyValue_InContainer(Info.Owner, Info.ClampedIndex());
		}

	}


}

void VarPropertySerializer_StructProperty(FTG_Var::VarPropertySerialInfo& Info)
{
	FProperty* Property = Info.Owner->GetClass()->FindPropertyByName(Info.Arg.GetName());

	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	// const UClass* StructClass = StructProperty->Struct->GetClass();
	const FName TypeName = FName(StructProperty->Struct->GetStructCPPName());
	
	const auto WriterIt = FTG_Var::DefaultPropertySerializers.Find(TypeName);
	if (!WriterIt)
	{
		/// TODO: Perhaps think about doing a simple memcpy?
		UE_LOG(LogTextureGraph, Log, TEXT("Fails serialize Var %s - Property %s FPClass %s CPPType %s"),
			*Info.Var->LogHead(),
			*Info.Arg.GetName().ToString(),
			*TypeName.ToString(),
			*Info.Arg.CPPTypeName.ToString());
		return;
	}

	(*WriterIt)(Info);
}


void VarPropertySerializer_ByteProperty(FTG_Var::VarPropertySerialInfo& Info)
{
	FProperty* Property = Info.Owner->GetClass()->FindPropertyByName(Info.Arg.GetName());

	const FByteProperty* ByteProperty = CastField<FByteProperty>(Property);

	uint8 PropValue = ByteProperty->GetPropertyValue_InContainer(Info.Owner);
	
	const uint8 VarValue = Info.Var->GetAs<uint8>();

	if (Info.CopyVarToProperty)
	{
		ByteProperty->SetPropertyValue_InContainer(Info.Owner, VarValue, 0);
	}
	else
	{
		Info.Var->EditAs<uint8>() = PropValue;
	}
}

void VarPropertySerializer_EnumProperty(FTG_Var::VarPropertySerialInfo& Info)
{
	FProperty* Property = Info.Owner->GetClass()->FindPropertyByName(Info.Arg.GetName());

	const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);

	uint32 PropValue = 0;
	EnumProperty->GetValue_InContainer(Info.Owner, &PropValue);
	auto enumVal = EnumProperty->GetEnum();

	const uint8 VarValue = Info.Var->GetAs<uint8>();

	if (Info.CopyVarToProperty)
	{
		EnumProperty->SetValue_InContainer(Info.Owner, &VarValue);
	}
	else
	{
		Info.Var->EditAs<uint8>() = PropValue;
	}
}

void VarPropertySerializer_FTG_OutputSettings(FTG_Var::VarPropertySerialInfo& Info)
{
	Generic_Struct_Serializer<FTG_OutputSettings>(Info);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FTG_Var::VarPropertySerializerMap FTG_Var::DefaultPropertySerializers
(
	{
		VAR_PROPERTY_SERIALIZER_DEF(StructProperty),
		VAR_PROPERTY_SERIALIZER_DEF(ObjectProperty),
		VAR_PROPERTY_SERIALIZER_DEF(ByteProperty),
		VAR_PROPERTY_SERIALIZER_DEF(EnumProperty),
		VAR_PROPERTY_SERIALIZER_DEF(bool),
		VAR_PROPERTY_SERIALIZER_DEF(int32),
		VAR_PROPERTY_SERIALIZER_DEF(uint32),
		VAR_PROPERTY_SERIALIZER_DEF(float),
		VAR_PROPERTY_SERIALIZER_DEF(FName),
		VAR_PROPERTY_SERIALIZER_DEF(FTG_Texture),
		VAR_PROPERTY_SERIALIZER_DEF(FVector4f),
		VAR_PROPERTY_SERIALIZER_DEF(FVector2f),
		VAR_PROPERTY_SERIALIZER_DEF(FLinearColor),
		VAR_PROPERTY_SERIALIZER_DEF(FTG_OutputSettings),
		VAR_PROPERTY_SERIALIZER_DEF(FTG_Variant),
	}
);


void FTG_Var::RegisterDefaultSerializers()
{
}

void FTG_Var::RegisterVarPropertySerializer(FName CPPTypeName, FTG_Var::VarPropertySerializer Serializer)
{
	DefaultPropertySerializers.Add(CPPTypeName, Serializer);
}
void FTG_Var::UnregisterVarPropertySerializer(FName CPPTypeName)
{
	DefaultPropertySerializers.Remove(CPPTypeName);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FTG_Var::CopyGeneric(UTG_Expression* Owner, const FTG_Argument& Arg, bool CopyVarToProperty)
{
	FProperty* Property = Owner->GetClass()->FindPropertyByName(Arg.GetName());
	if (Property)
	{
		const FFieldClass* PropertyClass = Property->GetClass();
		check(PropertyClass);
		FName PropertyClassName = PropertyClass->GetFName();
		// Filter first for a serializer  against the FPropertyClass name:
		auto SerializerIt = FTG_Var::DefaultPropertySerializers.Find(PropertyClassName);

		// If not found one:
		if (!SerializerIt)
		{
			// Let's try with the simpler Property's cpp name (same as the argument type)
			FName ArgTypeName = FName(Property->GetCPPType());
			SerializerIt = FTG_Var::DefaultPropertySerializers.Find(ArgTypeName);

			if (!SerializerIt)
			{
				UE_LOG(LogTextureGraph, Log, TEXT("Fails serialize Var %s - Property %s FPClass %s CPPType %s"),
					*LogHead(),
					*Arg.GetName().ToString(),
					*PropertyClassName.ToString(),
					*ArgTypeName.ToString());
				return false;
			}
		}

		//const FTG_Var::VarPropertySerializer Serializer = *SerializerIt;

		VarPropertySerialInfo Info = {
			this,
			Owner,
			Arg,
			-1,
			CopyVarToProperty
		};

		(*SerializerIt)(Info);

		return true;
	}

	// No Property cannot copy with the FProperty infrastructure
	return false;
}

void FTG_Var::CopyTo(UTG_Expression* Owner, FTG_Argument& Arg)
{
	Owner->CopyVarToExpressionArgument(Arg, this);
}

void FTG_Var::CopyFrom(UTG_Expression* Owner, FTG_Argument& Arg)
{
	Owner->CopyVarFromExpressionArgument(Arg, this);
}


void FTG_Var::CopyFrom(FTG_Var* InVar)
{
	if (InVar && InVar->Concept)
	{
		InVar->CopyTo(this);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define VAR_ARCHIVE_SERIALIZER(name, function)	{ TEXT(name), std::bind(&function, std::placeholders::_1) }
#define VAR_ARCHIVE_SERIALIZER_DEF(name)		VAR_ARCHIVE_SERIALIZER(#name, VarArchiveSerializer_##name)

template <typename T_ValueType>
void Generic_Simple_ArSerializer(FTG_Var::VarArchiveSerialInfo& Info)
{
	if (Info.Var->IsEmpty())
	{
		Info.Var->ResetAs<T_ValueType>();
	}
	if (Info.Ar.IsSaving())
		UE_LOG(LogTextureGraph, Log, TEXT("        Save Var %s: %s"), *Info.Var->GetId().ToString(), *Info.Var->LogValue());
	T_ValueType& Value = Info.Var->EditAs<T_ValueType>();
	Info.Ar << Value;
	if (Info.Ar.IsLoading())
		UE_LOG(LogTextureGraph, Log, TEXT("        Loaded Var %s: %s"), *Info.Var->GetId().ToString(), *Info.Var->LogValue());

}

void VarArchiveSerializer_FTG_Texture(FTG_Var::VarArchiveSerialInfo& Info)
{
	//Generic_Simple_ArSerializer<FTG_Texture>(Info);
}

void VarArchiveSerializer_FVector4f(FTG_Var::VarArchiveSerialInfo& Info)
{
	Generic_Simple_ArSerializer<FVector4f>(Info);
}
void VarArchiveSerializer_FVector2f(FTG_Var::VarArchiveSerialInfo& Info)
{
	Generic_Simple_ArSerializer<FVector2f>(Info);
}
void VarArchiveSerializer_FLinearColor(FTG_Var::VarArchiveSerialInfo& Info)
{
	Generic_Simple_ArSerializer<FLinearColor>(Info);
}

void VarArchiveSerializer_int32(FTG_Var::VarArchiveSerialInfo& Info)
{
	Generic_Simple_ArSerializer<int32>(Info);
}

void VarArchiveSerializer_uint32(FTG_Var::VarArchiveSerialInfo& Info)
{
	Generic_Simple_ArSerializer<int32>(Info);
}

void VarArchiveSerializer_float(FTG_Var::VarArchiveSerialInfo& Info)
{
	Generic_Simple_ArSerializer<float>(Info);
}

void VarArchiveSerializer_bool(FTG_Var::VarArchiveSerialInfo& Info)
{
	Generic_Simple_ArSerializer<bool>(Info);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
FTG_Var::VarArchiveSerializerMap FTG_Var::DefaultArchiveSerializers
(
	{
		VAR_ARCHIVE_SERIALIZER_DEF(bool),
		VAR_ARCHIVE_SERIALIZER_DEF(int32),
		VAR_ARCHIVE_SERIALIZER_DEF(uint32),
		VAR_ARCHIVE_SERIALIZER_DEF(float),
		VAR_ARCHIVE_SERIALIZER_DEF(FTG_Texture),
		VAR_ARCHIVE_SERIALIZER_DEF(FVector4f),
		VAR_ARCHIVE_SERIALIZER_DEF(FVector2f),
		VAR_ARCHIVE_SERIALIZER_DEF(FLinearColor),
	}
); 

void FTG_Var::Serialize(FArchive& Ar, FTG_Id InPinId, const FTG_Argument& InArgument)
{
	// noop for private field
	if (InArgument.IsPrivate())
		return;

	// Init Var transient fields
	if (!PinId.IsValid())
		PinId = InPinId;
	assert(PinId == InPinId);

	if (InArgument.IsPersistentSelfVar())
	{
		auto SerializerIt = FTG_Var::DefaultArchiveSerializers.Find(InArgument.GetCPPTypeName());
		if (SerializerIt)
		{
			VarArchiveSerialInfo Info{
				.Var = this,
				.Ar = Ar };
			(*SerializerIt)(Info);
		}
		else
		{
			UE_LOG(LogTextureGraph, Log, TEXT("serialize Var %s: NOT FOUND for %s"), *LogHead(), *InArgument.GetCPPTypeName().ToString());
		}
	}
}


