// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Concepts/StaticStructProvider.h"
#include "Templates/Models.h"
#include "Templates/ValueOrError.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"


// Forward declarations
class FStructOnScope;
class FFuncReflection;

/**
 * FVMReflection - Reflection Helper
 *
 * Description:
 *	The purpose of the reflection helper, is to allow complete access to the UE UScript/Blueprint Virtual Machine through reflection,
 *	i.e. without directly/statically referencing any classes/structs/variables/etc., referencing them all only by name/string instead,
 *	so anything using the VM can be accessed without a dependency on other packages (and without compile fails when something changes).
 *
 *	This is useful/important for writing unit tests that can break or go out of date, without breaking an entire suite of unit tests,
 *	and for maintaining permanent backwards compatibility through multiple engine/game codebase updates, and for general debugging.
 *
 *
 * Operator roles: (... is shorthand for an FVMReflection instance)
 *	- FVMReflection(Object):				Initialize a reflection helper, pointing to the specified object, for use with operators
 *	- FVMReflection(FStructOnScope):		Initialize a reflection helper, pointing to the struct within the specified FStructOnScope
 *	- FVMReflection(VMReflection):			Initialize a reflection helper, by copying another reflection helpers current state
 *
 *	- ...->*"Property":						Point the reflection helper, to the specified property
 *	- (...->*"Array")["Type"][Element]:		Array property element (also specifies the inner array type, for verification)
 *
 *	- (Type)(...):							Cast data the reflection helper points to, to specified type (only way to access data)
 *	- (Struct*)(void*)(...)["Struct"]:		Special cast (void*), for accessing structs (also specify struct type [], for verification)
 *
 *	- (..., &bError):						Outputs to bError, whether any error was encountered this far into parsing
 *	- (..., &ErrorString):					As above, excepts outputs a string containing the entire reflection history and error text
 *
 *
 * Not yet implemented:
 *	- (...->*"Function")("Parm1", etc.):	Execute the specified function, with the specified parameters. Might not get implemented.
 *
 *
 * - Example:
 *		FGuid* ItemGuid = (FGuid*)(void*)(((FVMReflection(UnitPC.Get())->*"WorldInventory"->*"Inventory"->*"Items")
 *							["FFortItemEntry"][0]->"ItemGuid")["FGuid"]);
 *
 *
 * Setting function parameters:
 *	The FFuncReflection class allows you to easily set the parameters for functions, using reflection; this is useful for RPC's,
 *	as well as for general local functions called using ProcessEvent etc..
 *
 *	For example:
 *		FFuncReflection FuncRefl(PlayerStateObj, TEXT("UndoRemoveCardFromHandAtIndex"));
 *
 *		FVMReflection(FuncRefl.ParmsRefl)->*"CardData"->*"CardGuid"->*"A" = 1;
 *
 *		PlayerStateObj->ProcessEvent(FuncRefl.GetFunc(), FuncRefl.GetParms());
 */


