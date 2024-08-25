// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/Optional.h"

// 
// Encapsulates the memory layout logic for an optional without implementing the full FProperty API.
//
struct COREUOBJECT_API FOptionalPropertyLayout
{
	explicit FOptionalPropertyLayout(FProperty* InValueProperty)
		: ValueProperty(InValueProperty)
	{
		check(ValueProperty);
	}

	FProperty* GetValueProperty() const 
	{
		checkf(ValueProperty, TEXT("Expected ValueProperty to be initialized"));
		return ValueProperty; 
	}

	FORCEINLINE bool IsSet(const void* Data) const
	{
		checkSlow(Data);
		return IsValueNonNullablePointer()
			? *reinterpret_cast<void*const*>(Data) != nullptr
			: *GetIsSetPointer(Data);
	}
	FORCEINLINE void* MarkSetAndGetInitializedValuePointerToReplace(void* Data) const
	{
		checkSlow(Data);
		if (IsValueNonNullablePointer())
		{
			ValueProperty->InitializeValue(Data);
		}
		else
		{
			bool* IsSetPointer = GetIsSetPointer(Data);
			if (!*IsSetPointer)
			{
				ValueProperty->InitializeValue(Data);
				*IsSetPointer = true;
			}
		}
		return Data;
	}
	FORCEINLINE void MarkUnset(void* Data) const
	{
		checkSlow(Data);
		if (IsValueNonNullablePointer())
		{
			ValueProperty->ClearValue(Data);
		}
		else
		{
			bool* IsSetPointer = GetIsSetPointer(Data);
			if (*IsSetPointer)
			{
				ValueProperty->DestroyValue(Data);
				*IsSetPointer = false;
			}
		}
	}

	// For reading the value of a set optional.
	// Must be called on a non-null pointer to a set optional.
	FORCEINLINE const void* GetValuePointerForRead(const void* Data) const 
	{
		checkSlow(Data && IsSet(Data));
		return Data; 
	}
	
	// For replacing the value of a set optional.
	// Must be called on a non-null pointer to a set optional.
	FORCEINLINE void* GetValuePointerForReplace(void* Data) const
	{
		checkSlow(Data && IsSet(Data));
		return Data;
	}

	// For reading the value of a set optional.
	// Must be called on a non-null pointer to an optional.
	// If called on an unset optional, will return null.
	FORCEINLINE const void* GetValuePointerForReadIfSet(const void* Data) const
	{
		checkSlow(Data);
		return IsSet(Data) ? Data : nullptr;
	}
	
	// For replacing the value of a set optional.
	// Must be called on a non-null pointer to an optional.
	// If called on an unset optional, will return null.
	FORCEINLINE void* GetValuePointerForReplaceIfSet(void* Data) const
	{
		checkSlow(Data);
		return IsSet(Data) ? Data : nullptr;
	}
	
	// For calling from polymorphic code that doesn't know whether it needs the value pointer for
	// read or replace, or whether it has a const pointer or not.
	// Must be called on a non-null pointer to a set optional.
	FORCEINLINE const void* GetValuePointerForReadOrReplace(const void* Data) const
	{
		checkSlow(Data && IsSet(Data));
		return Data;
	}
	FORCEINLINE void* GetValuePointerForReadOrReplace(void* Data) const
	{
		checkSlow(Data && IsSet(Data));
		return Data;
	}
	
	// For calling from polymorphic code that doesn't know whether it needs the value pointer for
	// read or replace, or whether it has a const pointer or not.
	// Must be called on a non-null pointer to an optional.
	// If called on an unset optional, will return null.
	FORCEINLINE const void* GetValuePointerForReadOrReplaceIfSet(const void* Data) const
	{
		checkSlow(Data);
		return IsSet(Data) ? Data : nullptr;
	}
	FORCEINLINE void* GetValuePointerForReadOrReplaceIfSet(void* Data) const
	{
		checkSlow(Data);
		return IsSet(Data) ? Data : nullptr;
	}
	
protected:

	FOptionalPropertyLayout() : ValueProperty(nullptr) {}

	// Variables
	FProperty* ValueProperty; // The type of the value

	FORCEINLINE int32 CalcIsSetOffset() const
	{
		check(!IsValueNonNullablePointer());
		checkfSlow(
			ValueProperty->GetSize() == Align(ValueProperty->GetSize(), ValueProperty->GetMinAlignment()),
			TEXT("Expected optional value property to have aligned size, but got misaligned size %i for %s that has minimum alignment %i"),
			ValueProperty->GetSize(),
			*ValueProperty->GetFullName(),
			ValueProperty->GetMinAlignment());
		return ValueProperty->GetSize();
	}
	FORCEINLINE int32 CalcSize() const
	{
		if (IsValueNonNullablePointer())
		{
			return ValueProperty->GetSize();
		}
		else
		{
			return Align(CalcIsSetOffset() + 1, ValueProperty->GetMinAlignment());
		}
	}

