// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Parameter.h"
#include "CADKernel/Core/Types.h"
#include "Serialization/Archive.h"

class FArchive;

namespace UE::CADKernel
{
class FDatabaseFile;

class CADKERNEL_API FParameters
{
	friend FParameter;
protected:

	void Add(FParameter& NewParameter)
	{
		FParameter** Parameter = Map.Find(NewParameter.GetName());
		if (Parameter)
		{
			(*Parameter)->SetValue(NewParameter.GetValue());
		}
		else
		{
			Map.Add(NewParameter.GetName(), &NewParameter);
		}
	}

	TMap<FString, FParameter*> Map;

public:
	FParameters(int32 ParametersNum = 3)
	{
		Map.Reserve(ParametersNum);
	}

	FParameters& operator=(const FParameters& Other)
	{
		for (const TPair<FString, FParameter*>& Parameter : Other.Map)
		{
			Map.Add(Parameter.Key, Parameter.Value);
		}
		return *this;
	}

	virtual ~FParameters() = default;

	FParameter* GetByName(const FString& Name)
	{
		FParameter** Parameter = Map.Find(Name);
		return (Parameter != nullptr) ? *Parameter : nullptr;
	}

	void SetFromString(const FString& ParameterStr);

	FString ToString(bool bOnlyChanged = false) const;

	void PrintParameterList();

	int32 Count()
	{
		return Map.Num();
	}

	const TMap<FString, FParameter*>& GetMap() const
	{
		return Map;
	}

	friend FArchive& operator<<(FArchive& Ar, FParameters& Database)
	{
		return Ar;
	}

};
}
