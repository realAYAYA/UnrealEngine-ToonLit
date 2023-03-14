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
		int32 InMaxOperationsPerFunction = 100)
	{
		ParseVM(InClassName, InModuleName, InModelToNativize, InVMToNativize, InPinToOperandMap, InMaxOperationsPerFunction);
	}

	FString DumpIncludes(bool bLog = false);
	FString DumpWrappedArrayTypes(bool bLog = false);
	FString DumpExternalVariables(bool bForHeader, bool bLog = false);
	FString DumpEntries(bool bForHeader, bool bLog = false);
	FString DumpProperties(bool bForHeader, int32 InOperationGroup, bool bLog = false);
	FString DumpInstructions(int32 InOperationGroup, bool bLog = false);
	FString DumpHeader(bool bLog = false);
	FString DumpSource(bool bLog = false);
	FString DumpLines(const TArray<FString>& InLines, bool bLog = false);
	
private:

	typedef TArray<FString> FStringArray;
	typedef TMap<FString, FString> FStringMap;
	typedef TArray<FRigVMPropertyDescription> FRigVMPropertyDescriptionArray;
	typedef TTuple<FString, FString> FMappedType;
	typedef TTuple<FString, FString, FString> FOpaqueArgument;
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

	struct FOperationGroup
	{
		FString Entry;
		int32 First;
		int32 Last;
		int32 Depth;
		int32 ParentGroup;
		TArray<int32> ChildGroups;
		TArray<int32> RequiredLabels;
		TArray<FOpaqueArgument> OpaqueArguments;

		FOperationGroup()
			: Entry()
			, First(INDEX_NONE)
			, Last(INDEX_NONE)
			, Depth(INDEX_NONE)
			, ParentGroup(INDEX_NONE)
		{}

		FOperationGroup(const FOperationGroup& InOther)
			: Entry(InOther.Entry)
			, First(InOther.First)
			, Last(InOther.Last)
			, Depth(InOther.Depth)
			, ParentGroup(InOther.ParentGroup)
			, ChildGroups(InOther.ChildGroups)
			, RequiredLabels(InOther.RequiredLabels)
			, OpaqueArguments(InOther.OpaqueArguments)
		{}
	};

	void Reset();
	void ParseVM(const FString& InClassName, const FString& InModuleName,
		URigVMGraph* InModelToNativize, URigVM* InVMToNativize,
		TMap<FString,FRigVMOperand> InPinToOperandMap, int32 InMaxOperationsPerFunction);
	void ParseInclude(UStruct* InDependency, const FName& InMethodName = NAME_None);
	void ParseMemory(URigVMMemoryStorage* InMemory);
	void ParseProperty(ERigVMMemoryType InMemoryType, const FProperty* InProperty, URigVMMemoryStorage* InMemory);
	void ParseOperationGroups();
	FString GetOperandName(const FRigVMOperand& InOperand, bool bPerSlice = true, bool bAsInput = true) const;
	FString GetOperandCPPType(const FRigVMOperand& InOperand) const;
	FString GetOperandCPPBaseType(const FRigVMOperand& InOperand) const;
	static FString SanitizeName(const FString& InName, const FString& CPPType = FString()); 
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
	const FOperationGroup& GetGroup(int32 InGroupIndex) const;
	bool IsOperationPartOfGroup(int32 InOperationIndex, int32 InGroupIndex, bool bIncludeChildGroups) const;
	bool IsPropertyPartOfGroup(int32 InPropertyIndex, int32 InGroupIndex) const;

	FORCEINLINE static FString FormatArgs(const TCHAR* InFormatString, const FStringFormatOrderedArguments& InArgs)
	{
		return FString::Format(InFormatString, InArgs);
	}

	template<typename TypeA>
	FORCEINLINE static FString Format(const TCHAR* InFormatString, const TypeA& InArgA)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA)
		});
	}

	template<typename TypeA, typename TypeB>
	FORCEINLINE static FString Format(const TCHAR* InFormatString, const TypeA& InArgA, const TypeB& InArgB)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB)
		});
	}

	template<typename TypeA, typename TypeB, typename TypeC>
	FORCEINLINE static FString Format(const TCHAR* InFormatString, const TypeA& InArgA, const TypeB& InArgB, const TypeC& InArgC)
	{
		return FormatArgs(InFormatString, {
			FStringFormatArg(InArgA),
			FStringFormatArg(InArgB),
			FStringFormatArg(InArgC)
		});
	}

	template<typename TypeA, typename TypeB, typename TypeC, typename TypeD>
	FORCEINLINE static FString Format(const TCHAR* InFormatString,
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
	FORCEINLINE static FString Format(const TCHAR* InFormatString,
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
	FORCEINLINE static FString Format(const TCHAR* InFormatString,
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
	FORCEINLINE static FString Format(const TCHAR* InFormatString,
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
	int32 MaxOperationsPerFunction;
	FString ClassName;
	FString ModuleName;
	
	FStringArray Libraries;
	FStringArray Includes;
	TArray<FOperationGroup> OperationGroups;
	FStringArray WrappedArrayCPPTypes;
	TMap<FString, FMappedType> MappedCPPTypes;
	TArray<FPropertyInfo> Properties;
	TMap<FName, int32> PropertyNameToIndex;
	TMap<FName, FName> MappedPropertyNames;

	static const TMap<FName, TStructConstGenerator>& GetStructConstGenerators();
};