/**
 * Example of how the reflection helper makes code more succinct:
 *
 * New code (reflection helper):
 *	// Reflection for AFortPlayerController->QuickBars
 *	AActor* QuickBars = (AActor*)(UObject*)(FVMReflection(UnitPC.Get())->*"QuickBars");
 *
 *	// Reflection for AFortPlayerController->WorldInventory->Inventory->Items->ItemGuid
 *	FGuid* EntryItemGuidRef = (FGuid*)(void*)(((FVMReflection(UnitPC.Get())
 *								->*"WorldInventory"->*"Inventory"->*"Items")["FFortItemEntry"][0]->*"ItemGuid")["FGuid"]);
 *
 *	...
 *
 *
 * Old code (manual reflection):
 *	// Reflection for AFortPlayerController->QuickBars
 *	FObjectProperty* QuickBarsProp = FindFProperty<FObjectProperty>(UnitPC->GetClass(), TEXT("QuickBars"));
 *	UObject** QuickBarsRef = (QuickBarsProp != NULL ? QuickBarsProp->ContainerPtrToValuePtr<UObject*>(UnitPC.Get()) : NULL);
 *	AActor* QuickBars = (QuickBarsRef != NULL ? Cast<AActor>(*QuickBarsRef) : NULL);
 *
 *
 *	// Reflection for AFortPlayerController->WorldInventory->Inventory->Items->ItemGuid
 *	FObjectProperty* WorldInvProp = FindFProperty<FObjectProperty>(UnitPC->GetClass(), TEXT("WorldInventory"));
 *	UObject** WorldInvRef = (WorldInvProp != NULL ? WorldInvProp->ContainerPtrToValuePtr<UObject*>(UnitPC.Get()) : NULL);
 *	AActor* WorldInv = (WorldInvRef != NULL ? Cast<AActor>(*WorldInvRef) : NULL);
 *	FStructProperty* WIInventoryProp = (WorldInv != NULL ? FindFProperty<FStructProperty>(WorldInv->GetClass(), TEXT("Inventory")) : NULL);
 *	void* WIInventoryRef = (WIInventoryProp != NULL ? WIInventoryProp->ContainerPtrToValuePtr<void>(WorldInv) : NULL);
 *	FArrayProperty* InvItemsProp = (WIInventoryProp != NULL ? FindFProperty<FArrayProperty>(WIInventoryProp->Struct, TEXT("Items")) : NULL);
 *	FScriptArrayHelper* InvItemsArray = ((InvItemsProp != NULL && WIInventoryRef != NULL) ?
 *							new FScriptArrayHelper(InvItemsProp, InvItemsProp->ContainerPtrToValuePtr<void>(WIInventoryRef)) : NULL);
 *
 *	void* InvItemsEntryRef = ((InvItemsArray != NULL && InvItemsArray->Num() > 0) ?
 *								InvItemsArray->GetRawPtr(0) : NULL);
 *	FStructProperty* InvItemsEntryProp = CastField<FStructProperty>(InvItemsProp != NULL ? InvItemsProp->Inner : NULL);
 *	FStructProperty* EntryItemGuidProp = (InvItemsEntryProp != NULL ?
 *											FindFProperty<FStructProperty>(InvItemsEntryProp->Struct, TEXT("ItemGuid")) : NULL);
 *	FGuid* EntryItemGuidRef = (EntryItemGuidProp != NULL ? InvItemsEntryProp->ContainerPtrToValuePtr<FGuid>(InvItemsEntryRef) : NULL);
 *
 *	...
 *
 *	if (InvItemsArray != NULL)
 *	{
 *		delete InvItemsArray;
 *	}
 */


/**
 * Currently supported fields (field list taken from UEd class viewer)
 *
 *			- Field
 *				- Enum
 *					- UserDefinedEnum
 *				- Property
 *	readwrite		- ArrayProperty
 *	readonly		- BoolProperty										arraycheck
 *					- DelegateProperty
 *					- InterfaceProperty
 *					- MulticastDelegateProperty
 *	readwrite		- NameProperty										arraycheck	assign
 *	done			- NumericProperty
 *	readwrite			- ByteProperty									arraycheck	assign
 *	readwriteupcast		- DoubleProperty								arraycheck	assign
 *	readwrite			- FloatProperty									arraycheck	assign
 *	readwriteupcast		- Int16Property									arraycheck	assign
 *	readwriteupcast		- Int64Property									arraycheck	assign
 *	readwrite			- Int8Property									arraycheck	assign
 *	readwriteupcast		- IntProperty									arraycheck	assign
 *	readwriteupcast		- FInt16Property								arraycheck	assign
 *	readwriteupcast		- FIntProperty									arraycheck	assign
 *	readwriteupcast		- FInt64Property								arraycheck	assign
 *					- ObjectPropertyBase
 *	readwrite			- SoftObjectProperty										assign
 *	inherited				- SoftClassProperty									inherited
 *						- LazyObjectProperty
 *	readwrite			- ObjectProperty								arraycheck	assign
 *	inherited				- ClassProperty								inherited	inherited
 *	readwrite			- WeakObjectProperty
 *	readwrite		- StrProperty										arraycheck	assign
 *	readwrite		- StructProperty									arraycheck
 *	readwrite		- TextProperty										arraycheck	assign
 *	readwrite	- Struct
 *	readwrite		- Class
 *						- BlueprintGeneratedClass
 *							- AnimBlueprintGeneratedClass
 *							- GameplayAbilityBlueprintGeneratedClass
 *							- ScriptBlueprintGeneratedClass
 *							- WidgetBlueprintGeneratedClass
 *						- LinkerPlaceholderClass
 *					- Function
 *						- DelegateFunction
 *						- LinkerPlaceholderFunction
 *	readwrite		- ScriptStruct
 *						- UserDefinedStruct
 *							- AISenseBlueprintListener
 *
 *			- Other:
 *	readwrite	- Static Arrays
 *	done			- Static object array, context/object access (i.e. using ->* on object array elements)
 *	done		- Dynamic object array, context/object access
 *	done		- Struct context access
 *	?			- Struct object access
 *	done		- Struct property casting
 */


