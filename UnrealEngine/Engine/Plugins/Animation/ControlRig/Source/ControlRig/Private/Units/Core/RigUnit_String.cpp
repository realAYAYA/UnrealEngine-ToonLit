// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Core/RigUnit_String.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_String)

FRigUnit_StringConcat_Execute()
{
	Result = A + B;
}

FRigUnit_StringTruncate_Execute()
{
	Remainder = Name;
	Chopped = FString();

	if(Name.IsEmpty() || Count <= 0)
	{
		return;
	}

	if (FromEnd)
	{
		Remainder = Name.LeftChop(Count);
		Chopped = Name.Right(Count);
	}
	else
	{
		Remainder = Name.RightChop(Count);
		Chopped = Name.Left(Count);
	}
}

FRigUnit_StringReplace_Execute()
{
	Result = Name.Replace(*Old, *New, ESearchCase::CaseSensitive);
}

FRigUnit_StringEndsWith_Execute()
{
	Result = Name.EndsWith(Ending, ESearchCase::CaseSensitive);
}

FRigUnit_StringStartsWith_Execute()
{
	Result = Name.StartsWith(Start, ESearchCase::CaseSensitive);
}

FRigUnit_StringContains_Execute()
{
	Result = Name.Contains(Search, ESearchCase::CaseSensitive);
}

FRigUnit_StringLength_Execute()
{
	Length = Value.Len();
}

FRigUnit_StringTrimWhitespace_Execute()
{
	Result = Value;
	Result.TrimStartAndEndInline();
}

FRigUnit_StringToUppercase_Execute()
{
	Result = Value.ToUpper();
}

FRigUnit_StringToLowercase_Execute()
{
	Result = Value.ToLower();
}

FRigUnit_StringReverse_Execute()
{
	Reverse = Value.Reverse();
}

FRigUnit_StringLeft_Execute()
{
	Result = Value.Left(Count);
}

FRigUnit_StringRight_Execute()
{
	Result = Value.Right(Count);
}

FRigUnit_StringMiddle_Execute()
{
	if(Count < 0)
	{
		Result = Value.Mid(Start);
	}
	else
	{
		Result = Value.Mid(Start, Count);
	}
}

FRigUnit_StringFind_Execute()
{
	Index = INDEX_NONE;

	if(!Value.IsEmpty() && !Search.IsEmpty())
	{
		Index = Value.Find(Search, ESearchCase::CaseSensitive);
	}

	Found = Index != INDEX_NONE;
}

FRigUnit_StringSplit_Execute()
{
	Result.Reset();
	if(!Value.IsEmpty())
	{
		if(Separator.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Separator is empty."));
			return;
		}

		FString ValueRemaining = Value;
		FString Left, Right;
		while(ValueRemaining.Split(Separator, &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromStart))
		{
			Result.Add(Left);
			Left.Empty();
			ValueRemaining = Right;
		}

		if (!Right.IsEmpty())
		{
			Result.Add(Right);
		}
	}
}

FRigUnit_StringJoin_Execute()
{
	Result.Reset();
	if(!Values.IsEmpty())
	{
		Result = FString::Join(Values, *Separator);
	}
}

FRigUnit_StringPadInteger_Execute()
{
	static constexpr TCHAR Format02Digits[] = TEXT("%02d");
	static constexpr TCHAR Format03Digits[] = TEXT("%03d");
	static constexpr TCHAR Format04Digits[] = TEXT("%04d");
	static constexpr TCHAR Format05Digits[] = TEXT("%05d");
	static constexpr TCHAR Format06Digits[] = TEXT("%06d");
	static constexpr TCHAR Format07Digits[] = TEXT("%07d");
	static constexpr TCHAR Format08Digits[] = TEXT("%08d");
	static constexpr TCHAR Format09Digits[] = TEXT("%09d");
	static constexpr TCHAR Format10Digits[] = TEXT("%10d");
	static constexpr TCHAR Format11Digits[] = TEXT("%11d");
	static constexpr TCHAR Format12Digits[] = TEXT("%12d");
	static constexpr TCHAR Format13Digits[] = TEXT("%13d");
	static constexpr TCHAR Format14Digits[] = TEXT("%14d");
	static constexpr TCHAR Format15Digits[] = TEXT("%15d");
	static constexpr TCHAR Format16Digits[] = TEXT("%16d");

	switch(Digits)
	{
		case 2:
		{
			Result = FString::Printf(Format02Digits, Value); 
			break;
		}
		case 3:
		{
			Result = FString::Printf(Format03Digits, Value); 
			break;
		}
		case 4:
		{
			Result = FString::Printf(Format04Digits, Value); 
			break;
		}
		case 5:
		{
			Result = FString::Printf(Format05Digits, Value); 
			break;
		}
		case 6:
		{
			Result = FString::Printf(Format06Digits, Value); 
			break;
		}
		case 7:
		{
			Result = FString::Printf(Format07Digits, Value); 
			break;
		}
		case 8:
		{
			Result = FString::Printf(Format08Digits, Value); 
			break;
		}
		case 9:
		{
			Result = FString::Printf(Format09Digits, Value); 
			break;
		}
		case 10:
		{
			Result = FString::Printf(Format10Digits, Value); 
			break;
		}
		case 11:
		{
			Result = FString::Printf(Format11Digits, Value); 
			break;
		}
		case 12:
		{
			Result = FString::Printf(Format12Digits, Value); 
			break;
		}
		case 13:
		{
			Result = FString::Printf(Format13Digits, Value); 
			break;
		}
		case 14:
		{
			Result = FString::Printf(Format14Digits, Value); 
			break;
		}
		case 15:
		{
			Result = FString::Printf(Format15Digits, Value); 
			break;
		}
		case 16:
		{
			Result = FString::Printf(Format16Digits, Value); 
			break;
		}
		default:
		{
			Result = FString::FormatAsNumber(Value);
			break;
		}
	}
}

