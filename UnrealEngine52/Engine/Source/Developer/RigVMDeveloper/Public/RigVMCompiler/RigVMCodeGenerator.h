// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVM.h"
#include "RigVMModel/RigVMGraph.h"

#include "UObject/StrongObjectPtr.h"

class URigVMGraph;


struct RIGVMDEVELOPER_API FRigVMCodeGenerator
{
public:

	FRigVMCodeGenerator(const FString& InClassName, const FString& InModuleName,
		URigVMGraph* InModelToNativize, URigVM* InVMToNativize, TMap<FString,FRigVMOperand> InPinToOperandMap,
		int32 InMaxInstructionsPerFunction = 100)
	{
		ParseVM(InClassName, InModuleName, InModelToNativize, InVMToNativize, InPinToOperandMap, InMaxInstructionsPerFunction);
	}

	FString DumpIncludes(bool bLog = false);
	FString DumpExternalVariables(bool bForHeader, bool bLog = false);
	FString DumpEntries(bool bForHeader, bool bLog = false);
	FString DumpBlockNames(bool bForHeader, bool bLog = false);
	FString DumpProperties(bool bForHeader, int32 InInstructionGroup, bool bLog = false);
	FString DumpDispatches(bool bLog = false);
	FString DumpRequiredUProperties(bool bLog = false);
	FString DumpInitialize(bool bLog = false);
	FString DumpInstructions(int32 InInstructionGroup, bool bLog = false);
	FString DumpHeader(bool bLog = false);
	FString DumpSource(bool bLog = false);
	FString DumpLines(const TArray<FString>& InLines, bool bLog = false);
	
private:

	typedef TArray<FString> FStringArray;
	typedef TMap<FString, FString> FStringMap;
	typedef TArray<FRigVMPropertyDescription> FRigVMPropertyDescriptionArray;
	typedef TTuple<FString, FString> FMappedType;
	typedef TFunction<FString(const FString&)> TStructConstGenerator;

	enum ERigVMNativizedPropertyType
	{
		Literal,
		Work,
		Sliced,
		Invalid
	};

	struct FPropertyInfo
	{
		FRigVMPropertyDescription Description;
		int32 MemoryPropertyIndex;
		ERigVMNativizedPropertyType PropertyType;
		TArray<int32> Groups;
	};

	struct FInstructionGroup
	{
		FString Entry;
		int32 First;
		int32 Last;
		int32 Depth;
		int32 ParentGroup;
		TArray<int32> ChildGroups;
		TArray<int32> RequiredLabels;

		FInstructionGroup()
			: Entry()
			, First(INDEX_NONE)
			, Last(INDEX_NONE)
			, Depth(INDEX_NONE)
			, ParentGroup(INDEX_NONE)
		{}

		FInstructionGroup(const FInstructionGroup& InOther)
			: Entry(InOther.Entry)
			, First(InOther.First)
			, Last(InOther.Last)
			, Depth(InOther.Depth)
			, ParentGroup(InOther.ParentGroup)
			, ChildGroups(InOther.ChildGroups)
			, RequiredLabels(InOther.RequiredLabels)
		{}
	};

	void Reset();
	void ParseVM(const FString& InClassName, const FString& InModuleName,
		URigVMGraph* InModelToNativize, URigVM* InVMToNativize,
		TMap<FString,FRigVMOperand> InPinToOperandMap, int32 InMaxInstructionsPerFunction);
	void ParseInclude(UStruct* InDependency, const FName& InMethodName = NAME_None);
	void ParseRequiredUProperties();
	void ParseMemory(URigVMMemoryStorage* InMemory);
	void ParseProperty(ERigVMMemoryType InMemoryType, const FProperty* InProperty, URigVMMemoryStorage* InMemory);
	void ParseInstructionGroups();
	FString DumpInstructions(const FString& InPrefix, int32 InFirstInstruction, int32 InLastInstruction, const FInstructionGroup& InGroup, bool bLog = false);
	FString DumpInstructions(const FString& InPrefix, const TArray<int32> InInstructionIndices, const FInstructionGroup& InGroup, bool bLog = false);
	FString GetOperandName(const FRigVMOperand& InOperand, bool bPerSlice = true, bool bAsInput = true) const;
	FString GetOperandCPPType(const FRigVMOperand& InOperand) const;
	FString GetOperandCPPBaseType(const FRigVMOperand& InOperand) const;
	static FString SanitizeName(const FString& InName, const FString& CPPType = FString()); 
	static FString SanitizeValue(const FString& InValue, const FString& CPPType, const UObject* CPPTypeObject); 
	FRigVMPropertyDescription GetPropertyForOperand(const FRigVMOperand& InOperand) const;
	const FRigVMPropertyPathDescription& GetPropertyPathForOperand(const FRigVMOperand& InOperand) const;
	int32 GetPropertyIndex(const FRigVMPropertyDescription& InProperty) const;
	int32 GetPropertyIndex(const FRigVMOperand& InOperand) const;
	ERigVMNativizedPropertyType GetPropertyType(const FRigVMPropertyDescription& InProperty) const;
	ERigVMNativizedPropertyType GetPropertyType(const FRigVMOperand& InOperand) const;
	void CheckOperand(const FRigVMOperand& InOperand) const;
	FString GetMappedType(const FString& InCPPType) const;
	const FString& GetMappedTypeSuffix(const FString& InCPPType) const;
	FString GetMappedArrayTypeName(const FString InBaseElementType) const;
	const FInstructionGroup& GetGroup(int32 InGroupIndex) const;
	bool IsInstructionPartOfGroup(int32 InInstructionIndex, int32 InGroupIndex, bool bIncludeChildGroups) const;
	bool IsPropertyPartOfGroup(int32 InPropertyIndex, int32 InGroupIndex) const;
	FString GetEntryParameters() const;
	static TArray<int32> GetInstructionIndicesFromRange(int32 First, int32 Last);

