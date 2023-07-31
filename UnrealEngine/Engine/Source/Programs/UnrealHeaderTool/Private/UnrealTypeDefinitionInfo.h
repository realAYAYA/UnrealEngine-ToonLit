// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Exceptions.h"
#include "Templates/SharedPointer.h"
#include <atomic>

#include "BaseParser.h"
#include "ParserHelper.h"

#define UHT_ENABLE_ENGINE_TYPE_CHECKS 0

// Forward declarations.
class FHeaderParser;
class FScope;
class FUnrealSourceFile;
struct FManifestModule;
enum class ECheckedMetadataSpecifier : int32;

// These are declared in this way to allow swapping out the classes for something more optimized in the future
typedef FStringOutputDevice FUHTStringBuilder;

// The FUnrealTypeDefinitionInfo represents most all types required during parsing.  At this time, these structures
// have a 1-1 correspondence with Engine types such as UClass and UProperty.  The goal is to provide a universal mechanism
// of associating compiler data with given types without needing to add extra data to the engine types.  They are also
// intended to eliminate all other UHT specific containers that map extra data with Engine classes.

// The following types are supported represented in hierarchal form below:
class FUnrealTypeDefinitionInfo;							// Base for all types, provides virtual methods to cast between all types
	class FUnrealPropertyDefinitionInfo;					// Represents properties (FField)
	class FUnrealObjectDefinitionInfo;						// Represents UObject
		class FUnrealPackageDefinitionInfo;					// Represents UPackage
		class FUnrealFieldDefinitionInfo;					// Represents UField
			class FUnrealEnumDefinitionInfo;				// Represents UEnum
			class FUnrealStructDefinitionInfo;				// Represents UStruct
				class FUnrealScriptStructDefinitionInfo;	// Represents UScriptStruct
				class FUnrealClassDefinitionInfo;			// Represents UClass
				class FUnrealFunctionDefinitionInfo;		// Represents UFunction

enum class EEnforceInterfacePrefix
{
	None,
	I,
	U
};

enum class ESerializerArchiveType
{
	None = 0,

	Archive = 1,
	StructuredArchiveRecord = 2
};
ENUM_CLASS_FLAGS(ESerializerArchiveType)

/** The category of variable declaration being parsed */
enum class EVariableCategory
{
	RegularParameter,
	ReplicatedParameter,
	Return,
	Member
};

enum class EFunctionType
{
	Function,
	Delegate,
	SparseDelegate,
};

enum class EParsedInterface
{
	NotAnInterface,
	ParsedUInterface,
	ParsedIInterface
};

enum class ECreateEngineTypesPhase : uint8
{
	Phase1,				// Create the Class, ScriptStruct, and Enum types
	Phase2,				// Create the functions
	Phase3,				// Create the properties and do final initialization
	MAX_Phases,
};

enum class EPostParseFinalizePhase : uint8
{
	Phase1,				// Flag propagation for structures and property finalization
	Phase2,				// All other finalization requirements
	MAX_Phases,
};

struct FDeclaration
{
	uint32 CurrentCompilerDirective;
	TArray<FToken> Tokens;
};

/**
 * UHT Specific implementation of meta data support
 */
class FUHTMetaData
{
public:

	static void RemapMetaData(FUnrealTypeDefinitionInfo& TypeDef, TMap<FName, FString>& MetaData);

	template <typename Def>
	static void RemapAndAddMetaData(Def& TypeDef, TMap<FName, FString>&& InMetaData)
	{
		RemapMetaData(TypeDef, InMetaData);
		TypeDef.ValidateMetaDataFormat(InMetaData);
		TypeDef.AddMetaData(MoveTemp(InMetaData));
	}

	template <typename Def>
	static void RemapAndAddMetaData(Def& TypeDef, TMap<FName, FString>& InMetaData)
	{
		RemapMetaData(TypeDef, InMetaData);
		TypeDef.ValidateMetaDataFormat(InMetaData);
		TypeDef.AddMetaData(MoveTemp(InMetaData));
	}

	/**
	 * Determines if the property has any metadata associated with the key
	 *
	 * @param Key The key to lookup in the metadata
	 * @param NameIndex if specified, will search for metadata linked to a specified value (in an enum); otherwise, searches for metadata for the object itself
	 * @return true if there is a (possibly blank) value associated with this key
	 */
	bool HasMetaData(const TCHAR* Key, int32 NameIndex = INDEX_NONE) const
	{
		return FindMetaData(Key, NameIndex) != nullptr;
	}
	bool HasMetaData(const FName& Key, int32 NameIndex = INDEX_NONE) const
	{
		return FindMetaData(Key, NameIndex) != nullptr;
	}

	/**
	 * Find the metadata value associated with the key
	 *
	 * @param Key The key to lookup in the metadata
	 * @param NameIndex if specified, will search for metadata linked to a specified value (in an enum); otherwise, searches for metadata for the object itself
	 * @return The value associated with the key if it exists, null otherwise
	 */
	const FString* FindMetaData(const TCHAR* Key, int32 NameIndex = INDEX_NONE) const
	{
		return GetMetaDataMap().Find(GetMetaDataKey(Key, NameIndex, FNAME_Find));
	}
	const FString* FindMetaData(const FName& Key, int32 NameIndex = INDEX_NONE) const
	{
		return GetMetaDataMap().Find(GetMetaDataKey(Key, NameIndex, FNAME_Find));
	}

	/**
	 * Find the metadata value associated with the key
	 *
	 * @param Key The key to lookup in the metadata
	 * @param NameIndex if specified, will search for metadata linked to a specified value (in an enum); otherwise, searches for metadata for the object itself
	 * @return The value associated with the key
	 */
	FString GetMetaData(const TCHAR* Key, int32 NameIndex = INDEX_NONE, bool bAllowRemap = true) const
	{
		return GetMetaDataHelper(GetMetaDataKey(Key, NameIndex, FNAME_Find), bAllowRemap);
	}
	FString GetMetaData(const FName& Key, int32  NameIndex = INDEX_NONE, bool bAllowRemap = true) const
	{
		return GetMetaDataHelper(GetMetaDataKey(Key, NameIndex, FNAME_Find), bAllowRemap);
	}

	/**
	 * Set the metadata value associated with the specified key.
	 *
	 * @param	Key			the metadata tag to find the value for
	 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
	 * @param	InValue		Value of the metadata for the key
	 *
	 */
	void SetMetaData(const TCHAR* Key, const TCHAR* InValue, int32 NameIndex = INDEX_NONE)
	{
		SetMetaDataHelper(GetMetaDataKey(Key, NameIndex, FNAME_Add), InValue);
	}
	void SetMetaData(const FName& Key, const TCHAR* InValue, int32 NameIndex = INDEX_NONE) 
	{
		SetMetaDataHelper(GetMetaDataKey(Key, NameIndex, FNAME_Add), InValue);
	}

	/**
	 * Find the metadata value associated with the key
	 * and return bool
	 * @param Key The key to lookup in the metadata
	 * @return return true if the value was true (case insensitive)
	 */
	bool GetBoolMetaData(const TCHAR* Key) const
	{
		const FString& BoolString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		return (BoolString == "true");
	}
	bool GetBoolMetaData(const FName& Key) const
	{
		const FString& BoolString = GetMetaData(Key);
		// FString == operator does case insensitive comparison
		return (BoolString == "true");
	}

	/**
	 * Split the ginen meta data into a string array
	 */
	void GetStringArrayMetaData(const FName& Key, TArray<FString>& Out) const;
	TArray<FString> GetStringArrayMetaData(const FName& Key) const;

	virtual TMap<FName, FString>& GetMetaDataMap() const = 0;

protected:
	virtual FString GetMetaDataIndexName(int32 NameIndex) const
	{
		checkf(false, TEXT("You can not perform indexed meta data searches on this type"));
		return FString();
	}

	virtual const TCHAR* GetMetaDataRemapConfigName() const
	{
		checkf(false, TEXT("You can not perform remapping of meta data values on this type"));
		return nullptr;
	}
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
	virtual void CheckFindMetaData(const FName& Key, const FString* ValuePtr) const = 0;
#endif

private:
	FName GetMetaDataKey(const TCHAR* Key, int32 NameIndex, EFindName FindName) const;
	FName GetMetaDataKey(FName Key, int32 NameIndex, EFindName FindName) const;
	const FString* FindMetaDataHelper(const FName& Key) const;
	FString GetMetaDataHelper(const FName& Key, bool bAllowRemap) const;
	virtual void SetMetaDataHelper(const FName& Key, const TCHAR* InValue);
};

/**
 * Class that stores information about type definitions.
 */
