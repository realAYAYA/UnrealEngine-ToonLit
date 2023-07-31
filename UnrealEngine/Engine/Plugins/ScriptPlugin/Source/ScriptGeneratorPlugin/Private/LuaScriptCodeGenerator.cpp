// Copyright Epic Games, Inc. All Rights Reserved.

#include "LuaScriptCodeGenerator.h"
#include "ScriptGeneratorLog.h"
#include "UObject/ErrorException.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Misc/Paths.h"

// Supported structs
static FName Name_Vector2D("Vector2D");
static FName Name_Vector("Vector");
static FName Name_Vector4("Vector4");
static FName Name_Quat("Quat");
static FName Name_Transform("Transform");
static FName Name_LinearColor("LinearColor");
static FName Name_Color("Color");

FLuaScriptCodeGenerator::FLuaScriptCodeGenerator(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& InIncludeBase)
: FScriptCodeGeneratorBase(RootLocalPath, RootBuildPath, OutputDirectory, InIncludeBase)
{
}

FString FLuaScriptCodeGenerator::GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	return GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, Function->GetName());
}

FString FLuaScriptCodeGenerator::GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, UClass* Class, const FString& FunctionName)
{
	return FString::Printf(TEXT("int32 %s_%s(lua_State* InScriptContext)"), *Class->GetName(), *FunctionName);
}

FString FLuaScriptCodeGenerator::InitializeFunctionDispatchParam(UFunction* Function, FProperty* Param, int32 ParamIndex)
{	
	if (!(Param->GetPropertyFlags() & CPF_ReturnParm))
	{
		FString Initializer;
		// In Lua, the first param index on the stack is 1 and it's the object we're invoking the function on
		ParamIndex += 2;

		if (Param->IsA(FIntProperty::StaticClass()))
		{
			Initializer = TEXT("(luaL_checkint");
		}
		else if (Param->IsA(FFloatProperty::StaticClass()))
		{
			Initializer = TEXT("(float)(luaL_checknumber");
		}
		else if (Param->IsA(FStrProperty::StaticClass()))
		{
			Initializer = TEXT("ANSI_TO_TCHAR(luaL_checkstring");
		}
		else if (Param->IsA(FNameProperty::StaticClass()))
		{
			Initializer = TEXT("FName(luaL_checkstring");
		}
		else if (Param->IsA(FBoolProperty::StaticClass()))
		{
			Initializer = TEXT("!!(lua_toboolean");
		}
		else if (Param->IsA(FStructProperty::StaticClass()))
		{
			FStructProperty* StructProp = CastFieldChecked<FStructProperty>(Param);
			if (StructProp->Struct->GetFName() == Name_Vector2D)
			{
				Initializer = TEXT("(FLuaVector2D::Get");
			}
			else if (StructProp->Struct->GetFName() == Name_Vector)
			{
				Initializer = TEXT("(FLuaVector::Get");
			}
			else if (StructProp->Struct->GetFName() == Name_Vector4)
			{
				Initializer = TEXT("(FLuaVector4::Get");
			}
			else if (StructProp->Struct->GetFName() == Name_Quat)
			{
				Initializer = TEXT("(FLuaQuat::Get");
			}
			else if (StructProp->Struct->GetFName() == Name_LinearColor)
			{
				Initializer = TEXT("(FLuaLinearColor::Get");
			}
			else if (StructProp->Struct->GetFName() == Name_Color)
			{
				Initializer = TEXT("FColor(FLuaLinearColor::Get");
			}
			else if (StructProp->Struct->GetFName() == Name_Transform)
			{
				Initializer = TEXT("(FLuaTransform::Get");
			}
			else
			{
				FError::Throwf(TEXT("Unsupported function param struct type: %s"), *StructProp->Struct->GetName());
			}
		}
		else if (Param->IsA(FClassProperty::StaticClass()))
		{
			Initializer = TEXT("(UClass*)(lua_touserdata");
		}
		else if (Param->IsA(FObjectPropertyBase::StaticClass()))
		{
			Initializer = FString::Printf(TEXT("(%s)(lua_touserdata"), *GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue), ParamIndex);
		}
		else
		{
			FError::Throwf(TEXT("Unsupported function param type: %s"), *Param->GetClass()->GetName());
		}

		return FString::Printf(TEXT("%s(InScriptContext, %d))"), *Initializer, ParamIndex);
	}
	else
	{
		return FScriptCodeGeneratorBase::InitializeFunctionDispatchParam(Function, Param, ParamIndex);
	}	
}

