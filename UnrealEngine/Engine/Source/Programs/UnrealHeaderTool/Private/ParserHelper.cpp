// Copyright Epic Games, Inc. All Rights Reserved.


#include "ParserHelper.h"
#include "UnrealHeaderTool.h"
#include "Algo/Find.h"
#include "Misc/DefaultValueHelper.h"
#include "UnrealTypeDefinitionInfo.h"
#include "ClassMaps.h"

// Globals for common class definitions
extern FUnrealClassDefinitionInfo* GUObjectDef;
extern FUnrealClassDefinitionInfo* GUClassDef;
extern FUnrealClassDefinitionInfo* GUInterfaceDef;

/////////////////////////////////////////////////////
// FPropertyBase

FPropertyBase::FPropertyBase(EPropertyType InType)
	: Type(InType)
	, IntType(GetSizedIntTypeFromPropertyType(InType))
{
}

FPropertyBase::FPropertyBase(EPropertyType InType, EIntType InIntType)
	: Type(InType)
	, IntType(InIntType)
{
}

FPropertyBase::FPropertyBase(FUnrealEnumDefinitionInfo& InEnumDef, EPropertyType InType)
	: Type(InType)
	, EnumDef(&InEnumDef)
	, IntType(GetSizedIntTypeFromPropertyType(InType))
{
}

FPropertyBase::FPropertyBase(FUnrealClassDefinitionInfo& InClassDef, EPropertyType InType, bool bWeakIsAuto/* = false*/)
	: Type(InType)
	, ClassDef(&InClassDef)
{
	if ((Type == CPT_WeakObjectReference) && bWeakIsAuto)
	{
		PropertyFlags |= CPF_AutoWeak;
	}
}

FPropertyBase::FPropertyBase(FUnrealScriptStructDefinitionInfo& InStructDef)
	: Type(CPT_Struct)
	, ScriptStructDef(&InStructDef)
{
}

FPropertyBase::FPropertyBase(FName InFieldClassName, EPropertyType InType)
	: Type(InType)
	, FieldClassName(InFieldClassName)
	, IntType(GetSizedIntTypeFromPropertyType(InType))
{
}

FUnrealEnumDefinitionInfo* FPropertyBase::AsEnum() const
{
	return UHTCast<FUnrealEnumDefinitionInfo>(TypeDef);
}

bool FPropertyBase::IsEnum() const
{
	return AsEnum() != nullptr;
}

EUHTPropertyType FPropertyBase::GetUHTPropertyType() const
{
	if (ArrayType == EArrayType::Dynamic)
	{
		return EUHTPropertyType::DynamicArray;
	}
	else if (ArrayType == EArrayType::Set)
	{
		return EUHTPropertyType::Set;
	}
	else if (MapKeyProp.IsValid())
	{
		return EUHTPropertyType::Map;
	}
	else if (IsEnum())
	{
		return EUHTPropertyType::Enum;
	}
	else
	{
		return EUHTPropertyType(Type);
	}
}

bool FPropertyBase::IsClassRefOrClassRefStaticArray() const
{
	return IsObjectRefOrObjectRefStaticArray() && ClassDef->IsChildOf(*GUClassDef);
}

bool FPropertyBase::IsByteEnumOrByteEnumStaticArray() const
{
	if (Type != CPT_Byte || !IsPrimitiveOrPrimitiveStaticArray())
	{
		return false;
	}
	FUnrealEnumDefinitionInfo* Enum = AsEnum();
	if (Enum == nullptr)
	{
		return false;
	}
	return Enum->GetCppForm() != UEnum::ECppForm::EnumClass;
}

bool FPropertyBase::ContainsEditorOnlyProperties() const
{
	if (Type == CPT_Struct)
	{
		check(ScriptStructDef);
		for (const TSharedRef<FUnrealPropertyDefinitionInfo>& PropDef : ScriptStructDef->GetProperties())
		{
			if (PropDef->IsEditorOnlyProperty() || PropDef->GetPropertyBase().ContainsEditorOnlyProperties())
			{
				return true;
			}
		}
	}
	return false;
}

