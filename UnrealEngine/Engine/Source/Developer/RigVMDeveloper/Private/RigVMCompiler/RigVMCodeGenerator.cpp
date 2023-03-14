// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCompiler/RigVMCodeGenerator.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMDeveloperModule.h"
#include "Algo/Count.h"
#include "Animation/Rig.h"

static constexpr TCHAR RigVM_CommaSeparator[] = TEXT(", ");
static constexpr TCHAR RigVM_NewLineFormat[] = TEXT("\r\n");
static constexpr TCHAR RigVM_IncludeBracketFormat[] = TEXT("#include <{0}>");
static constexpr TCHAR RigVM_IncludeQuoteFormat[] = TEXT("#include \"{0}.h\"");
static constexpr TCHAR RigVM_WrappedArrayTypeFormat[] = TEXT("struct {0}_API {1}\r\n{\r\n\tTArray<{2}> Array;\r\n};");
static constexpr TCHAR RigVM_WrappedTypeNameFormat[] = TEXT("{0}Array_{1}");
static constexpr TCHAR RigVM_DeclareExternalVariableFormat[] = TEXT("\t{0}* {1} = nullptr;");
static constexpr TCHAR RigVM_UpdateExternalVariableFormat[] = TEXT("\t{0} = &GetExternalVariableRef<{1}>(TEXT(\"{2}\"), TEXT(\"{1}\"));");
static constexpr TCHAR RigVM_MemberPropertyFormat[] = TEXT("\t{0} {1};");
static constexpr TCHAR RigVM_DeclareEntryNameFormat[] = TEXT("\tstatic const FName EntryName_{0};");
static constexpr TCHAR RigVM_DefineEntryNameFormat[] = TEXT("const FName U{0}::EntryName_{1} = TEXT(\"{2}\");");
static constexpr TCHAR RigVM_DefineConstFormatNoDefault[] = TEXT("\tstatic const {0} {1};");
static constexpr TCHAR RigVM_DefineStructConstFormat[] = TEXT("\tstatic const {0} {1} = URigVMNativized::GetStructConstant<{0}>(TEXT(\"{2}\"));");
static constexpr TCHAR RigVM_DefineStructArrayConstFormat[] = TEXT("\tstatic const {0} {1} = URigVMNativized::GetStructArrayConstant<{2}>(TEXT(\"{3}\"));");
static constexpr TCHAR RigVM_DefineConstFormat[] = TEXT("\tstatic const {0} {1} = {2};");
static constexpr TCHAR RigVM_NameNoneFormat[] = TEXT("FName(NAME_None)");
static constexpr TCHAR RigVM_EmptyStringFormat[] = TEXT("FString()");
static constexpr TCHAR RigVM_SingleStringFormat[] = TEXT("%s");
static constexpr TCHAR RigVM_TextFormat[] = TEXT("TEXT({0})");
static constexpr TCHAR RigVM_QuotedTextFormat[] = TEXT("TEXT(\"{0}\")");
static constexpr TCHAR RigVM_CurlyBracesFormat[] = TEXT("{{0}}");
static constexpr TCHAR RigVM_BracesFormat[] = TEXT("({0})");
static constexpr TCHAR RigVM_TemplateOneArgFormat[] = TEXT("<{0}>");
static constexpr TCHAR RigVM_CallExternOpFormat[] = TEXT("\t{0}::Static{1}({2});");
static constexpr TCHAR RigVM_ZeroOpFormat[] = TEXT("\t{0} = 0;");
static constexpr TCHAR RigVM_BoolFalseOpFormat[] = TEXT("\t{0} = false;");
static constexpr TCHAR RigVM_BoolTrueFormat[] = TEXT("\t{0} = true;");
static constexpr TCHAR RigVM_CopyUnrelatedArraysFormat[] = TEXT("\tCopyUnrelatedArrays<{0}, {1}>({2}, {3});");
static constexpr TCHAR RigVM_CopyOpMethodFormat[] = TEXT("\t{0}{1}{2});");
static constexpr TCHAR RigVM_CopyOpAssignFormat[] = TEXT("\t{0} = {1}{2};");
static constexpr TCHAR RigVM_IncrementOpFormat[] = TEXT("\t{0}++;");
static constexpr TCHAR RigVM_DecrementOpFormat[] = TEXT("\t{0}--;");
static constexpr TCHAR RigVM_EqualsOpFormat[] = TEXT("\t{0} = {1} == {2};");
static constexpr TCHAR RigVM_InvokeEntryFormat[] = TEXT("\tif (InEntryName == EntryName_{0}) return ExecuteEntry_{0}({1});");
static constexpr TCHAR RigVM_InvokeEntryByNameFormat[] = TEXT("\treturn InvokeEntryByName(InEntryName{0});");
static constexpr TCHAR RigVM_InvokeEntryByNameFormat2[] = TEXT("\tif(!InvokeEntryByName({0}{1})) return false;");
static constexpr TCHAR RigVM_CanExecuteEntryFormat[] = TEXT("\tif(!CanExecuteEntry(InEntryName)) { return false; }");
static constexpr TCHAR RigVM_EntryExecuteGuardFormat[] = TEXT("\tFEntryExecuteGuard EntryExecuteGuard(EntriesBeingExecuted, FindEntry(InEntryName));");
static constexpr TCHAR RigVM_PublicContextGuardFormat[] = TEXT("\tTGuardValue<FRigVMExecuteContext> PublicContextGuard(Context.PublicData, PublicContext);");
static constexpr TCHAR RigVM_EntryNameFormat[] = TEXT("EntryName_{0}");
static constexpr TCHAR RigVM_UpdateContextFormat[] = TEXT("\tconst FRigVMExecuteContext& PublicContext = UpdateContext(AdditionalArguments, {0}, InEntryName);");
static constexpr TCHAR RigVM_TrueFormat[] = TEXT("true");  
static constexpr TCHAR RigVM_FalseFormat[] = TEXT("false");
static constexpr TCHAR RigVM_SingleUnderscoreFormat[] = TEXT("_");
static constexpr TCHAR RigVM_DoubleUnderscoreFormat[] = TEXT("__");
static constexpr TCHAR RigVM_BoolPropertyPrefix[] = TEXT("b");
static constexpr TCHAR RigVM_EnumTypeSuffixFormat[] = TEXT("::Type");
static constexpr TCHAR RigVM_IsValidArraySizeFormat[] = TEXT("IsValidArraySize({0})");
static constexpr TCHAR RigVM_IsValidArrayIndexFormat[] = TEXT("IsValidArrayIndex<{0}>(TemporaryArrayIndex, {1})");
static constexpr TCHAR RigVM_TemporaryArrayIndexFormat[] = TEXT("\tTemporaryArrayIndex = {0};");
static constexpr TCHAR RigVM_ExecuteReachedExitFormat[] = TEXT("\tBroadcastExecutionReachedExit();");
static constexpr TCHAR RigVM_AdditionalArgumentNameFormat[] = TEXT("AdditionalArgument_{0}_{1}");
static constexpr TCHAR RigVM_FullOpaqueArgumentFormat[] = TEXT("\t{0} {1} = *({2}*)Context.OpaqueArguments[{3}];");
static constexpr TCHAR RigVM_ParameterOpaqueArgumentFormat[] = TEXT("{0} {1}");
static constexpr TCHAR RigVM_InstructionLabelFormat[] = TEXT("\tInstruction{0}Label:");
static constexpr TCHAR RigVM_SetInstructionIndexFormat[] = TEXT("\tSetInstructionIndex({0});");
static constexpr TCHAR RigVM_ContextPublicFormat[] = TEXT("PublicContext");
static constexpr TCHAR RigVM_ContextPublicParameterFormat[] = TEXT("const FRigVMExecuteContext& PublicContext");
static constexpr TCHAR RigVM_NotEqualsOpFormat[] = TEXT("\t{0} = {1} != {2};");
static constexpr TCHAR RigVM_JumpOpFormat[] = TEXT("\tgoto Instruction{0}Label;");
static constexpr TCHAR RigVM_JumpIfOpFormat[] = TEXT("\tif ({0} == {1}) { goto Instruction{2}Label; }");
static constexpr TCHAR RigVM_BeginBlockOpFormat[] = TEXT("\tBeginSlice({0}, {1});");
static constexpr TCHAR RigVM_EndBlockOpFormat[] = TEXT("\tEndSlice();");
static constexpr TCHAR RigVM_ArrayResetOpFormat[] = TEXT("\t{0}.Reset();");
static constexpr TCHAR RigVM_ArrayGetNumOpFormat[] = TEXT("\t{0} = {1}.Num();");
static constexpr TCHAR RigVM_ArraySetNumOpFormat[] = TEXT("\tif ({0}) { {1}.SetNum({2}); }");
static constexpr TCHAR RigVM_ArrayGetAtIndexOpFormat[] = TEXT("\tif ({0}) { {1} = {2}[TemporaryArrayIndex]; }");
static constexpr TCHAR RigVM_ArraySetAtIndexOpFormat[] = TEXT("\tif ({0}) { {1}[TemporaryArrayIndex] = {2}; }");
static constexpr TCHAR RigVM_ArrayAddOpFormat[] = TEXT("\tif ({0}) { {1} = {2}.Add({3}); }");
static constexpr TCHAR RigVM_ArrayNumPlusOneFormat[] = TEXT("{0}.Num() + 1");
static constexpr TCHAR RigVM_ArrayInsertOpFormat[] = TEXT("\tif ({0}) { {1}.Insert({2}, {3}); }");
static constexpr TCHAR RigVM_ArrayRemoveOpFormat[] = TEXT("\t{0}.RemoveAt({1});");
static constexpr TCHAR RigVM_ArrayFindOpFormat[] = TEXT("\t{0} = ArrayOp_Find{1}({2}, {3}, {4});");
static constexpr TCHAR RigVM_ArrayAppendOpFormat[] = TEXT("\tif ({0} { {1}.Append({2}); }");
static constexpr TCHAR RigVM_ArrayAppendOpNumFormat[] = TEXT("{0}.Num() + {1}.Num()");
static constexpr TCHAR RigVM_ArrayIteratorOpFormat[] = TEXT("\t{0} = ArrayOp_Iterator{1}({2}, {3}, {4}, {5}, {6});");
static constexpr TCHAR RigVM_ArrayUnionOpFormat[] = TEXT("\tArrayOp_Union{0}({1}, {2});");
static constexpr TCHAR RigVM_ArrayDifferenceOpFormat[] = TEXT("\tArrayOp_Difference{0}({1}, {2});");
static constexpr TCHAR RigVM_ArrayIntersectionOpFormat[] = TEXT("\tArrayOp_Intersection{0}({1}, {2});");
static constexpr TCHAR RigVM_ArrayReverseOpFormat[] = TEXT("\tArrayOp_Reverse{0}({1});");
static constexpr TCHAR RigVM_ReturnFalseFormat[] = TEXT("\treturn false;");
static constexpr TCHAR RigVM_ReturnTrueFormat[] = TEXT("\treturn true;");
static constexpr TCHAR RigVM_CopyrightFormat[] = TEXT("// Copyright Epic Games, Inc. All Rights Reserved.");
static constexpr TCHAR RigVM_AutoGeneratedFormat[] = TEXT("// THIS FILE HAS BEEN AUTO-GENERATED. PLEASE DO NOT MANUALLY EDIT THIS FILE FURTHER.");
static constexpr TCHAR RigVM_PragmaOnceFormat[] = TEXT("#pragma once");
static constexpr TCHAR RigVM_GeneratedIncludeFormat[] = TEXT("#include \"{0}.generated.h\"");
static constexpr TCHAR RigVM_UClassDefinitionFormat[] = TEXT("UCLASS()\r\nclass {0}_API U{1} : public URigVMNativized\r\n{\r\n\tGENERATED_BODY()\r\npublic:\r\n\tU{1}() {}\r\n\tvirtual ~U{1}() override {}\r\n");
static constexpr TCHAR RigVM_ProtectedFormat[] = TEXT("protected:");
static constexpr TCHAR RigVM_GetVMHashFormat[] = TEXT("\tvirtual uint32 GetVMHash() const override { return {0}; }");
static constexpr TCHAR RigVM_GetEntryNamesFormat[] = TEXT("\tvirtual const TArray<FName>& GetEntryNames() const override\r\n\t{\r\n\t\tstatic const TArray<FName> StaticEntryNames = { {0} };\r\n\t\treturn StaticEntryNames;\r\n\t}");
static constexpr TCHAR RigVM_DeclareUpdateExternalVariablesFormat[] = TEXT("\tvirtual void UpdateExternalVariables() override;");
static constexpr TCHAR RigVM_DeclareInvokeEntryByNameFormat[] = TEXT("\tbool InvokeEntryByName(const FName& InEntryName{0});");
static constexpr TCHAR RigVM_DeclareExecuteFormat[] = TEXT("\tvirtual bool Execute(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, const FName& InEntryName) override;");
static constexpr TCHAR RigVM_DefineUpdateExternalVariablesFormat[] = TEXT("void U{0}::UpdateExternalVariables()\r\n{");
static constexpr TCHAR RigVM_DefineInvokeEntryByNameFormat[] = TEXT("bool U{0}::InvokeEntryByName(const FName& InEntryName{1})\r\n{");
static constexpr TCHAR RigVM_DefineExecuteFormat[] = TEXT("bool U{0}::Execute(TArrayView<URigVMMemoryStorage*> Memory, TArrayView<void*> AdditionalArguments, const FName& InEntryName)\r\n{");
static constexpr TCHAR RigVM_DeclareExecuteEntryFormat[] = TEXT("\tbool ExecuteEntry_{0}({1});");
static constexpr TCHAR RigVM_DefineExecuteEntryFormat[] = TEXT("bool U{0}::ExecuteEntry_{1}({2})\r\n{");
static constexpr TCHAR RigVM_DeclareExecuteGroupFormat[] = TEXT("\tbool ExecuteGroup_{0}_{1}({2});");
static constexpr TCHAR RigVM_DefineExecuteGroupFormat[] = TEXT("bool U{0}::ExecuteGroup_{1}_{2}({3})\r\n{");
static constexpr TCHAR RigVM_InvokeExecuteGroupFormat[] = TEXT("\tif(!ExecuteGroup_{0}_{1}({2})) return false;");
static constexpr TCHAR RigVM_RigVMCoreIncludeFormat[] = TEXT("RigVMCore/RigVMCore.h");
static constexpr TCHAR RigVM_RigVMCoreLibraryFormat[] = TEXT("RigVM");
static constexpr TCHAR RigVM_JoinFilePathFormat[] = TEXT("{0}/{1}");
static constexpr TCHAR RigVM_GetOperandSliceFormat[] = TEXT("GetOperandSlice<{0}>({1}){2}");
static constexpr TCHAR RigVM_ExternalVariableFormat[] = TEXT("(*External_{0})");
static constexpr TCHAR RigVM_JoinSegmentPathFormat[] = TEXT("{0}.{1}");
static constexpr TCHAR RigVM_GetArrayElementSafeFormat[] = TEXT("GetArrayElementSafe<{0}>({1}, {2})");
static constexpr TCHAR RigVM_InvokeEntryOpFormat[] = TEXT("\tif(!InvokeEntryByName(EntryName_{0})) return false;");

