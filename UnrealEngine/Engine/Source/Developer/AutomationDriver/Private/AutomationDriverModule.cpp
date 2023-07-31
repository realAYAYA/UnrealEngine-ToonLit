// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAutomationDriverModule.h"
#include "PassThroughMessageHandler.h"
#include "AutomatedApplication.h"
#include "AutomationDriver.h"
#include "Framework/Application/SlateApplication.h"

class FAutomationDriverModule
	: public IAutomationDriverModule
{
public:

	virtual void StartupModule() override
	{

	}

	virtual void ShutdownModule() override
	{

	}

	virtual TSharedRef<IAutomationDriver, ESPMode::ThreadSafe> CreateDriver() const override
	{
		return FAutomationDriverFactory::Create(AutomatedApplication.Get());
	}

	virtual TSharedRef<IAutomationDriver, ESPMode::ThreadSafe> CreateDriver(const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration) const override
	{
		return FAutomationDriverFactory::Create(AutomatedApplication.Get(), Configuration);
	}

	virtual TSharedRef<IAsyncAutomationDriver, ESPMode::ThreadSafe> CreateAsyncDriver() const override
	{
		return FAsyncAutomationDriverFactory::Create(AutomatedApplication.Get());
	}

	virtual TSharedRef<IAsyncAutomationDriver, ESPMode::ThreadSafe> CreateAsyncDriver(const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration) const override
	{
		return FAsyncAutomationDriverFactory::Create(AutomatedApplication.Get(), Configuration);
	}

	virtual bool IsEnabled() const override
	{
		return AutomatedApplication.IsValid();
	}

	virtual void Enable() override
	{
		if (AutomatedApplication.IsValid())
		{
			return;
		}

		RealApplication = FSlateApplication::Get().GetPlatformApplication();
		RealMessageHandler = RealApplication->GetMessageHandler();

		AutomatedApplication = FAutomatedApplicationFactory::Create(
			RealApplication.ToSharedRef(),
			FPassThroughMessageHandlerFactoryFactory::Create());

		if (AutomatedApplication.IsValid())
		{
			FSlateApplication::Get().SetPlatformApplication(AutomatedApplication.ToSharedRef());
			AutomatedApplication->AllowPlatformMessageHandling();
		}
	}

	virtual void Disable() override
	{
		if (!AutomatedApplication.IsValid())
		{
			return;
		}

		AutomatedApplication->DisablePlatformMessageHandling();

#if WITH_ACCESSIBILITY
		// Unregister primary user on the accessible handler before swapping back the original application
		FGenericAccessibleUserRegistry& UserRegistry = AutomatedApplication->GetAccessibleMessageHandler()->GetAccessibleUserRegistry();
		UserRegistry.UnregisterUser(FGenericAccessibleUserRegistry::GetPrimaryUserIndex());
#endif

		FSlateApplication::Get().SetPlatformApplication(RealApplication.ToSharedRef());
		RealApplication->SetMessageHandler(RealMessageHandler.ToSharedRef());

		AutomatedApplication.Reset();
		RealApplication.Reset();
		RealMessageHandler.Reset();
	}

private:

	TSharedPtr<FAutomatedApplication> AutomatedApplication;
	TSharedPtr<GenericApplication> RealApplication;
	TSharedPtr<FGenericApplicationMessageHandler> RealMessageHandler;
};

IMPLEMENT_MODULE(FAutomationDriverModule, AutomationDriver);
