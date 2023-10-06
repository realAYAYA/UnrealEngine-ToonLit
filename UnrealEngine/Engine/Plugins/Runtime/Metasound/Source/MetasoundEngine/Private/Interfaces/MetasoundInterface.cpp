// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/MetasoundInterface.h"

#include "Algo/Transform.h"
#include "AudioParameterControllerInterface.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "IAudioParameterTransmitter.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Interfaces/MetasoundInputFormatInterfaces.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "Metasound.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundLog.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundSource.h"
#include "MetasoundUObjectRegistry.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/NoExportTypes.h"


namespace Metasound::Engine
{
	FInterfaceRegistryEntry::FInterfaceRegistryEntry(FMetasoundFrontendInterface&& InInterface, FName InRouterName)
		: Interface(MoveTemp(InInterface))
		, RouterName(InRouterName)
	{
	}

	FInterfaceRegistryEntry::FInterfaceRegistryEntry(const FMetasoundFrontendInterface& InInterface, FName InRouterName)
		: Interface(InInterface)
		, RouterName(InRouterName)
	{
	}

	FInterfaceRegistryEntry::FInterfaceRegistryEntry(const FMetasoundFrontendInterface& InInterface, TUniquePtr<Frontend::IDocumentTransform>&& InUpdateTransform, FName InRouterName)
		: Interface(InInterface)
		, UpdateTransform(MoveTemp(InUpdateTransform))
		, RouterName(InRouterName)
	{
	}

	FName FInterfaceRegistryEntry::GetRouterName() const
	{
		return RouterName;
	}

	const FMetasoundFrontendInterface& FInterfaceRegistryEntry::GetInterface() const
	{
		return Interface;
	}

	bool FInterfaceRegistryEntry::UpdateRootGraphInterface(Frontend::FDocumentHandle InDocument) const
	{
		if (UpdateTransform.IsValid())
		{
			return UpdateTransform->Transform(InDocument);
		}
		return false;
	}

	void RegisterAudioFormatInterfaces()
	{
		using namespace Frontend;

		IInterfaceRegistry& Reg = IInterfaceRegistry::Get();

		// Input Formats
		{
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(InputFormatMonoInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(InputFormatStereoInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(InputFormatQuadInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(InputFormatFiveDotOneInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(InputFormatSevenDotOneInterface::CreateInterface()));
		}

		// Output Formats
		{
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(OutputFormatMonoInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(OutputFormatStereoInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(OutputFormatQuadInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(OutputFormatFiveDotOneInterface::CreateInterface()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(OutputFormatSevenDotOneInterface::CreateInterface()));
		}
	}

	void RegisterExternalInterfaces()
	{
		// Register External Interfaces (Interfaces defined externally & can be managed directly by end-user).
		auto RegisterExternalInterface = [](Audio::FParameterInterfacePtr Interface)
		{
			using namespace Frontend;

			bool bSupportedInterface = false;
			IMetasoundUObjectRegistry::Get().IterateRegisteredUClasses([&bSupportedInterface, &Interface](UClass& InRegisteredClass)
			{
				const TArray<const UClass*> SupportedUClasses = Interface->FindSupportedUClasses();
				bSupportedInterface |= SupportedUClasses.IsEmpty();
				for (const UClass* SupportedUClass : SupportedUClasses)
				{
					check(SupportedUClass);
					bSupportedInterface |= SupportedUClass->IsChildOf(&InRegisteredClass);
				}
			});

			if (bSupportedInterface)
			{
				IInterfaceRegistry::Get().RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(FMetasoundFrontendInterface(Interface), Audio::IParameterTransmitter::RouterName));
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Parameter interface '%s' not supported by MetaSounds"), *Interface->GetName().ToString());
			}
		};

		Audio::IAudioParameterInterfaceRegistry::Get().IterateInterfaces(RegisterExternalInterface);
		Audio::IAudioParameterInterfaceRegistry::Get().OnRegistration(MoveTemp(RegisterExternalInterface));
	}

	void RegisterInterfaces()
	{
		using namespace Frontend;

		IInterfaceRegistry& Reg = IInterfaceRegistry::Get();

		// Default Source Interfaces
		{
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(SourceInterface::CreateInterface(*UMetaSoundSource::StaticClass()), MakeUnique<SourceInterface::FUpdateInterface>()));
			Reg.RegisterInterface(MakeUnique<FInterfaceRegistryEntry>(SourceOneShotInterface::CreateInterface(*UMetaSoundSource::StaticClass())));
		}

		RegisterAudioFormatInterfaces();
		RegisterExternalInterfaces();
	}
} // namespace Metasound::Engine