	static FString FormatArgs(const TCHAR* InFormatString, const FStringFormatOrderedArguments& InArgs)
	{
		return FString::Format(InFormatString, InArgs);
	}

	template<typename TypeA>
	static FString Format(const TCHAR* InFormatString, const TypeA& InArgA)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA)
		});
	}

	template<typename TypeA, typename TypeB>
	static FString Format(const TCHAR* InFormatString, const TypeA& InArgA, const TypeB& InArgB)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB)
		});
	}

	template<typename TypeA, typename TypeB, typename TypeC>
	static FString Format(const TCHAR* InFormatString, const TypeA& InArgA, const TypeB& InArgB, const TypeC& InArgC)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB),
			FStringFormatArg(InArgC)
		});
	}

	template<typename TypeA, typename TypeB, typename TypeC, typename TypeD>
	static FString Format(const TCHAR* InFormatString,
		const TypeA& InArgA,
		const TypeB& InArgB,
		const TypeC& InArgC,
		const TypeD& InArgD
	)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB),
			FStringFormatArg(InArgC),
			FStringFormatArg(InArgD)
		});
	}

	template<typename TypeA, typename TypeB, typename TypeC, typename TypeD, typename TypeE>
	static FString Format(const TCHAR* InFormatString,
		const TypeA& InArgA,
		const TypeB& InArgB,
		const TypeC& InArgC,
		const TypeD& InArgD,
		const TypeE& InArgE
	)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB),
			FStringFormatArg(InArgC),
			FStringFormatArg(InArgD),
			FStringFormatArg(InArgE)
		});
	}

	template<typename TypeA, typename TypeB, typename TypeC, typename TypeD, typename TypeE, typename TypeF>
	static FString Format(const TCHAR* InFormatString,
		const TypeA& InArgA,
		const TypeB& InArgB,
		const TypeC& InArgC,
		const TypeD& InArgD,
		const TypeE& InArgE,
		const TypeF& InArgF
	)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB),
			FStringFormatArg(InArgC),
			FStringFormatArg(InArgD),
			FStringFormatArg(InArgE),
			FStringFormatArg(InArgF)
		});
	}

	template<typename TypeA, typename TypeB, typename TypeC, typename TypeD, typename TypeE, typename TypeF, typename TypeG>
	static FString Format(const TCHAR* InFormatString,
		const TypeA& InArgA,
		const TypeB& InArgB,
		const TypeC& InArgC,
		const TypeD& InArgD,
		const TypeE& InArgE,
		const TypeF& InArgF,
		const TypeG& InArgG
	)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB),
			FStringFormatArg(InArgC),
			FStringFormatArg(InArgD),
			FStringFormatArg(InArgE),
			FStringFormatArg(InArgF),
			FStringFormatArg(InArgG)
		});
	}

	TStrongObjectPtr<URigVMGraph> Model;
	TStrongObjectPtr<URigVM> VM;
	TMap<FString,FRigVMOperand> PinToOperandMap;
	TMap<FRigVMOperand, FString> OperandToPinMap;
	int32 MaxInstructionsPerFunction;
	FString ClassName;
	FString ModuleName;
	FString ExecuteContextType;
	
	FStringArray Libraries;
	FStringArray Includes;

	struct FRigVMDispatchInfo
	{
		FString Name;
		const FRigVMFunction* Function;
		FRigVMDispatchContext Context;
	};
	
	TMap<FString, FRigVMDispatchInfo> Dispatches;
	TMap<TRigVMTypeIndex,TTuple<FString,FString>> RequiredUProperties;
	TArray<FInstructionGroup> InstructionGroups;
	TMap<FString, FMappedType> MappedCPPTypes;
	TArray<FPropertyInfo> Properties;
	TMap<FName, int32> PropertyNameToIndex;
	TMap<FName, FName> MappedPropertyNames;
	TMap<FString, FString> OverriddenOperatorNames;

	static const TMap<FName, TStructConstGenerator>& GetStructConstGenerators();
};