FString FLuaScriptCodeGenerator::GenerateObjectDeclarationFromContext(const FString& ClassNameCPP, UClass* Class)
{
	return FString::Printf(TEXT("UObject* Obj = (%s*)lua_touserdata(InScriptContext, 1);"), *ClassNameCPP);
}

FString FLuaScriptCodeGenerator::GenerateReturnValueHandler(const FString& ClassNameCPP, UClass* Class, UFunction* Function, FProperty* ReturnValue, const FString& ReturnValueName)
{
	if (ReturnValue)
	{
		FString Initializer;		
		if (ReturnValue->IsA(FIntProperty::StaticClass()))
		{
			Initializer = FString::Printf(TEXT("lua_pushinteger(InScriptContext, %s);"), *ReturnValueName);
		}
		else if (ReturnValue->IsA(FFloatProperty::StaticClass()))
		{
			Initializer = FString::Printf(TEXT("lua_pushnumber(InScriptContext, %s);"), *ReturnValueName);
		}
		else if (ReturnValue->IsA(FStrProperty::StaticClass()))
		{
			Initializer = FString::Printf(TEXT("lua_pushstring(InScriptContext, TCHAR_TO_ANSI(*%s));"), *ReturnValueName);
		}
		else if (ReturnValue->IsA(FNameProperty::StaticClass()))
		{
			Initializer = FString::Printf(TEXT("lua_pushstring(InScriptContext, TCHAR_TO_ANSI(*%s.ToString()));"), *ReturnValueName);
		}
		else if (ReturnValue->IsA(FBoolProperty::StaticClass()))
		{
			Initializer = FString::Printf(TEXT("lua_pushboolean(InScriptContext, %s);"), *ReturnValueName);
		}
		else if (ReturnValue->IsA(FStructProperty::StaticClass()))
		{
			FStructProperty* StructProp = CastFieldChecked<FStructProperty>(ReturnValue);
			if (StructProp->Struct->GetFName() == Name_Vector2D)
			{
				Initializer = FString::Printf(TEXT("FLuaVector2D::Return(InScriptContext, %s);"), *ReturnValueName);
			}
			else if (StructProp->Struct->GetFName() == Name_Vector)
			{
				Initializer = FString::Printf(TEXT("FLuaVector::Return(InScriptContext, %s);"), *ReturnValueName);
			}
			else if (StructProp->Struct->GetFName() == Name_Vector4)
			{
				Initializer = FString::Printf(TEXT("FLuaVector4::Return(InScriptContext, %s);"), *ReturnValueName);
			}
			else if (StructProp->Struct->GetFName() == Name_Quat)
			{
				Initializer = FString::Printf(TEXT("FLuaQuat::Return(InScriptContext, %s);"), *ReturnValueName);
			}
			else if (StructProp->Struct->GetFName() == Name_LinearColor)
			{
				Initializer = FString::Printf(TEXT("FLuaLinearColor::Return(InScriptContext, %s);"), *ReturnValueName);
			}
			else if (StructProp->Struct->GetFName() == Name_Color)
			{
				Initializer = FString::Printf(TEXT("FLuaLinearColor::Return(InScriptContext, FLinearColor(%s));"), *ReturnValueName);
			}
			else if (StructProp->Struct->GetFName() == Name_Transform)
			{
				Initializer = FString::Printf(TEXT("FLuaTransform::Return(InScriptContext, %s);"), *ReturnValueName);
			}
			else
			{
				FError::Throwf(TEXT("Unsupported function return value struct type: %s"), *StructProp->Struct->GetName());
			}
		}
		else if (ReturnValue->IsA(FObjectPropertyBase::StaticClass()))
		{
			Initializer = FString::Printf(TEXT("lua_pushlightuserdata(InScriptContext, %s);"), *ReturnValueName);
		}
		else
		{
			FError::Throwf(TEXT("Unsupported function return type: %s"), *ReturnValue->GetClass()->GetName());
		}

		return FString::Printf(TEXT("%s\r\n\treturn 1;"), *Initializer);
	}
	else
	{
		return TEXT("return 0;");
	}	
}