FString FRigVMCodeGenerator::DumpIncludes(bool bLog)
{
	FStringArray Lines;
	for(const FString& Include : Includes)
	{
		Lines.Add(Format(RigVM_IncludeBracketFormat, Include));
	}
	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpWrappedArrayTypes(bool bLog)
{
	FStringArray Lines;
	for(const FString& WrappedArrayType : WrappedArrayCPPTypes)
	{
		if (!Lines.IsEmpty())
		{
			Lines.Add(FString());
		}

		const FString TypeName = GetMappedArrayTypeName(WrappedArrayType);
		const FString Line = Format(RigVM_WrappedArrayTypeFormat, *ModuleName.ToUpper(), *TypeName, *WrappedArrayType);
		Lines.Add(Line);
	}

	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpExternalVariables(bool bForHeader, bool bLog)
{

	FStringArray Lines;
	for(int32 ExternalVariableIndex = 0; ExternalVariableIndex < VM->GetExternalVariables().Num(); ExternalVariableIndex++)
	{
		const FRigVMOperand ExternalVarOperand(ERigVMMemoryType::External, ExternalVariableIndex, INDEX_NONE);
		const FRigVMExternalVariable& ExternalVariable = VM->GetExternalVariables()[ExternalVariableIndex]; //-V758
		const FString ExternalVarCPPType = ExternalVariable.GetExtendedCPPType().ToString();
		FString OperandName = *GetOperandName(ExternalVarOperand, false);
		if(OperandName.StartsWith(TEXT("(*")) && OperandName.EndsWith(TEXT(")")))
		{
			OperandName = OperandName.Mid(2, OperandName.Len() - 3);
		}

		if(bForHeader)
		{
			Lines.Add(Format(RigVM_DeclareExternalVariableFormat, *ExternalVarCPPType, *OperandName));
		}
		else
		{
			Lines.Add(Format(RigVM_UpdateExternalVariableFormat, *OperandName, *ExternalVarCPPType, *ExternalVariable.Name.ToString(), *ExternalVarCPPType));
		}
	}

	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpEntries(bool bForHeader, bool bLog)
{
	FStringArray Lines;

	const FRigVMByteCode& ByteCode = VM->GetByteCode();
	for(int32 EntryIndex = 0; EntryIndex < ByteCode.NumEntries(); EntryIndex++)
	{
		const FRigVMByteCodeEntry& Entry = ByteCode.GetEntry(EntryIndex);
		const FString EntryName = Entry.GetSanitizedName();
		if(bForHeader)
		{
			Lines.Add(Format(RigVM_DeclareEntryNameFormat, *EntryName));
		}
		else
		{
			Lines.Add(Format(RigVM_DefineEntryNameFormat, *ClassName, *EntryName, *Entry.Name.ToString()));
		}
	}

	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpProperties(bool bForHeader, int32 InOperationGroup, bool bLog)
{
	if(bForHeader)
	{
		// for headers we show all properties
		check(InOperationGroup == INDEX_NONE);
	}

	FStringArray Lines;
	for(int32 Index = 0; Index < Properties.Num(); Index++)
	{
		if(!IsPropertyPartOfGroup(Index, InOperationGroup))
		{
			continue;
		}

		const FPropertyInfo& PropertyInfo = Properties[Index];
		
		// in headers we only dump the work / sliced properties,
		// and for source files we only dump the non-sliced
		if(bForHeader != (PropertyInfo.PropertyType == ERigVMNativizedPropertyType::Sliced ||
							(PropertyInfo.PropertyType == ERigVMNativizedPropertyType::Work &&
								PropertyInfo.Groups.Num() > 1)))
		{
			continue;
		}

		const FRigVMPropertyDescription& Property = PropertyInfo.Description;
		check(Property.IsValid());

		if(PropertyInfo.PropertyType == ERigVMNativizedPropertyType::Literal)
		{
			FRigVMOperand Operand(ERigVMMemoryType::Literal, PropertyInfo.MemoryPropertyIndex, INDEX_NONE);
			FString OperandName = GetOperandName(Operand, false);
			FString CPPType = GetOperandCPPType(Operand);

			FString BaseCPPType = CPPType;
			const bool bIsArray = RigVMTypeUtils::IsArrayType(CPPType);
			if (bIsArray)
			{
				BaseCPPType = RigVMTypeUtils::BaseTypeFromArrayType(CPPType);
			}
			
			FString DefaultValue = Property.DefaultValue;

			if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Property.CPPTypeObject))
			{
				DefaultValue = DefaultValue.ReplaceCharWithEscapedChar();

				if(DefaultValue.IsEmpty())
				{
					Lines.Add(Format(
						RigVM_DefineConstFormatNoDefault,
						*CPPType,
						*GetOperandName(Operand, false)
					));
				}
				else
				{
					if(const TStructConstGenerator* StructConstGenerator = GetStructConstGenerators().Find(*BaseCPPType))
					{
						if(bIsArray)
						{
							FStringArray DefaultValues = URigVMPin::SplitDefaultValue(DefaultValue);
							for(int32 DefaultValueIndex = 0; DefaultValueIndex < DefaultValues.Num(); DefaultValueIndex++)
							{
								DefaultValues[DefaultValueIndex] = (*StructConstGenerator)(DefaultValues[DefaultValueIndex]); 
							}
							DefaultValue = Format(RigVM_CurlyBracesFormat, FString::Join(DefaultValues, RigVM_CommaSeparator));
						}
						else
						{
							DefaultValue = (*StructConstGenerator)(DefaultValue);
						}
						
						Lines.Add(Format(
							RigVM_DefineConstFormat,
							*CPPType,
							*OperandName,
							*DefaultValue
						));
					}
					else if (bIsArray)
					{
						Lines.Add(Format(
							RigVM_DefineStructArrayConstFormat,
							*CPPType,
							*OperandName,
							*BaseCPPType,
							*DefaultValue
						));
					}
					else
					{
						Lines.Add(Format(
							RigVM_DefineStructConstFormat,
							*CPPType,
							*OperandName,
							*DefaultValue
						));
					}
				}
			}
			else if (const UEnum* Enum = Cast<UEnum>(Property.CPPTypeObject))
			{
				BaseCPPType = Enum->GetName();
				if (Enum->GetCppForm() == UEnum::ECppForm::Namespaced)
				{
					BaseCPPType += RigVM_EnumTypeSuffixFormat;
				}
				
				CPPType = BaseCPPType;
				if (bIsArray)
				{
					CPPType = RigVMTypeUtils::ArrayTypeFromBaseType(BaseCPPType);
				}

				FStringArray DefaultValues;
				if (bIsArray)
				{
					DefaultValues = URigVMPin::SplitDefaultValue(DefaultValue);
				}
				else
				{
					DefaultValues.Add(DefaultValue);
				}

				for(int32 DefaultValueIndex = 0; DefaultValueIndex < DefaultValues.Num(); DefaultValueIndex++)
				{
					if (DefaultValues[DefaultValueIndex].IsNumeric())
					{
						const int64 EnumIndex = FCString::Atoi64(*DefaultValues[DefaultValueIndex]);
						const FString EnumName = Enum->GetNameStringByValue(EnumIndex);
						DefaultValues[DefaultValueIndex] = Enum->GenerateFullEnumName(*EnumName);
					}
					else if (!UEnum::IsFullEnumName(*DefaultValues[DefaultValueIndex]))
					{
						DefaultValues[DefaultValueIndex] = Enum->GenerateFullEnumName(*DefaultValues[DefaultValueIndex]);
					}
				}
				
				if (bIsArray)
				{
					DefaultValue = Format(RigVM_CurlyBracesFormat, *FString::Join(DefaultValues, TEXT(",")));
				}
				else
				{
					DefaultValue = DefaultValues[0];
				}


				if(DefaultValue.IsEmpty())
				{
					Lines.Add(Format(
						RigVM_DefineConstFormatNoDefault,
						*CPPType,
						*OperandName
					));
				}
				else
				{
					Lines.Add(Format(
						RigVM_DefineConstFormat,
						*CPPType,
						*OperandName,
						*DefaultValue
					));
				}
			}
			else
			{
				bool bUseConstExpr = true;

				if (bIsArray)
				{
					bUseConstExpr = false;
					
					if (DefaultValue.StartsWith(TEXT("(")) && DefaultValue.EndsWith(TEXT(")")))
					{
						DefaultValue = Format(RigVM_CurlyBracesFormat, *DefaultValue.Mid(1, DefaultValue.Len() - 2));
					}
				}

				if (BaseCPPType == RigVMTypeUtils::BoolType)
				{
					DefaultValue.ToLowerInline();
				}

				if (CPPType == RigVMTypeUtils::FNameType || CPPType == RigVMTypeUtils::FStringType)
				{
					if(DefaultValue.IsEmpty())
					{
						if(CPPType == RigVMTypeUtils::FNameType)
						{
							DefaultValue = RigVM_NameNoneFormat;
						}
						else
						{
							DefaultValue = RigVM_EmptyStringFormat;
						}
					}
					else
					{
						if (DefaultValue.StartsWith(TEXT("\"")) && DefaultValue.EndsWith(TEXT("\"")))
						{
							DefaultValue = Format(RigVM_TextFormat, *DefaultValue);
						}
						else
						{
							DefaultValue = Format(RigVM_QuotedTextFormat, *DefaultValue);
						}
					}
					bUseConstExpr = false;
				}

				if(DefaultValue.IsEmpty())
				{
					Lines.Add(Format(
						RigVM_DefineConstFormatNoDefault, 
						*CPPType,
						*OperandName
					));
				}
				else
				{
					Lines.Add(Format(
						RigVM_DefineConstFormat, 
						*CPPType,
						*OperandName,
						*DefaultValue
					));
				}
			}
		}
		else // work and slice look the same in the file
		{
			FRigVMOperand Operand(ERigVMMemoryType::Work, PropertyInfo.MemoryPropertyIndex, INDEX_NONE);
			FString OperandName = GetOperandName(Operand, false);
			FString CPPType = GetOperandCPPType(Operand);

			const FString MappedType = GetMappedType(Property.CPPType);
			const FString Line = Format(RigVM_MemberPropertyFormat, *MappedType, *SanitizeName(Property.Name.ToString(), Property.CPPType));
			Lines.Add(Line);
		}
	}

	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpInstructions(int32 InOperationGroup, bool bLog)
{
	const FRigVMByteCode& ByteCode = VM->GetByteCode();
	const TArray<FName>& Functions = VM->GetFunctionNames();
	const FRigVMInstructionArray Operations = ByteCode.GetInstructions();

	FStringArray Lines;

	const FOperationGroup& Group = GetGroup(InOperationGroup);

	auto GetEntryParameters = [&]() -> FString
	{
		const int32 EntryIndex = ByteCode.NumEntries() - 1;
		const FRigVMByteCodeEntry& Entry = ByteCode.GetEntry(EntryIndex);
		const FString EntryName = Entry.GetSanitizedName();
		FString Parameters;

		// find the entry's group and provide the needed arguments
		for(const FOperationGroup& EntryGroup : OperationGroups)
		{
			if(EntryGroup.Depth <= 0 && EntryGroup.Entry == EntryName)
			{
				TArray<FString> ArgumentNames = {RigVM_ContextPublicFormat};
				for(const FOpaqueArgument& OpaqueArgument : EntryGroup.OpaqueArguments)
				{
					ArgumentNames.Add(OpaqueArgument.Get<1>());
				}
				Parameters = FString::Join(ArgumentNames, RigVM_CommaSeparator);
				break;
			}
		}

		if(!Parameters.IsEmpty())
		{
			Parameters = RigVM_CommaSeparator + Parameters;
		}
		
		return Parameters;
	};

	if(Group.Entry.IsEmpty())
	{
		if(InOperationGroup == INDEX_NONE)
		{
			Lines.Add(Format(RigVM_UpdateContextFormat, Operations.Num()));

			// let's get the additional arguments for all sub groups
			TArray<FString> OpaqueArgumentHit;
			for(const FOperationGroup& EntryGroup : OperationGroups)
			{
				if(EntryGroup.Entry.IsEmpty() || EntryGroup.Depth > 0)
				{
					continue;
				}

				for(const FOpaqueArgument& OpaqueArgument : EntryGroup.OpaqueArguments)
				{
					if(OpaqueArgumentHit.Contains(OpaqueArgument.Get<0>()))
					{
						continue;
					}
					const int32 ArgumentIndex = OpaqueArgumentHit.Add(OpaqueArgument.Get<0>());
					FString ArgumentTypeNoRef = OpaqueArgument.Get<2>();
					if (ArgumentTypeNoRef.EndsWith(TEXT("&")))
					{
						ArgumentTypeNoRef.LeftChopInline(1);
					}
					Lines.Add(Format(RigVM_FullOpaqueArgumentFormat, *OpaqueArgument.Get<2>(), *OpaqueArgument.Get<1>(), *ArgumentTypeNoRef, ArgumentIndex));
				}
			}

			Lines.Add(Format(RigVM_InvokeEntryByNameFormat, *GetEntryParameters()));

			return DumpLines(Lines, bLog);
		}
		else
		{
			Lines.Add(FString(RigVM_CanExecuteEntryFormat));
			Lines.Emplace();
			Lines.Add(FString(RigVM_EntryExecuteGuardFormat));
			Lines.Add(FString(RigVM_PublicContextGuardFormat));
			Lines.Emplace();
			
			for(int32 EntryIndex = 0; EntryIndex < ByteCode.NumEntries(); EntryIndex++)
			{
				const FRigVMByteCodeEntry& Entry = ByteCode.GetEntry(EntryIndex);
				const FString EntryName = Entry.GetSanitizedName();
				FString OpaqueArguments;

				// find the entry's group and provide the needed arguments
				for(const FOperationGroup& EntryGroup : OperationGroups)
				{
					if(EntryGroup.Depth <= 0 && EntryGroup.Entry == EntryName)
					{
						TArray<FString> ArgumentNames = {RigVM_ContextPublicFormat};
						for(const FOpaqueArgument& OpaqueArgument : EntryGroup.OpaqueArguments)
						{
							ArgumentNames.Add(OpaqueArgument.Get<1>());
						}
						OpaqueArguments = FString::Join(ArgumentNames, RigVM_CommaSeparator);
						break;
					}
				}
				Lines.Add(Format(RigVM_InvokeEntryFormat, *EntryName, *OpaqueArguments));
			}
		}
	}

	if(Group.Entry.IsEmpty() && OperationGroups.Num() > 0)
	{
		Lines.Add(RigVM_ReturnFalseFormat);
		return DumpLines(Lines, bLog);
	}

	if(Group.ChildGroups.IsEmpty())
	{
		// loop over all of the operations once more
		for(int32 OperationIndex = Group.First; OperationIndex <= Group.Last; OperationIndex++)
		{
			// inject a label if required
			if (Group.RequiredLabels.Contains(OperationIndex))
			{
				Lines.Add(Format(RigVM_InstructionLabelFormat, OperationIndex));
			}

			Lines.Add(Format(RigVM_SetInstructionIndexFormat, OperationIndex));

			const FRigVMInstruction& Operation = Operations[OperationIndex];
			switch(Operation.OpCode)
			{
				case ERigVMOpCode::Execute_0_Operands:
				case ERigVMOpCode::Execute_1_Operands:
				case ERigVMOpCode::Execute_2_Operands:
				case ERigVMOpCode::Execute_3_Operands:
				case ERigVMOpCode::Execute_4_Operands:
				case ERigVMOpCode::Execute_5_Operands:
				case ERigVMOpCode::Execute_6_Operands:
				case ERigVMOpCode::Execute_7_Operands:
				case ERigVMOpCode::Execute_8_Operands:
				case ERigVMOpCode::Execute_9_Operands:
				case ERigVMOpCode::Execute_10_Operands:
				case ERigVMOpCode::Execute_11_Operands:
				case ERigVMOpCode::Execute_12_Operands:
				case ERigVMOpCode::Execute_13_Operands:
				case ERigVMOpCode::Execute_14_Operands:
				case ERigVMOpCode::Execute_15_Operands:
				case ERigVMOpCode::Execute_16_Operands:
				case ERigVMOpCode::Execute_17_Operands:
				case ERigVMOpCode::Execute_18_Operands:
				case ERigVMOpCode::Execute_19_Operands:
				case ERigVMOpCode::Execute_20_Operands:
				case ERigVMOpCode::Execute_21_Operands:
				case ERigVMOpCode::Execute_22_Operands:
				case ERigVMOpCode::Execute_23_Operands:
				case ERigVMOpCode::Execute_24_Operands:
				case ERigVMOpCode::Execute_25_Operands:
				case ERigVMOpCode::Execute_26_Operands:
				case ERigVMOpCode::Execute_27_Operands:
				case ERigVMOpCode::Execute_28_Operands:
				case ERigVMOpCode::Execute_29_Operands:
				case ERigVMOpCode::Execute_30_Operands:
				case ERigVMOpCode::Execute_31_Operands:
				case ERigVMOpCode::Execute_32_Operands:
				case ERigVMOpCode::Execute_33_Operands:
				case ERigVMOpCode::Execute_34_Operands:
				case ERigVMOpCode::Execute_35_Operands:
				case ERigVMOpCode::Execute_36_Operands:
				case ERigVMOpCode::Execute_37_Operands:
				case ERigVMOpCode::Execute_38_Operands:
				case ERigVMOpCode::Execute_39_Operands:
				case ERigVMOpCode::Execute_40_Operands:
				case ERigVMOpCode::Execute_41_Operands:
				case ERigVMOpCode::Execute_42_Operands:
				case ERigVMOpCode::Execute_43_Operands:
				case ERigVMOpCode::Execute_44_Operands:
				case ERigVMOpCode::Execute_45_Operands:
				case ERigVMOpCode::Execute_46_Operands:
				case ERigVMOpCode::Execute_47_Operands:
				case ERigVMOpCode::Execute_48_Operands:
				case ERigVMOpCode::Execute_49_Operands:
				case ERigVMOpCode::Execute_50_Operands:
				case ERigVMOpCode::Execute_51_Operands:
				case ERigVMOpCode::Execute_52_Operands:
				case ERigVMOpCode::Execute_53_Operands:
				case ERigVMOpCode::Execute_54_Operands:
				case ERigVMOpCode::Execute_55_Operands:
				case ERigVMOpCode::Execute_56_Operands:
				case ERigVMOpCode::Execute_57_Operands:
				case ERigVMOpCode::Execute_58_Operands:
				case ERigVMOpCode::Execute_59_Operands:
				case ERigVMOpCode::Execute_60_Operands:
				case ERigVMOpCode::Execute_61_Operands:
				case ERigVMOpCode::Execute_62_Operands:
				case ERigVMOpCode::Execute_63_Operands:
				case ERigVMOpCode::Execute_64_Operands:
				{
					const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Operation);
					FRigVMOperandArray Operands = ByteCode.GetOperandsForExecuteOp(Operation);

					const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*Functions[Op.FunctionIndex].ToString());
					check(Function);
						
					FStringArray Arguments;
					Arguments.Add(RigVM_ContextPublicFormat);
					for(int32 OperandIndex = 0; OperandIndex < Operands.Num(); OperandIndex++)
					{
						const FRigVMOperand& Operand = Operands[OperandIndex];
						bool bSliced = false;
						const FRigVMPropertyDescription& Property = GetPropertyForOperand(Operand);
						if (RigVMTypeUtils::IsArrayType(Property.CPPType))
						{
							const FRigVMFunctionArgument& FunctionArgument = Function->GetArguments()[OperandIndex];
							bSliced = FunctionArgument.Type != Property.CPPType;
						}
						Arguments.Add(GetOperandName(Operand, bSliced));
					}

					for(const FRigVMFunctionArgument& Argument : Function->GetArguments())
					{
						if (Function->IsAdditionalArgument(Argument))
						{
							const FOpaqueArgument* OpaqueArgument = Group.OpaqueArguments.FindByPredicate([Argument](const FOpaqueArgument& InArgument) -> bool
							{
								return InArgument.Get<0>() == Argument.Name;
							});

							check(OpaqueArgument);
							Arguments.Add(OpaqueArgument->Get<1>());
						}
					}

					const FString JoinedArguments = FString::Join(Arguments, RigVM_CommaSeparator);
					Lines.Add(Format(RigVM_CallExternOpFormat, *Function->Struct->GetStructCPPName(), *Function->GetMethodName().ToString(), *JoinedArguments));
					break;
				}
				case ERigVMOpCode::Zero:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Operation);
					Lines.Add(Format(RigVM_ZeroOpFormat, *GetOperandName(Op.Arg, false)));
					break;
				}
				case ERigVMOpCode::BoolFalse:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Operation);
					Lines.Add(Format(RigVM_BoolFalseOpFormat, *GetOperandName(Op.Arg, false)));
					break;
				}
				case ERigVMOpCode::BoolTrue:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Operation);
					Lines.Add(Format(RigVM_BoolTrueFormat, *GetOperandName(Op.Arg, false)));
					break;
				}
				case ERigVMOpCode::Copy:
				case ERigVMOpCode::ArrayClone:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					const FString TargetOperand = GetOperandName(Op.ArgB, false, false);
					const FString SourceOperand = GetOperandName(Op.ArgA, false, true);
					const FString TargetCPPType = GetOperandCPPType(Op.ArgB);
					const FString SourceCPPType = GetOperandCPPType(Op.ArgA);

					if(RigVMTypeUtils::IsArrayType(TargetCPPType) &&
						RigVMTypeUtils::IsArrayType(SourceCPPType) &&
						TargetCPPType != SourceCPPType)
					{
						const FString TargetBaseCPPType = RigVMTypeUtils::BaseTypeFromArrayType(TargetCPPType);
						const FString SourceBaseCPPType = RigVMTypeUtils::BaseTypeFromArrayType(SourceCPPType);
						Lines.Add(Format(RigVM_CopyUnrelatedArraysFormat, *TargetBaseCPPType, *SourceBaseCPPType, *TargetOperand, *SourceOperand));
					}
					else
					{
						const FString CastPrefix = TargetCPPType != SourceCPPType ? Format(RigVM_BracesFormat, *TargetCPPType) : FString();  
						
						if(TargetOperand.EndsWith(TEXT("(")))
						{
							Lines.Add(Format(RigVM_CopyOpMethodFormat, *TargetOperand, *CastPrefix, *SourceOperand));
						}
						else
						{
							Lines.Add(Format(RigVM_CopyOpAssignFormat, *TargetOperand, *CastPrefix, *SourceOperand));
						}
					}
					break;
				}
				case ERigVMOpCode::Increment:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Operation);
					Lines.Add(Format(RigVM_IncrementOpFormat, *GetOperandName(Op.Arg, false)));
					break;
				}
				case ERigVMOpCode::Decrement:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Operation);
					Lines.Add(Format(RigVM_DecrementOpFormat, *GetOperandName(Op.Arg, false)));
					break;
				}
				case ERigVMOpCode::Equals:
				{
					const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Operation);
					Lines.Add(Format(RigVM_EqualsOpFormat, *GetOperandName(Op.Result, false), *GetOperandName(Op.B, false), *GetOperandName(Op.B, false)));
					break;
				}
				case ERigVMOpCode::NotEquals:
				{
					const FRigVMComparisonOp& Op = ByteCode.GetOpAt<FRigVMComparisonOp>(Operation);
					Lines.Add(Format(RigVM_NotEqualsOpFormat, *GetOperandName(Op.Result, false), *GetOperandName(Op.B, false), *GetOperandName(Op.B, false)));
					break;
				}
				case ERigVMOpCode::JumpAbsolute:
				{
					const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
					Lines.Add(Format(RigVM_JumpOpFormat, Op.InstructionIndex));
					break;
				}
				case ERigVMOpCode::JumpForward:
				{
					const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
					Lines.Add(Format(RigVM_JumpOpFormat, OperationIndex + Op.InstructionIndex));
					break;
				}
				case ERigVMOpCode::JumpBackward:
				{
					const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
					Lines.Add(Format(RigVM_JumpOpFormat, OperationIndex - Op.InstructionIndex));
					break;
				}
				case ERigVMOpCode::JumpAbsoluteIf:
				{
					const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
					const FString& Condition = Op.Condition ? RigVM_TrueFormat : RigVM_FalseFormat;
					Lines.Add(Format(RigVM_JumpIfOpFormat, *GetOperandName(Op.Arg, false), *Condition, Op.InstructionIndex));
					break;
				}
				case ERigVMOpCode::JumpForwardIf:
				{
					const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
					const FString& Condition = Op.Condition ? RigVM_TrueFormat : RigVM_FalseFormat;
					Lines.Add(Format(RigVM_JumpIfOpFormat, *GetOperandName(Op.Arg, false), *Condition, OperationIndex + Op.InstructionIndex));
					break;
				}
				case ERigVMOpCode::JumpBackwardIf:
				{
					const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
					const FString& Condition = Op.Condition ? RigVM_TrueFormat : RigVM_FalseFormat;
					Lines.Add(Format(RigVM_JumpIfOpFormat, *GetOperandName(Op.Arg, false), *Condition, OperationIndex - Op.InstructionIndex));
					break;
				}
				case ERigVMOpCode::Exit:
				{
					if(OperationIndex != Group.Last)
					{
						Lines.Add(Format(RigVM_JumpOpFormat, Operations.Num()));
					}
					break;
				}
				case ERigVMOpCode::BeginBlock:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					Lines.Add(Format(RigVM_BeginBlockOpFormat, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::EndBlock:
				{
					Lines.Add(RigVM_EndBlockOpFormat);
					break;
				}
				case ERigVMOpCode::ArrayReset:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Operation);
					Lines.Add(Format(RigVM_ArrayResetOpFormat, *GetOperandName(Op.Arg, false)));
					break;
				}
				case ERigVMOpCode::ArrayGetNum:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					Lines.Add(Format(RigVM_ArrayGetNumOpFormat, *GetOperandName(Op.ArgB, false), *GetOperandName(Op.ArgA, false)));
					break;
				} 
				case ERigVMOpCode::ArraySetNum:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					const FString ConditionString = Format(RigVM_IsValidArraySizeFormat, *GetOperandName(Op.ArgB, false));
					Lines.Add(Format(RigVM_ArraySetNumOpFormat, *ConditionString, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::ArrayGetAtIndex:
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Operation);
					const FString ConditionString = Format(RigVM_IsValidArrayIndexFormat, *GetOperandCPPBaseType(Op.ArgA), *GetOperandName(Op.ArgA, false));
					Lines.Add(Format(RigVM_TemporaryArrayIndexFormat, *GetOperandName(Op.ArgB, false)));
					Lines.Add(Format(RigVM_ArrayGetAtIndexOpFormat, *ConditionString, *GetOperandName(Op.ArgC, false), *GetOperandName(Op.ArgA, false)));
					break;
				}  
				case ERigVMOpCode::ArraySetAtIndex:
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Operation);
					const FString ConditionString = Format(RigVM_IsValidArrayIndexFormat,  *GetOperandCPPBaseType(Op.ArgA), *GetOperandName(Op.ArgA, false));
					Lines.Add(Format(RigVM_TemporaryArrayIndexFormat, *GetOperandName(Op.ArgB, false)));
					Lines.Add(Format(RigVM_ArraySetAtIndexOpFormat, *ConditionString, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgC, false), *GetOperandName(Op.ArgC, false)));
					break;
				}
				case ERigVMOpCode::ArrayAdd:
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Operation);
					const FString NumString = Format(RigVM_ArrayNumPlusOneFormat, *GetOperandName(Op.ArgA, false));
					const FString ConditionString = Format(RigVM_IsValidArraySizeFormat, *NumString);
					Lines.Add(Format(RigVM_ArrayAddOpFormat, *ConditionString, *GetOperandName(Op.ArgC, false), *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::ArrayInsert:
				{
					const FRigVMTernaryOp& Op = ByteCode.GetOpAt<FRigVMTernaryOp>(Operation);
					const FString NumString = Format(RigVM_ArrayNumPlusOneFormat, *GetOperandName(Op.ArgA, false));
					const FString ConditionString = Format(RigVM_IsValidArraySizeFormat, *NumString);
					Lines.Add(Format(RigVM_ArrayInsertOpFormat, *ConditionString, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgC, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::ArrayRemove:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					Lines.Add(Format(RigVM_ArrayRemoveOpFormat, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::ArrayFind:
				{
					const FRigVMQuaternaryOp& Op = ByteCode.GetOpAt<FRigVMQuaternaryOp>(Operation);
					const FString ArrayElementCPPType = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPBaseType(Op.ArgA));
					const FString ElementCPPType = GetOperandCPPType(Op.ArgB);
					const FString TemplateSuffix = ArrayElementCPPType == ElementCPPType ? Format(RigVM_TemplateOneArgFormat, *ElementCPPType) : FString();
					Lines.Add(Format(RigVM_ArrayFindOpFormat, *GetOperandName(Op.ArgD, false), *TemplateSuffix, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false), *GetOperandName(Op.ArgC, false)));
					break;
				}
				case ERigVMOpCode::ArrayAppend:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					const FString NumString = Format(RigVM_ArrayAppendOpNumFormat, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false));
					const FString ConditionString = Format(RigVM_IsValidArraySizeFormat, *NumString);
					Lines.Add(Format(RigVM_ArrayAppendOpFormat, *ConditionString, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::ArrayIterator:
				{
					const FRigVMSenaryOp& Op = ByteCode.GetOpAt<FRigVMSenaryOp>(Operation);
					const FString ArrayElementCPPType = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.ArgA));
					const FString ElementCPPType = GetOperandCPPType(Op.ArgB);
					const FString TemplateSuffix = ArrayElementCPPType == ElementCPPType ? Format(RigVM_TemplateOneArgFormat, *ElementCPPType) : FString();
					Lines.Add(Format(RigVM_ArrayIteratorOpFormat, *GetOperandName(Op.ArgF, false), *TemplateSuffix, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false), *GetOperandName(Op.ArgC, false), *GetOperandName(Op.ArgD, false), *GetOperandName(Op.ArgE, false)));
					break;
				}
				case ERigVMOpCode::ArrayUnion:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					const FString ElementCPPTypeA = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.ArgA));
					const FString ElementCPPTypeB = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.ArgA));
					const FString TemplateSuffix = ElementCPPTypeA == ElementCPPTypeB ? Format(RigVM_TemplateOneArgFormat, *ElementCPPTypeA) : FString();
					Lines.Add(Format(RigVM_ArrayUnionOpFormat, *TemplateSuffix, *GetOperandName(Op.ArgA, false, false), *GetOperandName(Op.ArgB, false, false)));
					break;
				}
				case ERigVMOpCode::ArrayDifference:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					const FString ElementCPPTypeA = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.ArgA));
					const FString ElementCPPTypeB = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.ArgA));
					const FString TemplateSuffix = ElementCPPTypeA == ElementCPPTypeB ? Format(RigVM_TemplateOneArgFormat, *ElementCPPTypeA) : FString();
					Lines.Add(Format(RigVM_ArrayDifferenceOpFormat, *TemplateSuffix, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::ArrayIntersection:
				{
					const FRigVMBinaryOp& Op = ByteCode.GetOpAt<FRigVMBinaryOp>(Operation);
					const FString ElementCPPTypeA = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.ArgA));
					const FString ElementCPPTypeB = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.ArgA));
					const FString TemplateSuffix = ElementCPPTypeA == ElementCPPTypeB ? Format(RigVM_TemplateOneArgFormat, *ElementCPPTypeA) : FString();
					Lines.Add(Format(RigVM_ArrayIntersectionOpFormat, *TemplateSuffix, *GetOperandName(Op.ArgA, false), *GetOperandName(Op.ArgB, false)));
					break;
				}
				case ERigVMOpCode::ArrayReverse:
				{
					const FRigVMUnaryOp& Op = ByteCode.GetOpAt<FRigVMUnaryOp>(Operation);
					const FString ElementCPPType = RigVMTypeUtils::BaseTypeFromArrayType(GetOperandCPPType(Op.Arg));
					const FString TemplateSuffix = Format(RigVM_TemplateOneArgFormat, *ElementCPPType);
					Lines.Add(Format(RigVM_ArrayReverseOpFormat, *TemplateSuffix, *GetOperandName(Op.Arg, false)));
					break;
				}
				case ERigVMOpCode::InvokeEntry:
				{
					const FRigVMInvokeEntryOp& Op = ByteCode.GetOpAt<FRigVMInvokeEntryOp>(Operation);
					const FString EntryName = Op.EntryName.ToString();
					Lines.Add(Format(RigVM_InvokeEntryByNameFormat2, *EntryName, *GetEntryParameters()));
					break;
				}
				case ERigVMOpCode::Invalid:
				case ERigVMOpCode::ChangeType:
				default:
				{
					// we expect to cover all op types
					checkNoEntry();
				}
			}
		}
	}
	else
	{
		// we have child groups - we need to invoke those
		for(int32 ChildGroupIndex : Group.ChildGroups)
		{
			const FOperationGroup& ChildGroup = OperationGroups[ChildGroupIndex];

			FString Parameters;
			TArray<FString> ParameterArray = {RigVM_ContextPublicFormat};
			if(!ChildGroup.OpaqueArguments.IsEmpty())
			{
				for(const FOpaqueArgument& OpaqueArgument : ChildGroup.OpaqueArguments)
				{
					ParameterArray.Add(OpaqueArgument.Get<1>());
				}
			}
			Parameters = FString::Join(ParameterArray, RigVM_CommaSeparator);
			Lines.Add(Format(RigVM_InvokeExecuteGroupFormat, *ChildGroup.Entry, ChildGroupIndex, *Parameters));
		}
	}
	
	Lines.Emplace();
	if(Group.Depth <= 0)
	{
		Lines.Add(RigVM_ExecuteReachedExitFormat);
	}
	Lines.Add(RigVM_ReturnTrueFormat);

	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpHeader(bool bLog)
{
	const FRigVMByteCode& ByteCode = VM->GetByteCode();

	FStringArray FormattedEntries;
	for(int32 EntryIndex = 0; EntryIndex < ByteCode.NumEntries(); EntryIndex++)
	{
		const FRigVMByteCodeEntry& Entry = ByteCode.GetEntry(EntryIndex);
		FormattedEntries.Add(Format(RigVM_EntryNameFormat, *Entry.GetSanitizedName()));
	}

	FStringArray Lines;
	Lines.Add(RigVM_CopyrightFormat);
	Lines.Emplace();
	Lines.Add(RigVM_AutoGeneratedFormat);
	Lines.Emplace();
	Lines.Add(RigVM_PragmaOnceFormat);
	Lines.Emplace();
	Lines.Add(DumpIncludes());
	Lines.Add(Format(RigVM_GeneratedIncludeFormat, *ClassName));
	Lines.Emplace();
	if(WrappedArrayCPPTypes.Num() > 0)
	{
		Lines.Add(DumpWrappedArrayTypes());
		Lines.Emplace();
	}
	Lines.Add(Format(RigVM_UClassDefinitionFormat, *ModuleName.ToUpper(), *ClassName));
	Lines.Add(Format(RigVM_GetVMHashFormat, VM->GetVMHash()));
	Lines.Add(Format(RigVM_GetEntryNamesFormat, *FString::Join(FormattedEntries, RigVM_CommaSeparator)));
	Lines.Emplace();
	Lines.Add(FString(RigVM_DeclareExecuteFormat));
	Lines.Emplace();
	Lines.Add(RigVM_ProtectedFormat);
	if(!VM->GetExternalVariables().IsEmpty())
	{
		Lines.Add(RigVM_DeclareUpdateExternalVariablesFormat);
	}

	for(int32 GroupIndex = 0; GroupIndex < OperationGroups.Num(); GroupIndex++)
	{
		const FOperationGroup& Group = OperationGroups[GroupIndex];

		FString Parameters;
		TArray<FString> ParameterArray = {RigVM_ContextPublicParameterFormat};
		if(!Group.OpaqueArguments.IsEmpty())
		{
			for(const FOpaqueArgument& OpaqueArgument : Group.OpaqueArguments)
			{
				ParameterArray.Add(Format(RigVM_ParameterOpaqueArgumentFormat, *OpaqueArgument.Get<2>(), *OpaqueArgument.Get<1>()));
			}
		}
		Parameters = FString::Join(ParameterArray, RigVM_CommaSeparator);
		
		if(Group.Depth == 0)
		{
			Lines.Add(Format(RigVM_DeclareExecuteEntryFormat, *Group.Entry, *Parameters));
		}
		else
		{
			Lines.Add(Format(RigVM_DeclareExecuteGroupFormat, *Group.Entry, GroupIndex, *Parameters));
		}

		if(GroupIndex == OperationGroups.Num() - 1)
		{
			if(!Parameters.IsEmpty())
			{
				Parameters = RigVM_CommaSeparator + Parameters;
			}
			Lines.Add(Format(RigVM_DeclareInvokeEntryByNameFormat, *Parameters));
		}
	}

	Lines.Emplace();
	Lines.Add(DumpEntries(true));

	Lines.Emplace();
	Lines.Add(DumpProperties(true, INDEX_NONE));
	if(!VM->GetExternalVariables().IsEmpty())
	{
		Lines.Add(DumpExternalVariables(true));
	}
	Lines.Add(TEXT("};"));
	Lines.Emplace();
	
	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpSource(bool bLog)
{
	const FRigVMByteCode& ByteCode = VM->GetByteCode();

	FStringArray Lines;
	Lines.Add(RigVM_CopyrightFormat);
	Lines.Emplace();
	Lines.Add(RigVM_AutoGeneratedFormat);
	Lines.Emplace();
	Lines.Add(Format(RigVM_IncludeQuoteFormat, *ClassName));
	Lines.Emplace();
	Lines.Add(DumpEntries(false));

	Lines.Emplace();
	Lines.Add(Format(RigVM_DefineExecuteFormat, *ClassName));
	Lines.Add(DumpInstructions(INDEX_NONE));
	Lines.Add(TEXT("}"));

	if(!VM->GetExternalVariables().IsEmpty())
	{
		Lines.Emplace();
		Lines.Add(Format(RigVM_DefineUpdateExternalVariablesFormat, *ClassName));
		Lines.Add(DumpExternalVariables(false));
		Lines.Add(TEXT("}"));
	}

	for(int32 GroupIndex = 0; GroupIndex < OperationGroups.Num(); GroupIndex++)
	{
		const FOperationGroup& Group = OperationGroups[GroupIndex];

		FString Parameters;
		TArray<FString> ParameterArray = {RigVM_ContextPublicParameterFormat};
		if(!Group.OpaqueArguments.IsEmpty())
		{
			for(const FOpaqueArgument& OpaqueArgument : Group.OpaqueArguments)
			{
				ParameterArray.Add(Format(RigVM_ParameterOpaqueArgumentFormat, *OpaqueArgument.Get<2>(), *OpaqueArgument.Get<1>()));
			}
		}
		Parameters = FString::Join(ParameterArray, RigVM_CommaSeparator);

		Lines.Emplace();
		if(Group.Depth == 0)
		{
			Lines.Add(Format(RigVM_DefineExecuteEntryFormat, *ClassName, *Group.Entry, *Parameters));
		}
		else
		{
			Lines.Add(Format(RigVM_DefineExecuteGroupFormat, *ClassName, *Group.Entry, GroupIndex, *Parameters));
		}

		const FString DumpedProperties = DumpProperties(false, GroupIndex);
		if(!DumpedProperties.IsEmpty())
		{
			Lines.Add(DumpedProperties);
			Lines.Emplace();
		}
		Lines.Add(DumpInstructions(GroupIndex));
		Lines.Add(TEXT("}"));

		if(GroupIndex == OperationGroups.Num() - 1)
		{
			if(!Parameters.IsEmpty())
			{
				Parameters = RigVM_CommaSeparator + Parameters;
			}

			Lines.Emplace();
			Lines.Add(Format(RigVM_DefineInvokeEntryByNameFormat, *ClassName, *Parameters));
			Lines.Add(DumpInstructions(-2));
			Lines.Add(TEXT("}"));
		}
	}
	
	return DumpLines(Lines, bLog);
}

FString FRigVMCodeGenerator::DumpLines(const TArray<FString>& InLines, bool bLog)
{
	if (bLog)
	{
		for(const FString& Line : InLines)
		{
			UE_LOG(LogRigVMDeveloper, Display, RigVM_SingleStringFormat, *Line);
		}
	}
	return FString::Join(InLines, RigVM_NewLineFormat);
}

void FRigVMCodeGenerator::Reset()
{
	ClassName.Reset();
	ModuleName.Reset();
	Libraries.Reset();
	Includes.Reset();
	OperationGroups.Reset();
	WrappedArrayCPPTypes.Reset();
	MappedCPPTypes.Reset();
	Properties.Reset();
	PropertyNameToIndex.Reset();
}

void FRigVMCodeGenerator::ParseVM(const FString& InClassName, const FString& InModuleName,
	URigVMGraph* InModelToNativize, URigVM* InVMToNativize,
	TMap<FString,FRigVMOperand> InPinToOperandMap, int32 InMaxOperationsPerFunction)
{
	check(InVMToNativize);

	Reset();

	Model = TStrongObjectPtr<URigVMGraph>(InModelToNativize);
	VM = TStrongObjectPtr<URigVM>(InVMToNativize);
	PinToOperandMap = InPinToOperandMap;
	MaxOperationsPerFunction = InMaxOperationsPerFunction;
	ClassName = InClassName;
	ModuleName = InModuleName;

	// create an inverted map to lookup pins from operands
	OperandToPinMap.Reset();
	for(const TPair<FString,FRigVMOperand>& Pair : PinToOperandMap)
	{
		OperandToPinMap.FindOrAdd(Pair.Value) = Pair.Key;
	}

	// default includes
	ParseInclude(URigVM::StaticClass());

	Properties.Reserve(
		InVMToNativize->GetLiteralMemory(true)->Num() +
		InVMToNativize->GetWorkMemory(true)->Num());
	ParseMemory(InVMToNativize->GetLiteralMemory(true));
	ParseMemory(InVMToNativize->GetWorkMemory(true));

	ParseOperationGroups();
}

void FRigVMCodeGenerator::ParseInclude(UStruct* InDependency, const FName& InMethodName)
{
	if (InDependency->IsChildOf(FRigVMStruct::StaticStruct()))
	{
		if (const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(Cast<UScriptStruct>(InDependency), *InMethodName.ToString()))
		{
			FString FunctionModuleName = Function->GetModuleName();
			if (FunctionModuleName.Contains(TEXT("/")))
			{
				FunctionModuleName.Split(TEXT("/"), nullptr, &FunctionModuleName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			}
			Libraries.AddUnique(FunctionModuleName);
			Includes.AddUnique(Format(RigVM_JoinFilePathFormat, *FunctionModuleName, *Function->GetModuleRelativeHeaderPath()));
			return;
		}
	}

	if (InDependency == URigVM::StaticClass() ||
		InDependency == FRigVMByteCode::StaticStruct() || 
		InDependency->IsChildOf(FRigVMExecuteContext::StaticStruct()) ||
		InDependency->IsChildOf(FRigVMBaseOp::StaticStruct()) ||
		InDependency->IsChildOf(URigVMMemoryStorage::StaticClass()))
	{
		Libraries.AddUnique(RigVM_RigVMCoreLibraryFormat);
		Includes.AddUnique(RigVM_RigVMCoreIncludeFormat);
	}
}

void FRigVMCodeGenerator::ParseMemory(URigVMMemoryStorage* InMemory)
{
	if (InMemory == nullptr)
	{
		return;
	}
	if (InMemory->GetClass() == URigVMMemoryStorage::StaticClass())
	{
		return;
	}

	for (TFieldIterator<FProperty> PropertyIt(InMemory->GetClass()); PropertyIt; ++PropertyIt)
	{
		const FProperty* Property = *PropertyIt;
		ParseProperty(InMemory->GetMemoryType(), Property, InMemory);
	}
}

void FRigVMCodeGenerator::ParseProperty(ERigVMMemoryType InMemoryType, const FProperty* InProperty, URigVMMemoryStorage* InMemory)
{
	if (URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(InMemory->GetClass()))
	{
		const int32 PropertyIndex = InMemory->GetPropertyIndex(InProperty);
		const FRigVMOperand Operand(InMemoryType, PropertyIndex);
		const FRigVMPropertyDescription& PropertyDescription = GetPropertyForOperand(Operand);

		FPropertyInfo Info;
		Info.MemoryPropertyIndex = PropertyIndex;
		Info.Description = PropertyDescription;

		check(InMemoryType == ERigVMMemoryType::Literal || InMemoryType == ERigVMMemoryType::Work);
		if(InMemoryType == ERigVMMemoryType::Literal)
		{
			Info.PropertyType = ERigVMNativizedPropertyType::Literal;
		}
		else
		{
			Info.PropertyType = ERigVMNativizedPropertyType::Work;

			// work state may be a hidden / sliced property.
			if(RigVMTypeUtils::IsArrayType(PropertyDescription.CPPType))
			{
				if(const FString* PinPath = OperandToPinMap.Find(Operand))
				{
					if(const URigVMPin* Pin = Model->FindPin(*PinPath))
					{
						if(Pin->GetDirection() == ERigVMPinDirection::Hidden)
						{
							Info.PropertyType = ERigVMNativizedPropertyType::Sliced;
						}
					}
				}
			}
		}

		const int32 LookupIndex = Properties.Add(Info);
		PropertyNameToIndex.Add(Info.Description.Name, LookupIndex);

		// if this property is a nested array, we need to inject out wrapped types
		if(PropertyDescription.Containers.Num() > 1)
		{
			check(PropertyDescription.Containers[0] == EPinContainerType::Array);
			check(PropertyDescription.Containers[1] == EPinContainerType::Array);

			FString BaseElementType = RigVMTypeUtils::BaseTypeFromArrayType(PropertyDescription.CPPType);
			BaseElementType = RigVMTypeUtils::BaseTypeFromArrayType(BaseElementType);
			WrappedArrayCPPTypes.AddUnique(BaseElementType);
			
			const FString ArrayArray = RigVMTypeUtils::ArrayTypeFromBaseType(RigVMTypeUtils::ArrayTypeFromBaseType(BaseElementType));
			const FString MappedType = RigVMTypeUtils::ArrayTypeFromBaseType(GetMappedArrayTypeName(BaseElementType));
			MappedCPPTypes.Add(ArrayArray, FMappedType(MappedType, TEXT(".Array")));
		}
	}
}

void FRigVMCodeGenerator::ParseOperationGroups()
{
	const TArray<FName>& Functions = VM->GetFunctionNames();
	const FRigVMByteCode& ByteCode = VM->GetByteCode();
	const FRigVMInstructionArray Operations = ByteCode.GetInstructions();

	for(int32 EntryIndex = 0; EntryIndex < ByteCode.NumEntries(); EntryIndex++)
	{
		const FRigVMByteCodeEntry& Entry = ByteCode.GetEntry(EntryIndex);

		FOperationGroup Group;
		Group.Entry = Entry.GetSanitizedName();
		Group.Depth = 0;
		Group.First = Entry.InstructionIndex;
		Group.Last = Operations.Num() - 1;

		for(int32 OperationIndex = Group.First+1; OperationIndex < Operations.Num(); OperationIndex++)
		{
			if(Operations[OperationIndex].OpCode == ERigVMOpCode::Exit)
			{
				Group.Last = OperationIndex;
				break;
			}
		}

		OperationGroups.Add(Group);
	}

	for(int32 GroupIndex = 0; GroupIndex < OperationGroups.Num(); GroupIndex++)
	{
		// copy the group here since it will be invalid soon
		FOperationGroup Group = OperationGroups[GroupIndex];
		if(Group.Last - Group.First + 1 > MaxOperationsPerFunction)
		{
			// find all of the constraints / jumps
			TArray<TTuple<int32,int32>> Constraints;
			for(int32 OperationIndex = Group.First; OperationIndex <= Group.Last; OperationIndex++)
			{
				int32 OtherIndex = INDEX_NONE;
				
				const FRigVMInstruction& Operation = Operations[OperationIndex];
				switch(Operation.OpCode)
				{
					case ERigVMOpCode::JumpAbsolute:
					{
						const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
						OtherIndex = Op.InstructionIndex;
						break;
					}
					case ERigVMOpCode::JumpForward:
					{
						const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
						OtherIndex = OperationIndex + Op.InstructionIndex;
						break;
					}
					case ERigVMOpCode::JumpBackward:
					{
						const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
						OtherIndex = OperationIndex - Op.InstructionIndex;
						break;
					}
					case ERigVMOpCode::JumpAbsoluteIf:
					{
						const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
						OtherIndex = OperationIndex;
						break;
					}
					case ERigVMOpCode::JumpForwardIf:
					{
						const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
						OtherIndex = OperationIndex + Op.InstructionIndex;
						break;
					}
					case ERigVMOpCode::JumpBackwardIf:
					{
						const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
						OtherIndex = OperationIndex - Op.InstructionIndex;
						break;
					}
					default:
					{
						break;
					}
				}

				if(OtherIndex != INDEX_NONE)
				{
					Constraints.Add(TTuple<int32,int32>(
						FMath::Min(OperationIndex, OtherIndex),
						FMath::Max(OperationIndex, OtherIndex)));
				}
			}

			const int32 First = Group.First;
			const int32 Last = Group.Last;
			int32 Middle = (Group.Last + Group.First) / 2;
			int32 MiddleHead = Middle;
			int32 MiddleTail = Middle;
			bool bMoveTowardsHead = true;

			auto IsWithinConstraint = [&bMoveTowardsHead, &MiddleHead, &MiddleTail](const TTuple<int32,int32>& Constraint) -> bool
			{
				int32& Middle = bMoveTowardsHead ? MiddleHead : MiddleTail;
				return FMath::IsWithinInclusive(Middle, Constraint.Get<0>(), Constraint.Get<1>());
			};

			// while the middle instruction is within a constraint - move it  
			while(Constraints.FindByPredicate(IsWithinConstraint))
			{
				MiddleHead--;
			}
			bMoveTowardsHead = false;
			while(Constraints.FindByPredicate(IsWithinConstraint))
			{
				MiddleTail++;
			}

			Middle = ((MiddleHead - First) > (Last - MiddleTail)) ? MiddleHead : MiddleTail;
			if(Middle == First || Middle == Last)
			{
				continue;
			}

			FOperationGroup HeadGroup = Group;
			HeadGroup.ParentGroup = GroupIndex;
			HeadGroup.ChildGroups.Reset();
			HeadGroup.First = First;
			HeadGroup.Last = Middle;
			HeadGroup.Depth++;

			FOperationGroup TailGroup = Group;
			TailGroup.ParentGroup = GroupIndex;
			TailGroup.ChildGroups.Reset();
			TailGroup.First = Middle+1;
			TailGroup.Last = Last;
			TailGroup.Depth++;
			
			const int32 HeadGroupIndex = OperationGroups.Add(HeadGroup);
			const int32 TailGroupIndex = OperationGroups.Add(TailGroup);
			OperationGroups[GroupIndex].ChildGroups.Add(HeadGroupIndex);
			OperationGroups[GroupIndex].ChildGroups.Add(TailGroupIndex);
		}
	}

	for(int32 GroupIndex = 0; GroupIndex < OperationGroups.Num(); GroupIndex++)
	{
		FOperationGroup& Group = OperationGroups[GroupIndex];
		
		// determine all the necessary labels and optional arguments
		for(int32 OperationIndex = Group.First; OperationIndex <= Group.Last; OperationIndex++)
		{
			const FRigVMInstruction& Operation = Operations[OperationIndex];
			switch(Operation.OpCode)
			{
				case ERigVMOpCode::Execute_0_Operands:
				case ERigVMOpCode::Execute_1_Operands:
				case ERigVMOpCode::Execute_2_Operands:
				case ERigVMOpCode::Execute_3_Operands:
				case ERigVMOpCode::Execute_4_Operands:
				case ERigVMOpCode::Execute_5_Operands:
				case ERigVMOpCode::Execute_6_Operands:
				case ERigVMOpCode::Execute_7_Operands:
				case ERigVMOpCode::Execute_8_Operands:
				case ERigVMOpCode::Execute_9_Operands:
				case ERigVMOpCode::Execute_10_Operands:
				case ERigVMOpCode::Execute_11_Operands:
				case ERigVMOpCode::Execute_12_Operands:
				case ERigVMOpCode::Execute_13_Operands:
				case ERigVMOpCode::Execute_14_Operands:
				case ERigVMOpCode::Execute_15_Operands:
				case ERigVMOpCode::Execute_16_Operands:
				case ERigVMOpCode::Execute_17_Operands:
				case ERigVMOpCode::Execute_18_Operands:
				case ERigVMOpCode::Execute_19_Operands:
				case ERigVMOpCode::Execute_20_Operands:
				case ERigVMOpCode::Execute_21_Operands:
				case ERigVMOpCode::Execute_22_Operands:
				case ERigVMOpCode::Execute_23_Operands:
				case ERigVMOpCode::Execute_24_Operands:
				case ERigVMOpCode::Execute_25_Operands:
				case ERigVMOpCode::Execute_26_Operands:
				case ERigVMOpCode::Execute_27_Operands:
				case ERigVMOpCode::Execute_28_Operands:
				case ERigVMOpCode::Execute_29_Operands:
				case ERigVMOpCode::Execute_30_Operands:
				case ERigVMOpCode::Execute_31_Operands:
				case ERigVMOpCode::Execute_32_Operands:
				case ERigVMOpCode::Execute_33_Operands:
				case ERigVMOpCode::Execute_34_Operands:
				case ERigVMOpCode::Execute_35_Operands:
				case ERigVMOpCode::Execute_36_Operands:
				case ERigVMOpCode::Execute_37_Operands:
				case ERigVMOpCode::Execute_38_Operands:
				case ERigVMOpCode::Execute_39_Operands:
				case ERigVMOpCode::Execute_40_Operands:
				case ERigVMOpCode::Execute_41_Operands:
				case ERigVMOpCode::Execute_42_Operands:
				case ERigVMOpCode::Execute_43_Operands:
				case ERigVMOpCode::Execute_44_Operands:
				case ERigVMOpCode::Execute_45_Operands:
				case ERigVMOpCode::Execute_46_Operands:
				case ERigVMOpCode::Execute_47_Operands:
				case ERigVMOpCode::Execute_48_Operands:
				case ERigVMOpCode::Execute_49_Operands:
				case ERigVMOpCode::Execute_50_Operands:
				case ERigVMOpCode::Execute_51_Operands:
				case ERigVMOpCode::Execute_52_Operands:
				case ERigVMOpCode::Execute_53_Operands:
				case ERigVMOpCode::Execute_54_Operands:
				case ERigVMOpCode::Execute_55_Operands:
				case ERigVMOpCode::Execute_56_Operands:
				case ERigVMOpCode::Execute_57_Operands:
				case ERigVMOpCode::Execute_58_Operands:
				case ERigVMOpCode::Execute_59_Operands:
				case ERigVMOpCode::Execute_60_Operands:
				case ERigVMOpCode::Execute_61_Operands:
				case ERigVMOpCode::Execute_62_Operands:
				case ERigVMOpCode::Execute_63_Operands:
				case ERigVMOpCode::Execute_64_Operands:
				{
					const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Operation);
					const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*Functions[Op.FunctionIndex].ToString());
					check(Function);

					// make sure to include the required header
					ParseInclude(Function->Struct, Function->GetMethodName());

					// check all of the arguments that may not be part of the struct definition
					for(const FRigVMFunctionArgument& Argument : Function->GetArguments())
					{
						if (Function->IsAdditionalArgument(Argument))
						{
							if(!Group.OpaqueArguments.ContainsByPredicate([Argument](const FOpaqueArgument& OpaqueArgument) -> bool
							{
								return Argument.Name == OpaqueArgument.Get<0>();
							}))
							{
								const FString ArgumentName = Format(RigVM_AdditionalArgumentNameFormat, Group.OpaqueArguments.Num(), Argument.Name);
								FOpaqueArgument OpaqueArgument(Argument.Name, ArgumentName, Argument.Type);
								Group.OpaqueArguments.AddUnique(OpaqueArgument);
							}
						}
					}
					break;
				}
				case ERigVMOpCode::JumpAbsolute:
				{
					const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
					Group.RequiredLabels.AddUnique(Op.InstructionIndex);break;
					break;
				}
				case ERigVMOpCode::JumpForward:
				{
					const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
					Group.RequiredLabels.AddUnique(OperationIndex + Op.InstructionIndex);break;
					break;
				}
				case ERigVMOpCode::JumpBackward:
				{
					const FRigVMJumpOp& Op = ByteCode.GetOpAt<FRigVMJumpOp>(Operation);
					Group.RequiredLabels.AddUnique(OperationIndex - Op.InstructionIndex);break;
					break;
				}
				case ERigVMOpCode::JumpAbsoluteIf:
				{
					const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
					Group.RequiredLabels.AddUnique(Op.InstructionIndex);break;
					break;
				}
				case ERigVMOpCode::JumpForwardIf:
				{
					const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
					Group.RequiredLabels.AddUnique(OperationIndex + Op.InstructionIndex);break;
					break;
				}
				case ERigVMOpCode::JumpBackwardIf:
				{
					const FRigVMJumpIfOp& Op = ByteCode.GetOpAt<FRigVMJumpIfOp>(Operation);
					Group.RequiredLabels.AddUnique(OperationIndex - Op.InstructionIndex);break;
					break;
				}
				default:
				{
					break;
				}
			}

			// update the usage table to know which property is used where
			FRigVMOperandArray Operands = ByteCode.GetOperandsForOp(Operation);
			for(const FRigVMOperand& Operand : Operands)
			{
				if(Operand.GetMemoryType() == ERigVMMemoryType::Literal ||
					Operand.GetMemoryType() == ERigVMMemoryType::Work)
				{
					const int32 PropertyIndex = GetPropertyIndex(Operand);
					Properties[PropertyIndex].Groups.AddUnique(GroupIndex);
				}
			}
		}
	}

	// now that we know the groups that properties belong to, we need to filter them.
	// we only want to declare properties once - and pass them as parameters to the enclosed
	// groups. parent groups which only pass them to one sub group can be removed from the
	// property's group table. the top level group which still contains the property is the
	// one to declare it.
	const TArray<FOperationGroup>& Groups = OperationGroups;
	for(int32 PropertyIndex = 0; PropertyIndex < Properties.Num(); PropertyIndex++)
	{
		// work / sliced properties are always defined at the top
		FPropertyInfo& Property = Properties[PropertyIndex];

		if(Property.PropertyType == ERigVMNativizedPropertyType::Sliced)
		{
			Property.Groups.Reset();
			continue;
		}
		
		int32 GroupCount;
		do
		{
			GroupCount = Property.Groups.Num();
			TArray<int32> FilteredGroups = Property.Groups;
			FilteredGroups.RemoveAll([Groups, Property](int32 Group) -> bool
			{
				return !Groups[Group].ChildGroups.IsEmpty();
			});
			Property.Groups = FilteredGroups;
			
		} while(GroupCount != Property.Groups.Num());
	}
}

