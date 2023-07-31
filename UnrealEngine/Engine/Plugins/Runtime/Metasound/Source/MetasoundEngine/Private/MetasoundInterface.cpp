// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundInterface.h"

#include "Algo/Transform.h"
#include "AudioParameterControllerInterface.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "IAudioParameterTransmitter.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundEngineArchetypes.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundLog.h"
#include "Interfaces/MetasoundFrontendOutputFormatInterfaces.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundSource.h"
#include "MetasoundUObjectRegistry.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/NoExportTypes.h"


namespace Metasound
{
	namespace Engine
	{
		struct FInterfaceRegistryOptions
		{
			bool bIsDefault = false;
			bool bEditorCanAddOrRemove = true;
			FName InputSystemName;
			FName UClassName;
		};

		// Entry for registered interface.
		class FInterfaceRegistryEntry : public Frontend::IInterfaceRegistryEntry
		{
		public:
			FInterfaceRegistryEntry(
				const FMetasoundFrontendInterface& InInterface,
				TUniquePtr<Frontend::IDocumentTransform>&& InUpdateTransform,
				FInterfaceRegistryOptions&& InOptions
			)
				: Interface(InInterface)
				, UpdateTransform(MoveTemp(InUpdateTransform))
				, Options(MoveTemp(InOptions))
			{
			}

			virtual bool EditorCanAddOrRemove() const override
			{
				return Options.bEditorCanAddOrRemove;
			}

			virtual bool UClassIsSupported(FName InUClassName) const override
			{
				if (Options.UClassName.IsNone())
				{
					return true;
				}

				// TODO: Support child asset class types.
				return Options.UClassName == InUClassName;
			}

			virtual bool IsDefault() const override
			{
				return Options.bIsDefault;
			}

			virtual FName GetRouterName() const override
			{
				return Options.InputSystemName;
			}

			virtual const FMetasoundFrontendInterface& GetInterface() const override
			{
				return Interface;
			}

			virtual bool UpdateRootGraphInterface(Frontend::FDocumentHandle InDocument) const override
			{
				if (UpdateTransform.IsValid())
				{
					return UpdateTransform->Transform(InDocument);
				}
				return false;
			}

		private:
			FMetasoundFrontendInterface Interface;
			TUniquePtr<Frontend::IDocumentTransform> UpdateTransform;
			FInterfaceRegistryOptions Options;
		};

		template <typename UClassType>
		void RegisterInterface(const FMetasoundFrontendInterface& InInterface, TUniquePtr<Frontend::IDocumentTransform>&& InUpdateTransform, bool bInIsDefault, bool bEditorCanAddOrRemove, FName InRouterName)
		{
			using namespace Frontend;

			FInterfaceRegistryOptions Options
			{
				bInIsDefault,
				bEditorCanAddOrRemove,
				InRouterName,
				UClassType::StaticClass()->GetFName()
			};

			IMetasoundUObjectRegistry::Get().RegisterUClassInterface(MakeUnique<TMetasoundUObjectRegistryEntry<UClassType>>(InInterface.Version));
			IInterfaceRegistry::Get().RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(InInterface, MoveTemp(InUpdateTransform), MoveTemp(Options)));
		}

