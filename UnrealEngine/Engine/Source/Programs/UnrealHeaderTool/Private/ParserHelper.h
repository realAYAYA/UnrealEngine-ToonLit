// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"
#include "UObject/FieldPathProperty.h"
#include "UObject/Stack.h"
#include "UnrealHeaderToolGlobals.h"
#include "Templates/UniqueObj.h"
#include "Templates/UniquePtr.h"
#include "RigVMDefines.h"

class UEnum;
class UScriptStruct;
class FProperty;
class FUnrealSourceFile;
class UObject;
class UField;
class UMetaData;
class FHeaderParser;
class FUnrealPropertyDefinitionInfo;
class FUnrealTypeDefinitionInfo;
class FUnrealEnumDefinitionInfo;
class FUnrealClassDefinitionInfo;
class FUnrealScriptStructDefinitionInfo;
class FUnrealFunctionDefinitionInfo;

/*-----------------------------------------------------------------------------
	FPropertyBase.
-----------------------------------------------------------------------------*/

enum EFunctionExportFlags
{
	FUNCEXPORT_Final			=0x00000001,	// function declaration included "final" keyword.  Used to differentiate between functions that have FUNC_Final only because they're private
	//							=0x00000002,
	//							=0x00000004,
	FUNCEXPORT_RequiredAPI		=0x00000008,	// Function should be exported as a public API function
	FUNCEXPORT_Inline			=0x00000010,	// export as an inline static C++ function
	FUNCEXPORT_CppStatic		=0x00000020,	// Export as a real C++ static function, causing thunks to call via ClassName::FuncName instead of this->FuncName
	FUNCEXPORT_CustomThunk		=0x00000040,	// Export no thunk function; the user will manually define a custom one
	//							=0x00000080,
	//							=0x00000100,
};

enum EPropertyHeaderExportFlags
{
	PROPEXPORT_Public		=0x00000001,	// property should be exported as public
	PROPEXPORT_Private		=0x00000002,	// property should be exported as private
	PROPEXPORT_Protected	=0x00000004,	// property should be exported as protected
};

struct EPointerType
{
	enum Type
	{
		None,
		Native
	};
};

struct EArrayType
{
	enum Type
	{
		None,
		Static,
		Dynamic,
		Set
	};
};

struct ERefQualifier
{
	enum Type
	{
		None,
		ConstRef,
		NonConstRef
	};
};

enum class EIntType
{
	None,
	Sized,  // e.g. int32, int16
	Unsized // e.g. int, unsigned int
};

enum class EAllocatorType
{
	Default,
	MemoryImage
};

/** Types access specifiers. */
enum EAccessSpecifier
{
	ACCESS_NotAnAccessSpecifier = 0,
	ACCESS_Public,
	ACCESS_Private,
	ACCESS_Protected,
	ACCESS_Num,
};

enum class EUHTPropertyType : uint8
{
	None = CPT_None,
	Byte = CPT_Byte,
	UInt16 = CPT_UInt16,
	UInt32 = CPT_UInt32,
	UInt64 = CPT_UInt64,
	Int8 = CPT_Int8,
	Int16 = CPT_Int16,
	Int = CPT_Int,
	Int64 = CPT_Int64,
	Bool = CPT_Bool,
	Bool8 = CPT_Bool8,
	Bool16 = CPT_Bool16,
	Bool32 = CPT_Bool32,
	Bool64 = CPT_Bool64,
	Float = CPT_Float,
	ObjectReference = CPT_ObjectReference,
	Name = CPT_Name,
	Delegate = CPT_Delegate,
	Interface = CPT_Interface,
	Struct = CPT_Struct,
	String = CPT_String,
	Text = CPT_Text,
	MulticastDelegate = CPT_MulticastDelegate,
	WeakObjectReference = CPT_WeakObjectReference,
	LazyObjectReference = CPT_LazyObjectReference,
	ObjectPtrReference = CPT_ObjectPtrReference,
	SoftObjectReference = CPT_SoftObjectReference,
	Double = CPT_Double,
	Map = CPT_Map,
	Set = CPT_Set,
	FieldPath = CPT_FieldPath,
	LargeWorldCoordinatesReal = CPT_FLargeWorldCoordinatesReal,

