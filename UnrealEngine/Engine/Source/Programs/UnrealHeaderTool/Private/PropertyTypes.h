// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HeaderParser.h" // for EVariableCategory
#include "UObject/Stack.h"

class FProperty;
class FString;
class FUnrealPropertyDefinitionInfo;
class FUnrealSourceFile;

struct FPropertyTraits
{
	/**
	 * Transforms CPP-formated string containing default value, to inner formated string
	 * If it cannot be transformed empty string is returned.
	 *
	 * @param PropDef The property that owns the default value.
	 * @param CppForm A CPP-formated string.
	 * @param out InnerForm Inner formated string
	 * @return true on success, false otherwise.
	 */
	static bool DefaultValueStringCppFormatToInnerFormat(const FUnrealPropertyDefinitionInfo& PropDef, const FString& CppForm, FString& InnerForm);

	/**
	 * Given a property definition token, create the property definition and then underlying engine FProperty
	 *
	 * @param Token The definition of the property
	 * @param Outer The parent object owning the property
	 * @param Name The name of the property
	 * @param ObjectFlags The flags associated with the property
	 * @param VariableCategory The parsing context of the property
	 * @param AccessCategory The access of the property (i.e. public, private, etc...)
	 * @param Dimensions When this is a static array, this represents the dimensions value
	 * @param SourceFile The source file containing the property
	 * @param LineNumber Line number of the property
	 * @param ParsePosition Character position of the property in the header
	 * @return The pointer to the newly created property.  It will be attached to the definition by the caller
	 */
	static TSharedRef<FUnrealPropertyDefinitionInfo> CreateProperty(const FPropertyBase& VarProperty, FUnrealTypeDefinitionInfo& Outer, const FName& Name, EVariableCategory VariableCategory, EAccessSpecifier AccessSpecifier, const TCHAR* Dimensions, FUnrealSourceFile& SourceFile, int LineNumber, int ParsePosition);

	/**
	 * Given a property, create the underlying engine types
	 * 
	 * @param PropDef The property in question
	 * @param ObjectFlags The flags associated with the property
	 */
	static FProperty* CreateEngineType(TSharedRef<FUnrealPropertyDefinitionInfo> PropDefRef);

	/**
	 * Test to see if the property can be used in a blueprint
	 * 
	 * @param PropDef The property in question
	 * @param bMemberVariable If true, this is a member variable being tested
	 * @return Return true if the property is supported in blueprints
	 */
	static bool IsSupportedByBlueprint(const FUnrealPropertyDefinitionInfo& PropDef, bool bMemberVariable);

	/**
	 * Return the engine class name for the given property information.
	 * @param PropDef The property in question
	 * @return The name of the engine property that will represent this definition.
	 */
	static FString GetEngineClassName(const FUnrealPropertyDefinitionInfo& PropDef);

	/**
	 * Returns the text to use for exporting this property to header file.
	 *
	 * @param	ExtendedTypeText	for property types which use templates, will be filled in with the type
	 * @param	CPPExportFlags		flags for modifying the behavior of the export
	 */
	static FString GetCPPType(const FUnrealPropertyDefinitionInfo& PropDef, FString* ExtendedTypeText = nullptr, uint32 CPPExportFlags = 0);

	static FString GetCPPTypeForwardDeclaration(const FUnrealPropertyDefinitionInfo& PropDef);

	static FString GetCPPMacroType(const FUnrealPropertyDefinitionInfo& PropDef, FString& ExtendedTypeText);

	/**
	 * Tests to see if the two types are the same types
	 */
	static bool SameType(const FUnrealPropertyDefinitionInfo& Lhs, const FUnrealPropertyDefinitionInfo& Rhs);

	/**
	 * Tests to see if the given field class name is valid
	 */
	static bool IsValidFieldClass(FName FieldClassName);

	/**
	 * Handle any final initialization after parsing has completed
	 *
	 * @param PropDef The property in question
	 */
	static void PostParseFinalize(FUnrealPropertyDefinitionInfo& PropDef);
};
