// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMMemoryStorageStruct.h"
#include "RigVMTypeUtils.h"
#include "RigVMModule.h"
#include "Misc/Guid.h"
#include "Hash/Blake3.h"

namespace UE::RigVM::RigVMCore::Private
{
	static const FString EmptyBraces = TEXT("()");
	static constexpr TCHAR BraceFormat[] = TEXT("(%s)");
}


FRigVMMemoryStorageStruct::FRigVMMemoryStorageStruct()
	: FInstancedPropertyBag()
	, MemoryType(ERigVMMemoryType::Invalid)
{
}

FRigVMMemoryStorageStruct::FRigVMMemoryStorageStruct(ERigVMMemoryType InMemoryType)
	: FInstancedPropertyBag()
	, MemoryType(InMemoryType)
{
}

FRigVMMemoryStorageStruct::FRigVMMemoryStorageStruct(ERigVMMemoryType InMemoryType, const TArray<FRigVMPropertyDescription>& InPropertyDescriptions, const TArray<FRigVMPropertyPathDescription>& InPropertyPaths)
	: FInstancedPropertyBag()
	, MemoryType(InMemoryType)
{
	AddProperties(InPropertyDescriptions, InPropertyPaths);
}

FRigVMMemoryStorageStruct::~FRigVMMemoryStorageStruct()
{
}

bool FRigVMMemoryStorageStruct::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << MemoryType;
	Ar << PropertyPathDescriptions;

	if (Ar.IsLoading())
	{
		// rebuild property list and property path list
		RefreshLinkedProperties();
		RefreshPropertyPaths();

		CachedMemoryHash = 0;
	}

	return true;
}

void FRigVMMemoryStorageStruct::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddStructReferencedObjects(Collector);
}

void FRigVMMemoryStorageStruct::AddProperties(const TArray<FRigVMPropertyDescription>& InPropertyDescriptions, const TArray<FRigVMPropertyPathDescription>& InPropertyPathDescriptions)
{
	TArray<FPropertyBagPropertyDesc> BagDescriptors;
	BagDescriptors.Reserve(InPropertyDescriptions.Num());

	// Generate PropertyBag descriptors
	for (const FRigVMPropertyDescription& RigVMDescriptor : InPropertyDescriptions)
	{
		if (!ensure(BagDescriptors.FindByPredicate([&RigVMDescriptor](const FPropertyBagPropertyDesc& Desc) { return Desc.Name == RigVMDescriptor.Name; }) == nullptr))
		{
			UE_LOG(LogRigVM, Error, TEXT("Detected duplicated name in the generated properties : [%s]"), *RigVMDescriptor.Name.ToString());
		}

		const FPropertyBagPropertyDesc& PropertyBagDescriptor = GeneratePropertyBagDescriptor(RigVMDescriptor);
		if (PropertyBagDescriptor.ValueType != EPropertyBagPropertyType::None)
		{
			BagDescriptors.Add(PropertyBagDescriptor);
		}
	}

	// Generate the struct with the passed types
	Super::AddProperties(BagDescriptors);

	PropertyPathDescriptions = InPropertyPathDescriptions;

	RefreshLinkedProperties();
	RefreshPropertyPaths();

	// And set the passed text DefaultValues
	SetDefaultValues(InPropertyDescriptions);

	CachedMemoryHash = 0;
}

int32 FRigVMMemoryStorageStruct::GetPropertyIndex(const FProperty* InProperty) const
{
	if (InProperty != nullptr)
	{
		const TArray<const FProperty*>& Properties = GetProperties();

		return Properties.IndexOfByKey(InProperty);
	}

	return INDEX_NONE;
}

int32 FRigVMMemoryStorageStruct::GetPropertyIndexByName(const FName& InName) const
{
	const FProperty* Property = FindPropertyByName(InName);
	return GetPropertyIndex(Property);
}

FProperty* FRigVMMemoryStorageStruct::FindPropertyByName(const FName& InName) const
{
	const FName SanitizedName = FRigVMPropertyDescription::SanitizeName(InName);
	return Value.GetScriptStruct() != nullptr ? Value.GetScriptStruct()->FindPropertyByName(SanitizedName) : nullptr;
}

