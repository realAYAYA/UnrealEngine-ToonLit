// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/Dsp.h"

class FEvent;

namespace Audio
{

	
	/**
	 * This class can be thought of as an output for a single constructed instance of FPatchInput.
	 * Each FPatchOutput can only be connected to one FPatchInput. To route multiple outputs, see FPatchSplitter.
	 * To route multiple inputs, see FPatchMixer.
	 *
	 * Example usage:
	 * 
	 * FPatchOutputStrongPtr NewOutput(new FPatchOutput(4096));
	 * FPatchInput NewInput(NewOutput);
	 *
	 * // On one thread, push audio to the output:
	 * NewInput.PushAudio(AudioBufferPtr, AudioBufferNumSamples);
	 *
	 * // and on a seperate thread, retrieve the audio:
	 * NewOutput->PopAudio(OutAudioBufferPtr, AudioBufferNumSamples);
	 * 
	 */
	struct SIGNALPROCESSING_API FPatchOutput
	{
	public:
		FPatchOutput(int32 InMaxCapacity, float InGain = 1.0f);

		/** The default constructor will result in an uninitialized, disconnected patch point. */
		FPatchOutput();

		virtual ~FPatchOutput();
		
		/** Copies the minimum of NumSamples or however many samples are available into OutBuffer. Returns the number of samples copied, or -1 if this output's corresponding input has been destroyed. */
		int32 PopAudio(float* OutBuffer, int32 NumSamples, bool bUseLatestAudio);

		/** Sums the minimum of NumSamples or however many samples are available into OutBuffer. Returns the number of samples summed into OutBuffer. */
		int32 MixInAudio(float* OutBuffer, int32 NumSamples, bool bUseLatestAudio);

		/** Returns the current number of samples buffered on this output. */
		int32 GetNumSamplesAvailable() const;

		/** Pauses the current thread until there are the given number of samples available to pop. Will return true if it succeeded, false if it timed out. */
		bool WaitUntilNumSamplesAvailable(int32 NumSamples, uint32 TimeOutMilliseconds = MAX_uint32);
		
		/** Returns true if the input for this patch has been destroyed. */
		bool IsInputStale() const;

		friend class FPatchInput;
		friend class FPatchMixer;
		friend class FPatchSplitter;
	private:
		
		int32 PushAudioToInternalBuffer(const float* InBuffer, int32 NumSamples);
		
		// Internal buffer.
		TCircularAudioBuffer<float> InternalBuffer;

		// For MixInAudio, audio is popped off of InternalBuffer onto here, and then mixed into OutBuffer in MixInAudio.
		FAlignedFloatBuffer MixingBuffer;

		// This is applied in PopAudio or MixInAudio.
		TAtomic<float> TargetGain;
		float PreviousGain;

		// This is used to breadcrumb the FPatchOutput when we want to delete it.
		int32 PatchID;

		// Counter that is incremented/decremented to allow FPatchInput to be copied around safely.
		int32 NumAliveInputs;

		// Event to pause the current thread until a given number of samples has been filled
		FEvent* SamplesFilledEvent;
		int32 NumSamplesToWaitFor;
		
		static TAtomic<int32> PatchIDCounter;
	};

	/** Patch outputs are owned by the FPatchMixer, and are pinned by the FPatchInput. */
	typedef TSharedPtr<FPatchOutput, ESPMode::ThreadSafe> FPatchOutputStrongPtr;
	typedef TWeakPtr<FPatchOutput, ESPMode::ThreadSafe> FPatchOutputWeakPtr;

	/**
	 * Handle to a patch. Should only be used by a single thread.
	 */
	class SIGNALPROCESSING_API FPatchInput
	{
	public:
		/** PatchInputs can only be created from explicit outputs. */
		FPatchInput(const FPatchOutputStrongPtr& InOutput);
		FPatchInput(const FPatchInput& Other);
		FPatchInput& operator=(const FPatchInput& Other);
		FPatchInput& operator=(FPatchInput&& Other);
		FPatchInput(FPatchInput&& Other);

		/** Default constructed FPatchInput instances will always return -1 for PushAudio and true for IsOutputStillActive. */
		FPatchInput() = default;

		~FPatchInput();

		/** pushes audio from InBuffer to the corresponding FPatchOutput.
		 *  Returns how many samples were able to be pushed, or -1 if the output was disconnected.
		 */
		int32 PushAudio(const float* InBuffer, int32 NumSamples);

		void SetGain(float InGain);

		/** Returns false if this output was removed, either because someone called FPatchMixer::RemoveTap with this FPatchInput, or the FPatchMixer was destroyed. */
		bool IsOutputStillActive();

		friend class FPatchMixer;
		friend class FPatchSplitter;

	private:
		/** Strong pointer to our destination buffer. */
		FPatchOutputStrongPtr OutputHandle;

		/** Counter of the number of push calls. */
		int32 PushCallsCounter = 0;
	};

	/**
	 * This class is used for retrieving and mixing down audio from multiple threads.
	 * Important to note that this is MPSC: while multiple threads can enqueue audio on an instance of FPatchMixer using instances of FPatchInput,
	 * only one thread can call PopAudio safely.
	 */
	class SIGNALPROCESSING_API FPatchMixer
	{
	public:
		/** Adds a new input to the tap collector. Calling this is thread safe, but individual instances of FPatchInput are only safe to be used from one thread. */
		FPatchInput AddNewInput(int32 MaxLatencyInSamples, float InGain);

		/** Adds an existing patch input to the patch mixer. */
		void AddNewInput(const FPatchInput& InPatchInput);

