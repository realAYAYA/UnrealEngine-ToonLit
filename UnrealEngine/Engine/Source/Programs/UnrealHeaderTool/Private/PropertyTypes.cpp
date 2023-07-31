// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyTypes.h"
#include "BaseParser.h"
#include "ClassMaps.h"
#include "EngineAPI.h"
#include "HeaderParser.h"
#include "UnrealHeaderTool.h"
#include "UnrealTypeDefinitionInfo.h"
#include "Misc/DefaultValueHelper.h"
#include "UObject/ObjectMacros.h"

// Globals for common class definitions
extern FUnrealClassDefinitionInfo* GUObjectDef;
extern FUnrealClassDefinitionInfo* GUClassDef;
extern FUnrealClassDefinitionInfo* GUInterfaceDef;

extern void AddEditInlineMetaData(TMap<FName, FString>& MetaData);

// Following is the relationship between the property types
//
//		FProperty
//			FNumericProperty
//				FByteProperty
//				FInt8Property
//				FInt16Property
//				FIntProperty
//				FInt64Property
//				FUInt16Property
//				FUInt32Property
//				FUInt64Property
//				FFloatProperty
//				FDoubleProperty
//				FLargeWorldCoordinatesRealProperty
//			FBoolProperty
//			FEnumProperty
//			TObjectPropertyBase
//				FObjectProperty
//					FClassProperty
//						FClassPtrProperty
//					FObjectPtrProperty
//				FWeakObjectProperty
//				FLazyObjectProperty
//				FSoftObjectProperty
//					FSoftClassProperty
//			FInterfaceProperty
//			FNameProperty
//			FStrProperty
//			FTextProperty
//			FStructProperty
//			FMulticastSparseDelegateProperty
//			FMulticastInlineDelegateProperty
//			FFieldPathProperty
//			FArrayProperty
//			FSetProperty
//			FMapProperty

struct FPropertyTypeTraitsByte;
struct FPropertyTypeTraitsInt8;
struct FPropertyTypeTraitsInt16;
struct FPropertyTypeTraitsInt;
struct FPropertyTypeTraitsInt64;
struct FPropertyTypeTraitsUInt16;
struct FPropertyTypeTraitsUInt32;
struct FPropertyTypeTraitsUInt64;
struct FPropertyTypeTraitsBool;
struct FPropertyTypeTraitsBool8;
struct FPropertyTypeTraitsBool16;
struct FPropertyTypeTraitsBool32;
struct FPropertyTypeTraitsBool64;
struct FPropertyTypeTraitsFloat;
struct FPropertyTypeTraitsDouble;
struct FPropertyTypeTraitsLargeWorldCoordinatesReal;
struct FPropertyTypeTraitsObjectReference;
struct FPropertyTypeTraitsWeakObjectReference;
struct FPropertyTypeTraitsLazyObjectReference;
struct FPropertyTypeTraitsObjectPtrReference;
struct FPropertyTypeTraitsSoftObjectReference;
struct FPropertyTypeTraitsInterface;
struct FPropertyTypeTraitsName;
struct FPropertyTypeTraitsString;
struct FPropertyTypeTraitsText;
struct FPropertyTypeTraitsStruct;
struct FPropertyTypeTraitsDelegate;
struct FPropertyTypeTraitsMulticastDelegate;
struct FPropertyTypeTraitsFieldPath;
struct FPropertyTypeTraitsStaticArray;
struct FPropertyTypeTraitsDynamicArray;
struct FPropertyTypeTraitsSet;
struct FPropertyTypeTraitsMap;
struct FPropertyTypeTraitsEnum;

namespace
{
	static const FName NAME_ArraySizeEnum(TEXT("ArraySizeEnum"));

	struct FPropertyFlagsChanges
	{
		EPropertyFlags Added;
		EPropertyFlags Removed;
	};

	struct FPropertyFlagsChangeDetector
	{
		FPropertyFlagsChangeDetector(FUnrealPropertyDefinitionInfo& InPropDef)
			: PropDef(InPropDef)
			, OldFlags(InPropDef.GetPropertyFlags())
		{}

		FPropertyFlagsChanges Changes()
		{
			EPropertyFlags NewFlags = PropDef.GetPropertyFlags();
			return FPropertyFlagsChanges{ NewFlags & ~OldFlags, OldFlags & ~NewFlags };
		}

		FUnrealPropertyDefinitionInfo& PropDef;
		EPropertyFlags OldFlags;
	};

	void PropagateFlagsFromInnerAndHandlePersistentInstanceMetadata(EPropertyFlags& DestFlags, const TMap<FName, FString>& InMetaData, FUnrealPropertyDefinitionInfo& InnerDef)
	{
		// Copy some of the property flags to the container property.
		if (InnerDef.HasAnyPropertyFlags(CPF_ContainsInstancedReference | CPF_InstancedReference))
		{
			DestFlags |= CPF_ContainsInstancedReference;
			DestFlags &= ~(CPF_InstancedReference | CPF_PersistentInstance); //this was propagated to the inner

			if (InnerDef.HasAnyPropertyFlags(CPF_PersistentInstance))
			{
				FUHTMetaData::RemapAndAddMetaData(InnerDef, TMap<FName, FString>(InMetaData));
			}
		}
	};

	class FEnumLookup
	{
	public:
		FEnumLookup()
		{
			GTypeDefinitionInfoMap.ForAllTypesByName([this](FUnrealTypeDefinitionInfo& TypeDef) 
				{
					if (FUnrealEnumDefinitionInfo* EnumDef = UHTCast<FUnrealEnumDefinitionInfo>(TypeDef))
					{
						bool bAddShortNames = EnumDef->GetCppForm() == UEnum::ECppForm::EnumClass || EnumDef->GetCppForm() == UEnum::ECppForm::Namespaced;
						FString CheckName = EnumDef->GetNameCPP() + TEXT("::");
						for (const TPair<FName, int64>& Kvp : EnumDef->GetEnums())
						{
							FullEnumValueMap.Add(Kvp.Key, FEnumAndValue{ EnumDef, Kvp.Value });
							if (bAddShortNames)
							{
								FString EnumName = Kvp.Key.ToString();
								check(EnumName.StartsWith(CheckName, ESearchCase::CaseSensitive));
								EnumName.RightChopInline(CheckName.Len());
								ShortEnumValueMap.Add(FName(EnumName), EnumDef);
							}
						}
					}
				}
			);
		}

		// This code emulates UEnum::LookupEnumNameSlow.  There are some cases where the Value check for INDEX_NONE is 
		// rejecting matches.
		FUnrealEnumDefinitionInfo* Find(const TCHAR* EnumValueName)
		{
			FName Key(EnumValueName, FNAME_Find);
			if (Key == NAME_None)
			{
				return nullptr;
			}

			// If we can find the full name and it has an index, then use this enum
			const FEnumAndValue* Value = FullEnumValueMap.Find(Key);
			if (Value != nullptr && Value->Value != INDEX_NONE)
			{
				return Value->Enum;
			}

			// If the enum name already has a ::, then we can't do a short name search
			if (FCString::Strifind(EnumValueName, TEXT("::"), false) != nullptr)
			{
				return nullptr;
			}

			return ShortEnumValueMap.FindRef(Key);
		}

	private:
		struct FEnumAndValue
		{
			FUnrealEnumDefinitionInfo* Enum;
			int64 Value;
		};

		TMap<FName, FEnumAndValue> FullEnumValueMap;
		TMap<FName, FUnrealEnumDefinitionInfo*> ShortEnumValueMap;
	};

	FEnumLookup& GetEnumLookup()
	{
		static FEnumLookup EnumLookup;
		return EnumLookup;
	}

	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	// Field class bootstrap system
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------

	class FFieldClassInit
	{
	public:
		FFieldClassInit()
		{
			FProperty::StaticClass();
			FNumericProperty::StaticClass();
			FByteProperty::StaticClass();
			FInt8Property::StaticClass();
			FInt16Property::StaticClass();
			FIntProperty::StaticClass();
			FInt64Property::StaticClass();
			FUInt16Property::StaticClass();
			FUInt32Property::StaticClass();
			FUInt64Property::StaticClass();
			FFloatProperty::StaticClass();
			FDoubleProperty::StaticClass();
			FLargeWorldCoordinatesRealProperty::StaticClass();
			FBoolProperty::StaticClass();
			FEnumProperty::StaticClass();
			//TObjectPropertyBase::StaticClass();
			FObjectProperty::StaticClass();
			FClassProperty::StaticClass();
			FClassPtrProperty::StaticClass();
			FObjectPtrProperty::StaticClass();
			FWeakObjectProperty::StaticClass();
			FLazyObjectProperty::StaticClass();
			FSoftObjectProperty::StaticClass();
			FSoftClassProperty::StaticClass();
			FInterfaceProperty::StaticClass();
			FNameProperty::StaticClass();
			FStrProperty::StaticClass();
			FTextProperty::StaticClass();
			FStructProperty::StaticClass();
			FMulticastDelegateProperty::StaticClass();
			FMulticastSparseDelegateProperty::StaticClass();
			FMulticastInlineDelegateProperty::StaticClass();
			FFieldPathProperty::StaticClass();
			FArrayProperty::StaticClass();
			FSetProperty::StaticClass();
			FMapProperty::StaticClass();
		}
	};

	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	// Dispatch system
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------

	/**
	 * Based on the type and container settings, dispatch to the correct traits
	 */
	template <template <typename PropertyTraits> typename FuncDispatch, bool bHandleContainers, typename RetValue, typename PropDefType, typename ... Args>
	RetValue PropertyTypeDispatch(PropDefType& PropDef, Args&& ... args)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		if constexpr (bHandleContainers)
		{
			switch (VarProperty.ArrayType)
			{
			case EArrayType::Static:
				return FuncDispatch<FPropertyTypeTraitsStaticArray>()(PropDef, std::forward<Args>(args)...);
			case EArrayType::Dynamic:
				return FuncDispatch<FPropertyTypeTraitsDynamicArray>()(PropDef, std::forward<Args>(args)...);
			case EArrayType::Set:
				return FuncDispatch<FPropertyTypeTraitsSet>()(PropDef, std::forward<Args>(args)...);
			}

			if (VarProperty.MapKeyProp.IsValid())
			{
				return FuncDispatch<FPropertyTypeTraitsMap>()(PropDef, std::forward<Args>(args)...);
			}
		}

		// Check if it's an enum class property
		// NOTE: VarProperty.EnumDef is a union and might not be an enum
		if (VarProperty.IsEnum())
		{
			return FuncDispatch<FPropertyTypeTraitsEnum>()(PropDef, std::forward<Args>(args)...);
		}


