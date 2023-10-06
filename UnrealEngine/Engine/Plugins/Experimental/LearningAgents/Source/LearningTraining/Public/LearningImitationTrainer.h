// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningLog.h"
#include "LearningTrainer.h"
#include "LearningSharedMemory.h"
#include "LearningNeuralNetwork.h" // Included for EActivationFunction::ELU

#include "Commandlets/Commandlet.h"
#include "Templates/SharedPointer.h"

#include "LearningImitationTrainer.generated.h"

class FSocket;
class FMonitoredProcess;

UCLASS()
class LEARNINGTRAINING_API ULearningSocketImitationTrainerServerCommandlet : public UCommandlet
{
	GENERATED_BODY()

	ULearningSocketImitationTrainerServerCommandlet(const FObjectInitializer& ObjectInitializer);

	/** Runs the commandlet */
	virtual int32 Main(const FString& Params) override;
};

namespace UE::Learning
{
	/**
	* Settings for the network used for training. These settings must match the Neural Network
	* objects passed to ImitationTrainer::Train.
	*/
	struct FImitationTrainerNetworkSettings
	{
		/** Minimum action noise used by the policy */
		float PolicyActionNoiseMin = 0.25f;

		/** Maximum action noise used by the policy */
		float PolicyActionNoiseMax = 0.25f;

		/** Total layers for policy network including input, hidden, and output layers */
		int32 PolicyLayerNum = 3;

		/** Number of neurons in each hidden layer of the policy network */
		int32 PolicyHiddenLayerSize = 128;

		/** Activation function to use on hidden layers of the policy network */
		EActivationFunction PolicyActivationFunction = EActivationFunction::ELU;
	};

	struct FImitationTrainerTrainingSettings
	{
		// Number of iterations to train the network for. Controls the overall training time.
		// Training for about 100000 iterations should give you well trained network, but
		// closer to 1000000 iterations or more is required for an exhaustively trained network.
		uint32 IterationNum = 1000000;

		// Learning rate of the actor network. Typical values are between 0.001f and 0.0001f
		float LearningRateActor = 0.0001f;

		// Ratio by which to decay the learning rate every 1000 iterations.
		float LearningRateDecay = 0.99f;

		// Amount of weight decay to apply to the network. Larger values encourage network 
		// weights to be smaller.
		float WeightDecay = 0.001f;

		// Batch size to use for training. Smaller values tend to produce better results 
		// at the cost of slowing down training.
		uint32 BatchSize = 128;

		// Random seed to use for training
		uint32 Seed = 1234;

		// Which device to use for training
		ETrainerDevice Device = ETrainerDevice::GPU;

		// If to use TensorBoard for logging and tracking the training progress.
		// 
		// Even when enabled, TensorBoard will only work if it is installed in your Unreal Editor
		// bundled version of Python, which is not the case by default. TensorBoard can be installed 
		// for this version of Python by going to your Unreal Editor Python Binaries directory 
		// (e.g. "\Engine\Binaries\ThirdParty\Python3\Win64") and running `python -m pip install tensorboard`. 
		bool bUseTensorboard = false;
	};

	/**
	* ImitationTrainer flags controlling some aspects of the process of communication with the trainer
	*/
	enum class EImitationTrainerFlags : uint8
	{
		None = 0,

		// If to send over the initial provided policy network rather than reinitialize it from random weights at 
		// the start of training. Use this if you want to start from a network which has already been trained.
		UseInitialPolicyNetwork = 1 << 0,
	};
	ENUM_CLASS_FLAGS(EImitationTrainerFlags)

	/**
	* Interface for an object which can train a policy from experience using imitation learning.
	*/
	struct IImitationTrainer
	{
		virtual ~IImitationTrainer() {}

		/**
		* Terminate the trainer immediately.
		*/
		virtual void Terminate() = 0;

		/**
		* Wait for the trainer to finish.
		* 
		* @param Timeout		Timeout to wait in seconds
		* @returns				Trainer response
		*/
		virtual ETrainerResponse Wait(const float Timeout = Trainer::DefaultTimeout) = 0;

		/*
		* Signal for the trainer to stop.
		* 
		* @param Timeout		Timeout to wait in seconds
		* @returns				Trainer response
		*/
		virtual ETrainerResponse SendStop(const float Timeout = Trainer::DefaultTimeout) = 0;

		/*
		* Check if a new policy is available or if training is complete.
		* 
		* @returns				If there is a new policy ready or training is complete.
		*/
		virtual bool HasPolicyOrCompleted() = 0;

		/**
		* Wait for the trainer and pull an updated policy.
		*
		* @param OutNetwork		Network to update
		* @param Timeout		Timeout to wait in seconds
		* @param NetworkLock	Lock to use when updating network
		* @param LogSettings	Log settings
		* @returns				Trainer response
		*/
		virtual ETrainerResponse RecvPolicy(
			FNeuralNetwork& OutNetwork,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) = 0;