TArray<FRigVMTemplateArgument> FRigDispatch_ToString::GetArguments() const
{
	const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
		FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
		FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
	};
	return {
		FRigVMTemplateArgument(TEXT("Value"), ERigVMPinDirection::Input, ValueCategories),
		FRigVMTemplateArgument(TEXT("Result"), ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::FString)
	};
}

FRigVMTemplateTypeMap FRigDispatch_ToString::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(TEXT("Value"), InTypeIndex);
	Types.Add(TEXT("Result"), RigVMTypeUtils::TypeIndex::FString);
	return Types;
}

FRigVMFunctionPtr FRigDispatch_ToString::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	// treat name and string special
	const TRigVMTypeIndex& ValueTypeIndex = InTypes.FindChecked(TEXT("Value"));
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return [](FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
		{
			check(Handles[0].IsName());
			check(Handles[1].IsString());

			const FName& Value = *(const FName*)Handles[0].GetData();
			FString& Result = *(FString*)Handles[1].GetData();
			Result = Value.ToString();
		};
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FString)
	{
		return [](FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
		{
			check(Handles[0].IsString());
			check(Handles[1].IsString());

			const FString& Value = *(const FString*)Handles[0].GetData();
			FString& Result = *(FString*)Handles[1].GetData();
			Result = Value;
		};
	}
	
	return &FRigDispatch_ToString::Execute;
}

void FRigDispatch_ToString::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
{
	const FProperty* ValueProperty = Handles[0].GetResolvedProperty(); 
	check(ValueProperty);
	check(Handles[1].IsString());

	const uint8* Value = Handles[0].GetData();
	FString& Result = *(FString*)Handles[1].GetData();
	Result.Reset();
	ValueProperty->ExportText_Direct(Result, Value, Value, nullptr, PPF_None, nullptr);
}

TArray<FRigVMTemplateArgument> FRigDispatch_FromString::GetArguments() const
{
	const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
		FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
		FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
	};
	return {
		FRigVMTemplateArgument(TEXT("String"), ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FString),
		FRigVMTemplateArgument(TEXT("Result"), ERigVMPinDirection::Output, ValueCategories)
	};
}

FRigVMTemplateTypeMap FRigDispatch_FromString::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(TEXT("String"), RigVMTypeUtils::TypeIndex::FString);
	Types.Add(TEXT("Result"), InTypeIndex);
	return Types;
}

FRigVMFunctionPtr FRigDispatch_FromString::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	// treat name and string special
	const TRigVMTypeIndex& ResultTypeIndex = InTypes.FindChecked(TEXT("Result"));
	if(ResultTypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return [](FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
		{
			check(Handles[0].IsString());
			check(Handles[1].IsName());

			const FString& String = *(FString*)Handles[0].GetData();
			FName& Result = *(FName*)Handles[1].GetData();
			Result = *String;
		};
	}
	if(ResultTypeIndex == RigVMTypeUtils::TypeIndex::FString)
	{
		return [](FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
		{
			check(Handles[0].IsString());
			check(Handles[1].IsString());

			const FString& String = *(FString*)Handles[0].GetData();
			FString& Result = *(FString*)Handles[1].GetData();
			Result = String;
		};
	}

	return &FRigDispatch_FromString::Execute;
}

void FRigDispatch_FromString::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
{
	const FProperty* ValueProperty = Handles[1].GetResolvedProperty(); 
	check(ValueProperty);
	check(Handles[0].IsString());

	const FString& String = *(const FString*)Handles[0].GetData();
	uint8* Value = Handles[1].GetData();

	class FRigDispatch_FromString_ErrorPipe : public FOutputDevice
	{
	public:

		TArray<FString> Errors;

		FRigDispatch_FromString_ErrorPipe()
			: FOutputDevice()
		{
		}

		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
		{
			Errors.Add(FString::Printf(TEXT("Error convert to string: %s"), V));
		}
	};

	FRigDispatch_FromString_ErrorPipe ErrorPipe;
	ValueProperty->ImportText_Direct(*String, Value, nullptr, PPF_None, &ErrorPipe);

	if(!ErrorPipe.Errors.IsEmpty())
	{
		ValueProperty->InitializeValue(Value);
		for(const FString& Error : ErrorPipe.Errors)
		{
			const FRigUnitContext& Context = GetRigUnitContext(InContext);

#if WITH_EDITOR
			if(Context.Log != nullptr)
			{
				Context.Log->Report(EMessageSeverity::Error, InContext.PublicData.GetFunctionName(), InContext.PublicData.GetInstructionIndex(), Error);
			}
#endif

			FString ObjectPath;
			if(InContext.VM)
			{
				ObjectPath = InContext.VM->GetName();
			}
			
			static constexpr TCHAR ErrorLogFormat[] = TEXT("%s: [%04d] %s");
			UE_LOG(LogControlRig, Error, ErrorLogFormat, *ObjectPath, InContext.PublicData.GetInstructionIndex(), *Error);
		}
	}
}