class FUnrealTypeDefinitionInfo 
	: public TSharedFromThis<FUnrealTypeDefinitionInfo>
	, public FUHTMessageProvider
{
private:
	enum class EFinalizeState : uint8
	{
		None,
		InProgress,
		Finished,
	};

public:
	virtual ~FUnrealTypeDefinitionInfo() = default;

	/**
	 * If this is a property, return the property version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealPropertyDefinitionInfo* AsProperty();

	/**
	 * If this is a property, return the property version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealPropertyDefinitionInfo& AsPropertyChecked()
	{
		FUnrealPropertyDefinitionInfo* Info = AsProperty();
		check(Info);
		return *Info;
	}

	/**
	 * If this is an object, return the object version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealObjectDefinitionInfo* AsObject();

	/**
	 * If this is a object, return the object version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealObjectDefinitionInfo& AsObjectChecked()
	{
		FUnrealObjectDefinitionInfo* Info = AsObject();
		check(Info);
		return *Info;
	}

	/**
	 * If this is a package, return the package version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealPackageDefinitionInfo* AsPackage();

	/**
	 * If this is a package, return the package version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealPackageDefinitionInfo& AsPackageChecked()
	{
		FUnrealPackageDefinitionInfo* Info = AsPackage();
		check(Info);
		return *Info;
	}

	/**
	 * If this is an field, return the field version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealFieldDefinitionInfo* AsField();

	/**
	 * If this is an field, return the field version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealFieldDefinitionInfo& AsFieldChecked()
	{
		FUnrealFieldDefinitionInfo* Info = AsField();
		check(Info);
		return *Info;
	}

	/**
	 * If this is an enumeration, return the enumeration version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealEnumDefinitionInfo* AsEnum();

	/**
	 * If this is an enumeration, return the enumeration version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealEnumDefinitionInfo& AsEnumChecked()
	{
		FUnrealEnumDefinitionInfo* Info = AsEnum();
		check(Info);
		return *Info;
	}

	/**
	 * If this is a struct, return the struct version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealStructDefinitionInfo* AsStruct();

	/**
	 * If this is a struct, return the struct version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealStructDefinitionInfo& AsStructChecked()
	{
		FUnrealStructDefinitionInfo* Info = AsStruct();
		check(Info);
		return *Info;
	}

	/**
	 * If this is a script struct, return the script struct version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealScriptStructDefinitionInfo* AsScriptStruct();

	/**
	 * If this is a script struct, return the script struct version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealScriptStructDefinitionInfo& AsScriptStructChecked()
	{
		FUnrealScriptStructDefinitionInfo* Info = AsScriptStruct();
		check(Info);
		return *Info;
	}

	/**
	 * If this is a function, return the function version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealFunctionDefinitionInfo* AsFunction();

	/**
	 * If this is a function, return the function version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealFunctionDefinitionInfo& AsFunctionChecked()
	{
		FUnrealFunctionDefinitionInfo* Info = AsFunction();
		check(Info);
		return *Info;
	}

	/**
	 * If this is a class, return the class version of the object
	 * @return Pointer to the definition info or nullptr if not of that type.
	 */
	virtual FUnrealClassDefinitionInfo* AsClass();

	/**
	 * If this is a class, return the class version of the object
	 * @return Reference to the definition.  Will assert if not of that type.
	 */
	FUnrealClassDefinitionInfo& AsClassChecked()
	{
		FUnrealClassDefinitionInfo* Info = AsClass();
		check(Info);
		return *Info;
	}

	/**
	 * Handles the propagation of property flags
	 */
	virtual void OnPostParsePropertyFlagsChanged(FUnrealPropertyDefinitionInfo& PropertyDef) {}

	/**
	 * Create UObject engine types.
	 */
	void CreateUObjectEngineTypes(ECreateEngineTypesPhase Phase);

	/**
	 * Perform any post parsing finalization and validation
	 */
	void PostParseFinalize(EPostParseFinalizePhase Phase);

	/**
	 * Perform any post parsing finalization that can happen specific to the containing source file
	 */
	virtual void ConcurrentPostParseFinalize() {}

	/**
	 * Return the compilation scope associated with this object
	 */
	virtual TSharedRef<FScope> GetScope();

	/**
	 * Return the CPP version of the name
	 */
	const FString& GetNameCPP() const
	{
		return NameCPP;
	}

	/**
	 * Returns the name of the property
	 */
	virtual FString GetName() const = 0;

	/** Returns the logical name of this object */
	virtual FName GetFName() const = 0;

	virtual FString GetFullName() const = 0;

	/**
	 * Returns the fully qualified pathname for this object, in the format:
	 * 'Outermost.[Outer:]Name'
	 */
	virtual FString GetPathName(FUnrealObjectDefinitionInfo* StopOuter = nullptr) const = 0;
	virtual void GetPathName(FUnrealObjectDefinitionInfo* StopOuter, FStringBuilderBase& ResultString) const = 0;

	/**
	 * Returns the fully qualified pathname for this object as an FTopLevelAssetPath
	 */
	virtual FTopLevelAssetPath GetStructPathName() const = 0;

	/**
	 * Walks up the chain of packages until it reaches the top level, which it ignores.
	 *
	 * @param	bStartWithOuter		whether to include this object's name in the returned string
	 * @return	string containing the path name for this object, minus the outermost-package's name
	 */
	virtual FString GetFullGroupName(bool bStartWithOuter) const = 0;

	/**
	 * Return true if this type has source information
	 */
	bool HasSource() const
	{
		return SourceFile != nullptr;
	}

	/**
	 * Gets the line number in source file this type was defined in.
	 */
	virtual int32 GetLineNumber() const override
	{
		return LineNumber;
	}

	/** 
	 * Returns the filename from the source file
	 */
	virtual FString GetFilename() const override;

	/**
	 * Sets the input line in the rare case where the definition is created before fully parsed (sparse delegates)
	 */
	void SetLineNumber(int32 InLineNumber)
	{
		LineNumber = InLineNumber;
	}

	/**
	 * Gets the reference to FUnrealSourceFile object that stores information about
	 * source file this type was defined in.
	 */
	FUnrealSourceFile& GetUnrealSourceFile()
	{
		check(HasSource());
		return *SourceFile;
	}

	const FUnrealSourceFile& GetUnrealSourceFile() const
	{
		check(HasSource());
		return *SourceFile;
	}

	/**
	 * Set the hash calculated from the generated code for this type
	 */
	void SetHash(uint32 InHash);

	/**
	 * Return the previously set hash. This method will assert if the hash has not been set.
	 */
	virtual uint32 GetHash(FUnrealTypeDefinitionInfo& ReferencingDef, bool bIncludeNoExport = true) const;

	/**
	 * Return the hash as a code comment.
	 */
	void GetHashTag(FUnrealTypeDefinitionInfo& ReferencingDef, FUHTStringBuilder& Out) const;

	/**
	 * Add meta data for the given definition
	 */
	virtual void AddMetaData(TMap<FName, FString>&& InMetaData)
	{
		Throwf(TEXT("Meta data can not be set for a definition of this type."));
	}

	// @todo: BP2CPP_remove
	/**
	 * Helper function that checks if the field is a dynamic type
	 */
	UE_DEPRECATED(5.0, "This method is no longer in use.")
	virtual bool IsDynamic() const
	{
		return false;
	}

	// @todo: BP2CPP_remove
	/**
	 * Helper function that checks if the field is belongs to a dynamic type
	 */
	UE_DEPRECATED(5.0, "This method is no longer in use.")
	virtual bool IsOwnedByDynamicType() const
	{
		return false;
	}

	/**
	 * Return the "outer" object that contains the definition for this object
	 */
	FUnrealTypeDefinitionInfo* GetOuter() const
	{
		return Outer;
	}

	/**
	 * Return the meta data in map form
	 */
	virtual TMap<FName, FString> GenerateMetadataMap() const = 0;

	/**
	 * Ensures at script compile time that the metadata formatting is correct
	 * @param	InKey			the metadata key being added
	 * @param	InValue			the value string that will be associated with the InKey
	 */
	void ValidateMetaDataFormat(const FName InKey, const FString& InValue) const;

	// Ensures at script compile time that the metadata formatting is correct
	virtual void ValidateMetaDataFormat(const TMap<FName, FString>& MetaData) const
	{
		for (const TPair<FName, FString>& Pair : MetaData)
		{
			ValidateMetaDataFormat(Pair.Key, Pair.Value);
		}
	}

protected:
	explicit FUnrealTypeDefinitionInfo(FString&& InNameCPP)
		: NameCPP(MoveTemp(InNameCPP))
	{ }

	FUnrealTypeDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, FString&& InNameCPP, FUnrealTypeDefinitionInfo& InOuter)
		: NameCPP(MoveTemp(InNameCPP))
		, Outer(&InOuter)
		, SourceFile(&InSourceFile)
		, LineNumber(InLineNumber)
	{ }

	virtual void ValidateMetaDataFormat(const FName InKey, ECheckedMetadataSpecifier InCheckedMetadataSpecifier, const FString& InValue) const;

	/**
	 * Create UObject engine types
	 */
	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) 
	{
		if (Phase == ECreateEngineTypesPhase::Phase1)
		{
			if (Outer != nullptr && Outer->AsPackage() == nullptr)
			{
				CreateUObjectEngineTypes(Phase);
			}
		}
	}

	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) {}

	void SetOuter(FUnrealTypeDefinitionInfo* InOuter)
	{
		Outer = InOuter;
	}

private:
	FString NameCPP;
	FUnrealTypeDefinitionInfo* Outer = nullptr;
	FUnrealSourceFile* SourceFile = nullptr;
	int32 LineNumber = 0;
	std::atomic<uint32> Hash = 0;
	EFinalizeState CreateUObectEngineTypesState[uint8(ECreateEngineTypesPhase::MAX_Phases)] = { EFinalizeState::None, EFinalizeState::None };
	EFinalizeState PostParseFinalizeState[uint8(EPostParseFinalizePhase::MAX_Phases)] = { EFinalizeState::None, EFinalizeState::None };
};

/**
 * Class that stores information about type definitions derived from UProperty
 */
class FUnrealPropertyDefinitionInfo
	: public FUnrealTypeDefinitionInfo
	, public FUHTMetaData
{
public:
	FUnrealPropertyDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, int32 InParsePosition, const FPropertyBase& InVarProperty, EVariableCategory InVariableCategory, EAccessSpecifier InAccessSpecifier, const TCHAR* Dimensions, FName InName, FUnrealTypeDefinitionInfo& InOuter)
		: FUnrealTypeDefinitionInfo(InSourceFile, InLineNumber, InName.ToString(), InOuter)
		, Name(InName)
		, PropertyBase(InVarProperty)
		, ArrayDimensions(InVarProperty.ArrayType == EArrayType::Static ? FString(Dimensions) : FString())
		, ParsePosition(InParsePosition)
		, VariableCategory(InVariableCategory)
		, AccessSpecifier(InAccessSpecifier)
	{ }

	virtual FUnrealPropertyDefinitionInfo* AsProperty() override
	{
		return this;
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	FProperty* GetProperty() const
	{
		check(Property);
		return Property;
	}

	/**
	 * Return the Engine instance associated with the compiler instance, does not check the pointer, can be null
	 */
	FProperty* GetPropertySafe() const
	{
		return Property;
	}

	/**
	 * Sets the engine type
	 */
	void SetProperty(FProperty* InProperty)
	{
		check(Property == nullptr || Property == InProperty);
		Property = InProperty;
	}

	void ExportCppDeclaration(FOutputDevice& Out, EExportedDeclaration::Type DeclarationType, const TCHAR* ArrayDimOverride = NULL, uint32 AdditionalExportCPPFlags = 0, bool bSkipParameterName = false) const;

	FString GetCPPMacroType(FString& ExtendedTypeText) const;

	/**
	 * Returns the name of the property
	 */
	virtual FString GetName() const override
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || GetPropertySafe()->GetName() == GetNameCPP());
#endif
		return GetNameCPP();
	}

	/** Returns the logical name of this object */
	virtual FName GetFName() const override
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || GetPropertySafe()->GetFName() == Name);
#endif
		return Name;
	}

	virtual FString GetFullName() const override;

	/**
	 * Returns the fully qualified pathname for this object, in the format:
	 * 'Outermost.[Outer:]Name'
	 */
	virtual FString GetPathName(FUnrealObjectDefinitionInfo* StopOuter = nullptr) const override;
	virtual void GetPathName(FUnrealObjectDefinitionInfo* StopOuter, FStringBuilderBase& ResultString) const override;

	/**
	 * Returns the fully qualified pathname for this object as an FTopLevelAssetPath
	 */
	virtual FTopLevelAssetPath GetStructPathName() const override
	{
		checkf(false, TEXT("Proeprties do not support GetStructPathName()"));
		return FTopLevelAssetPath();
	}

	/**
	 * Walks up the chain of packages until it reaches the top level, which it ignores.
	 *
	 * @param	bStartWithOuter		whether to include this object's name in the returned string
	 * @return	string containing the path name for this object, minus the outermost-package's name
	 */
	virtual FString GetFullGroupName(bool bStartWithOuter) const;

	/**
	 * Return the engine class name
	 */
	FString GetEngineClassName() const;

	/**
	 * Return the package associated with this object
	 */
	FUnrealPackageDefinitionInfo& GetPackageDef() const;

	/**
	* Finds the localized display name or native display name as a fallback.
	*
	* @return The display name for this object.
	*/
	FText GetDisplayNameText() const;

	/**
	* Finds the localized tooltip or native tooltip as a fallback.
	*
	* @param bShortTooltip Look for a shorter version of the tooltip (falls back to the long tooltip if none was specified)
	*
	* @return The tooltip for this object.
	*/
	FText GetToolTipText(bool bShortTooltip = false) const;

	/**
	 * Returns the C++ name of the property, including the _DEPRECATED suffix if the
	 * property is deprecated.
	 *
	 * @return C++ name of property
	 */
	FString GetNameWithDeprecated() const
	{
		FString OutName = HasAnyPropertyFlags(CPF_Deprecated) ? GetName() + TEXT("_DEPRECATED") : GetName();
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || GetPropertySafe()->GetNameCPP() == OutName);
#endif
		return OutName;
	}

	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
	 * @param	CPPExportFlags		flags for modifying the behavior of the export
	 */
	FString GetCPPType(FString* ExtendedTypeText = nullptr, uint32 CPPExportFlags = 0) const;

	FString GetCPPTypeForwardDeclaration() const;

	/**
	 * Returns this property's propertyflags
	 */
	EPropertyFlags GetPropertyFlags() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || (GetPropertySafe()->GetPropertyFlags() & ~CPF_ComputedFlags) == PropertyBase.PropertyFlags);
#endif
		return PropertyBase.PropertyFlags;
	}

	void SetPropertyFlags(EPropertyFlags NewFlags)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || (GetPropertySafe()->GetPropertyFlags() & ~CPF_ComputedFlags) == PropertyBase.PropertyFlags);
#endif
		if (FProperty* LocalProperty = GetPropertySafe())
		{
			LocalProperty->PropertyFlags |= NewFlags;
		}
		PropertyBase.PropertyFlags |= NewFlags;
	}

	void ClearPropertyFlags(EPropertyFlags NewFlags)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || (GetPropertySafe()->GetPropertyFlags() & ~CPF_ComputedFlags) == PropertyBase.PropertyFlags);