bool FLuaScriptCodeGenerator::CanExportClass(UClass* Class)
{
	bool bCanExport = FScriptCodeGeneratorBase::CanExportClass(Class);
	if (bCanExport)
	{
		const FString ClassNameCPP = GetClassNameCPP(Class);
		// No functions to export? Don't bother exporting the class.
		bool bHasMembersToExport = false;
		for (TFieldIterator<UFunction> FuncIt(Class); !bHasMembersToExport && FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (CanExportFunction(ClassNameCPP, Class, Function))
			{
				bHasMembersToExport = true;
			}
		}			
		// Check properties too
		if (!bHasMembersToExport)
		{
			for (TFieldIterator<FProperty> PropertyIt(Class/*, EFieldIteratorFlags::ExcludeSuper*/); !bHasMembersToExport && PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;
				if (CanExportProperty(ClassNameCPP, Class, Property))
				{
					bHasMembersToExport = true;
				}
			}
		}
		bCanExport = bHasMembersToExport;
	}
	return bCanExport;
}

bool FLuaScriptCodeGenerator::CanExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	bool bExport = FScriptCodeGeneratorBase::CanExportFunction(ClassNameCPP, Class, Function);
	if (bExport)
	{
		for (TFieldIterator<FProperty> ParamIt(Function); bExport && ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			bExport = IsPropertyTypeSupported(Param);
		}
	}
	return bExport;
}

FString FLuaScriptCodeGenerator::ExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	FString GeneratedGlue = GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, Function);
	GeneratedGlue += TEXT("\r\n{\r\n");

	FProperty* ReturnValue = NULL;
	UClass* FuncSuper = NULL;
	
	// Find the base class where this function was defined
	if (Function->GetOwnerClass() != Class && CanExportClass(Function->GetOwnerClass()))
	{
		FuncSuper = Function->GetOwnerClass();
	}

	FString FunctionBody;
	if (FuncSuper == NULL)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		FunctionBody += GenerateFunctionDispatch(Function);

		FString FunctionCallArguments;
		FString ReturnValueDeclaration;
		for (TFieldIterator<FProperty> ParamIt(Function); !ReturnValue && ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			if (Param->GetPropertyFlags() & CPF_ReturnParm)
			{
				ReturnValue = Param;
			}
		}
		FString ReturnValueName;
		if (ReturnValue)
		{
			ReturnValueName = FString::Printf(TEXT("Params.%s"), *ReturnValue->GetName());
		}
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateReturnValueHandler(ClassNameCPP, Class, Function, ReturnValue, *ReturnValueName));
	}
	else
	{
		FunctionBody = FString::Printf(TEXT("\treturn %s_%s(InScriptContext);\r\n"), *FuncSuper->GetName(), *Function->GetName());
	}

	GeneratedGlue += FunctionBody;
	GeneratedGlue += TEXT("}\r\n\r\n");

	auto& Exports = ClassExportedFunctions.FindOrAdd(Class);
	Exports.Add(Function->GetFName());

	return GeneratedGlue;
}