		void RegisterInterface(Audio::FParameterInterfacePtr Interface, TUniquePtr<Frontend::IDocumentTransform>&& InUpdateTransform, bool bInIsDefault, bool bEditorCanAddOrRemove, FName InRouterName)
		{
			using namespace Frontend;

			auto ResolveMemberDataType = [](FName DataType, EAudioParameterType ParamType)
			{
				if (!DataType.IsNone())
				{
					const bool bIsRegisteredType = Frontend::IDataTypeRegistry::Get().IsRegistered(DataType);
					if (ensureAlwaysMsgf(bIsRegisteredType, TEXT("Attempting to register Interface member with unregistered DataType '%s'."), *DataType.ToString()))
					{
						return DataType;
					}
				}

				return ConvertParameterToDataType(ParamType);
			};

			FMetasoundFrontendInterface FrontendInterface;
			FrontendInterface.Version = { Interface->GetName(), FMetasoundFrontendVersionNumber { Interface->GetVersion().Major, Interface->GetVersion().Minor } };

			// Transfer all input data from AudioExtension interface struct to FrontendInterface
			{
				Algo::Transform(Interface->GetInputs(), FrontendInterface.Inputs, [&](const Audio::FParameterInterface::FInput& Input)
				{
					FMetasoundFrontendClassInput ClassInput;
					ClassInput.Name = Input.InitValue.ParamName;
					ClassInput.DefaultLiteral = FMetasoundFrontendLiteral(Input.InitValue);
					ClassInput.TypeName = ResolveMemberDataType(Input.DataType, Input.InitValue.ParamType);

#if WITH_EDITOR
					// Interfaces should never serialize text to avoid desync between
					// copied versions serialized in assets and those defined in code.
					ClassInput.Metadata.SetSerializeText(false);
					ClassInput.Metadata.SetDisplayName(Input.DisplayName);
					ClassInput.Metadata.SetDescription(Input.Description);
					ClassInput.Metadata.SortOrderIndex = Input.SortOrderIndex;
					
					FrontendInterface.AddSortOrderToInputStyle(Input.SortOrderIndex);

					// Setup required inputs by telling the style that the input is required
					// This will later be validated against.
					if (!Input.RequiredText.IsEmpty())
					{
						FrontendInterface.AddRequiredInputToStyle(Input.InitValue.ParamName, Input.RequiredText);
					}
#endif // WITH_EDITOR

					ClassInput.VertexID = FGuid::NewGuid();

					return ClassInput;
				});
			}

			// Transfer all output data from AudioExtension interface struct to FrontendInterface
			{
				Algo::Transform(Interface->GetOutputs(), FrontendInterface.Outputs, [&](const Audio::FParameterInterface::FOutput& Output)
				{
					FMetasoundFrontendClassOutput ClassOutput;
					ClassOutput.Name = Output.ParamName;
					ClassOutput.TypeName = ResolveMemberDataType(Output.DataType, Output.ParamType);

#if WITH_EDITOR
					// Interfaces should never serialize text to avoid desync between
					// copied versions serialized in assets and those defined in code.
					ClassOutput.Metadata.SetSerializeText(false);
					ClassOutput.Metadata.SetDisplayName(Output.DisplayName);
					ClassOutput.Metadata.SetDescription(Output.Description);
					ClassOutput.Metadata.SortOrderIndex = Output.SortOrderIndex;
					
					FrontendInterface.AddSortOrderToOutputStyle(Output.SortOrderIndex);

					// Setup required outputs by telling the style that the output is required
					// This will later be validated against.
					if (!Output.RequiredText.IsEmpty())
					{
						FrontendInterface.AddRequiredOutputToStyle(Output.ParamName, Output.RequiredText);
					}
#endif // WITH_EDITOR

					ClassOutput.VertexID = FGuid::NewGuid();

					return ClassOutput;
				});
			}

			Algo::Transform(Interface->GetEnvironment(), FrontendInterface.Environment, [&](const Audio::FParameterInterface::FEnvironmentVariable& Environment)
			{
				FMetasoundFrontendClassEnvironmentVariable EnvironmentVariable;
				EnvironmentVariable.Name = Environment.ParamName;

				// Disabled as it isn't used to infer type when getting/setting at a lower level.
				// TODO: Either remove type info for environment variables all together or enforce type.
				// EnvironmentVariable.TypeName = ResolveMemberDataType(Environment.DataType, Environment.ParamType);

				return EnvironmentVariable;
			});

			const UClass* SourceClass = UMetaSoundSource::StaticClass();
			check(SourceClass);
			const bool bInterfaceSupportsSource = SourceClass->IsChildOf(&Interface->GetType());
			if (ensureAlwaysMsgf(bInterfaceSupportsSource, TEXT("Interfaces defined using Audio::FParameterInterface currently only supported by UMetaSoundSource asset type.")))
			{
				RegisterInterface<UMetaSoundSource>(FrontendInterface, MoveTemp(InUpdateTransform), bInIsDefault, bEditorCanAddOrRemove, InRouterName);
				UE_LOG(LogMetaSound, Verbose, TEXT("Interface '%s' registered for asset type '%s'."), *Interface->GetName().ToString(), *SourceClass->GetName());
			}
		}

