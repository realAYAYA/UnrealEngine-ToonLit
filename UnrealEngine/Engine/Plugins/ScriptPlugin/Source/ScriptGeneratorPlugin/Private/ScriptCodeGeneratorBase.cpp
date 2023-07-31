// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptCodeGeneratorBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "ScriptGeneratorLog.h"
#include "UObject/UnrealType.h"

FScriptCodeGeneratorBase::FScriptCodeGeneratorBase(const FString& InRootLocalPath, const FString& InRootBuildPath, const FString& InOutputDirectory, const FString& InIncludeBase)
{
	GeneratedCodePath = InOutputDirectory;
	RootLocalPath = InRootLocalPath;
	RootBuildPath = InRootBuildPath;
	IncludeBase = InIncludeBase;
}

bool FScriptCodeGeneratorBase::SaveHeaderIfChanged(const FString& HeaderPath, const FString& NewHeaderContents)
{
	FString OriginalHeaderLocal;
	FFileHelper::LoadFileToString(OriginalHeaderLocal, *HeaderPath);

	const bool bHasChanged = OriginalHeaderLocal.Len() == 0 || FCString::Strcmp(*OriginalHeaderLocal, *NewHeaderContents);
	if (bHasChanged)
	{
		// save the updated version to a tmp file so that the user can see what will be changing
		const FString TmpHeaderFilename = HeaderPath + TEXT(".tmp");

		// delete any existing temp file
		IFileManager::Get().Delete(*TmpHeaderFilename, false, true);
		if (!FFileHelper::SaveStringToFile(NewHeaderContents, *TmpHeaderFilename))
		{
			UE_LOG(LogScriptGenerator, Warning, TEXT("Failed to save header export: '%s'"), *TmpHeaderFilename);
		}
		else
		{
			TempHeaders.Add(TmpHeaderFilename);
		}
	}

	return bHasChanged;
}

void FScriptCodeGeneratorBase::RenameTempFiles()
{
	// Rename temp headers
	for (auto& TempFilename : TempHeaders)
	{
		FString Filename = TempFilename.Replace(TEXT(".tmp"), TEXT(""));
		if (!IFileManager::Get().Move(*Filename, *TempFilename, true, true))
		{
			UE_LOG(LogScriptGenerator, Error, TEXT("%s"), *FString::Printf(TEXT("Couldn't write file '%s'"), *Filename));
		}
		else
		{
			UE_LOG(LogScriptGenerator, Log, TEXT("Exported updated script header: %s"), *Filename);
		}
	}
}

FString FScriptCodeGeneratorBase::RebaseToBuildPath(const FString& FileName) const
{
	FString NewFilename(FileName);
	FPaths::MakePathRelativeTo(NewFilename, *IncludeBase);
	return NewFilename;
}

FString FScriptCodeGeneratorBase::GetClassNameCPP(UClass* Class)
{
	return FString::Printf(TEXT("%s%s"), Class->GetPrefixCPP(), *Class->GetName());
}

FString FScriptCodeGeneratorBase::GetPropertyTypeCPP(FProperty* Property, uint32 PortFlags /*= 0*/)
{
	static const FString EnumDecl(TEXT("enum "));
	static const FString StructDecl(TEXT("struct "));
	static const FString ClassDecl(TEXT("class "));
	static const FString TEnumAsByteDecl(TEXT("TEnumAsByte<enum "));
	static const FString TSubclassOfDecl(TEXT("TSubclassOf<class "));

	FString ExtendedType;
	FString PropertyType = Property->GetCPPType(&ExtendedType, PortFlags);
	PropertyType += ExtendedType;
	// Strip any forward declaration keywords
	if (PropertyType.StartsWith(EnumDecl, ESearchCase::CaseSensitive) || PropertyType.StartsWith(StructDecl, ESearchCase::CaseSensitive) || PropertyType.StartsWith(ClassDecl, ESearchCase::CaseSensitive))
	{
		int32 FirstSpaceIndex = PropertyType.Find(TEXT(" "), ESearchCase::CaseSensitive);
		PropertyType.MidInline(FirstSpaceIndex + 1, MAX_int32, false);
	}
	else if (PropertyType.StartsWith(TEnumAsByteDecl, ESearchCase::CaseSensitive))
	{
		int32 FirstSpaceIndex = PropertyType.Find(TEXT(" "), ESearchCase::CaseSensitive);
		PropertyType = TEXT("TEnumAsByte<") + PropertyType.Mid(FirstSpaceIndex + 1);
	}
	else if (PropertyType.StartsWith(TSubclassOfDecl, ESearchCase::CaseSensitive))
	{
		int32 FirstSpaceIndex = PropertyType.Find(TEXT(" "), ESearchCase::CaseSensitive);
		PropertyType = TEXT("TSubclassOf<") + PropertyType.Mid(FirstSpaceIndex + 1);
	}
	return PropertyType;
}