bool FLuaScriptCodeGenerator::IsPropertyTypeSupported(FProperty* Property) const
{
	bool bSupported = true;
	if (Property->IsA(FStructProperty::StaticClass()))
	{
		FStructProperty* StructProp = CastFieldChecked<FStructProperty>(Property);
		if (StructProp->Struct->GetFName() != Name_Vector2D &&
			StructProp->Struct->GetFName() != Name_Vector &&
			StructProp->Struct->GetFName() != Name_Vector4 &&
			StructProp->Struct->GetFName() != Name_Quat &&
			StructProp->Struct->GetFName() != Name_LinearColor &&
			StructProp->Struct->GetFName() != Name_Color &&
			StructProp->Struct->GetFName() != Name_Transform)
		{
			bSupported = false;
		}
	}
	else if (Property->IsA(FLazyObjectProperty::StaticClass()) ||
		Property->IsA(FSoftObjectProperty::StaticClass()) ||
		Property->IsA(FSoftClassProperty::StaticClass()) ||
		Property->IsA(FWeakObjectProperty::StaticClass()))
	{
		bSupported = false;
	}
	else if (!Property->IsA(FIntProperty::StaticClass()) &&
		!Property->IsA(FFloatProperty::StaticClass()) &&
		!Property->IsA(FStrProperty::StaticClass()) &&
		!Property->IsA(FNameProperty::StaticClass()) &&
		!Property->IsA(FBoolProperty::StaticClass()) &&
		!Property->IsA(FObjectPropertyBase::StaticClass()) &&
		!Property->IsA(FClassProperty::StaticClass()))
	{
		bSupported = false;
	}

	return bSupported;
}

bool FLuaScriptCodeGenerator::CanExportProperty(const FString& ClassNameCPP, UClass* Class, FProperty* Property)
{
	// Only editable properties can be exported
	if (!(Property->PropertyFlags & CPF_Edit))
	{
		return false;
	}
	// Check if property type is supported
	return IsPropertyTypeSupported(Property);
}

FString FLuaScriptCodeGenerator::ExportProperty(const FString& ClassNameCPP, UClass* Class, FProperty* Property, int32 PropertyIndex)
{
	FString PropertyName = Property->GetName();
	FProperty* ReturnValue = NULL;
	UClass* PropertySuper = NULL;

	// Find the base class where this property was defined
	if (Property->GetOwnerClass() != Class && CanExportClass(Property->GetOwnerClass()))
	{
		PropertySuper = Property->GetOwnerClass();
	}

	// Getter	
	FString GetterName = FString::Printf(TEXT("Get_%s"), *PropertyName);
	FString GeneratedGlue = GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, GetterName);
	GeneratedGlue += TEXT("\r\n{\r\n");
	FString FunctionBody;
	if (PropertySuper == NULL)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		FunctionBody += FString::Printf(TEXT("\tstatic FProperty* Property = FindScriptPropertyHelper(%s::StaticClass(), TEXT(\"%s\"));\r\n"), *ClassNameCPP, *Property->GetName());
		FunctionBody += FString::Printf(TEXT("\t%s PropertyValue;\r\n"), *GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue));
		FunctionBody += TEXT("\tProperty->CopyCompleteValue(&PropertyValue, Property->ContainerPtrToValuePtr<void>(Obj));\r\n");
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateReturnValueHandler(ClassNameCPP, Class, NULL, Property, TEXT("PropertyValue")));
	}
	else
	{
		FunctionBody = FString::Printf(TEXT("\treturn %s_%s(InScriptContext);\r\n"), *PropertySuper->GetName(), *GetterName);
	}
	GeneratedGlue += FunctionBody;
	GeneratedGlue += TEXT("}\r\n\r\n");
	FunctionBody.Empty(FunctionBody.Len());

	// Store the name of this getter as well as the name of the wrapper function
	FPropertyAccessor Getter;
	Getter.AccessorName = GetterName;
	Getter.FunctionName = FString::Printf(TEXT("%s_%s"), *Class->GetName(), *GetterName);
	auto& Exports = ClassExportedProperties.FindOrAdd(Class);
	Exports.Add(Getter);

	// Setter
	FString SetterName = FString::Printf(TEXT("Set_%s"), *PropertyName);
	GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, SetterName);
	GeneratedGlue += TEXT("\r\n{\r\n");
	if (PropertySuper == NULL)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		FunctionBody += FString::Printf(TEXT("\tstatic FProperty* Property = FindScriptPropertyHelper(%s::StaticClass(), TEXT(\"%s\"));\r\n"), *ClassNameCPP, *Property->GetName());
		FunctionBody += FString::Printf(TEXT("\t%s PropertyValue = %s;\r\n"), *GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue), *InitializeFunctionDispatchParam(NULL, Property, 0));
		FunctionBody += TEXT("\tProperty->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Obj), &PropertyValue);\r\n");
		FunctionBody += TEXT("\treturn 0;\r\n");
	}
	else
	{
		FunctionBody = FString::Printf(TEXT("\treturn %s_%s(InScriptContext);\r\n"), *PropertySuper->GetName(), *SetterName);
	}
	GeneratedGlue += FunctionBody;	
	GeneratedGlue += TEXT("}\r\n\r\n");

	// Store the name of this setter as well as the name of the wrapper function
	FPropertyAccessor Setter;
	Setter.AccessorName = SetterName;
	Setter.FunctionName = FString::Printf(TEXT("%s_%s"), *Class->GetName(), *SetterName);
	Exports.Add(Setter);

	return GeneratedGlue;
}