		// We are just a simple engine type
		switch (PropDef.GetPropertyBase().Type)
		{
		case CPT_Byte:						return FuncDispatch<FPropertyTypeTraitsByte>()(PropDef, std::forward<Args>(args)...);
		case CPT_Int8:						return FuncDispatch<FPropertyTypeTraitsInt8>()(PropDef, std::forward<Args>(args)...);
		case CPT_Int16:						return FuncDispatch<FPropertyTypeTraitsInt16>()(PropDef, std::forward<Args>(args)...);
		case CPT_Int:						return FuncDispatch<FPropertyTypeTraitsInt>()(PropDef, std::forward<Args>(args)...);
		case CPT_Int64:						return FuncDispatch<FPropertyTypeTraitsInt64>()(PropDef, std::forward<Args>(args)...);
		case CPT_UInt16:					return FuncDispatch<FPropertyTypeTraitsUInt16>()(PropDef, std::forward<Args>(args)...);
		case CPT_UInt32:					return FuncDispatch<FPropertyTypeTraitsUInt32>()(PropDef, std::forward<Args>(args)...);
		case CPT_UInt64:					return FuncDispatch<FPropertyTypeTraitsUInt64>()(PropDef, std::forward<Args>(args)...);
		case CPT_Bool:						return FuncDispatch<FPropertyTypeTraitsBool>()(PropDef, std::forward<Args>(args)...);
		case CPT_Bool8:						return FuncDispatch<FPropertyTypeTraitsBool8>()(PropDef, std::forward<Args>(args)...);
		case CPT_Bool16:					return FuncDispatch<FPropertyTypeTraitsBool16>()(PropDef, std::forward<Args>(args)...);
		case CPT_Bool32:					return FuncDispatch<FPropertyTypeTraitsBool32>()(PropDef, std::forward<Args>(args)...);
		case CPT_Bool64:					return FuncDispatch<FPropertyTypeTraitsBool64>()(PropDef, std::forward<Args>(args)...);
		case CPT_Float:						return FuncDispatch<FPropertyTypeTraitsFloat>()(PropDef, std::forward<Args>(args)...);
		case CPT_Double:					return FuncDispatch<FPropertyTypeTraitsDouble>()(PropDef, std::forward<Args>(args)...);
		case CPT_FLargeWorldCoordinatesReal:return FuncDispatch<FPropertyTypeTraitsLargeWorldCoordinatesReal>()(PropDef, std::forward<Args>(args)...);
		case CPT_ObjectReference:			return FuncDispatch<FPropertyTypeTraitsObjectReference>()(PropDef, std::forward<Args>(args)...);
		case CPT_WeakObjectReference:		return FuncDispatch<FPropertyTypeTraitsWeakObjectReference>()(PropDef, std::forward<Args>(args)...);
		case CPT_LazyObjectReference:		return FuncDispatch<FPropertyTypeTraitsLazyObjectReference>()(PropDef, std::forward<Args>(args)...);
		case CPT_ObjectPtrReference:		return FuncDispatch<FPropertyTypeTraitsObjectPtrReference>()(PropDef, std::forward<Args>(args)...);
		case CPT_SoftObjectReference:		return FuncDispatch<FPropertyTypeTraitsSoftObjectReference>()(PropDef, std::forward<Args>(args)...);
		case CPT_Interface:					return FuncDispatch<FPropertyTypeTraitsInterface>()(PropDef, std::forward<Args>(args)...);
		case CPT_Name:						return FuncDispatch<FPropertyTypeTraitsName>()(PropDef, std::forward<Args>(args)...);
		case CPT_String:					return FuncDispatch<FPropertyTypeTraitsString>()(PropDef, std::forward<Args>(args)...);
		case CPT_Text:						return FuncDispatch<FPropertyTypeTraitsText>()(PropDef, std::forward<Args>(args)...);
		case CPT_Struct:					return FuncDispatch<FPropertyTypeTraitsStruct>()(PropDef, std::forward<Args>(args)...);
		case CPT_Delegate:					return FuncDispatch<FPropertyTypeTraitsDelegate>()(PropDef, std::forward<Args>(args)...);
		case CPT_MulticastDelegate:			return FuncDispatch<FPropertyTypeTraitsMulticastDelegate>()(PropDef, std::forward<Args>(args)...);
		case CPT_FieldPath:					return FuncDispatch<FPropertyTypeTraitsFieldPath>()(PropDef, std::forward<Args>(args)...);
		default:							PropDef.Throwf(TEXT("Unknown property type %i"), (uint8)PropDef.GetPropertyBase().Type);
		}
	}

	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	// Dispatch Functors
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------

	template<typename TraitsType>
	struct DefaultValueStringCppFormatToInnerFormatDispatch
	{
		bool operator()(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
		{
			return TraitsType::DefaultValueStringCppFormatToInnerFormat(PropDef, CppForm, OutForm);
		}
	};

	template<typename TraitsType>
	struct CreatePropertyDispatch
	{
		void operator()(FUnrealPropertyDefinitionInfo& PropDef)
		{
			TraitsType::CreateProperty(PropDef);
		}
	};

	template<typename TraitsType>
	struct CreateEngineTypeDispatch
	{
		FProperty* operator()(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
		{
			return TraitsType::CreateEngineType(PropDef, Scope, Name, ObjectFlags);
		}
	};

	template<typename TraitsType>
	struct IsSupportedByBlueprintDispatch
	{
		bool operator()(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
		{
			return TraitsType::IsSupportedByBlueprint(PropDef, bMemberVariable);
		}
	};

	template<typename TraitsType>
	struct GetEngineClassNameDispatch
	{
		FString operator()(const FUnrealPropertyDefinitionInfo& PropDef)
		{
			return TraitsType::GetEngineClassName(PropDef);
		}
	};

	template<typename TraitsType>
	struct GetCPPTypeDispatch
	{
		FString operator()(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
		{
			return TraitsType::GetCPPType(PropDef, ExtendedTypeText, CPPExportFlags);
		}
	};

	template<typename TraitsType>
	struct GetCPPTypeForwardDeclarationDispatch
	{
		FString operator()(const FUnrealPropertyDefinitionInfo& PropDef)
		{
			return TraitsType::GetCPPTypeForwardDeclaration(PropDef);
		}
	};

	template<typename TraitsType>
	struct GetCPPMacroTypeDispatch
	{
		FString operator()(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
		{
			return TraitsType::GetCPPMacroType(PropDef, ExtendedTypeText);
		}
	};

	template<typename TraitsType>
	struct PostParseFinalizeDispatch
	{
		FPropertyFlagsChanges operator()(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
		{
			return TraitsType::PostParseFinalize(PropDef, AddedFlags);
		}
	};

	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	// Helper Methods
	//----------------------------------------------------------------------------------------------------------------------------------------------------------
	//----------------------------------------------------------------------------------------------------------------------------------------------------------

	template <bool bHandleContainers>
	void CreatePropertyHelper(FUnrealPropertyDefinitionInfo& PropDef)
	{
		return PropertyTypeDispatch<CreatePropertyDispatch, bHandleContainers, void>(PropDef);
	}

	template <bool bHandleContainers>
	FProperty* CreateEngineTypeHelper(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		return PropertyTypeDispatch<CreateEngineTypeDispatch, bHandleContainers, FProperty*>(PropDef, Scope, std::ref(Name), ObjectFlags);
	}

	bool IsSupportedByBlueprintSansContainers(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return PropertyTypeDispatch<IsSupportedByBlueprintDispatch, false, bool>(PropDef, bMemberVariable);
	}

	template <bool bHandleContainers>
	FPropertyFlagsChanges PostParseFinalizeHelper(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		return PropertyTypeDispatch<PostParseFinalizeDispatch, bHandleContainers, FPropertyFlagsChanges>(PropDef, AddedFlags);
	}
}

/**
 * Every property type is required to implement the follow methods and constants or derive from a base class that has them.
 */
struct FPropertyTypeTraitsBase
{
	/**
	 * Transforms CPP-formated string containing default value, to inner formated string
	 * If it cannot be transformed empty string is returned.
	 *
	 * @param Property The property that owns the default value.
	 * @param CppForm A CPP-formated string.
	 * @param out InnerForm Inner formated string
	 * @return true on success, false otherwise.
	 */
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		return false;
	}

	/**
	 * Given a property definition with the property base data already populated, create the underlying engine type.
	 *
	 * NOTE: This method MUST be implemented by each of the property types.
	 *
	 * @param PropDef The definition of the property
	 */
	 //static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)

	/**
	 * Given a property definition create the underlying engine type
	 *
	 * NOTE: This method MUST be implemented by each of the property types.
	 *
	 * @param PropDef The definition of the property
	 * @param Scope The parent object owning the property
	 * @param Name The name of the property
	 * @param ObjectFlags The flags associated with the property
	 * @return The pointer to the newly created property.  It will be attached to the definition by the caller
	 */
	 //static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)

	/**
	 * Returns true if this property is supported by blueprints
	 */
	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return false;
	}

	/**
	 * Return the engine class name for the given property information.
	 * @param PropDef The property in question
	 * @return The name of the engine property that will represent this definition.
	 */
	 //static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef);

	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param PropDef The definition of the property
	 * @param ExtendedTypeText for property types which use templates, will be filled in with the type
	 * @param CPPExportFlags flags for modifying the behavior of the export
	 */
	//static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags);

	//static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef);

	//static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText);

	/**
	 * Handle any final initialization after parsing has completed
	 *
	 * @param PropDef The property in question
	 * @param AddedFlags Flags added to the property that need to be propagated to the map key type
	 * @return Collection of properties added and removed
	 */
	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		return FPropertyFlagsChanges{ CPF_None, CPF_None };
	}
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Numeric types
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------

struct FPropertyTypeTraitsNumericBase : public FPropertyTypeTraitsBase
{
	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = TEXT("F");
		ExtendedTypeText += PropDef.GetEngineClassName();
		return TEXT("PROPERTY");
	}
};

struct FPropertyTypeTraitsByte : public FPropertyTypeTraitsNumericBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		int32 Value;
		if (FDefaultValueHelper::ParseInt(CppForm, Value))
		{
			OutForm = FString::FromInt(Value);
			return (0 <= Value) && (255 >= Value);
		}
		return false;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
#if UHT_ENABLE_VALUE_PROPERTY_TAG
		PropDef.GetUnrealSourceFile().AddTypeDefIncludeIfNeeded(VarProperty.EnumDef);
#endif
		if (VarProperty.EnumDef)
		{
			VarProperty.EnumDef->AddReferencingProperty(PropDef);
		}
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FByteProperty* Result = new FByteProperty(Scope, Name, ObjectFlags);
		Result->Enum = VarProperty.EnumDef ? VarProperty.EnumDef->GetEnum() : nullptr;
		check(VarProperty.IntType == EIntType::Sized);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FByteProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		if (FUnrealEnumDefinitionInfo* EnumDef = PropDef.GetPropertyBase().AsEnum())
		{
			const bool bEnumClassForm = EnumDef->GetCppForm() == UEnum::ECppForm::EnumClass;
			const bool bNonNativeEnum = false; // Enum->GetClass() != UEnum::StaticClass(); // cannot use RF_Native flag, because in UHT the flag is not set
			const bool bRawParam = (CPPExportFlags & CPPF_ArgumentOrReturnValue)
				&& ((PropDef.HasAnyPropertyFlags(CPF_ReturnParm) || !PropDef.HasAnyPropertyFlags(CPF_OutParm))
					|| bNonNativeEnum);
			const bool bConvertedCode = (CPPExportFlags & CPPF_BlueprintCppBackend) && bNonNativeEnum;

			FString FullyQualifiedEnumName;
			if (!EnumDef->GetCppType().IsEmpty())
			{
				FullyQualifiedEnumName = EnumDef->GetCppType();
			}
			else
			{
				// This would give the wrong result if it's a namespaced type and the CppType hasn't
				// been set, but we do this here in case existing code relies on it... somehow.
				if ((CPPExportFlags & CPPF_BlueprintCppBackend) && bNonNativeEnum)
				{
					FullyQualifiedEnumName = ::UnicodeToCPPIdentifier(EnumDef->GetName(), false, TEXT("E__"));
				}
				else
				{
					FullyQualifiedEnumName = EnumDef->GetName();
				}
			}

			if (bEnumClassForm || bRawParam || bConvertedCode)
			{
				return FullyQualifiedEnumName;
			}
			else
			{
				return FString::Printf(TEXT("TEnumAsByte<%s>"), *FullyQualifiedEnumName);
			}
		}
		return TEXT("uint8");
	}
};

