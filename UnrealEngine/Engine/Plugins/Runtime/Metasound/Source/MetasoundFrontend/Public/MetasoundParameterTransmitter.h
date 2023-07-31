// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioParameterControllerInterface.h"
#include "IAudioParameterTransmitter.h"
#include "MetasoundDataReference.h"
#include "MetasoundFrontendLiteral.h"
#include "MetasoundLog.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundPrimitives.h"
#include "MetasoundRouter.h"

struct FMetasoundFrontendLiteral;

namespace Metasound
{
	extern METASOUNDFRONTEND_API int32 MetaSoundParameterEnableWarningOnIgnoredParameterCVar;

	namespace Frontend
	{
		METASOUNDFRONTEND_API FLiteral ConvertParameterToLiteral(FAudioParameter&& InValue);
		METASOUNDFRONTEND_API FLiteral ConvertParameterToLiteral(const FAudioParameter& InValue);
		METASOUNDFRONTEND_API FName ConvertParameterToDataType(EAudioParameterType InParameterType);
	}

	/** FMetaSoundParameterTransmitter provides a communication interface for 
	 * sending values to a MetaSound instance. It relies on the send/receive transmission
	 * system to ferry data from the transmitter to the MetaSound instance. Data will
	 * be safely ushered across thread boundaries in scenarios where the instance
	 * transmitter and metasound instance live on different threads. 
	 */
	class METASOUNDFRONTEND_API FMetaSoundParameterTransmitter : public Audio::FParameterTransmitterBase
	{
		FMetaSoundParameterTransmitter(const FMetaSoundParameterTransmitter&) = delete;
		FMetaSoundParameterTransmitter& operator=(const FMetaSoundParameterTransmitter&) = delete;

	public:
		/** FSendInfo describes the MetaSounds input parameters as well as the 
		 * necessary information to route data to the instances inputs. 
		 */
		struct FSendInfo
		{
			/** Global address of instance input. */
			FSendAddress Address;

			/** Name of parameter on MetaSound instance. */
			FName ParameterName;

			/** Type name of parameter on MetaSound instance. */
			FName TypeName;
		};


		/** Initialization parameters for a FMetaSoundParameterTransmitter. */
		struct FInitParams
		{
			/** FOperatorSettings must match the operator settings of the MetaSound 
			 * instance to ensure proper operation. */
			FOperatorSettings OperatorSettings;

			/** ID of the MetaSound instance.  */
			uint64 InstanceID;

			/** Available input parameters on MetaSound instance. */
			TArray<FSendInfo> Infos;

			/** Name of MetaSound used to log parameter related errors. */
			FName DebugMetaSoundName;

			/** Default Audio Parameters set when transmitter is initialized */
			TArray<FAudioParameter> DefaultParams;

			FInitParams(const FOperatorSettings& InSettings, uint64 InInstanceID, TArray<FAudioParameter>&& InDefaultParams, const TArray<FSendInfo>& InInfos=TArray<FSendInfo>())
			: OperatorSettings(InSettings)
			, InstanceID(InInstanceID)
			, Infos(InInfos)
			, DefaultParams(MoveTemp(InDefaultParams))
			{
			}
		};

		/** Creates a unique send address using the given MetaSound environment. */
		static FSendAddress CreateSendAddressFromEnvironment(const FMetasoundEnvironment& InEnvironment, const FVertexName& InVertexName, const FName& InTypeName);
		
		/** Creates a unique send address using the given InstanceID. */
		static FSendAddress CreateSendAddressFromInstanceID(uint64 InInstanceID, const FVertexName& InVertexName, const FName& InTypeName);

		FMetaSoundParameterTransmitter(FMetaSoundParameterTransmitter::FInitParams&& InInitParams);
		virtual ~FMetaSoundParameterTransmitter() = default;

		bool Reset() override;

		/** Sets parameters using array of AudioParameter structs
		 *
		 * @param InParameter - Parameter to set.
		 */
		virtual bool SetParameters(TArray<FAudioParameter>&& InParameters) override;

		/** Set a parameter using a literal.
		 *
		 * @param InParameterName - Name of MetaSound instance parameter.
		 * @param InValue - Literal value used to construct parameter value. 
		 *
		 * @return true on success, false on failure. 
		 */
		bool SetParameterWithLiteral(FName InParameterName, const FLiteral& InValue);

	private:
		// Find FSendInfo by parameter name. 
		const FSendInfo* FindSendInfo(const FName& InParameterName) const;

		// Find ISender by parameter name. 
		ISender* FindSender(const FName& InParameterName);

		// Create and store a new ISender for the given FSendInfo.
		ISender* AddSender(const FSendInfo& InInfo);

		FOperatorSettings OperatorSettings;
		uint64 InstanceID;
		FName DebugMetaSoundName;
		TArray<FSendInfo> SendInfos;

		TMap<FName, TUniquePtr<ISender>> InputSends;
	};
}
