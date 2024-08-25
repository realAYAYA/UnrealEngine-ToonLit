// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ICVFXTestControllerBase.h"

#include "Cluster/IDisplayClusterClusterManager.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Controllers/LiveLinkTransformController.h"
#include "DisplayClusterRootActor.h"
#include "Game/IDisplayClusterGameManager.h"
#include "HAL/IConsoleManager.h"
#include "IDisplayClusterCallbacks.h"
#include "ICVFXTestLocation.h"
#include "IDisplayCluster.h"
#include "LiveLinkComponentController.h"
#include "LiveLinkPreset.h"
#include "LiveLinkPresetTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Render/Viewport/IDisplayClusterViewportManager.h"

#include "ICVFXTestControllerAutoTest.generated.h"

class ULocalPlayer;

UENUM()
enum class EICVFXAutoTestState : uint8
{
	InitialLoad,
	Soak,
	TraverseTestLocations ,
	Finished,
	Shutdown,

	MAX
};

class FICVFXAutoTestState
{
public:
	using Super = FICVFXAutoTestState;

	FICVFXAutoTestState() = delete;
	FICVFXAutoTestState(class UICVFXTestControllerAutoTest* const TestController) : Controller(TestController) {}
	virtual ~FICVFXAutoTestState() {}

	virtual void Start(const EICVFXAutoTestState PrevState);
	virtual void End(const EICVFXAutoTestState NewState) {}
	virtual void Tick(const float TimeDelta);

	double GetTestStateTime() const 
	{
		return TimeSinceStart;
	}

protected:
	class UICVFXTestControllerAutoTest* Controller;

private:
	double TimeSinceStart = 0.0;
};

UCLASS()
class UICVFXTestControllerAutoTest : public UICVFXTestControllerBase
{
	GENERATED_BODY()

public:
	FString GetStateName(const EICVFXAutoTestState State) const;
	FICVFXAutoTestState& GetTestState() const;
	FICVFXAutoTestState& SetTestState(const EICVFXAutoTestState NewState);
	void SetTestLocations(const TArray<AActor*> TestLocations);
	virtual void EndICVFXTest(const int32 ExitCode=0) override;

	double GetCurrentStateTime() const
	{
		return GetTimeInCurrentState();
	}

	int32 GetCurrentTestLocationIndex() const
	{
		return CurrentTestLocationIndex;
	}

	void GoToTestLocation(int32 Index);

	void GoToNextTestLocation()
	{
		CurrentTestLocationIndex++;
		GoToTestLocation(CurrentTestLocationIndex);
	}

	int32 NumTestLocations() const
	{
		return TestLocations.Num();
	}

	void SetInnerGPUIndex(int32 InGPUIndex)
	{
		InnerGPUIndex = InGPUIndex;
	}

	int32 GetInnerGPUIndex() const
	{
		return InnerGPUIndex.load();
	}

	void InitializeLiveLink() const
	{
		if (DisplayClusterActor)
		{
			ULiveLinkPreset* LLPreset = LoadObject<ULiveLinkPreset>(NULL, TEXT("/ICVFXTesting/AutomationPreset"));
			check(LLPreset);

			UDisplayClusterICVFXCameraComponent* Component = DisplayClusterActor->FindComponentByClass<UDisplayClusterICVFXCameraComponent>();
			if (ACineCameraActor* CameraActor = Component->GetCameraSettingsICVFX().ExternalCameraActor.Get())
			{
				ULiveLinkComponentController* LiveLinkComponentController = NewObject<ULiveLinkComponentController>(CameraActor);
				LiveLinkComponentController->bUpdateInEditor = true;
				LiveLinkComponentController->bEvaluateLiveLink = true;


				CameraActor->AddInstanceComponent(LiveLinkComponentController);
				LiveLinkComponentController->RegisterComponentWithWorld(CameraActor->GetWorld());

				FName LLVirtualSubjectName;
				if (LLPreset->GetSubjectPresets().Num())
				{
					LLVirtualSubjectName = LLPreset->GetSubjectPresets()[0].Key.SubjectName.Name;
				}

				LLPreset->ApplyToClientLatent([LiveLinkComponentController, LLVirtualSubjectName, CameraActor](bool bSuccess)
				{
					if (bSuccess)
					{
						FLiveLinkSubjectRepresentation SubjectRepresentation;
						SubjectRepresentation.Role = ULiveLinkTransformRole::StaticClass();
						SubjectRepresentation.Subject = LLVirtualSubjectName;

						LiveLinkComponentController->SetSubjectRepresentation(SubjectRepresentation);
						LiveLinkComponentController->SetControlledComponent(ULiveLinkTransformRole::StaticClass(), CameraActor->GetCineCameraComponent());

						if (TObjectPtr<ULiveLinkControllerBase>* Controller = LiveLinkComponentController->ControllerMap.Find(ULiveLinkTransformRole::StaticClass()))
						{
							if (ULiveLinkTransformController* TransformController = Cast<ULiveLinkTransformController>(*Controller))
							{
								TransformController->TransformData.bUseLocation = false;
							}
						}
					}
				});
			}
		}
	}