// @todo #JohnB_VMRefl: Write unit test for 'struct object access' above (a struct with an object member)
// @todo #JohnB_VMRefl: Add 'structonscope' example to unit test as well (it's verified as working, but should be in the unit test too)


// @todo #JohnB_VMRefl: Add an example of iterating struct arrays of unknown struct type, to the unit test - as an example, see:
//				UInfiniteItems_Grenades::GetInventoryInfo

// @todo #JohnB_VMRefl: You haven't yet had to actually adjust the size of dynamic arrays of unknown/inaccessible type,
//				or add/remove elements and other manipulation yet; add and document a method of doing this, eventually



// @todo #JohnBFeature: In principle, there's nothing to stop you adding another boilerplate 'FVMReflectionString' class,
//				taking and parsing a string using same syntax as FVMReflection (may as well optimize syntax though, as C++ is limited),
//				then tying that to a console-command, as a supercharged version of 'get/set' commands, but with reflection capabilities.
//
//				This would be extremely nice to have, as a debug command; could even use the reflection helper to parse while typing,
//				adding VAX-like autocomplete suggestions while typing on the console, for this class of console commands.
//
//				Beyond the scope of the unit test tool, but (with much of the legwork done below) it's low hanging fruit,
//				that'd make for a nice feature.
//				If you (eventually) add autocomplete to the unit test tool log window, this would be the next logical feature.

/**
 * Used for specifying the warning level, for reflection helpers
 */
enum class EVMRefWarning : uint8
{
	Warn,	// Any errors during reflection are printed to log (default)
	NoWarn	// No errors during reflection are printed to log
};

/**
 * FVMReflection
 */
class NETCODEUNITTEST_API FVMReflection
{
private:
	/**
	 * Private base constructor - should never be used externally
	 */
	FVMReflection();

public:
	/**
	 * UObject constructor - Initializing from a UObject
	 *
	 * @param InBaseObject	The object the reflection helper should be initialized with
	 * @param InWarnLevel	Whether or not reflection failures (e.g. something major like missing properties) should be logged
	 */
	explicit FVMReflection(UObject* InBaseObject, EVMRefWarning InWarnLevel=EVMRefWarning::Warn);

	/**
	 * Struct constructor - initializing from a scoped struct (allows you to use reflection, to work with unknown structs)
	 *
	 * @param InStruct		The scoped struct the reflection helper should be initialized with
	 * @param InWarnLevel	Whether or not reflection failures (e.g. something major like missing properties) should be logged
	 */
	explicit FVMReflection(FStructOnScope& InStruct, EVMRefWarning InWarnLevel=EVMRefWarning::Warn);


	/**
	 * Copy constructor - used regularly to copy reflection states, but without passing on history or temporary variables
	 * (such as error return)
	 *
	 * @param ToCopy		The reflection helper to copy
	 */
	explicit FVMReflection(const FVMReflection& ToCopy);

	/**
	 * FFuncReflection copy constructor - initializing from a function reflection struct instance (shortcut to reference its parameters)
	 *
	 * @param InFuncRefl	The function reflection struct to initialize from
	 * @param InWarnLevel	Whether or not reflection failures (e.g. something major like missing properties) should be logged
	 */
	explicit FVMReflection(FFuncReflection& InFuncRefl, EVMRefWarning InWarnLevel=EVMRefWarning::Warn);


private:
	/**
	 * Copy assignment operator - not supported
	 */
	FVMReflection& operator = (const FVMReflection& ToCopy);

	/**
	 * Member access operator - not used, use ->* instead
	 */
	FVMReflection* operator ->()
	{
		return nullptr;
	}

public:
	/**
	 * Member access operator. Used to access object/struct properties.
	 *
	 * @param PropertyName	The name of the property being accessed
	 * @return				Returns this VM reflector, for operator chaining
	 */
	FVMReflection& operator ->*(FString PropertyName);

	FORCEINLINE FVMReflection& operator ->*(const ANSICHAR* PropertyName)
	{
		FString PropertyStr(ANSI_TO_TCHAR(PropertyName));

		return (*this)->*PropertyStr;
	}

