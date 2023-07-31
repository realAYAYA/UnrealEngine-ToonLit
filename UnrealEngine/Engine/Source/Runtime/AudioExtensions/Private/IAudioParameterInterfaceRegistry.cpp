// Copyright Epic Games, Inc. All Rights Reserved.
#include "IAudioParameterInterfaceRegistry.h"


namespace Audio
{
	namespace ParameterInterfaceRegistryPrivate
	{
		class FParameterInterfaceRegistry : public IAudioParameterInterfaceRegistry
		{
		public:
			virtual void IterateInterfaces(TFunction<void(FParameterInterfacePtr)> InFunction) const override
			{
				for (const FParameterInterfacePtr& InterfacePtr : Interfaces)
				{
					InFunction(InterfacePtr);
				}
			}

			virtual void RegisterInterface(FParameterInterfacePtr InInterface) override
			{
				Interfaces.Add(InInterface);
				if (RegistrationFunction)
				{
					RegistrationFunction(InInterface);
				}
			}

			virtual void OnRegistration(TUniqueFunction<void(FParameterInterfacePtr)>&& InFunction) override
			{
				RegistrationFunction = MoveTemp(InFunction);
			}
		};
	} // namespace ParameterInterfaceRegistryPrivate

	TUniquePtr<IAudioParameterInterfaceRegistry> IAudioParameterInterfaceRegistry::Instance;

	IAudioParameterInterfaceRegistry& IAudioParameterInterfaceRegistry::Get()
	{
		using namespace ParameterInterfaceRegistryPrivate;

		if (!Instance.IsValid())
		{
			Instance = MakeUnique<FParameterInterfaceRegistry>();
		}
		return *Instance;
	}
} // namespace Audio