	void UpdateInnerGPUIndex()
	{
		TArray<UDisplayClusterICVFXCameraComponent*> CameraComponents;
		ADisplayClusterRootActor* RootActor = Cast<ADisplayClusterRootActor>(DisplayClusterActor);
		constexpr bool bIncludeChildActors = false;
		RootActor->GetComponents<UDisplayClusterICVFXCameraComponent>(CameraComponents, bIncludeChildActors);

		UWorld* CurrentWorld = RootActor->GetWorld();

		for (UDisplayClusterICVFXCameraComponent* CameraComponent : CameraComponents)
		{
			CameraComponent->CameraSettings.RenderSettings.AdvancedRenderSettings.GPUIndex = GetInnerGPUIndex();
		}

		if (IDisplayClusterViewportManager* ViewportManager = RootActor->GetViewportManager())
		{
			const FString NodeId = IDisplayCluster::Get().GetClusterMgr()->GetNodeId();
			const EDisplayClusterRenderFrameMode RenderMode = EDisplayClusterRenderFrameMode::Mono;

			ViewportManager->GetConfiguration().UpdateConfigurationForClusterNode(RenderMode, CurrentWorld, NodeId);
		}
	}

	void UpdateTestLocations()
	{
		UClass* TestLocationClass = AICVFXTestLocation::StaticClass();

		TestLocations.Reset();

		for (TActorIterator<AActor> ItActor = TActorIterator<AActor>(GetWorld(), TestLocationClass); ItActor; ++ItActor)
		{
			TestLocations.Add(*ItActor);
		}

		if (TestLocations.Num())
		{
			UE_LOG(LogICVFXTest, Display, TEXT("Found %d test locations."), TestLocations.Num());
		}
		else
		{
			UE_LOG(LogICVFXTest, Display, TEXT("Could not find test locations, defaulting to display cluster position."));
			// Todo: If no uobject path passed to us, default on the first display cluster actor we find?
			TestLocations.Add(DisplayClusterActor);
		}

		SetTestLocations(TestLocations);
	}

protected:
	virtual void OnInit() override;
	virtual void OnPreMapChange() override;
	virtual void OnTick(float TimeDelta) override;
	virtual void BeginDestroy() override;

public:
	float TimePerTestLocation = 60.f;
	double TimeAtTestLocation = 0.0;
	AActor* DisplayClusterActor = nullptr;
private:
	virtual void UnbindAllDelegates() override;

	FICVFXAutoTestState* States[(uint8)EICVFXAutoTestState::MAX];
	float StateTimeouts[(uint8)EICVFXAutoTestState::MAX];
	EICVFXAutoTestState CurrentState;

	virtual void OnPreWorldInitialize(UWorld* World) override;

	UFUNCTION()
	void OnWorldBeginPlay();

	UFUNCTION()
	void OnGameStateSet(AGameStateBase* const GameStateBase);
	
	FConsoleVariableSinkHandle SoakTimeSink;

	UFUNCTION()
	void OnSoakTimeChanged();

	TArray<AActor*> TestLocations;

	int32 CurrentTestLocationIndex = 0;

	/** What GPU Index should be used for the inner viewport. */
	std::atomic<int32> InnerGPUIndex = 0;
};