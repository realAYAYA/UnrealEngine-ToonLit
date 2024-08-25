// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoupledOutputProvider.h"

#include "DecoupledOutputProviderModule.h"
#include "IOutputProviderLogic.h"

namespace UE::DecoupledOutputProvider::Private
{
	/** Handles calling the of the super function. */
	class FOutputProviderEvent : public IOutputProviderEvent
	{
		UDecoupledOutputProvider& OutputProvider;
		bool bCalledSuper = false;
		const TFunctionRef<void()> SuperFunc;
	public:

		FOutputProviderEvent(UDecoupledOutputProvider& OutputProvider, TFunctionRef<void()> SuperFunc)
			: OutputProvider(OutputProvider)
			, SuperFunc(MoveTemp(SuperFunc))
		{}

		virtual ~FOutputProviderEvent() override
		{
			if (!bCalledSuper)
			{
				SuperFunc();
			}
		}
		
		virtual void ExecuteSuperFunction() override
		{
			bCalledSuper = true;
			SuperFunc();
		}
		
		virtual UDecoupledOutputProvider& GetOutputProvider() const override
		{
			return OutputProvider;
		}
	};

	template<typename TLambda>
	static void SafeModuleCall(TLambda Lambda)
	{
		if (FDecoupledOutputProviderModule::IsAvailable())
		{
			Lambda(FDecoupledOutputProviderModule::Get());
		}
	}
}

void UDecoupledOutputProvider::Initialize()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::Initialize(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnInitialize(EventScope); });
}

void UDecoupledOutputProvider::Deinitialize()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::Deinitialize(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnDeinitialize(EventScope); });
}

void UDecoupledOutputProvider::Tick(const float DeltaTime)
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this, DeltaTime](){ Super::Tick(DeltaTime); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnTick(EventScope, DeltaTime); });
}

void UDecoupledOutputProvider::OnActivate()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::OnActivate(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnActivate(EventScope); });
}

void UDecoupledOutputProvider::OnDeactivate()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::OnDeactivate(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnDeactivate(EventScope); });
}

UE::VCamCore::EViewportChangeReply UDecoupledOutputProvider::PreReapplyViewport()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::PreReapplyViewport(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	
	if (FDecoupledOutputProviderModule::IsAvailable())
	{
		return FDecoupledOutputProviderModule::Get().PreReapplyViewport(EventScope).Get(UE::VCamCore::EViewportChangeReply::Reinitialize);
	}
	return UE::VCamCore::EViewportChangeReply::Reinitialize;
}

void UDecoupledOutputProvider::PostReapplyViewport()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::PostReapplyViewport(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.PostReapplyViewport(EventScope); });
}

void UDecoupledOutputProvider::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	using namespace UE::DecoupledOutputProvider::Private;
	UDecoupledOutputProvider* CastThis = CastChecked<UDecoupledOutputProvider>(InThis);
	const auto SuperFunc = [InThis, &Collector](){ Super::AddReferencedObjects(InThis, Collector); };
	FOutputProviderEvent EventScope(*CastThis, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnAddReferencedObjects(EventScope, Collector); });
}

void UDecoupledOutputProvider::BeginDestroy()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::BeginDestroy(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnBeginDestroy(EventScope); });
}

void UDecoupledOutputProvider::Serialize(FArchive& Ar)
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this, &Ar](){ Super::Serialize(Ar); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnSerialize(EventScope, Ar); });
}

void UDecoupledOutputProvider::PostLoad()
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this](){ Super::PostLoad(); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnPostLoad(EventScope); });
}

#if WITH_EDITOR
void UDecoupledOutputProvider::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	using namespace UE::DecoupledOutputProvider::Private;
	const auto SuperFunc = [this, &PropertyChangedEvent](){ Super::PostEditChangeProperty(PropertyChangedEvent); };
	FOutputProviderEvent EventScope(*this, SuperFunc);
	SafeModuleCall([&](FDecoupledOutputProviderModule& Module){ Module.OnPostEditChangeProperty(EventScope, PropertyChangedEvent); });
}
#endif