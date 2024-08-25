// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningPPOTrainer.h"

#include "LearningArray.h"
#include "LearningLog.h"
#include "LearningNeuralNetwork.h"
#include "LearningExperience.h"
#include "LearningProgress.h"
#include "LearningSharedMemory.h"
#include "LearningSharedMemoryTraining.h"
#include "LearningSocketTraining.h"
#include "LearningObservation.h"
#include "LearningAction.h"

#include "Misc/MonitoredProcess.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "SocketSubsystem.h"

ULearningSocketPPOTrainerServerCommandlet::ULearningSocketPPOTrainerServerCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

int32 ULearningSocketPPOTrainerServerCommandlet::Main(const FString& Commandline)
{
	UE_LOG(LogLearning, Display, TEXT("Running PPO Training Server Commandlet..."));

#if WITH_EDITOR
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;

	UCommandlet::ParseCommandLine(*Commandline, Tokens, Switches, Params);

	const FString* PythonExecutiblePathParam = Params.Find(TEXT("PythonExecutiblePath"));
	const FString* ExtraSitePackagesPathParam = Params.Find(TEXT("ExtraSitePackagesPath"));
	const FString* PythonContentPathParam = Params.Find(TEXT("PythonContentPath"));
	const FString* IntermediatePathParam = Params.Find(TEXT("IntermediatePath"));
	const FString* IpAddressParam = Params.Find(TEXT("IpAddress"));
	const FString* PortParam = Params.Find(TEXT("Port"));
	const FString* LogSettingsParam = Params.Find(TEXT("LogSettings"));

	const FString PythonExecutiblePath = PythonExecutiblePathParam ? *PythonExecutiblePathParam : UE::Learning::Trainer::GetPythonExecutablePath(FPaths::ProjectIntermediateDir());
	const FString ExtraSitePackagesPath = ExtraSitePackagesPathParam ? *ExtraSitePackagesPathParam : TEXT("");
	const FString PythonContentPath = PythonContentPathParam ? *PythonContentPathParam : UE::Learning::Trainer::GetPythonContentPath(FPaths::EngineDir());
	const FString IntermediatePath = IntermediatePathParam ? *IntermediatePathParam : UE::Learning::Trainer::GetIntermediatePath(FPaths::ProjectIntermediateDir());

	const TCHAR* IpAddress = IpAddressParam ? *(*IpAddressParam) : UE::Learning::Trainer::DefaultIp;
	const uint32 Port = PortParam ? FCString::Atoi(*(*PortParam)) : UE::Learning::Trainer::DefaultPort;
	
	UE::Learning::ELogSetting LogSettings = UE::Learning::ELogSetting::Normal;
	if (LogSettingsParam)
	{
		if (*LogSettingsParam == TEXT("Normal"))
		{
			LogSettings = UE::Learning::ELogSetting::Normal;
		}
		else if (*LogSettingsParam == TEXT("Silent"))
		{
			LogSettings = UE::Learning::ELogSetting::Silent;
		}
		else
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			return 1;
		}
	}
	
	UE_LOG(LogLearning, Display, TEXT("---  PPO Training Server Arguments ---"));
	UE_LOG(LogLearning, Display, TEXT("PythonExecutiblePath: %s"), *PythonExecutiblePath);
	UE_LOG(LogLearning, Display, TEXT("ExtraSitePackagesPath: %s"), *ExtraSitePackagesPath);
	UE_LOG(LogLearning, Display, TEXT("PythonContentPath: %s"), *PythonContentPath);
	UE_LOG(LogLearning, Display, TEXT("IntermediatePath: %s"), *IntermediatePath);
	UE_LOG(LogLearning, Display, TEXT("IpAddress: %s"), IpAddress);
	UE_LOG(LogLearning, Display, TEXT("Port: %i"), Port);
	UE_LOG(LogLearning, Display, TEXT("LogSettings: %s"), LogSettings == UE::Learning::ELogSetting::Normal ? TEXT("Normal") : TEXT("Silent"));

	UE::Learning::FSocketPPOTrainerServerProcess ServerProcess(
		PythonExecutiblePath,
		ExtraSitePackagesPath,
		PythonContentPath,
		IntermediatePath,
		IpAddress,
		Port,
		UE::Learning::ESubprocessFlags::None,
		LogSettings);

	while (ServerProcess.IsRunning())
	{
		FPlatformProcess::Sleep(0.01f);
	}

#else
	UE_LEARNING_NOT_IMPLEMENTED();
#endif

	return 0;
}

