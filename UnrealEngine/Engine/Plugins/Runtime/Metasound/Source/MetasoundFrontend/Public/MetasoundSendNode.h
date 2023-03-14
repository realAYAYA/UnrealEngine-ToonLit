// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundBuilderInterface.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundDataReference.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundParamHelper.h"
#include "MetasoundRouter.h"
#include "MetasoundVertex.h"

#include <type_traits>

#define LOCTEXT_NAMESPACE "MetasoundFrontend"


namespace Metasound
{
	namespace SendVertexNames
	{
		METASOUND_PARAM(AddressInput, "Address", "Address")
	}

	template<typename TDataType>
	class TSendNode : public FNode
	{
	public:
		static const FVertexName& GetSendInputName()
		{
			static const FVertexName& SendInput = GetMetasoundDataTypeName<TDataType>();
			return SendInput;
		}

		static FVertexInterface DeclareVertexInterface()
		{
			using namespace SendVertexNames; 
			static const FDataVertexMetadata AddressInputMetadata
			{
				  FText::GetEmpty() // description
				, METASOUND_GET_PARAM_DISPLAYNAME(AddressInput) // display name
			};

			return FVertexInterface(
				FInputVertexInterface(
					TInputDataVertex<FSendAddress>(METASOUND_GET_PARAM_NAME(AddressInput), AddressInputMetadata),
					TInputDataVertex<TDataType>(GetSendInputName(), FDataVertexMetadata{ FText::GetEmpty() })
				),
				FOutputVertexInterface(
				)
			);
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				const FVertexName& InputName = GetSendInputName();
				FNodeClassMetadata Info;

				Info.ClassName = { "Send", GetMetasoundDataTypeName<TDataType>(), FName() };
				Info.MajorVersion = 1;
				Info.MinorVersion = 0;
				Info.DisplayName = METASOUND_LOCTEXT_FORMAT("Metasound_SendNodeDisplayNameFormat", "Send {0}", GetMetasoundDataTypeDisplayText<TDataType>());
				Info.Description = METASOUND_LOCTEXT("Metasound_SendNodeDescription", "Sends data from a send node with the same name.");
				Info.Author = PluginAuthor;
				Info.PromptIfMissing = PluginNodeMissingPrompt;
				Info.DefaultInterface = DeclareVertexInterface();
				Info.CategoryHierarchy = { METASOUND_LOCTEXT("Metasound_TransmissionNodeCategory", "Transmission") };
				Info.Keywords = { };

				// Then send & receive nodes do not work as expected, particularly 
				// around multiple-consumer scenarios. Deprecate them to avoid
				// metasound assets from relying on send & receive nodes. 
				Info.bDeprecated = true;

				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}


	private:
		class TSendOperator : public TExecutableOperator<TSendOperator>
		{
			public:

				TSendOperator(TDataReadReference<TDataType> InInputData, TDataReadReference<FSendAddress> InSendAddress, const FOperatorSettings& InOperatorSettings)
					: InputData(InInputData)
					, SendAddress(InSendAddress)
					, CachedSendAddress(*InSendAddress)
					, CachedSenderParams({InOperatorSettings, 0.0f})
					, Sender(nullptr)
				{
					Sender = CreateNewSender();
				}

				virtual ~TSendOperator() 
				{
					ResetSenderAndCleanupChannel();
				}

				virtual FDataReferenceCollection GetInputs() const override
				{
					using namespace SendVertexNames; 

					FDataReferenceCollection Inputs;
					Inputs.AddDataReadReference<FSendAddress>(METASOUND_GET_PARAM_NAME(AddressInput), SendAddress);
					Inputs.AddDataReadReference<TDataType>(GetSendInputName(), TDataReadReference<TDataType>(InputData));
					return Inputs;
				}

				virtual FDataReferenceCollection GetOutputs() const override
				{
					return {};
				}

				void Execute()
				{
					if (*SendAddress != CachedSendAddress)
					{
						ResetSenderAndCleanupChannel();
						CachedSendAddress = *SendAddress;
						Sender = CreateNewSender();
						check(Sender.IsValid());
					}

					Sender->Push(*InputData);
				}

			private:
				FSendAddress GetSendAddressWithDataType(const FSendAddress& InAddress) const 
				{
					// The data type of a send address is inferred by the underlying
					// data type of this node. A full send address, including the data type,
					// cannot be constructed from a literal. 
					return FSendAddress{ InAddress.GetChannelName(), GetMetasoundDataTypeName<TDataType>(), InAddress.GetInstanceID() };
				}

				TSenderPtr<TDataType> CreateNewSender() const
				{
					if (ensure(SendAddress->GetDataType().IsNone() || (GetMetasoundDataTypeName<TDataType>() == SendAddress->GetDataType())))
					{
						return FDataTransmissionCenter::Get().RegisterNewSender<TDataType>(GetSendAddressWithDataType(*SendAddress), CachedSenderParams);
					}
					return TSenderPtr<TDataType>(nullptr);
				}

				void ResetSenderAndCleanupChannel()
				{
					Sender.Reset();
					FDataTransmissionCenter::Get().UnregisterDataChannelIfUnconnected(GetSendAddressWithDataType(CachedSendAddress));
				}

				TDataReadReference<TDataType> InputData;
				TDataReadReference<FSendAddress> SendAddress;
				FSendAddress CachedSendAddress;
				FSenderInitParams CachedSenderParams;

				TSenderPtr<TDataType> Sender;
		};

		class FSendOperatorFactory : public IOperatorFactory
		{
			public:
				FSendOperatorFactory() = default;

				virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
				{
					using namespace SendVertexNames;

					if (InParams.InputData.IsVertexBound(GetSendInputName()))
					{
						return MakeUnique<TSendOperator>(InParams.InputData.GetDataReadReference<TDataType>(GetSendInputName()),
							InParams.InputData.GetOrConstructDataReadReference<FSendAddress>(METASOUND_GET_PARAM_NAME(AddressInput)),
							InParams.OperatorSettings
						);
					}
					else
					{
						// No input hook up to send, so this node can no-op
						return MakeUnique<FNoOpOperator>();
					}
				}
		};

		public:

			TSendNode(const FNodeInitData& InInitData)
				: FNode(InInitData.InstanceName, InInitData.InstanceID, GetNodeInfo())
				, Interface(DeclareVertexInterface())
				, Factory(MakeOperatorFactoryRef<FSendOperatorFactory>())
			{
			}

			virtual ~TSendNode() = default;

			virtual const FVertexInterface& GetVertexInterface() const override
			{
				return Interface;
			}

			virtual bool SetVertexInterface(const FVertexInterface& InInterface) override
			{
				return Interface == InInterface;
			}

			virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override
			{
				return Interface == InInterface;
			}

			virtual FOperatorFactorySharedRef GetDefaultOperatorFactory() const override
			{
				return Factory;
			}

		private:
			FVertexInterface Interface;
			FOperatorFactorySharedRef Factory;
	};
}
#undef LOCTEXT_NAMESPACE