FRigVMOperand FRigVMMemoryStorageStruct::GetOperand(int32 InPropertyIndex, int32 InPropertyPathIndex) const
{
	if (GetProperties().IsValidIndex(InPropertyIndex))
	{
		if (InPropertyPathIndex != INDEX_NONE)
		{
			check(GetPropertyPaths().IsValidIndex(InPropertyPathIndex));
			return FRigVMOperand(GetMemoryType(), InPropertyIndex, InPropertyPathIndex);
		}
		return FRigVMOperand(GetMemoryType(), InPropertyIndex);
	}
	return FRigVMOperand();
}

FRigVMOperand FRigVMMemoryStorageStruct::GetOperandByName(const FName& InName, int32 InPropertyPathIndex) const
{
	const int32 PropertyIndex = GetPropertyIndexByName(InName);
	return GetOperand(PropertyIndex, InPropertyPathIndex);
}

void* FRigVMMemoryStorageStruct::GetContainerPtr() const
{
	return (void*)GetValue().GetMemory();
}

uint32 FRigVMMemoryStorageStruct::GetMemoryHash() const
{
	if (CachedMemoryHash != 0)
	{
		return CachedMemoryHash;
	}

	for (const FProperty* Property : LinkedProperties)
	{
		CachedMemoryHash = HashCombine(CachedMemoryHash, GetTypeHash(Property->GetFName().ToString()));
		CachedMemoryHash = HashCombine(CachedMemoryHash, GetTypeHash(Property->GetCPPType()));
	}

	// for literals we also hash the content / defaults for each property
	if (GetMemoryType() == ERigVMMemoryType::Literal)
	{
		for (const FProperty* Property : LinkedProperties)
		{
			FString DefaultValue;
			Property->ExportTextItem_InContainer(DefaultValue, GetContainerPtr(), nullptr, nullptr, PPF_None, nullptr);
			CachedMemoryHash = HashCombine(CachedMemoryHash, GetTypeHash(DefaultValue));
		}
	}

	return CachedMemoryHash;
}

//****************************************************************************

const TArray<const FProperty*> FRigVMMemoryStorageStruct::EmptyProperties;
const TArray<FRigVMPropertyPath> FRigVMMemoryStorageStruct::EmptyPropertyPaths;

FString FRigVMMemoryStorageStruct::GetDataAsString(int32 InPropertyIndex, int32 PortFlags)
{
	check(IsValidIndex(InPropertyIndex));
	const uint8* Data = GetData<uint8>(InPropertyIndex);

	FString RetValue;
	GetProperty(InPropertyIndex)->ExportTextItem_Direct(RetValue, Data, nullptr, nullptr, PortFlags);
	return RetValue;
}

FString FRigVMMemoryStorageStruct::GetDataAsString(const FRigVMOperand& InOperand, int32 PortFlags)
{
	const int32 PropertyIndex = InOperand.GetRegisterIndex();
	check(IsValidIndex(PropertyIndex));
	return GetDataAsString(PropertyIndex, PortFlags);
}

FString FRigVMMemoryStorageStruct::GetDataAsStringSafe(int32 InPropertyIndex, int32 PortFlags)
{
	if (!IsValidIndex(InPropertyIndex))
	{
		return FString();
	}
	return GetDataAsString(InPropertyIndex, PortFlags);
}

FString FRigVMMemoryStorageStruct::GetDataAsStringSafe(const FRigVMOperand& InOperand, int32 PortFlags)
{
	const int32 PropertyIndex = InOperand.GetRegisterIndex();
	return GetDataAsStringSafe(PropertyIndex, PortFlags);
}

bool FRigVMMemoryStorageStruct::SetDataFromString(int32 InPropertyIndex, const FString& InValue)
{
	check(IsValidIndex(InPropertyIndex));
	uint8* Data = GetData<uint8>(InPropertyIndex);

	const FProperty* Property = GetProperty(InPropertyIndex);

	FRigVMMemoryStorageImportErrorContext ErrorPipe(false);
	Property->ImportText_Direct(*InValue, Data, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe);

	if (ErrorPipe.NumErrors > 0)
	{
		// check if the value was provided as a single element
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			const FString ValueWithBraces = FString::Printf(UE::RigVM::RigVMCore::Private::BraceFormat, *InValue);

			ErrorPipe = FRigVMMemoryStorageImportErrorContext(false);
			Property->ImportText_Direct(*ValueWithBraces, Data, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe);
		}
	}

	return ErrorPipe.NumErrors == 0;
}

