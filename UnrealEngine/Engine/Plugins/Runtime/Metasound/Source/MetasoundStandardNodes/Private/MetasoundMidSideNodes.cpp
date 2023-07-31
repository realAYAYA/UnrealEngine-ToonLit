// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundAudioBuffer.h"
#include "DSP/Dsp.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "DSP/FloatArrayMath.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_MetasoundMidSideNodes"

namespace Metasound
{
	/* Mid-Side Encoder */
	namespace MidSideEncodeVertexNames
	{
		METASOUND_PARAM(InputAudioLeft, "In Left", "The left audio channel to convert.");
		METASOUND_PARAM(InputAudioRight, "In Right", "The right audio channel to convert.");
		METASOUND_PARAM(InputSpreadAmount, "Spread Amount", "Amount of Stereo Spread. 0.0 is no spread, 0.5 is the original signal, and 1.0 is full wide.");
		METASOUND_PARAM(InputEqualPower, "Equal Power", "Whether an equal power relationship between the mid and side signals should be maintained.");

		METASOUND_PARAM(OutputAudioMid, "Out Mid", "The Mid content from the audio signal.");
		METASOUND_PARAM(OutputAudioSide, "Out Side", "The Side content from the audio signal.");
	}

	// Operator Class
	class FMidSideEncodeOperator : public TExecutableOperator<FMidSideEncodeOperator>
	{
	public:

