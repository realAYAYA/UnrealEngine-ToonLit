// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Core/RigUnit_Print.h"
#include "Units/RigUnitContext.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Print)

TArray<FRigVMTemplateArgument> FRigDispatch_Print::GetArguments() const
{
	const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
		FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
		FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
	};
	return {
		FRigVMTemplateArgument(FRigVMStruct::ExecuteContextName, ERigVMPinDirection::IO, FRigVMRegistry::Get().GetTypeIndex<FControlRigExecuteContext>()),
		FRigVMTemplateArgument(TEXT("Prefix"), ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FString),
		FRigVMTemplateArgument(TEXT("Value"), ERigVMPinDirection::Input, ValueCategories),
		FRigVMTemplateArgument(TEXT("Enabled"), ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Bool),
		FRigVMTemplateArgument(TEXT("ScreenDuration"), ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Float),
		FRigVMTemplateArgument(TEXT("ScreenColor"), ERigVMPinDirection::Input, FRigVMRegistry::Get().GetTypeIndex<FLinearColor>())
	};
}

FRigVMTemplateTypeMap FRigDispatch_Print::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(FRigVMStruct::ExecuteContextName, FRigVMRegistry::Get().GetTypeIndex<FControlRigExecuteContext>());
	Types.Add(TEXT("Prefix"), RigVMTypeUtils::TypeIndex::FString);
	Types.Add(TEXT("Value"), InTypeIndex);
	Types.Add(TEXT("Enabled"), RigVMTypeUtils::TypeIndex::Bool);
	Types.Add(TEXT("ScreenDuration"), RigVMTypeUtils::TypeIndex::Float);
	Types.Add(TEXT("ScreenColor"), FRigVMRegistry::Get().GetTypeIndex<FLinearColor>());
	return Types;
}

#if WITH_EDITOR

FString FRigDispatch_Print::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == TEXT("Enabled"))
	{
		return TEXT("True");
	}
	if(InArgumentName == TEXT("ScreenDuration"))
	{
		return TEXT("0.050000");
	}
	return FRigDispatchFactory::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}

FString FRigDispatch_Print::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if(InArgumentName == TEXT("ScreenDuration") || InArgumentName == TEXT("ScreenColor"))
	{
		if(InMetaDataKey == FRigVMStruct::DetailsOnlyMetaName)
		{
			return TEXT("True");
		}
	}
	return FRigDispatchFactory::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}

#endif

void FRigDispatch_Print::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
{
#if WITH_EDITOR
	const FProperty* ValueProperty = Handles[2].GetResolvedProperty(); 
	check(ValueProperty);
	check(Handles[1].IsString());
	check(Handles[3].IsBool());
	check(Handles[4].IsFloat());
	check(Handles[5].IsType<FLinearColor>());

	const FString& Prefix = *(const FString*)Handles[1].GetData();
	const bool bEnabled = *(const bool*)Handles[3].GetData();
	const float& ScreenDuration = *(const float*)Handles[4].GetData();
	const FLinearColor& ScreenColor = *(const FLinearColor*)Handles[5].GetData();
	const uint8* Value = Handles[2].GetData();
	
	if(!bEnabled)
	{
		return;
	}

	const FRigUnitContext& Context = GetRigUnitContext(InContext);
	if (Context.State == EControlRigState::Init)
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
	UE_LOG(LogControlRig, Display, LogFormat, *ObjectPath, InContext.PublicData.GetInstructionIndex(), *Prefix, *String);

	if(ScreenDuration > SMALL_NUMBER && Context.World)
	{
		static constexpr TCHAR PrintStringFormat[] = TEXT("[%04d] %s%s");
		UKismetSystemLibrary::PrintString(Context.World, FString::Printf(PrintStringFormat, InContext.PublicData.GetInstructionIndex(), *Prefix, *String), true, false, ScreenColor, ScreenDuration);
	}
#endif
}