#endif
		if (FProperty* LocalProperty = GetPropertySafe())
		{
			LocalProperty->PropertyFlags &= ~NewFlags;
		}
		PropertyBase.PropertyFlags &= ~NewFlags;
	}

	/**
	* 
	 * Used to safely check whether any of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and bool is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an bool.
	 *
	 * @param FlagsToCheck	Object flags to check for.
	 *
	 * @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	bool HasAnyPropertyFlags(uint64 FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check((FlagsToCheck & CPF_ComputedFlags) == 0);
		check(GetPropertySafe() == nullptr || (GetPropertySafe()->GetPropertyFlags() & ~CPF_ComputedFlags) == PropertyBase.PropertyFlags);
#endif
		return (PropertyBase.PropertyFlags & FlagsToCheck) != 0;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set. This is required
	 * as PropertyFlags currently is a 64 bit data type and bool is a 32 bit data type so
	 * simply using PropertyFlags&CPF_MyFlagBiggerThanMaxInt won't work correctly when
	 * assigned directly to an bool.
	 *
	 * @param FlagsToCheck	Object flags to check for
	 *
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	bool HasAllPropertyFlags(uint64 FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check((FlagsToCheck & CPF_ComputedFlags) == 0);
		check(GetPropertySafe() == nullptr || (GetPropertySafe()->GetPropertyFlags() & ~CPF_ComputedFlags) == PropertyBase.PropertyFlags);
#endif
		return (PropertyBase.PropertyFlags & FlagsToCheck) == FlagsToCheck;
	}

	bool HasSpecificPropertyFlags(uint64 FlagsToCheck, uint64 ExpectedFlags)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check((FlagsToCheck & CPF_ComputedFlags) == 0);
		check(GetPropertySafe() == nullptr || (GetPropertySafe()->GetPropertyFlags() & ~CPF_ComputedFlags) == PropertyBase.PropertyFlags);
#endif
		return (PropertyBase.PropertyFlags & FlagsToCheck) == ExpectedFlags;
	}

	/**
	 * Return the meta data in map form
	 */
	virtual TMap<FName, FString> GenerateMetadataMap() const override
	{
		return GetMetaDataMap();
	}

	/**
	 * Editor-only properties are those that only are used with the editor is present or cannot be removed from serialisation.
	 * Editor-only properties include: EditorOnly properties
	 * Properties that cannot be removed from serialisation are:
	 *		Boolean properties (may affect GCC_BITFIELD_MAGIC computation)
	 *		Native properties (native serialisation)
	 */
	bool IsEditorOnlyProperty() const
	{
		bool bResult = HasAnyPropertyFlags(CPF_DevelopmentAssets);
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || bResult == GetPropertySafe()->IsEditorOnlyProperty());
#endif
		return bResult;
	}

	/**
	 * Returns true if this property, or in the case of e.g. array or struct properties any sub- property, contains a
	 * UObject reference that is marked CPF_NeedCtorLink (i.e. instanced keyword).
	 *
	 * @return true if property (or sub- properties) contain a FObjectProperty that is marked CPF_NeedCtorLink, false otherwise
	 */
	bool ContainsInstancedObjectProperty() const
	{
		bool bResult = HasAnyPropertyFlags(CPF_ContainsInstancedReference | CPF_InstancedReference);
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || bResult == GetPropertySafe()->ContainsInstancedObjectProperty());
#endif
		return bResult;
	}

	/**
	 * Test to see if two properties share the same types.  This method utilizes the underlying UProperty SameType implementation.
	 */
	bool SameType(const FUnrealPropertyDefinitionInfo& Other) const;

	/**
	 * Determines whether this property's type is compatible with another property's type.
	 *
	 * @param	Other							the property to check against this one.
	 *											Given the following example expressions, VarA is Other and VarB is 'this':
	 *												VarA = VarB;
	 *
	 *												function func(type VarB) {}
	 *												func(VarA);
	 *
	 *												static operator==(type VarB_1, type VarB_2) {}
	 *												if ( VarA_1 == VarA_2 ) {}
	 *
	 * @param	bDisallowGeneralization			controls whether it should be considered a match if this property's type is a generalization
	 *											of the other property's type (or vice versa, when dealing with structs
	 * @param	bIgnoreImplementedInterfaces	controls whether two types can be considered a match if one type is an interface implemented
	 *											by the other type.
	 */
	bool MatchesType(const FUnrealPropertyDefinitionInfo& Other, bool bDisallowGeneralization, bool bIgnoreImplementedInterfaces = false) const
	{
		return GetPropertyBase().MatchesType(Other.GetPropertyBase(), bDisallowGeneralization, bIgnoreImplementedInterfaces);
	}

	/**
	 * Return true if the property is a primitive type or an primitive type array
	 */
	bool IsPrimitiveOrPrimitiveStaticArray() const
	{
		return PropertyBase.IsPrimitiveOrPrimitiveStaticArray();
	}

	/**
	 * Return true if the property is a static array
	 */
	bool IsStaticArray() const
	{
		return PropertyBase.ArrayType == EArrayType::Static;
	}

	/**
	 * Return true if the property is a dynamic array
	 */
	bool IsDynamicArray() const
	{
		return PropertyBase.ArrayType == EArrayType::Dynamic;
	}

	/**
	 * Return true if the property is a map
	 */
	bool IsMap() const
	{
		return PropertyBase.ArrayType == EArrayType::None && PropertyBase.MapKeyProp.IsValid();
	}

	/**
	 * Return true if the property is a set
	 */
	bool IsSet() const
	{
		return PropertyBase.ArrayType == EArrayType::Set;
	}

	/**
	 * Return true if the property is a boolean or boolean static array
	 */
	bool IsBooleanOrBooleanStaticArray() const
	{
		return PropertyBase.IsBooleanOrBooleanStaticArray();
	}

	/**
	 * Return true if the property is an object or object static array
	 */
	bool IsObjectRefOrObjectRefStaticArray() const
	{
		return PropertyBase.IsObjectRefOrObjectRefStaticArray();
	}

	/**
	 * Return true if the property is a class or class static array 
	 * NOTE: This is specific to Object and ObjectPtr where the class derrives from UClass
	 */
	bool IsClassRefOrClassRefStaticArray() const
	{
		return PropertyBase.IsClassRefOrClassRefStaticArray();
	}

	/**
	 * Return true if this is a byte enumeration
	 */
	bool IsByteEnumOrByteEnumStaticArray() const
	{
		return PropertyBase.IsByteEnumOrByteEnumStaticArray();
	}

	/** 
	 * Return true if this is a numeric or a numeric static array
	 */
	bool IsNumericOrNumericStaticArray() const
	{
		return PropertyBase.IsNumericOrNumericStaticArray();
	}

	/**
	 * Return true if this is a struct or a struct static array
	 */
	bool IsStructOrStructStaticArray() const
	{
		return PropertyBase.IsStructOrStructStaticArray();
	}

	/**
	 * Return true if this is a delegate or a delegate static array
	 */
	bool IsDelegateOrDelegateStaticArray() const
	{
		return PropertyBase.IsDelegateOrDelegateStaticArray();
	}

	/**
	 * Return true if this is a multicast delegate or a multicast delegate static array
	 */
	bool IsMulticastDelegateOrMulticastDelegateStaticArray() const
	{
		return PropertyBase.IsMulticastDelegateOrMulticastDelegateStaticArray();
	}	

	/**
	 * Return true if this is a structure and it has no op constructor
	 */
	bool HasNoOpConstructor() const;

	/**
	 * Get the string that represents the array dimensions.  A nullptr is returned if the property doesn't have any dimensions
	 */
	const TCHAR* GetArrayDimensions() const
	{
		return ArrayDimensions.IsEmpty() ? nullptr : *ArrayDimensions;
	}
	
	/**
	 * Get the category of this property
	 */
	EVariableCategory GetVariableCategory() const
	{
		return VariableCategory;
	}

	/**
	 * Get the access specifier of this property
	 */
	EAccessSpecifier GetAccessSpecifier() const
	{
		return AccessSpecifier;
	}

	/**
	 * Return true if the property is unsized
	 */
	bool IsUnsized() const
	{
		return bUnsized;
	}

	/**
	 * Set the unsized flag
	 */
	void SetUnsized(bool bInUnsized)
	{
		bUnsized = bInUnsized;
	}

	/**
	 * Return the allocator type
	 */
	EAllocatorType GetAllocatorType() const
	{
		return AllocatorType;
	}

	/**
	 * Set the allocator type
	 */
	void SetAllocatorType(EAllocatorType InAllocatorType)
	{
		AllocatorType = InAllocatorType;
	}

	/**
	 * Return the token associated with the property parsing
	 */
	FPropertyBase& GetPropertyBase()
	{
		return PropertyBase;
	}
	const FPropertyBase& GetPropertyBase() const
	{
		return PropertyBase;
	}

	/**
	 * Return true if we have a key property def
	 */
	bool HasKeyPropDef() const
	{
		return KeyPropDef.IsValid();
	}

	/**
	 * Get the associated key property definition (valid for maps)
	 */
	FUnrealPropertyDefinitionInfo& GetKeyPropDef() const
	{
		check(KeyPropDef);
		return *KeyPropDef;
	}

	/**
	 * Get the associated key property definition (valid for maps)
	 */
	TSharedRef<FUnrealPropertyDefinitionInfo> GetKeyPropDefRef() const
	{
		check(KeyPropDef);
		return KeyPropDef.ToSharedRef();
	}

	/**
	 * Sets the associated key property definition (valid for maps)
	 */
	void SetKeyPropDef(TSharedRef<FUnrealPropertyDefinitionInfo> InKeyPropDef)
	{
		check(!KeyPropDef);
		KeyPropDef = InKeyPropDef;
	}

	/**
	 * Return true if we have a value property def
	 */
	bool HasValuePropDef() const
	{
		return ValuePropDef.IsValid();
	}

	/**
	 * Get the associated value property definition (valid for maps, sets, and dynamic arrays)
	 */
	FUnrealPropertyDefinitionInfo& GetValuePropDef() const
	{
		check(ValuePropDef);
		return *ValuePropDef;
	}

	/**
	 * Get the associated value property definition (valid for maps, sets, and dynamic arrays)
	 */
	TSharedRef<FUnrealPropertyDefinitionInfo> GetValuePropDefRef() const
	{
		check(ValuePropDef);
		return ValuePropDef.ToSharedRef();
	}

	/**
	 * Sets the associated value property definition (valid for maps, sets, and dynamic arrays)
	 */
	void SetValuePropDef(TSharedRef<FUnrealPropertyDefinitionInfo> InValuePropDef)
	{
		check(!ValuePropDef);
		ValuePropDef = InValuePropDef;
	}

	/**
	 * Get the parsing position of the property
	 */
	int32 GetParsePosition() const
	{
		return ParsePosition;
	}

	/**
	 * Return the type package name
	 */
	const FString& GetTypePackageName() const
	{
		return TypePackageName;
	}

	/**
	 * Return the replication notification function name
	 */
	FName GetRepNotifyFunc() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetPropertySafe() == nullptr || PropertyBase.RepNotifyName == GetPropertySafe()->RepNotifyFunc);
#endif
		return PropertyBase.RepNotifyName;
	}

	/**
	 * Set the delegate function signature
	 */
	void SetDelegateFunctionSignature(FUnrealFunctionDefinitionInfo& DelegateFunctionDef);

	/**
	 * Add meta data for the given definition
	 */
	virtual void AddMetaData(TMap<FName, FString>&& InMetaData) override
	{
		check(Property == nullptr);
		GetMetaDataMap().Append(MoveTemp(InMetaData));
	}

	virtual TMap<FName, FString>& GetMetaDataMap() const override
	{
		return const_cast<TMap<FName, FString>&>(PropertyBase.MetaData);
	}

	FUnrealObjectDefinitionInfo* GetOwnerObject() const;
	FUnrealStructDefinitionInfo* GetOwnerStruct() const;

	/** Linked list of properties referencing a specific field */
	FUnrealPropertyDefinitionInfo* NextReferencingProperty = nullptr;

	/**
	 * Perform any post parsing finalization that can happen specific to the containing source file
	 */
	virtual void ConcurrentPostParseFinalize() override;

protected:
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
	virtual void CheckFindMetaData(const FName& Key, const FString* ValuePtr) const
	{
		if (FProperty* LocalProperty = GetPropertySafe())
		{
			const FString* CheckPtr = LocalProperty->FindMetaData(Key);
			check((CheckPtr == nullptr && ValuePtr == nullptr) || (CheckPtr != nullptr && ValuePtr != nullptr && *CheckPtr == *ValuePtr));
		}
	}
#endif

	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) override;

	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) override;

private:
	FName Name;
	FPropertyBase PropertyBase;
	FString ArrayDimensions;
	FString TypePackageName;
	TSharedPtr<FUnrealPropertyDefinitionInfo> KeyPropDef;
	TSharedPtr<FUnrealPropertyDefinitionInfo> ValuePropDef;
	FProperty* Property = nullptr;
	int32 ParsePosition;
	EVariableCategory VariableCategory = EVariableCategory::Member;
	EAccessSpecifier AccessSpecifier = ACCESS_Public;
	EAllocatorType AllocatorType = EAllocatorType::Default;
	bool bUnsized = false;
	bool bSignatureSet = false; // temporary flag to know if we have set the signature
};

/**
 * Class that stores information about type definitions derived from UObject.
 */