FString FRigVMCodeGenerator::GetOperandName(const FRigVMOperand& InOperand, bool bPerSlice, bool bAsInput) const
{
	const FRigVMPropertyDescription& Property = GetPropertyForOperand(InOperand);
	const FRigVMPropertyPathDescription& PropertyPath = GetPropertyPathForOperand(InOperand);

	FString OperandName;
	if (Property.IsValid())
	{
		OperandName = Property.Name.ToString();
	}

	if (InOperand.GetMemoryType() != ERigVMMemoryType::External)
	{
		OperandName = SanitizeName(OperandName, Property.CPPType);
	}

	// if we are an array on work memory this indicates that we'll be sliced.
	if (bPerSlice && InOperand.GetMemoryType() == ERigVMMemoryType::Work && RigVMTypeUtils::IsArrayType(Property.CPPType))
	{
		const FString MappedType = GetMappedType(Property.CPPType);
		const FString MappedTypeSuffix = GetMappedTypeSuffix(Property.CPPType);
		const FString BaseCPPType = RigVMTypeUtils::IsArrayType(MappedType) ? RigVMTypeUtils::BaseTypeFromArrayType(MappedType) : MappedType;
		OperandName = Format(RigVM_GetOperandSliceFormat, *BaseCPPType, *OperandName, *MappedTypeSuffix);
	}

	if (InOperand.GetMemoryType() == ERigVMMemoryType::External)
	{
		OperandName = Format(RigVM_ExternalVariableFormat, *OperandName);
	}

	if (PropertyPath.IsValid())
	{
		check(Property.IsValid());

		FString RemainingSegmentPath = PropertyPath.SegmentPath;
		FString SegmentPath = OperandName;

		static TMap<UScriptStruct*, TMap<FString, TTuple<FString, FString>>> MappedProperties;
		if (MappedProperties.IsEmpty())
		{
			TMap<FString, TTuple<FString, FString>>& TransformMap = MappedProperties.Add(TBaseStructure<FTransform>::Get());
			TransformMap.Add(TEXT("Translation"), TTuple<FString, FString>(TEXT("{0}.GetTranslation()"), TEXT("{0}.SetTranslation(")));
			TransformMap.Add(TEXT("Translation.X"), TTuple<FString, FString>(TEXT("{0}.GetTranslation().X"), TEXT("FTransformSetter({0}).SetTranslationX(")));
			TransformMap.Add(TEXT("Translation.Y"), TTuple<FString, FString>(TEXT("{0}.GetTranslation().Y"), TEXT("FTransformSetter({0}).SetTranslationY(")));
			TransformMap.Add(TEXT("Translation.Z"), TTuple<FString, FString>(TEXT("{0}.GetTranslation().Z"), TEXT("FTransformSetter({0}).SetTranslationZ(")));
			TransformMap.Add(TEXT("Rotation"), TTuple<FString, FString>(TEXT("{0}.GetRotation()"), TEXT("{0}.SetRotation(")));
			TransformMap.Add(TEXT("Rotation.X"), TTuple<FString, FString>(TEXT("{0}.GetRotation().X"), TEXT("FTransformSetter({0}).SetRotationX(")));
			TransformMap.Add(TEXT("Rotation.Y"), TTuple<FString, FString>(TEXT("{0}.GetRotation().Y"), TEXT("FTransformSetter({0}).SetRotationY(")));
			TransformMap.Add(TEXT("Rotation.Z"), TTuple<FString, FString>(TEXT("{0}.GetRotation().Z"), TEXT("FTransformSetter({0}).SetRotationZ(")));
			TransformMap.Add(TEXT("Rotation.W"), TTuple<FString, FString>(TEXT("{0}.GetRotation().W"), TEXT("FTransformSetter({0}).SetRotationW(")));
			TransformMap.Add(TEXT("Scale3D"), TTuple<FString, FString>(TEXT("{0}.GetScale3D()"), TEXT("{0}.SetScale3D(")));
			TransformMap.Add(TEXT("Scale3D.X"), TTuple<FString, FString>(TEXT("{0}.GetScale3D().X"), TEXT("FTransformSetter({0}).SetScaleX(")));
			TransformMap.Add(TEXT("Scale3D.Y"), TTuple<FString, FString>(TEXT("{0}.GetScale3D().Y"), TEXT("FTransformSetter({0}).SetScaleY(")));
			TransformMap.Add(TEXT("Scale3D.Z"), TTuple<FString, FString>(TEXT("{0}.GetScale3D().Z"), TEXT("FTransformSetter({0}).SetScaleZ(")));
		}

		const FProperty* CurrentProperty = Property.Property;

		while(!RemainingSegmentPath.IsEmpty() && (CurrentProperty != nullptr))
		{
			FString Left, Right;
			if(!URigVMPin::SplitPinPathAtStart(RemainingSegmentPath, Left, Right))
			{
				Left = RemainingSegmentPath;
				Right.Reset();
			}
			
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty))
			{
				CurrentProperty = StructProperty->Struct->FindPropertyByName(*Left);
				
				if (const TMap<FString, TTuple<FString, FString>>* PropertyMap = MappedProperties.Find(StructProperty->Struct))
				{
					if (const TTuple<FString, FString>* NewSegmentName = PropertyMap->Find(RemainingSegmentPath))
					{
						FStringFormatOrderedArguments FormatArguments;
						FormatArguments.Add(SegmentPath);

						SegmentPath = FString::Format(
							bAsInput ? *NewSegmentName->Get<0>() : *NewSegmentName->Get<1>(),
							FormatArguments);
						break;
					}
				}
			}
			else if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
			{
				CurrentProperty = ArrayProperty->Inner;

				Left = Left.TrimChar(TEXT('['));
				Left = Left.TrimChar(TEXT(']'));

				FString ExtendedType;
				FString CPPType = CurrentProperty->GetCPPType(&ExtendedType);
				CPPType += ExtendedType;
				if(CPPType == RigVMTypeUtils::UInt8Type && CastField<FBoolProperty>(CurrentProperty))
				{
					CPPType = RigVMTypeUtils::BoolType;
				}

				SegmentPath = Format(RigVM_GetArrayElementSafeFormat, *CPPType, *SegmentPath, *Left);
				RemainingSegmentPath = Right;
				continue;
			}
			else
			{
				CurrentProperty = nullptr;
			}

			SegmentPath = Format(RigVM_JoinSegmentPathFormat, *SegmentPath, *Left);
			RemainingSegmentPath = Right;
		}

		return SegmentPath;
	}

	return OperandName;
}