	/**
	 * Array subscript operator, used to access static/dynamic array elements
	 *
	 * NOTE: Due to operator precedence, you MUST wrap the accessed property in brackets, like so
	 *	(FVMReflectior(blah)->*"ArrayProp")["Type"][Element]->*"etc"
	 *
	 * @param ArrayElement	The array element to access
	 * @return				Returns this VM reflector, for operator chaining
	 */
	FVMReflection& operator [](int32 ArrayElement);


	/**
	 * Array subscript operator, which takes a string for verifying that an array is of a particular type, e.g. ["uint8"].
	 * This is mandatory, and must be specified before the array element subscript, like so: ByteArray["uint8"][0].
	 *
	 * NOTE: This also works for structs, but should only be used when using (void*) to cast a struct to a pointer.
	 *
	 * The primary purpose of this, is for casting arrays to an (FScriptArray*), and then to (TArray<Type>*);
	 * without type verification with this operator, casting like this would be unsafe,
	 * as you would be assuming a type which may have later changed as the codebase evolves.
	 *
	 * Object arrays should be specified in the format ["U?*"], e.g. ["UObject*"],
	 * Actor arrays should be specified in the format ["A?*"], e.g. ["APawn*"], and
	 * Struct and struct arrays should be specified in the format ["F?"], e.g. ["FVector"].
	 *
	 * @param InFieldType	The expected field type
	 * @return				Returns this VM reflector, for operator chaining
	 */
	FVMReflection& operator [](const ANSICHAR* InFieldType);


private:
	/**
	 * When an operator finishes, and is pointing to an object property, this changes the VM reflector context to that object
	 */
	void ProcessObjectProperty();

	/**
	 * As above, but for struct properties
	 */
	void ProcessStructProperty();


	/**
	 * Internal template for defining simple pointer cast operators, for basic (e.g. numeric) types
	 */
	template<typename InType, class InTypeClass>
	InType* GetWritableCast(const TCHAR* InTypeStr, bool bDoingUpCast=false);

	/**
	 * Internal template for defining a readonly cast operator, for basic (e.g. numeric) types, with support for casting between types
	 *
	 * @param SupportedUpCasts		Types which support upcasting to InType
	 */
	template<typename InType, class InTypeClass>
	FORCEINLINE InType GetNumericTypeCast(const TCHAR* InTypeStr, const TArray<FFieldClass*>& SupportedUpCasts);

	/**
	 * Base version of above, for types with no upcasts
	 */
	template<typename InType>
	FORCEINLINE InType GetTypeCast(const TCHAR* InTypeStr);


	// Some macros for declaring casts, which are implemented using correspondingly named macros, in the .cpp
	#define GENERIC_POINTER_CAST_ASSIGN(InType) \
		explicit operator InType*(); \
		\
		/** Also implement the assignment operator, for automatically performing these casts and assigning */ \
		FORCEINLINE FVMReflection& operator = (InType Value) \
		{ \
			InType* VarRef = (InType*)(*this); \
			\
			if (VarRef != nullptr) \
			{ \
				*VarRef = Value; \
			} \
			\
			return *this; \
		}

	#define NUMERIC_POINTER_CAST_ASSIGN(InType) GENERIC_POINTER_CAST_ASSIGN(InType)

	#define NUMERIC_CAST_ASSIGN_BASIC(InType) \
		explicit operator InType();

	#define NUMERIC_CAST_ASSIGN(InType) NUMERIC_CAST_ASSIGN_BASIC(InType)


public:
	/**
	 * Casting operators
	 */

	// Declare numeric pointer casts
	NUMERIC_POINTER_CAST_ASSIGN(uint8);
	NUMERIC_POINTER_CAST_ASSIGN(uint16);
	NUMERIC_POINTER_CAST_ASSIGN(uint32);
	NUMERIC_POINTER_CAST_ASSIGN(uint64);
	NUMERIC_POINTER_CAST_ASSIGN(int8);
	NUMERIC_POINTER_CAST_ASSIGN(int16);
	NUMERIC_POINTER_CAST_ASSIGN(int32);
	NUMERIC_POINTER_CAST_ASSIGN(int64);
	NUMERIC_POINTER_CAST_ASSIGN(float);
	NUMERIC_POINTER_CAST_ASSIGN(double);