		/** Removes a tap from the tap collector. Calling this is thread safe, though FPatchOutput will likely not be deleted until the next call of PopAudio. */
		void RemovePatch(const FPatchInput& InPatchInput);

		/** Mixes all inputs into a single buffer. This should only be called from a single thread. Returns the number of non-silent samples popped to OutBuffer. */
		int32 PopAudio(float* OutBuffer, int32 OutNumSamples, bool bUseLatestAudio);

		/** This returns the number of inputs currently connected to this patch mixer. Thread safe, but blocks for PopAudio. */
		int32 Num();

		/** This function call gets the maximum number of samples that's safe to pop, based on the thread with the least amount of samples buffered. Thread safe, but blocks for PopAudio. */
		int32 MaxNumberOfSamplesThatCanBePopped();

		/** Disconnect everything currently connected to this mixer. */
		void DisconnectAllInputs();

	private:
		/** Called within PopAudio. Flushes the PendingNewPatches array into CurrentPatches. During this function, AddNewPatch is blocked. */
		void ConnectNewPatches();

		/** Called within PopAudio and MaxNumberOfSamplesThatCanBePopped. Removes PendingTapsToDelete from CurrentPatches and ConnectNewPatches. 
		 * During this function, RemoveTap and AddNewPatch are blocked. Callers of this function must have CurrentPatchesCriticalSection locked. */
		void CleanUpDisconnectedPatches();

		/** New taps are added here in AddNewPatch, and then are moved to CurrentPatches in ConnectNewPatches. */
		TArray<FPatchOutputStrongPtr> PendingNewInputs;
		/** Contended by AddNewPatch, ConnectNewPatches and CleanUpDisconnectedTaps. */
		FCriticalSection PendingNewInputsCriticalSection;

		/** Patch IDs of individual audio taps that will be removed on the next call of CleanUpDisconnectedPatches. */
		TArray<int32> DisconnectedInputs;
		/** Contended by RemoveTap, AddNewPatch, and ConnectNewPatches. */
		FCriticalSection InputDeletionCriticalSection;

		/** Only accessed within PopAudio. Indirect array of taps that are mixed in during PopAudio. */
		TArray<FPatchOutputStrongPtr> CurrentInputs;
		FCriticalSection CurrentPatchesCriticalSection;
	};

	/**
	 * This class is used to post audio from one source to multiple threads.
	 * This class is SPMC: multiple threads can call FPatchOutputStrongPtr->PopAudio safely,
	 * but only one thread can call PushAudio.
	 */
	class SIGNALPROCESSING_API FPatchSplitter
	{
	public:
		/**
		 * Adds a new output. Calling this is thread safe, but individual instances of FPatchOutput are only safe to be used from one thread.
		 * the returned FPatchOutputPtr can be safely destroyed at any point.
		 */
		FPatchOutputStrongPtr AddNewPatch(int32 MaxLatencyInSamples, float InGain);

		/** Adds a new a patch from an existing patch output. */
		void AddNewPatch(FPatchOutputStrongPtr&& InPatchOutputStrongPtr);
		void AddNewPatch(const FPatchOutputStrongPtr& InPatchOutputStrongPtr);

		/** This call pushes audio to all outputs connected to this splitter. Only should be called from one thread. */
		int32 PushAudio(const float* InBuffer, int32 InNumSamples);

		/** This returns the number of outputs currently connected to this patch splitter. Thread safe, but blocks for PushAudio. */
		int32 Num();

		/** This function call gets the maximum number of samples that's safe to push. Thread safe, but blocks for PushAudio. */
		int32 MaxNumberOfSamplesThatCanBePushed();

	private:
		void AddPendingPatches();

		TArray<FPatchInput> PendingOutputs;
		FCriticalSection PendingOutputsCriticalSection;

		TArray<FPatchInput> ConnectedOutputs;
		FCriticalSection ConnectedOutputsCriticalSection;
	};

	/**
	 * This class is used to mix multiple inputs from disparate threads to a single mixdown and deliver that mixdown to multiple outputs.
	 * This class is MPMC, but only one thread can and should call ProcessAudio().
	 */
	class SIGNALPROCESSING_API FPatchMixerSplitter
	{
	public:
		virtual ~FPatchMixerSplitter() = default;

		/**
		 * Adds a new output. Calling this is thread safe, but individual instances of FPatchOutput are only safe to be used from one thread.
		 * the returned FPatchOutputPtr can be safely destroyed at any point.
		 */
		FPatchOutputStrongPtr AddNewOutput(int32 MaxLatencyInSamples, float InGain);

		/** Adds a new a patch from an existing patch output. */
		void AddNewOutput(const FPatchOutputStrongPtr& InPatchOutputStrongPtr);

		/** Adds a new input to the tap collector. Calling this is thread safe, but individual instances of FPatchInput are only safe to be used from one thread. */
		FPatchInput AddNewInput(int32 MaxLatencyInSamples, float InGain);

		/** Adds a new a patch input from an existing patch input object. */
		void AddNewInput(FPatchInput& InInput);

		/** Removes a tap from the tap collector. Calling this is thread safe, though FPatchOutput will likely not be deleted until the next call of PopAudio. */
		void RemovePatch(const FPatchInput& InInput);

		/** Mixes audio from all inputs and pushes it to all outputs. Should be called regularly. */
		void ProcessAudio();

	protected:
		/** This class can be subclassed with OnProcessAudio overridden. */
		virtual void OnProcessAudio(TArrayView<const float> InAudio) { }

	private:
		FPatchMixer Mixer;
		FPatchSplitter Splitter;

		/** This buffer is used to pop audio from our Mixer and push to our splitter. */
		FAlignedFloatBuffer IntermediateBuffer;
	};
}