FString FRigVMCodeGenerator::GetOperandCPPType(const FRigVMOperand& InOperand) const
{
	const FRigVMPropertyDescription& Property = GetPropertyForOperand(InOperand);
	const FRigVMPropertyPathDescription& PropertyPath = GetPropertyPathForOperand(InOperand);

	if (PropertyPath.IsValid())
	{
		check(Property.IsValid());

		const FRigVMPropertyPath Path(Property.Property, PropertyPath.SegmentPath);
		const FProperty* TailProperty = Path.GetTailProperty();
		check(TailProperty);

		FString ExtendedType;
		const FString CPPType = TailProperty->GetCPPType(&ExtendedType);
		return CPPType + ExtendedType;
	}
	
	if (Property.IsValid())
	{
		return Property.CPPType;
	}

	return FString(); 
}

FString FRigVMCodeGenerator::GetOperandCPPBaseType(const FRigVMOperand& InOperand) const
{
	FString CPPType = GetOperandCPPType(InOperand);
	while(RigVMTypeUtils::IsArrayType(CPPType))
	{
		CPPType = RigVMTypeUtils::BaseTypeFromArrayType(CPPType);
	}
	return CPPType;
}


FString FRigVMCodeGenerator::SanitizeName(const FString& InName, const FString& CPPType)
{
	FString Name = InName;
	while(Name.Contains(RigVM_DoubleUnderscoreFormat))
	{
		Name.ReplaceInline(RigVM_DoubleUnderscoreFormat, RigVM_SingleUnderscoreFormat);
	}

	if(CPPType == RigVMTypeUtils::BoolType && !Name.StartsWith(RigVM_BoolPropertyPrefix, ESearchCase::CaseSensitive))
	{
		Name = RigVM_BoolPropertyPrefix + Name;
	}

	return Name;
}

