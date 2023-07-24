// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"

#include "Serialization/Archive.h"

namespace UE::CADKernel
{
class CADKERNEL_API FParameters;

class CADKERNEL_API FParameterValue
{
public:
	union
	{
		double Real;
		int32 Integer;
		bool Boolean;
	} Value;

	FString StringValue;

protected:
	EValue Type;

public:
	FParameterValue(int32 InValue)
		: Type(EValue::Integer)
	{
		*this = InValue;
	}

	FParameterValue(double InValue)
		: Type(EValue::Double)
	{
		*this = InValue;
	}

	FParameterValue(const FString& InValue)
		: Type(EValue::String)
	{
		*this = InValue;
	}

	FParameterValue(bool InValue)
		: Type(EValue::Boolean)
	{
		*this = InValue;
	}

	FParameterValue(EValue InType = EValue::Integer)
		: Type(InType)
	{
		*this = 0;
	}

	FParameterValue(const FParameterValue& Other)
	{
		Type = Other.Type;
		Value = Other.Value;
		StringValue = Other.StringValue;
	}

	FString ToString() const;

	inline EValue GetType() const
	{
		return Type;
	}

	inline void SetType(EValue NewType)
	{
		Type = NewType;
	}

	inline FParameterValue& operator=(double InValue)
	{
		Value.Real = InValue;
		return *this;
	}

	inline FParameterValue& operator=(int32 InValue)
	{
		Value.Integer = InValue;
		return *this;
	}

	inline FParameterValue& operator=(const FString& InValue)
	{
		StringValue = InValue;
		return *this;
	}

	inline FParameterValue& operator=(bool InValue)
	{
		Value.Boolean = InValue;
		return *this;
	}

	inline operator double& ()
	{
		return Value.Real;
	}

	inline operator FString& ()
	{
		return StringValue;
	}

	inline operator int32& ()
	{
		return Value.Integer;
	}

	inline operator bool& ()
	{
		return Value.Boolean;
	}

	inline operator const double& () const
	{
		return Value.Real;
	}

	inline operator const FString& () const
	{
		return StringValue;
	}

	inline operator const int32& () const
	{
		return Value.Integer;
	}

	inline operator const bool& () const
	{
		return Value.Boolean;
	}

	bool operator==(const FParameterValue& Other) const;
};

class CADKERNEL_API FParameter
{
private:
	FString Name;

	FParameterValue Value;
	FParameterValue DefaultValue;

	const TCHAR** EnumLabels;

	void AddToParameterMap(FParameter& Parameter, FParameters& Parameters);

public:

	template<typename ValueType>
	FParameter(const FString& ParameterName, const ValueType& FirstValue, FParameters& Parameters)
		: Name(ParameterName)
		, Value(FirstValue)
		, DefaultValue(FirstValue)
		, EnumLabels(nullptr)
	{
		AddToParameterMap(*this, Parameters);
	}

	FParameter(const FString& ParameterName, const TCHAR* v, FParameters& Parameters)
		: Name(ParameterName)
		, Value(FString(v))
		, DefaultValue(FString(v))
		, EnumLabels(nullptr)
	{
		AddToParameterMap(*this, Parameters);
	}

	FParameter(const FParameter& Other, FParameters& Parameters)
		: Name(Other.Name)
		, Value(Other.Value)
		, DefaultValue(Other.DefaultValue)
		, EnumLabels(Other.EnumLabels)
	{
		AddToParameterMap(*this, Parameters);
	}

	FParameter()
		: Name()
		, Value()
		, DefaultValue()
		, EnumLabels()
	{
	}

	virtual ~FParameter()
	{
	}

	const FParameterValue& GetValue() const
	{
		return Value;
	}

	const FParameterValue& GetDefaultValue() const
	{
		return DefaultValue;
	}

	template<typename ValueType>
	void SetValue(const ValueType NewValue)
	{
		Value = NewValue;
	}

	template<typename ValueType>
	void SetDefaultValue(const ValueType NewValue)
	{
		DefaultValue = NewValue;
	}

	template<typename ValueType>
	inline FParameter& operator= (const ValueType& NewValue)
	{
		SetValue(NewValue);
		return *this;
	}

	inline FParameter& operator= (const FParameter& Parameter)
	{
		SetValue(Parameter.Value);
		return *this;
	}

	inline operator int32 () const
	{
		return Value.Value.Integer;
	}

	inline operator double() const
	{
		return Value.Value.Real;
	}

	inline operator bool() const
	{
		return Value.Value.Boolean;
	}

	inline operator const FString& () const
	{
		return Value.StringValue;
	}

	inline bool IsDefault() const
	{
		return Value == DefaultValue;
	}

	inline const FString& GetName() const
	{
		return Name;
	}

	inline EValue GetType() const
	{
		return Value.GetType();
	}

	FString GetTypeString() const;

	void SetFromString(const FString& str);
	FString ToString() const;
};

class CADKERNEL_API FAngleParameter
	: public FParameter
{
private:
	double RadianValue;
	double CosineValue;

public:
	FAngleParameter(const FString paramName, double AngleValue, FParameters& Parameters)
		: FParameter(paramName, AngleValue, Parameters)
	{
		RadianValue = FMath::DegreesToRadians(AngleValue);
		CosineValue = cos(RadianValue);
	}

	void SetValue(const FParameterValue& NewValue)
	{
		FParameter::SetValue(NewValue);
		RadianValue = FMath::DegreesToRadians((double)NewValue);
		CosineValue = cos(RadianValue);
	}

	const double GetAngle() const
	{
		return RadianValue;
	};

	const double GetCosine() const
	{
		return CosineValue;
	};
};
} // namespace UE::CADKernel