class FUnrealObjectDefinitionInfo
	: public FUnrealTypeDefinitionInfo
	, public FUHTMetaData
{
public:

	virtual FUnrealObjectDefinitionInfo* AsObject() override
	{
		return this;
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UObject* GetObject() const
	{
		check(Object);
		return Object;
	}

	/**
	 * Return the Engine instance associated with the compiler instance without the check that it is set
	 */
	UObject* GetObjectSafe() const
	{
		return Object;
	}

	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString ClassName(TEXT("Object"));
		return ClassName;
	}

	/**
	 * Set the Engine instance associated with the compiler instance
	 */
	virtual void SetObject(UObject* InObject)
	{
		check(InObject != nullptr);
		check(Object == nullptr);
		Object = InObject;
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(Object->GetFName() == Name);
		check(Object->GetInternalFlags() == InternalObjectFlags);
#endif
	}

	/**
	 * Returns the name of this object (with no path information)
	 */
	virtual FString GetName() const override
	{
		FString Result = GetFName().ToString();
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetObjectSafe() == nullptr || GetObjectSafe()->GetName() == Result);
#endif
		return Result;
	}

	/** Returns the logical name of this object */
	virtual FName GetFName() const override
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetObjectSafe() == nullptr || GetObjectSafe()->GetFName() == Name);
#endif
		return Name;
	}

	virtual FString GetFullName() const override;

	/**
	 * Returns the fully qualified pathname for this object, in the format:
	 * 'Outermost.[Outer:]Name'
	 */
	virtual FString GetPathName(FUnrealObjectDefinitionInfo* StopOuter = nullptr) const override;
	virtual void GetPathName(FUnrealObjectDefinitionInfo* StopOuter, FStringBuilderBase& ResultString) const override;

	/**
	 * Returns the fully qualified pathname for this object as an FTopLevelAssetPath
	 */
	virtual FTopLevelAssetPath GetStructPathName() const override;

	/**
	 * Walks up the chain of packages until it reaches the top level, which it ignores.
	 *
	 * @param	bStartWithOuter		whether to include this object's name in the returned string
	 * @return	string containing the path name for this object, minus the outermost-package's name
	 */
	virtual FString GetFullGroupName(bool bStartWithOuter) const override;

	/**
	 * Return the package associated with this object
	 */
	FUnrealPackageDefinitionInfo& GetPackageDef() const;

	/**
	 * Return the "outer" object that contains the definition for this object
	 */
	FUnrealObjectDefinitionInfo* GetOuter() const
	{
		return static_cast<FUnrealObjectDefinitionInfo*>(FUnrealTypeDefinitionInfo::GetOuter());
	}

	/**
	 * Return the flags that were parsed as part of the pre-parse phase
	 */
	EInternalObjectFlags GetInternalFlags() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetObjectSafe() == nullptr || GetObjectSafe()->GetInternalFlags() == InternalObjectFlags);
#endif
		return InternalObjectFlags;
	}

	/**
	 * Sets the given flags
	 */
	void SetInternalFlags(EInternalObjectFlags FlagsToSet)
	{
		InternalObjectFlags |= FlagsToSet;
		if (UObject* Obj = GetObjectSafe())
		{
			Obj->SetInternalFlags(FlagsToSet);
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
			check(Obj->GetInternalFlags() == InternalObjectFlags);
#endif
		}
	}

	/**
	 * Clears the given flags
	 */
	void ClearInternalFlags(EInternalObjectFlags FlagsToClear)
	{
		InternalObjectFlags &= ~FlagsToClear;
		if (UObject* Obj = GetObjectSafe())
		{
			Obj->ClearInternalFlags(FlagsToClear);
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
			check(Obj->GetInternalFlags() == InternalObjectFlags);
#endif
		}
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagsToCheck		Flag(s) to check for
	 * @return	true if the passed in flag is set, false otherwise
	 */
	bool HasAnyInternalFlags(EInternalObjectFlags FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetObjectSafe() == nullptr || GetObjectSafe()->GetInternalFlags() == InternalObjectFlags);
#endif
		return EnumHasAnyFlags(InternalObjectFlags, FlagsToCheck);
	}

	/**
	 * Helper method to see if native
	 */
	bool IsNative() const
	{
		return HasAnyInternalFlags(EInternalObjectFlags::Native);
	}

	/**
	 * Return the meta data in map form
	 */
	virtual TMap<FName, FString> GenerateMetadataMap() const override
	{
		TMap<FName, FString> Map;
		for (const TPair<FName, FString>& MetaKeyValue : GetMetaDataMap())
		{
			FString Key = MetaKeyValue.Key.ToString();
			if (!Key.StartsWith(TEXT("/Script")))
			{
				Map.Add(MetaKeyValue.Key, MetaKeyValue.Value);
			}
		}
		return Map;
	}

	virtual TMap<FName, FString>& GetMetaDataMap() const override
	{
		return const_cast<TMap<FName, FString>&>(MetaData);
	}

	/**
	 * Add meta data for the given definition
	 */
	virtual void AddMetaData(TMap<FName, FString>&& InMetaData) override;

protected:
	virtual void SetMetaDataHelper(const FName& Key, const TCHAR* InValue) override;

	explicit FUnrealObjectDefinitionInfo(UObject* Object);

	FUnrealObjectDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, FString&& InNameCPP, FName InName, FUnrealObjectDefinitionInfo& InOuter)
		: FUnrealTypeDefinitionInfo(InSourceFile, InLineNumber, MoveTemp(InNameCPP), InOuter)
		, Name(InName)
	{ }

#if UHT_ENABLE_ENGINE_TYPE_CHECKS
	virtual void CheckFindMetaData(const FName& Key, const FString* ValuePtr) const override
	{
		if (UObject* Obj = GetObjectSafe())
		{
			TMap<FName, FString>* PackageMap = GetUObjectMetaDataMap();
			const FString* CheckPtr = PackageMap ? PackageMap->Find(Key) : nullptr;
			check((CheckPtr == nullptr && ValuePtr == nullptr) || (CheckPtr != nullptr && ValuePtr != nullptr && *CheckPtr == *ValuePtr));
		}
	}
#endif

	UMetaData* GetUObjectMetaData() const
	{
		if (UObject* Obj = GetObjectSafe())
		{
			UPackage* Package = Obj->GetPackage();
			check(Package);
			UMetaData* Metadata = Package->GetMetaData();
			check(Metadata);
			return Metadata;
		}
		return nullptr;
	}

	TMap<FName, FString>* GetUObjectMetaDataMap() const
	{
		UMetaData* Metadata = GetUObjectMetaData();
		return Metadata ? Metadata->ObjectMetaDataMap.Find(GetObject()) : nullptr;
	}

private:
	TMap<FName, FString> MetaData;
	UObject* Object = nullptr;
	FName Name = NAME_None;
	EInternalObjectFlags InternalObjectFlags = EInternalObjectFlags::None;
};

/**
 * Class that stores information about packages.
 */
class FUnrealPackageDefinitionInfo 
	: public FUnrealObjectDefinitionInfo
{
public:
	// Constructor
	FUnrealPackageDefinitionInfo(const FManifestModule& InModule, UPackage* InPackage);

	virtual FUnrealPackageDefinitionInfo* AsPackage() override
	{
		return this;
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UPackage* GetPackage() const
	{
		return static_cast<UPackage*>(GetObject());
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UPackage* GetPackageSafe() const
	{
		return static_cast<UPackage*>(GetObjectSafe());
	}

	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString ClassName(TEXT("Package"));
		return ClassName;
	}

	/**
	 * Return the module information from the manifest associated with this package
	 */
	const FManifestModule& GetModule()
	{
		return Module;
	}

	/**
	 * Return a collection of all source files contained within this package.
	 * This collection is always valid.
	 */
	TArray<TSharedRef<FUnrealSourceFile>>& GetAllSourceFiles()
	{
		return AllSourceFiles;
	}

	/**
	 * Return a collection of all classes associated with this package.  This is not valid until parsing begins.
	 */
	TArray<TSharedRef<FUnrealTypeDefinitionInfo>>& GetAllClasses()
	{
		return AllClasses;
	}

	/**
	 * If true, this package should generate the classes H file.  This is not valid until code generation begins.
	 */
	bool GetWriteClassesH() const
	{
		return bWriteClassesH;
	}

	/**
	 * Set the flag indicating that the classes H file should be generated.
	 */
	void SetWriteClassesH(bool bInWriteClassesH)
	{
		bWriteClassesH = bInWriteClassesH;
	}

	/**
	 * Return a string that references the "PACKAGE_API " macro with a trailing space.
	 */
	const FString& GetAPI() const
	{
		return API;
	}

	/**
	 * Get the short name of the package uppercased.
	 */
	const FString& GetShortUpperName() const
	{
		return ShortUpperName;
	}

	/**
	 * Add a unique cross module reference for this field
	 */
	void AddCrossModuleReference(TSet<FString>* UniqueCrossModuleReferences) const;

	/**
	 * Return the name of the singleton for this field.  Only valid post parsing
	 */
	const FString& GetSingletonName() const
	{
		return SingletonName;
	}

	/**
	 * Return the name of the singleton without the trailing "()" for this field.  Only valid post parsing
	 */
	const FString& GetSingletonNameChopped() const
	{
		return SingletonNameChopped;
	}

	/**
	 * Return the external declaration for this field.  Only valid post parsing
	 */
	const FString& GetExternDecl() const
	{
		return ExternDecl;
	}

protected:
	/**
	 * Create UObject engine types
	 */
	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) override;

	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) override;

private:
	const FManifestModule& Module;
	TArray<TSharedRef<FUnrealSourceFile>> AllSourceFiles;
	TArray<TSharedRef<FUnrealTypeDefinitionInfo>> AllClasses;
	FString SingletonName;
	FString SingletonNameChopped;
	FString ExternDecl;
	FString ShortUpperName;
	FString API;
	bool bWriteClassesH = false;
};

/**
 * Class that stores information about type definitions derived from UField.
 */
class FUnrealFieldDefinitionInfo 
	: public FUnrealObjectDefinitionInfo
{
public:
	struct FDefinitionRange
	{
		const TCHAR* Start = nullptr;
		const TCHAR* End = nullptr;
	};

protected:
	using FUnrealObjectDefinitionInfo::FUnrealObjectDefinitionInfo;

public:

	virtual FUnrealFieldDefinitionInfo* AsField() override
	{
		return this;
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UField* GetField() const
	{
		return static_cast<UField*>(GetObject());
	}

	/**
	 * Return the Engine instance associated with the compiler instance without the check that it is set
	 */
	UField* GetFieldSafe() const
	{
		return static_cast<UField*>(GetObjectSafe());
	}

	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString ClassName(TEXT("Field"));
		return ClassName;
	}

	/**
	* Finds the localized tooltip or native tooltip as a fallback.
	*
	* @param bShortTooltip Look for a shorter version of the tooltip (falls back to the long tooltip if none was specified)
	*
	* @return The tooltip for this object.
	*/
	FText GetToolTipText(bool bShortTooltip = false) const;

	/**
	 * Add a unique cross module reference for this field
	 */
	void AddCrossModuleReference(TSet<FString>* UniqueCrossModuleReferences, bool bRequiresValidObject) const;

	/**
	 * Return the name of the singleton for this field.  Only valid post parsing
	 */
	const FString& GetSingletonName(bool bRequiresValidObject) const
	{
		return SingletonName[bRequiresValidObject];
	}

	/**
	 * Return the name of the singleton without the trailing "()" for this field.  Only valid post parsing
	 */
	const FString& GetSingletonNameChopped(bool bRequiresValidObject) const
	{
		return SingletonNameChopped[bRequiresValidObject];
	}

	/**
	 * Return the external declaration for this field.  Only valid post parsing
	 */
	const FString& GetExternDecl(bool bRequiresValidObject) const
	{
		return ExternDecl[bRequiresValidObject];
	}

	/** 
	 * Return the type package name
	 */
	const FString& GetTypePackageName() const
	{
		return TypePackageName;
	}

	/**
	 * Return the owning class
	 */
	FUnrealClassDefinitionInfo* GetOwnerClass() const;

	/**
	 * Add a referencing property
	 */
	void AddReferencingProperty(FUnrealPropertyDefinitionInfo& PropertyDef);

	/**
	 * Return the first property that references this field
	 */
	FUnrealPropertyDefinitionInfo* GetFirstReferencingProperty();

	/**
	 * Invoke the post parse finalize method on all referenced properties
	 */
	void PostParseFinalizeReferencedProperties();

	/**
	 * Return the definition range of the structure
	 */
	FDefinitionRange& GetDefinitionRange()
	{
		return DefinitionRange;
	}

	/**
	 * Return the definition range of the structure
	 */
	const FDefinitionRange& GetDefinitionRange() const
	{
		return DefinitionRange;
	}

	/**
	 * Verify we have a valid definition range
	 */
	void ValidateDefinitionRange()
	{
		if (DefinitionRange.End <= DefinitionRange.Start)
		{
			Throwf(TEXT("The definition range is invalid. Most probably caused by previous parsing error."));
		}
	}

	/**
	 * Perform any post parsing finalization that can happen specific to the containing source file
	 */
	virtual void ConcurrentPostParseFinalize() override;

protected:
	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) override;

private:
	FString SingletonName[2];
	FString SingletonNameChopped[2];
	FString ExternDecl[2];
	FString TypePackageName;
	FDefinitionRange DefinitionRange;

	/** Linked list of all properties that reference this field */
	std::atomic<FUnrealPropertyDefinitionInfo*> ReferencingProperties = nullptr;
};

/**
 * Class that stores information about type definitions derived from UEnum
 */