FRigVMPropertyDescription FRigVMCodeGenerator::GetPropertyForOperand(const FRigVMOperand& InOperand) const
{
	CheckOperand(InOperand);

	const FProperty* Property = nullptr;
	const uint8* Memory = nullptr;

	if (InOperand.GetMemoryType() == ERigVMMemoryType::External)
	{
		const FRigVMExternalVariable& ExternalVariable = VM->ExternalVariables[InOperand.GetRegisterIndex()];
		Property = ExternalVariable.Property;
		Memory = ExternalVariable.Memory;
	}
	else
	{
		if (URigVMMemoryStorage* MemoryStorage = VM->GetMemoryByType(InOperand.GetMemoryType()))
		{
			Property = MemoryStorage->GetProperty(InOperand.GetRegisterIndex());
			if (Property)
			{
				Memory = Property->ContainerPtrToValuePtr<uint8>(MemoryStorage);
			}
		}
	}

	if (Property)
	{
		FString DefaultValue;
		
		if (const UClass* OwningClass = Property->GetOwnerClass())
		{
			if (const UObject* CDO = OwningClass->GetDefaultObject())
			{
				Memory = Property->ContainerPtrToValuePtr<uint8>(CDO);
			}
		}

		if (Memory)
		{
			DefaultValue = FRigVMStruct::ExportToFullyQualifiedText(Property, Memory, true);
		}
		
		return FRigVMPropertyDescription(Property, DefaultValue);
	}

	return FRigVMPropertyDescription();
}

