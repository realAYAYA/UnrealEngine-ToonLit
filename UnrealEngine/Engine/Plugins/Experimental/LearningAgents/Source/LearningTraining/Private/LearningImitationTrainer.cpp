// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningImitationTrainer.h"

#include "LearningArray.h"
#include "LearningLog.h"
#include "LearningNeuralNetwork.h"
#include "LearningProgress.h"
#include "LearningSharedMemory.h"
#include "LearningSharedMemoryTraining.h"
#include "LearningSocketTraining.h"

#include "Misc/MonitoredProcess.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "SocketSubsystem.h"

ULearningSocketImitationTrainerServerCommandlet::ULearningSocketImitationTrainerServerCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

int32 ULearningSocketImitationTrainerServerCommandlet::Main(const FString& Commandline)
{
	UE_LOG(LogLearning, Display, TEXT("Running Imitation Training Server Commandlet..."));

#if WITH_EDITOR
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;

	UCommandlet::ParseCommandLine(*Commandline, Tokens, Switches, Params);

	const FString* PythonExecutablePathParam = Params.Find(TEXT("PythonExecutablePath"));
	const FString* ExtraSitePackagesPathParam = Params.Find(TEXT("ExtraSitePackagesPath"));
	const FString* PythonContentPathParam = Params.Find(TEXT("PythonContentPath"));
	const FString* IntermediatePathParam = Params.Find(TEXT("IntermediatePath"));
	const FString* IpAddressParam = Params.Find(TEXT("IpAddress"));
	const FString* PortParam = Params.Find(TEXT("Port"));
	const FString* LogSettingsParam = Params.Find(TEXT("LogSettings"));

	const FString PythonExecutablePath = PythonExecutablePathParam ? *PythonExecutablePathParam : UE::Learning::Trainer::GetPythonExecutablePath(FPaths::ProjectIntermediateDir());
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

	UE_LOG(LogLearning, Display, TEXT("---  Imitation Training Server Arguments ---"));
	UE_LOG(LogLearning, Display, TEXT("PythonExecutablePath: %s"), *PythonExecutablePath);
	UE_LOG(LogLearning, Display, TEXT("ExtraSitePackagesPath: %s"), *ExtraSitePackagesPath);
	UE_LOG(LogLearning, Display, TEXT("PythonContentPath: %s"), *PythonContentPath);
	UE_LOG(LogLearning, Display, TEXT("IntermediatePath: %s"), *IntermediatePath);
	UE_LOG(LogLearning, Display, TEXT("IpAddress: %s"), IpAddress);
	UE_LOG(LogLearning, Display, TEXT("Port: %i"), Port);
	UE_LOG(LogLearning, Display, TEXT("LogSettings: %s"), LogSettings == UE::Learning::ELogSetting::Normal ? TEXT("Normal") : TEXT("Silent"));

	UE::Learning::FSocketImitationTrainerServerProcess ServerProcess(
		PythonExecutablePath,
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
	FSharedMemoryImitationTrainer::FSharedMemoryImitationTrainer(
		const FString& TaskName,
		const FString& PythonExecutablePath,
		const FString& ExtraSitePackagesPath,
		const FString& PythonContentPath,
		const FString& IntermediatePath,
		const int32 MaxEpisodeNum,
		const int32 MaxStepNum,
		const int32 ObservationDimNum,
		const int32 ActionDimNum,
		const int32 MemoryStateDimNum,
		const ULearningNeuralNetworkData& PolicyNetwork,
		const ULearningNeuralNetworkData& EncoderNetwork,
		const ULearningNeuralNetworkData& DecoderNetwork,
		const Observation::FSchema& ObservationSchema,
		const Observation::FSchemaElement& ObservationSchemaElement,
		const Action::FSchema& ActionSchema,
		const Action::FSchemaElement& ActionSchemaElement,
		const FImitationTrainerTrainingSettings& TrainingSettings,
		const ESubprocessFlags TrainingProcessFlags,
		const ELogSetting LogSettings)
	{
		UE_LEARNING_CHECK(FPaths::FileExists(PythonExecutablePath));
		UE_LEARNING_CHECK(FPaths::DirectoryExists(PythonContentPath));
		UE_LEARNING_CHECK(FPaths::DirectoryExists(ExtraSitePackagesPath));

		// Allocate Shared Memory

		Policy = SharedMemory::Allocate<1, uint8>({ PolicyNetwork.GetSnapshotByteNum() });
		Encoder = SharedMemory::Allocate<1, uint8>({ EncoderNetwork.GetSnapshotByteNum() });
		Decoder = SharedMemory::Allocate<1, uint8>({ DecoderNetwork.GetSnapshotByteNum() });
		Controls = SharedMemory::Allocate<1, volatile int32>({ SharedMemoryTraining::GetControlNum() });
		EpisodeStarts = SharedMemory::Allocate<1, int32>({ MaxEpisodeNum });
		EpisodeLengths = SharedMemory::Allocate<1, int32>({ MaxEpisodeNum });
		Observations = SharedMemory::Allocate<2, float>({ MaxStepNum, ObservationDimNum });
		Actions = SharedMemory::Allocate<2, float>({ MaxStepNum, ActionDimNum });

		// We need to zero the control memory before we start
		// the training sub-process since it may contain uninitialized 
		// values or those left over from previous runs.

		Array::Zero(Controls.View);

		// Write Config

		IFileManager& FileManager = IFileManager::Get();
		const FString TimeStamp = FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d_%H-%M-%S"));
		const FString TrainerMethod = TEXT("Imitation");
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
		ConfigObject->SetStringField(TEXT("EncoderGuid"), *Encoder.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		ConfigObject->SetStringField(TEXT("DecoderGuid"), *Decoder.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		ConfigObject->SetStringField(TEXT("ControlsGuid"), *Controls.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		ConfigObject->SetStringField(TEXT("EpisodeStartsGuid"), *EpisodeStarts.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		ConfigObject->SetStringField(TEXT("EpisodeLengthsGuid"), *EpisodeLengths.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		ConfigObject->SetStringField(TEXT("ObservationsGuid"), *Observations.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		ConfigObject->SetStringField(TEXT("ActionsGuid"), *Actions.Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces));

		ConfigObject->SetObjectField(TEXT("ObservationSchema"), Trainer::ConvertObservationSchemaToJSON(ObservationSchema, ObservationSchemaElement));
		ConfigObject->SetObjectField(TEXT("ActionSchema"), Trainer::ConvertActionSchemaToJSON(ActionSchema, ActionSchemaElement));
		ConfigObject->SetNumberField(TEXT("ObservationVectorDimensionNum"), ObservationDimNum);
		ConfigObject->SetNumberField(TEXT("ActionVectorDimensionNum"), ActionDimNum);
		ConfigObject->SetNumberField(TEXT("MemoryStateVectorDimensionNum"), MemoryStateDimNum);
		ConfigObject->SetNumberField(TEXT("MaxEpisodeNum"), MaxEpisodeNum);
		ConfigObject->SetNumberField(TEXT("MaxStepNum"), MaxStepNum);

		ConfigObject->SetNumberField(TEXT("PolicyNetworkByteNum"), PolicyNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("EncoderNetworkByteNum"), EncoderNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("DecoderNetworkByteNum"), DecoderNetwork.GetSnapshotByteNum());

		ConfigObject->SetNumberField(TEXT("IterationNum"), TrainingSettings.IterationNum);
		ConfigObject->SetNumberField(TEXT("LearningRate"), TrainingSettings.LearningRate);
		ConfigObject->SetNumberField(TEXT("LearningRateDecay"), TrainingSettings.LearningRateDecay);
		ConfigObject->SetNumberField(TEXT("WeightDecay"), TrainingSettings.WeightDecay);
		ConfigObject->SetNumberField(TEXT("BatchSize"), TrainingSettings.BatchSize);
		ConfigObject->SetNumberField(TEXT("Window"), TrainingSettings.Window);
		ConfigObject->SetNumberField(TEXT("ActionRegularizationWeight"), TrainingSettings.ActionRegularizationWeight);
		ConfigObject->SetNumberField(TEXT("ActionEntropyWeight"), TrainingSettings.ActionEntropyWeight);
		ConfigObject->SetNumberField(TEXT("Seed"), TrainingSettings.Seed);
		ConfigObject->SetStringField(TEXT("Device"), Trainer::GetDeviceString(TrainingSettings.Device));
		ConfigObject->SetBoolField(TEXT("UseTensorBoard"), TrainingSettings.bUseTensorboard);
		ConfigObject->SetBoolField(TEXT("SaveSnapshots"), TrainingSettings.bSaveSnapshots);

		ConfigObject->SetBoolField(TEXT("LoggingEnabled"), LogSettings == ELogSetting::Silent ? false : true);

		FString JsonString;
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString, 0);
		FJsonSerializer::Serialize(ConfigObject, JsonWriter, true);

		FFileHelper::SaveStringToFile(JsonString, *ConfigPath);

		// Start Python Sub-process

		const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" SharedMemory \"%s\""), 
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*(PythonContentPath / TEXT("train_imitation.py"))),
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*ConfigPath));

		TrainingProcess = MakeShared<FMonitoredProcess>(
			FileManager.ConvertToAbsolutePathForExternalAppForRead(*PythonExecutablePath),
			CommandLineArguments,
			!(TrainingProcessFlags & ESubprocessFlags::ShowWindow),
			!(TrainingProcessFlags & ESubprocessFlags::NoRedirectOutput));

		if (!(TrainingProcessFlags & ESubprocessFlags::NoRedirectOutput))
		{
			TrainingProcess->OnCanceled().BindRaw(this, &FSharedMemoryImitationTrainer::HandleTrainingProcessCanceled);
			TrainingProcess->OnCompleted().BindRaw(this, &FSharedMemoryImitationTrainer::HandleTrainingProcessCompleted);
			TrainingProcess->OnOutput().BindStatic(&FSharedMemoryImitationTrainer::HandleTrainingProcessOutput);
		}

		TrainingProcess->Launch();
	}

	FSharedMemoryImitationTrainer::~FSharedMemoryImitationTrainer()
	{
		Terminate();
	}

	ETrainerResponse FSharedMemoryImitationTrainer::SendStop(const float Timeout)
	{
		return SharedMemoryTraining::SendStop(Controls.View);
	}

	bool FSharedMemoryImitationTrainer::HasPolicyOrCompleted()
	{
		return SharedMemoryTraining::HasPolicyOrCompleted(Controls.View);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::RecvPolicy(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::RecvNetwork(
			TrainingProcess.Get(),
			Controls.View,
			OutNetwork,
			SharedMemoryTraining::EControls::PolicySignal,
			Policy.View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::RecvEncoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::RecvNetwork(
			TrainingProcess.Get(),
			Controls.View,
			OutNetwork,
			SharedMemoryTraining::EControls::EncoderSignal,
			Encoder.View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::RecvDecoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::RecvNetwork(
			TrainingProcess.Get(),
			Controls.View,
			OutNetwork,
			SharedMemoryTraining::EControls::DecoderSignal,
			Decoder.View,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::SendPolicy(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendNetwork(
			TrainingProcess.Get(),
			Controls.View,
			Policy.View,
			SharedMemoryTraining::EControls::PolicySignal,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::SendEncoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendNetwork(
			TrainingProcess.Get(),
			Controls.View,
			Encoder.View,
			SharedMemoryTraining::EControls::EncoderSignal,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::SendDecoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendNetwork(
			TrainingProcess.Get(),
			Controls.View,
			Decoder.View,
			SharedMemoryTraining::EControls::DecoderSignal,
			Network,
			Timeout,
			NetworkLock,
			LogSettings);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::SendExperience(
		const TLearningArrayView<1, const int32> EpisodeStartsExperience,
		const TLearningArrayView<1, const int32> EpisodeLengthsExperience,
		const TLearningArrayView<2, const float> ObservationsExperience,
		const TLearningArrayView<2, const float> ActionsExperience,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		return SharedMemoryTraining::SendExperience(
			TrainingProcess.Get(),
			EpisodeStarts.View,
			EpisodeLengths.View,
			Observations.View,
			Actions.View,
			Controls.View,
			EpisodeStartsExperience,
			EpisodeLengthsExperience,
			ObservationsExperience,
			ActionsExperience,
			Timeout,
			LogSettings);
	}

	void FSharedMemoryImitationTrainer::Deallocate()
	{
		SharedMemory::Deallocate(Policy);
		SharedMemory::Deallocate(Encoder);
		SharedMemory::Deallocate(Decoder);
		SharedMemory::Deallocate(Controls);
		SharedMemory::Deallocate(Observations);
		SharedMemory::Deallocate(Actions);
	}

	ETrainerResponse FSharedMemoryImitationTrainer::Wait(float Timeout)
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

	void FSharedMemoryImitationTrainer::Terminate()
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

	void FSharedMemoryImitationTrainer::HandleTrainingProcessCanceled()
	{
		UE_LOG(LogLearning, Warning, TEXT("Training process canceled"));

		TrainingProcess.Reset();
	}

	void FSharedMemoryImitationTrainer::HandleTrainingProcessCompleted(int32 ReturnCode)
	{
		if (ReturnCode != 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("Training Process finished with warnings or errors"));
		}

		TrainingProcess.Reset();
	}

	void FSharedMemoryImitationTrainer::HandleTrainingProcessOutput(FString Output)
	{
		if (!Output.IsEmpty())
		{
			UE_LOG(LogLearning, Display, TEXT("Training Process: %s"), *Output);
		}
	}


	FSocketImitationTrainerServerProcess::FSocketImitationTrainerServerProcess(
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
		UE_LEARNING_CHECK(FPaths::DirectoryExists(ExtraSitePackagesPath));

		IFileManager& FileManager = IFileManager::Get();
		const FString CommandLineArguments = FString::Printf(TEXT("\"%s\" Socket \"%s:%i\" \"%s\" \"%s\" %i"),
			*FileManager.ConvertToAbsolutePathForExternalAppForRead(*(PythonContentPath / TEXT("train_imitation.py"))), 
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
			TrainingProcess->OnCanceled().BindRaw(this, &FSocketImitationTrainerServerProcess::HandleTrainingProcessCanceled);
			TrainingProcess->OnCompleted().BindRaw(this, &FSocketImitationTrainerServerProcess::HandleTrainingProcessCompleted);
			TrainingProcess->OnOutput().BindStatic(&FSocketImitationTrainerServerProcess::HandleTrainingProcessOutput);
		}

		TrainingProcess->Launch();
	}

	FSocketImitationTrainerServerProcess::~FSocketImitationTrainerServerProcess()
	{
		Terminate();
	}

	bool FSocketImitationTrainerServerProcess::IsRunning() const
	{
		return TrainingProcess.IsValid();
	}

	bool FSocketImitationTrainerServerProcess::Wait(float Timeout)
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

	void FSocketImitationTrainerServerProcess::Terminate()
	{
		if (TrainingProcess.IsValid())
		{
			TrainingProcess->Cancel(true);
		}

		TrainingProcess.Reset();
	}

	void FSocketImitationTrainerServerProcess::HandleTrainingProcessCanceled()
	{
		UE_LOG(LogLearning, Warning, TEXT("Training process canceled"));

		TrainingProcess.Reset();
	}

	void FSocketImitationTrainerServerProcess::HandleTrainingProcessCompleted(int32 ReturnCode)
	{
		if (ReturnCode != 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("Training Process finished with warnings or errors"));
		}

		TrainingProcess.Reset();
	}

	void FSocketImitationTrainerServerProcess::HandleTrainingProcessOutput(FString Output)
	{
		if (!Output.IsEmpty())
		{
			UE_LOG(LogLearning, Display, TEXT("Training Process: %s"), *Output);
		}
	}

	FSocketImitationTrainer::FSocketImitationTrainer(
		ETrainerResponse& OutResponse,
		const FString& TaskName,
		const int32 MaxEpisodeNum,
		const int32 MaxStepNum,
		const int32 ObservationDimNum,
		const int32 ActionDimNum,
		const int32 MemoryStateDimNum,
		const ULearningNeuralNetworkData& PolicyNetwork,
		const ULearningNeuralNetworkData& EncoderNetwork,
		const ULearningNeuralNetworkData& DecoderNetwork,
		const Observation::FSchema& ObservationSchema,
		const Observation::FSchemaElement& ObservationSchemaElement,
		const Action::FSchema& ActionSchema,
		const Action::FSchemaElement& ActionSchemaElement,
		const TCHAR* IpAddress,
		const uint32 Port,
		const float Timeout,
		const FImitationTrainerTrainingSettings& TrainingSettings)
	{
		// Write Config

		const FString TimeStamp = FDateTime::Now().ToFormattedString(TEXT("%Y-%m-%d_%H-%M-%S"));
		const FString TrainerMethod = TEXT("RL");
		const FString TrainerType = TEXT("Network");

		TSharedRef<FJsonObject> ConfigObject = MakeShared<FJsonObject>();
		ConfigObject->SetStringField(TEXT("TaskName"), *TaskName);
		ConfigObject->SetStringField(TEXT("TrainerMethod"), TrainerMethod);
		ConfigObject->SetStringField(TEXT("TrainerType"), TrainerType);
		ConfigObject->SetStringField(TEXT("TimeStamp"), *TimeStamp);

		ConfigObject->SetObjectField(TEXT("ObservationSchema"), Trainer::ConvertObservationSchemaToJSON(ObservationSchema, ObservationSchemaElement));
		ConfigObject->SetObjectField(TEXT("ActionSchema"), Trainer::ConvertActionSchemaToJSON(ActionSchema, ActionSchemaElement));
		ConfigObject->SetNumberField(TEXT("ObservationVectorDimensionNum"), ObservationDimNum);
		ConfigObject->SetNumberField(TEXT("ActionVectorDimensionNum"), ActionDimNum);
		ConfigObject->SetNumberField(TEXT("MemoryStateVectorDimensionNum"), MemoryStateDimNum);
		ConfigObject->SetNumberField(TEXT("MaxEpisodeNum"), MaxEpisodeNum);
		ConfigObject->SetNumberField(TEXT("MaxStepNum"), MaxStepNum);

		ConfigObject->SetNumberField(TEXT("PolicyNetworkByteNum"), PolicyNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("EncoderNetworkByteNum"), EncoderNetwork.GetSnapshotByteNum());
		ConfigObject->SetNumberField(TEXT("DecoderNetworkByteNum"), DecoderNetwork.GetSnapshotByteNum());

		ConfigObject->SetNumberField(TEXT("IterationNum"), TrainingSettings.IterationNum);
		ConfigObject->SetNumberField(TEXT("LearningRate"), TrainingSettings.LearningRate);
		ConfigObject->SetNumberField(TEXT("LearningRateDecay"), TrainingSettings.LearningRateDecay);
		ConfigObject->SetNumberField(TEXT("WeightDecay"), TrainingSettings.WeightDecay);
		ConfigObject->SetNumberField(TEXT("BatchSize"), TrainingSettings.BatchSize);
		ConfigObject->SetNumberField(TEXT("Window"), TrainingSettings.Window);
		ConfigObject->SetNumberField(TEXT("ActionRegularizationWeight"), TrainingSettings.ActionRegularizationWeight);
		ConfigObject->SetNumberField(TEXT("ActionEntropyWeight"), TrainingSettings.ActionEntropyWeight);
		ConfigObject->SetNumberField(TEXT("Seed"), TrainingSettings.Seed);
		ConfigObject->SetStringField(TEXT("Device"), Trainer::GetDeviceString(TrainingSettings.Device));
		ConfigObject->SetBoolField(TEXT("UseTensorBoard"), TrainingSettings.bUseTensorboard);
		ConfigObject->SetBoolField(TEXT("SaveSnapshots"), TrainingSettings.bSaveSnapshots);

		FString JsonString;
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString, 0);
		FJsonSerializer::Serialize(ConfigObject, JsonWriter, true);

		// Allocate buffer to receive network data in

		PolicyBuffer.SetNumUninitialized({ PolicyNetwork.GetSnapshotByteNum() });
		EncoderBuffer.SetNumUninitialized({ EncoderNetwork.GetSnapshotByteNum() });
		DecoderBuffer.SetNumUninitialized({ DecoderNetwork.GetSnapshotByteNum() });

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

		Socket = FTcpSocketBuilder(TEXT("LearningNetworkRLTrainerSocket")).AsNonBlocking().Build();
		Socket->Connect(*Address);

		OutResponse = SocketTraining::WaitForConnection(*Socket, Timeout);
		if (OutResponse != ETrainerResponse::Success) { return; }


		// Send Config

		OutResponse = SocketTraining::SendConfig(*Socket, JsonString, Timeout);
		return;
	}

	FSocketImitationTrainer::~FSocketImitationTrainer()
	{
		Terminate();
	}

	ETrainerResponse FSocketImitationTrainer::Wait(const float Timeout)
	{
		return ETrainerResponse::Success;
	}

	void FSocketImitationTrainer::Terminate()
	{
		if (Socket)
		{
			Socket->Close();
			Socket = nullptr;
		}
	}

	ETrainerResponse FSocketImitationTrainer::SendStop(const float Timeout)
	{
		return SocketTraining::SendStop(*Socket, Timeout);
	}

	bool FSocketImitationTrainer::HasPolicyOrCompleted()
	{
		return SocketTraining::HasPolicyOrCompleted(*Socket);
	}

	ETrainerResponse FSocketImitationTrainer::RecvPolicy(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::RecvNetwork(*Socket, OutNetwork, PolicyBuffer, SocketTraining::ESignal::RecvPolicy, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketImitationTrainer::RecvEncoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::RecvNetwork(*Socket, OutNetwork, EncoderBuffer, SocketTraining::ESignal::RecvEncoder, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketImitationTrainer::RecvDecoder(
		ULearningNeuralNetworkData& OutNetwork,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::RecvNetwork(*Socket, OutNetwork, DecoderBuffer, SocketTraining::ESignal::RecvDecoder, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketImitationTrainer::SendPolicy(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendNetwork(*Socket, PolicyBuffer, SocketTraining::ESignal::SendPolicy, Network, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketImitationTrainer::SendEncoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendNetwork(*Socket, EncoderBuffer, SocketTraining::ESignal::SendEncoder, Network, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketImitationTrainer::SendDecoder(
		const ULearningNeuralNetworkData& Network,
		const float Timeout,
		FRWLock* NetworkLock,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendNetwork(*Socket, DecoderBuffer, SocketTraining::ESignal::SendDecoder, Network, Timeout, NetworkLock, LogSettings);
	}

	ETrainerResponse FSocketImitationTrainer::SendExperience(
		const TLearningArrayView<1, const int32> EpisodeStartsExperience,
		const TLearningArrayView<1, const int32> EpisodeLengthsExperience,
		const TLearningArrayView<2, const float> ObservationsExperience,
		const TLearningArrayView<2, const float> ActionsExperience,
		const float Timeout,
		const ELogSetting LogSettings)
	{
		return SocketTraining::SendExperience(
			*Socket, 
			EpisodeStartsExperience,
			EpisodeLengthsExperience,
			ObservationsExperience,
			ActionsExperience,
			Timeout, 
			LogSettings);
	}

	namespace ImitationTrainer
	{
		ETrainerResponse Train(
			IImitationTrainer& Trainer,
			ULearningNeuralNetworkData& PolicyNetwork,
			ULearningNeuralNetworkData& EncoderNetwork,
			ULearningNeuralNetworkData& DecoderNetwork,
			const TLearningArrayView<1, const int32> EpisodeStartsExperience,
			const TLearningArrayView<1, const int32> EpisodeLengthsExperience,
			const TLearningArrayView<2, const float> ObservationsExperience,
			const TLearningArrayView<2, const float> ActionsExperience,
			TAtomic<bool>* bRequestTrainingStopSignal,
			FRWLock* PolicyNetworkLock,
			FRWLock* EncoderNetworkLock,
			FRWLock* DecoderNetworkLock,
			TAtomic<bool>* bPolicyNetworkUpdatedSignal,
			TAtomic<bool>* bEncoderNetworkUpdatedSignal,
			TAtomic<bool>* bDecoderNetworkUpdatedSignal,
			const ELogSetting LogSettings)
		{
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
					UE_LOG(LogLearning, Error, TEXT("Error sending initial policy from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
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
					UE_LOG(LogLearning, Error, TEXT("Error sending initial encoder from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
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
					UE_LOG(LogLearning, Error, TEXT("Error sending initial decoder from trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
				}

				Trainer.Terminate();
				return Response;
			}

			// Send Experience

			Response = Trainer.SendExperience(
				EpisodeStartsExperience,
				EpisodeLengthsExperience,
				ObservationsExperience, 
				ActionsExperience, 
				Trainer::DefaultTimeout);

			if (Response != ETrainerResponse::Success)
			{
				if (LogSettings != ELogSetting::Silent)
				{
					UE_LOG(LogLearning, Error, TEXT("Error sending experience to trainer: %s. Check log for errors."), Trainer::GetResponseString(Response));
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
				
				if (Trainer.HasPolicyOrCompleted())
				{
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
				else
				{
					FPlatformProcess::Sleep(0.001f);
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