class FUnrealEnumDefinitionInfo : public FUnrealFieldDefinitionInfo
{
public:
	FUnrealEnumDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, FString&& InNameCPP, FName InName, UEnum::ECppForm InCppForm, EUnderlyingEnumType InUnderlyingType);

	virtual FUnrealEnumDefinitionInfo* AsEnum() override
	{
		return this;
	}

	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString ClassName(TEXT("Enum"));
		return ClassName;
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UEnum* GetEnum() const
	{
		return static_cast<UEnum*>(GetObject());
	}

	/**
	 * Return the Engine instance associated with the compiler instance but does not check to see if it is set
	 */
	UEnum* GetEnumSafe() const
	{
		return static_cast<UEnum*>(GetObjectSafe());
	}

	/**
	 * Test if the enum contains and of the given flags
	 */
	bool HasAnyEnumFlags(EEnumFlags InFlags) const
	{
		bool bResults = EnumHasAnyFlags(EnumFlags, InFlags);
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetEnumSafe() == nullptr || GetEnumSafe()->HasAnyEnumFlags(InFlags) == bResults);
#endif
		return bResults;
	}

	/**
	 * Tests if the enum contains a MAX value
	 *
	 * @return	true if the enum contains a MAX enum, false otherwise.
	 */
	bool ContainsExistingMax() const;

	/**
	 * Return the enumeration values
	 */
	const TArray<TPair<FName, int64>>& GetEnums()
	{
		return Names;
	}

	/**
	 * @return The number of enum names.
	 */
	int32 NumEnums() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetEnumSafe() == nullptr || GetEnumSafe()->NumEnums() == Names.Num());
#endif
		return Names.Num();
	}

	/** Gets max value of Enum. Defaults to zero if there are no entries. */
	int64 GetMaxEnumValue() const;

	/** Gets enum value by index in Names array. Asserts on invalid index */
	int64 GetValueByIndex(int32 Index) const
	{
		check(Names.IsValidIndex(Index));
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetEnumSafe() == nullptr || GetEnumSafe()->GetValueByIndex(Index) == Names[Index].Value);
#endif
		return Names[Index].Value;
	}

	/** Gets enum name by index in Names array. Returns NAME_None if Index is not valid. */
	FName GetNameByIndex(int32 Index) const;

	/** Gets index of name in enum, returns INDEX_NONE and optionally errors when name is not found. This is faster than ByNameString if the FName is exact, but will fall back if needed */
	int32 GetIndexByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const;

	/** Checks if enum has entry with given value. Includes autogenerated _MAX entry. */
	bool IsValidEnumValue(int64 InValue) const;

	/**
	 * Returns the type of enum: whether it's a regular enum, namespaced enum or C++11 enum class.
	 *
	 * @return The enum type.
	 */
	UEnum::ECppForm GetCppForm() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetEnumSafe() == nullptr || GetEnumSafe()->GetCppForm() == CppForm);
#endif
		return CppForm;
	}

	/**
	 * Returns the type of the enum
	 */
	const FString& GetCppType() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetEnumSafe() == nullptr || GetEnumSafe()->CppType == CppType);
#endif
		return CppType;
	}

	/**
	 * Sets the enum type
	 */
	void SetCppType(FString&& InCppType)
	{
		CppType = MoveTemp(InCppType);
		if (GetEnumSafe() != nullptr)
		{
			GetEnumSafe()->CppType = CppType;
		}
	}

	/**
	 * Find the longest common prefix of all items in the enumeration.
	 *
	 * @return	the longest common prefix between all items in the enum.  If a common prefix
	 *			cannot be found, returns the full name of the enum.
	 */
	FString GenerateEnumPrefix() const;

	/**
	 * Sets the array of enums.
	 *
	 * @param InNames List of enum names.
	 * @param InCppForm The form of enum.
	 * @param bAddMaxKeyIfMissing Should a default Max item be added.
	 * @return	true unless the MAX enum already exists and isn't the last enum.
	 */
	bool SetEnums(TArray<TPair<FName, int64>>& InNames, UEnum::ECppForm InCppForm, EEnumFlags InFlags = EEnumFlags::None)
	{
		Names = InNames;
		check(CppForm == InCppForm);
		EnumFlags = InFlags;
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		if (UEnum* Enum = GetEnumSafe())
		{
			Enum->SetEnums(InNames, InCppForm, InFlags, false /*bAddMaxKeyIfMissing*/);
			check(Enum->GetCppForm() == InCppForm);
		}
#endif
		return true;
	}

	/**
	 * Returns the underlying enumeration type
	 */
	EUnderlyingEnumType GetUnderlyingType() const
	{
		return UnderlyingType;
	}

	/**
	 * Return true if the enumeration is editor only
	 */
	bool IsEditorOnly() const
	{
		return bIsEditorOnly;
	}

	/**
	 * Make the enumeration editor only
	 */
	void MakeEditorOnly()
	{
		bIsEditorOnly = true;
	}

protected:
	/**
	 * Create the shell of the UObject.  Only invoked if the object has not been set
	 */
	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) override;

	virtual FString GetMetaDataIndexName(int32 NameIndex) const
	{
		FString EnumName = GetNameStringByIndex(NameIndex);
		check(!EnumName.IsEmpty());
		return EnumName;
	}

	virtual const TCHAR* GetMetaDataRemapConfigName() const
	{
		return TEXT("EnumRemap");
	}

	FString GenerateFullEnumName(const TCHAR* InEnumName) const;
	int32 GetIndexByNameString(const FString& InSearchString, EGetByNameFlags Flags) const;
	FString GetNameStringByIndex(int32 InIndex) const;

private:
	TArray<TPair<FName, int64>> Names;
	FString CppType;
	EUnderlyingEnumType UnderlyingType = EUnderlyingEnumType::Unspecified;
	EEnumFlags EnumFlags = EEnumFlags::None;
	UEnum::ECppForm CppForm = UEnum::ECppForm::Regular;
	bool bIsEditorOnly = false;
};

/**
 * Class that stores information about type definitions derived from UStruct
 */
class FUnrealStructDefinitionInfo : public FUnrealFieldDefinitionInfo
{
public:
	struct FBaseStructInfo
	{
		FString Name;
		FUnrealStructDefinitionInfo* Struct = nullptr;
	};

public:
	virtual FUnrealStructDefinitionInfo* AsStruct() override
	{
		return this;
	}

	virtual TSharedRef<FScope> GetScope() override;

	virtual void SetObject(UObject* InObject) override;

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UStruct* GetStruct() const
	{
		return static_cast<UStruct*>(GetObject());
	}

	/**
	 * Return the Engine instance associated with the compiler instance without the null check
	 */
	UStruct* GetStructSafe() const
	{
		return static_cast<UStruct*>(GetObjectSafe());
	}

	/**
	 * @return true if this object is of the specified type.
	 */
	bool IsChildOf(const FUnrealStructDefinitionInfo& SomeBase) const;

	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString ClassName(TEXT("Struct"));
		return ClassName;
	}

	/**
	 * Return the CPP prefix 
	 */
	virtual const TCHAR* GetPrefixCPP() const
	{
		const TCHAR* ReturnValue = TEXT("F");
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		if (UStruct* Struct = GetStructSafe())
		{
			const TCHAR* EngineValue = Struct->GetPrefixCPP();
			check(FCString::Strcmp(EngineValue, ReturnValue) == 0);
		}
#endif
		return ReturnValue;
	}

	/**
	 * Returns the name used for declaring the passed in struct in C++
	 * 
	 * NOTE: This does not match the CPP name parsed from the header file.
	 *
	 * @param	bForceInterface If true, force an 'I' prefix
	 * @return	Name used for C++ declaration
	 */
	FString GetAlternateNameCPP(bool bForceInterface = false)
	{
		return FString::Printf(TEXT("%s%s"), (bForceInterface ? TEXT("I") : GetPrefixCPP()), *GetName());
	}

	/**
	 * Create the underlying properties for the structure
	 */
	void CreatePropertyEngineTypes();

	/**
	 * Add a new property to the structure
	 */
	virtual void AddProperty(TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef);

	/**
	 * Get the collection of properties
	 */
	const TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& GetProperties() const
	{
		return Properties;
	}
	TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& GetProperties()
	{
		return Properties;
	}

	/**
	 * Add a new function to the structure
	 */
	virtual void AddFunction(TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef);

	/**
	 * Get the collection of functions
	 */
	const TArray< TSharedRef<FUnrealFunctionDefinitionInfo>>& GetFunctions() const
	{
		return Functions;
	}
	TArray<TSharedRef<FUnrealFunctionDefinitionInfo>>& GetFunctions()
	{
		return Functions;
	}

	/**
	 * Return the super struct
	 */
	FUnrealStructDefinitionInfo* GetSuperStruct() const
	{
		return SuperStructInfo.Struct;
	}

	/**
	 * Return the super struct information
	 */
	const FBaseStructInfo& GetSuperStructInfo() const
	{
		return SuperStructInfo;
	}

	FBaseStructInfo& GetSuperStructInfo()
	{
		return SuperStructInfo;
	}

	/**
	 * Return the base struct information
	 */
	const TArray<FBaseStructInfo>& GetBaseStructInfos() const
	{
		return BaseStructInfos;
	}

	TArray<FBaseStructInfo>& GetBaseStructInfos()
	{
		return BaseStructInfos;
	}

	/**
	 * Get the contains delegates flag
	 */
	bool ContainsDelegates() const
	{
		return bContainsDelegates;
	}

	const FString* FindMetaDataHierarchical(const FName& Key) const
	{
		if (Key == NAME_None)
		{
			return nullptr;
		}

		for (const FUnrealStructDefinitionInfo* StructDef = this; StructDef; StructDef = StructDef->GetSuperStruct())
		{
			if (const FString* Found = StructDef->FindMetaData(Key))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** Try and find boolean metadata with the given key. If not found on this class, work up hierarchy looking for it. */
	bool GetBoolMetaDataHierarchical(const FName& Key) const
	{
		const FString* Found = FindMetaDataHierarchical(Key);
		// FString == operator does case insensitive comparison
		return Found != nullptr && *Found == "true";
	}

	/** Try and find string metadata with the given key. If not found on this class, work up hierarchy looking for it. */
	bool GetStringMetaDataHierarchical(const FName& Key, FString* OutValue = nullptr) const
	{
		const FString* Found = FindMetaDataHierarchical(Key);
		if (OutValue)
		{
			*OutValue = Found ? *Found : FString();
		}
		return Found != nullptr;
	}

	/**
	 * Get the generated code version
	 */
	EGeneratedCodeVersion GetGeneratedCodeVersion() const
	{
		return GeneratedCodeVersion;
	}

	/**
	 * Set the generated code version
	 */
	void SetGeneratedCodeVersion(EGeneratedCodeVersion InGeneratedCodeVersion)
	{
		GeneratedCodeVersion = InGeneratedCodeVersion;
	}

	/**
	 * Get if we have a generated body
	 */
	bool HasGeneratedBody() const
	{
		return bHasGeneratedBody;
	}

	/**
	 * Mark that we have a generated body
	 */
	void MarkGeneratedBody()
	{
		bHasGeneratedBody = true;
	}

	/**
	 * Get the RigVM information
	 */
	FRigVMStructInfo& GetRigVMInfo()
	{
		return RigVMInfo;
	}

	/**
	 * Add a declaration to the struct
	 */
	void AddDeclaration(uint32 CurrentCompilerDirective, TArray<FToken>&& Tokens)
	{
		Declarations.Emplace(FDeclaration{ CurrentCompilerDirective, MoveTemp(Tokens) });
	}

	/** 
	 * Get all of the declarations
	 */
	const TArray<FDeclaration>& GetDeclarations() const
	{
		return Declarations;
	}

	/**
	 * Perform any post parsing finalization that can happen specific to the containing source file
	 */
	virtual void ConcurrentPostParseFinalize() override;

protected:
	FUnrealStructDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, FString&& InNameCPP, FName InName, FUnrealObjectDefinitionInfo& InOuter);

	/**
	 * Create UObject engine types
	 */
	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) override;

	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) override;

private:
	TSharedPtr<FScope> StructScope;

	/** Properties of the structure */
	TArray<TSharedRef<FUnrealPropertyDefinitionInfo>> Properties;

	/** Functions of the structure */
	TArray<TSharedRef<FUnrealFunctionDefinitionInfo>> Functions;

	/** Other declarations */
	TArray<FDeclaration> Declarations;

	FBaseStructInfo SuperStructInfo;
	TArray<FBaseStructInfo> BaseStructInfos;

	FRigVMStructInfo RigVMInfo;

	EGeneratedCodeVersion GeneratedCodeVersion = FUHTConfig::Get().DefaultGeneratedCodeVersion;

	/** whether this struct declares delegate functions or properties */
	bool bContainsDelegates = false;

	/** If true, this struct contains the generated body macro */
	bool bHasGeneratedBody = false;
};

/**
 * Class that stores information about type definitions derived from UScriptStruct
 */