FString FLuaScriptCodeGenerator::ExportAdditionalClassGlue(const FString& ClassNameCPP, UClass* Class)
{
	FString GeneratedGlue;

	const FString ClassName = Class->GetName();

	// Constructor and destructor
	if (!(Class->GetClassFlags() & CLASS_Abstract))
	{
		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, TEXT("New"));
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += TEXT("\tUObject* Outer = (UObject*)lua_touserdata(InScriptContext, 1);\r\n");
		GeneratedGlue += TEXT("\tFName Name = FName(luaL_checkstring(InScriptContext, 2));\r\n");
		GeneratedGlue += FString::Printf(TEXT("\tUObject* Obj = NewObject<%s>(Outer, Name);\r\n"), *ClassNameCPP);
		GeneratedGlue += TEXT("\tif (Obj)\r\n\t{\r\n");
		GeneratedGlue += TEXT("\t\tFScriptObjectReferencer::Get().AddObjectReference(Obj);\r\n");
		GeneratedGlue += TEXT("\t}\r\n");
		GeneratedGlue += TEXT("\tlua_pushlightuserdata(InScriptContext, Obj);\r\n");
		GeneratedGlue += TEXT("\treturn 1;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");

		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, TEXT("Destroy")); 
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP, Class));
		GeneratedGlue += TEXT("\tif (Obj)\r\n\t{\r\n");
		GeneratedGlue += TEXT("\t\tFScriptObjectReferencer::Get().RemoveObjectReference(Obj);\r\n");
		GeneratedGlue += TEXT("\t}\r\n\treturn 0;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");
	}

	// Class: Equivalent of StaticClass()
	GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class, TEXT("Class"));
	GeneratedGlue += TEXT("\r\n{\r\n");
	GeneratedGlue += FString::Printf(TEXT("\tUClass* Class = %s::StaticClass();\r\n"), *ClassNameCPP);
	GeneratedGlue += TEXT("\tlua_pushlightuserdata(InScriptContext, Class);\r\n");
	GeneratedGlue += TEXT("\treturn 1;\r\n");
	GeneratedGlue += TEXT("}\r\n\r\n");

	// Library
	GeneratedGlue += FString::Printf(TEXT("static const luaL_Reg %s_Lib[] =\r\n{\r\n"), *ClassName);
	if (!(Class->GetClassFlags() & CLASS_Abstract))
	{
		GeneratedGlue += FString::Printf(TEXT("\t{ \"New\", %s_New },\r\n"), *ClassName);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"Destroy\", %s_Destroy },\r\n"), *ClassName);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"Class\", %s_Class },\r\n"), *ClassName);
	}
	auto FunctionExports = ClassExportedFunctions.Find(Class);
	if (FunctionExports)
	{
		for (auto& FunctionName : *FunctionExports)
		{
			GeneratedGlue += FString::Printf(TEXT("\t{ \"%s\", %s_%s },\r\n"), *FunctionName.ToString(), *ClassName, *FunctionName.ToString());
		}
	}
	auto PropertyExports = ClassExportedProperties.Find(Class);
	if (PropertyExports)
	{
		for (auto& Accessor : *PropertyExports)
		{
			GeneratedGlue += FString::Printf(TEXT("\t{ \"%s\", %s },\r\n"), *Accessor.AccessorName, *Accessor.FunctionName);
		}
	}
	GeneratedGlue += TEXT("\t{ NULL, NULL }\r\n};\r\n\r\n");

	return GeneratedGlue;
}

