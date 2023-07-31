// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundPrimitives.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundRouter.h"
#include "MetasoundTime.h"
#include "MetasoundTrigger.h"

#include <type_traits>

namespace Metasound
{
	// Disable audio buffer arrays.
	template<>
	struct TEnableAutoArrayTypeRegistration<FAudioBuffer>
	{
		static constexpr bool Value = false;
	};

	// Disable constructor pins of audio buffers
	template<>
	struct TEnableConstructorVertex<FAudioBuffer>
	{
		static constexpr bool Value = false;
	};

	// Disable arrays of triggers
	template<>
	struct TEnableAutoArrayTypeRegistration<FTrigger>
	{
		static constexpr bool Value = false;
	};

	// Disable constructor pins of triggers 
	template<>
	struct TEnableConstructorVertex<FTrigger>
	{
		static constexpr bool Value = false;
	};

	// Disable auto-conversion using the FAudioBuffer(int32) constructor
	template<typename FromDataType>
	struct TEnableAutoConverterNodeRegistration<FromDataType, FAudioBuffer>
	{
		static constexpr bool Value = !std::is_arithmetic<FromDataType>::value;
	};

	// Disable auto-conversions based on FTrigger implicit converters
	template<typename ToDataType>
	struct TEnableAutoConverterNodeRegistration<FTrigger, ToDataType>
	{
		static constexpr bool Value = !std::is_arithmetic<ToDataType>::value;
	};
}

REGISTER_METASOUND_DATATYPE(bool, "Bool", ::Metasound::ELiteralType::Boolean)
REGISTER_METASOUND_DATATYPE(int32, "Int32", ::Metasound::ELiteralType::Integer)
REGISTER_METASOUND_DATATYPE(float, "Float", ::Metasound::ELiteralType::Float)
REGISTER_METASOUND_DATATYPE(FString, "String", ::Metasound::ELiteralType::String)
REGISTER_METASOUND_DATATYPE(Metasound::FTrigger, "Trigger", ::Metasound::ELiteralType::Boolean)
REGISTER_METASOUND_DATATYPE(Metasound::FTime, "Time", ::Metasound::ELiteralType::Float)
REGISTER_METASOUND_DATATYPE(Metasound::FAudioBuffer, "Audio")
REGISTER_METASOUND_DATATYPE(Metasound::FSendAddress, "Transmission:Address", ::Metasound::ELiteralType::String)
