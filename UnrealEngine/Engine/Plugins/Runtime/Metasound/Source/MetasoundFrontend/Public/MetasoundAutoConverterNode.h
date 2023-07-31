// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundBuildError.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundDataReference.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendNodesCategories.h"
#include "MetasoundVertex.h"

#include <type_traits>

#define LOCTEXT_NAMESPACE "MetasoundFrontend"


namespace Metasound
{
	// Determines whether an auto converter node will be registered to convert 
	// between two types. 
	template<typename TFromDataType, typename TToDataType>
	struct TIsAutoConvertible
	{
		static constexpr bool bIsConvertible = std::is_convertible<TFromDataType, TToDataType>::value;

		// Handle case of converting enums to/from integers.
		static constexpr bool bIsIntToEnumConversion = std::is_same<int32, TFromDataType>::value && TEnumTraits<TToDataType>::bIsEnum;
		static constexpr bool bIsEnumToIntConversion = TEnumTraits<TFromDataType>::bIsEnum && std::is_same<int32, TToDataType>::value;

		static constexpr bool Value = bIsConvertible || bIsIntToEnumConversion || bIsEnumToIntConversion;
	};

	// This convenience node can be registered and will invoke static_cast<ToDataType>(FromDataType) every time it is executed, 
	// with a special case for enum <-> int32 conversions. 
	template<typename FromDataType, typename ToDataType>
	class TAutoConverterNode : public FNode
	{
		static_assert(TIsAutoConvertible<FromDataType, ToDataType>::Value,
		"Tried to create an auto converter node between two types we can't static_cast between.");
		
	public:
		static const FVertexName& GetInputName()
		{
			static const FVertexName InputName = GetMetasoundDataTypeName<FromDataType>();
			return InputName;
		}

		static const FVertexName& GetOutputName()
		{
			static const FVertexName OutputName = GetMetasoundDataTypeName<ToDataType>();
			return OutputName;
		}

		static FVertexInterface DeclareVertexInterface()
		{
			static const FText InputDesc = METASOUND_LOCTEXT_FORMAT("AutoConvDisplayNamePatternFrom", "Input {0} value.", GetMetasoundDataTypeDisplayText<FromDataType>());
			static const FText OutputDesc = METASOUND_LOCTEXT_FORMAT("AutoConvDisplayNamePatternTo", "Output {0} value.", GetMetasoundDataTypeDisplayText<ToDataType>());

			return FVertexInterface(
				FInputVertexInterface(
					TInputDataVertex<FromDataType>(GetInputName(), FDataVertexMetadata{ InputDesc })
				),
				FOutputVertexInterface(
					TOutputDataVertex<ToDataType>(GetOutputName(), FDataVertexMetadata{ OutputDesc })
				)
			);
		}

		static const FNodeClassMetadata& GetAutoConverterNodeMetadata()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FNodeDisplayStyle DisplayStyle;
				DisplayStyle.bShowName = false;
				DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Conversion");
				DisplayStyle.bShowInputNames = false;
				DisplayStyle.bShowOutputNames = false;

				const FText FromTypeText = GetMetasoundDataTypeDisplayText<FromDataType>();
				const FText ToTypeText = GetMetasoundDataTypeDisplayText<ToDataType>();

				FNodeClassMetadata Info;
				Info.ClassName = { TEXT("Convert"), GetMetasoundDataTypeName<ToDataType>(), GetMetasoundDataTypeName<FromDataType>() };
				Info.MajorVersion = 1;
				Info.MinorVersion = 0;
				Info.DisplayName = METASOUND_LOCTEXT_FORMAT("Metasound_AutoConverterNodeDisplayNameFormat", "{0} to {1}", FromTypeText, ToTypeText);
				Info.Description = METASOUND_LOCTEXT_FORMAT("Metasound_AutoConverterNodeDescriptionNameFormat", "Converts from {0} to {1}.", FromTypeText, ToTypeText);
				Info.Author = PluginAuthor;
				Info.DisplayStyle = DisplayStyle;
				Info.PromptIfMissing = PluginNodeMissingPrompt;
				Info.DefaultInterface = DeclareVertexInterface();

				Info.CategoryHierarchy.Emplace(NodeCategories::Conversions);
				if (TEnumTraits<FromDataType>::bIsEnum || TEnumTraits<ToDataType>::bIsEnum)
				{
					Info.CategoryHierarchy.Emplace(NodeCategories::EnumConversions);
				}
				
				Info.Keywords =
				{
					METASOUND_LOCTEXT("MetasoundConvertKeyword", "Convert"),
					GetMetasoundDataTypeDisplayText<FromDataType>(),
					GetMetasoundDataTypeDisplayText<ToDataType>()
				};

				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}

	private:
		/** FConverterOperator converts from "FromDataType" to "ToDataType" using
		 * a implicit conversion operators.
		 */
		class FConverterOperator : public TExecutableOperator<FConverterOperator>
		{
		public:

