// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "BufferVectorOperations.h"

namespace Audio
{	
	// First In First Out Buffer designed for audio buffers. Generally performs best when buffers are greater
	// than 16 samples, optimally for buffers between 256 to 4096 samples.  Memory operations are designed 
	// for audio applications by ensuring memory allocations only occur in the constructor or when capacity 
	// is altered, memory freeing only happens in the destructor or when capacity is altered. All assignments 
	// are performed using block memory copies.
	class SIGNALPROCESSING_API FAlignedBlockBuffer
	{
	public:
		
		// Constructor
		FAlignedBlockBuffer(int32 InSampleCapacity=8192, int32 InMaxNumInspectSamples=1024, uint32 InByteAlignment=AUDIO_SIMD_BYTE_ALIGNMENT, uint32 InAllocByteAlignment=AUDIO_BUFFER_ALIGNMENT);

		// Destructor
		~FAlignedBlockBuffer() throw();
		
		// Maximum size of buffer
		int32 GetSampleCapacity() const;
		
		// Amount of samples left over in buffer
		int32 GetNumAvailable() const;

		// Add zeros to the buffer
		void AddZeros(int32 InNum);
		
		// Add samples to the buffer.
		void AddSamples(const float* InSamples, int32 InNum);
		
		// Remove samples from the buffer
		void RemoveSamples(int32 InNum);

		// Clear entire buffer
		void ClearSamples();

		// Inspect samples in the buffer. Returned samples abide by the memory alignment specified during construction.
		// Warning: Not thread safe. The returned pointer is only valid as long as this object exists 
		// and before any mutating operations are made to this object.
		const float* InspectSamples(int32 InNum, int32 InOffset=0);
		
		// Maximum number of elements that can be inspected
		int32 GetMaxNumInspectSamples() const;
		
	protected:

		// Set the maximum number of elements that can be inspected
		void SetMaxNumInspectSamples(int32 InMax);

		float* AllocateAlignedFloatArray(int32 InNum) const;
		void FreeFloatArray(float*& Array) const;
		void ZeroFloatArray(float* InArray, int32 InNum) const;
		void CopyFloatArray(float* ToArray, const float* FromArray, int32 InNum) const;

	private:

		int32 CheckNumAndIncrementWriteCount(int32 InNum);

		// Hide copy and assignment operators
		FAlignedBlockBuffer& operator=(const FAlignedBlockBuffer& rhs);
		FAlignedBlockBuffer(const FAlignedBlockBuffer& CopyBuff);
		
		// Alignment definition
		uint32 AllocByteAlignment;
		uint32 ByteAlignment;
		uint32 FloatAlignment;

		// Internal memory
		float* InternalBuffer;
		float* RolloverBuffer;

		// Read and write pointers
		float* ReadPointer;
		float* WritePointer;

		int32 SampleCapacity;
		int32 MaxNumInspectSamples;
		int32 WriteCount;
		int32 WritePos;
		int32 ReadPos;
	};
}