	// Declare readonly numeric casts
	NUMERIC_CAST_ASSIGN_BASIC(uint8);
	NUMERIC_CAST_ASSIGN(uint16);
	NUMERIC_CAST_ASSIGN(uint32);
	NUMERIC_CAST_ASSIGN(uint64);
	NUMERIC_CAST_ASSIGN_BASIC(int8);
	NUMERIC_CAST_ASSIGN(int16);
	NUMERIC_CAST_ASSIGN(int32);
	NUMERIC_CAST_ASSIGN(int64);
	NUMERIC_CAST_ASSIGN_BASIC(float);
	NUMERIC_CAST_ASSIGN(double);


	// Declare generic pointer casts
	GENERIC_POINTER_CAST_ASSIGN(FName);
	GENERIC_POINTER_CAST_ASSIGN(FString);
	GENERIC_POINTER_CAST_ASSIGN(FText);


	// @todo #JohnB: Implement the assignment operator for other writable casts below


	/**
	 * Cast to bool
	 */
	explicit operator bool();

	/**
	 * Cast to FName
	 */
	explicit operator FName();

	/**
	 * Cast to FString
	 */
	explicit operator FString();

	/**
	 * Cast to FText
	 */
	explicit operator FText();

	/**
	 * Cast to writable UObject* pointer (only valid for object properties)
	 */
	explicit operator UObject**();

	/**
	 * Cast to UObject*
	 */
	explicit operator UObject*();

	/**
	 * Cast to writable FSoftObjectPtr* (where write actions should be performed through FSoftObjectPrr methods, not by pointer assignment).
	 * For FSoftObjectProperty
	 */
	explicit operator FSoftObjectPtr*();


	/**
	 * Cast to FScriptArray* pointer (only valid for dynamic arrays), then cast to TArray<Type*>
	 * NOTE: There is no FScriptArray** operator, as that should never be writable
	 * NOTE: Use the TSharedPtr<FScriptArrayHelper> operator, if you need to modify an array of uncertain/undefined type
	 */
	explicit operator FScriptArray*();

	/**
	 * Cast to an FScriptArrayHelper, which is useful for performing operations on arrays of an uncertain/undefined type
	 * NOTE: Useful specifically in cases where you need to add to an array, less so for array iteration.
	 */
	explicit operator TSharedPtr<FScriptArrayHelper>();


	// @todo JohnB: Perhaps add casts for known array types, the same ones you do type verification on


	/**
	 * Cast operator for structs in general - cast to void*, then cast to StructType*
	 */
	explicit operator void*();

	/**
	 * Gets a reference to the struct being pointed to, with compile-time and runtime checks on the validity of the struct type
	 * NOTE: This alters the state of the reflection object.
	 */
	template<typename T>
	FORCEINLINE T* GetStructRef()
	{
		static_assert(TModels<CStaticStructProvider, T>::Value, "Only USTRUCT types can be passed to GetStructRef");

		return (T*)(void*)(FVMReflection(*this)[TCHAR_TO_ANSI(*FString::Printf(TEXT("F%s"), *T::StaticStruct()->GetName()))]);
	}


	/**
	 * Special assignment operators
	 */

	/**
	 * Assign a value to a bool property
	 */
	FVMReflection& operator = (bool Value);

	/**
	 * Assign a value to an object property
	 */
	FVMReflection& operator = (UObject* Value);

	/**
	 * Assign a value to either a string or an enum property (autodetects enums)
	 * NOTE: Enums must be specified in format: EEnumName::EnumValueName
	 */
	FVMReflection& operator = (TCHAR* Value);


	#undef GENERIC_POINTER_CAST_ASSIGN
	#undef NUMERIC_POINTER_CAST_ASSIGN
	#undef NUMERIC_CAST_ASSIGN_BASIC
	#undef NUMERIC_CAST_ASSIGN


	/**
	 * Converts the value of whatever the reflection helper is pointing to, into a human readable string.
	 *
	 * @return	A human readable representation of what the reflection helper is pointing to
	 */
	TValueOrError<FString, FString> GetValueAsString();

	/**
	 * If pointing to an array, returns the array size. Cache the value, instead of using it directly in a for loop
	 *
	 * @return	Returns the size of the array
	 */
	FORCEINLINE int32 GetArrayNum() const
	{
		return (CanCastArray() ? (((FScriptArray*)FieldAddress)->Num()) : 0);
	}


