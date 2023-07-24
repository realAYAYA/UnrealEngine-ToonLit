// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IAudioParameterInterfaceRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "UObject/NameTypes.h"

// Forward Declarations
class UClass;

namespace Metasound
{
	namespace Frontend
	{
		namespace OutputFormatMonoInterface
		{
			namespace Outputs
			{
				METASOUNDFRONTEND_API const extern FName MonoOut;
			} // namespace Outputs

			METASOUNDFRONTEND_API const FMetasoundFrontendVersion& GetVersion();
			METASOUNDFRONTEND_API Audio::FParameterInterfacePtr CreateInterface(const UClass& InClass);
		} // namespace OutputFormatMonoInterface

		namespace OutputFormatStereoInterface
		{
			namespace Outputs
			{
				METASOUNDFRONTEND_API const extern FName LeftOut;
				METASOUNDFRONTEND_API const extern FName RightOut;
			} // namespace Outputs

			METASOUNDFRONTEND_API const FMetasoundFrontendVersion& GetVersion();
			METASOUNDFRONTEND_API Audio::FParameterInterfacePtr CreateInterface(const UClass& InClass);
		} // namespace OutputFormatStereoInterface

		namespace OutputFormatQuadInterface
		{
			namespace Outputs
			{
				METASOUNDFRONTEND_API const extern FName FrontLeftOut;
				METASOUNDFRONTEND_API const extern FName FrontRightOut;
				METASOUNDFRONTEND_API const extern FName SideLeftOut;
				METASOUNDFRONTEND_API const extern FName SideRightOut;
			} // namespace Outputs

			METASOUNDFRONTEND_API const FMetasoundFrontendVersion& GetVersion();
			METASOUNDFRONTEND_API Audio::FParameterInterfacePtr CreateInterface(const UClass& InClass);
		} // namespace OutputFormatQuadInterface

		namespace OutputFormatFiveDotOneInterface
		{
			namespace Outputs
			{
				METASOUNDFRONTEND_API const extern FName FrontLeftOut;
				METASOUNDFRONTEND_API const extern FName FrontRightOut;
				METASOUNDFRONTEND_API const extern FName FrontCenterOut;
				METASOUNDFRONTEND_API const extern FName LowFrequencyOut;
				METASOUNDFRONTEND_API const extern FName SideLeftOut;
				METASOUNDFRONTEND_API const extern FName SideRightOut;
			} // namespace Outputs

			METASOUNDFRONTEND_API const FMetasoundFrontendVersion& GetVersion();
			METASOUNDFRONTEND_API Audio::FParameterInterfacePtr CreateInterface(const UClass& InClass);
		} // namespace OutputFormatFiveDotOneInterface

		namespace OutputFormatSevenDotOneInterface
		{
			namespace Outputs
			{
				METASOUNDFRONTEND_API const extern FName FrontLeftOut;
				METASOUNDFRONTEND_API const extern FName FrontRightOut;
				METASOUNDFRONTEND_API const extern FName FrontCenterOut;
				METASOUNDFRONTEND_API const extern FName LowFrequencyOut;
				METASOUNDFRONTEND_API const extern FName SideLeftOut;
				METASOUNDFRONTEND_API const extern FName SideRightOut;
				METASOUNDFRONTEND_API const extern FName BackLeftOut;
				METASOUNDFRONTEND_API const extern FName BackRightOut;
			} // namespace Outputs

			METASOUNDFRONTEND_API const FMetasoundFrontendVersion& GetVersion();
			METASOUNDFRONTEND_API Audio::FParameterInterfacePtr CreateInterface(const UClass& InClass);
		} // namespace OutputFormatSevenDotOneInterface
	} // namespace Frontend
} // namespace Metasound