void FLuaScriptCodeGenerator::ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged)
{
	if (!CanExportClass(Class) || ExportedClasses.Contains(Class->GetFName()))
	{
		return;
	}
	
	UE_LOG(LogScriptGenerator, Log, TEXT("Exporting class %s"), *Class->GetName());
	
	ExportedClasses.Add(Class->GetFName());
	LuaExportedClasses.Add(Class);
	AllSourceClassHeaders.Add(SourceHeaderFilename);

	const FString ClassGlueFilename = GetScriptHeaderForClass(Class);
	AllScriptHeaders.Add(ClassGlueFilename);	

	const FString ClassNameCPP = GetClassNameCPP(Class);
	FString GeneratedGlue(TEXT("#pragma once\r\n\r\n"));		

	// Export all functions
	for (TFieldIterator<UFunction> FuncIt(Class /*, EFieldIteratorFlags::ExcludeSuper*/); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (CanExportFunction(ClassNameCPP, Class, Function))
		{
			GeneratedGlue += ExportFunction(ClassNameCPP, Class, Function);
		}
	}

	// Export properties that are owned by this class
	int32 PropertyIndex = 0;
	for (TFieldIterator<FProperty> PropertyIt(Class /*, EFieldIteratorFlags::ExcludeSuper*/); PropertyIt; ++PropertyIt, ++PropertyIndex)
	{
		FProperty* Property = *PropertyIt;
		if (CanExportProperty(ClassNameCPP, Class, Property))
		{
			UE_LOG(LogScriptGenerator, Log, TEXT("  %s %s"), *Property->GetClass()->GetName(), *Property->GetName());
			GeneratedGlue += ExportProperty(ClassNameCPP, Class, Property, PropertyIndex);
		}
	}

	GeneratedGlue += ExportAdditionalClassGlue(ClassNameCPP, Class);

	SaveHeaderIfChanged(ClassGlueFilename, GeneratedGlue);
}

void FLuaScriptCodeGenerator::FinishExport()
{
	GlueAllGeneratedFiles();
	RenameTempFiles();
}

void FLuaScriptCodeGenerator::GlueAllGeneratedFiles()
{
	// Generate inl library file
	FString LibGlueFilename = GeneratedCodePath / TEXT("GeneratedScriptLibraries.inl");
	FString LibGlue;

	// Include all source header files
	TArray<FString> Headers;
	Headers.Reserve(AllSourceClassHeaders.Num());
	for (auto& HeaderFilename : AllSourceClassHeaders)
	{
		Headers.Add(HeaderFilename);
	}

	Headers.Sort();
	for (auto& HeaderFilename : Headers)
	{
		// Re-base to make sure we're including the right files on a remote machine
		FString NewFilename(RebaseToBuildPath(HeaderFilename));
		LibGlue += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
	}

	// Include all script glue headers
	AllScriptHeaders.Sort();
	for (auto& HeaderFilename : AllScriptHeaders)
	{
		// Re-base to make sure we're including the right files on a remote machine
		FString NewFilename(FPaths::GetCleanFilename(HeaderFilename));
		LibGlue += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
	}

	LuaExportedClasses.Sort([](const UClass& x, const UClass& y) { return x.GetName() < y.GetName(); });
	LibGlue += TEXT("\r\nvoid LuaRegisterExportedClasses(lua_State* InScriptContext)\r\n{\r\n");
	for (auto Class : LuaExportedClasses)
	{
		LibGlue += FString::Printf(TEXT("\tFLuaUtils::RegisterLibrary(InScriptContext, %s_Lib, \"%s\");\r\n"), *Class->GetName(), *Class->GetName());
	}
	LibGlue += TEXT("}\r\n\r\n");

	SaveHeaderIfChanged(LibGlueFilename, LibGlue);
}