	/**
	 * Allows an inline method of returning the error status - the comma operator has the lowest precedence, so is executed last.
	 *
	 * This also allows you to test for errors at every stage of reflection (although it's a bit ugly if done this way), e.g:
	 *	UObject* Result = (UObject*)(((FVMReflection(TestObjA), &bError)->*"AObjectRef", &bErrorA)->*"BObjectRef", &bErrorB);
	 *
	 *
	 * Since the operator specifies a bool pointer though, we don't want to hold onto this pointer longer than necessary
	 * (in case it becomes a dangling pointer), so it is unset every time an operator returns (with the exception of this operator).
	 *
	 * This means both, that you can use error bools to catch errors from within specific parts of a statement,
	 * while also avoiding invalid memory access:
	 * (Cast)((->*"Blah", &bError)->*"Boo"):	Returns errors accessing 'Blah', but not 'Boo' or 'Cast'
	 *
	 * It also means, that if you specify a bool right at the end of a reflection statement, before casting to a return type,
	 * that you can use this last reflection statement to detect casting errors too:
	 * (Cast)(->*"Blah"->*"Boo", &bError):		Returns errors accessing Blah, Boo, or with Cast
	 */
	FVMReflection& operator ,(bool* bErrorPointer);

	/**
	 * As above, but returns the complete reflection history so far, including any errors
	 *
	 * @param OutHistory	Takes and stores a temporary reference, to the a string for outputting the reflection history
	 */
	FVMReflection& operator ,(FString* OutHistory);


	/**
	 * Returns the current error status
	 *
	 * @return Whether or not the reflection helper encountered an error
	 */
	FORCEINLINE bool IsError() const
	{
		return bIsError;
	}

	/**
	 * Disables the need to verify field types before accessing structs etc., to e.g. make use of the 'reflect' console command easier.
	 */
	FORCEINLINE void DisableFieldVerification()
	{
		bSkipFieldVerification = true;
		bVerifiedFieldType = true;
	}

	/**
	 * Returns the reflection helper history as a string
	 *
	 * @return	Returns the history
	 */
	FORCEINLINE FString GetHistory() const
	{
		FString Result;

		for (FString CurString : History)
		{
			Result += CurString;
		}

		return Result;
	}


	/**
	 * Does a complete debug dump of the state of this reflection instance, and disables further use
	 */
	void DebugDump();

private:
	/**
	 * Casting helpers
	 */

	/**
	 * Whether or not a property is an array (static or dynamic)
	 *
	 * @return	Returns whether or not the property is an array (static or dynamic)
	 */
	FORCEINLINE bool IsPropertyArray() const
	{
		const FProperty* CurProp = FieldInstance.Get<FProperty>();

		return CurProp != nullptr && (CurProp->ArrayDim > 1 || CastField<FArrayProperty>(CurProp) != nullptr);
	}

	/**
	 * Whether or not a property is an object
	 *
	 * @return	Returns whether or not the object is an object
	 */
	FORCEINLINE bool IsPropertyObject() const
	{
		return FieldInstance.IsValid() && (FieldInstance.IsA(FObjectProperty::StaticClass()) ||
				FieldInstance.IsA(FWeakObjectProperty::StaticClass()) || FieldInstance.IsA(FSoftObjectProperty::StaticClass()));
	}

	/**
	 * Whether or not the VM reflector points to a property that is ready for casting
	 *
	 * @return	Whether or not the VM reflector is ready for casting
	 */
	FORCEINLINE bool CanCastProperty() const
	{
		return !bIsError && BaseAddress != nullptr && FieldInstance.IsValid() && FieldAddress != nullptr &&
				FieldInstance.IsA(FProperty::StaticClass()) && (!IsPropertyArray() || (bVerifiedFieldType && bSetArrayElement));
	}

	/**
	 * Whether or not the VM reflector points to an object that is ready for casting
	 *
	 * @param PropertyClass		The UObject property to check for
	 * @return					Whether or not the VM reflector is ready for casting
	 */
	template<typename PropertyClass>
	FORCEINLINE bool CanCastObject() const
	{
		static_assert(TPointerIsConvertibleFromTo<PropertyClass, FObjectPropertyBase>::Value,
						"PropertyClass must derive from FObjectPropertyBase");

		bool bReturnVal = false;

		if (!bIsError && FieldInstance.IsValid() && (!IsPropertyArray() || (bVerifiedFieldType && bSetArrayElement)))
		{
			bool bBaseAddressIsObject = BaseAddress != nullptr && FieldInstance.IsA(UClass::StaticClass());
			
			// This can only happen when the attempt to step-in to an object property (i.e. to assign it to BaseAddress) failed,
			// due to the object property being nullptr. However, nullptr is still a valid cast return
			bool bFieldAddressIsObject = !bBaseAddressIsObject && bNextActionMustBeCast && FieldAddress != nullptr &&
											FieldInstance.IsA(PropertyClass::StaticClass());

			bReturnVal = bBaseAddressIsObject || bFieldAddressIsObject;
		}

		return bReturnVal;
	}