		/**
		* Wait for the training process to be ready and push an updated policy to the shared memory.
		*
		* @param Network		Network to push
		* @param Timeout		Timeout to wait in seconds
		* @param NetworkLock	Lock to use when pushing network
		* @param LogSettings	Log settings
		* @returns				Trainer response
		*/
		virtual ETrainerResponse SendPolicy(
			const FNeuralNetwork& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) = 0;

		/**
		* Push experience to the trainer.
		*
		* @param ObservationVectors		Set of observation vectors
		* @param ActionVectors			Set of action vectors
		* @param Timeout				Timeout to wait in seconds
		* @param LogSettings			Log settings
		* @returns						Trainer response
		*/
		virtual ETrainerResponse SendExperience(
			const TLearningArrayView<2, const float> ObservationVectors,
			const TLearningArrayView<2, const float> ActionVectors,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) = 0;
	};

	/**
	* Object used to create and communicate with the Python training sub-process
	*/
	struct LEARNINGTRAINING_API FSharedMemoryImitationTrainer : public IImitationTrainer
	{
		/**
		* Create a new imitation trainer sub-process
		*
		* @param TaskName				Name of the training task - used to help identify the logs, snapshots, and other files generated by training
		* @param PythonExecutablePath	Path to the python executable used for training
		* @param SitePackagesPath		Path to the site-packages shipped with the PythonFoundationPackages plugin
		* @param PythonContentPath		Path to the Python Content folder provided by the Learning plugin
		* @param IntermediatePath		Path to the intermediate folder to write temporary files, logs, and snapshots to
		* @param MaxSampleNum			Maximum number of samples in the training data
		* @param ObservationDimNum		Number of dimensions in the observation vector
		* @param ActionDimNum			Number of dimensions in the action vector
		* @param TrainingSettings		Trainer Training settings
		* @param NetworkSettings		Trainer Network settings
		* @param TrainingProcessFlags	Training subprocess flags
		* @param LogSettings			Logging settings to use
		*/
		FSharedMemoryImitationTrainer(
			const FString& TaskName,
			const FString& PythonExecutablePath,
			const FString& SitePackagesPath,
			const FString& PythonContentPath,
			const FString& IntermediatePath,
			const int32 MaxSampleNum,
			const int32 ObservationDimNum,
			const int32 ActionDimNum,
			const FImitationTrainerTrainingSettings& TrainingSettings = FImitationTrainerTrainingSettings(),
			const FImitationTrainerNetworkSettings& NetworkSettings = FImitationTrainerNetworkSettings(),
			const EImitationTrainerFlags TrainerFlags = EImitationTrainerFlags::None,
			const ESubprocessFlags TrainingProcessFlags = ESubprocessFlags::None,
			const ELogSetting LogSettings = ELogSetting::Normal);

		~FSharedMemoryImitationTrainer();

		virtual void Terminate() override final;

		virtual ETrainerResponse Wait(const float Timeout = Trainer::DefaultTimeout) override final;

		virtual ETrainerResponse SendStop(const float Timeout = Trainer::DefaultTimeout) override final;

		virtual bool HasPolicyOrCompleted() override final;