//****************************************************************************

FRigVMMemoryHandle FRigVMMemoryStorageStruct::GetHandle(int32 InPropertyIndex, const FRigVMPropertyPath* InPropertyPath)
{
	check(IsValidIndex(InPropertyIndex));

	const FProperty* Property = GetProperty(InPropertyIndex);
	uint8* Data = GetData<uint8>(InPropertyIndex);

	return FRigVMMemoryHandle(Data, Property, InPropertyPath);
}

//****************************************************************************

bool FRigVMMemoryStorageStruct::CopyProperty(
	const FProperty* InTargetProperty,
	uint8* InTargetPtr,
	const FProperty* InSourceProperty,
	const uint8* InSourcePtr)
{
	check(InTargetProperty != nullptr);
	check(InSourceProperty != nullptr);
	check(InTargetPtr != nullptr);
	check(InSourcePtr != nullptr);

	// This block below is there to support Large World Coordinates (LWC).
	// We allow to link float and double pins (single and arrays) so we need
	// to support copying values between those as well.
	if (!InTargetProperty->SameType(InSourceProperty))
	{
		if (const FFloatProperty* TargetFloatProperty = CastField<FFloatProperty>(InTargetProperty))
		{
			if (const FDoubleProperty* SourceDoubleProperty = CastField<FDoubleProperty>(InSourceProperty))
			{
				if (TargetFloatProperty->ArrayDim == SourceDoubleProperty->ArrayDim)
				{
					float* TargetFloats = (float*)InTargetPtr;
					double* SourceDoubles = (double*)InSourcePtr;
					for (int32 Index = 0; Index < TargetFloatProperty->ArrayDim; Index++)
					{
						TargetFloats[Index] = (float)SourceDoubles[Index];
					}
					return true;
				}
			}
		}
		else if (const FDoubleProperty* TargetDoubleProperty = CastField<FDoubleProperty>(InTargetProperty))
		{
			if (const FFloatProperty* SourceFloatProperty = CastField<FFloatProperty>(InSourceProperty))
			{
				if (TargetDoubleProperty->ArrayDim == SourceFloatProperty->ArrayDim)
				{
					double* TargetDoubles = (double*)InTargetPtr;
					float* SourceFloats = (float*)InSourcePtr;
					for (int32 Index = 0; Index < TargetDoubleProperty->ArrayDim; Index++)
					{
						TargetDoubles[Index] = (double)SourceFloats[Index];
					}
					return true;
				}
			}
		}
		else if (const FByteProperty* TargetByteProperty = CastField<FByteProperty>(InTargetProperty))
		{
			if (const FEnumProperty* SourceEnumProperty = CastField<FEnumProperty>(InSourceProperty))
			{
				if (TargetByteProperty->Enum == SourceEnumProperty->GetEnum())
				{
					InTargetProperty->CopyCompleteValue(InTargetPtr, InSourcePtr);
					return true;
				}
			}
		}
		else if (const FEnumProperty* TargetEnumProperty = CastField<FEnumProperty>(InTargetProperty))
		{
			if (const FByteProperty* SourceByteProperty = CastField<FByteProperty>(InSourceProperty))
			{
				if (TargetEnumProperty->GetEnum() == SourceByteProperty->Enum)
				{
					InTargetProperty->CopyCompleteValue(InTargetPtr, InSourcePtr);
					return true;
				}
			}
		}
		else if (const FArrayProperty* TargetArrayProperty = CastField<FArrayProperty>(InTargetProperty))
		{
			if (const FArrayProperty* SourceArrayProperty = CastField<FArrayProperty>(InSourceProperty))
			{
				FScriptArrayHelper TargetArray(TargetArrayProperty, InTargetPtr);
				FScriptArrayHelper SourceArray(SourceArrayProperty, InSourcePtr);

				if (TargetArrayProperty->Inner->IsA<FFloatProperty>())
				{
					if (SourceArrayProperty->Inner->IsA<FDoubleProperty>())
					{
						TargetArray.Resize(SourceArray.Num());
						for (int32 Index = 0; Index < TargetArray.Num(); Index++)
						{
							float* TargetFloat = (float*)TargetArray.GetRawPtr(Index);
							const double* SourceDouble = (const double*)SourceArray.GetRawPtr(Index);
							*TargetFloat = (float)*SourceDouble;
						}
						return true;
					}
				}
				else if (TargetArrayProperty->Inner->IsA<FDoubleProperty>())
				{
					if (SourceArrayProperty->Inner->IsA<FFloatProperty>())
					{
						TargetArray.Resize(SourceArray.Num());
						for (int32 Index = 0; Index < TargetArray.Num(); Index++)
						{
							double* TargetDouble = (double*)TargetArray.GetRawPtr(Index);
							const float* SourceFloat = (const float*)SourceArray.GetRawPtr(Index);
							*TargetDouble = (double)*SourceFloat;
						}
						return true;
					}
				}
				else if (FByteProperty* TargetArrayInnerByteProperty = CastField<FByteProperty>(TargetArrayProperty->Inner))
				{
					if (FEnumProperty* SourceArrayInnerEnumProperty = CastField<FEnumProperty>(SourceArrayProperty->Inner))
					{
						if (TargetArrayInnerByteProperty->Enum == SourceArrayInnerEnumProperty->GetEnum())
						{
							InTargetProperty->CopyCompleteValue(InTargetPtr, InSourcePtr);
							return true;
						}
					}
				}
				else if (FEnumProperty* TargetArrayInnerEnumProperty = CastField<FEnumProperty>(TargetArrayProperty->Inner))
				{
					if (FByteProperty* SourceArrayInnerByteProperty = CastField<FByteProperty>(SourceArrayProperty->Inner))
					{
						if (TargetArrayInnerEnumProperty->GetEnum() == SourceArrayInnerByteProperty->Enum)
						{
							InTargetProperty->CopyCompleteValue(InTargetPtr, InSourcePtr);
							return true;
						}
					}
				}
			}
		}

		// if we reach this we failed since we are trying to copy
		// between two properties which are not compatible
		if (!InTargetProperty->SameType(InSourceProperty))
		{
			// Only log the issue once, rather than spam.
			static TSet<FString> ReportedErrors;

			FString TargetType, SourceType, TargetExtendedType, SourceExtendedType;
			TargetType = InTargetProperty->GetCPPType(&TargetExtendedType);
			SourceType = InSourceProperty->GetCPPType(&SourceExtendedType);
			TargetType += TargetExtendedType;
			SourceType += SourceExtendedType;

			UPackage* Package = InTargetProperty->GetOutermost();

			FString Message = FString::Printf(TEXT("Failed to copy %s (%s) to %s (%s) in package %s"),
				*InSourceProperty->GetName(),
				*SourceType,
				*InTargetProperty->GetName(),
				*TargetType,
				Package ? *Package->GetName() : TEXT("<Unknown Package>"));

			if (!ReportedErrors.Contains(Message))
			{
				UE_LOG(LogRigVM, Error, TEXT("%s"), *Message);
				ReportedErrors.Add(Message);
			}

			return false;
		}
	}

	// rely on the core to copy the property contents
	InTargetProperty->CopyCompleteValue(InTargetPtr, InSourcePtr);
	return true;
}