class FUnrealScriptStructDefinitionInfo : public FUnrealStructDefinitionInfo
{
public:
	FUnrealScriptStructDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, FString&& InNameCPP, FName InName);

	virtual FUnrealScriptStructDefinitionInfo* AsScriptStruct() override
	{
		return this;
	}

	virtual uint32 GetHash(FUnrealTypeDefinitionInfo& ReferencingDef, bool bIncludeNoExport = true) const override;

	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString ClassName(TEXT("ScriptStruct"));
		return ClassName;
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UScriptStruct* GetScriptStruct() const
	{
		return static_cast<UScriptStruct*>(GetObject());
	}

	/**
	 * Return the Engine instance associated with the compiler instance without the null check
	 */
	UScriptStruct* GetScriptStructSafe() const
	{
		return static_cast<UScriptStruct*>(GetObjectSafe());
	}

	/**
	 * Return the super script struct
	 */
	FUnrealScriptStructDefinitionInfo* GetSuperScriptStruct() const;

	/**
	 * Return the struct flags
	 */
	EStructFlags GetStructFlags() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetScriptStructSafe() == nullptr || (GetScriptStructSafe()->StructFlags & ~STRUCT_ComputedFlags) == StructFlags);
#endif
		return StructFlags;
	}

	/**
	 * Sets the given struct flags
	 */
	void SetStructFlags(EStructFlags FlagsToSet)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		if (UScriptStruct* Struct = GetScriptStructSafe())
		{
			check((Struct->StructFlags & ~STRUCT_ComputedFlags) == StructFlags);
			Struct->StructFlags = EStructFlags(int32(Struct->StructFlags) | int32(FlagsToSet));
		}
#endif
		StructFlags = EStructFlags(int32(StructFlags) | int32(FlagsToSet));
	}

	/**
	 * Clears the given flags
	 */
	void ClearStructFlags(EStructFlags FlagsToClear)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetScriptStructSafe() == nullptr || (GetScriptStructSafe()->StructFlags & ~STRUCT_ComputedFlags) == StructFlags);
#endif
		StructFlags = EStructFlags(int32(StructFlags) & int32(~FlagsToClear));
		if (UScriptStruct* Struct = GetScriptStructSafe())
		{
			Struct->StructFlags = EStructFlags(int32(Struct->StructFlags) & int32(~FlagsToClear));
		}
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		Class flag to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	bool HasAnyStructFlags(EStructFlags FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		if (GetScriptStructSafe())
		{
			int32 a = GetScriptStructSafe()->StructFlags & ~STRUCT_ComputedFlags;
			check(StructFlags == a);
		}
		check(GetScriptStructSafe() == nullptr || (GetScriptStructSafe()->StructFlags & ~STRUCT_ComputedFlags) == StructFlags);
#endif
		return EnumHasAnyFlags(StructFlags, FlagsToCheck);
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Function flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	bool HasAllStructFlags(EStructFlags FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetScriptStructSafe() == nullptr || (GetScriptStructSafe()->StructFlags & ~STRUCT_ComputedFlags) == StructFlags);
#endif
		return EnumHasAllFlags(StructFlags, FlagsToCheck);
	}

	/**
	 * Used to safely check whether specific of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Function flags to check for
	 * @param ExpectedFlags The flags from the flags to check that should be set
	 * @return true if specific of the passed in flags are set (including no flags passed in), false otherwise
	 */
	bool HasSpecificStructFlags(EStructFlags FlagsToCheck, EStructFlags ExpectedFlags) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetScriptStructSafe() == nullptr || (GetScriptStructSafe()->StructFlags & ~STRUCT_ComputedFlags) == StructFlags);
#endif
		return (StructFlags & FlagsToCheck) == ExpectedFlags;
	}

	/**
	 * If it is native, it is assumed to have defaults because it has a constructor
	 * @return true if this struct has defaults
	 */
	bool HasDefaults() const
	{
		return UScriptStruct::FindDeferredCppStructOps(GetStructPathName()) != nullptr;
	}


	int32 GetMacroDeclaredLineNumber() const
	{
		return MacroDeclaredLineNumber;
	}

	void SetMacroDeclaredLineNumber(int32 InMacroDeclaredLineNumber)
	{
		MacroDeclaredLineNumber = InMacroDeclaredLineNumber;
	}

	bool HasNoOpConstructor() const;

	/**
	 * Handles the propagation of property flags
	 */
	virtual void OnPostParsePropertyFlagsChanged(FUnrealPropertyDefinitionInfo& PropertyDef) override;

protected:
	/**
	 * Create UObject engine types
	 */
	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) override;

	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) override;

private:

	void SetHasInstancedReference();

	int32 MacroDeclaredLineNumber = INDEX_NONE;
	EStructFlags StructFlags = STRUCT_NoFlags;
};

/**
 * Class that stores information about type definitions derrived from UFunction
 */
class FUnrealFunctionDefinitionInfo : public FUnrealStructDefinitionInfo
{
public:
	FUnrealFunctionDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, FString&& InNameCPP, FName InName, FUnrealObjectDefinitionInfo& InOuter, FFuncInfo&& InFuncInfo, EFunctionType InFunctionType)
		: FUnrealStructDefinitionInfo(InSourceFile, InLineNumber, MoveTemp(InNameCPP), InName, InOuter)
		, FunctionData(MoveTemp(InFuncInfo))
		, FunctionType(InFunctionType)
	{ 
	}

	virtual FUnrealFunctionDefinitionInfo* AsFunction() override
	{
		return this;
	}

	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString FunctionClassName(TEXT("Function"));
		static FString DelegateFunctionClassName(TEXT("DelegateFunction"));
		static FString SparseDelegateFunctionClassName(TEXT("SparseDelegateFunction"));

		if (bRootClassName)
		{
			return FunctionClassName;
		}

		switch (FunctionType)
		{
		case EFunctionType::Function:
			return FunctionClassName;
		case EFunctionType::Delegate:
			return DelegateFunctionClassName;
		case EFunctionType::SparseDelegate:
			return SparseDelegateFunctionClassName;
		default:
			check(false);
			return FunctionClassName;
		}
	}

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UFunction* GetFunction() const
	{
		return static_cast<UFunction*>(GetObject());
	}

	/**
	 * Return the Engine instance associated with the compiler instance without the check to see if it is set
	 */
	UFunction* GetFunctionSafe() const
	{
		return static_cast<UFunction*>(GetObjectSafe());
	}

	/**
	 * Return the function flags
	 */
	EFunctionFlags GetFunctionFlags() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetFunctionSafe() == nullptr || GetFunctionSafe()->FunctionFlags == FunctionData.FunctionFlags);
#endif
		return FunctionData.FunctionFlags;
	}

	/**
	 * Sets the given flags
	 */
	void SetFunctionFlags(EFunctionFlags FlagsToSet)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetFunctionSafe() == nullptr || GetFunctionSafe()->FunctionFlags == FunctionData.FunctionFlags);
#endif
		if (GetFunctionSafe())
		{
			GetFunctionSafe()->FunctionFlags |= FlagsToSet;
		}
		FunctionData.FunctionFlags |= FlagsToSet;
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagToCheck		Class flag to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	bool HasAnyFunctionFlags(EFunctionFlags FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetFunctionSafe() == nullptr || GetFunctionSafe()->FunctionFlags == FunctionData.FunctionFlags);
#endif
		return EnumHasAnyFlags(FunctionData.FunctionFlags, FlagsToCheck);
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Function flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	bool HasAllFunctionFlags(EFunctionFlags FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetFunctionSafe() == nullptr || GetFunctionSafe()->FunctionFlags == FunctionData.FunctionFlags);
#endif
		return EnumHasAllFlags(FunctionData.FunctionFlags, FlagsToCheck);
	}

	/**
	 * Used to safely check whether specific of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Function flags to check for
	 * @param ExpectedFlags The flags from the flags to check that should be set
	 * @return true if specific of the passed in flags are set (including no flags passed in), false otherwise
	 */
	bool HasSpecificFunctionFlags(EFunctionFlags FlagsToCheck, EFunctionFlags ExpectedFlags) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetFunctionSafe() == nullptr || GetFunctionSafe()->FunctionFlags == FunctionData.FunctionFlags);
#endif
		EFunctionFlags Flags = FunctionData.FunctionFlags;
		return (Flags & FlagsToCheck) == ExpectedFlags;
	}

	FFuncInfo& GetFunctionData() { return FunctionData; }
	const FFuncInfo& GetFunctionData() const { return FunctionData; }
	FUnrealPropertyDefinitionInfo* GetReturn() const { return ReturnProperty.Get(); }

	/**
	 * Adds a new function property to be tracked.  Determines whether the property is a
	 * function parameter, local property, or return value, and adds it to the appropriate
	 * list
	 */
	virtual void AddProperty(TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef) override;

	/**
	 * Sets the specified function export flags
	 */
	void SetFunctionExportFlags(uint32 NewFlags)
	{
		FunctionData.FunctionExportFlags |= NewFlags;
	}

	/**
	 * Clears the specified function export flags
	 */
	void ClearFunctionExportFlags(uint32 ClearFlags)
	{
		FunctionData.FunctionExportFlags &= ~ClearFlags;
	}

	/**
	 * Return the super function
	 */
	FUnrealFunctionDefinitionInfo* GetSuperFunction() const;

	/**
	 * Get the name of the sparse owning class name
	 */
	FName GetSparseOwningClassName() const
	{
		return SparseOwningClassName;
	}

	/**
	 * Set the name of the sparse owning class name
	 */
	void SetSparseOwningClassName(FName InSparseOwningClassName)
	{
		SparseOwningClassName = InSparseOwningClassName;
	}

	/**
	 * Get the name of the sparse delegate name
	 */
	FName GetSparseDelegateName() const
	{
		return SparseDelegateName;
	}

	/**
	 * Set the name of the sparse delegate name
	 */
	void SetSparseDelegateName(FName InSparseDelegateName)
	{
		SparseDelegateName = InSparseDelegateName;
	}

	EFunctionType GetFunctionType() const
	{
		return FunctionType;
	}

	bool IsDelegateFunction() const
	{
		return FunctionType == EFunctionType::Delegate || FunctionType == EFunctionType::SparseDelegate;
	}

protected:
	/**
	 * Create UObject engine types
	 */
	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) override;

	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) override;

private:

	/** info about the function associated with this FFunctionData */
	FFuncInfo FunctionData;

	/** return value for this function */
	TSharedPtr<FUnrealPropertyDefinitionInfo> ReturnProperty;

	FName SparseOwningClassName = NAME_None;
	FName SparseDelegateName = NAME_None;
	EFunctionType FunctionType = EFunctionType::Function;
};

/**
 * Class that stores information about type definitions derived from UClass
 */
class FUnrealClassDefinitionInfo
	: public FUnrealStructDefinitionInfo
{
public:
	FUnrealClassDefinitionInfo(FUnrealSourceFile& InSourceFile, int32 InLineNumber, FString&& InNameCPP, FName InName, bool bInIsInterface);

	/**
	 * Attempts to find a class definition based on the given name
	 */
	static FUnrealClassDefinitionInfo* FindClass(const TCHAR* ClassName);

	/**
	 * Attempts to find a script class based on the given name. Will attempt to strip
	 * the prefix of the given name while searching. Throws an exception with the script error
	 * if the class could not be found.
	 *
	 * @param Parser Calling context for exceptions
	 * @param InClassName Name w/ Unreal prefix to use when searching for a class
	 * @return The found class.
	 */
	static FUnrealClassDefinitionInfo* FindScriptClassOrThrow(const FHeaderParser& Parser, const FString& InClassName);

	/**
	 * Attempts to find a script class based on the given name. Will attempt to strip
	 * the prefix of the given name while searching. Optionally returns script errors when appropriate.
	 *
	 * @param InClassName  Name w/ Unreal prefix to use when searching for a class
	 * @param OutErrorMsg  Error message (if any) giving the caller flexibility in how they present an error
	 * @return The found class, or NULL if the class was not found.
	 */
	static FUnrealClassDefinitionInfo* FindScriptClass(const FString& InClassName, FString* OutErrorMsg = nullptr);

	virtual FUnrealClassDefinitionInfo* AsClass() override
	{
		return this;
	}
	/**
	 * Return the engine class name
	 */
	virtual const FString& GetEngineClassName(bool bRootClassName = false) const
	{
		static FString ClassName(TEXT("Class"));
		return ClassName;
	}

	/**
	 * Return the CPP prefix
	 */
	virtual const TCHAR* GetPrefixCPP() const;

	virtual uint32 GetHash(FUnrealTypeDefinitionInfo& ReferencingDef, bool bIncludeNoExport = true) const override;

	/**
	 * Return the Engine instance associated with the compiler instance
	 */
	UClass* GetClass() const
	{
		return static_cast<UClass*>(GetObject());
	}

	/**
	 * Return the Engine instance associated with the compiler instance without null check
	 */
	UClass* GetClassSafe() const
	{
		return static_cast<UClass*>(GetObjectSafe());
	}

	/**
	 * Set the Engine instance associated with the compiler instance
	 */
	virtual void SetObject(UObject* InObject) override
	{
		FUnrealStructDefinitionInfo::SetObject(InObject);
		CastChecked<UClass>(InObject)->ClassFlags |= ClassFlags;
	}

	FUnrealClassDefinitionInfo* GetSuperClass() const;

	/**
	 * Get the archive type
	 */
	ESerializerArchiveType GetArchiveType() const
	{
		return ArchiveType;
	}

	/**
	 * Set the archive type
	 */
	void AddArchiveType(ESerializerArchiveType InArchiveType)
	{
		ArchiveType |= InArchiveType;
	}

	/**
	 * Get the enclosing define
	 */
	const FString& GetEnclosingDefine() const
	{
		return EnclosingDefine;
	}

	/**
	 * Set the enclosing define
	 */
	void SetEnclosingDefine(FString&& InEnclosingDefine)
	{
		EnclosingDefine = MoveTemp(InEnclosingDefine);
	}

	/**
	 * Return true if this is an interface
	 */
	bool IsInterface() const
	{
		return bIsInterface;
	}

	/**
	 * Return the flags that were parsed as part of the pre-parse phase
	 */
	EClassFlags GetClassFlags() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || (GetClassSafe()->ClassFlags & CLASS_SaveInCompiledInClasses) == (ClassFlags & CLASS_SaveInCompiledInClasses));