	FORCEINLINE bool IsValueNonNullablePointer() const
	{
		return (ValueProperty->GetPropertyFlags() & CPF_NonNullable) != 0;
	}

	FORCEINLINE bool* GetIsSetPointer(void* Data) const
	{
		return reinterpret_cast<bool*>(reinterpret_cast<uint8*>(Data) + CalcIsSetOffset());
	}
	FORCEINLINE const bool* GetIsSetPointer(const void* Data) const
	{
		return reinterpret_cast<const bool*>(reinterpret_cast<const uint8*>(Data) + CalcIsSetOffset());
	}
};

//
// A property corresponding to UE's optional type, TOptional<T>.
// NOTE: this property is not yet handled by all UE subsystems that produce or consume properties.
//
class COREUOBJECT_API FOptionalProperty : public FProperty, public FOptionalPropertyLayout
{
	DECLARE_FIELD(FOptionalProperty, FProperty, CASTCLASS_FOptionalProperty)

public:

	FOptionalProperty(FFieldVariant InOwner, const FName& InName, EObjectFlags InObjectFlags);
	FOptionalProperty(FFieldVariant InOwner, const UECodeGen_Private::FGenericPropertyParams& Prop);
	virtual ~FOptionalProperty();

	// Sets the optional property's value property.
	void SetValueProperty(FProperty* InValueProperty);

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	// End of UObject interface

	// Field interface
	virtual void PostDuplicate(const FField& InField) override;
	virtual FField* GetInnerFieldByName(const FName& InName) override;
	virtual void GetInnerFields(TArray<FField*>& OutFields) override;
	virtual void AddCppProperty(FProperty* Property) override;
	// End of Field interface

	// UHT interface
	virtual FString GetCPPType(FString* ExtendedTypeText = NULL, uint32 CPPExportFlags = 0) const override;
	virtual FString GetCPPMacroType(FString& ExtendedTypeText) const override;
	UE_DEPRECATED(5.4, "UnrealHeaderTool only API.  No replacement available.")
	virtual FString GetCPPTypeForwardDeclaration() const override;
	// End of UHT interface

	// FProperty interface
	virtual void LinkInternal(FArchive& Ar) override;
	virtual bool Identical(const void* A, const void* B, uint32 PortFlags) const override;
	virtual void SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const override;
	virtual bool NetSerializeItem(FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8>* MetaData = NULL) const override;
	virtual bool SupportsNetSharedSerialization() const override;
	virtual void ExportText_Internal(FString& ValueStr, const void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const override;
	virtual const TCHAR* ImportText_Internal(const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* Parent, int32 PortFlags, FOutputDevice* ErrorText) const override;
	virtual void CopyValuesInternal(void* Dest, void const* Src, int32 Count) const override;
	virtual void ClearValueInternal(void* Data) const override;
	virtual void InitializeValueInternal(void* Data) const override;
	virtual void DestroyValueInternal(void* Data) const override;
	virtual void InstanceSubobjects(void* Data, void const* DefaultData, UObject* Owner, struct FObjectInstancingGraph* InstanceGraph) override;
	virtual int32 GetMinAlignment() const override;
	virtual bool ContainsObjectReference(TArray<const FStructProperty*>& EncounteredStructProps, EPropertyObjectReferenceType InReferenceType = EPropertyObjectReferenceType::Strong) const override;
	virtual void EmitReferenceInfo(UE::GC::FSchemaBuilder& Schema, int32 BaseOffset, TArray<const FStructProperty*>& EncounteredStructProps, UE::GC::FPropertyStack& DebugPath) override;
	virtual EConvertFromTypeResult ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, const uint8* Defaults) override;
	virtual uint32 GetValueTypeHashInternal(const void* Src) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual bool UseBinaryOrNativeSerialization(const FArchive& Ar) const override;
	virtual bool LoadTypeName(UE::FPropertyTypeName Type, const FPropertyTag* Tag = nullptr) override;
	virtual void SaveTypeName(UE::FPropertyTypeNameBuilder& Type) const override;
	virtual bool CanSerializeFromTypeName(UE::FPropertyTypeName Type) const override;
	// End of FProperty interface
};
