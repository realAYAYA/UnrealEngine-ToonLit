// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/RigVMDispatch_GetParameter.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVM.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Context.h"
#include "Param/ParamStack.h"
#include "Param/ParamStack.h"

const FName FRigVMDispatch_GetParameter::ValueName = TEXT("Value");
const FName FRigVMDispatch_GetParameter::TypeHandleName = TEXT("Type");
const FName FRigVMDispatch_GetParameter::ParameterName = TEXT("Parameter");
const FName FRigVMDispatch_GetParameter::ParameterIdName = TEXT("ParameterId");

FRigVMDispatch_GetParameter::FRigVMDispatch_GetParameter()
{
	FactoryScriptStruct = StaticStruct();
}

FName FRigVMDispatch_GetParameter::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] =
	{
		ParameterName,
		ValueName,
		ParameterIdName,
		TypeHandleName
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

#if WITH_EDITOR
FString FRigVMDispatch_GetParameter::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if ((InArgumentName == TypeHandleName || InArgumentName == ParameterIdName) &&
		InMetaDataKey == FRigVMStruct::SingletonMetaName)
	{
		return TEXT("True");
	}
	else if(InArgumentName == ParameterName && InMetaDataKey == FRigVMStruct::CustomWidgetMetaName)
	{
		return TEXT("ParamName");
	}

	return Super::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}
#endif

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_GetParameter::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if(Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories =
		{
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};

		Infos.Emplace(ParameterName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);
		Infos.Emplace(ValueName, ERigVMPinDirection::Output, ValueCategories);
		Infos.Emplace(ParameterIdName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32);
		Infos.Emplace(TypeHandleName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32);
	}

	return Infos;
}

FRigVMTemplateTypeMap FRigVMDispatch_GetParameter::OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(ParameterName, InTypeIndex);
	Types.Add(ValueName, InTypeIndex);
	Types.Add(ValueName, RigVMTypeUtils::TypeIndex::UInt32);
	Types.Add(ValueName, RigVMTypeUtils::TypeIndex::UInt32);
	return Types;
}

void FRigVMDispatch_GetParameter::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	using namespace UE::AnimNext;

	const FName Parameter = *(FName*)Handles[0].GetData();
	const FProperty* ValueProperty = Handles[1].GetResolvedProperty();
	check(ValueProperty);
	uint8* TargetDataPtr = Handles[1].GetData();

	uint32& ParameterHash = *(uint32*)Handles[2].GetData();
	if (ParameterHash == 0 && Parameter != NAME_None)
	{
		ParameterHash = GetTypeHash(Parameter);
	}

	uint32& TypeHandle = *(uint32*)Handles[3].GetData();
	if (TypeHandle == 0)
	{
		TypeHandle = FParamTypeHandle::FromProperty(ValueProperty).ToRaw();
	}

	TConstArrayView<uint8> SourceData;
	if (FParamStack::Get().GetParamData(FParamId(Parameter, ParameterHash), FParamTypeHandle::FromRaw(TypeHandle), SourceData).IsSuccessful())
	{
		ValueProperty->CopyCompleteValue(TargetDataPtr, SourceData.GetData());
	}
}