namespace UE::Learning
{
	FSharedMemoryPPOTrainer::FSharedMemoryPPOTrainer(
		const FString& TaskName,
		const FString& PythonExecutablePath,
		const FString& ExtraSitePackagesPath,
		const FString& PythonContentPath,
		const FString& IntermediatePath,
		const FReplayBuffer& ReplayBuffer,
		const ULearningNeuralNetworkData& PolicyNetwork,
		const ULearningNeuralNetworkData& CriticNetwork,
		const ULearningNeuralNetworkData& EncoderNetwork,
		const ULearningNeuralNetworkData& DecoderNetwork,
		const Observation::FSchema& ObservationSchema,
		const Observation::FSchemaElement& ObservationSchemaElement,
		const Action::FSchema& ActionSchema,
		const Action::FSchemaElement& ActionSchemaElement,
		const FPPOTrainerTrainingSettings& TrainingSettings,
		const ELogSetting LogSettings,
		const ESubprocessFlags TrainingProcessFlags,
		const uint16 ProcessNum,
		const ESubprocessFlags MultiProcessFlags)
	{
		UE_LEARNING_CHECK(FPaths::FileExists(PythonExecutablePath));
		UE_LEARNING_CHECK(FPaths::DirectoryExists(PythonContentPath));

		const int32 ObservationVectorDimensionNum = ReplayBuffer.GetObservations().Num<1>();
		const int32 ActionVectorDimensionNum = ReplayBuffer.GetActions().Num<1>();
		const int32 MemoryStateVectorDimensionNum = ReplayBuffer.GetMemoryStates().Num<1>();

		if (!ensure(Policy.Region == nullptr))
		{
			UE_LOG(LogLearning, Warning, TEXT("Training already started!"));
			return;
		}

		// Allocate shared memory

		ProcessIdx = 0;
		FParse::Value(FCommandLine::Get(), TEXT("LearningProcessIdx"), ProcessIdx);

		if (ProcessIdx == 0)
		{
			// Allocate Shared Memory

			Policy = SharedMemory::Allocate<1, uint8>({ PolicyNetwork.GetSnapshotByteNum() });
			Critic = SharedMemory::Allocate<1, uint8>({ CriticNetwork.GetSnapshotByteNum() });
			Encoder = SharedMemory::Allocate<1, uint8>({ EncoderNetwork.GetSnapshotByteNum() });
			Decoder = SharedMemory::Allocate<1, uint8>({ DecoderNetwork.GetSnapshotByteNum() });
			Controls = SharedMemory::Allocate<2, volatile int32>({ ProcessNum, SharedMemoryTraining::GetControlNum() });
			EpisodeStarts = SharedMemory::Allocate<2, int32>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			EpisodeLengths = SharedMemory::Allocate<2, int32>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			EpisodeCompletionModes = SharedMemory::Allocate<2, ECompletionMode>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			EpisodeFinalObservations = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), ObservationVectorDimensionNum });
			EpisodeFinalMemoryStates = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), MemoryStateVectorDimensionNum });
			Observations = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum(), ObservationVectorDimensionNum });
			Actions = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum(), ActionVectorDimensionNum });
			MemoryStates = SharedMemory::Allocate<3, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum(), MemoryStateVectorDimensionNum });
			Rewards = SharedMemory::Allocate<2, float>({ ProcessNum, ReplayBuffer.GetMaxStepNum() });

			// We need to zero the control memory before we start
			// the training sub-process since it may contain uninitialized 
			// values or those left over from previous runs.

			Array::Zero(Controls.View);

			// Create Experience Gathering Sub-processes

			for (uint16 SubprocessIdx = 1; SubprocessIdx < ProcessNum; SubprocessIdx++)
			{
				ensureMsgf(!WITH_EDITOR || IsRunningCommandlet(), TEXT("Multi-processing generally does not work in-editor as it requires a standalone executable."));

				FString SubprocessCommandLine = FCommandLine::GetOriginal();

				SubprocessCommandLine += FString::Printf(TEXT(" -LearningProcessIdx %i"), SubprocessIdx);
				SubprocessCommandLine += FString(TEXT(" -LearningPolicyGuid ")) + Policy.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningCriticGuid ")) + Critic.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningEncoderGuid ")) + Encoder.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningDecoderGuid ")) + Decoder.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningControlsGuid ")) + Controls.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningEpisodeStartsGuid ")) + EpisodeStarts.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningEpisodeLengthsGuid ")) + EpisodeLengths.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningEpisodeCompletionModesGuid ")) + EpisodeCompletionModes.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningEpisodeFinalObservationsGuid ")) + EpisodeFinalObservations.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningEpisodeFinalMemoryStatesGuid ")) + EpisodeFinalMemoryStates.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningObservationsGuid ")) + Observations.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningActionsGuid ")) + Actions.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningMemoryStatesGuid ")) + MemoryStates.Guid.ToString();
				SubprocessCommandLine += FString(TEXT(" -LearningRewardsGuid ")) + Rewards.Guid.ToString();

				const TSharedPtr<FMonitoredProcess> Subprocess = MakeShared<FMonitoredProcess>(
					FPlatformProcess::ExecutablePath(), 
					SubprocessCommandLine, 
					!(MultiProcessFlags & ESubprocessFlags::ShowWindow),
					!(MultiProcessFlags & ESubprocessFlags::NoRedirectOutput));

				if (!(MultiProcessFlags & ESubprocessFlags::NoRedirectOutput))
				{
					Subprocess->OnCanceled().BindRaw(this, &FSharedMemoryPPOTrainer::HandleSubprocessCanceled);
					Subprocess->OnCompleted().BindRaw(this, &FSharedMemoryPPOTrainer::HandleSubprocessCompleted);
					Subprocess->OnOutput().BindStatic(&FSharedMemoryPPOTrainer::HandleSubprocessOutput);
				}

				Subprocess->Launch();

				UE_LOG(LogLearning, Display, TEXT("Subprocess Command: %s %s"), FPlatformProcess::ExecutablePath(), *SubprocessCommandLine);

				ExperienceGatheringSubprocesses.Emplace(Subprocess);
			}

			// Write Config

			IFileManager& FileManager = IFileManager::Get();
			const FString TimeStamp = FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d_%H-%M-%S"));
			const FString TrainerMethod = TEXT("PPO");
			const FString TrainerType = TEXT("SharedMemory");
			const FString ConfigPath = IntermediatePath / TEXT("Configs") / FString::Printf(TEXT("%s_%s_%s_%s.json"), *TaskName, *TrainerMethod, *TrainerType, *TimeStamp);

			TSharedRef<FJsonObject> ConfigObject = MakeShared<FJsonObject>();
			ConfigObject->SetStringField(TEXT("TaskName"), *TaskName);
			ConfigObject->SetStringField(TEXT("TrainerMethod"), TrainerMethod);
			ConfigObject->SetStringField(TEXT("TrainerType"), TrainerType);
			ConfigObject->SetStringField(TEXT("TimeStamp"), *TimeStamp);

			ConfigObject->SetStringField(TEXT("ExtraSitePackagesPath"), *FileManager.ConvertToAbsolutePathForExternalAppForRead(*ExtraSitePackagesPath));
			ConfigObject->SetStringField(TEXT("IntermediatePath"), *FileManager.ConvertToAbsolutePathForExternalAppForRead(*IntermediatePath));

			ConfigObject->SetStringField(TEXT("PolicyGuid"), *Policy.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("CriticGuid"), *Critic.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("EncoderGuid"), *Encoder.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("DecoderGuid"), *Decoder.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("ControlsGuid"), *Controls.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("EpisodeStartsGuid"), *EpisodeStarts.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("EpisodeLengthsGuid"), *EpisodeLengths.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("EpisodeCompletionModesGuid"), *EpisodeCompletionModes.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("EpisodeFinalObservationsGuid"), *EpisodeFinalObservations.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("EpisodeFinalMemoryStatesGuid"), *EpisodeFinalMemoryStates.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("ObservationsGuid"), *Observations.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("ActionsGuid"), *Actions.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("MemoryStatesGuid"), *MemoryStates.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			ConfigObject->SetStringField(TEXT("RewardsGuid"), *Rewards.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));

			ConfigObject->SetObjectField(TEXT("ObservationSchema"), Trainer::ConvertObservationSchemaToJSON(ObservationSchema, ObservationSchemaElement));
			ConfigObject->SetObjectField(TEXT("ActionSchema"), Trainer::ConvertActionSchemaToJSON(ActionSchema, ActionSchemaElement));
			ConfigObject->SetNumberField(TEXT("ObservationVectorDimensionNum"), ObservationVectorDimensionNum);
			ConfigObject->SetNumberField(TEXT("ActionVectorDimensionNum"), ActionVectorDimensionNum);
			ConfigObject->SetNumberField(TEXT("MemoryStateVectorDimensionNum"), MemoryStateVectorDimensionNum);
			ConfigObject->SetNumberField(TEXT("MaxEpisodeNum"), ReplayBuffer.GetMaxEpisodeNum());
			ConfigObject->SetNumberField(TEXT("MaxStepNum"), ReplayBuffer.GetMaxStepNum());

			ConfigObject->SetNumberField(TEXT("PolicyNetworkByteNum"), PolicyNetwork.GetSnapshotByteNum());
			ConfigObject->SetNumberField(TEXT("CriticNetworkByteNum"), CriticNetwork.GetSnapshotByteNum());
			ConfigObject->SetNumberField(TEXT("EncoderNetworkByteNum"), EncoderNetwork.GetSnapshotByteNum());
			ConfigObject->SetNumberField(TEXT("DecoderNetworkByteNum"), DecoderNetwork.GetSnapshotByteNum());

			ConfigObject->SetNumberField(TEXT("ProcessNum"), ProcessNum);

			ConfigObject->SetNumberField(TEXT("IterationNum"), TrainingSettings.IterationNum);
			ConfigObject->SetNumberField(TEXT("LearningRatePolicy"), TrainingSettings.LearningRatePolicy);
			ConfigObject->SetNumberField(TEXT("LearningRateCritic"), TrainingSettings.LearningRateCritic);
			ConfigObject->SetNumberField(TEXT("LearningRateDecay"), TrainingSettings.LearningRateDecay);
			ConfigObject->SetNumberField(TEXT("WeightDecay"), TrainingSettings.WeightDecay);
			ConfigObject->SetNumberField(TEXT("PolicyBatchSize"), TrainingSettings.PolicyBatchSize);
			ConfigObject->SetNumberField(TEXT("CriticBatchSize"), TrainingSettings.CriticBatchSize);
			ConfigObject->SetNumberField(TEXT("PolicyWindow"), TrainingSettings.PolicyWindow);
			ConfigObject->SetNumberField(TEXT("IterationsPerGather"), TrainingSettings.IterationsPerGather);
			ConfigObject->SetNumberField(TEXT("CriticWarmupIterations"), TrainingSettings.CriticWarmupIterations);
			ConfigObject->SetNumberField(TEXT("EpsilonClip"), TrainingSettings.EpsilonClip);
			ConfigObject->SetNumberField(TEXT("ActionSurrogateWeight"), TrainingSettings.ActionSurrogateWeight);
			ConfigObject->SetNumberField(TEXT("ActionRegularizationWeight"), TrainingSettings.ActionRegularizationWeight);
			ConfigObject->SetNumberField(TEXT("ActionEntropyWeight"), TrainingSettings.ActionEntropyWeight);
			ConfigObject->SetNumberField(TEXT("ReturnRegularizationWeight"), TrainingSettings.ReturnRegularizationWeight);
			ConfigObject->SetNumberField(TEXT("GaeLambda"), TrainingSettings.GaeLambda);
			ConfigObject->SetBoolField(TEXT("AdvantageNormalization"), TrainingSettings.bAdvantageNormalization);
			ConfigObject->SetNumberField(TEXT("AdvantageMin"), TrainingSettings.AdvantageMin);
			ConfigObject->SetNumberField(TEXT("AdvantageMax"), TrainingSettings.AdvantageMax);
			ConfigObject->SetBoolField(TEXT("UseGradNormMaxClipping"), TrainingSettings.bUseGradNormMaxClipping);
			ConfigObject->SetNumberField(TEXT("GradNormMax"), TrainingSettings.GradNormMax);
			ConfigObject->SetNumberField(TEXT("TrimEpisodeStartStepNum"), TrainingSettings.TrimEpisodeStartStepNum);
			ConfigObject->SetNumberField(TEXT("TrimEpisodeEndStepNum"), TrainingSettings.TrimEpisodeEndStepNum);
			ConfigObject->SetNumberField(TEXT("Seed"), TrainingSettings.Seed);
			ConfigObject->SetNumberField(TEXT("DiscountFactor"), TrainingSettings.DiscountFactor);
			ConfigObject->SetStringField(TEXT("Device"), Trainer::GetDeviceString(TrainingSettings.Device));
			ConfigObject->SetBoolField(TEXT("UseTensorBoard"), TrainingSettings.bUseTensorboard);
			ConfigObject->SetBoolField(TEXT("SaveSnapshots"), TrainingSettings.bSaveSnapshots);

			ConfigObject->SetBoolField(TEXT("LoggingEnabled"), LogSettings == ELogSetting::Silent ? false : true);

			FString JsonString;
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString, 0);
			FJsonSerializer::Serialize(ConfigObject, JsonWriter, true);

			FFileHelper::SaveStringToFile(JsonString, *ConfigPath);

			// Start Python Training Sub-process

			const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" SharedMemory \"%s\""), 
				*FileManager.ConvertToAbsolutePathForExternalAppForRead(*(PythonContentPath / TEXT("train_ppo.py"))), 
				*FileManager.ConvertToAbsolutePathForExternalAppForRead(*ConfigPath));

			TrainingProcess = MakeShared<FMonitoredProcess>(
				FileManager.ConvertToAbsolutePathForExternalAppForRead(*PythonExecutablePath),
				CommandLineArguments, 
				!(TrainingProcessFlags & ESubprocessFlags::ShowWindow),
				!(TrainingProcessFlags & ESubprocessFlags::NoRedirectOutput));

			if (!(TrainingProcessFlags & ESubprocessFlags::NoRedirectOutput))
			{
				TrainingProcess->OnCanceled().BindRaw(this, &FSharedMemoryPPOTrainer::HandleTrainingProcessCanceled);
				TrainingProcess->OnCompleted().BindRaw(this, &FSharedMemoryPPOTrainer::HandleTrainingProcessCompleted);
				TrainingProcess->OnOutput().BindStatic(&FSharedMemoryPPOTrainer::HandleTrainingProcessOutput);
			}

			TrainingProcess->Launch();
		}
		else
		{
			// Parse Guids from command line args

			FGuid PolicyGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningPolicyGuid"), PolicyGuid));
			FGuid CriticGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningCriticGuid"), CriticGuid));
			FGuid EncoderGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEncoderGuid"), EncoderGuid));
			FGuid DecoderGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningDecoderGuid"), DecoderGuid));
			FGuid ControlsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningControlsGuid"), ControlsGuid));
			FGuid EpisodeStartsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeStartsGuid"), EpisodeStartsGuid));
			FGuid EpisodeLengthsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeLengthsGuid"), EpisodeLengthsGuid));
			FGuid EpisodeCompletionModesGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeCompletionModesGuid"), EpisodeCompletionModesGuid));
			FGuid EpisodeFinalObservationsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeFinalObservationsGuid"), EpisodeFinalObservationsGuid));
			FGuid EpisodeFinalMemoryStatesGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningEpisodeFinalMemoryStatesGuid"), EpisodeFinalMemoryStatesGuid));
			FGuid ObservationsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningObservationsGuid"), ObservationsGuid));
			FGuid ActionsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningActionsGuid"), ActionsGuid));
			FGuid MemoryStatesGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningMemoryStatesGuid"), MemoryStatesGuid));
			FGuid RewardsGuid; ensure(FParse::Value(FCommandLine::Get(), TEXT("LearningRewardsGuid"), RewardsGuid));

			// Map shared memory

			Policy = SharedMemory::Map<1, uint8>(PolicyGuid, { PolicyNetwork.GetSnapshotByteNum() });
			Critic = SharedMemory::Map<1, uint8>(CriticGuid, { CriticNetwork.GetSnapshotByteNum() });
			Encoder = SharedMemory::Map<1, uint8>(EncoderGuid, { EncoderNetwork.GetSnapshotByteNum() });
			Decoder = SharedMemory::Map<1, uint8>(DecoderGuid, { DecoderNetwork.GetSnapshotByteNum() });
			Controls = SharedMemory::Map<2, volatile int32>(ControlsGuid, { ProcessNum, SharedMemoryTraining::GetControlNum() });
			EpisodeStarts = SharedMemory::Map<2, int32>(EpisodeStartsGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			EpisodeLengths = SharedMemory::Map<2, int32>(EpisodeLengthsGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			EpisodeCompletionModes = SharedMemory::Map<2, ECompletionMode>(EpisodeCompletionModesGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum() });
			EpisodeFinalObservations = SharedMemory::Map<3, float>(EpisodeFinalObservationsGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), ObservationVectorDimensionNum });
			EpisodeFinalMemoryStates = SharedMemory::Map<3, float>(EpisodeFinalMemoryStatesGuid, { ProcessNum, ReplayBuffer.GetMaxEpisodeNum(), MemoryStateVectorDimensionNum });
			Observations = SharedMemory::Map<3, float>(ObservationsGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum(), ObservationVectorDimensionNum });
			Actions = SharedMemory::Map<3, float>(ActionsGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum(), ActionVectorDimensionNum });
			MemoryStates = SharedMemory::Map<3, float>(MemoryStatesGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum(), MemoryStateVectorDimensionNum });
			Rewards = SharedMemory::Map<2, float>(RewardsGuid, { ProcessNum, ReplayBuffer.GetMaxStepNum() });
		}
	}

	FSharedMemoryPPOTrainer::~FSharedMemoryPPOTrainer()
	{
		Terminate();
	}

	ETrainerResponse FSharedMemoryPPOTrainer::Wait(float Timeout)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		while (TrainingProcess.IsValid())
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return ETrainerResponse::Timeout;
			}
		}

		return ETrainerResponse::Success;
	}

	void FSharedMemoryPPOTrainer::Terminate()
	{
		if (TrainingProcess.IsValid())
		{
			TrainingProcess->Cancel(true);
		}

		TrainingProcess.Reset();

		if (Policy.Region)
		{
			Deallocate();
		}
	}

	ETrainerResponse FSharedMemoryPPOTrainer::SendStop(const float Timeout)
	{
		return SharedMemoryTraining::SendStop(Controls.View[ProcessIdx]);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::RecvPolicy(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::RecvNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			OutNetwork,
			SharedMemoryTraining::EControls::PolicySignal,
			Policy.View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::RecvCritic(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::RecvNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			OutNetwork,
			SharedMemoryTraining::EControls::CriticSignal,
			Critic.View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::RecvEncoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::RecvNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			OutNetwork,
			SharedMemoryTraining::EControls::EncoderSignal,
			Encoder.View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::RecvDecoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::RecvNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			OutNetwork,
			SharedMemoryTraining::EControls::DecoderSignal,
			Decoder.View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::SendPolicy(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			Policy.View,
			SharedMemoryTraining::EControls::PolicySignal,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::SendCritic(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			Critic.View,
			SharedMemoryTraining::EControls::CriticSignal,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::SendEncoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			Encoder.View,
			SharedMemoryTraining::EControls::EncoderSignal,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::SendDecoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendNetwork(
			TrainingProcess.Get(),
			Controls.View[ProcessIdx],
			Decoder.View,
			SharedMemoryTraining::EControls::DecoderSignal,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryPPOTrainer::SendExperience(
		const FReplayBuffer& ReplayBuffer,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendExperience(
			TrainingProcess.Get(),
			EpisodeStarts.View[ProcessIdx],
			EpisodeLengths.View[ProcessIdx],
			EpisodeCompletionModes.View[ProcessIdx],
			EpisodeFinalObservations.View[ProcessIdx],
			EpisodeFinalMemoryStates.View[ProcessIdx],
			Observations.View[ProcessIdx],
			Actions.View[ProcessIdx],
			MemoryStates.View[ProcessIdx],
			Rewards.View[ProcessIdx],
			Controls.View[ProcessIdx],
			ReplayBuffer,
			Timeout,
			LogSettings);
	}

	void FSharedMemoryPPOTrainer::Deallocate()
	{
		if (Policy.Region != nullptr)
		{
			SharedMemory::Deallocate(Policy);
			SharedMemory::Deallocate(Critic);
			SharedMemory::Deallocate(Encoder);
			SharedMemory::Deallocate(Decoder);
			SharedMemory::Deallocate(Controls);
			SharedMemory::Deallocate(EpisodeStarts);
			SharedMemory::Deallocate(EpisodeLengths);
			SharedMemory::Deallocate(EpisodeCompletionModes);
			SharedMemory::Deallocate(EpisodeFinalObservations);
			SharedMemory::Deallocate(EpisodeFinalMemoryStates);
			SharedMemory::Deallocate(Observations);
			SharedMemory::Deallocate(Actions);
			SharedMemory::Deallocate(MemoryStates);
			SharedMemory::Deallocate(Rewards);
		}
	}

	void FSharedMemoryPPOTrainer::HandleSubprocessCanceled()
	{
		UE_LOG(LogLearning, Warning, TEXT("Subprocess canceled"));
	}

	void FSharedMemoryPPOTrainer::HandleSubprocessCompleted(int32 ReturnCode)
	{
		if (ReturnCode != 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("Subprocess finished with warnings or errors"));
		}
	}

	void FSharedMemoryPPOTrainer::HandleSubprocessOutput(FString Output)
	{
		if (!Output.IsEmpty())
		{
			UE_LOG(LogLearning, Display, TEXT("Subprocess: %s"), *Output);
		}
	}


	void FSharedMemoryPPOTrainer::HandleTrainingProcessCanceled()
	{
		UE_LOG(LogLearning, Warning, TEXT("Training process canceled"));

		TrainingProcess.Reset();
	}

	void FSharedMemoryPPOTrainer::HandleTrainingProcessCompleted(int32 ReturnCode)
	{
		if (ReturnCode != 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("Training Process finished with warnings or errors"));
		}

		TrainingProcess.Reset();
	}

	void FSharedMemoryPPOTrainer::HandleTrainingProcessOutput(FString Output)
	{
		if (!Output.IsEmpty())
		{
			UE_LOG(LogLearning, Display, TEXT("Training Process: %s"), *Output);
		}
	}

	FSocketPPOTrainerServerProcess::FSocketPPOTrainerServerProcess(
		const FString& PythonExecutablePath,
		const FString& ExtraSitePackagesPath,
		const FString& PythonContentPath,
		const FString& IntermediatePath,
		const TCHAR* IpAddress,
		const uint32 Port,
		const ESubprocessFlags TrainingProcessFlags,
		const ELogSetting LogSettings)
	{
		UE_LEARNING_CHECK(FPaths::FileExists(PythonExecutablePath));
		UE_LEARNING_CHECK(FPaths::DirectoryExists(PythonContentPath));

		IFileManager& FileManager = IFileManager::Get();
		const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" Socket \"%s:%i\" \"%s\" \"%s\" %i"), 
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*(PythonContentPath / TEXT("train_ppo.py"))), 
			IpAddress, 
			Port, 
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*ExtraSitePackagesPath),
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*IntermediatePath), 
			LogSettings == ELogSetting::Normal ? 1 : 0);

		TrainingProcess = MakeShared<FMonitoredProcess>(
			FileManager.ConvertToAbsolutePathForExternalAppForRead(*PythonExecutablePath),
			CommandLineArguments,
			!(TrainingProcessFlags & ESubprocessFlags::ShowWindow),
			!(TrainingProcessFlags & ESubprocessFlags::NoRedirectOutput));

		if (!(TrainingProcessFlags & ESubprocessFlags::NoRedirectOutput))
		{
			TrainingProcess->OnCanceled().BindRaw(this, &FSocketPPOTrainerServerProcess::HandleTrainingProcessCanceled);
			TrainingProcess->OnCompleted().BindRaw(this, &FSocketPPOTrainerServerProcess::HandleTrainingProcessCompleted);
			TrainingProcess->OnOutput().BindStatic(&FSocketPPOTrainerServerProcess::HandleTrainingProcessOutput);
		}

		TrainingProcess->Launch();
	}

	FSocketPPOTrainerServerProcess::~FSocketPPOTrainerServerProcess()
	{
		Terminate();
	}

	bool FSocketPPOTrainerServerProcess::IsRunning() const
	{
		return TrainingProcess.IsValid();
	}

	bool FSocketPPOTrainerServerProcess::Wait(float Timeout)
	{
		const float SleepTime = 0.001f;
		float WaitTime = 0.0f;

		while (TrainingProcess.IsValid())
		{
			FPlatformProcess::Sleep(SleepTime);
			WaitTime += SleepTime;

			if (WaitTime > Timeout)
			{
				return false;
			}
		}

		return true;
	}

	void FSocketPPOTrainerServerProcess::Terminate()
	{
		if (TrainingProcess.IsValid())
		{
			TrainingProcess->Cancel(true);
		}

		TrainingProcess.Reset();
	}

	void FSocketPPOTrainerServerProcess::HandleTrainingProcessCanceled()
	{
		UE_LOG(LogLearning, Warning, TEXT("Training process canceled"));

		TrainingProcess.Reset();
	}

	void FSocketPPOTrainerServerProcess::HandleTrainingProcessCompleted(int32 ReturnCode)
	{
		if (ReturnCode != 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("Training Process finished with warnings or errors"));
		}

		TrainingProcess.Reset();
	}

	void FSocketPPOTrainerServerProcess::HandleTrainingProcessOutput(FString Output)
	{
		if (!Output.IsEmpty())
		{
			UE_LOG(LogLearning, Display, TEXT("Training Process: %s"), *Output);
		}
	}

	FSocketPPOTrainer::FSocketPPOTrainer(
		ETrainerResponse& OutResponse,
		const FString& TaskName,
		const FReplayBuffer& ReplayBuffer,
		const ULearningNeuralNetworkData& PolicyNetwork,
		const ULearningNeuralNetworkData& CriticNetwork,
		const ULearningNeuralNetworkData& EncoderNetwork,
		const ULearningNeuralNetworkData& DecoderNetwork,
		const Observation::FSchema& ObservationSchema,
		const Observation::FSchemaElement& ObservationSchemaElement,
		const Action::FSchema& ActionSchema,
		const Action::FSchemaElement& ActionSchemaElement,
		const TCHAR* IpAddress,
		const uint32 Port,
		const float Timeout,
		const FPPOTrainerTrainingSettings& TrainingSettings)
	{
		const int32 ObservationVectorDimensionNum = ReplayBuffer.GetObservations().Num<1>();
		const int32 ActionVectorDimensionNum = ReplayBuffer.GetActions().Num<1>();
		const int32 MemoryStateVectorDimensionNum = ReplayBuffer.GetMemoryStates().Num<1>();

		// Write Config

		TSharedRef<FJsonObject> ConfigObject = MakeShared<FJsonObject>();
		ConfigObject->SetStringField(TEXT("TaskName"), *TaskName);
		ConfigObject->SetStringField(TEXT("TrainerMethod"), TEXT("PPO"));
		ConfigObject->SetStringField(TEXT("TrainerType"), TEXT("Network"));
		ConfigObject->SetStringField(TEXT("TimeStamp"), *FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d_%H-%M-%S")));

		ConfigObject->SetObjectField(TEXT("ObservationSchema"), Trainer::ConvertObservationSchemaToJSON(ObservationSchema, ObservationSchemaElement));
		ConfigObject->SetObjectField(TEXT("ActionSchema"), Trainer::ConvertActionSchemaToJSON(ActionSchema, ActionSchemaElement));
		ConfigObject->SetNumberField(TEXT("ObservationVectorDimensionNum"), ObservationVectorDimensionNum);
		ConfigObject->SetNumberField(TEXT("ActionVectorDimensionNum"), ActionVectorDimensionNum);
		ConfigObject->SetNumberField(TEXT("MemoryStateVectorDimensionNum"), MemoryStateVectorDimensionNum);
		ConfigObject->SetNumberField(TEXT("MaxEpisodeNum"), ReplayBuffer.GetMaxEpisodeNum());
		ConfigObject->SetNumberField(TEXT("MaxStepNum"), ReplayBuffer.GetMaxStepNum());

		ConfigObject->SetNumberField(TEXT("PolicyNetworkByteNum"), PolicyNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("CriticNetworkByteNum"), CriticNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("EncoderNetworkByteNum"), EncoderNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("DecoderNetworkByteNum"), DecoderNetwork.GetSnapshotByteNum());

		ConfigObject->SetNumberField(TEXT("IterationNum"), TrainingSettings.IterationNum);
		ConfigObject->SetNumberField(TEXT("LearningRatePolicy"), TrainingSettings.LearningRatePolicy);
		ConfigObject->SetNumberField(TEXT("LearningRateCritic"), TrainingSettings.LearningRateCritic);
		ConfigObject->SetNumberField(TEXT("LearningRateDecay"), TrainingSettings.LearningRateDecay);
		ConfigObject->SetNumberField(TEXT("WeightDecay"), TrainingSettings.WeightDecay);
		ConfigObject->SetNumberField(TEXT("PolicyBatchSize"), TrainingSettings.PolicyBatchSize);
		ConfigObject->SetNumberField(TEXT("CriticBatchSize"), TrainingSettings.CriticBatchSize);
		ConfigObject->SetNumberField(TEXT("PolicyWindow"), TrainingSettings.PolicyWindow);
		ConfigObject->SetNumberField(TEXT("IterationsPerGather"), TrainingSettings.IterationsPerGather);
		ConfigObject->SetNumberField(TEXT("CriticWarmupIterations"), TrainingSettings.CriticWarmupIterations);
		ConfigObject->SetNumberField(TEXT("EpsilonClip"), TrainingSettings.EpsilonClip);
		ConfigObject->SetNumberField(TEXT("ActionSurrogateWeight"), TrainingSettings.ActionSurrogateWeight);
		ConfigObject->SetNumberField(TEXT("ActionRegularizationWeight"), TrainingSettings.ActionRegularizationWeight);
		ConfigObject->SetNumberField(TEXT("ActionEntropyWeight"), TrainingSettings.ActionEntropyWeight);
		ConfigObject->SetNumberField(TEXT("ReturnRegularizationWeight"), TrainingSettings.ReturnRegularizationWeight);
		ConfigObject->SetNumberField(TEXT("GaeLambda"), TrainingSettings.GaeLambda);
		ConfigObject->SetBoolField(TEXT("AdvantageNormalization"), TrainingSettings.bAdvantageNormalization);
		ConfigObject->SetNumberField(TEXT("AdvantageMin"), TrainingSettings.AdvantageMin);
		ConfigObject->SetNumberField(TEXT("AdvantageMax"), TrainingSettings.AdvantageMax);
		ConfigObject->SetBoolField(TEXT("UseGradNormMaxClipping"), TrainingSettings.bUseGradNormMaxClipping);
		ConfigObject->SetNumberField(TEXT("GradNormMax"), TrainingSettings.GradNormMax);
		ConfigObject->SetNumberField(TEXT("TrimEpisodeStartStepNum"), TrainingSettings.TrimEpisodeStartStepNum);
		ConfigObject->SetNumberField(TEXT("TrimEpisodeEndStepNum"), TrainingSettings.TrimEpisodeEndStepNum);
		ConfigObject->SetNumberField(TEXT("Seed"), TrainingSettings.Seed);
		ConfigObject->SetNumberField(TEXT("DiscountFactor"), TrainingSettings.DiscountFactor);
		ConfigObject->SetStringField(TEXT("Device"), Trainer::GetDeviceString(TrainingSettings.Device));
		ConfigObject->SetBoolField(TEXT("UseTensorBoard"), TrainingSettings.bUseTensorboard);
		ConfigObject->SetBoolField(TEXT("SaveSnapshots"), TrainingSettings.bSaveSnapshots);

		FString JsonString;
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString, 0);
		FJsonSerializer::Serialize(ConfigObject, JsonWriter, true);

		// Allocate buffer to receive network data in

		PolicyNetworkBuffer.SetNumUninitialized({ PolicyNetwork.GetSnapshotByteNum() });
		CriticNetworkBuffer.SetNumUninitialized({ CriticNetwork.GetSnapshotByteNum() });
		EncoderNetworkBuffer.SetNumUninitialized({ EncoderNetwork.GetSnapshotByteNum() });
		DecoderNetworkBuffer.SetNumUninitialized({ DecoderNetwork.GetSnapshotByteNum() });

		// Create Socket

		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!SocketSubsystem)
		{
			UE_LOG(LogLearning, Error, TEXT("Could not get socket subsystem"));
			OutResponse = ETrainerResponse::Unexpected;
			return;
		}

		bool bIsValid = false;
		TSharedRef<FInternetAddr> Address = SocketSubsystem->CreateInternetAddr();
		Address->SetIp(IpAddress, bIsValid);
		Address->SetPort(Port);

		if (!bIsValid)
		{
			UE_LOG(LogLearning, Error, TEXT("Invalid Ip Address \"%s\"..."), IpAddress);
			OutResponse = ETrainerResponse::Unexpected;
			return;
		}

		// Connect  to Server

		Socket = FTcpSocketBuilder(TEXT("LearningNetworkPPOTrainerSocket")).AsNonBlocking().Build();
		Socket->Connect(*Address);

		OutResponse = SocketTraining::WaitForConnection(*Socket, Timeout);
		if (OutResponse != ETrainerResponse::Success) { return; }

		// Send Config

		OutResponse = SocketTraining::SendConfig(*Socket, JsonString, Timeout);
		return;
	}

	FSocketPPOTrainer::~FSocketPPOTrainer()
	{
		Terminate();
	}

	ETrainerResponse FSocketPPOTrainer::Wait(const float Timeout)
	{
		return ETrainerResponse::Success;
	}

	void FSocketPPOTrainer::Terminate()
	{
		if (Socket)
		{
			Socket->Close();
			Socket = nullptr;
		}
	}

	ETrainerResponse FSocketPPOTrainer::SendStop(const float Timeout)
	{
		return SocketTraining::SendStop(*Socket, Timeout);
	}

	ETrainerResponse FSocketPPOTrainer::RecvPolicy(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::RecvNetwork(*Socket, OutNetwork, PolicyNetworkBuffer, SocketTraining::ESignal::RecvPolicy, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::RecvCritic(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::RecvNetwork(*Socket, OutNetwork, CriticNetworkBuffer, SocketTraining::ESignal::RecvCritic, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::RecvEncoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::RecvNetwork(*Socket, OutNetwork, EncoderNetworkBuffer, SocketTraining::ESignal::RecvEncoder, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::RecvDecoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::RecvNetwork(*Socket, OutNetwork, DecoderNetworkBuffer, SocketTraining::ESignal::RecvDecoder, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::SendPolicy(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendNetwork(*Socket, PolicyNetworkBuffer, SocketTraining::ESignal::SendPolicy, Network, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::SendCritic(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendNetwork(*Socket, CriticNetworkBuffer, SocketTraining::ESignal::SendCritic, Network, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::SendEncoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendNetwork(*Socket, EncoderNetworkBuffer, SocketTraining::ESignal::SendEncoder, Network, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::SendDecoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendNetwork(*Socket, DecoderNetworkBuffer, SocketTraining::ESignal::SendDecoder, Network, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketPPOTrainer::SendExperience(
		const FReplayBuffer& ReplayBuffer,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendExperience(*Socket, ReplayBuffer, Timeout, LogSettings);
	}

	namespace PPOTrainer
	{
		ETrainerResponse Train(
			IPPOTrainer& Trainer,
			FReplayBuffer& ReplayBuffer,
			FEpisodeBuffer& EpisodeBuffer,
			FResetInstanceBuffer& ResetBuffer,
			ULearningNeuralNetworkData& PolicyNetwork,
			ULearningNeuralNetworkData& CriticNetwork,
			ULearningNeuralNetworkData& EncoderNetwork,
			ULearningNeuralNetworkData& DecoderNetwork,
			TLearningArrayView<2, float> ObservationVectorBuffer,
			TLearningArrayView<2, float> ActionVectorBuffer,
			TLearningArrayView<2, float> PreEvaluationMemoryStateVectorBuffer,
			TLearningArrayView<2, float> MemoryStateVectorBuffer,
			TLearningArrayView<1, float> RewardBuffer,
			TLearningArrayView<1, ECompletionMode> CompletionBuffer,
			TLearningArrayView<1, ECompletionMode> EpisodeCompletionBuffer,
			TLearningArrayView<1, ECompletionMode> AllCompletionBuffer,
			const TFunctionRef<void(const FIndexSet Instances)> ResetFunction,
			const TFunctionRef<void(const FIndexSet Instances)> ObservationFunction,
			const TFunctionRef<void(const FIndexSet Instances)> PolicyFunction,
			const TFunctionRef<void(const FIndexSet Instances)> ActionFunction,
			const TFunctionRef<void(const FIndexSet Instances)> UpdateFunction,
			const TFunctionRef<void(const FIndexSet Instances)> RewardFunction,
			const TFunctionRef<void(const FIndexSet Instances)> CompletionFunction,
			const FIndexSet Instances,
			TAtomic<bool>* bRequestTrainingStopSignal,
			FRWLock* PolicyNetworkLock,
			FRWLock* CriticNetworkLock,
			FRWLock* EncoderNetworkLock,
			FRWLock* DecoderNetworkLock,
			TAtomic<bool>* bPolicyNetworkUpdatedSignal,
			TAtomic<bool>* bCriticNetworkUpdatedSignal,
			TAtomic<bool>* bEncoderNetworkUpdatedSignal,
			TAtomic<bool>* bDecoderNetworkUpdatedSignal,
			const ELogSetting LogSettings)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Learning::PPOTrainer::Train);

			ETrainerResponse Response = ETrainerResponse::Success;

			// Send initial Policy

			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Display, TEXT("Sending initial Policy..."));
			}

			Response = Trainer.SendPolicy(PolicyNetwork, Trainer::DefaultTimeout, PolicyNetworkLock);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error sending initial policy to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}

				Trainer.Terminate();
				return Response;
			}

			// Send initial Critic

			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Display, TEXT("Sending initial Critic..."));
			}

			Response = Trainer.SendCritic(CriticNetwork, Trainer::DefaultTimeout, CriticNetworkLock);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error sending initial critic to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}

				Trainer.Terminate();
				return Response;
			}

			// Send initial Encoder

			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Display, TEXT("Sending initial Encoder..."));
			}

			Response = Trainer.SendEncoder(EncoderNetwork, Trainer::DefaultTimeout, EncoderNetworkLock);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error sending initial encoder to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}

				Trainer.Terminate();
				return Response;
			}

			// Send initial Decoder

			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Display, TEXT("Sending initial Decoder..."));
			}

			Response = Trainer.SendDecoder(DecoderNetwork, Trainer::DefaultTimeout, DecoderNetworkLock);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error sending initial decoder to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}

				Trainer.Terminate();
				return Response;
			}

			// Start Training Loop

			while (true)
			{
				if (bRequestTrainingStopSignal && (*bRequestTrainingStopSignal))
				{
					*bRequestTrainingStopSignal = false;

					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Display, TEXT("Stopping Training..."));
					}

					Response = Trainer.SendStop();

					if (Response != ETrainerResponse::Success)
					{
						if (LogSettings != ELogSetting::Silent)
						{
							UE_LOG(LogLearning, Error, TEXT("Error sending stop signal to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
						}

						Trainer.Terminate();
						return Response;
					}

					break;
				}
				else
				{
					Experience::GatherExperienceUntilReplayBufferFull(
						ReplayBuffer,
						EpisodeBuffer,
						ResetBuffer,
						ObservationVectorBuffer,
						ActionVectorBuffer,
						PreEvaluationMemoryStateVectorBuffer,
						MemoryStateVectorBuffer,
						RewardBuffer,
						CompletionBuffer,
						EpisodeCompletionBuffer,
						AllCompletionBuffer,
						ResetFunction,
						ObservationFunction,
						PolicyFunction,
						ActionFunction,
						UpdateFunction,
						RewardFunction,
						CompletionFunction,
						Instances);

					Response = Trainer.SendExperience(ReplayBuffer, Trainer::DefaultTimeout);

					if (Response != ETrainerResponse::Success)
					{
						if (LogSettings != ELogSetting::Silent)
						{
							UE_LOG(LogLearning, Error, TEXT("Error sending experience to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
						}

						Trainer.Terminate();
						return Response;
					}
				}

				// Update Policy

				Response = Trainer.RecvPolicy(PolicyNetwork, Trainer::DefaultTimeout, PolicyNetworkLock);

				if (Response == ETrainerResponse::Completed)
				{
					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Display, TEXT("Trainer completed training."));
					}
					break;
				}
				else if (Response != ETrainerResponse::Success)
				{
					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Error, TEXT("Error receiving policy from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
					}
					break;
				}

				if (bPolicyNetworkUpdatedSignal)
				{
					*bPolicyNetworkUpdatedSignal = true;
				}

				// Update Critic

				Response = Trainer.RecvCritic(CriticNetwork, Trainer::DefaultTimeout, CriticNetworkLock);

				if (Response != ETrainerResponse::Success)
				{
					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Error, TEXT("Error receiving critic from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
					}
					break;
				}

				if (bCriticNetworkUpdatedSignal)
				{
					*bCriticNetworkUpdatedSignal = true;
				}

				// Update Encoder

				Response = Trainer.RecvEncoder(EncoderNetwork, Trainer::DefaultTimeout, EncoderNetworkLock);

				if (Response != ETrainerResponse::Success)
				{
					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Error, TEXT("Error receiving encoder from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
					}
					break;
				}

				if (bEncoderNetworkUpdatedSignal)
				{
					*bEncoderNetworkUpdatedSignal = true;
				}

				// Update Decoder

				Response = Trainer.RecvDecoder(DecoderNetwork, Trainer::DefaultTimeout, DecoderNetworkLock);

				if (Response != ETrainerResponse::Success)
				{
					if (LogSettings != ELogSetting::Silent)
					{
						UE_LOG(LogLearning, Error, TEXT("Error receiving decoder from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
					}
					break;
				}

				if (bDecoderNetworkUpdatedSignal)
				{
					*bDecoderNetworkUpdatedSignal = true;
				}
			}

			// Allow some time for trainer to shut down gracefully before we kill it...
			
			Response = Trainer.Wait(Trainer::DefaultTimeout);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error waiting for trainer to exit: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}
			}

			Trainer.Terminate();

			if (LogSettings != ELogSetting::Silent)
			{
				UE_LOG(LogLearning, Display, TEXT("Training Task Done!"));
			}

			return ETrainerResponse::Success;
		}
	}
}
