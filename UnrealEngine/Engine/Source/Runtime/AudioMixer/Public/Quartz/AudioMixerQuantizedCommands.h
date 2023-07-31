// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMixerClock.h"

namespace Audio
{
	// QuartzQuantizedCommand that plays a sound on a sample-accurate boundary
	class AUDIOMIXER_API FQuantizedPlayCommand : public IQuartzQuantizedCommand
	{
	public:
		// ctor
		FQuantizedPlayCommand();

		// dtor
		~FQuantizedPlayCommand() {}

		virtual TSharedPtr<IQuartzQuantizedCommand> GetDeepCopyOfDerivedObject() const override;

		virtual void OnQueuedCustom(const FQuartzQuantizedCommandInitInfo& InCommandInitInfo) override;

		virtual void OnFinalCallbackCustom(int32 InNumFramesLeft) override;

		virtual void CancelCustom() override;

		virtual bool RequiresAudioDevice() const override { return true; }

		virtual FName GetCommandName() const override;
		
		// for your implementation, a new EQuartzCommandType needs to be defined in QuartzQuantizationUtilities.h
		virtual EQuartzCommandType GetCommandType() const { return EQuartzCommandType::PlaySound; };

	protected:
		TSharedPtr<FQuartzClock> OwningClockPtr{ nullptr };

		int32 SourceID{ -1 };

		bool bIsCanceled = false;

	}; // class FQuantizedPlayCommand 


	class AUDIOMIXER_API FQuantizedQueueCommand : public IQuartzQuantizedCommand
	{
	public:
		virtual TSharedPtr<IQuartzQuantizedCommand> GetDeepCopyOfDerivedObject() const override;

		virtual void OnQueuedCustom(const FQuartzQuantizedCommandInitInfo& InCommandInitInfo) override;

		virtual int32 OverrideFramesUntilExec(int32 NumFramesUntilExec) override;

		virtual void OnFinalCallbackCustom(int32 InNumFramesLeft) override;

		virtual bool RequiresAudioDevice() const override { return true; }

		virtual FName GetCommandName() const override;
		virtual EQuartzCommandType GetCommandType() const { return EQuartzCommandType::QueueSoundToPlay; };

		void SetQueueCommand(const FAudioComponentCommandInfo& InAudioCommandData);

		FQuantizedQueueCommand() {}

		FQuantizedQueueCommand(const FQuantizedQueueCommand& Other)
			: OwningClockPtr(Other.OwningClockPtr)
			, AudioComponentData(Other.AudioComponentData)
		{}

	private:
		TSharedPtr<FQuartzClock> OwningClockPtr{ nullptr };

		//Data for the quantization event
		FAudioComponentCommandInfo AudioComponentData;
	}; // class FQuantizedQueueCommand 
	
	// QuartzQuantizedCommand that changes the TickRate of a clock on a sample-accurate boundary (i.e. BPM changes)
	class AUDIOMIXER_API FQuantizedTickRateChange : public IQuartzQuantizedCommand
	{
	public:
		void SetTickRate(const FQuartzClockTickRate& InTickRate)
		{
			TickRate = InTickRate;
		}

		virtual TSharedPtr<IQuartzQuantizedCommand> GetDeepCopyOfDerivedObject() const override;

		virtual void OnQueuedCustom(const FQuartzQuantizedCommandInitInfo& InCommandInitInfo) override;

		virtual void OnFinalCallbackCustom(int32 InNumFramesLeft) override;

		virtual bool IsClockAltering() override { return true; }

		virtual FName GetCommandName() const override;
		virtual EQuartzCommandType GetCommandType() const { return EQuartzCommandType::TickRateChange; };

	private:
		FQuartzClockTickRate TickRate;
		TSharedPtr<FQuartzClock> OwningClockPtr{ nullptr };

	}; // class FQuantizedTickRateChange 


	// QuartzQuantizedCommand that resets the transport of a clock's metronome on a sample-accurate boundary
	class AUDIOMIXER_API FQuantizedTransportReset : public IQuartzQuantizedCommand
	{
	public:
		virtual TSharedPtr<IQuartzQuantizedCommand> GetDeepCopyOfDerivedObject() const override;

		virtual void OnQueuedCustom(const FQuartzQuantizedCommandInitInfo& InCommandInitInfo) override;

		virtual void OnFinalCallbackCustom(int32 InNumFramesLeft) override;

		virtual bool IsClockAltering() override { return false; }

		virtual FName GetCommandName() const override;
		virtual EQuartzCommandType GetCommandType() const { return EQuartzCommandType::TransportReset; };

	private:
		TSharedPtr<FQuartzClock> OwningClockPtr{ nullptr };

	}; // class FQuantizedTransportReset 


	// QuartzQuantizedCommand that starts a second clock on a sample-accurate boundary
	class AUDIOMIXER_API FQuantizedOtherClockStart : public IQuartzQuantizedCommand
	{
	public:
		virtual TSharedPtr<IQuartzQuantizedCommand> GetDeepCopyOfDerivedObject() const override;

		virtual void OnQueuedCustom(const FQuartzQuantizedCommandInitInfo& InCommandInitInfo) override;

		virtual void OnFinalCallbackCustom(int32 InNumFramesLeft) override;

		virtual bool IsClockAltering() override { return true; }

		virtual FName GetCommandName() const override;
		virtual EQuartzCommandType GetCommandType() const { return EQuartzCommandType::StartOtherClock; };

	private:
		TSharedPtr<FQuartzClock> OwningClockPtr{ nullptr };
		FName NameOfClockToStart;

	}; // class FQuantizedOtherClockStart 

} // namespace Audio