	Enum,
	DynamicArray,

	MAX,
};

inline bool IsBool(EPropertyType Type)
{
	return Type == CPT_Bool || Type == CPT_Bool8 || Type == CPT_Bool16 || Type == CPT_Bool32 || Type == CPT_Bool64;
}

inline bool IsNumeric(EPropertyType Type)
{
	return Type == CPT_Byte || Type == CPT_UInt16 || Type == CPT_UInt32 || Type == CPT_UInt64 || Type == CPT_Int8 || Type == CPT_Int16 || Type == CPT_Int || Type == CPT_Int64 || Type == CPT_Float || Type == CPT_Double;
}

inline bool IsObjectOrInterface(EPropertyType Type)
{
	return Type == CPT_ObjectReference || Type == CPT_Interface || Type == CPT_WeakObjectReference || Type == CPT_LazyObjectReference || Type == CPT_ObjectPtrReference || Type == CPT_SoftObjectReference;
}

inline bool IsObject(EPropertyType Type)
{
	return Type == CPT_ObjectReference || Type == CPT_WeakObjectReference || Type == CPT_LazyObjectReference || Type == CPT_ObjectPtrReference || Type == CPT_SoftObjectReference;
}

inline bool IsInterface(EPropertyType Type)
{
	return Type == CPT_Interface;
}

/**
 * Basic information describing a type.
 */
class FPropertyBase : public TSharedFromThis<FPropertyBase>
{
public:
	// Variables.
	EPropertyType       Type = EPropertyType::CPT_None;
	EArrayType::Type    ArrayType = EArrayType::None;
	EAllocatorType      AllocatorType = EAllocatorType::Default;
	EPropertyFlags      PropertyFlags = CPF_None;
	EPropertyFlags      ImpliedPropertyFlags = CPF_None;
	EPropertyFlags		DisallowFlags = ~CPF_None; 
	ERefQualifier::Type RefQualifier = ERefQualifier::None; // This is needed because of legacy stuff - FString mangles the flags for reasons that have become lost in time but we need this info for testing for invalid replicated function signatures.

	TSharedPtr<FPropertyBase> MapKeyProp;

	/**
	 * A mask of EPropertyHeaderExportFlags which are used for modifying how this property is exported to the native class header
	 */
	uint32 PropertyExportFlags = PROPEXPORT_Public;

	union
	{
		FUnrealTypeDefinitionInfo* TypeDef = nullptr;
		FUnrealEnumDefinitionInfo* EnumDef;
		FUnrealScriptStructDefinitionInfo* ScriptStructDef;
		FUnrealClassDefinitionInfo* ClassDef;
		FUnrealFunctionDefinitionInfo* FunctionDef;
	};
	FName FieldClassName = NAME_None;
	FUnrealClassDefinitionInfo* MetaClassDef = nullptr;

	FName   DelegateName = NAME_None;
	FUnrealClassDefinitionInfo* DelegateSignatureOwnerClassDef = nullptr;
	FName   RepNotifyName = NAME_None;

	/** Raw string (not type-checked) used for specifying special text when exporting a property to the *Classes.h file */
	FString	ExportInfo;

	/** Map of key value pairs that will be added to the package's UMetaData for this property */
	TMap<FName, FString> MetaData;

	EPointerType::Type PointerType = EPointerType::None;
	EIntType IntType = EIntType::None;

	/** Setter function name. Either specified in the UPROPERTY macro or autogenerated while parsing class declaration */
	FString SetterName;	
	/** Getter function name. Either specified in the UPROPERTY macro or autogenerated while parsing class declaration */
	FString GetterName;
	/** True if the property was marked as having a setter */
	bool bSetterTagFound = false;
	/** True if the property was marked as having a getter */
	bool bGetterTagFound = false;
	/** True if setter function declaration was found while parsing class header */
	bool bSetterFunctionFound = false;
	/** True if getter function declaration was found while parsing class header */
	bool bGetterFunctionFound = false;
	/** True if the property is declared as FieldNotify. */
	bool bFieldNotify = false;

public:
	/** @name Constructors */
	//@{
	FPropertyBase(const FPropertyBase&) = default;
	FPropertyBase(FPropertyBase&&) = default;
	FPropertyBase& operator=(const FPropertyBase&) = default;
	FPropertyBase& operator=(FPropertyBase&&) = default;

