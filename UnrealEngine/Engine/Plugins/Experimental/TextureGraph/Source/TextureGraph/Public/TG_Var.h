// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <functional>

#include "TG_Signature.h"

#include "TG_Var.generated.h"

template <typename T>
void TG_Var_SetValueFromString(T& Value, const FString& StrVal)
{
	UE_LOG(LogTemp, Warning, TEXT("Need To Implement TG_Var_SetValueFromString"));
}


template <typename T>
FString TG_Var_LogValue(T& Value)
{
	FString LogMessage = TEXT("Default Var Log : Need to define a type specific TG_Var_LogValue function");
	return LogMessage;
}

USTRUCT()
struct TEXTUREGRAPH_API FTG_Var
{
	GENERATED_BODY()

	friend class UTG_Pin;
	friend class UTG_Expression;

	// Map of per type serializer for the var allowing to copy the value in the Var to a matching UObject's FProperty
	struct VarPropertySerialInfo
	{
		FTG_Var*				Var = nullptr;
		UObject*				Owner = nullptr;
		FTG_Argument			Arg;
		int32					Index = 0;
		bool					CopyVarToProperty = false;

		FORCEINLINE int32		ClampedIndex() const { return Index >= 0 ? Index : 0; }
	};
	//typedef std::function<void(VarPropertySerialInfo&)>		VarPropertySerializer;
	typedef void (*VarPropertySerializer) (VarPropertySerialInfo&)		;
	typedef TMap<FName, VarPropertySerializer>				VarPropertySerializerMap;
	static VarPropertySerializerMap							DefaultPropertySerializers;
	static void RegisterDefaultSerializers();
	static void RegisterVarPropertySerializer(FName CPPTypeName, VarPropertySerializer Serializer);
	static void UnregisterVarPropertySerializer(FName CPPTypeName);

	bool CopyGeneric(UTG_Expression* Owner, const FTG_Argument& Arg, bool CopyVarToProperty);

	// Map of per type serializer for the var to/from an Archive
	struct VarArchiveSerialInfo
	{
		FTG_Var* Var = nullptr;
		FArchive& Ar;
	};
	typedef std::function<void(VarArchiveSerialInfo&)>	VarArchiveSerializer;
	typedef TMap<FName, VarArchiveSerializer>			VarArchiveSerializerMap;
	static VarArchiveSerializerMap						DefaultArchiveSerializers;

	// Serialize the var value
	// Called from the Pin serialize()
	void Serialize(FArchive& Ar, FTG_Id PinId, const FTG_Argument& Argument);


	UPROPERTY(Transient)
	FTG_Id						PinId;



	template <typename T_ValueType>
	static void FGeneric_Struct_Serializer(FTG_Var::VarPropertySerialInfo& Info)
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

public:
	FTG_Var()									{}
	FTG_Var(FTG_Id InPinUuid)					: PinId(InPinUuid) {}
	FTG_Var(const FTG_Var& InVar)				: PinId(InVar.PinId), Concept(InVar.Concept) {}
	FTG_Var& operator = (const FTG_Var& InVar) { PinId = InVar.PinId; Concept = InVar.Concept; return *this; }

	void ShareData(const FTG_Var& InVar)
	{
		Concept = InVar.Concept;
	}

	FTG_Id			GetId() const { return PinId; }

	// Check that the Var is valid meaning that it is correctly allocated in the Graph and reference its Pin Owner
	bool IsValid() const { return PinId != FTG_Id::INVALID; }

	// Check if the Var has No Data allocated
	bool IsEmpty() const { return !Concept; } 

	FString LogValue() const { if (Concept) return Concept->LogValue(); else return TEXT("nullptr"); }

	void SetValueFromStr(FString StrVal) {	if (Concept) Concept->SetValueFromString(StrVal); }
	
	// Manage data Arg provide the type name and rely on the VarAllocator map to create the concrete cpp type
	// If Owner is provided then Var can be initialized from the matching owner's property if found
	void CopyTo(UTG_Expression* Owner, FTG_Argument& Arg);
	void CopyFrom(UTG_Expression* Owner, FTG_Argument& Arg);

	void CopyTo(FTG_Var* InVar)
	{
		if (Concept && InVar)
		{
			InVar->Concept = Concept->Clone();
		}
	}
	void CopyFrom(FTG_Var* InVar);

	// Manage the Var Data with the knowledge of the Type
	template <class T> void ResetAs() const { Concept = MakeShared<FModel<T>>(); }
	template <class T> T& GetAs() const
	{
		if (IsEmpty())
			ResetAs<T>();
		return static_cast<FModel<T>*>(Concept.Get())->Value;
	}
	
	template <class T> T& GetAsWithDefault(T& Default)
	{
		if (IsEmpty())
			return Default;
		return static_cast<FModel<T>*>(Concept.Get())->Value;
	}
	
	template <class T> void SetAs(const T& InValue)
	{
		if (IsEmpty())
			ResetAs<T>();
		static_cast<FModel<T>*>(Concept.Get())->Value = InValue;
	}
	
	template <class T> T& EditAs()
	{
		if (IsEmpty())
			ResetAs<T>();
		return static_cast<FModel<T>*>(Concept.Get())->Value;
	}

	FString LogHead() const;

private:

	struct TEXTUREGRAPH_API FConcept
	{
		virtual ~FConcept() {}

		virtual FString LogValue() = 0;
		virtual void SetValueFromString(const FString& String) = 0;
		virtual TSharedPtr<FConcept> Clone() = 0;
	};
	template <class T>
	struct TEXTUREGRAPH_API FModel : public FConcept
	{
		T Value;
		FString LogValue() override { return TG_Var_LogValue(Value); }
	
		void SetValueFromString(const FString& StrValue) override { TG_Var_SetValueFromString(Value, StrValue); }

		TSharedPtr<FConcept> Clone() override {
			auto Ptr = MakeShared<FModel<T>>();
			Ptr->Value = Value;
			return Ptr;
		}
	};

	mutable TSharedPtr<FConcept> Concept;
};


template <> FString TG_Var_LogValue(bool& Value);
template <> FString TG_Var_LogValue(int& Value);
template <> FString TG_Var_LogValue(float& Value);
template <> FString TG_Var_LogValue(FLinearColor& Value);
template <> FString TG_Var_LogValue(FVector4f& Value);
template <> FString TG_Var_LogValue(FVector2f& Value);
template <> FString TG_Var_LogValue(FName& Value);
template <> FString TG_Var_LogValue(TObjectPtr<UObject>& Value);
template <> FString TG_Var_LogValue(struct FTG_Texture& Value);
template <> FString TG_Var_LogValue(struct FTG_Scalar& Value);
template <> FString TG_Var_LogValue(struct FTG_OutputSettings& Value);
template <> FString TG_Var_LogValue(struct FTG_Variant& Value);


template <> void TG_Var_SetValueFromString(int& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(bool& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(float& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(uint8& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(TObjectPtr<UObject>& Value, const FString& StrVal);

template <> void TG_Var_SetValueFromString(FLinearColor& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(FVector4f& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(FVector2f& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(FName& Value, const FString& StrVal);

//template <> void TG_Var_SetValueFromString(struct FTG_Texture& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(struct FTG_Scalar& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(struct FTG_OutputSettings& Value, const FString& StrVal);
template <> void TG_Var_SetValueFromString(struct FTG_Variant& Value, const FString& StrVal);