#endif
		return ClassFlags;
	}

	/**
	 * Sets the given flags
	 */
	void SetClassFlags(EClassFlags FlagsToSet)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || (GetClassSafe()->ClassFlags & CLASS_SaveInCompiledInClasses) == (ClassFlags & CLASS_SaveInCompiledInClasses));
#endif
		ClassFlags |= FlagsToSet;
		if (UClass* Class = GetClassSafe())
		{
			Class->ClassFlags |= FlagsToSet;
		}
	}

	/**
	 * Clears the given flags
	 */
	void ClearClassFlags(EClassFlags FlagsToClear)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || (GetClassSafe()->ClassFlags & CLASS_SaveInCompiledInClasses) == (ClassFlags & CLASS_SaveInCompiledInClasses));
#endif
		ClassFlags &= ~FlagsToClear;
		if (UClass* Class = GetClassSafe())
		{
			Class->ClassFlags &= ~FlagsToClear;
		}
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagsToCheck		Class flag(s) to check for
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	bool HasAnyClassFlags(EClassFlags FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || (GetClassSafe()->ClassFlags & CLASS_SaveInCompiledInClasses) == (ClassFlags & CLASS_SaveInCompiledInClasses));
#endif
		return EnumHasAnyFlags(ClassFlags, FlagsToCheck);
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Class flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	bool HasAllClassFlags(EClassFlags FlagsToCheck) const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || (GetClassSafe()->ClassFlags & CLASS_SaveInCompiledInClasses) == (ClassFlags & CLASS_SaveInCompiledInClasses));
#endif
		return EnumHasAllFlags(ClassFlags, FlagsToCheck);
	}

	/**
	 * Used to safely check whether the passed in flag is set in the whole hierarchy
	 *
	 * @param	FlagsToCheck		Class flag(s) to check for
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	bool HierarchyHasAnyClassFlags(EClassFlags FlagsToCheck) const
	{
		for (const FUnrealClassDefinitionInfo* ClassDef = this; ClassDef; ClassDef = ClassDef->GetSuperClass())
		{
			if (ClassDef->HasAnyClassFlags(FlagsToCheck))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Class flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	bool HierarchyHasAllClassFlags(EClassFlags FlagsToCheck) const
	{
		for (const FUnrealClassDefinitionInfo* ClassDef = this; ClassDef; ClassDef = ClassDef->GetSuperClass())
		{
			if (ClassDef->HasAllClassFlags(FlagsToCheck))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Get the class cast flags
	 */
	EClassCastFlags GetClassCastFlags() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || GetClassSafe()->ClassCastFlags == ClassCastFlags);
#endif
		return ClassCastFlags;
	}

	/**
	 * Set addition class cast flags
	 */
	void SetClassCastFlags(EClassCastFlags FlagsToSet)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || GetClassSafe()->ClassCastFlags == ClassCastFlags);
#endif
		ClassCastFlags |= FlagsToSet;
		if (UClass* Class = GetClassSafe())
		{
			Class->ClassCastFlags |= FlagsToSet;
		}
	}

	/**
	 * Used to safely check whether the passed in cast flag is set.
	 *
	 * @param	FlagsToCheck		Class flag(s) to check for
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	bool HasAnyCastFlags(EClassCastFlags FlagsToCheck) const
	{
		return EnumHasAnyFlags(ClassCastFlags, FlagsToCheck);
	}

	/**
	 * Return the flags that were parsed as part of the pre-parse phase
	 */
	EClassFlags GetParsedClassFlags() const
	{
		return ParsedClassFlags;
	}

	/**
	 * Get the flags we inherit from base classes
	 */
	EClassFlags GetInheritClassFlags() const
	{
		return InheritClassFlags;
	}

	/**
	 * Return the meta data from the preparse phase
	 */
	TMap<FName, FString>& GetParsedMetaData()
	{
		return ParsedMetaData;
	}

	/**
	* Parse Class's properties to generate its declaration data.
	*
	* @param	InClassSpecifiers Class properties collected from its UCLASS macro
	* @param	InRequiredAPIMacroIfPresent *_API macro if present (empty otherwise)
	* @param	OutClassData Parsed class meta data
	*/
	void ParseClassProperties(TArray<FPropertySpecifier>&& InClassSpecifiers, const FString& InRequiredAPIMacroIfPresent);

	/**
	* Merges all category properties with the class which at this point only has its parent propagated categories
	*/
	void MergeClassCategories();

	/**
	* Merges all class flags and validates them
	*
	* @param	DeclaredClassName Name this class was declared with (for validation)
	* @param	PreviousClassFlags Class flags before resetting the class (for validation)
	*/
	void MergeAndValidateClassFlags(const FString& DeclaredClassName);

	/**
	 * Add the category meta data
	 */
	void MergeCategoryMetaData(TMap<FName, FString>& InMetaData);

	void MergeCatagoryMetaData(TMap<FName, FString>& InMetaData, FName InName, const TArray<FString>& InNames);

	void GetSparseClassDataTypes(TArray<FString>& OutSparseClassDataTypes) const;

	/**
	 * Get the class's class within setting
	 */
	FUnrealClassDefinitionInfo* GetClassWithin() const
	{
		return ClassWithin;
	}

	/**
	 * Set the class's class within setting
	 */
	void SetClassWithin(FUnrealClassDefinitionInfo* InClassWithin)
	{
		ClassWithin = InClassWithin;
		if (UClass* Class = GetClassSafe())
		{
			Class->ClassWithin = ClassWithin->GetClass();
		}
	}

	/**
	 * Get the class repliaction properties.  This collection includes the parents
	 */
	const TArray<FUnrealPropertyDefinitionInfo*>& GetClassReps() const
	{
		return ClassReps;
	}

	/**
	 * Get the index of the first class rep that is part of this class
	 */
	int32 GetFirstOwnedClassRep() const
	{
		return FirstOwnedClassRep;
	}

	/**
	 * Return true if we have owned class reps
	 */
	bool HasOwnedClassReps() const
	{
		return FirstOwnedClassRep < ClassReps.Num();
	}

	/**
	 * Return true if we have class reps
	 */
	bool HasClassReps() const
	{
		return !ClassReps.IsEmpty();
	}

	/**
	 * Get the class config name
	 */
	FName GetClassConfigName() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || GetClassSafe()->ClassConfigName == ClassConfigName);
#endif
		return ClassConfigName;
	}

	/**
	 * Set the class config name
	 */
	void SetClassConfigName(FName InClassConfigName)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || GetClassSafe()->ClassConfigName == ClassConfigName);
#endif
		ClassConfigName = InClassConfigName;
		if (UClass* Class = GetClassSafe())
		{
			Class->ClassConfigName = InClassConfigName;
		}
	}

	/**
	 * Get the properties size
	 */
	int32 GetPropertiesSize() const
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || GetClassSafe()->PropertiesSize == PropertiesSize);
#endif
		return PropertiesSize;
	}

	/**
	 * Set the properties size
	 */
	void SetPropertiesSize(int32 InPropertiesSize)
	{
#if UHT_ENABLE_ENGINE_TYPE_CHECKS
		check(GetClassSafe() == nullptr || GetClassSafe()->PropertiesSize == PropertiesSize);
#endif
		PropertiesSize = InPropertiesSize;
		if (UClass* Class = GetClassSafe())
		{
			Class->PropertiesSize = InPropertiesSize;
		}
	}

	/**
	 * Gets prolog line number for this class.
	 */
	int32 GetPrologLine() const
	{
		check(PrologLine > 0);
		return PrologLine;
	}

	/**
	 * Sets prolog line number for this class.
	 */
	void SetPrologLine(int32 Line)
	{
		check(Line > 0);
		PrologLine = Line;
	}

	/**
	 * Gets generated body line number for this class.
	 */
	int32 GetGeneratedBodyLine() const
	{
		check(GeneratedBodyLine > 0);
		return GeneratedBodyLine;
	}

	/**
	 * Sets generated body line number for this class.
	 */
	void SetGeneratedBodyLine(int32 Line)
	{
		check(Line > 0);
		GeneratedBodyLine = Line;
	}

	/**
	 * Gets interface generated body line number for this class.
	 */
	int32 GetInterfaceGeneratedBodyLine() const
	{
		check(InterfaceGeneratedBodyLine > 0);
		return InterfaceGeneratedBodyLine;
	}

	/**
	 * Sets interface generated body line number for this class.
	 */
	void SetInterfaceGeneratedBodyLine(int32 Line)
	{
		check(Line > 0);
		InterfaceGeneratedBodyLine = Line;
	}

	/**
	 * Return true if the constructor is declared
	 */
	bool IsConstructorDeclared() const
	{
		return bConstructorDeclared;
	}

	/**
	 * Mark that the constructor has been declared
	 */
	void MarkConstructorDeclared()
	{
		bConstructorDeclared = true;
	}

	/**
	 * Return true if the default constructor is declared
	 */
	bool IsDefaultConstructorDeclared() const
	{
		return bDefaultConstructorDeclared;
	}

	/**
	 * Mark that the default constructor has been declared
	 */
	void MarkDefaultConstructorDeclared()
	{
		bDefaultConstructorDeclared = true;
	}

	/**
	 * Return true if the object initializer constructor is declared
	 */
	bool IsObjectInitializerConstructorDeclared() const
	{
		return bObjectInitializerConstructorDeclared;
	}

	/**
	 * Mark that the object initializer constructor has been declared
	 */
	void MarkObjectInitializerConstructorDeclared()
	{
		bObjectInitializerConstructorDeclared = true;
	}

	/**
	 * Return true if the custom vtable helper destructor is declared
	 */
	bool IsDestructorDeclared() const
	{
		return bDestructorDeclared;
	}
	/**
	 * Mark that the destructor has been declared
	 */
	void MarkDestructorDeclared()
	{
		bDestructorDeclared = true;
	}

	/**
	 * Return true if the custom vtable helper constructor is declared
	 */
	bool IsCustomVTableHelperConstructorDeclared() const
	{
		return bCustomVTableHelperConstructorDeclared;
	}

	/**
	 * Mark that the custom vtable helper constructor has been declared
	 */
	void MarkCustomVTableHelperConstructorDeclared()
	{
		bCustomVTableHelperConstructorDeclared = true;
	}

	/**
	 * Return true if the class has been parsed
	 */
	bool IsParsed() const
	{
		return bParsed;
	}

	/**
	 * Mark the class as being parsed
	 */
	void MarkParsed()
	{
		bParsed = true;
	}

	/**
	 * Return true if the class has a custom constructor
	 */
	bool HasCustomConstructor() const
	{
		return bCustomConstructor;
	}

	/**
	 * Mark the class as having a custom constructor
	 */
	void MarkCustomConstructor()
	{
		bCustomConstructor = true;
	}

	/**
	 * Return true if the class has a custom FieldNotify declaration and implementation
	 */
	bool HasCustomFieldNotify() const
	{
		return bCustomFieldNotify;
	}

	/**
	 * Mark the class as having a custom FieldNotify declaration and implementation
	 */
	void MarkCustomFieldNotify()
	{
		bCustomFieldNotify = true;
	}

	/**
	 * Return true if the class has at least one FieldNotify field
	 */
	bool HasFieldNotify() const
	{
		return bHasFieldNotify;
	}

	/**
	 * Mark the class as having at least one FieldNotify field
	 */
	void MarkHasFieldNotify();

	/**
	 * Return true if the class is compiled in
	 */
	bool IsCompiledIn() const
	{
		return bIsCompiledIn;
	}

	/**
	 * Return true if the class should not export
	 */
	bool IsNoExport() const
	{
		return bNoExport;
	}

	/**
	 * Mark the class as not to be exported
	 */
	void MarkNoExport()
	{
		bNoExport = true;
	}

	/**
	 * Get the generated body access specifier
	 */
	EAccessSpecifier GetGeneratedBodyMacroAccessSpecifier() const
	{
		return GeneratedBodyMacroAccessSpecifier;
	}

	/**
	 * Set the generated body access specifier
	 */
	EAccessSpecifier SetGeneratedBodyMacroAccessSpecifier(EAccessSpecifier InGeneratedBodyMacroAccessSpecifier)
	{
		return GeneratedBodyMacroAccessSpecifier = InGeneratedBodyMacroAccessSpecifier;
	}

	/**
	 * Get the parsed interface state 
	 */
	EParsedInterface GetParsedInterfaceState() const
	{
		return ParsedInterface;
	}

	/**
	 * Set the parsed interface state
	 */
	void SetParsedInterfaceState(EParsedInterface InParsedInterface)
	{
		ParsedInterface = InParsedInterface;
	}

	/**
	 * Test to see if the class implements a specific interface
	 */
	bool ImplementsInterface(const FUnrealClassDefinitionInfo& SomeInterface) const;

	FString GetNameWithPrefix(EEnforceInterfacePrefix EnforceInterfacePrefix = EEnforceInterfacePrefix::None) const;