	explicit FPropertyBase(EPropertyType InType);

	explicit FPropertyBase(EPropertyType InType, EIntType InIntType);

	explicit FPropertyBase(FUnrealEnumDefinitionInfo& InEnumDef, EPropertyType InType);

	explicit FPropertyBase(FUnrealClassDefinitionInfo& InClassDef, EPropertyType InType, bool bWeakIsAuto = false);

	explicit FPropertyBase(FUnrealScriptStructDefinitionInfo& InStructDef);

	explicit FPropertyBase(FName InFieldClassName, EPropertyType InType);
	//@}

	/** @name Functions */
	//@{

	/**
	 * Returns whether this token represents an object reference
	 */
	bool IsObjectOrInterface() const
	{
		return ::IsObjectOrInterface(Type);
	}

	bool IsObject() const
	{
		return ::IsObject(Type);
	}

	bool IsInterface() const
	{
		return ::IsInterface(Type);
	}

	bool IsBool() const
	{
		return ::IsBool(Type);
	}

	bool IsContainer() const
	{
		return (ArrayType != EArrayType::None || MapKeyProp.IsValid());
	}

	bool IsPrimitiveOrPrimitiveStaticArray() const
	{
		return (ArrayType == EArrayType::None || ArrayType == EArrayType::Static) && !MapKeyProp.IsValid();
	}

	bool IsBooleanOrBooleanStaticArray() const
	{
		return IsBool() && IsPrimitiveOrPrimitiveStaticArray();
	}

	bool IsStructOrStructStaticArray() const
	{
		return Type == CPT_Struct && IsPrimitiveOrPrimitiveStaticArray();
	}

	bool IsObjectRefOrObjectRefStaticArray() const
	{
		return (Type == CPT_ObjectReference || Type == CPT_ObjectPtrReference) && IsPrimitiveOrPrimitiveStaticArray();
	}

	bool IsClassRefOrClassRefStaticArray() const;

	bool IsInterfaceOrInterfaceStaticArray() const
	{
		return Type == CPT_Interface && IsPrimitiveOrPrimitiveStaticArray();
	}

	bool IsByteEnumOrByteEnumStaticArray() const;

	bool IsNumericOrNumericStaticArray() const
	{
		return IsNumeric(Type) && IsPrimitiveOrPrimitiveStaticArray();
	}

	bool IsDelegateOrDelegateStaticArray() const
	{
		return Type == CPT_Delegate && IsPrimitiveOrPrimitiveStaticArray();
	}

	bool IsMulticastDelegateOrMulticastDelegateStaticArray() const
	{
		return Type == CPT_Delegate && IsPrimitiveOrPrimitiveStaticArray();
	}

	bool IsEditorOnlyProperty() const
	{
		return (PropertyFlags & CPF_EditorOnly) != 0;
	}

	bool ContainsEditorOnlyProperties() const;

	bool PassCPPArgsByRef() const
	{
		if (ArrayType == EArrayType::Dynamic || ArrayType == EArrayType::Set || MapKeyProp.IsValid())
		{
			return true;
		}
		switch (Type)
		{
		//case CPT_Struct:
		case CPT_String:
		case CPT_Text:
		case CPT_Delegate:
		case CPT_MulticastDelegate:
		case CPT_WeakObjectReference:
		case CPT_LazyObjectReference:
		case CPT_ObjectPtrReference:
		case CPT_SoftObjectReference:
		case CPT_Map:
		case CPT_Set:
			return true;

		default:
			return false;
		}
	}

	FUnrealEnumDefinitionInfo* AsEnum() const;