bool FPropertyBase::MatchesType(const FPropertyBase& Other, bool bDisallowGeneralization, bool bIgnoreImplementedInterfaces/* = false*/, bool bEmulateSameType/* = false*/) const
{
	check(Type != CPT_None || !bDisallowGeneralization);

	bool bIsObjectType = IsObjectOrInterface();
	bool bOtherIsObjectType = Other.IsObjectOrInterface();
	bool bIsObjectComparison = bIsObjectType && bOtherIsObjectType;
	bool bReverseClassChainCheck = true;

	// If converting to an l-value, we require an exact match with an l-value.
	if ((PropertyFlags & CPF_OutParm) != 0)
	{
		// if the other type is not an l-value, disallow
		if ((Other.PropertyFlags & CPF_OutParm) == 0)
		{
			return false;
		}

		// if the other type is const and we are not const, disallow
		if ((Other.PropertyFlags & CPF_ConstParm) != 0 && (PropertyFlags & CPF_ConstParm) == 0)
		{
			return false;
		}

		if (Type == CPT_Struct)
		{
			// Allow derived structs to be passed by reference, unless this is a dynamic array of structs
			bDisallowGeneralization = bDisallowGeneralization || ArrayType == EArrayType::Dynamic || Other.ArrayType == EArrayType::Dynamic;
		}

		// if Type == CPT_ObjectReference, out object function parm; allow derived classes to be passed in
		// if Type == CPT_Interface, out interface function parm; allow derived classes to be passed in
		else if ((PropertyFlags & CPF_ConstParm) == 0 || !IsObjectOrInterface())
		{
			// all other variable types must match exactly when passed as the value to an 'out' parameter
			bDisallowGeneralization = true;
		}

		// both types are objects, but one is an interface and one is an object reference
		else if (bIsObjectComparison && Type != Other.Type)
		{
			return false;
		}
	}
	else if ((Type == CPT_ObjectReference || Type == CPT_WeakObjectReference || Type == CPT_LazyObjectReference || Type == CPT_ObjectPtrReference || Type == CPT_SoftObjectReference) && Other.Type != CPT_Interface && (PropertyFlags & CPF_ReturnParm))
	{
		bReverseClassChainCheck = false;
	}

	// Check everything.
	if (Type == CPT_None && (Other.Type == CPT_None || !bDisallowGeneralization))
	{
		// If Other has no type, accept anything.
		return true;
	}
	else if (Type != Other.Type && !(bEmulateSameType && ::IsBool(Type) && ::IsBool(Other.Type)) && !bIsObjectComparison)
	{
		// Mismatched base types.
		return false;
	}
	else if (ArrayType != Other.ArrayType)
	{
		// Mismatched array types.
		return false;
	}
	else if (Type == CPT_Byte)
	{
		// Make sure enums match, or we're generalizing.
		return EnumDef == Other.EnumDef || (EnumDef == NULL && !bDisallowGeneralization);
	}
	else if (bIsObjectType)
	{
		check(ClassDef != NULL);

		// Make sure object types match, or we're generalizing.
		if (bDisallowGeneralization)
		{
			// Exact match required.
			return ClassDef == Other.ClassDef && MetaClassDef == Other.MetaClassDef;
		}
		else if (Other.ClassDef == NULL)
		{
			// Cannonical 'None' matches all object classes.
			return true;
		}
		else
		{
			// Generalization is ok (typical example of this check would look like: VarA = VarB;, where this is VarB and Other is VarA)
			if (Other.ClassDef->IsChildOf(*ClassDef))
			{
				if (!bIgnoreImplementedInterfaces || ((Type == CPT_Interface) == (Other.Type == CPT_Interface)))
				{
					if (!ClassDef->IsChildOf(*GUClassDef) || MetaClassDef == nullptr || Other.MetaClassDef->IsChildOf(*MetaClassDef) ||
						(bReverseClassChainCheck && (Other.MetaClassDef == nullptr || MetaClassDef->IsChildOf(*Other.MetaClassDef))))
					{
						return true;
					}
				}
			}
			// check the opposite class chain for object types
			else if (bReverseClassChainCheck && Type != CPT_Interface && bIsObjectComparison && ClassDef != nullptr && ClassDef->IsChildOf(*Other.ClassDef))
			{
				if (!Other.ClassDef->IsChildOf(*GUClassDef) || MetaClassDef == nullptr || Other.MetaClassDef == nullptr || MetaClassDef->IsChildOf(*Other.MetaClassDef) || Other.MetaClassDef->IsChildOf(*MetaClassDef))
				{
					return true;
				}
			}

			if (ClassDef->HasAnyClassFlags(CLASS_Interface) && !bIgnoreImplementedInterfaces)
			{
				if (Other.ClassDef->ImplementsInterface(*ClassDef))
				{
					return true;
				}
			}

			return false;
		}
	}
	else if (Type == CPT_Struct)
	{
		check(ScriptStructDef != NULL);
		check(Other.ScriptStructDef != NULL);

		if (ScriptStructDef == Other.ScriptStructDef)
		{
			// struct types match exactly 
			return true;
		}

		// returning false here prevents structs related through inheritance from being used interchangeably, such as passing a derived struct as the value for a parameter
		// that expects the base struct, or vice versa.  An easier example is assignment (e.g. Vector = Plane or Plane = Vector).
		// there are two cases to consider (let's use vector and plane for the example):
		// - Vector = Plane;
		//		in this expression, 'this' is the vector, and Other is the plane.  This is an unsafe conversion, as the destination property type is used to copy the r-value to the l-value
		//		so in this case, the VM would call CopyCompleteValue on the FPlane struct, which would copy 16 bytes into the l-value's buffer;  However, the l-value buffer will only be
		//		12 bytes because that is the size of FVector
		// - Plane = Vector;
		//		in this expression, 'this' is the plane, and Other is the vector.  This is a safe conversion, since only 12 bytes would be copied from the r-value into the l-value's buffer
		//		(which would be 16 bytes).  The problem with allowing this conversion is that what to do with the extra member (e.g. Plane.W); should it be left alone? should it be zeroed?
		//		difficult to say what the correct behavior should be, so let's just ignore inheritance for the sake of determining whether two structs are identical

		// Previously, the logic for determining whether this is a generalization of Other was reversed; this is very likely the culprit behind all current issues with 
		// using derived structs interchangeably with their base versions.  The inheritance check has been fixed; for now, allow struct generalization and see if we can find any further
		// issues with allowing conversion.  If so, then we disable all struct generalization by returning false here.
		// return false;

		if (bDisallowGeneralization)
		{
			return false;
		}

		// Generalization is ok if this is not a dynamic array
		if (ArrayType != EArrayType::Dynamic && Other.ArrayType != EArrayType::Dynamic)
		{
			if (!Other.ScriptStructDef->IsChildOf(*ScriptStructDef) && ScriptStructDef->IsChildOf(*Other.ScriptStructDef))
			{
				return true;
			}
		}

		return false;
	}
	else
	{
		// General match.
		return true;
	}
}

