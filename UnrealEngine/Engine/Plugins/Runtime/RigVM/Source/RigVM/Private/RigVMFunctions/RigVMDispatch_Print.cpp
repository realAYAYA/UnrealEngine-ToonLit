// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMDispatch_Print.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVM.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatch_Print)


const FName FRigVMDispatch_Print::PrefixName = TEXT("Prefix");
const FName FRigVMDispatch_Print::ValueName = TEXT("Value");
const FName FRigVMDispatch_Print::EnabledName = TEXT("Enabled");
const FName FRigVMDispatch_Print::ScreenDurationName = TEXT("ScreenDuration");
const FName FRigVMDispatch_Print::ScreenColorName = TEXT("ScreenColor");


FName FRigVMDispatch_Print::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] = {
		PrefixName,
		ValueName,
		EnabledName,
		ScreenDurationName,
		ScreenColorName
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_Print::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if (Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};
	
		Infos.Emplace(PrefixName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FString);
		Infos.Emplace(ValueName, ERigVMPinDirection::Input, ValueCategories);
		Infos.Emplace(EnabledName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Bool);
		Infos.Emplace(ScreenDurationName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Float);
		Infos.Emplace(ScreenColorName, ERigVMPinDirection::Input, FRigVMRegistry::Get().GetTypeIndex<FLinearColor>());
	}
	return Infos;
}

const TArray<FRigVMExecuteArgument>& FRigVMDispatch_Print::GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const
{
	static const TArray<FRigVMExecuteArgument> Arguments = {
		{TEXT("ExecuteContext"), ERigVMPinDirection::IO}
	};
	return Arguments;
}

FRigVMTemplateTypeMap FRigVMDispatch_Print::OnNewArgumentType(const FName& InArgumentName,
                                                              TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(PrefixName, RigVMTypeUtils::TypeIndex::FString);
	Types.Add(ValueName, InTypeIndex);
	Types.Add(EnabledName, RigVMTypeUtils::TypeIndex::Bool);
	Types.Add(ScreenDurationName, RigVMTypeUtils::TypeIndex::Float);
	Types.Add(ScreenColorName, FRigVMRegistry::Get().GetTypeIndex<FLinearColor>());
	return Types;
}

#if WITH_EDITOR

FString FRigVMDispatch_Print::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == EnabledName)
	{
		return TEXT("True");
	}
	if(InArgumentName == ScreenDurationName)
	{
		return TEXT("0.050000");
	}
	return FRigVMDispatchFactory::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}

FString FRigVMDispatch_Print::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if(InArgumentName == ScreenDurationName || InArgumentName == ScreenColorName)
	{
		if(InMetaDataKey == FRigVMStruct::DetailsOnlyMetaName)
		{
			return TEXT("True");
		}
	}
	return FRigVMDispatchFactory::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}

#endif

void FRigVMDispatch_Print::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
{
#if WITH_EDITOR
	const FProperty* ValueProperty = Handles[1].GetResolvedProperty(); 
	check(ValueProperty);
	check(Handles[0].IsString());
	check(Handles[2].IsBool());
	check(Handles[3].IsFloat());
	check(Handles[4].IsType<FLinearColor>());

	const FString& Prefix = *(const FString*)Handles[0].GetData();
	const bool bEnabled = *(const bool*)Handles[2].GetData();
	const float& ScreenDuration = *(const float*)Handles[3].GetData();
	const FLinearColor& ScreenColor = *(const FLinearColor*)Handles[4].GetData();
	const uint8* Value = Handles[1].GetData();
	
	if(!bEnabled)
	{
		return;
	}

	FString String;
	ValueProperty->ExportText_Direct(String, Value, Value, nullptr, PPF_None, nullptr);

	FString ObjectPath;
	if(InContext.VM)
	{
		ObjectPath = InContext.VM->GetName();
	}

	static constexpr TCHAR LogFormat[] = TEXT("%s[%04d] %s%s");
	InContext.GetPublicData<>().Logf({EMessageSeverity::Info, false}, LogFormat, *ObjectPath, InContext.GetPublicData<>().GetInstructionIndex(), *Prefix, *String);
	const UObject* WorldObject = (const UObject*)InContext.VM;

	if(ScreenDuration > SMALL_NUMBER && WorldObject)
	{
		static constexpr TCHAR PrintStringFormat[] = TEXT("[%04d] %s%s");
		UKismetSystemLibrary::PrintString(WorldObject, FString::Printf(PrintStringFormat, InContext.GetPublicData<>().GetInstructionIndex(), *Prefix, *String), true, false, ScreenColor, ScreenDuration);
	}
#endif
}