	/**
	 * Whether or not the VM reflector points to an array that is ready for casting to an array pointer
	 *
	 * @return	Whether or not the VM reflector is ready for casting
	 */
	FORCEINLINE bool CanCastArray() const
	{
		return !bIsError && BaseAddress != NULL && FieldInstance.IsValid() && FieldInstance.IsA(FArrayProperty::StaticClass()) &&
				FieldAddress != NULL && bVerifiedFieldType && !bSetArrayElement;
	}

	/**
	 * Whether or not the VM reflector points to a struct that is ready for casting to a pointer
	 *
	 * @return	Whether or not the VM reflector is ready for casting
	 */
	FORCEINLINE bool CanCastStruct() const
	{
		return !bIsError && BaseAddress != NULL && FieldInstance.IsValid() && FieldInstance.IsA(UStruct::StaticClass()) &&
				FieldAddress != NULL && bVerifiedFieldType;
	}


	/**
	 * Other internal helper functions
	 */

	/**
	 * Called early within non-cast operators, to unset error pointers
	 */
	FORCEINLINE void NotifyOperator(FString Operation)
	{
		// Got a non-cast action when only a cast is allowed - error
		if (bNextActionMustBeCast)
		{
			SetError(NextActionError + FString::Printf(TEXT(" Failed operation: %s"), *Operation));
		}

		bOutError = NULL;
		OutHistoryPtr = NULL;
	}

	/**
	 * Notification of a cast return, to unset error pointers
	 */
	FORCEINLINE void NotifyCastReturn()
	{
		bOutError = NULL;
		OutHistoryPtr = NULL;
	}



	/**
	 * Encapsulates code that sets the field address
	 *
	 * @param InFieldAddress		The new field address
	 * @param bSettingArrayElement	Whether or not the new field address is for an array element
	 */
	void SetFieldAddress(void* InFieldAddress, bool bSettingArrayElement=false);

	/**
	 * Adds a new operation to the reflection helper history
	 *
	 * @param InHistory		The line to be added to the history
	 */
	FORCEINLINE void AddHistory(FString InHistory)
	{
		History.Add(InHistory);

		if (OutHistoryPtr != NULL)
		{
			*OutHistoryPtr = TEXT("");

			for (auto CurStr : History)
			{
				*OutHistoryPtr += CurStr;
			}
		}
	}

	/**
	 * Adds the final casting operation to the reflection helper history
	 *
	 * @param InHistory		The line to be added to the history
	 */
	FORCEINLINE void AddCastHistory(FString InHistory)
	{
		History.Insert(InHistory, 0);

		if (OutHistoryPtr != NULL)
		{
			*OutHistoryPtr = TEXT("");

			for (auto CurStr : History)
			{
				*OutHistoryPtr += CurStr;
			}
		}
	}

	/**
	 * Sets the error status, and relays it to the error/history output pointers, if set
	 *
	 * @param InError		The error description
	 * @param bCastError	Whether or not this was a casting error
	 */
	void SetError(FString InError, bool bCastError=false);

	FORCEINLINE void SetCastError(FString InHistory)
	{
		SetError(InHistory, true);
	}

private:
	// NOTE: If you add any new variables, make sure to update both constructors

	/** The current base object (or struct) address location */
	void* BaseAddress;


	/** The current field instance (e.g. a UPROPERTY within AActor or such) */
	FFieldVariant FieldInstance;

	/** The address of the current field - OR, if bSetArrayElement - the address of the current array element */
	void* FieldAddress;

	/** Whether or not the array or struct type has been specified for verification */
	bool bVerifiedFieldType : 1;

	/** Whether or not to skip field verification (e.g. when using the 'reflect' console command) */
	bool bSkipFieldVerification : 1;

	/** Whether or not the array element has been set, for an array */
	bool bSetArrayElement : 1;

