// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DSP/AlignedBuffer.h"
#include "DSP/BufferDiagnostics.h"

namespace Audio
{
	/**
	 ** Opaque wrapper around FAlignedFloatBuffer.
	 ** Will perform different checks on accessing the buffer, which can be enabled by flags.
	 ** Error states are sticky.
	 **/
	class SIGNALPROCESSING_API FCheckedAudioBuffer
	{
	public:
		using ECheckBehavior = Audio::EBufferCheckBehavior;
				
		void SetName(const FString& InName) { DescriptiveName = InName; }
		void SetCheckBehavior(const ECheckBehavior InBehavior) { Behavior = InBehavior; }
		void SetCheckFlags(const ECheckBufferFlags InCheckFlags) { CheckFlags = InCheckFlags; }
		
		// Automatic conversions to FAlignedBuffer with a Check.
		operator FAlignedFloatBuffer&() { return GetBuffer(); };
		operator TArrayView<float>() { return MakeArrayView(GetBuffer()); };
		operator TArrayView<const float>() const { return MakeArrayView(GetBuffer()); };
		operator const FAlignedFloatBuffer&() const { return GetBuffer(); }
		void operator=(const FAlignedFloatBuffer& InOther);
		const FAlignedFloatBuffer* operator&() const { return &GetBuffer(); }
		FAlignedFloatBuffer* operator&() { return &GetBuffer(); }
				
		int32 Num() const;
		void Reserve(const int32 InSize);
		void Reset(const int32 InSize=0);
		void AddZeroed(const int32 InSize);
		void SetNumZeroed(const int32 InSize);
		void SetNumUninitialized(const int32 InNum);

		// Const.
		const FAlignedFloatBuffer& GetBuffer() const;

		// Non-const.
		FAlignedFloatBuffer& GetBuffer();
		float* GetData();
				
		void Append(const FAlignedFloatBuffer& InBuffer);
		void Append(TArrayView<const float> InView);
		void Append(const FCheckedAudioBuffer& InBuffer);
	private:
		void DoCheck(TArrayView<const float> InBuffer) const;
		
		FString DescriptiveName;			// String so this can be procedural.
		FAlignedFloatBuffer Buffer;			// Wrapped buffer.
		
		ECheckBehavior Behavior = ECheckBehavior::Nothing;
		ECheckBufferFlags CheckFlags = ECheckBufferFlags::None;
		mutable ECheckBufferFlags FailedFlags = ECheckBufferFlags::None;
		mutable bool bFailedChecks = false;
	};
}