	bool IsEnum() const;

	/**
	 * Determines whether this token's type is compatible with another token's type.
	 *
	 * @param	Other							the token to check against this one.
	 *											Given the following example expressions, VarA is Other and VarB is 'this':
	 *												VarA = VarB;
	 *
	 *												function func(type VarB) {}
	 *												func(VarA);
	 *
	 *												static operator==(type VarB_1, type VarB_2) {}
	 *												if ( VarA_1 == VarA_2 ) {}
	 *
	 * @param	bDisallowGeneralization			controls whether it should be considered a match if this token's type is a generalization
	 *											of the other token's type (or vice versa, when dealing with structs
	 * @param	bIgnoreImplementedInterfaces	controls whether two types can be considered a match if one type is an interface implemented
	 *											by the other type.
	 * @param   bEmulateSameType				If true, perform slightly different validation as per FProperty::SameType.  Implementation is not
	 *											complete.
	 */
	bool MatchesType(const FPropertyBase& Other, bool bDisallowGeneralization, bool bIgnoreImplementedInterfaces = false, bool bEmulateSameType = false) const;
	//@}

	EIntType GetSizedIntTypeFromPropertyType(EPropertyType PropType)
	{
		switch (PropType)
		{
			case CPT_Byte:
			case CPT_UInt16:
			case CPT_UInt32:
			case CPT_UInt64:
			case CPT_Int8:
			case CPT_Int16:
			case CPT_Int:
			case CPT_Int64:
				return EIntType::Sized;

			default:
				return EIntType::None;
		}
	}

	EUHTPropertyType GetUHTPropertyType() const;

	friend struct FPropertyBaseArchiveProxy;
};

/**
 * Information about a function being compiled.
 */
struct FFuncInfo
{
	/** @name Variables */
	//@{
	/** Function flags. */
	EFunctionFlags	FunctionFlags = FUNC_None;
	/** Function flags which are only required for exporting */
	uint32		FunctionExportFlags = 0;
	/** Number of parameters expected for operator. */
	int32		ExpectParms = 0;
	/** Name of the wrapper function that marshalls the arguments and does the indirect call **/
	FString		MarshallAndCallName;
	/** Name of the actual implementation **/
	FString		CppImplName;
	/** Name of the actual validation implementation **/
	FString		CppValidationImplName;
	/** Name for callback-style names **/
	FString		UnMarshallAndCallName;
	/** Endpoint name */
	FString		EndpointName;
	/** Identifier for an RPC call to a platform service */
	uint16		RPCId = 0;
	/** Identifier for an RPC call expecting a response */
	uint16		RPCResponseId = 0;
	/** Delegate macro line in header. */
	int32		MacroLine = -1;
	/** Position in file where this function was declared. Points to first char of function name. */
	int32		InputPos = -1;
	/** Whether this function represents a sealed event */
	bool		bSealedEvent = false;
	/** TRUE if the function is being forced to be considered as impure by the user */
	bool		bForceBlueprintImpure = false;
	/** TRUE generate the entry for the FieldNotificationClassDescriptor. */
	bool		bFieldNotify = false;

	//@}

	/** Set the internal function names based on flags **/
	void SetFunctionNames(FUnrealFunctionDefinitionInfo& FunctionDef);
};

/*-----------------------------------------------------------------------------
	Retry points.
-----------------------------------------------------------------------------*/

/**
 * A point in the header parsing state that can be set and returned to
 * using InitScriptLocation() and ReturnToLocation().  This is used in cases such as testing
 * to see which overridden operator should be used, where code must be compiled
 * and then "undone" if it was found not to match.
 * <p>
 * Retries are not allowed to cross command boundaries (and thus nesting 
 * boundaries).  Retries can occur across a single command or expressions and
 * subexpressions within a command.
 */
struct FScriptLocation
{
	/** the text buffer for the class associated with this retry point */
	const TCHAR* Input;

	/** the position into the Input buffer where this retry point is located */
	int32 InputPos;

	/** the LineNumber of the compiler when this retry point was created */
	int32 InputLine;
};