const FRigVMPropertyPathDescription& FRigVMCodeGenerator::GetPropertyPathForOperand(const FRigVMOperand& InOperand) const
{
	CheckOperand(InOperand);

	const int32 RegisterOffsetIndex = InOperand.GetRegisterOffset();
	if (RegisterOffsetIndex != INDEX_NONE)
	{
		if (InOperand.GetMemoryType() == ERigVMMemoryType::External)
		{
			if (VM->ExternalPropertyPathDescriptions.IsValidIndex(RegisterOffsetIndex))
			{
				return VM->ExternalPropertyPathDescriptions[RegisterOffsetIndex];
			}
		}
		else
		{
			if (const URigVMMemoryStorage* MemoryStorage = VM->GetMemoryByType(InOperand.GetMemoryType()))
			{
				if (const URigVMMemoryStorageGeneratorClass* MemoryClass = Cast<URigVMMemoryStorageGeneratorClass>(MemoryStorage->GetClass()))
				{
					if (MemoryClass->PropertyPathDescriptions.IsValidIndex(RegisterOffsetIndex))
					{
						return MemoryClass->PropertyPathDescriptions[RegisterOffsetIndex];
					}
				}
			}
		}
	}

	static const FRigVMPropertyPathDescription EmptyPropertyPath;
	return EmptyPropertyPath;
}