struct FPropertyTypeTraitsInt8 : public FPropertyTypeTraitsNumericBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FInt8Property* Result = new FInt8Property(Scope, Name, ObjectFlags);
		check(VarProperty.IntType == EIntType::Sized);
		return Result;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FInt8Property::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("int8");
	}
};

struct FPropertyTypeTraitsInt16 : public FPropertyTypeTraitsNumericBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FInt16Property* Result = new FInt16Property(Scope, Name, ObjectFlags);
		check(VarProperty.IntType == EIntType::Sized);
		return Result;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FInt16Property::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("int16");
	}
};

struct FPropertyTypeTraitsInt : public FPropertyTypeTraitsNumericBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		int32 Value;
		if (FDefaultValueHelper::ParseInt(CppForm, Value))
		{
			OutForm = FString::FromInt(Value);
			return true;
		}
		return false;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		PropDef.SetUnsized(VarProperty.IntType == EIntType::Unsized);
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FIntProperty* Result = new FIntProperty(Scope, Name, ObjectFlags);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FIntProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("int32");
	}
};

struct FPropertyTypeTraitsInt64 : public FPropertyTypeTraitsNumericBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		int64 Value;
		if (FDefaultValueHelper::ParseInt64(CppForm, Value))
		{
			OutForm = FString::Printf(TEXT("%lld"), Value);
			return true;
		}
		return false;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FInt64Property* Result = new FInt64Property(Scope, Name, ObjectFlags);
		check(VarProperty.IntType == EIntType::Sized);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FInt64Property::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("int64");
	}
};

struct FPropertyTypeTraitsUInt16 : public FPropertyTypeTraitsNumericBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUInt16Property* Result = new FUInt16Property(Scope, Name, ObjectFlags);
		check(VarProperty.IntType == EIntType::Sized);
		return Result;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FUInt16Property::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("uint16");
	}
};

struct FPropertyTypeTraitsUInt32 : public FPropertyTypeTraitsNumericBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		PropDef.SetUnsized(VarProperty.IntType == EIntType::Unsized);
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FUInt32Property* Result = new FUInt32Property(Scope, Name, ObjectFlags);
		return Result;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FUInt32Property::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("uint32");
	}
};

struct FPropertyTypeTraitsUInt64 : public FPropertyTypeTraitsNumericBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUInt64Property* Result = new FUInt64Property(Scope, Name, ObjectFlags);
		check(VarProperty.IntType == EIntType::Sized);
		return Result;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FUInt64Property::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("uint64");
	}
};

struct FPropertyTypeTraitsFloat : public FPropertyTypeTraitsNumericBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		float Value;
		if (FDefaultValueHelper::ParseFloat(CppForm, Value))
		{
			OutForm = FString::Printf(TEXT("%f"), Value);
			return true;
		}
		return false;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FFloatProperty* Result = new FFloatProperty(Scope, Name, ObjectFlags);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FFloatProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("float");
	}
};

struct FPropertyTypeTraitsDouble : public FPropertyTypeTraitsNumericBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		double Value;
		if (FDefaultValueHelper::ParseDouble(CppForm, Value))
		{
			OutForm = FString::Printf(TEXT("%f"), Value);
			return true;
		}
		return false;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FDoubleProperty* Result = new FDoubleProperty(Scope, Name, ObjectFlags);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FDoubleProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("double");
	}
};

struct FPropertyTypeTraitsLargeWorldCoordinatesReal : public FPropertyTypeTraitsNumericBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		double Value;
		if (FDefaultValueHelper::ParseDouble(CppForm, Value))
		{
			OutForm = FString::Printf(TEXT("%f"), Value);
			return true;
		}
		return false;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FLargeWorldCoordinatesRealProperty* Result = new FLargeWorldCoordinatesRealProperty(Scope, Name, ObjectFlags);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FLargeWorldCoordinatesRealProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return TEXT("double");
	}
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Boolean types
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------

/**
 * Base implementation for all boolean property types
 */
struct FPropertyTypeTraitsBooleanBase : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	template <typename SizeType, bool bIsNativeBool>
	static FProperty* CreateEngineTypeHelper(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FBoolProperty* Result = new FBoolProperty(Scope, Name, ObjectFlags);
		bool bActsLikeNativeBool = bIsNativeBool || PropDef.GetVariableCategory() == EVariableCategory::Return;
		Result->SetBoolSize(bActsLikeNativeBool ? sizeof(bool) : sizeof(SizeType), bActsLikeNativeBool);
		return Result;
	}

	static FString GetCPPTypeHelper(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags, bool bIsNativeBool, const TCHAR* SizeText)
	{
		if (bIsNativeBool
			|| ((CPPExportFlags & (CPPF_Implementation | CPPF_ArgumentOrReturnValue)) == (CPPF_Implementation | CPPF_ArgumentOrReturnValue))
			|| ((CPPExportFlags & CPPF_BlueprintCppBackend) != 0))
		{
			// Export as bool if this is actually a bool or it's being exported as a return value of C++ function definition.
			return TEXT("bool");
		}
		else
		{
			return SizeText;
		}
	}

	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		if (FDefaultValueHelper::Is(CppForm, TEXT("true")) ||
			FDefaultValueHelper::Is(CppForm, TEXT("false")))
		{
			OutForm = FDefaultValueHelper::RemoveWhitespaces(CppForm);
			return true;
		}
		return false;
	}


	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FBoolProperty::StaticClass()->GetName();
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString();
	}
};

struct FPropertyTypeTraitsBool : public FPropertyTypeTraitsBooleanBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		return CreateEngineTypeHelper<bool, true>(PropDef, Scope, Name, ObjectFlags);
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return GetCPPTypeHelper(PropDef, ExtendedTypeText, CPPExportFlags, true, TEXT("bool"));
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		return TEXT("UBOOL");
	}
};

struct FPropertyTypeTraitsBool8 : public FPropertyTypeTraitsBooleanBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		return CreateEngineTypeHelper<uint8, false>(PropDef, Scope, Name, ObjectFlags);
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return GetCPPTypeHelper(PropDef, ExtendedTypeText, CPPExportFlags, false, TEXT("uint8"));
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		return TEXT("UBOOL8");
	}
};

struct FPropertyTypeTraitsBool16 : public FPropertyTypeTraitsBooleanBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		return CreateEngineTypeHelper<uint16, false>(PropDef, Scope, Name, ObjectFlags);
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return GetCPPTypeHelper(PropDef, ExtendedTypeText, CPPExportFlags, false, TEXT("uint16"));
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		return TEXT("UBOOL16");
	}
};

struct FPropertyTypeTraitsBool32 : public FPropertyTypeTraitsBooleanBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		return CreateEngineTypeHelper<uint32, false>(PropDef, Scope, Name, ObjectFlags);
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return GetCPPTypeHelper(PropDef, ExtendedTypeText, CPPExportFlags, false, TEXT("uint32"));
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		return TEXT("UBOOL32");
	}
};

struct FPropertyTypeTraitsBool64 : public FPropertyTypeTraitsBooleanBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		return CreateEngineTypeHelper<uint64, false>(PropDef, Scope, Name, ObjectFlags);
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return GetCPPTypeHelper(PropDef, ExtendedTypeText, CPPExportFlags, false, TEXT("uint64"));
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		return TEXT("UBOOL64");
	}
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Enumeration types
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------

struct FPropertyTypeTraitsEnum : public FPropertyTypeTraitsBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		FUnrealEnumDefinitionInfo* EnumDef = PropDef.GetPropertyBase().EnumDef;
		OutForm = FDefaultValueHelper::GetUnqualifiedEnumValue(FDefaultValueHelper::RemoveWhitespaces(CppForm));

		const int32 EnumEntryIndex = EnumDef->GetIndexByName(*OutForm);
		if (EnumEntryIndex == INDEX_NONE)
		{
			return false;
		}
		if (EnumDef->HasMetaData(TEXT("Hidden"), EnumEntryIndex))
		{
			PropDef.Throwf(TEXT("Hidden enum entries cannot be used as default values: %s \"%s\" "), *PropDef.GetName(), *CppForm);
		}
		return true;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		if (VarProperty.EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			check(VarProperty.Type == EPropertyType::CPT_Byte);
			return FPropertyTypeTraitsByte::CreateProperty(PropDef);
		}

		VarProperty.EnumDef->AddReferencingProperty(PropDef);

#if UHT_ENABLE_VALUE_PROPERTY_TAG
		PropDef.GetUnrealSourceFile().AddTypeDefIncludeIfNeeded(VarProperty.EnumDef);
