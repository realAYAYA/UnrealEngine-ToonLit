// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundNoiseGenerator.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "DSP/Noise.h"
#include "MetasoundStandardNodesCategories.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_GeneratorNodes"

namespace Metasound
{
	enum class ENoiseType
	{
		Pink,
		White
	};

	DECLARE_METASOUND_ENUM(ENoiseType, ENoiseType::Pink, METASOUNDSTANDARDNODES_API,
		FEnumNoiseType, FEnumNoiseTypeInfo, FEnumNoiseTypeReadRef, FEnumNoiseTypeWriteRef);

	DEFINE_METASOUND_ENUM_BEGIN(ENoiseType, FEnumNoiseType, "NoiseType")
		DEFINE_METASOUND_ENUM_ENTRY(ENoiseType::Pink, "PinkDescription", "Pink Noise", "PinkDescriptionTT", "Pink noise or 1/f noise, spectral density is inversely proportional to the frequency of the signal"),
		DEFINE_METASOUND_ENUM_ENTRY(ENoiseType::White, "WhiteDescription", "White Noise", "WhiteDescriptionTT", "A random signal having equal intensity at different frequencies"),
	DEFINE_METASOUND_ENUM_END()

	namespace NoiseGeneratorVertexNames
	{
		METASOUND_PARAM(OutAudio, "Audio", "Audio output.")
		METASOUND_PARAM(InputType, "Type", "Type of Noise to Generate.")
		METASOUND_PARAM(InputSeed, "Seed", "Seed for seeding the Random Number Generator, -1 (default) will use current time.")
	}

	class FNoiseOperator : public IOperator
	{
	public:
		static constexpr int32 DefaultSeed = INDEX_NONE;

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);
		static const FNodeClassMetadata& GetNodeInfo();
		static FVertexInterface DeclareVertexInterface();

		FNoiseOperator(const FOperatorSettings& InSettings, FInt32ReadRef&& InReadRef);
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

	protected:
		FInt32ReadRef Seed;
		FAudioBufferWriteRef Out;
		int32 OldSeed;

		template<typename T>
		static T MakeGenerator(int32 InSeed)
		{
			// If we the default seed, use the default noise generator constructor.
			if (InSeed == DefaultSeed)
			{
				return T{};
			}
			else
			{
				return T{ InSeed };
			}
		}

		template<typename T>
		FORCEINLINE void CheckAndReseed(T& InOutGenerator)
		{
			// Reseed?
			int32 NewSeed = *Seed;
			if (OldSeed != NewSeed)
			{
				InOutGenerator = MakeGenerator<T>(NewSeed);
				OldSeed = NewSeed;
			}
		}