int32 FRigVMCodeGenerator::GetPropertyIndex(const FRigVMPropertyDescription& InProperty) const
{
	if(const int32* Index = PropertyNameToIndex.Find(InProperty.Name))
	{
		return *Index;
	}
	return INDEX_NONE;
}

int32 FRigVMCodeGenerator::GetPropertyIndex(const FRigVMOperand& InOperand) const
{
	return GetPropertyIndex(GetPropertyForOperand(InOperand));
}

FRigVMCodeGenerator::ERigVMNativizedPropertyType FRigVMCodeGenerator::GetPropertyType(
	const FRigVMPropertyDescription& InProperty) const
{
	const int32 PropertyIndex = GetPropertyIndex(InProperty);
	if(Properties.IsValidIndex(PropertyIndex))
	{
		return Properties[PropertyIndex].PropertyType;
	}
	return ERigVMNativizedPropertyType::Invalid;
}

FRigVMCodeGenerator::ERigVMNativizedPropertyType FRigVMCodeGenerator::GetPropertyType(const FRigVMOperand& InOperand) const
{
	return GetPropertyType(GetPropertyForOperand(InOperand));
}

void FRigVMCodeGenerator::CheckOperand(const FRigVMOperand& InOperand) const
{
	ensure(InOperand.IsValid());
	ensure(InOperand.GetMemoryType() != ERigVMMemoryType::Invalid);
	ensure(InOperand.GetMemoryType() != ERigVMMemoryType::Debug);

	if (InOperand.GetMemoryType() == ERigVMMemoryType::External)
	{
		ensure(VM->ExternalVariables.IsValidIndex(InOperand.GetRegisterIndex()));
		if (InOperand.GetRegisterOffset() != INDEX_NONE)
		{
			ensure(VM->ExternalPropertyPaths.IsValidIndex(InOperand.GetRegisterOffset()));
			ensure(VM->ExternalPropertyPathDescriptions.IsValidIndex(InOperand.GetRegisterOffset()));
		}
	}
	else
	{
		URigVMMemoryStorage* Memory = VM->GetMemoryByType(InOperand.GetMemoryType(), false);
		check(Memory);

		ensure(Memory->GetProperties().IsValidIndex(InOperand.GetRegisterIndex()));
		if (InOperand.GetRegisterOffset() != INDEX_NONE)
		{
			ensure(Memory->GetPropertyPaths().IsValidIndex(InOperand.GetRegisterOffset()));
			URigVMMemoryStorageGeneratorClass* Class = CastChecked<URigVMMemoryStorageGeneratorClass>(Memory->GetClass()); 
			ensure(Class->PropertyPathDescriptions.IsValidIndex(InOperand.GetRegisterOffset()));
		}
	}
}