	/** The ->* operator tried to step into an object/struct which was NULL. If the next action is not a cast, it is an error. */
	bool bNextActionMustBeCast : 1;


	/** Whether or not there was an error during reflection */
	bool bIsError : 1;

	/** Tied with 'bNextActionMustBeCast', this is the error that will be logged if an action other than cast is tried. */
	FString NextActionError;

	/** Holds a temporary pointer, for outputting errors - regularly reset to NULL, to avoid potential invalid memory access */
	bool* bOutError;

	/** The history of reflection operations */
	TArray<FString> History;

	/** Holds a temporary pointer, for outputting error history - regularly reset to NULL, like bOutError */
	FString* OutHistoryPtr;

	/** Controls whether or not errors are printed to log */
	EVMRefWarning WarnLevel;
};


// @todo #JohnB: Why do you use InFuncName below, in cases where you already know the UFunction? Examine/refactor this.
//					(though sometimes, a nullptr UFunction seems to be possible - perhaps it's for better tracking during errors)

/**
 * FFuncReflection - helper for quickly/concisely setting function parameters through reflection
 */
class FFuncReflection
{
private:
	FFuncReflection(UFunction* InFunction, const TCHAR* InFuncName)
		: FunctionName(InFuncName)
		, Function(InFunction)
		, ParmsMemory(Function)
		, ParmsRefl(ParmsMemory)
	{
	}

	FFuncReflection(UFunction* InFunction, const TCHAR* InFuncName, void* InParmsMemory)
		: FunctionName(InFuncName)
		, Function(InFunction)
		, ParmsMemory(Function, (uint8*)InParmsMemory)
		, ParmsRefl(ParmsMemory)
	{
	}

public:
	/**
	 * Constructor for creating a function instance and (optionally) filling in its parameters
	 *
	 * @param InClassName	The name of the class the function is in
	 * @param InFuncName	The name of the function
	 */
	FFuncReflection(const TCHAR* InClassName, const TCHAR* InFuncName)
		: FFuncReflection(FindUField<UFunction>(UClass::TryFindTypeSlow<UClass>(InClassName), InFuncName), InFuncName)
	{
	}

	/**
	 * Constructor for creating a function instance and (optionally) filling in its parameters
	 *
	 * @param TargetObj		The object the function is in
	 * @param InFuncName	The name of the function
	 */
	FFuncReflection(UObject* TargetObj, const TCHAR* InFuncName)
		: FFuncReflection((TargetObj != nullptr ? TargetObj->FindFunction(InFuncName) : nullptr), InFuncName)
	{
	}

	/**
	 * Constructor for reading (and optionally modifying) an existing functions parameters (e.g. from received RPC's)
	 *
	 * @param InFunction	The function whose parameters we want to read/modify
	 * @param InParms		The function parameters memory pointer
	 */
	FFuncReflection(UFunction* InFunction, void* InParms)
		: FFuncReflection(InFunction, *InFunction->GetName(), InParms)
	{
	}

	FORCEINLINE bool IsValid() const
	{
		return Function != nullptr && ParmsMemory.IsValid() && !ParmsRefl.IsError();
	}

	FORCEINLINE UFunction* GetFunc()
	{
		return Function;
	}

	FORCEINLINE void* GetParms() const
	{
		return (void*)ParmsMemory.GetStructMemory();
	}

public:
	/** The name of the function */
	const TCHAR* FunctionName;

	/** Reference to the function */
	UFunction* Function;

private:
	/** The function parameters in memory */
	FStructOnScope ParmsMemory;

public:
	/** Reflection instance, for writing the function parameters - initialize a new instance from this, e.g. FVMReflection(ParmsRefl) */
	const FVMReflection ParmsRefl;
};


/**
 * NUTUtilRefl - general reflection helper utility functions
 */
struct NETCODEUNITTEST_API NUTUtilRefl
{
	/**
	 * Iterates a functions parameters, and converts them to a human readable string
	 * NOTE: Can not be passed back to functions that read parameters from a string, e.g. 'CallFunctionByNameWithArguments'
	 *			@todo #JohnB: This would be a good goal though
	 *
	 * @param InFunction	The function template whose parameters are being parsed
	 * @param Parms			The parameter data to be parsed
	 * @return				Returns the parsed parameter data
	 */
	static FString FunctionParmsToString(UFunction* InFunction, void* Parms);
};