		void RegisterInterfaces()
		{
			using namespace Frontend;

			// Register Default Internal Interfaces (Not managed directly by end-user & added by default when creating new MetaSound assets).
			{
				constexpr bool bIsDefault = true;
				constexpr bool bEditorCanAddOrRemove = false;
				RegisterInterface<UMetaSoundPatch>(MetasoundV1_0::GetInterface(), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);

				RegisterInterface(SourceInterface::CreateInterface(*UMetaSoundSource::StaticClass()), MakeUnique<SourceInterface::FUpdateInterface>(), bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);
				RegisterInterface(OutputFormatMonoInterface::CreateInterface(*UMetaSoundSource::StaticClass()), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);

				RegisterInterface(SourceOneShotInterface::CreateInterface(*UMetaSoundSource::StaticClass()), nullptr, bIsDefault, true, IDataReference::RouterName);
			}

			// Register Non-Default Internal Interfaces (Not managed directly by end-user & not added by default when creating new MetaSound assets).
			{
				constexpr bool bIsDefault = false;
				constexpr bool bEditorCanAddOrRemove = false;
				
				// Added for upgrading old metasounds
				RegisterInterface(SourceInterfaceV1_0::CreateInterface(*UMetaSoundSource::StaticClass()), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);

				// Set default interface with unset version to use base UMetaSoundPatch class implementation (legacy requirement for 5.0 alpha).
				RegisterInterface<UMetaSoundPatch>(FMetasoundFrontendInterface(), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);

				RegisterInterface<UMetaSoundSource>(MetasoundOutputFormatStereoV1_0::GetInterface(), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);
				RegisterInterface<UMetaSoundSource>(MetasoundOutputFormatStereoV1_1::GetInterface(), MakeUnique<MetasoundOutputFormatStereoV1_1::FUpdateInterface>(), bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);
				RegisterInterface<UMetaSoundSource>(MetasoundOutputFormatStereoV1_2::GetInterface(), MakeUnique<MetasoundOutputFormatStereoV1_2::FUpdateInterface>(), bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);

				RegisterInterface<UMetaSoundSource>(MetasoundOutputFormatMonoV1_0::GetInterface(), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);
				RegisterInterface<UMetaSoundSource>(MetasoundOutputFormatMonoV1_1::GetInterface(), MakeUnique<MetasoundOutputFormatMonoV1_1::FUpdateInterface>(), bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);
				RegisterInterface<UMetaSoundSource>(MetasoundOutputFormatMonoV1_2::GetInterface(), MakeUnique<MetasoundOutputFormatMonoV1_2::FUpdateInterface>(), bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);

				RegisterInterface(OutputFormatStereoInterface::CreateInterface(*UMetaSoundSource::StaticClass()), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);			
				RegisterInterface(OutputFormatQuadInterface::CreateInterface(*UMetaSoundSource::StaticClass()), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);			
				RegisterInterface(OutputFormatFiveDotOneInterface::CreateInterface(*UMetaSoundSource::StaticClass()), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);			
				RegisterInterface(OutputFormatSevenDotOneInterface::CreateInterface(*UMetaSoundSource::StaticClass()), nullptr, bIsDefault, bEditorCanAddOrRemove, IDataReference::RouterName);			
			}

			// Register External Interfaces (Interfaces defined externally & can be managed directly by end-user).
			auto RegisterExternalInterface = [](Audio::FParameterInterfacePtr Interface)
			{
				// Currently, no externally defined interfaces can be added as default for protection against undesired
				// interfaces automatically being added when creating a new MetaSound asset through the editor. Also,
				// all parameter interfaces are enabled in the editor for addition/removal.
				constexpr bool bIsDefault = false;
				constexpr bool bEditorCanAddOrRemove = true;

				TUniquePtr<Frontend::IDocumentTransform> NullTransform;
				RegisterInterface(Interface, MoveTemp(NullTransform), bIsDefault, bEditorCanAddOrRemove, Audio::IParameterTransmitter::RouterName);
			};

			Audio::IAudioParameterInterfaceRegistry::Get().IterateInterfaces(RegisterExternalInterface);
			Audio::IAudioParameterInterfaceRegistry::Get().OnRegistration(MoveTemp(RegisterExternalInterface));
		}
	} // namespace Engine
} // namespace Metasound