FString FScriptCodeGeneratorBase::GenerateFunctionDispatch(UFunction* Function)
{
	FString Params;
	
	const bool bHasParamsOrReturnValue = (Function->ChildProperties != NULL);
	if (bHasParamsOrReturnValue)
	{
		Params += TEXT("\tstruct FDispatchParams\r\n\t{\r\n");

		for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			Params += FString::Printf(TEXT("\t\t%s %s;\r\n"), *GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue), *Param->GetName());
		}
		Params += TEXT("\t} Params;\r\n");
		int32 ParamIndex = 0;
		for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt, ++ParamIndex)
		{
			FProperty* Param = *ParamIt;
			Params += FString::Printf(TEXT("\tParams.%s = %s;\r\n"), *Param->GetName(), *InitializeFunctionDispatchParam(Function, Param, ParamIndex));
		}
	}
	Params += FString::Printf(TEXT("\tstatic UFunction* Function = Obj->FindFunctionChecked(TEXT(\"%s\"));\r\n"), *Function->GetName());
	if (bHasParamsOrReturnValue)
	{
		Params += TEXT("\tcheck(Function->ParmsSize == sizeof(FDispatchParams));\r\n");
		Params += TEXT("\tObj->ProcessEvent(Function, &Params);\r\n");
	}
	else
	{
		Params += TEXT("\tObj->ProcessEvent(Function, NULL);\r\n");
	}	

	return Params;
}

FString FScriptCodeGeneratorBase::InitializeFunctionDispatchParam(UFunction* Function, FProperty* Param, int32 ParamIndex)
{
	if (Param->IsA(FObjectPropertyBase::StaticClass()) || Param->IsA(FClassProperty::StaticClass()))
	{
		return TEXT("NULL");
	}
	else
	{
		return FString::Printf(TEXT("%s()"), *GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue));
	}	
}

FString FScriptCodeGeneratorBase::GetScriptHeaderForClass(UClass* Class)
{
	return GeneratedCodePath / (Class->GetName() + TEXT(".script.h"));
}

bool FScriptCodeGeneratorBase::CanExportClass(UClass* Class)
{
	bool bCanExport = (Class->ClassFlags & (CLASS_RequiredAPI | CLASS_MinimalAPI)) != 0; // Don't export classes that don't export DLL symbols

	return bCanExport;
}

bool FScriptCodeGeneratorBase::CanExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	// We don't support delegates and non-public functions
	if ((Function->FunctionFlags & FUNC_Delegate))
	{
		return false;
	}

	// Reject if any of the parameter types is unsupported yet
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;
		if (Param->IsA(FArrayProperty::StaticClass()) ||
			  Param->ArrayDim > 1 ||
			  Param->IsA(FDelegateProperty::StaticClass()) ||
				Param->IsA(FMulticastDelegateProperty::StaticClass()) ||
			  Param->IsA(FWeakObjectProperty::StaticClass()) ||
			  Param->IsA(FInterfaceProperty::StaticClass()))
		{
			return false;
		}
	}

	return true;
}

bool FScriptCodeGeneratorBase::CanExportProperty(const FString& ClassNameCPP, UClass* Class, FProperty* Property)
{
	// Property must be DLL exported
	if (!(Class->ClassFlags & CLASS_RequiredAPI))
	{
		return false;
	}

	// Only public, editable properties can be exported
	if (!Property->HasAnyFlags(RF_Public) || 
		  (Property->PropertyFlags & CPF_Protected) || 
			!(Property->PropertyFlags & CPF_Edit))
	{
		return false;
	}


	// Reject if it's one of the unsupported types (yet)
	if (Property->IsA(FArrayProperty::StaticClass()) ||
		Property->ArrayDim > 1 ||
		Property->IsA(FDelegateProperty::StaticClass()) ||
		Property->IsA(FMulticastDelegateProperty::StaticClass()) ||
		Property->IsA(FWeakObjectProperty::StaticClass()) ||
		Property->IsA(FInterfaceProperty::StaticClass()) ||
		Property->IsA(FStructProperty::StaticClass()))
	{
		return false;
	}

	return true;
}