FString FRigVMCodeGenerator::GetMappedType(const FString& InCPPType) const
{
	FString CPPType = InCPPType;
	CPPType.RemoveSpacesInline();
	
	if(const FMappedType* MappedType = MappedCPPTypes.Find(CPPType))
	{
		return MappedType->Get<0>();
	}

	if(RigVMTypeUtils::IsArrayType(CPPType))
	{
		const FString BaseType = RigVMTypeUtils::BaseTypeFromArrayType(CPPType);
		return RigVMTypeUtils::ArrayTypeFromBaseType(GetMappedType(BaseType));
	}

	return CPPType;
}

const FString& FRigVMCodeGenerator::GetMappedTypeSuffix(const FString& InCPPType) const
{
	FString CPPType = InCPPType;
	CPPType.RemoveSpacesInline();
	
	if(const FMappedType* MappedType = MappedCPPTypes.Find(CPPType))
	{
		return MappedType->Get<1>();
	}

	static const FString EmptyString;
	return EmptyString;
}

FString FRigVMCodeGenerator::GetMappedArrayTypeName(const FString InBaseElementType) const
{
	return Format(RigVM_WrappedTypeNameFormat, *InBaseElementType, *ClassName);
}

const FRigVMCodeGenerator::FOperationGroup& FRigVMCodeGenerator::GetGroup(int32 InGroupIndex) const
{
	if(OperationGroups.IsValidIndex(InGroupIndex))
	{
		return OperationGroups[InGroupIndex];
	}
	
	static FOperationGroup CompleteGroup;
	if(CompleteGroup.First == INDEX_NONE)
	{
		CompleteGroup.First = 0;
		CompleteGroup.Last = VM->GetByteCode().GetNumInstructions() - 1; 
	}
	return CompleteGroup;
}

bool FRigVMCodeGenerator::IsOperationPartOfGroup(int32 InOperationIndex, int32 InGroupIndex, bool bIncludeChildGroups) const
{
	if(!OperationGroups.IsValidIndex(InGroupIndex) && OperationGroups.IsEmpty())
	{
		return true;
	}

	const FOperationGroup& Group = OperationGroups[InGroupIndex];
	for(const int32 ChildGroupIndex : Group.ChildGroups)
	{
		if(IsOperationPartOfGroup(InOperationIndex, ChildGroupIndex, true))
		{
			// if we found the operation in a child group
			// we'll return true if we are also looking within the child groups,
			// and false if we are not. this means that an operation that's part of a
			// child group but not the main group will return false here
			return bIncludeChildGroups;
		}
	}

	return FMath::IsWithinInclusive(InOperationIndex, Group.First, Group.Last);;
}

bool FRigVMCodeGenerator::IsPropertyPartOfGroup(int32 InPropertyIndex, int32 InGroupIndex) const
{
	if(InGroupIndex == INDEX_NONE)
	{
		return true;
	}

	if(!OperationGroups.IsValidIndex(InGroupIndex))
	{
		return false;
	}

	if(Properties.IsValidIndex(InPropertyIndex))
	{
		const FPropertyInfo& Property = Properties[InPropertyIndex];
		const FOperationGroup& Group = OperationGroups[InGroupIndex];

		// properties are only defines in the leaf groups
		if(!Group.ChildGroups.IsEmpty())
		{
			return false;
		}

		if(Property.PropertyType == ERigVMNativizedPropertyType::Work)
		{
			if(Property.Groups.Num() > 1)
			{
				return false;
			}
		}

		return Property.Groups.Contains(InGroupIndex);
	}
	
	return false;
}

const TMap<FName, FRigVMCodeGenerator::TStructConstGenerator>& FRigVMCodeGenerator::GetStructConstGenerators()
{
	static TMap<FName, FRigVMCodeGenerator::TStructConstGenerator> StructConstGenerators;
	if(!StructConstGenerators.IsEmpty())
	{
		return StructConstGenerators;
	}

	struct DefaultValueHelpers
	{
		static FStringArray SplitIntoArray(const FString& InValue)
		{
			return URigVMPin::SplitDefaultValue(InValue);
		}
		
		static FStringMap SplitIntoMap(const FString& InValue)
		{
			FStringArray Pairs = SplitIntoArray(InValue);
			FStringMap Map;
			for(const FString& Pair : Pairs)
			{
				FString Key, Value;
				if(Pair.Split(TEXT("="), &Key, &Value))
				{
					Map.Add(Key, Value);
				}
			}
			return Map;
		}

		static FString RemoveQuotes(const FString& InValue)
		{
			FString Value = InValue;
			if(Value.StartsWith(TEXT("\\\"")))
			{
				Value = Value.Mid(2, Value.Len()  - 4);
			}
			else
			{
				Value = Value.TrimQuotes();
			}
			return Value;
		}
	};

	static constexpr TCHAR X[] = TEXT("X");
	static constexpr TCHAR Y[] = TEXT("Y");
	static constexpr TCHAR Z[] = TEXT("Z");
	static constexpr TCHAR W[] = TEXT("W");
	static constexpr TCHAR R[] = TEXT("R");
	static constexpr TCHAR G[] = TEXT("G");
	static constexpr TCHAR B[] = TEXT("B");
	static constexpr TCHAR A[] = TEXT("A");
	static constexpr TCHAR Translation[] = TEXT("Translation");
	static constexpr TCHAR Rotation[] = TEXT("Rotation");
	static constexpr TCHAR Scale[] = TEXT("Scale");
	static constexpr TCHAR Scale3D[] = TEXT("Scale3D");
	static constexpr TCHAR Pitch[] = TEXT("Pitch");
	static constexpr TCHAR Yaw[] = TEXT("Yaw");
	static constexpr TCHAR Roll[] = TEXT("Roll");
	static constexpr TCHAR Type[] = TEXT("Type");
	static constexpr TCHAR Name[] = TEXT("Name");

	StructConstGenerators.Add(TEXT("FVector2D"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FVector2D({0}, {1})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(X) && Defaults.Contains(Y))
		{
			return Format(Constructor, Defaults.FindChecked(X), Defaults.FindChecked(Y));
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FVector"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FVector({0}, {1}, {2})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(X) && Defaults.Contains(Y) && Defaults.Contains(Z))
		{
			return Format(Constructor, Defaults.FindChecked(X), Defaults.FindChecked(Y), Defaults.FindChecked(Z));
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FVector4"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FVector4({0}, {1}, {2}, {3})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(X) && Defaults.Contains(Y) && Defaults.Contains(Z) && Defaults.Contains(W))
		{
			return Format(Constructor, Defaults.FindChecked(X), Defaults.FindChecked(Y), Defaults.FindChecked(Z), Defaults.FindChecked(W));
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FLinearColor"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FLinearColor({0}, {1}, {2}, {3})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(R) && Defaults.Contains(G) && Defaults.Contains(B) && Defaults.Contains(A))
		{
			return Format(Constructor, Defaults.FindChecked(R), Defaults.FindChecked(G), Defaults.FindChecked(B), Defaults.FindChecked(A));
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FQuat"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FQuat({0}, {1}, {2}, {3})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(X) && Defaults.Contains(Y) && Defaults.Contains(Z) && Defaults.Contains(W))
		{
			return Format(Constructor, Defaults.FindChecked(X), Defaults.FindChecked(Y), Defaults.FindChecked(Z), Defaults.FindChecked(W));
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FRotator"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FRotator({0}, {1}, {2})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(Pitch) && Defaults.Contains(Yaw) && Defaults.Contains(Roll))
		{
			return Format(Constructor, Defaults.FindChecked(Pitch), Defaults.FindChecked(Yaw), Defaults.FindChecked(Roll));
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FTransform"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FTransform({0}, {1}, {2})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(Rotation) && Defaults.Contains(Translation) && Defaults.Contains(Scale3D))
		{
			const FString RotationValue = StructConstGenerators.FindChecked(TEXT("FQuat"))(Defaults.FindChecked(Rotation));
			const FString TranslationValue = StructConstGenerators.FindChecked(TEXT("FVector"))(Defaults.FindChecked(Translation));
			const FString Scale3DValue = StructConstGenerators.FindChecked(TEXT("FVector"))(Defaults.FindChecked(Scale3D));
			return Format(Constructor, RotationValue, TranslationValue, Scale3DValue);
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FEulerTransform"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FEulerTransform({0}, {1}, {2})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(Rotation) && Defaults.Contains(Translation) && Defaults.Contains(Scale3D))
		{
			const FString RotationValue = StructConstGenerators.FindChecked(TEXT("FRotator"))(Defaults.FindChecked(Rotation));
			const FString TranslationValue = StructConstGenerators.FindChecked(TEXT("FVector"))(Defaults.FindChecked(Translation));
			const FString Scale3DValue = StructConstGenerators.FindChecked(TEXT("FVector"))(Defaults.FindChecked(Scale3D));
			return Format(Constructor, RotationValue, TranslationValue, Scale3DValue);
		}
		return FString();
	});

	StructConstGenerators.Add(TEXT("FRigElementKey"), [](const FString& InDefault) -> FString
	{
		static constexpr TCHAR Constructor[] = TEXT("FRigElementKey(TEXT(\"{0}\"), ERigElementType::{1})");
		const FStringMap Defaults = DefaultValueHelpers::SplitIntoMap(InDefault);
		if(Defaults.Contains(Type) && Defaults.Contains(Name))
		{
			const FString NameValue = DefaultValueHelpers::RemoveQuotes(Defaults.FindChecked(Name));
			return Format(Constructor, NameValue, Defaults.FindChecked(Type));
		}
		return FString();
	});

	return StructConstGenerators;
}