/////////////////////////////////////////////////////
// FFuncData

void FFuncInfo::SetFunctionNames(FUnrealFunctionDefinitionInfo& FunctionDef)
{
	FString FunctionName = FunctionDef.GetName();
	if (FunctionDef.HasAnyFunctionFlags(FUNC_Delegate))
	{
		FunctionName.LeftChopInline(FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len(), false);
	}
	UnMarshallAndCallName = FString(TEXT("exec")) + FunctionName;

	if (FunctionDef.HasAnyFunctionFlags(FUNC_BlueprintEvent))
	{
		MarshallAndCallName = FunctionName;
	}
	else
	{
		MarshallAndCallName = FString(TEXT("event")) + FunctionName;
	}

	if (FunctionDef.HasAllFunctionFlags(FUNC_Native | FUNC_Net))
	{
		MarshallAndCallName = FunctionName;
		if (FunctionDef.HasAllFunctionFlags(FUNC_NetResponse))
		{
			// Response function implemented by programmer and called directly from thunk
			CppImplName = FunctionDef.GetName();
		}
		else
		{
			if (CppImplName.IsEmpty())
			{
				CppImplName = FunctionDef.GetName() + TEXT("_Implementation");
			}
			else if (CppImplName == FunctionName)
			{
				FunctionDef.Throwf(TEXT("Native implementation function must be different than original function name."));
			}

			if (CppValidationImplName.IsEmpty() && FunctionDef.HasAllFunctionFlags(FUNC_NetValidate))
			{
				CppValidationImplName = FunctionDef.GetName() + TEXT("_Validate");
			}
			else if (CppValidationImplName == FunctionName)
			{
				FunctionDef.Throwf(TEXT("Validation function must be different than original function name."));
			}
		}
	}

	if (FunctionDef.HasAllFunctionFlags(FUNC_Delegate))
	{
		MarshallAndCallName = FString(TEXT("delegate")) + FunctionName;
	}

	if (FunctionDef.HasAllFunctionFlags(FUNC_BlueprintEvent | FUNC_Native))
	{
		MarshallAndCallName = FunctionName;
		CppImplName = FunctionDef.GetName() + TEXT("_Implementation");
	}

	if (CppImplName.IsEmpty())
	{
		CppImplName = FunctionName;
	}
}

/////////////////////////////////////////////////////
// FAdvancedDisplayParameterHandler
static const FName NAME_AdvancedDisplay(TEXT("AdvancedDisplay"));

FAdvancedDisplayParameterHandler::FAdvancedDisplayParameterHandler(const TMap<FName, FString>* MetaData)
	: NumberLeaveUnmarked(-1), AlreadyLeft(0), bUseNumber(false)
{
	if(MetaData)
	{
		const FString* FoundString = MetaData->Find(NAME_AdvancedDisplay);
		if(FoundString)
		{
			FoundString->ParseIntoArray(ParametersNames, TEXT(","), true);
			for(int32 NameIndex = 0; NameIndex < ParametersNames.Num();)
			{
				FString& ParameterName = ParametersNames[NameIndex];
				ParameterName.TrimStartAndEndInline();
				if(ParameterName.IsEmpty())
				{
					ParametersNames.RemoveAtSwap(NameIndex);
				}
				else
				{
					++NameIndex;
				}
			}
			if(1 == ParametersNames.Num())
			{
				bUseNumber = FDefaultValueHelper::ParseInt(ParametersNames[0], NumberLeaveUnmarked);
			}
		}
	}
}

bool FAdvancedDisplayParameterHandler::ShouldMarkParameter(const FString& ParameterName)
{
	if(bUseNumber)
	{
		if(NumberLeaveUnmarked < 0)
		{
			return false;
		}
		if(AlreadyLeft < NumberLeaveUnmarked)
		{
			AlreadyLeft++;
			return false;
		}
		return true;
	}
	return ParametersNames.Contains(ParameterName);
}

bool FAdvancedDisplayParameterHandler::CanMarkMore() const
{
	return bUseNumber ? (NumberLeaveUnmarked > 0) : (0 != ParametersNames.Num());
}