#endif

		FPropertyBase UnderlyingProperty = VarProperty;
		UnderlyingProperty.EnumDef = nullptr;
		UnderlyingProperty.PropertyFlags = EPropertyFlags::CPF_None;
		UnderlyingProperty.ArrayType = EArrayType::None;
		UnderlyingProperty.RepNotifyName = NAME_None;
		UnderlyingProperty.MetaData.Empty();
		switch (VarProperty.EnumDef->GetUnderlyingType())
		{
		case EUnderlyingEnumType::int8:        UnderlyingProperty.Type = CPT_Int8;   break;
		case EUnderlyingEnumType::int16:       UnderlyingProperty.Type = CPT_Int16;  break;
		case EUnderlyingEnumType::int32:       UnderlyingProperty.Type = CPT_Int;    break;
		case EUnderlyingEnumType::int64:       UnderlyingProperty.Type = CPT_Int64;  break;
		case EUnderlyingEnumType::uint8:       UnderlyingProperty.Type = CPT_Byte;   break;
		case EUnderlyingEnumType::uint16:      UnderlyingProperty.Type = CPT_UInt16; break;
		case EUnderlyingEnumType::uint32:      UnderlyingProperty.Type = CPT_UInt32; break;
		case EUnderlyingEnumType::uint64:      UnderlyingProperty.Type = CPT_UInt64; break;
		case EUnderlyingEnumType::Unspecified:
			UnderlyingProperty.Type = CPT_Int;
			UnderlyingProperty.IntType = EIntType::Unsized;
			break;

		default:
			check(false);
		}

		PropDef.SetValuePropDef(FPropertyTraits::CreateProperty(UnderlyingProperty, PropDef, TEXT("UnderlyingType"), 
			PropDef.GetVariableCategory(), ACCESS_Public, PropDef.GetArrayDimensions(), PropDef.GetUnrealSourceFile(), PropDef.GetLineNumber(), PropDef.GetParsePosition()));
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		if (VarProperty.EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			check(VarProperty.Type == EPropertyType::CPT_Byte);
			return FPropertyTypeTraitsByte::CreateEngineType(PropDef, Scope, Name, ObjectFlags);
		}

		FEnumProperty* Result = new FEnumProperty(Scope, Name, ObjectFlags);
		PropDef.SetProperty(Result);
		Result->UnderlyingProp = CastFieldChecked<FNumericProperty>(FPropertyTraits::CreateEngineType(PropDef.GetValuePropDefRef()));
		Result->Enum = VarProperty.EnumDef->GetEnum();
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		if (VarProperty.EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			return FPropertyTypeTraitsByte::GetEngineClassName(PropDef);
		}
		else
		{
			return FEnumProperty::StaticClass()->GetName();
		}
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUnrealEnumDefinitionInfo* EnumDef = VarProperty.EnumDef;
		if (EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			return FPropertyTypeTraitsByte::GetCPPType(PropDef, ExtendedTypeText, CPPExportFlags);
		}
		else
		{
			const bool bNonNativeEnum = false; // Enum->GetClass() != UEnum::StaticClass(); // cannot use RF_Native flag, because in UHT the flag is not set

			if (!EnumDef->GetCppType().IsEmpty())
			{
				return EnumDef->GetCppType();
			}

			FString EnumName = EnumDef->GetName();

			// This would give the wrong result if it's a namespaced type and the CppType hasn't
			// been set, but we do this here in case existing code relies on it... somehow.
			if ((CPPExportFlags & CPPF_BlueprintCppBackend) && bNonNativeEnum)
			{
				FString Result = ::UnicodeToCPPIdentifier(EnumName, false, TEXT("E__"));
				return Result;
			}

			return EnumName;
		}
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		if (VarProperty.EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			return FPropertyTypeTraitsByte::GetCPPTypeForwardDeclaration(PropDef);
		}

		return FString::Printf(TEXT("enum class %s : %s;"), *VarProperty.EnumDef->GetName(), *PropDef.GetValuePropDef().GetCPPType());
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUnrealEnumDefinitionInfo* EnumDef = VarProperty.EnumDef;
		if (EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			return FPropertyTypeTraitsByte::GetCPPMacroType(PropDef, ExtendedTypeText);
		}
		ExtendedTypeText = EnumDef->GetName();
		return TEXT("ENUM");
	}
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Object types
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------

/**
 * Base class for all object based property types
 */
struct FPropertyTypeTraitsObjectBase : public FPropertyTypeTraitsBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		const bool bIsNull = FDefaultValueHelper::Is(CppForm, TEXT("NULL")) || FDefaultValueHelper::Is(CppForm, TEXT("nullptr")) || FDefaultValueHelper::Is(CppForm, TEXT("0"));
		if (bIsNull)
		{
			OutForm = TEXT("None");
		}
		return bIsNull; // always return as null is the only the processing we can do for object defaults
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		VarProperty.ClassDef->AddReferencingProperty(PropDef);

#if UHT_ENABLE_PTR_PROPERTY_TAG
		PropDef.GetUnrealSourceFile().AddTypeDefIncludeIfNeeded(VarProperty.ClassDef);
#endif
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		// Inherit instancing flags
		if (VarProperty.ClassDef->HierarchyHasAnyClassFlags(CLASS_DefaultToInstanced))
		{
			VarProperty.PropertyFlags |= (CPF_InstancedReference | CPF_ExportObject) & (~VarProperty.DisallowFlags);
		}
		AddedFlags |= Detector.Changes().Added;
		return Detector.Changes();
	}
};