			FConverterOperator(TDataReadReference<FromDataType> InFromDataReference, TDataWriteReference<ToDataType> InToDataReference)
				: FromData(InFromDataReference)
				, ToData(InToDataReference)
			{
				Execute();
			}

			virtual ~FConverterOperator() {}

			virtual FDataReferenceCollection GetInputs() const override
			{
				FDataReferenceCollection Inputs;
				Inputs.AddDataReadReference<FromDataType>(GetInputName(), FromData);
				return Inputs;
			}

			virtual FDataReferenceCollection GetOutputs() const override
			{
				FDataReferenceCollection Outputs;
				Outputs.AddDataReadReference<ToDataType>(GetOutputName(), ToData);
				return Outputs;
			}

			void Execute()
			{
				// enum -> int32
				if constexpr (TIsAutoConvertible<FromDataType, ToDataType>::bIsEnumToIntConversion)
				{
					// Convert from enum wrapper to inner enum type, then to int
					typename TEnumTraits<FromDataType>::InnerType InnerEnum = static_cast<typename TEnumTraits<FromDataType>::InnerType>(*FromData);
					*ToData = static_cast<ToDataType>(InnerEnum);
				}
				// int32 -> enum
				else if constexpr (TIsAutoConvertible<FromDataType, ToDataType>::bIsIntToEnumConversion)
				{
					const int32 FromInt = *FromData;
					// Convert from int to inner enum type
					typename TEnumTraits<ToDataType>::InnerType InnerEnum = static_cast<typename TEnumTraits<ToDataType>::InnerType>(FromInt);

					// Update tracking for previous int value we tried to convert, used to prevent log spam if it's an invalid enum value
					if (FromInt != PreviousIntValueForEnumConversion)
					{
						PreviousIntValueForEnumConversion = FromInt;
						bHasLoggedInvalidEnum = false;
					}

					// If int value is invalid for this enum, return enum default value
					TOptional<FName> EnumName = ToDataType::ToName(InnerEnum);
					if (!EnumName.IsSet())
					{
						if (!bHasLoggedInvalidEnum)
						{
							UE_LOG(LogMetaSound, Warning, TEXT("Cannot convert int32 value '%d' to enum type '%s'. No valid corresponding enum value exists, so returning enum default value instead."), FromInt, *GetMetasoundDataTypeDisplayText<ToDataType>().ToString());
							bHasLoggedInvalidEnum = true;
						}
						*ToData = static_cast<ToDataType>(TEnumTraits<ToDataType>::DefaultValue);
					}
					else
					{
						// Convert from inner enum type to int
						*ToData = static_cast<ToDataType>(InnerEnum);
					}
				}
				else
				{
					*ToData = static_cast<ToDataType>(*FromData);
				}
			}

			private:
				TDataReadReference<FromDataType> FromData;
				TDataWriteReference<ToDataType> ToData;

				// To prevent log spam, keep track of whether we've logged an invalid enum value being converted already
				// and the previous int value (need both bool and int for the initial case)
				bool bHasLoggedInvalidEnum = false;
				int32 PreviousIntValueForEnumConversion = 0;
		};

		/** FConverterOperatorFactory creates an operator which converts from 
		 * "FromDataType" to "ToDataType". 
		 */
		class FCoverterOperatorFactory : public IOperatorFactory
		{
			public:
				FCoverterOperatorFactory() = default;

				virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
				{
					TDataWriteReference<ToDataType> WriteReference = TDataWriteReferenceFactory<ToDataType>::CreateAny(InParams.OperatorSettings);

					const FVertexName& InputName = GetInputName();
					const bool bContainsRef = InParams.InputData.IsVertexBound(InputName);
					if (bContainsRef)
					{
						TDataReadReference<FromDataType> ReadReference = InParams.InputData.GetDataReadReference<FromDataType>(InputName);
						return MakeUnique<FConverterOperator>(ReadReference, WriteReference);
					}

					if constexpr (TIsParsable<FromDataType>::Value)
					{
						TDataReadReference<FromDataType> ReadReference = TDataReadReferenceFactory<FromDataType>::CreateAny(InParams.OperatorSettings);
						return MakeUnique<FConverterOperator>(ReadReference, WriteReference);
					}

					// Converter node requires parsable reference if input not connected. Report as an error.
					if (ensure(InParams.Node.GetVertexInterface().ContainsInputVertex(InputName)))
					{
						FInputDataDestination Dest(InParams.Node, InParams.Node.GetVertexInterface().GetInputVertex(GetInputName()));
						AddBuildError<FMissingInputDataReferenceError>(OutResults.Errors, Dest);
					}

					return TUniquePtr<IOperator>(nullptr);
				}
		};

		public:

			TAutoConverterNode(const FNodeInitData& InInitData)
				: FNode(InInitData.InstanceName, InInitData.InstanceID, GetAutoConverterNodeMetadata())
				, Interface(DeclareVertexInterface())
				, Factory(MakeOperatorFactoryRef<FCoverterOperatorFactory>())
			{
			}

			virtual ~TAutoConverterNode() = default;

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
} // namespace Metasound
#undef LOCTEXT_NAMESPACE