		virtual ETrainerResponse RecvPolicy(
			FNeuralNetwork& OutNetwork,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual ETrainerResponse SendPolicy(
			const FNeuralNetwork& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual ETrainerResponse SendExperience(
			const TLearningArrayView<2, const float> ObservationVectors,
			const TLearningArrayView<2, const float> ActionVectors,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

	private:

		/**
		* Free and deallocate all shared memory
		*/
		void Deallocate();

		// Shared memory

		UE::Learning::TSharedMemoryArrayView<1, uint8> Policy;
		UE::Learning::TSharedMemoryArrayView<1, volatile int32> Controls; // Mark as volatile to avoid compiler optimizing away reads without writes etc.
		UE::Learning::TSharedMemoryArrayView<2, float> Observations;
		UE::Learning::TSharedMemoryArrayView<2, float> Actions;

		// Training Process

		TSharedPtr<FMonitoredProcess> TrainingProcess;

		void HandleTrainingProcessCanceled();
		void HandleTrainingProcessCompleted(int32 ReturnCode);
		static void HandleTrainingProcessOutput(FString Output);
	};

	/**
	* This object allows you to launch the FSocketImitationTrainer server as a subprocess,
	* which is convenient when you want to train using it locally.
	*/
	struct LEARNINGTRAINING_API FSocketImitationTrainerServerProcess
	{
		/**
		* Creates a training server as a subprocess
		*
		* @param PythonExecutablePath		Path to the python executable used for training. In general should be the python shipped with Unreal Editor.
		* @param SitePackagesPath			Path to the site-packages shipped with the PythonFoundationPackages plugin
		* @param PythonContentPath			Path to the Python Content folder provided by the Learning plugin
		* @param IntermediatePath			Path to the intermediate folder to write temporary files, logs, and snapshots to
		* @param IpAddress					Ip address to bind the listening socket to. For a local server you will want to use 127.0.0.1
		* @param Port						Port to use for the listening socket.
		* @param TrainingProcessFlags		Training server subprocess flags
		* @param LogSettings				Logging settings to use
		*/
		FSocketImitationTrainerServerProcess(
			const FString& PythonExecutablePath,
			const FString& SitePackagesPath,
			const FString& PythonContentPath,
			const FString& IntermediatePath,
			const TCHAR* IpAddress = Trainer::DefaultIp,
			const uint32 Port = Trainer::DefaultPort,
			const ESubprocessFlags TrainingProcessFlags = ESubprocessFlags::None,
			const ELogSetting LogSettings = ELogSetting::Normal);

		~FSocketImitationTrainerServerProcess();

		/**
		* Check if the server process is still running
		*/
		bool IsRunning() const;

		/**
		* Wait for the server process to end
		*
		* @param Timeout		Timeout to wait in seconds
		* @returns				true if successful, otherwise false if it times out
		*/
		bool Wait(float Timeout);

		/**
		* Terminate the server process
		*/
		void Terminate();

	private:

		TSharedPtr<FMonitoredProcess> TrainingProcess;

		void HandleTrainingProcessCanceled();
		void HandleTrainingProcessCompleted(int32 ReturnCode);
		static void HandleTrainingProcessOutput(FString Output);
	};

	struct LEARNINGTRAINING_API FSocketImitationTrainer : public IImitationTrainer
	{
		/**
		* Create a new socket-based imitation trainer 
		*
		* @param OutResponse				Response to the initial connection
		* @param TaskName					Name of the training task - used to help identify the logs, snapshots, and other files generated by training
		* @param MaxSampleNum			Maximum number of samples in the training data
		* @param ObservationDimNum		Number of dimensions in the observation vector
		* @param ActionDimNum			Number of dimensions in the action vector
		* @param IpAddress					Server Ip address
		* @param Port						Server Port
		* @param Timeout					Timeout to wait in seconds for connection and initial data transfer
		* @param TrainingSettings			Trainer Training settings
		* @param NetworkSettings			Trainer Network settings
		* @param TrainerFlags				Flags for the trainer
		*/
		FSocketImitationTrainer(
			ETrainerResponse& OutResponse,
			const FString& TaskName,
			const int32 MaxSampleNum,
			const int32 ObservationDimNum,
			const int32 ActionDimNum,
			const TCHAR* IpAddress = Trainer::DefaultIp,
			const uint32 Port = Trainer::DefaultPort,
			const float Timeout = Trainer::DefaultTimeout,
			const FImitationTrainerTrainingSettings& TrainingSettings = FImitationTrainerTrainingSettings(),
			const FImitationTrainerNetworkSettings& NetworkSettings = FImitationTrainerNetworkSettings(),
			const EImitationTrainerFlags TrainerFlags = EImitationTrainerFlags::None);

		~FSocketImitationTrainer();

		virtual void Terminate() override final;

		virtual ETrainerResponse Wait(const float Timeout = Trainer::DefaultTimeout) override final;

		virtual ETrainerResponse SendStop(const float Timeout = Trainer::DefaultTimeout) override final;

		virtual bool HasPolicyOrCompleted() override final;

		virtual ETrainerResponse RecvPolicy(
			FNeuralNetwork& OutNetwork,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual ETrainerResponse SendPolicy(
			const FNeuralNetwork& Network,
			const float Timeout = Trainer::DefaultTimeout,
			FRWLock* NetworkLock = nullptr,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

		virtual ETrainerResponse SendExperience(
			const TLearningArrayView<2, const float> ObservationVectors,
			const TLearningArrayView<2, const float> ActionVectors,
			const float Timeout = Trainer::DefaultTimeout,
			const ELogSetting LogSettings = Trainer::DefaultLogSettings) override final;

	private:

		TLearningArray<1, uint8> NetworkBuffer;
		FSocket* Socket = nullptr;
	};

	namespace ImitationTrainer
	{
		/**
		* Train a policy using experience already gathered from example episodes
		*
		* @param Trainer							Trainer
		* @param Network							Policy network
		* @param ObservationVectors					Observation Data
		* @param ActionVectors						Action Data
		* @param TrainerFlags						Flags for the trainer, should match what was used to initialize the Trainer object.
		* @param bRequestTrainingStopSignal			Optional signal that can be raised to indicate training should be stopped
		* @param NetworkLock						Optional Lock to use when updating the policy network
		* @param bNetworkUpdatedSignal				Optional signal that will be raised when the policy network is updated
		* @param LogSettings						Logging settings
		* @returns									Trainer response in case of errors during communication otherwise Success
		*/
		LEARNINGTRAINING_API ETrainerResponse Train(
			IImitationTrainer& Trainer,
			FNeuralNetwork& Network,
			const TLearningArrayView<2, const float> ObservationVectors,
			const TLearningArrayView<2, const float> ActionVectors,
			const EImitationTrainerFlags TrainerFlags = EImitationTrainerFlags::None,
			TAtomic<bool>* bRequestTrainingStopSignal = nullptr,
			FRWLock* NetworkLock = nullptr,
			TAtomic<bool>* bNetworkUpdatedSignal = nullptr,
			const ELogSetting LogSettings = ELogSetting::Normal);
	}

}