bool FRigVMMemoryStorageStruct::CopyProperty(
	const FProperty* InTargetProperty,
	uint8* InTargetPtr,
	const FRigVMPropertyPath& InTargetPropertyPath,
	const FProperty* InSourceProperty,
	const uint8* InSourcePtr,
	const FRigVMPropertyPath& InSourcePropertyPath)
{
	check(InTargetProperty != nullptr);
	check(InSourceProperty != nullptr);
	check(InTargetPtr != nullptr);
	check(InSourcePtr != nullptr);

	auto TraversePropertyPath = [](const FProperty*& Property, uint8*& MemoryPtr, const FRigVMPropertyPath& PropertyPath)
	{
		if (PropertyPath.IsEmpty())
		{
			return;
		}

		MemoryPtr = PropertyPath.GetData<uint8>(MemoryPtr, Property);
		Property = PropertyPath.GetTailProperty();
	};

	uint8* SourcePtr = (uint8*)InSourcePtr;
	TraversePropertyPath(InTargetProperty, InTargetPtr, InTargetPropertyPath);
	TraversePropertyPath(InSourceProperty, SourcePtr, InSourcePropertyPath);

	if (InTargetPtr == nullptr)
	{
		check(!InTargetPropertyPath.IsEmpty());
		return false;
	}
	if (InSourcePtr == nullptr)
	{
		check(!InSourcePropertyPath.IsEmpty());
		return false;
	}

	return CopyProperty(InTargetProperty, InTargetPtr, InSourceProperty, SourcePtr);
}