		FMidSideEncodeOperator(const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InLeftAudioInput,
			const FAudioBufferReadRef& InRightAudioInput,
			const FFloatReadRef& InSpreadAmount,
			const FBoolReadRef& InEqualPower)
			: AudioInputLeft(InLeftAudioInput)
			, AudioInputRight(InRightAudioInput)
			, SpreadAmount(InSpreadAmount)
			, bEqualPower(InEqualPower)
			, AudioOutputMid(FAudioBufferWriteRef::CreateNew(InSettings))
			, AudioOutputSide(FAudioBufferWriteRef::CreateNew(InSettings))
			, MidScale(0.0f)
			, SideScale(0.0f)
			, PrevMidScale(0.0f)
			, PrevSideScale(0.0f)
			, PrevSpreadAmount(*SpreadAmount)
		{
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FVertexInterface NodeInterface = DeclareVertexInterface();

				FNodeClassMetadata Metadata
				{
					FNodeClassName { StandardNodes::Namespace, "Mid-Side Decode", StandardNodes::AudioVariant },
					1, // Major Version
					0, // Minor Version
					METASOUND_LOCTEXT("MidSideEncodeDisplayName", "Mid-Side Encode"),
					METASOUND_LOCTEXT("MidSideEncodeDesc", "Converts a stereo audio signal from Left and Right to Mid and Side channels."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					NodeInterface,
					{ NodeCategories::Spatialization},
					{ },
					FNodeDisplayStyle{}
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace MidSideEncodeVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudioLeft)),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudioRight)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSpreadAmount), 0.5f),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEqualPower), false)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudioMid)),
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudioSide))
				)
			);

			return Interface;
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace MidSideEncodeVertexNames;

			FDataReferenceCollection InputDataReferences;
			
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAudioLeft), AudioInputLeft);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAudioRight), AudioInputRight);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSpreadAmount), SpreadAmount);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEqualPower), bEqualPower);
			
			return InputDataReferences;

		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace MidSideEncodeVertexNames;
			
			FDataReferenceCollection OutputDataReferences;

			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudioMid), AudioOutputMid);
			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudioSide), AudioOutputSide);
			
			return OutputDataReferences;
		}
		
		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
		{
			using namespace MidSideEncodeVertexNames;
			const FDataReferenceCollection& Inputs = InParams.InputDataReferences;
			const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();
			
			FAudioBufferReadRef LeftAudioIn = Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudioLeft), InParams.OperatorSettings);
			FAudioBufferReadRef RightAudioIn = Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudioRight), InParams.OperatorSettings);
			FFloatReadRef SpreadAmountIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputSpreadAmount), InParams.OperatorSettings);
			FBoolReadRef bEqualPowerIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(InputEqualPower), InParams.OperatorSettings);

			return MakeUnique<FMidSideEncodeOperator>(InParams.OperatorSettings, LeftAudioIn, RightAudioIn, SpreadAmountIn, bEqualPowerIn);
		}

		void Execute()
		{
			/* Update internal variables, if necessary */
			
			const float ClampedSpread = FMath::Clamp(*SpreadAmount, 0.0f, 1.0f);
			bool bNeedsUpdate = FMath::IsNearlyEqual(ClampedSpread, PrevSpreadAmount);
			if (bNeedsUpdate)
			{
				// Convert to radians between 0.0 and PI/2
				const float SpreadScale = ClampedSpread * 0.5f * PI;

				// Compute equal power relationship between Mid and Side
				FMath::SinCos(&SideScale, &MidScale, SpreadScale);

				// Adjust gain so 0.5f Spread results in a 1.0f to 1.0f gain ratio between Mid and Side
				const float NormalizingFactor = UE_SQRT_2;
				MidScale *= NormalizingFactor;
				SideScale *= NormalizingFactor;

				// Clamp values if not Equal Power
				if (!*bEqualPower)
				{
					MidScale = FMath::Clamp(MidScale, 0.0f, 1.0f);
					SideScale = FMath::Clamp(SideScale, 0.0f, 1.0f);
				}

			}

			PrevSpreadAmount = ClampedSpread;

			/* Generate Mid-Side Output */

			// Encode the signal to to MS
			Audio::EncodeMidSide(*AudioInputLeft, *AudioInputRight, *AudioOutputMid, *AudioOutputSide);			
			
			// We should now be in MS mode, now we can apply our gain scalars

			// SideScale never changes quickly without MidScale also changing quickly
			if (FMath::IsNearlyEqual(PrevMidScale, MidScale))
			{
				Audio::ArrayMultiplyByConstantInPlace(*AudioOutputMid, MidScale);
				Audio::ArrayMultiplyByConstantInPlace(*AudioOutputSide, SideScale);
			}
			else
			{
				Audio::ArrayFade(*AudioOutputMid, PrevMidScale, MidScale);
				Audio::ArrayFade(*AudioOutputSide, PrevSideScale, SideScale);
				
				PrevMidScale = MidScale;
				PrevSideScale = SideScale;
			}
			

		}

	private:

		// The input audio buffer
		FAudioBufferReadRef AudioInputLeft;
		FAudioBufferReadRef AudioInputRight;

		// Stereo spread amount
		FFloatReadRef SpreadAmount;

		// Whether an equal power relationship is maintained between mid and side channels
		FBoolReadRef bEqualPower;

		// Output audio buffer
		FAudioBufferWriteRef AudioOutputMid;
		FAudioBufferWriteRef AudioOutputSide;

		// Internal variables used to calculate Mid and side channel gain
		float MidScale;
		float SideScale;
		
		// Storing previous scales for interpolated gain
		float PrevMidScale;
		float PrevSideScale;

		// Previous Spread variable; need to update MidScale and SideScale if this changes between Execute() calls
		float PrevSpreadAmount;

	};

	// Node Class
	class FMidSideEncodeNode : public FNodeFacade
	{
	public:
		FMidSideEncodeNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FMidSideEncodeOperator>())
		{
		}
	};

	// Register node
	METASOUND_REGISTER_NODE(FMidSideEncodeNode)


	/* Mid-Side Decoder */

	namespace MidSideDecodeVertexNames
	{
		METASOUND_PARAM(InputAudioMid, "In Mid", "The mid audio channel to convert.");
		METASOUND_PARAM(InputAudioSide, "In Side", "The side audio channel to convert.");
		METASOUND_PARAM(InputSpreadAmount, "Spread Amount", "Amount of Stereo Spread. 0.0 is no spread, 0.5 is the original signal, and 1.0 is full wide.");
		METASOUND_PARAM(InputEqualPower, "Equal Power", "Whether an equal power relationship between the mid and side signals should be maintained.");
	
		METASOUND_PARAM(OutputAudioLeft, "Out Left", "The left audio channel which has been processed.");
		METASOUND_PARAM(OutputAudioRight, "Out Right", "The Right audio channel which has been processed.");
	}

	// Operator Class
	class FMidSideDecodeOperator : public TExecutableOperator<FMidSideDecodeOperator>
	{
	public:

		FMidSideDecodeOperator(const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InLeftAudioInput,
			const FAudioBufferReadRef& InRightAudioInput,
			const FFloatReadRef& InSpreadAmount,
			const FBoolReadRef& InEqualPower)
			: AudioInputMid(InLeftAudioInput)
			, AudioInputSide(InRightAudioInput)
			, SpreadAmount(InSpreadAmount)
			, bEqualPower(InEqualPower)
			, AudioOutputLeft(FAudioBufferWriteRef::CreateNew(InSettings))
			, AudioOutputRight(FAudioBufferWriteRef::CreateNew(InSettings))
			, MidScale(0.0f)
			, SideScale(0.0f)
			, PrevMidScale(0.0f)
			, PrevSideScale(0.0f)
			, PrevSpreadAmount(*SpreadAmount)
		{
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FVertexInterface NodeInterface = DeclareVertexInterface();

				FNodeClassMetadata Metadata
				{
					FNodeClassName { StandardNodes::Namespace, "Mid-Side Encode", StandardNodes::AudioVariant },
					1, // Major Version
					0, // Minor Version
					METASOUND_LOCTEXT("MidSideDecodeDisplayName", "Mid-Side Decode"),
					METASOUND_LOCTEXT("MidSideDecodeDesc", "Decodes a stereo signal with Mid and Side channels to Left and Right channels."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					NodeInterface,
					{ NodeCategories::Spatialization },
					{ },
					FNodeDisplayStyle()
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace MidSideDecodeVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudioMid)),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudioSide)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSpreadAmount), 0.5f),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEqualPower), false)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudioLeft)),
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudioRight))
				)
			);

			return Interface;
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace MidSideDecodeVertexNames;

			FDataReferenceCollection InputDataReferences;

			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAudioMid), AudioInputMid);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAudioSide), AudioInputSide);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSpreadAmount), SpreadAmount);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEqualPower), bEqualPower);

			return InputDataReferences;

		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace MidSideDecodeVertexNames;

			FDataReferenceCollection OutputDataReferences;

			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudioLeft), AudioOutputLeft);
			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudioRight), AudioOutputRight);

			return OutputDataReferences;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
		{
			using namespace MidSideDecodeVertexNames;
			const FDataReferenceCollection& Inputs = InParams.InputDataReferences;
			const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

			FAudioBufferReadRef MidAudioIn = Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudioMid), InParams.OperatorSettings);
			FAudioBufferReadRef SideAudioIn = Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudioSide), InParams.OperatorSettings);
			FFloatReadRef SpreadAmountIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputSpreadAmount), InParams.OperatorSettings);
			FBoolReadRef bEqualPowerIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(InputEqualPower), InParams.OperatorSettings);

			return MakeUnique<FMidSideDecodeOperator>(InParams.OperatorSettings, MidAudioIn, SideAudioIn, SpreadAmountIn, bEqualPowerIn);
		}

		void Execute()
		{
			/* Update internal variables, if necessary */
			const float ClampedSpread = FMath::Clamp(*SpreadAmount, 0.0f, 1.0f);
			bool bNeedsUpdate = FMath::IsNearlyEqual(ClampedSpread, PrevSpreadAmount);
			if (bNeedsUpdate)
			{
				// Convert to radians between 0.0 and PI/2
				const float SpreadScale = ClampedSpread * 0.5f * PI;

				// Compute equal power relationship between Mid and Side
				FMath::SinCos(&SideScale, &MidScale, SpreadScale);


				// Adjust gain so 0.5f Spread results in a 1.0f to 1.0f gain ratio between Mid and Side
				const float NormalizingFactor = UE_SQRT_2;
				MidScale *= NormalizingFactor;
				SideScale *= NormalizingFactor;

				// Clamp values if not Equal Power
				if (!*bEqualPower)
				{
					MidScale = FMath::Clamp(MidScale, 0.0f, 1.0f);
					SideScale = FMath::Clamp(SideScale, 0.0f, 1.0f);
				}
			}

			PrevSpreadAmount = ClampedSpread;

			/* Generate Mid-Side Output */

			// We need to make a copy of the input data so we can scale it before decoding
			const float* MidInput = AudioInputMid->GetData();
			const float* SideInput = AudioInputSide->GetData();

			const int32 NumFrames = AudioInputMid->Num();

			MidInBuffer.Reset();
			MidInBuffer.Append(MidInput, NumFrames);
			SideInBuffer.Reset();
			SideInBuffer.Append(SideInput, NumFrames);


			// SideScale never changes quickly without MidScale also changing quickly
			if (FMath::IsNearlyEqual(PrevMidScale, MidScale))
			{
				Audio::ArrayMultiplyByConstantInPlace(MidInBuffer, MidScale);
				Audio::ArrayMultiplyByConstantInPlace(SideInBuffer, SideScale);
			}
			else
			{
				Audio::ArrayFade(MidInBuffer, PrevMidScale, MidScale);
				Audio::ArrayFade(SideInBuffer, PrevSideScale, SideScale);

				PrevMidScale = MidScale;
				PrevSideScale = SideScale;
			}

			// Convert to L/R Signal
			Audio::DecodeMidSide(MidInBuffer, SideInBuffer, *AudioOutputLeft, *AudioOutputRight);
		}

	private:

		// The input audio buffer
		FAudioBufferReadRef AudioInputMid;
		FAudioBufferReadRef AudioInputSide;

		// Stereo spread amount
		FFloatReadRef SpreadAmount;

		// Whether an equal power relationship is maintained between mid and side channels
		FBoolReadRef bEqualPower;

		// Output audio buffer
		FAudioBufferWriteRef AudioOutputLeft;
		FAudioBufferWriteRef AudioOutputRight;

		// Internal variables used to calculate Mid and side channel gain
		float MidScale;
		float SideScale;

		// Storing previous scales for interpolated gain
		float PrevMidScale;
		float PrevSideScale;

		// Previous Spread variable; need to update MidScale and SideScale if this changes between Execute() calls
		float PrevSpreadAmount;

		// Reusable buffers to save performance during Execute().
		Audio::FAlignedFloatBuffer MidInBuffer;
		Audio::FAlignedFloatBuffer SideInBuffer;
	};

	// Node Class
	class FMidSideDecodeNode : public FNodeFacade
	{
	public:
		FMidSideDecodeNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FMidSideDecodeOperator>())
		{
		}
	};

	// Register node
	METASOUND_REGISTER_NODE(FMidSideDecodeNode)

}

#undef LOCTEXT_NAMESPACE