		template<typename T>
		FORCEINLINE void Generate(T& InGenerator)
		{
			float* WritePtr = Out->GetData();
			for (int32 i = Out->Num(); i > 0; --i)
			{
				*WritePtr++ = InGenerator.Generate();
			}
		}
	};

	constexpr int32 FNoiseOperator::DefaultSeed;

	struct FNoiseOperator_White final : public FNoiseOperator
	{
		Audio::FWhiteNoise Generator;

		FNoiseOperator_White(const FOperatorSettings& InSettings, FInt32ReadRef&& InSeed)
			: FNoiseOperator{ InSettings, MoveTemp(InSeed) }
			, Generator{ MakeGenerator<Audio::FWhiteNoise>(*Seed) }
		{}

		void Execute()
		{
			// Reseed if necessary.
			CheckAndReseed(Generator);

			// Generate a block.
			Generate(Generator);
		}
		static void ExecuteFunction(IOperator* InOperator) { static_cast<FNoiseOperator_White*>(InOperator)->Execute(); }
		FExecuteFunction GetExecuteFunction() override { return &FNoiseOperator_White::ExecuteFunction; }
	};

	struct FNoiseOperator_Pink final : public FNoiseOperator
	{
		using FNoiseOperator::FNoiseOperator;
		Audio::FPinkNoise Generator;

		FNoiseOperator_Pink(const FOperatorSettings& InSettings, FInt32ReadRef&& InSeed)
			: FNoiseOperator{ InSettings, MoveTemp(InSeed) }
			, Generator{ MakeGenerator<Audio::FPinkNoise>(*Seed) }
		{}

		static void ExecuteFunction(IOperator* InOperator) { static_cast<FNoiseOperator_Pink*>(InOperator)->Execute(); }
		FExecuteFunction GetExecuteFunction() override { return &FNoiseOperator_Pink::ExecuteFunction; }

		void Execute()
		{
			// Reseed if necessary.
			CheckAndReseed(Generator);

			// Generate a block.
			Generate(Generator);
		}
	};
	
	FNoiseOperator::FNoiseOperator(const FOperatorSettings& InSettings, FInt32ReadRef&& InReadRef)
		: Seed{ MoveTemp(InReadRef) }
		, Out{ FAudioBufferWriteRef::CreateNew(InSettings) }
		, OldSeed(*Seed)
	{}

	FDataReferenceCollection FNoiseOperator::GetInputs() const
	{
		using namespace NoiseGeneratorVertexNames;

		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSeed), FInt32ReadRef(Seed));
		return InputDataReferences;
	}

	FDataReferenceCollection FNoiseOperator::GetOutputs() const
	{
		using namespace NoiseGeneratorVertexNames;

		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutAudio), FAudioBufferReadRef(Out));
		return OutputDataReferences;
	}

	FVertexInterface FNoiseOperator::DeclareVertexInterface()
	{
		using namespace NoiseGeneratorVertexNames; 

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSeed), FNoiseOperator::DefaultSeed),
				TInputDataVertex<FEnumNoiseType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputType))
			),
			FOutputVertexInterface(
				TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutAudio))
			)
		);
		return Interface;
	}

	const FNodeClassMetadata& FNoiseOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, TEXT("Noise"), StandardNodes::AudioVariant };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_NoiseNodeDisplayNameX", "Noise");
			Info.Description = METASOUND_LOCTEXT("Metasound_NoiseNodeDescription", "Noise Generator that produces different types of noise");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Generators);
			return Info;
		};
		static const FNodeClassMetadata Info = InitNodeInfo();
		return Info;
	}

	FNoiseNode::FNoiseNode(const FVertexName& InName, const FGuid& InInstanceID, int32 InDefaultSeed)
		: FNodeFacade(InName, InInstanceID, TFacadeOperatorClass<FNoiseOperator>())
		, DefaultSeed(InDefaultSeed)
	{}

	FNoiseNode::FNoiseNode(const FNodeInitData& InInitData)
		: FNoiseNode(InInitData.InstanceName, InInitData.InstanceID, FNoiseOperator::DefaultSeed)
	{}

	TUniquePtr<IOperator> FNoiseOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		using namespace NoiseGeneratorVertexNames; 

		const FNoiseNode& Node = static_cast<const FNoiseNode&>(InParams.Node);
		const FDataReferenceCollection& InputCol = InParams.InputDataReferences;
		const FOperatorSettings& Settings = InParams.OperatorSettings;

		// Static property pin, only used for factory.
		FEnumNoiseTypeReadRef Type = InputCol.GetDataReadReferenceOrConstruct<FEnumNoiseType>(METASOUND_GET_PARAM_NAME(InputType));

		// Seed.
		FInt32ReadRef Seed = InputCol.GetDataReadReferenceOrConstruct<int32>(METASOUND_GET_PARAM_NAME(InputSeed), Node.GetDefaultSeed());

		switch (*Type)
		{
		default:
		case ENoiseType::White:
			return MakeUnique<FNoiseOperator_White>(InParams.OperatorSettings, MoveTemp(Seed));
		case ENoiseType::Pink:
			return MakeUnique<FNoiseOperator_Pink>(InParams.OperatorSettings, MoveTemp(Seed));
		}
		checkNoEntry();
		return nullptr;
	}

	METASOUND_REGISTER_NODE(FNoiseNode);
}
#undef LOCTEXT_NAMESPACE //MetasoundStandardNodes