struct FPropertyTypeTraitsObjectReference : public FPropertyTypeTraitsObjectBase
{
	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyTypeTraitsObjectBase::PostParseFinalize(PropDef, AddedFlags);

		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
		}
		else
		{
			if (VarProperty.ClassDef->HierarchyHasAnyClassFlags(CLASS_DefaultToInstanced))
			{
				VarProperty.PropertyFlags |= CPF_InstancedReference;
				AddEditInlineMetaData(VarProperty.MetaData);
			}
		}
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			FClassProperty* Result = new FClassProperty(Scope, Name, ObjectFlags);
			Result->MetaClass = VarProperty.MetaClassDef ? VarProperty.MetaClassDef->GetClass() : nullptr;
			Result->PropertyClass = VarProperty.ClassDef->GetClass();
			return Result;
		}
		else
		{
			FObjectProperty* Result = new FObjectProperty(Scope, Name, ObjectFlags);
			Result->PropertyClass = VarProperty.ClassDef->GetClass();
			return Result;
		}
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FClassProperty::StaticClass()->GetName();
		}
		else
		{
			return FObjectProperty::StaticClass()->GetName();
		}
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			if (PropDef.HasAnyPropertyFlags(CPF_UObjectWrapper))
			{
				return FString::Printf(TEXT("TSubclassOf<%s%s> "), VarProperty.MetaClassDef->GetPrefixCPP(), *VarProperty.MetaClassDef->GetName());
			}
			else
			{
				return TEXT("UClass*");
			}
		}
		else
		{
			return FString::Printf(TEXT("%s%s*"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FString::Printf(TEXT("class %s%s;"), VarProperty.MetaClassDef->GetPrefixCPP(), *VarProperty.MetaClassDef->GetName());
		}
		else
		{
			return FString::Printf(TEXT("class %s%s;"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			ExtendedTypeText = TEXT("UClass");
		}
		else
		{
			ExtendedTypeText =  FString::Printf(TEXT("%s%s"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
		return TEXT("OBJECT");
	}
};

struct FPropertyTypeTraitsWeakObjectReference : public FPropertyTypeTraitsObjectBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		FWeakObjectProperty* Result = new FWeakObjectProperty(Scope, Name, ObjectFlags);
		Result->PropertyClass = VarProperty.ClassDef->GetClass();
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return bMemberVariable;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FWeakObjectProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		FString InnerNativeTypeName = FString::Printf(TEXT("%s%s"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		if (PropDef.HasAnyPropertyFlags(CPF_AutoWeak))
		{
			return FString::Printf(TEXT("TAutoWeakObjectPtr<%s>"), *InnerNativeTypeName);
		}
		return FString::Printf(TEXT("TWeakObjectPtr<%s>"), *InnerNativeTypeName);
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		return FString::Printf(TEXT("class %s%s;"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (PropDef.HasAnyPropertyFlags(CPF_AutoWeak))
		{
			ExtendedTypeText = FString::Printf(TEXT("TAutoWeakObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
			return TEXT("AUTOWEAKOBJECT");
		}
		ExtendedTypeText = FString::Printf(TEXT("TWeakObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		return TEXT("WEAKOBJECT");
	}
};

struct FPropertyTypeTraitsLazyObjectReference : public FPropertyTypeTraitsObjectBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		FLazyObjectProperty* Result = new FLazyObjectProperty(Scope, Name, ObjectFlags);
		Result->PropertyClass = VarProperty.ClassDef->GetClass();
		return Result;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FLazyObjectProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		return FString::Printf(TEXT("TLazyObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		return FString::Printf(TEXT("class %s%s;"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		ExtendedTypeText = FString::Printf(TEXT("TLazyObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		return TEXT("LAZYOBJECT");
	}
};

struct FPropertyTypeTraitsObjectPtrReference : public FPropertyTypeTraitsObjectBase
{
	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyTypeTraitsObjectBase::PostParseFinalize(PropDef, AddedFlags);

		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
		}
		else
		{
			if (VarProperty.ClassDef->HierarchyHasAnyClassFlags(CLASS_DefaultToInstanced))
			{
				VarProperty.PropertyFlags |= CPF_InstancedReference;
				AddEditInlineMetaData(VarProperty.MetaData);
			}
		}
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			FClassPtrProperty* Result = new FClassPtrProperty(Scope, Name, ObjectFlags);
			Result->MetaClass = VarProperty.MetaClassDef ? VarProperty.MetaClassDef->GetClass() : nullptr;
			Result->PropertyClass = VarProperty.ClassDef->GetClass();
			return Result;
		}
		else
		{
			FObjectPtrProperty* Result = new FObjectPtrProperty(Scope, Name, ObjectFlags);
			Result->PropertyClass = VarProperty.ClassDef->GetClass();
			return Result;
		}
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FClassPtrProperty::StaticClass()->GetName();
		}
		else
		{
			return FObjectPtrProperty::StaticClass()->GetName();
		}
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FString::Printf(TEXT("TObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
		else if ((CPPExportFlags & CPPF_ArgumentOrReturnValue) == CPPF_ArgumentOrReturnValue)
		{
			return FString::Printf(TEXT("%s%s*"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
		else
		{
			return FString::Printf(TEXT("TObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FString::Printf(TEXT("class %s%s;"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
		else
		{
			return FString::Printf(TEXT("class %s%s;"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			ExtendedTypeText = FString::Printf(TEXT("TObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
		else
		{
			ExtendedTypeText = FString::Printf(TEXT("TObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
		return TEXT("OBJECTPTR");
	}
};

struct FPropertyTypeTraitsSoftObjectReference : public FPropertyTypeTraitsObjectBase
{
	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			FSoftClassProperty* Result = new FSoftClassProperty(Scope, Name, ObjectFlags);
			Result->MetaClass = VarProperty.MetaClassDef ? VarProperty.MetaClassDef->GetClass() : nullptr;
			Result->PropertyClass = VarProperty.ClassDef->GetClass();
			return Result;
		}
		else
		{
			FSoftObjectProperty* Result = new FSoftObjectProperty(Scope, Name, ObjectFlags);
			Result->PropertyClass = VarProperty.ClassDef->GetClass();
			return Result;
		}
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FSoftClassProperty::StaticClass()->GetName();
		}
		else
		{
			return FSoftObjectProperty::StaticClass()->GetName();
		}
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FString::Printf(TEXT("TSoftClassPtr<%s%s> "), VarProperty.MetaClassDef->GetPrefixCPP(), *VarProperty.MetaClassDef->GetName()); //@TODO - This has an extra space in it
		}
		else
		{
			return FString::Printf(TEXT("TSoftObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			return FString::Printf(TEXT("class %s%s;"), VarProperty.MetaClassDef->GetPrefixCPP(), *VarProperty.MetaClassDef->GetName());
		}
		else
		{
			return FString::Printf(TEXT("class %s%s;"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
		}
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
		{
			ExtendedTypeText = FString::Printf(TEXT("TSoftClassPtr<%s%s> "), VarProperty.MetaClassDef->GetPrefixCPP(), *VarProperty.MetaClassDef->GetName()); //@TODO - This has an extra space in it
			return TEXT("SOFTCLASS");
		}
		else
		{
			ExtendedTypeText = FString::Printf(TEXT("TSoftObjectPtr<%s%s>"), VarProperty.ClassDef->GetPrefixCPP(), *VarProperty.ClassDef->GetName());
			return TEXT("SOFTOBJECT");
		}
	}
};

struct FPropertyTypeTraitsInterface : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		FPropertyTypeTraitsObjectBase::CreateProperty(PropDef);
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		return FPropertyTypeTraitsObjectBase::PostParseFinalize(PropDef, AddedFlags);
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.ClassDef);
		check(VarProperty.ClassDef->HasAnyClassFlags(CLASS_Interface));
		FInterfaceProperty* Result = new  FInterfaceProperty(Scope, Name, ObjectFlags);
		Result->InterfaceClass = VarProperty.ClassDef->GetClass();
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FInterfaceProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		if (ExtendedTypeText != NULL)
		{
			FUnrealClassDefinitionInfo* ExportClassDef = PropDef.GetPropertyBase().ClassDef;
			if (0 == (CPPF_BlueprintCppBackend & CPPExportFlags))
			{
				while (ExportClassDef && !ExportClassDef->HasAnyClassFlags(CLASS_Native))
				{
					ExportClassDef = ExportClassDef->GetSuperClass();
				}
			}
			check(ExportClassDef);
			check(ExportClassDef->HasAnyClassFlags(CLASS_Interface) || 0 != (CPPF_BlueprintCppBackend & CPPExportFlags));

			*ExtendedTypeText = FString::Printf(TEXT("<I%s>"), *ExportClassDef->GetName());
		}
		return TEXT("TScriptInterface");
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		FUnrealClassDefinitionInfo* ExportClass = PropDef.GetPropertyBase().ClassDef;
		while (ExportClass && !ExportClass->HasAnyClassFlags(CLASS_Native))
		{
			ExportClass = ExportClass->GetSuperClass();
		}
		check(ExportClass);
		check(ExportClass->HasAnyClassFlags(CLASS_Interface));

		return FString::Printf(TEXT("class I%s;"), *ExportClass->GetName());
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		FUnrealClassDefinitionInfo* ExportClass = PropDef.GetPropertyBase().ClassDef;
		while (ExportClass && !ExportClass->HasAnyClassFlags(CLASS_Native))
		{
			ExportClass = ExportClass->GetSuperClass();
		}
		check(ExportClass);
		check(ExportClass->HasAnyClassFlags(CLASS_Interface));

		ExtendedTypeText = FString::Printf(TEXT("I%s"), *ExportClass->GetName());
		return TEXT("TINTERFACE");
	}
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Other types
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------

struct FPropertyTypeTraitsName : public FPropertyTypeTraitsBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		if (FDefaultValueHelper::Is(CppForm, TEXT("NAME_None")))
		{
			OutForm = TEXT("None");
			return true;
		}
		return FDefaultValueHelper::StringFromCppString(CppForm, TEXT("FName"), OutForm);
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FNameProperty* Result = new FNameProperty(Scope, Name, ObjectFlags);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FNameProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return FString(TEXT("FName"));
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = TEXT("F");
		ExtendedTypeText += PropDef.GetEngineClassName();
		return TEXT("PROPERTY");
	}
};

struct FPropertyTypeTraitsString : public FPropertyTypeTraitsBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		return FDefaultValueHelper::StringFromCppString(CppForm, TEXT("FString"), OutForm);
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FStrProperty* Result = new FStrProperty(Scope, Name, ObjectFlags);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FStrProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return FString(TEXT("FString"));
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = TEXT("F");
		ExtendedTypeText += PropDef.GetEngineClassName();
		return TEXT("PROPERTY");
	}
};

struct FPropertyTypeTraitsText : public FPropertyTypeTraitsBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		// Handle legacy cases of FText::FromString being used as default values
		// These should be replaced with INVTEXT as FText::FromString can produce inconsistent keys
		if (FDefaultValueHelper::StringFromCppString(CppForm, TEXT("FText::FromString"), OutForm))
		{
			PropDef.LogWarning(TEXT("FText::FromString should be replaced with INVTEXT for default parameter values"));
			return true;
		}

		// Parse the potential value into an instance
		FText ParsedText;
		if (FDefaultValueHelper::Is(CppForm, TEXT("FText()")) || FDefaultValueHelper::Is(CppForm, TEXT("FText::GetEmpty()")))
		{
			ParsedText = FText::GetEmpty();
		}
		else
		{
			static const FString UHTDummyNamespace = TEXT("__UHT_DUMMY_NAMESPACE__");

			if (!FTextStringHelper::ReadFromBuffer(*CppForm, ParsedText, *UHTDummyNamespace, nullptr, /*bRequiresQuotes*/true))
			{
				return false;
			}

			// If the namespace of the parsed text matches the default we gave then this was a LOCTEXT macro which we 
			// don't allow in default values as they rely on an external macro that is known to C++ but not to UHT
			// TODO: UHT could parse these if it tracked the current LOCTEXT_NAMESPACE macro as it parsed
			if (TOptional<FString> ParsedTextNamespace = FTextInspector::GetNamespace(ParsedText))
			{
				if (ParsedTextNamespace.GetValue().Equals(UHTDummyNamespace))
				{
					PropDef.Throwf(TEXT("LOCTEXT default parameter values are not supported; use NSLOCTEXT instead: %s \"%s\" "), *PropDef.GetName(), *CppForm);
				}
			}
		}

		// Normalize the default value from the parsed value
		FTextStringHelper::WriteToBuffer(OutForm, ParsedText, /*bRequiresQuotes*/false);
		return true;
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FTextProperty* Result = new FTextProperty(Scope, Name, ObjectFlags);
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FTextProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return FString(TEXT("FText"));
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = TEXT("F");
		ExtendedTypeText += PropDef.GetEngineClassName();
		return TEXT("PROPERTY");
	}
};

struct FPropertyTypeTraitsStruct : public FPropertyTypeTraitsBase
{
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
	{
		// Cache off the struct types, in case we need them later
		static const FUnrealScriptStructDefinitionInfo& VectorStructDef = GTypeDefinitionInfoMap.FindByNameChecked<FUnrealScriptStructDefinitionInfo>(TEXT("Vector"));
		static const FUnrealScriptStructDefinitionInfo& Vector2DStructDef = GTypeDefinitionInfoMap.FindByNameChecked<FUnrealScriptStructDefinitionInfo>(TEXT("Vector2D"));
		static const FUnrealScriptStructDefinitionInfo& RotatorStructDef = GTypeDefinitionInfoMap.FindByNameChecked<FUnrealScriptStructDefinitionInfo>(TEXT("Rotator"));
		static const FUnrealScriptStructDefinitionInfo& LinearColorStructDef = GTypeDefinitionInfoMap.FindByNameChecked<FUnrealScriptStructDefinitionInfo>(TEXT("LinearColor"));
		static const FUnrealScriptStructDefinitionInfo& ColorStructDef = GTypeDefinitionInfoMap.FindByNameChecked<FUnrealScriptStructDefinitionInfo>(TEXT("Color"));

		FUnrealScriptStructDefinitionInfo* ScriptStructDef = PropDef.GetPropertyBase().ScriptStructDef;
		if (ScriptStructDef == &VectorStructDef)
		{
			FString Parameters;
			if (FDefaultValueHelper::Is(CppForm, TEXT("FVector::ZeroVector")))
			{
				return true;
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FVector::UpVector")))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"),
					FVector::UpVector.X, FVector::UpVector.Y, FVector::UpVector.Z);
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FVector::ForwardVector")))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"),
					FVector::ForwardVector.X, FVector::ForwardVector.Y, FVector::ForwardVector.Z);
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FVector::RightVector")))
			{
				OutForm = FString::Printf(TEXT("%f,%f,%f"),
					FVector::RightVector.X, FVector::RightVector.Y, FVector::RightVector.Z);
			}
			else if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector"), Parameters))
			{
				if (FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")))
				{
					return true;
				}
				FVector Vector;
				float Value;
				if (FDefaultValueHelper::ParseVector(Parameters, Vector))
				{
					OutForm = FString::Printf(TEXT("%f,%f,%f"),
						Vector.X, Vector.Y, Vector.Z);
				}
				else if (FDefaultValueHelper::ParseFloat(Parameters, Value))
				{
					OutForm = FString::Printf(TEXT("%f,%f,%f"),
						Value, Value, Value);
				}
			}
		}
		else if (ScriptStructDef == &RotatorStructDef)
		{
			if (FDefaultValueHelper::Is(CppForm, TEXT("FRotator::ZeroRotator")))
			{
				return true;
			}
			FString Parameters;
			if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FRotator"), Parameters))
			{
				if (FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")))
				{
					return true;
				}
				FRotator Rotator;
				if (FDefaultValueHelper::ParseRotator(Parameters, Rotator))
				{
					OutForm = FString::Printf(TEXT("%f,%f,%f"),
						Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
				}
			}
		}
		else if (ScriptStructDef == &Vector2DStructDef)
		{
			if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2D::ZeroVector")))
			{
				return true;
			}
			if (FDefaultValueHelper::Is(CppForm, TEXT("FVector2D::UnitVector")))
			{
				OutForm = FString::Printf(TEXT("(X=%3.3f,Y=%3.3f)"),
					FVector2D::UnitVector.X, FVector2D::UnitVector.Y);
			}
			FString Parameters;
			if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FVector2D"), Parameters))
			{
				if (FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")))
				{
					return true;
				}
				FVector2D Vector2D;
				if (FDefaultValueHelper::ParseVector2D(Parameters, Vector2D))
				{
					OutForm = FString::Printf(TEXT("(X=%3.3f,Y=%3.3f)"),
						Vector2D.X, Vector2D.Y);
				}
			}
		}
		else if (ScriptStructDef == &LinearColorStructDef)
		{
			if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::White")))
			{
				OutForm = FLinearColor::White.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::Gray")))
			{
				OutForm = FLinearColor::Gray.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::Black")))
			{
				OutForm = FLinearColor::Black.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::Transparent")))
			{
				OutForm = FLinearColor::Transparent.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::Red")))
			{
				OutForm = FLinearColor::Red.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::Green")))
			{
				OutForm = FLinearColor::Green.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::Blue")))
			{
				OutForm = FLinearColor::Blue.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FLinearColor::Yellow")))
			{
				OutForm = FLinearColor::Yellow.ToString();
			}
			else
			{
				FString Parameters;
				if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FLinearColor"), Parameters))
				{
					if (FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")))
					{
						return true;
					}
					FLinearColor Color;
					if (FDefaultValueHelper::ParseLinearColor(Parameters, Color))
					{
						OutForm = Color.ToString();
					}
				}
			}
		}
		else if (ScriptStructDef == &ColorStructDef)
		{
			if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::White")))
			{
				OutForm = FColor::White.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::Black")))
			{
				OutForm = FColor::Black.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::Red")))
			{
				OutForm = FColor::Red.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::Green")))
			{
				OutForm = FColor::Green.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::Blue")))
			{
				OutForm = FColor::Blue.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::Yellow")))
			{
				OutForm = FColor::Yellow.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::Cyan")))
			{
				OutForm = FColor::Cyan.ToString();
			}
			else if (FDefaultValueHelper::Is(CppForm, TEXT("FColor::Magenta")))
			{
				OutForm = FColor::Magenta.ToString();
			}
			else
			{
				FString Parameters;
				if (FDefaultValueHelper::GetParameters(CppForm, TEXT("FColor"), Parameters))
				{
					if (FDefaultValueHelper::Is(Parameters, TEXT("ForceInit")))
					{
						return true;
					}
					FColor Color;
					if (FDefaultValueHelper::ParseColor(Parameters, Color))
					{
						OutForm = Color.ToString();
					}
				}
			}
		}
		else
		{
			// Allow a default constructed struct as a default argument
			const FString StructNameCPP = ScriptStructDef->GetAlternateNameCPP();
			FString Parameters;
			if (FDefaultValueHelper::GetParameters(CppForm, StructNameCPP, Parameters))
			{
				if (Parameters.IsEmpty())
				{
					OutForm = TEXT("()");
				}
			}
		}
		return !OutForm.IsEmpty();
	}

	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUnrealScriptStructDefinitionInfo* ScriptStructDef = VarProperty.ScriptStructDef;
		ScriptStructDef->AddReferencingProperty(PropDef);

#if UHT_ENABLE_VALUE_PROPERTY_TAG
		PropDef.GetUnrealSourceFile().AddTypeDefIncludeIfNeeded(ScriptStructDef);
#endif
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		// We always add regardless of disallow
		if (VarProperty.ScriptStructDef->HasAnyStructFlags(STRUCT_HasInstancedReference))
		{
			VarProperty.PropertyFlags |= CPF_ContainsInstancedReference;
		}
		AddedFlags |= Detector.Changes().Added;
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FStructProperty* Result = new FStructProperty(Scope, Name, ObjectFlags);
		Result->Struct = PropDef.GetPropertyBase().ScriptStructDef->GetScriptStruct();
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		return VarProperty.ScriptStructDef->GetBoolMetaDataHierarchical(FHeaderParserNames::NAME_BlueprintType);
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FStructProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return FString::Printf(TEXT("F%s"), *PropDef.GetPropertyBase().ScriptStructDef->GetName());
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		// Core type structs don't need to forward declare in UHT as every generated.h indirectly includes CoreMinimal.h
		if (UScriptStruct::ICppStructOps* CppStructOps = UScriptStruct::FindDeferredCppStructOps(PropDef.GetPropertyBase().ScriptStructDef->GetStructPathName()); CppStructOps != nullptr && CppStructOps->IsUECoreType())
		{
			return FString();
		}
			
		return FString::Printf(TEXT("struct F%s;"), *PropDef.GetPropertyBase().ScriptStructDef->GetName());
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = GetCPPType(PropDef, nullptr, CPPF_None);
		return TEXT("STRUCT");
	}
};

struct FPropertyTypeTraitsDelegate : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		VarProperty.PropertyFlags |= CPF_InstancedReference & (~VarProperty.DisallowFlags);
		AddedFlags |= Detector.Changes().Added;
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.FunctionDef);

		FDelegateProperty* Result = new FDelegateProperty(Scope, Name, ObjectFlags);
		Result->SignatureFunction = VarProperty.FunctionDef->GetFunction();
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FDelegateProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.FunctionDef);

		FString UnmangledFunctionName = VarProperty.FunctionDef->GetName().LeftChop(FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len());
		const bool bBlueprintCppBackend = (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_BlueprintCppBackend));
		const bool bNative = VarProperty.FunctionDef->IsNative();
		if (bBlueprintCppBackend && bNative)
		{
			if (FUnrealStructDefinitionInfo* StructOwnerDef = UHTCast<FUnrealStructDefinitionInfo>(VarProperty.FunctionDef->GetOuter()))
			{
				return FString::Printf(TEXT("%s%s::F%s"), StructOwnerDef->GetPrefixCPP(), *StructOwnerDef->GetName(), *UnmangledFunctionName);
			}
		}
		else
		{
			FUnrealClassDefinitionInfo* OwnerClassDef = VarProperty.FunctionDef->GetOwnerClass();
			const bool NonNativeClassOwner = OwnerClassDef && !OwnerClassDef->HasAnyClassFlags(CLASS_Native);
			if (bBlueprintCppBackend && NonNativeClassOwner)
			{
				// The name must be valid, this removes spaces, ?, etc from the user's function name. It could
				// be slightly shorter because the postfix ("__pf") is not needed here because we further post-
				// pend to the string. Normally the postfix is needed to make sure we don't mangle to a valid
				// identifier and collide:
				UnmangledFunctionName = UnicodeToCPPIdentifier(UnmangledFunctionName, false, TEXT(""));
				// the name must be unique
				const FString OwnerName = UnicodeToCPPIdentifier(OwnerClassDef->GetName(), false, TEXT(""));
				const FString NewUnmangledFunctionName = FString::Printf(TEXT("%s__%s"), *UnmangledFunctionName, *OwnerName);
				UnmangledFunctionName = NewUnmangledFunctionName;
			}
			if (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_CustomTypeName))
			{
				UnmangledFunctionName += TEXT("__SinglecastDelegate");
			}
		}
		return FString(TEXT("F")) + UnmangledFunctionName;
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = TEXT("F");
		ExtendedTypeText += PropDef.GetEngineClassName();
		return TEXT("PROPERTY");
	}
};

struct FPropertyTypeTraitsMulticastDelegate : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		VarProperty.PropertyFlags |= CPF_InstancedReference & (~VarProperty.DisallowFlags);
		AddedFlags |= Detector.Changes().Added;
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		if (VarProperty.FunctionDef->GetFunctionType() == EFunctionType::SparseDelegate)
		{
			FMulticastSparseDelegateProperty* Result = new FMulticastSparseDelegateProperty(Scope, Name, ObjectFlags);
			Result->SignatureFunction = VarProperty.FunctionDef->GetFunction();
			return Result;
		}
		else
		{
			FMulticastInlineDelegateProperty* Result = new FMulticastInlineDelegateProperty(Scope, Name, ObjectFlags);
			Result->SignatureFunction = VarProperty.FunctionDef->GetFunction();
			return Result;
		}
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return bMemberVariable;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		if (VarProperty.FunctionDef->GetFunctionType() == EFunctionType::SparseDelegate)
		{
			return FMulticastSparseDelegateProperty::StaticClass()->GetName();
		}
		else
		{
			return FMulticastInlineDelegateProperty::StaticClass()->GetName();
		}
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		check(VarProperty.FunctionDef);

		// We have this test because sometimes the delegate hasn't been set up by FixupDelegateProperties at the time
		// we need the type for an error message.  We deliberately format it so that it's unambiguously not CPP code, but is still human-readable.
		if (!VarProperty.FunctionDef)
		{
			return FString(TEXT("{multicast delegate type}"));
		}

		FString UnmangledFunctionName = VarProperty.FunctionDef->GetName().LeftChop(FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len());
		FUnrealClassDefinitionInfo* OwnerClassDef = VarProperty.FunctionDef->GetOwnerClass();

		const bool bBlueprintCppBackend = (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_BlueprintCppBackend));
		const bool bNative = VarProperty.FunctionDef->IsNative();
		if (bBlueprintCppBackend && bNative)
		{
			if (FUnrealStructDefinitionInfo* StructOwnerDef = UHTCast<FUnrealStructDefinitionInfo>(VarProperty.FunctionDef->GetOuter()))
			{
				return FString::Printf(TEXT("%s%s::F%s"), StructOwnerDef->GetPrefixCPP(), *StructOwnerDef->GetName(), *UnmangledFunctionName);
			}
		}
		else
		{
			if ((0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_BlueprintCppBackend)) && OwnerClassDef && !OwnerClassDef->HasAnyClassFlags(CLASS_Native))
			{
				// The name must be valid, this removes spaces, ?, etc from the user's function name. It could
				// be slightly shorter because the postfix ("__pf") is not needed here because we further post-
				// pend to the string. Normally the postfix is needed to make sure we don't mangle to a valid
				// identifier and collide:
				UnmangledFunctionName = UnicodeToCPPIdentifier(UnmangledFunctionName, false, TEXT(""));
				// the name must be unique
				const FString OwnerName = UnicodeToCPPIdentifier(OwnerClassDef->GetName(), false, TEXT(""));
				const FString NewUnmangledFunctionName = FString::Printf(TEXT("%s__%s"), *UnmangledFunctionName, *OwnerName);
				UnmangledFunctionName = NewUnmangledFunctionName;
			}
			if (0 != (CPPExportFlags & EPropertyExportCPPFlags::CPPF_CustomTypeName))
			{
				UnmangledFunctionName += TEXT("__MulticastDelegate");
			}
		}
		return FString(TEXT("F")) + UnmangledFunctionName;
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = TEXT("F");
		ExtendedTypeText += PropDef.GetEngineClassName();
		return TEXT("PROPERTY");
	}
};

struct FPropertyTypeTraitsFieldPath : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FFieldPathProperty* Result = new FFieldPathProperty(Scope, Name, ObjectFlags);
		FFieldClass** FieldClass = FFieldClass::GetNameToFieldClassMap().Find(VarProperty.FieldClassName);
		check(FieldClass);
		Result->PropertyClass = *FieldClass;
		return Result;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return true;
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FFieldPathProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		if (ExtendedTypeText != nullptr)
		{
			const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
			FString& InnerTypeText = *ExtendedTypeText;
			InnerTypeText = TEXT("<F");
			InnerTypeText += VarProperty.FieldClassName.ToString();
			InnerTypeText += TEXT(">");
		}
		return TEXT("TFieldPath");
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString::Printf(TEXT("class F%s;"), *PropDef.GetPropertyBase().FieldClassName.ToString());
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		const FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		ExtendedTypeText = FString::Printf(TEXT("TFieldPath<F%s>"), *VarProperty.FieldClassName.ToString());
		return TEXT("TFIELDPATH");
	}
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Container types
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------

struct FPropertyTypeTraitsStaticArray : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		CreatePropertyHelper<false>(PropDef);
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		if (PropDef.GetArrayDimensions())
		{
			FString Temp = PropDef.GetArrayDimensions();

			bool bAgain;
			do
			{
				bAgain = false;

				// Remove any casts
				static const TCHAR* Casts[] = {
					TEXT("(uint32)"),
					TEXT("(int32)"),
					TEXT("(uint16)"),
					TEXT("(int16)"),
					TEXT("(uint8)"),
					TEXT("(int8)"),
					TEXT("(int)"),
					TEXT("(unsigned)"),
					TEXT("(signed)"),
					TEXT("(unsigned int)"),
					TEXT("(signed int)")
				};
				
				// Remove any irrelevant whitespace
				Temp.TrimStartAndEndInline();

				// Remove any brackets
				if (Temp.Len() > 0 && Temp[0] == TEXT('('))
				{
					int32 TempLen = Temp.Len();
					int32 ClosingParen = FindMatchingClosingParenthesis(Temp);
					if (ClosingParen == TempLen - 1)
					{
						Temp.MidInline(1, TempLen - 2, false);
						bAgain = true;
					}
				}

				for (const TCHAR* Cast : Casts)
				{
					if (Temp.StartsWith(Cast, ESearchCase::CaseSensitive))
					{
						Temp.RightChopInline(FCString::Strlen(Cast), false);
						bAgain = true;
					}
				}
			} while (bAgain);

			if (Temp.Len() > 0 && !FChar::IsDigit(Temp[0]))
			{
				FUnrealEnumDefinitionInfo* EnumDef = GetEnumLookup().Find(*Temp);

				if (!EnumDef)
				{
					// If the enum wasn't declared in this scope, then try to find it anywhere we can
					EnumDef = GTypeDefinitionInfoMap.FindByName<FUnrealEnumDefinitionInfo>(*Temp);
				}

				if (EnumDef)
				{
					// set the ArraySizeEnum if applicable
					PropDef.GetPropertyBase().MetaData.Add(NAME_ArraySizeEnum, EnumDef->GetPathName());
				}
			}
		}

		return PostParseFinalizeHelper<false>(PropDef, AddedFlags);
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FProperty* Property = CreateEngineTypeHelper<false>(PropDef, Scope, Name, ObjectFlags);
		Property->ArrayDim = 2;
		return Property;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return IsSupportedByBlueprintSansContainers(PropDef, bMemberVariable);
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return PropertyTypeDispatch<GetEngineClassNameDispatch, false, FString>(PropDef);
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		return PropertyTypeDispatch<GetCPPTypeDispatch, false, FString>(PropDef, ExtendedTypeText, CPPExportFlags);
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return PropertyTypeDispatch<GetCPPTypeForwardDeclarationDispatch, false, FString>(PropDef);
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		return PropertyTypeDispatch<GetCPPMacroTypeDispatch, false, FString>(PropDef, ExtendedTypeText);
	}
};

struct FPropertyTypeTraitsDynamicArray : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		FPropertyBase InnerVarProperty = PropDef.GetPropertyBase();
		InnerVarProperty.ArrayType = EArrayType::None;
		InnerVarProperty.RepNotifyName = NAME_None;
		InnerVarProperty.MetaData.Empty();
		PropDef.SetValuePropDef(FPropertyTraits::CreateProperty(InnerVarProperty, PropDef, PropDef.GetFName(), 
			PropDef.GetVariableCategory(), ACCESS_Public, PropDef.GetArrayDimensions(), PropDef.GetUnrealSourceFile(), PropDef.GetLineNumber(), PropDef.GetParsePosition()));

		FUnrealPropertyDefinitionInfo& ValuePropDef = PropDef.GetValuePropDef();
		VarProperty.PropertyFlags = ValuePropDef.GetPropertyBase().PropertyFlags;
		ValuePropDef.GetPropertyBase().PropertyFlags &= CPF_PropagateToArrayInner;
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUnrealPropertyDefinitionInfo& ValuePropDef = PropDef.GetValuePropDef();

		EPropertyFlags ValueAddedFlags = CPF_None;
		VarProperty.PropertyFlags |= PostParseFinalizeHelper<true>(ValuePropDef, ValueAddedFlags).Added;

		VarProperty.MetaData.Append(MoveTemp(ValuePropDef.GetPropertyBase().MetaData)); // Move any added meta data items to the container
		PropDef.SetAllocatorType(VarProperty.AllocatorType);

		PropagateFlagsFromInnerAndHandlePersistentInstanceMetadata(VarProperty.PropertyFlags, PropDef.GetPropertyBase().MetaData, ValuePropDef);
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		FArrayProperty* Array = new FArrayProperty(Scope, Name, ObjectFlags);
		PropDef.SetProperty(Array);
		Array->Inner = FPropertyTraits::CreateEngineType(PropDef.GetValuePropDefRef());
		return Array;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return IsSupportedByBlueprintSansContainers(PropDef.GetValuePropDef(), false);
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FArrayProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		if (ExtendedTypeText != NULL)
		{
			FString InnerExtendedTypeText;
			FString InnerTypeText = PropDef.GetValuePropDef().GetCPPType(&InnerExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider array inners to be "arguments or return values"
			if (InnerExtendedTypeText.Len() && InnerExtendedTypeText.Right(1) == TEXT(">"))
			{
				// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
				InnerExtendedTypeText += TEXT(" ");
			}
			else if (!InnerExtendedTypeText.Len() && InnerTypeText.Len() && InnerTypeText.Right(1) == TEXT(">"))
			{
				// if our internal property type is a template class, add a space between the closing brackets b/c VS.NET cannot parse this correctly
				InnerExtendedTypeText += TEXT(" ");
			}
			*ExtendedTypeText = FString::Printf(TEXT("<%s%s>"), *InnerTypeText, *InnerExtendedTypeText);
		}
		return TEXT("TArray");
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return PropDef.GetValuePropDef().GetCPPTypeForwardDeclaration();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = PropDef.GetValuePropDef().GetCPPType();
		return TEXT("TARRAY");
	}
};

struct FPropertyTypeTraitsSet : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		FPropertyBase InnerVarProperty = PropDef.GetPropertyBase();
		InnerVarProperty.ArrayType = EArrayType::None;
		InnerVarProperty.RepNotifyName = NAME_None;
		InnerVarProperty.MetaData.Empty();
		PropDef.SetValuePropDef(FPropertyTraits::CreateProperty(InnerVarProperty, PropDef, PropDef.GetFName(),
			PropDef.GetVariableCategory(), ACCESS_Public, PropDef.GetArrayDimensions(), PropDef.GetUnrealSourceFile(), PropDef.GetLineNumber(), PropDef.GetParsePosition()));

		FUnrealPropertyDefinitionInfo& ValuePropDef = PropDef.GetValuePropDef();
		VarProperty.PropertyFlags = ValuePropDef.GetPropertyBase().PropertyFlags;
		ValuePropDef.GetPropertyBase().PropertyFlags &= CPF_PropagateToSetElement;
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUnrealPropertyDefinitionInfo& ValuePropDef = PropDef.GetValuePropDef();

		EPropertyFlags ValueAddedFlags = CPF_None;
		VarProperty.PropertyFlags |= PostParseFinalizeHelper<true>(ValuePropDef, ValueAddedFlags).Added;

		VarProperty.MetaData.Append(MoveTemp(ValuePropDef.GetPropertyBase().MetaData)); // Move any added meta data items to the container

		PropagateFlagsFromInnerAndHandlePersistentInstanceMetadata(VarProperty.PropertyFlags, PropDef.GetPropertyBase().MetaData, ValuePropDef);
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		FSetProperty* Set = new FSetProperty(Scope, Name, ObjectFlags);
		PropDef.SetProperty(Set);
		Set->ElementProp = FPropertyTraits::CreateEngineType(PropDef.GetValuePropDefRef());
		return Set;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return IsSupportedByBlueprintSansContainers(PropDef.GetValuePropDef(), false);
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FSetProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		if (ExtendedTypeText != NULL)
		{
			FString ElementExtendedTypeText;
			FString ElementTypeText = PropDef.GetValuePropDef().GetCPPType(&ElementExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider set values to be "arguments or return values"

			// if property type is a template class, add a space between the closing brackets
			if ((ElementExtendedTypeText.Len() && ElementExtendedTypeText.Right(1) == TEXT(">"))
				|| (!ElementExtendedTypeText.Len() && ElementTypeText.Len() && ElementTypeText.Right(1) == TEXT(">")))
			{
				ElementExtendedTypeText += TEXT(" ");
			}

			*ExtendedTypeText = FString::Printf(TEXT("<%s%s>"), *ElementTypeText, *ElementExtendedTypeText);
		}
		return TEXT("TSet");
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return PropDef.GetValuePropDef().GetCPPTypeForwardDeclaration();
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = PropDef.GetValuePropDef().GetCPPType();
		return TEXT("TSET");
	}
};

struct FPropertyTypeTraitsMap : public FPropertyTypeTraitsBase
{
	static void CreateProperty(FUnrealPropertyDefinitionInfo& PropDef)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		// The key shouldn't ever have meta data, so eliminate was came in as part of parsing and the remove anything that was added
		// as part of property creation.  
		PropDef.GetPropertyBase().MapKeyProp->MetaData.Empty();
		PropDef.SetKeyPropDef(FPropertyTraits::CreateProperty(*PropDef.GetPropertyBase().MapKeyProp, PropDef, *(PropDef.GetFName().ToString() + TEXT("_Key")), 
			PropDef.GetVariableCategory(), ACCESS_Public, PropDef.GetArrayDimensions(), PropDef.GetUnrealSourceFile(), PropDef.GetLineNumber(), PropDef.GetParsePosition()));
		FUnrealPropertyDefinitionInfo& KeyPropDef = PropDef.GetKeyPropDef();

		FPropertyBase ValueVarProperty = PropDef.GetPropertyBase();
		ValueVarProperty.ArrayType = EArrayType::None;
		ValueVarProperty.MapKeyProp = nullptr;
		ValueVarProperty.RepNotifyName = NAME_None;
		ValueVarProperty.MetaData.Empty();
		PropDef.SetValuePropDef(FPropertyTraits::CreateProperty(ValueVarProperty, PropDef, PropDef.GetFName(), 
			PropDef.GetVariableCategory(), ACCESS_Public, PropDef.GetArrayDimensions(), PropDef.GetUnrealSourceFile(), PropDef.GetLineNumber(), PropDef.GetParsePosition()));
		FUnrealPropertyDefinitionInfo& ValuePropDef = PropDef.GetValuePropDef();

		VarProperty.PropertyFlags = ValuePropDef.GetPropertyBase().PropertyFlags;
		KeyPropDef.GetPropertyBase().PropertyFlags &= CPF_PropagateToMapKey;
		ValuePropDef.GetPropertyBase().PropertyFlags &= CPF_PropagateToMapValue;

		PropDef.SetAllocatorType(VarProperty.AllocatorType);//@TODO - This is a duplicate now
	}

	static FPropertyFlagsChanges PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef, EPropertyFlags& AddedFlags)
	{
		FPropertyFlagsChangeDetector Detector(PropDef);
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();
		FUnrealPropertyDefinitionInfo& KeyPropDef = PropDef.GetKeyPropDef();
		FUnrealPropertyDefinitionInfo& ValuePropDef = PropDef.GetValuePropDef();

		EPropertyFlags KeyAddedFlags = CPF_None;
		PostParseFinalizeHelper<true>(KeyPropDef, KeyAddedFlags);
		KeyPropDef.GetPropertyBase().MetaData.Empty();

		EPropertyFlags ValueAddedFlags = CPF_None;
		VarProperty.PropertyFlags |= PostParseFinalizeHelper<true>(ValuePropDef, ValueAddedFlags).Added;
		KeyPropDef.GetPropertyBase().PropertyFlags |= ValueAddedFlags;

		VarProperty.MetaData.Append(MoveTemp(ValuePropDef.GetPropertyBase().MetaData)); // Move any added meta data items to the container

		PropagateFlagsFromInnerAndHandlePersistentInstanceMetadata(VarProperty.PropertyFlags, PropDef.GetPropertyBase().MapKeyProp->MetaData, KeyPropDef);
		PropagateFlagsFromInnerAndHandlePersistentInstanceMetadata(VarProperty.PropertyFlags, PropDef.GetPropertyBase().MetaData, ValuePropDef);
		return Detector.Changes();
	}

	static FProperty* CreateEngineType(FUnrealPropertyDefinitionInfo& PropDef, FFieldVariant Scope, const FName& Name, EObjectFlags ObjectFlags)
	{
		FPropertyBase& VarProperty = PropDef.GetPropertyBase();

		FMapProperty* Map = new FMapProperty(Scope, Name, ObjectFlags);
		PropDef.SetProperty(Map);
		Map->KeyProp = FPropertyTraits::CreateEngineType(PropDef.GetKeyPropDefRef());
		Map->ValueProp = FPropertyTraits::CreateEngineType(PropDef.GetValuePropDefRef());
		return Map;
	}

	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
	{
		return
			IsSupportedByBlueprintSansContainers(PropDef.GetValuePropDef(), false) &&
			IsSupportedByBlueprintSansContainers(PropDef.GetKeyPropDef(), false);
	}

	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FMapProperty::StaticClass()->GetName();
	}

	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText, uint32 CPPExportFlags)
	{
		if (ExtendedTypeText != NULL)
		{
			FString KeyExtendedTypeText;
			FString ValueExtendedTypeText;

			FString KeyTypeText = PropDef.GetKeyPropDef().GetCPPType(&KeyExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider map keys to be "arguments or return values"
			FString ValueTypeText = PropDef.GetValuePropDef().GetCPPType(&ValueExtendedTypeText, CPPExportFlags & ~CPPF_ArgumentOrReturnValue); // we won't consider map values to be "arguments or return values"

			// if property type is a template class, add a space between the closing brackets
			if ((KeyExtendedTypeText.Len() && KeyExtendedTypeText.Right(1) == TEXT(">"))
				|| (!KeyExtendedTypeText.Len() && KeyTypeText.Len() && KeyTypeText.Right(1) == TEXT(">")))
			{
				KeyExtendedTypeText += TEXT(" ");
			}

			// if property type is a template class, add a space between the closing brackets
			if ((ValueExtendedTypeText.Len() && ValueExtendedTypeText.Right(1) == TEXT(">"))
				|| (!ValueExtendedTypeText.Len() && ValueTypeText.Len() && ValueTypeText.Right(1) == TEXT(">")))
			{
				ValueExtendedTypeText += TEXT(" ");
			}

			*ExtendedTypeText = FString::Printf(TEXT("<%s%s,%s%s>"), *KeyTypeText, *KeyExtendedTypeText, *ValueTypeText, *ValueExtendedTypeText);
		}
		return TEXT("TMap");
	}

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
	{
		return FString::Printf(TEXT("%s %s"), *PropDef.GetKeyPropDef().GetCPPTypeForwardDeclaration(), *PropDef.GetValuePropDef().GetCPPTypeForwardDeclaration());
	}

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
	{
		ExtendedTypeText = FString::Printf(TEXT("%s,%s"), *PropDef.GetKeyPropDef().GetCPPType(), *PropDef.GetValuePropDef().GetCPPType());
		return TEXT("TMAP");
	}
};

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
// APIs
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------

bool FPropertyTraits::DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& OutForm)
{
	OutForm = FString();
	if (CppForm.IsEmpty())
	{
		return false;
	}

	return PropertyTypeDispatch<DefaultValueStringCppFormatToInnerFormatDispatch, false, bool>(PropDef, std::ref(CppForm), std::ref(OutForm));
}

TSharedRef<FUnrealPropertyDefinitionInfo> FPropertyTraits::CreateProperty(const FPropertyBase& VarProperty, FUnrealTypeDefinitionInfo& Outer, const FName& Name, EVariableCategory VariableCategory, EAccessSpecifier AccessSpecifier, const TCHAR* Dimensions, FUnrealSourceFile& SourceFile, int LineNumber, int ParsePosition)
{
	TSharedRef<FUnrealPropertyDefinitionInfo> PropDefRef = MakeShared<FUnrealPropertyDefinitionInfo>(SourceFile, LineNumber, ParsePosition, VarProperty, VariableCategory, AccessSpecifier, Dimensions, Name, Outer);
	FUnrealPropertyDefinitionInfo& PropDef = *PropDefRef;

	// Create the property and attach to the definition
	CreatePropertyHelper<true>(PropDef);

	// If we aren't a sub-property, then add any meta data to the property
	// We do this because of the edit inline metadata which might be added.
	// Technically, it should be added to the outermost property.
	if (UHTCast<FUnrealPropertyDefinitionInfo>(Outer) == nullptr)
	{
		FUHTMetaData::RemapMetaData(PropDef, PropDef.GetPropertyBase().MetaData);
		PropDef.ValidateMetaDataFormat(PropDef.GetPropertyBase().MetaData);
	}
	return PropDefRef;
}

FProperty* FPropertyTraits::CreateEngineType(TSharedRef<FUnrealPropertyDefinitionInfo> PropDefRef)
{
	FUnrealPropertyDefinitionInfo& PropDef = *PropDefRef;
	FUnrealTypeDefinitionInfo* Outer = PropDef.GetOuter();
	check(Outer);

	EObjectFlags ObjectFlags = RF_Public;
	if (PropDef.GetVariableCategory() == EVariableCategory::Member && PropDef.GetAccessSpecifier() == ACCESS_Private)
	{
		ObjectFlags = RF_NoFlags;
	}

	// Create the property and attach to the definition
	FFieldVariant Scope = Outer->AsProperty() ? FFieldVariant(Outer->AsProperty()->GetProperty()) : FFieldVariant(Outer->AsObject()->GetObject());
	FProperty* Property = CreateEngineTypeHelper<true>(PropDef, Scope, PropDef.GetFName(), ObjectFlags);
	PropDef.SetProperty(Property);

	// Perform some final initialization from the property base data
	Property->PropertyFlags = PropDef.GetPropertyBase().PropertyFlags;

	// Special initialization for member variables
	if (PropDef.GetVariableCategory() == EVariableCategory::Member)
	{
		if (PropDef.HasAnyPropertyFlags(CPF_RepNotify))
		{
			Property->RepNotifyFunc = PropDef.GetPropertyBase().RepNotifyName;
		}
	}

	// Add the meta data to the property
	for (TPair<FName, FString>& KVP : PropDef.GetMetaDataMap())
	{
		Property->SetMetaData(KVP.Key, FString(KVP.Value));
	}

	// If we are parent to a struct
	if (FUnrealStructDefinitionInfo* StructDef = UHTCast<FUnrealStructDefinitionInfo>(Outer))
	{

		// Add to the end of the properties slist
		FField** Prev = &StructDef->GetStruct()->ChildProperties;
		for (; *Prev != nullptr; Prev = &(*Prev)->Next)
		{
			// No body
		}
		check(*Prev == nullptr);
		Property->Next = nullptr;
		*Prev = Property;
	}
	return Property;
}

bool FPropertyTraits::IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable)
{
	return PropertyTypeDispatch<IsSupportedByBlueprintDispatch, true, bool>(PropDef, bMemberVariable);
}

FString FPropertyTraits::GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef)
{
	return PropertyTypeDispatch<GetEngineClassNameDispatch, true, FString>(PropDef);
}

FString FPropertyTraits::GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText/* = nullptr */, uint32 CPPExportFlags/* = 0 */)
{
	return PropertyTypeDispatch<GetCPPTypeDispatch, true, FString>(PropDef, ExtendedTypeText, CPPExportFlags);
}

FString FPropertyTraits::GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef)
{
	return PropertyTypeDispatch<GetCPPTypeForwardDeclarationDispatch, true, FString>(PropDef);
}

FString FPropertyTraits::GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText)
{
	return PropertyTypeDispatch<GetCPPMacroTypeDispatch, true, FString>(PropDef, ExtendedTypeText);
}

bool FPropertyTraits::SameType(const FUnrealPropertyDefinitionInfo& Lhs, const FUnrealPropertyDefinitionInfo& Rhs)
{
	auto AdjustTypes = [](EUHTPropertyType PropertyType)
	{
		switch (PropertyType)
		{
		case EUHTPropertyType::ObjectPtrReference:
			return EUHTPropertyType::ObjectReference;
		case EUHTPropertyType::Bool8:
		case EUHTPropertyType::Bool16:
		case EUHTPropertyType::Bool32:
		case EUHTPropertyType::Bool64:
			return EUHTPropertyType::Bool;
		default:
			return PropertyType;
		}
	};

	const FPropertyBase& LhsPropertyBase = Lhs.GetPropertyBase();
	const FPropertyBase& RhsPropertyBase = Rhs.GetPropertyBase();

	EUHTPropertyType LhsType = AdjustTypes(LhsPropertyBase.GetUHTPropertyType());
	EUHTPropertyType RhsType = AdjustTypes(RhsPropertyBase.GetUHTPropertyType());
	if (LhsType != RhsType)
	{
		return  false;
	}

	if (Lhs.HasKeyPropDef() && !SameType(Lhs.GetKeyPropDef(), Rhs.GetKeyPropDef()))
	{
		return false;
	}


	if (Lhs.HasValuePropDef() && !SameType(Lhs.GetValuePropDef(), Rhs.GetValuePropDef()))
	{
		return false;
	}

	if (LhsPropertyBase.TypeDef != RhsPropertyBase.TypeDef)
	{
		return false;
	}

	if (LhsPropertyBase.FieldClassName != RhsPropertyBase.FieldClassName)
	{
		return false;
	}

	if (LhsPropertyBase.MetaClassDef != RhsPropertyBase.MetaClassDef)
	{
		return false;
	}

	return true;
}

bool FPropertyTraits::IsValidFieldClass(FName FieldClassName)
{
	static FFieldClassInit FieldClassInit;

	return FFieldClass::GetNameToFieldClassMap().Find(FieldClassName) != nullptr;
}

void FPropertyTraits::PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef)
{
	EPropertyFlags AddedFlags = CPF_None;
	if (PostParseFinalizeHelper<true>(PropDef, AddedFlags).Added != CPF_None)
	{
		PropDef.GetOuter()->OnPostParsePropertyFlagsChanged(PropDef);
	}
}