bool FRigVMMemoryStorageStruct::CopyProperty(
	FRigVMMemoryStorageStruct* InTargetStorage,
	int32 InTargetPropertyIndex,
	const FRigVMPropertyPath& InTargetPropertyPath,
	FRigVMMemoryStorageStruct* InSourceStorage,
	int32 InSourcePropertyIndex,
	const FRigVMPropertyPath& InSourcePropertyPath)
{
	check(InTargetStorage != nullptr);
	check(InSourceStorage != nullptr);

	const FProperty* TargetProperty = InTargetStorage->GetProperty(InTargetPropertyIndex);
	const FProperty* SourceProperty = InSourceStorage->GetProperty(InSourcePropertyIndex);
	uint8* TargetPtr = InTargetStorage->GetData<uint8>(TargetProperty);
	uint8* SourcePtr = InSourceStorage->GetData<uint8>(SourceProperty);

	return CopyProperty(TargetProperty, TargetPtr, InTargetPropertyPath, SourceProperty, SourcePtr, InSourcePropertyPath);
}

bool FRigVMMemoryStorageStruct::CopyProperty(
	FRigVMMemoryHandle& InTargetHandle,
	FRigVMMemoryHandle& InSourceHandle)
{
	return CopyProperty(
		InTargetHandle.GetProperty(),
		InTargetHandle.GetData(false),
		InTargetHandle.GetPropertyPathRef(),
		InSourceHandle.GetProperty(),
		InSourceHandle.GetData(false),
		InSourceHandle.GetPropertyPathRef());
}


//****************************************************************************

void FRigVMMemoryStorageStruct::RefreshLinkedProperties()
{
	LinkedProperties.Reset();

	if (const UPropertyBag* CurrentBagStruct = GetPropertyBagStruct())
	{
		const FProperty* Property = CastField<FProperty>(CurrentBagStruct->ChildProperties);
		while (Property)
		{
			LinkedProperties.Add(Property);
			Property = CastField<FProperty>(Property->Next);
		}
	}
}

void FRigVMMemoryStorageStruct::RefreshPropertyPaths()
{
	// Update the property paths based on the descriptions
	PropertyPaths.SetNumZeroed(PropertyPathDescriptions.Num());
	for (int32 PropertyPathIndex = 0; PropertyPathIndex < PropertyPaths.Num(); PropertyPathIndex++)
	{
		PropertyPaths[PropertyPathIndex] = FRigVMPropertyPath();

		const int32 PropertyIndex = PropertyPathDescriptions[PropertyPathIndex].PropertyIndex;
		if (LinkedProperties.IsValidIndex(PropertyIndex))
		{
			PropertyPaths[PropertyPathIndex] = FRigVMPropertyPath(
				LinkedProperties[PropertyIndex],
				PropertyPathDescriptions[PropertyPathIndex].SegmentPath);
		}
	}
}

void FRigVMMemoryStorageStruct::SetDefaultValues(const TArray<FRigVMPropertyDescription>& InPropertyDescriptions)
{
	// and store default values.
	const TArray<const FProperty*>& Properties = GetProperties();
	
	const int32 NumProperties = Properties.Num();
	if (!ensure(NumProperties == InPropertyDescriptions.Num()))
	{
		UE_LOG(LogRigVM, Error, TEXT("Number of properties in storage is different from descriptors, unsupported type or duplicated name in the generated properties"));
		return;
	}

	for (int32 PropertyIndex = 0; PropertyIndex < NumProperties; PropertyIndex++)
	{
		const FRigVMPropertyDescription& RigVMDesc = InPropertyDescriptions[PropertyIndex];
		const FString& DefaultValue = RigVMDesc.DefaultValue;

		if (DefaultValue.IsEmpty() || DefaultValue == UE::RigVM::RigVMCore::Private::EmptyBraces)
		{
			continue;
		}

		SetDataFromString(PropertyIndex, DefaultValue);
	}
}

//****************************************************************************

