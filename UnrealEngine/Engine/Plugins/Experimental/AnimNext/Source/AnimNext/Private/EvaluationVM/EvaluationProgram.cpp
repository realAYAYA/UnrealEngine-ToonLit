// Copyright Epic Games, Inc. All Rights Reserved.

#include "EvaluationVM/EvaluationProgram.h"

#include "EvaluationVM/EvaluationTask.h"
#include "EvaluationVM/EvaluationVM.h"
#include "AnimNextStats.h"
#include "Misc/StringBuilder.h"

DEFINE_STAT(STAT_AnimNext_EvaluationProgram_Execute);

namespace UE::AnimNext
{
	bool FEvaluationProgram::IsEmpty() const
	{
		return Tasks.IsEmpty();
	}

	void FEvaluationProgram::Execute(FEvaluationVM& VM) const
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimNext_EvaluationProgram_Execute);

		if (VM.IsValid())
		{
			for (const TUniquePtr<FAnimNextEvaluationTask>& Task : Tasks)
			{
				Task->Execute(VM);
			}
		}
	}

	FString FEvaluationProgram::ToString() const
	{
		FStringBuilderBase Result;

		FString PropertyValue;
		for (const TUniquePtr<FAnimNextEvaluationTask>& Task : Tasks)
		{
			const UScriptStruct* TaskStruct = Task->GetStruct();

			Result.Appendf(TEXT("%s\n"), *TaskStruct->GetName());

			for (TFieldIterator<FProperty> It(TaskStruct); It; ++It)
			{
				if (const FProperty* Property = *It)
				{
					PropertyValue.Reset();

					const bool bIsDefaultValue = !Property->ExportText_InContainer(0, PropertyValue, Task.Get(), nullptr, nullptr, PPF_None);

					if (bIsDefaultValue)
					{
						if (Property->IsA<FBoolProperty>())
						{
							PropertyValue = TEXT("False");
						}
						else if (Property->IsA<FNumericProperty>())
						{
							PropertyValue = TEXT("0");
						}
					}

					Result.Appendf(TEXT("\t%s: %s\n"), *Property->GetName(), *PropertyValue);
				}
			}
		}

		return Result.ToString();
	}
}
