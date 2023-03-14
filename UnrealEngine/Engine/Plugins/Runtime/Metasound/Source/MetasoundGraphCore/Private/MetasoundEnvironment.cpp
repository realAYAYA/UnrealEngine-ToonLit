// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundEnvironment.h"


DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(void);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(bool);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(int8);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(uint8);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(int16);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(uint16);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(int32);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(uint32);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(int64);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(uint64);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(float);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(double);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(FString);
DEFINE_METASOUND_ENVIRONMENT_VARIABLE_TYPE(FName);

namespace Metasound
{
	FMetasoundEnvironment::FMetasoundEnvironment(const FMetasoundEnvironment& InOther)
	{
		*this = InOther;
	}

	FMetasoundEnvironment& FMetasoundEnvironment::operator=(const FMetasoundEnvironment& InOther)
	{
		Variables.Reset();

		for (auto& VarTuple : InOther.Variables)
		{
			if (ensure(VarTuple.Value.IsValid()))
			{
				Variables.Add(VarTuple.Key, VarTuple.Value->Clone());
			}
		}

		return *this;
	}
}