/////////////////////////////////////////////////////
// FAdvancedDisplayParameterHandler - used by FHeaderParser::ParseParameterList, to check if a property if a function parameter has 'AdvancedDisplay' flag

/** 
 *	AdvancedDisplay can be used in two ways:
 *	1. 'AdvancedDisplay = "3"' - the number tells how many parameters (from beginning) should NOT BE marked
 *	2. 'AdvancedDisplay = "AttachPointName, Location, LocationType"' - list the parameters, that should BE marked
 */
class FAdvancedDisplayParameterHandler
{
	TArray<FString> ParametersNames;

	int32 NumberLeaveUnmarked;
	int32 AlreadyLeft;

	bool bUseNumber;
public:
	FAdvancedDisplayParameterHandler(const TMap<FName, FString>* MetaData);

	/** 
	 * return if given parameter should be marked as Advance View, 
	 * the function should be called only once for any parameter
	 */
	bool ShouldMarkParameter(const FString& ParameterName);

	/** return if more parameters can be marked */
	bool CanMarkMore() const;
};

/**
 * The FRigVMParameter represents a single parameter of a method
 * marked up with RIGVM_METHOD.
 * Each parameter can be marked with Constant, Input or Output
 * metadata - this struct simplifies access to that information.
 */
struct FRigVMParameter
{
	FRigVMParameter()
		: Name()
		, Type()
		, bConstant(false)
		, bInput(false)
		, bOutput(false)
		, bSingleton(false)
		, Getter()
		, CastName()
		, CastType()
		, bEditorOnly(false)
		, bIsEnum(false)
	{
	}

	FString Name;
	FString Type;
	bool bConstant;
	bool bInput;
	bool bOutput;
	bool bSingleton;
	FString Getter;
	FString CastName;
	FString CastType;
	bool bEditorOnly;
	bool bIsEnum;

	const FString& NameOriginal(bool bCastName = false) const
	{
		return (bCastName && !CastName.IsEmpty()) ? CastName : Name;
	}

	const FString& TypeOriginal(bool bCastType = false) const
	{
		return (bCastType && !CastType.IsEmpty()) ? CastType : Type;
	}

	FString Declaration(bool bCastType = false, bool bCastName = false) const
	{
		return FString::Printf(TEXT("%s %s"), *TypeOriginal(bCastType), *NameOriginal(bCastName));
	}

	FString BaseType(bool bCastType = false) const
	{
		const FString& String = TypeOriginal(bCastType);
		int32 LesserPos = 0;
		if (String.FindChar(TEXT('<'), LesserPos))
		{
			return String.Mid(0, LesserPos);
		}
		return String;
	}

	FString ExtendedType(bool bCastType = false) const
	{
		const FString& String = TypeOriginal(bCastType);
		int32 LesserPos = 0;
		if (String.FindChar(TEXT('<'), LesserPos))
		{
			return String.Mid(LesserPos);
		}
		return String;
	}

	FString TypeConstRef(bool bCastType = false) const
	{
		const FString& String = TypeNoRef(bCastType);
		if (String.StartsWith(TEXT("T"), ESearchCase::CaseSensitive) || String.StartsWith(TEXT("F"), ESearchCase::CaseSensitive))
		{
			return FString::Printf(TEXT("const %s&"), *String);
		}
		return FString::Printf(TEXT("const %s"), *String);
	}

	FString TypeRef(bool bCastType = false) const
	{
		const FString& String = TypeNoRef(bCastType);
		return FString::Printf(TEXT("%s&"), *String);
	}

	FString TypeNoRef(bool bCastType = false) const
	{
		const FString& String = TypeOriginal(bCastType);
		if (String.EndsWith(TEXT("&")))
		{
			return String.LeftChop(1);
		}
		return String;
	}

	FString TypeVariableRef(bool bCastType = false) const
	{
		return IsConst() ? TypeConstRef(bCastType) : TypeRef(bCastType);
	}