/*static*/ FPropertyBagPropertyDesc FRigVMMemoryStorageStruct::GeneratePropertyBagDescriptor(const FRigVMPropertyDescription& RigVMDescriptor)
{
	FPropertyBagPropertyDesc Result;

	EPropertyBagPropertyType PropertyBagType = EPropertyBagPropertyType::None;
	FPropertyBagContainerTypes PropertyBagContainerTypes;

	if (GetPropertyTypeDataFromVMDescriptor(RigVMDescriptor, PropertyBagType, PropertyBagContainerTypes))
	{
		if (PropertyBagContainerTypes.Num() > 0)
		{
			Result = FPropertyBagPropertyDesc(RigVMDescriptor.Name, PropertyBagContainerTypes, PropertyBagType, RigVMDescriptor.CPPTypeObject);
		}
		else
		{
			Result = FPropertyBagPropertyDesc(RigVMDescriptor.Name, PropertyBagType, RigVMDescriptor.CPPTypeObject);
		}

		const FString Name = RigVMDescriptor.Name.ToString();

		FBlake3 Builder;
		Builder.Update(*Name, Name.Len());
		Builder.Update(*RigVMDescriptor.CPPType, RigVMDescriptor.CPPType.Len());
		FBlake3Hash Hash = Builder.Finalize();
		Result.ID = FGuid::NewGuidFromHash(Hash);
	}

	return Result;
}

/*static*/ bool FRigVMMemoryStorageStruct::GetPropertyTypeDataFromVMDescriptor(const FRigVMPropertyDescription& RigVMDescriptor, EPropertyBagPropertyType & OutBagPropertyType, FPropertyBagContainerTypes& OutBagContainerTypes)
{
	EPropertyBagPropertyType BagPropertyType = EPropertyBagPropertyType::None;

	FString VMTypeString = *RigVMDescriptor.CPPType;

	OutBagContainerTypes.Reset(); // in case it is reused by the caller

	if (RigVMTypeUtils::IsArrayType(VMTypeString))
	{
		const int32 NumContainers = RigVMDescriptor.Containers.Num();
		for (int i= 0; i < NumContainers; ++i)
		{
			switch (RigVMDescriptor.Containers[i])
			{
			case EPinContainerType::Array:
				OutBagContainerTypes.Add(EPropertyBagContainerType::Array);
				break;
			case EPinContainerType::Set:
				ensureMsgf(false, TEXT("Unsuported Set type container : %s"), *VMTypeString);
				break;
			case EPinContainerType::Map:
				ensureMsgf(false, TEXT("Unsuported Map type container : %s"), *VMTypeString);
				break;
			default:
				break;
			}
		}

		VMTypeString = RigVMDescriptor.GetTailCPPType();
	}

	FName VMType = *VMTypeString;

	static const FName IntTypeName(TEXT("int")); // type used by some engine tests
	static const FName Int64TypeName(TEXT("Int64"));
	static const FName UInt64TypeName(TEXT("UInt64"));

	if (VMType == RigVMTypeUtils::BoolTypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::Bool;
	}
	else if (VMType == RigVMTypeUtils::Int32TypeName || VMType == IntTypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::Int32;
	}
	else if (VMType == RigVMTypeUtils::UInt32TypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::UInt32;
	}
	else if (VMType == Int64TypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::Int64;
	}
	else if (VMType == UInt64TypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::UInt64;
	}
	else if (VMType == RigVMTypeUtils::FloatTypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::Float;
	}
	else if (VMType == RigVMTypeUtils::DoubleTypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::Double;
	}
	else if (VMType == RigVMTypeUtils::FNameTypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::Name;
	}
	else if (VMType == RigVMTypeUtils::FStringTypeName)
	{
		OutBagPropertyType = EPropertyBagPropertyType::String;
	}
	else if (Cast<UScriptStruct>(RigVMDescriptor.CPPTypeObject))
	{
		OutBagPropertyType = EPropertyBagPropertyType::Struct;
	}
	else if (Cast<UEnum>(RigVMDescriptor.CPPTypeObject))
	{
		OutBagPropertyType = EPropertyBagPropertyType::Enum;
	}
	else if (Cast<UObject>(RigVMDescriptor.CPPTypeObject))
	{
		OutBagPropertyType = EPropertyBagPropertyType::Object;
	}
	else
	{
		ensureMsgf(false, TEXT("Unsupported type : %s"), *VMTypeString);
		OutBagPropertyType = EPropertyBagPropertyType::None;
	}

	return OutBagPropertyType != EPropertyBagPropertyType::None;
}