protected:
	/**
	 * Create UObject engine types
	 */
	virtual void CreateUObjectEngineTypesInternal(ECreateEngineTypesPhase Phase) override;

	/**
	 * Perform any post parsing finalization and validation
	 */
	virtual void PostParseFinalizeInternal(EPostParseFinalizePhase Phase) override;

private:
	/** Merges all 'show' categories */
	void MergeShowCategories();
	/** Sets and validates 'within' property */
	void SetAndValidateWithinClass();
	/** Sets and validates 'ConfigName' property */
	void SetAndValidateConfigName();

	TMap<FName, FString> ParsedMetaData; // Meta data from the preparse phase
	TArray<FString> ShowCategories;
	TArray<FString> ShowFunctions;
	TArray<FString> DontAutoCollapseCategories;
	TArray<FString> HideCategories;
	TArray<FString> ShowSubCatgories;
	TArray<FString> HideFunctions;
	TArray<FString> AutoExpandCategories;
	TArray<FString> AutoCollapseCategories;
	TArray<FString> PrioritizeCategories;
	TArray<FString> DependsOn;
	TArray<FString> ClassGroupNames;
	TArray<FString> SparseClassDataTypes;
	TArray<FUnrealPropertyDefinitionInfo*> ClassReps;
	FName ClassConfigName = NAME_None;
	FString EnclosingDefine;
	FString ClassWithinStr;
	FString ConfigName;
	EClassFlags ClassFlags = CLASS_None; // Current class flags
	EClassFlags ParsedClassFlags = CLASS_None; // Class flags from the preparse phase
	EClassFlags InheritClassFlags = CLASS_ScriptInherit; // Flags to inherit from the super class
	EClassCastFlags ClassCastFlags = CASTCLASS_None;
	FUnrealClassDefinitionInfo* ClassWithin = nullptr;
	ESerializerArchiveType ArchiveType = ESerializerArchiveType::None;

	/** GENERATED_BODY access specifier to preserve. */
	EAccessSpecifier GeneratedBodyMacroAccessSpecifier = ACCESS_NotAnAccessSpecifier;

	/** Parsed interface state */
	EParsedInterface ParsedInterface = EParsedInterface::NotAnInterface;

	/** The line of UCLASS/UINTERFACE macro in this class. */
	int32 PrologLine = -1;

	/** The line of GENERATED_BODY/GENERATED_UCLASS_BODY macro in this class. */
	int32 GeneratedBodyLine = -1;

	/** Same as above, but for interface class associated with this class. */
	int32 InterfaceGeneratedBodyLine = -1;

	int32 FirstOwnedClassRep = 0;
	int32 PropertiesSize = 0;

	/** Is constructor declared? */
	bool bConstructorDeclared = false;

	/** Is default constructor declared? */
	bool bDefaultConstructorDeclared = false;

	/** Is ObjectInitializer constructor (i.e. a constructor with only one parameter of type FObjectInitializer) declared? */
	bool bObjectInitializerConstructorDeclared = false;

	/** Is destructor declared? */
	bool bDestructorDeclared = false;

	/** Is custom VTable helper constructor declared? */
	bool bCustomVTableHelperConstructorDeclared = false;

	/** True if the class definition has been parsed */
	std::atomic<bool> bParsed = false;

	/** True if the class has a custom constructor */
	bool bCustomConstructor = false;

	/** True if the class has a custom FieldNotify declaration and implementation */
	bool bCustomFieldNotify = false;

	/** True if the class has at least one FProperty or UFunction that has the FieldNotify specifier */
	bool bHasFieldNotify = false;

	/** True if the class is not to be exported */
	bool bNoExport = false;

	/** True if the class is an interface */
	bool bIsInterface = false;

	/** True if the class is a compiled-in type and not created by UHT */
	bool bIsCompiledIn = false;
};

template <typename To>
struct FUHTCastImplTo
{};

template <>
struct FUHTCastImplTo<FUnrealTypeDefinitionInfo>
{
	template <typename From>
	FUnrealTypeDefinitionInfo* CastImpl(From& Src)
	{
		return &Src;
	}

	template <>
	FUnrealTypeDefinitionInfo* CastImpl(FUnrealTypeDefinitionInfo& Src)
	{
		return &Src;
	}
};

#define UHT_CAST_IMPL(TypeName, RoutineName) \
	template <> \
	struct FUHTCastImplTo<TypeName> \
	{ \
		template <typename From> \
		TypeName* CastImpl(From& Src) \
		{ \
			return Src.RoutineName(); \
		} \
		template <> \
		TypeName* CastImpl(TypeName& Src) \
		{ \
			return &Src; \
		} \
	};

UHT_CAST_IMPL(FUnrealPropertyDefinitionInfo, AsProperty);
UHT_CAST_IMPL(FUnrealObjectDefinitionInfo, AsObject);
UHT_CAST_IMPL(FUnrealPackageDefinitionInfo, AsPackage);
UHT_CAST_IMPL(FUnrealFieldDefinitionInfo, AsField);
UHT_CAST_IMPL(FUnrealEnumDefinitionInfo, AsEnum);
UHT_CAST_IMPL(FUnrealStructDefinitionInfo, AsStruct);
UHT_CAST_IMPL(FUnrealScriptStructDefinitionInfo, AsScriptStruct);
UHT_CAST_IMPL(FUnrealClassDefinitionInfo, AsClass);
UHT_CAST_IMPL(FUnrealFunctionDefinitionInfo, AsFunction);

#undef UHT_CAST_IMPL

template <typename To, typename From>
To* UHTCast(TSharedRef<From>* Src)
{
	return Src ? FUHTCastImplTo<To>().template CastImpl<From>(**Src) : nullptr;
}

template <typename To, typename From>
To* UHTCast(TSharedRef<From>& Src)
{
	return FUHTCastImplTo<To>().template CastImpl<From>(*Src);
}

template <typename To, typename From>
To* UHTCast(From* Src)
{
	return Src ? FUHTCastImplTo<To>().template CastImpl<From>(*Src) : nullptr;
}

template <typename To, typename From>
const To* UHTCast(const From* Src)
{
	return Src ? FUHTCastImplTo<To>().template CastImpl<From>(const_cast<From&>(*Src)) : nullptr;
}

template <typename To, typename From>
To* UHTCast(From& Src)
{
	return FUHTCastImplTo<To>().template CastImpl<From>(Src);
}

template <typename To, typename From>
const To* UHTCast(const From& Src)
{
	return FUHTCastImplTo<To>().template CastImpl<From>(const_cast<From&>(Src));
}

template <typename To, typename From>
To& UHTCastChecked(TSharedRef<From>* Src)
{
	To* Dst = UHTCast<To, From>(Src);
	check(Dst);
	return *Dst;
}

template <typename To, typename From>
To& UHTCastChecked(TSharedRef<From>& Src)
{
	To* Dst = UHTCast<To, From>(Src);
	check(Dst);
	return *Dst;
}

template <typename To, typename From>
To& UHTCastChecked(From* Src)
{
	To* Dst = UHTCast<To, From>(Src);
	check(Dst);
	return *Dst;
}

template <typename To, typename From>
const To& UHTCastChecked(const From* Src)
{
	const To* Dst = UHTCast<To, From>(Src);
	check(Dst);
	return *Dst;
}

template <typename To, typename From>
To& UHTCastChecked(From& Src)
{
	To* Dst = UHTCast<To, From>(Src);
	check(Dst);
	return *Dst;
}

template <typename To, typename From>
const To& UHTCastChecked(const From& Src)
{
	const To* Dst = UHTCast<To, From>(Src);
	check(Dst);
	return *Dst;
}

template <class T>
const TArray<TSharedRef<T>>& GetFieldsFromDef(const FUnrealStructDefinitionInfo& InStructDef);

template <>
inline const TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& GetFieldsFromDef(const FUnrealStructDefinitionInfo& InStructDef)
{
	return InStructDef.GetProperties();
}

template <>
inline const TArray<TSharedRef<FUnrealFunctionDefinitionInfo>>& GetFieldsFromDef(const FUnrealStructDefinitionInfo& InStructDef)
{
	return InStructDef.GetFunctions();
}

//
// For iterating through a linked list of fields.
//
template <class T>
class TUHTFieldIterator
{
private:
	/** The object being searched for the specified field */
	const FUnrealStructDefinitionInfo* StructDef;
	/** The current location in the list of fields being iterated */
	TSharedRef<T> const* Field;
	TSharedRef<T> const* End;
	/** Whether to include the super class or not */
	const bool bIncludeSuper;

public:
	TUHTFieldIterator(const FUnrealStructDefinitionInfo* InStructDef, EFieldIteratorFlags::SuperClassFlags InSuperClassFlags = EFieldIteratorFlags::IncludeSuper)
		: StructDef(InStructDef)
		, Field(UpdateRange(InStructDef))
		, bIncludeSuper(InSuperClassFlags == EFieldIteratorFlags::IncludeSuper)
	{
		IterateToNext();
	}

	TUHTFieldIterator(const TUHTFieldIterator& rhs) = default;

	/** conversion to "bool" returning true if the iterator is valid. */
	explicit operator bool() const
	{
		return Field != NULL;
	}
	/** inverse of the "bool" operator */
	bool operator !() const
	{
		return !(bool)*this;
	}

	inline friend bool operator==(const TUHTFieldIterator<T>& Lhs, const TUHTFieldIterator<T>& Rhs) { return Lhs.Field == Rhs.Field; }
	inline friend bool operator!=(const TUHTFieldIterator<T>& Lhs, const TUHTFieldIterator<T>& Rhs) { return Lhs.Field != Rhs.Field; }

	inline void operator++()
	{
		checkSlow(Field != End);
		++Field;
		IterateToNext();
	}
	inline T* operator*()
	{
		checkSlow(Field != End);
		return &**Field;
	}
	inline const T* operator*() const
	{
		checkSlow(Field != End);
		return &**Field;
	}
	inline T* operator->()
	{
		checkSlow(Field != End);
		return &**Field;
	}
	inline const FUnrealStructDefinitionInfo* GetStructDef()
	{
		return StructDef;
	}
protected:
	inline TSharedRef<T> const* UpdateRange(const FUnrealStructDefinitionInfo* InStructDef)
	{
		if (InStructDef)
		{
			auto& Array = GetFieldsFromDef<T>(*InStructDef);
			TSharedRef<T> const * Out = Array.GetData();
			End = Out + Array.Num();
			return Out;
		}
		else
		{
			End = nullptr;
			return nullptr;
		}
	}

	inline void IterateToNext()
	{
		TSharedRef<T> const* CurrentField = Field;
		const FUnrealStructDefinitionInfo* CurrentStructDef = StructDef;

		while (CurrentStructDef)
		{
			if (CurrentField != End)
			{
				StructDef = CurrentStructDef;
				Field = CurrentField;
				return;
			}

			if (bIncludeSuper)
			{
				CurrentStructDef = CurrentStructDef->GetSuperStructInfo().Struct;
				if (CurrentStructDef)
				{
					CurrentField = UpdateRange(CurrentStructDef);
					continue;
				}
			}

			break;
		}

		StructDef = CurrentStructDef;
		Field = nullptr;
	}
};

template <typename T>
struct TUHTFieldRange
{
	TUHTFieldRange(const FUnrealStructDefinitionInfo& InStruct, EFieldIteratorFlags::SuperClassFlags InSuperClassFlags = EFieldIteratorFlags::IncludeSuper)
		: Begin(&InStruct, InSuperClassFlags)
	{
	}

	friend TUHTFieldIterator<T> begin(const TUHTFieldRange& Range) { return Range.Begin; }
	friend TUHTFieldIterator<T> end(const TUHTFieldRange& Range) { return TUHTFieldIterator<T>(nullptr); }

	TUHTFieldIterator<T> Begin;
};