	FString Variable(bool bCastType = false, bool bCastName = false) const
	{
		return FString::Printf(TEXT("%s %s"), *TypeVariableRef(bCastType), *NameOriginal(bCastName));
	}

	bool IsConst() const
	{
		return bConstant || (bInput && !bOutput);
	}

	bool IsArray() const
	{
		return BaseType().Equals(TEXT("TArray"));
	}

	bool RequiresCast() const
	{
		return !CastType.IsEmpty() && !CastName.IsEmpty();
	}
};

/**
 * The FRigVMParameterArray represents the parameters in a notation
 * of a function marked with RIGVM_METHOD. The parameter array can
 * produce a comma separated list of names or parameter declarations.
 */
struct FRigVMParameterArray
{
public:
	int32 Num() const { return Parameters.Num(); }
	const FRigVMParameter& operator[](int32 InIndex) const { return Parameters[InIndex]; }
	FRigVMParameter& operator[](int32 InIndex) { return Parameters[InIndex]; }
	TArray<FRigVMParameter>::RangedForConstIteratorType begin() const { return Parameters.begin(); }
	TArray<FRigVMParameter>::RangedForConstIteratorType end() const { return Parameters.end(); }
	TArray<FRigVMParameter>::RangedForIteratorType begin() { return Parameters.begin(); }
	TArray<FRigVMParameter>::RangedForIteratorType end() { return Parameters.end(); }

	int32 Add(const FRigVMParameter& InParameter)
	{
		return Parameters.Add(InParameter);
	}

	FString Names(bool bLeadingSeparator = false, const TCHAR* Separator = TEXT(", "), bool bCastType = false, bool bIncludeEditorOnly = true) const
	{
		if (Parameters.Num() == 0)
		{
			return FString();
		}
		TArray<FString> NameArray;
		for (const FRigVMParameter& Parameter : Parameters)
		{
			if (!bIncludeEditorOnly && Parameter.bEditorOnly)
			{
				continue;
			}
			NameArray.Add(Parameter.NameOriginal(bCastType));
		}

		if (NameArray.Num() == 0)
		{
			return FString();
		}

		FString Joined = FString::Join(NameArray, Separator);
		if (bLeadingSeparator)
		{
			return FString::Printf(TEXT("%s%s"), Separator, *Joined);
		}
		return Joined;
	}

	FString Declarations(bool bLeadingSeparator = false, const TCHAR* Separator = TEXT(", "), bool bCastType = false, bool bCastName = false, bool bIncludeEditorOnly = true) const
	{
		if (Parameters.Num() == 0)
		{
			return FString();
		}
		TArray<FString> DeclarationArray;
		for (const FRigVMParameter& Parameter : Parameters)
		{
			if (!bIncludeEditorOnly && Parameter.bEditorOnly)
			{
				continue;
			}
			DeclarationArray.Add(Parameter.Variable(bCastType, bCastName));
		}

		if (DeclarationArray.Num() == 0)
		{
			return FString();
		}

		FString Joined = FString::Join(DeclarationArray, Separator);
		if (bLeadingSeparator)
		{
			return FString::Printf(TEXT("%s%s"), Separator, *Joined);
		}
		return Joined;
	}

private:
	TArray<FRigVMParameter> Parameters;
};

/**
 * A single info dataset for a function marked with RIGVM_METHOD.
 * This struct provides access to its name, the return type and all parameters.
 */
struct FRigVMMethodInfo
{
	FString ReturnType;
	FString Name;
	FRigVMParameterArray Parameters;

	FString ReturnPrefix() const
	{
		return (ReturnType.IsEmpty() || (ReturnType == TEXT("void"))) ? TEXT("") : TEXT("return ");
	}
};

/**
 * An info dataset providing access to all functions marked with RIGVM_METHOD
 * for each struct.
 */
struct FRigVMStructInfo
{
	bool bHasRigVM = false;
	bool bHasGetUpgradeInfoMethod = false;
	bool bHasGetNextAggregateNameMethod = false;
	FString Name;
	FRigVMParameterArray Members;
	TArray<FRigVMMethodInfo> Methods;
};
