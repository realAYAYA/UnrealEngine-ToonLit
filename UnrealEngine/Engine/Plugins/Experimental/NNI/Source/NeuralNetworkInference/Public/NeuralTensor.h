// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NeuralEnumClasses.h"
#include "RenderGraphBuilder.h" // FRDGBuilder
#include "RenderGraphDefinitions.h" // FRDGBufferSRVRef, FRDGBufferUAVRef
#include "RenderGraphResources.h" // FRDGPooledBuffer
#include "RHIDefinitions.h" // EBufferUsageFlags
#include "Templates/RefCounting.h" // TRefCountPtr
#include <algorithm> // std::fill
#include "NeuralTensor.generated.h"

/**
 * For a general overview of NeuralNetworkInference (NNI), including documentation and code samples, @see UNeuralNetwork, the main class of NNI.
 *
 * NNI's users should not directly create or modify instances of FNeuralTensor. Instead, they should only interact with FNeuralNetwork and its input
 * and output functions. E.g., @see FNeuralNetwork::GetInputTensor, FNeuralNetwork::SetInputFromArrayCopy, FNeuralNetwork::GetOutputTensor, etc.
 *
 * FNeuralTensor is an auxiliary class of UNeuralNetwork which represents a tensor of the UNeuralNetwork model. It is Unreal Engine's equivalent of
 * torch.Tensor (PyTorch) or caffe::Blob (Caffe).
 *
 * Most of FNeuralTensor's functions run on the CPU, so `CPUToRDGBuilder_RenderThread()` must be called before running on GPU and after running any
 * FNeuralTensor function that modifies the CPU memory. In addition, FNeuralTensor's CPU functions are very similar to those of TArray<T>.
 */
USTRUCT()
struct NEURALNETWORKINFERENCE_API FNeuralTensor
{
	GENERATED_BODY()

public:
	/**
	 * There are several constructors of FNeuralTensor, all of them similar to each other. They set and fill the main variables of this class:
	 *
	 * @param InDataType It sets DataType. @see DataType for more details.
	 * @param InSizes It sets Sizes and Volume. Set to empty (or omit this argument) if memory allocation is not required or the final tensor size is
	 *  unknown. @see Sizes, Volume for more details.
	 * @param InVolume Alternative to InSizes which also sets Sizes and Volume. Using InVolume is equivalent to using
	 *  InSizes = TArray<int64>({InVolume}). I.e., it behaves like InSizes, but assumes Sizes.Num() == 1 and sets Sizes[0] = Volume = InVolume. Set
	 *  to 0 or a negative value if memory allocation is not required or the final size is unknown.
	 * @param InName It sets Name. @see Name for more details.
	 * @param InTensorType It sets TensorType. @see TensorType for more details.
	 * @param InArray Alternative to InSizes/InVolume, and represents the input data to copy from. The template constructor that uses InArray is a
	 *  simple (but not efficient) way of initializing a FNeuralTensor from the existing InArray TArray. It is equivalent to calling
	 *  FNeuralTensor(..., InSizes/InVolume, ...) and then SetFromArrayCopy(InArray)/SetFromVoidPointerCopy(InVoidPtr). It is not efficient because
	 *  it makes a deep copy of the data of InArray. For maximum speed, rather than creating this intermediate InArray TArray, you can avoid the
	 *  double copy by calling FNeuralTensor(..., InSizes/InVolume, ...) and then filling the tensor memory with GetData()/GetDataCasted().
	 *
	 * In addition, UnderlyingUInt8ArrayData is preallocated from InDataType and InSizes/InVolume. @see UnderlyingUInt8ArrayData for more details.
	 */
	FNeuralTensor(const ENeuralDataType InDataType = ENeuralDataType::None, const TArray<int64>& InSizes = TArray<int64>(),
		const FString& InName = TEXT("FNeuralTensor"), const ENeuralTensorType InTensorType = ENeuralTensorType::Generic);
	FNeuralTensor(const ENeuralDataType InDataType, const int64 InVolume, const FString& InName = TEXT("FNeuralTensor"),
		const ENeuralTensorType InTensorType = ENeuralTensorType::Generic);
	template <typename T>
	FNeuralTensor(const TArray<T>& InArray, const TArray<int64>& InSizes = TArray<int64>(), const FString& InName = TEXT("FNeuralTensor"),
		const ENeuralTensorType InTensorType = ENeuralTensorType::Generic);

	/**
	 * Copy and move constructors and assignment functions, respectively.
	 * - Copy constructor and assignment (input is a const FNeuralTensor): They perform a deep (slow but safe) copy of the current tensor. The
	 *   resulting tensor will not share any parameters with the current one and both can be safely used.
	 * - Move constructor and assignment (input is not const and will not be valid after calling this function): They move the members of the input
	 *   InTensor FNeuralTensor, thus InTensor is no longer safe to use.
	 *
	 * @param InTensor The input FNeuralTensor to copy/move.
	 */
	FNeuralTensor(const FNeuralTensor& InTensor);
	FNeuralTensor& operator=(const FNeuralTensor& InTensor);
	FNeuralTensor(FNeuralTensor&& InTensor);
	FNeuralTensor& operator=(FNeuralTensor&& InTensor);

	/**
	 * Comparison operators (equal and not equal). They compare whether the CPU variables match between the 2 FNeuralTensor's (i.e., DataType, Sizes,
	 * Volume and CPU data). They do not consider other FNeuralTensor variables (such as ENeuralTensorType).
	 * @return
	 * - Equal operator, operator==(): True if the CPU variables matched each other, false otherwise.
	 * - Not equal operator, operator!=() (opposite than the equal operator): False if the CPU variables matched each other, false otherwise.
	 */
	bool operator==(const FNeuralTensor& InTensor) const;
	FORCEINLINE bool operator!=(const FNeuralTensor& InTensor) const;

	/**
	 * It returns a constant reference of DataType. @see DataType for more details.
	 */
	FORCEINLINE ENeuralDataType GetDataType() const;

	/**
	 * It returns Sizes[InDimension], i.e., the size of the current dimension, or 1 if InDimension >= GetNumberDimensions().
	 * @see Sizes for more details.
	 */
	int64 GetSize(const int32 InDimension) const;

	/**
	 * It returns a constant reference of Sizes. @see Sizes for more details.
	 */
	FORCEINLINE const TArray<int64>& GetSizes() const;

	/**
	 * It returns the number of dimensions, i.e., Sizes.Num(). @see Sizes for more details.
	 */
	FORCEINLINE int32 GetNumberDimensions() const;

	/**
	 * It returns Volume, i.e., the number of elements of the tensor.
	 */
	FORCEINLINE int64 Num() const;

	/**
	 * It returns the volume in bytes. I.e., UnderlyingUInt8ArrayData.Num() or equivalently:
	 * NumInBytes() = Num() * sizeof(type used).
	 */
	FORCEINLINE int64 NumInBytes() const;

	/**
	 * It returns whether the tensor is empty (i.e., Volume is 0 and/or Sizes is empty).
	 */
	FORCEINLINE bool IsEmpty() const;

	/**
	 * It returns a constant reference of Name. @see Name for more details.
	 */
	FORCEINLINE const FString& GetName() const;

	/**
	 * It returns NeuralTensorType. @see NeuralTensorType for more details.
	 */
	FORCEINLINE ENeuralTensorType GetTensorType() const;

	/**
	 * It sets NeuralTensorType. @see NeuralTensorType for more details.
	 */
	void SetTensorType(const ENeuralTensorType InTensorType);

	/**
	 * It sets bEnableGPU. @see bEnableGPU for more details.
	 */
	FORCEINLINE void SetEnableGPU(const bool bInEnableGPU);

	/**
	 * @see At(), GetArrayCopy(), GetUnderlyingUInt8ArrayRef(), GetData(), GetDataCasted(), SetNumUninitialized(), SetFromUnderlyingUInt8ArrayCopy(),
	 * SetFromArrayCopy(), SetFromVoidPointerCopy(), SetTo(), SetFromTensorProto(), which represent all the functions that can access/modify the CPU
	 * memory. Some of these functions could result in undefined behavior in not used properly:
	 * - If FNeuralTensor goes out of scope, it is destructed, or its memory is reset (constructor, SetNum(), etc.), the functions returning
	 *   references/pointers will no longer be valid.
	 * - These functions only modify the CPU memory, they do not synchronize CPU and GPU memory automatically. The user must do this accordingly.
	 *
	 * At() returns a reference to the desired indexed element of the underlying CPU data (UnderlyingUInt8ArrayData). There are 2 variations of this
	 * function, one constant (it does not allow to modify the referenced value) and a non-const one (which allows modifying it).
	 *
	 * @return Reference to the indexed element (const or non-constant).
	 */
	template <typename T, typename TInput>
	T& At(const TInput InIndex);
	template <typename T, typename TInput>
	const T& At(const TInput InIndex) const;

	/**
	 * @see At(), GetArrayCopy(), GetUnderlyingUInt8ArrayRef(), GetData(), GetDataCasted(), SetNumUninitialized(), SetFromUnderlyingUInt8ArrayCopy(),
	 * SetFromArrayCopy(), SetFromVoidPointerCopy(), SetTo(), SetFromTensorProto(), which represent all the functions that can access/modify the CPU
	 * memory. Some of these functions could result in undefined behavior in not used properly:
	 * - If FNeuralTensor goes out of scope, it is destructed, or its memory is reset (constructor, SetNum(), etc.), the functions returning
	 *   references/pointers will no longer be valid.
	 * - These functions only modify the CPU memory, they do not synchronize CPU and GPU memory automatically. The user must do this accordingly.
	 *
	 * GetArrayCopy and GetUnderlyingUInt8ArrayRef allow accessing the tensor data as a TArray, but they cannot modify the data values:
	 * - GetArrayCopy<T> (slower but safer) returns a copy of the data as a TArray<T>. T has to be the same size than sizeof(DataType).
	 * - GetUnderlyingUInt8ArrayRef (faster but could go out of scope) returns a reference to the underlying TArray<uint8> that contains the results.
	 */
	template <typename T>
	TArray<T> GetArrayCopy() const;
	FORCEINLINE const TArray<uint8>& GetUnderlyingUInt8ArrayRef() const;

	/**
	 * @see At(), GetArrayCopy(), GetUnderlyingUInt8ArrayRef(), GetData(), GetDataCasted(), SetNumUninitialized(), SetFromUnderlyingUInt8ArrayCopy(),
	 * SetFromArrayCopy(), SetFromVoidPointerCopy(), SetTo(), SetFromTensorProto(), which represent all the functions that can access/modify the CPU
	 * memory. Some of these functions could result in undefined behavior in not used properly:
	 * - If FNeuralTensor goes out of scope, it is destructed, or its memory is reset (constructor, SetNum(), etc.), the functions returning
	 *   references/pointers will no longer be valid.
	 * - These functions only modify the CPU memory, they do not synchronize CPU and GPU memory automatically. The user must do this accordingly.
	 *
	 * GetArrayCopy and GetUnderlyingUInt8ArrayRef allow accessing and optionally modifying the tensor data as a pointer:
	 * - GetData() returns a void pointer.
	 * - GetDataCasted<T>() returns a pointer casted to the desired type T.
	 * There are 2 variations of each of these 2 functions, a constant (for read-only access) and a non-const one (which allows modifying the data).
	 *
	 * @return Void (GetData) or T (GetDataCasted) pointer to the underlying FNeuralTensor TArray<uint8> data (const or non-constant).
	 */
	FORCEINLINE void* GetData();
	FORCEINLINE const void* const GetData() const;

	template<typename T>
	FORCEINLINE T* GetDataCasted();

	template<typename T>
	FORCEINLINE const T* const GetDataCasted() const;

	/**
	 * @see At(), GetArrayCopy(), GetUnderlyingUInt8ArrayRef(), GetData(), GetDataCasted(), SetNumUninitialized(), SetFromUnderlyingUInt8ArrayCopy(),
	 * SetFromArrayCopy(), SetFromVoidPointerCopy(), SetTo(), SetFromTensorProto(), which represent all the functions that can access/modify the CPU
	 * memory. Some of these functions could result in undefined behavior in not used properly:
	 * - If FNeuralTensor goes out of scope, it is destructed, or its memory is reset (constructor, SetNum(), etc.), the functions returning
	 *   references/pointers will no longer be valid.
	 * - These functions only modify the CPU memory, they do not synchronize CPU and GPU memory automatically. The user must do this accordingly.
	 *
	 * SetNumUninitialized resizes the tensor to the desired new size (i.e., modifying DataType, Sizes, Volume, UnderlyingUInt8ArrayData, etc.).
	 * @param InDataType It sets DataType. If None, it will keep its current value. @see DataType for more details.
	 * @param InSizes It sets Sizes and Volume. Set to empty (or omit this argument) if memory allocation is not required or the final tensor size is
	 *  unknown. @see Sizes, Volume for more details.
	 * @param InVolume Alternative to InSizes which also sets Sizes and Volume. Using InVolume is equivalent to using
	 *  InSizes = TArray<int64>({InVolume}). I.e., it behaves like InSizes, but assumes Sizes.Num() == 1 and sets Sizes[0] = Volume = InVolume. Set
	 *  to 0 or a negative value if memory allocation is not required or the final size is unknown.
	 * @param bInAllowShrinking This boolean is used when calling UnderlyingUInt8ArrayData.SetNumUninitialized(NumInBytes(), bInAllowShrinking).
	 * @see TArray::SetNumUninitialized for more information about bInAllowShrinking.
	 */
	void SetNumUninitialized(const ENeuralDataType InDataType, const TArray<int64>& InSizes, const bool bInAllowShrinking = true);
	FORCEINLINE void SetNumUninitialized(const ENeuralDataType InDataType, const int64 InVolume, const bool bInAllowShrinking = true);

	/**
	 * @see At(), GetArrayCopy(), GetUnderlyingUInt8ArrayRef(), GetData(), GetDataCasted(), SetNumUninitialized(), SetFromUnderlyingUInt8ArrayCopy(),
	 * SetFromArrayCopy(), SetFromVoidPointerCopy(), SetTo(), SetFromTensorProto(), which represent all the functions that can access/modify the CPU
	 * memory. Some of these functions could result in undefined behavior in not used properly:
	 * - If FNeuralTensor goes out of scope, it is destructed, or its memory is reset (constructor, SetNum(), etc.), the functions returning
	 *   references/pointers will no longer be valid.
	 * - These functions only modify the CPU memory, they do not synchronize CPU and GPU memory automatically. The user must do this accordingly.
	 *
	 * SetFromUnderlyingUInt8ArrayCopy/SetFromArrayCopy/SetFromVoidPointerCopy will replace the contents of the underlying CPU TArray with the input
	 * TArray or void pointer, by deeply copying the array/pointer (safer and easier to use). For maximum performance:
	 * - If you are given an input TArray/pointer (or a FNeuralTensor with different Sizes) that cannot be moved, use SetFromArrayCopy() or
	 *   SetFromVoidPointerCopy().
	 * - If you are initializing the FNeuralTensor iteratively (and to avoid a copy), fill the tensor with GetData()/GetDataCasted<T>().
	 * Modifying InArray after calling this function will not modify the data of this FNeuralTensor.
	 * The size of InArray and this tensor must match, i.e., NumInBytes() must match TArray<uint8>::Num() or Num() match TArray<T>.Num().
	 */
	void SetFromUnderlyingUInt8ArrayCopy(const TArray<uint8>& InArray);
	template<typename T>
	void SetFromArrayCopy(const TArray<T>& InArray);
	void SetFromVoidPointerCopy(const void* const InVoidPtr);

	/**
	 * @see At(), GetArrayCopy(), GetUnderlyingUInt8ArrayRef(), GetData(), GetDataCasted(), SetNumUninitialized(), SetFromUnderlyingUInt8ArrayCopy(),
	 * SetFromArrayCopy(), SetFromVoidPointerCopy(), SetTo(), SetFromTensorProto(), which represent all the functions that can access/modify the CPU
	 * memory. Some of these functions could result in undefined behavior in not used properly:
	 * - If FNeuralTensor goes out of scope, it is destructed, or its memory is reset (constructor, SetNum(), etc.), the functions returning
	 *   references/pointers will no longer be valid.
	 * - These functions only modify the CPU memory, they do not synchronize CPU and GPU memory automatically. The user must do this accordingly.
	 *
	 * SetTo() sets all the elements of the FNeuralTensor to the same value (InValue).
	 * It uses a double typename to avoid the mistake of SetTo(0) for a non-32-byte type (because that 0 would be an int32 otherwise).
	 */
	template<typename T, typename TInput>
	void SetTo(const TInput InValue);

	/**
	 * @see At(), GetArrayCopy(), GetUnderlyingUInt8ArrayRef(), GetData(), GetDataCasted(), SetNumUninitialized(), SetFromUnderlyingUInt8ArrayCopy(),
	 * SetFromArrayCopy(), SetFromVoidPointerCopy(), SetTo(), SetFromTensorProto(), which represent all the functions that can access/modify the CPU
	 * memory. Some of these functions could result in undefined behavior in not used properly:
	 * - If FNeuralTensor goes out of scope, it is destructed, or its memory is reset (constructor, SetNum(), etc.), the functions returning
	 *   references/pointers will no longer be valid.
	 * - These functions only modify the CPU memory, they do not synchronize CPU and GPU memory automatically. The user must do this accordingly.
	 *
	 * SetFromTensorProto() fills the current neural tensor with the input FTensorProto.
	 * @return Whether the conversion was successful.
	 */
	bool SetFromTensorProto(const struct FTensorProto* const InTensorProto, const FString& InTensorName, const ENeuralTensorType InTensorType);

	/**
	 * It reverses/flips the order of elements in an array along the given axis. It is needed for applications such as convolution.
	 * - If 1 argument, it flips the InDimension dimension of the tensor.
	 * - If 2 arguments, it flips all the dimensions of the tensor in the range [InDimensionFirst, InDimensionLast).
	 * E.g., for the following [2x2x2] tensor:
	 *   - Tensor    = [[[0, 1], [2, 3]], [[4, 5], [6, 7]]]
	 *   - Flip(0)   = [[[4, 5], [6, 7]], [0, 1], [2, 3]]]
	 *   - Flip(1)   = [[[2, 3], [0, 1]], [[6, 7], [4, 5]]]
	 *   - Flip(0,3) = [[[7, 6], [5, 4]], [[3, 2], [1, 0]]]
	 * @return Whether the flip was successful. It could only fail if the given arguments are not valid.
	 * @see https://numpy.org/doc/stable/reference/generated/numpy.flip.html for more information.
	 */
	bool Flip(const int32 InDimension);
	bool Flip(const int32 InDimensionFirst, const int32 InDimensionLast);

	/**
	 * It transposes the matrix if the tensor has up to 2 dimensions. Otherwise, it will return false and do nothing.
	 * @return Whether the transpose was successful. E.g., it will fail if GetNumberDimensions() > 2.
	 * @see https://numpy.org/doc/stable/reference/generated/numpy.transpose.html for more information.
	 */
	bool Transpose();

	/**
	 * If the number of elements is preserved (i.e., Prod(Sizes) == Prod(InSizes)), it reshapes the current tensor, meaning it updates Sizes with
	 * InSizes. No other internal variable is modified.
	 * Reshape() copies and ReshapeMove() moves InSizes.
	 * @return Whether the reshape was successful. It would fail if and only if Prod(Sizes) != Prod(InSizes).
	 */
	bool Reshape(const TArray<int64>& InSizes);
	bool ReshapeMove(TArray<int64>& InSizes);

	/**
	 * There are different functions to upload/download memory from/to CPU to/from GPU:
	 * - CPUToRDGBuilder_RenderThread either uploads the memory from the CPU to the GPU (if bInShouldCopyFromCPU) or simply allocates the desired GPU
	 *   memory. Then, it registers that GPU memory on the input FRDGBuilder.
	 * - GPUToRDGBuilder_RenderThread implies that the memory is already on the GPU, and simply registers that GPU memory on the input FRDGBuilder.
	 * - RDGBuilderToCPU_RenderThread downloads the memory from the GPU to the CPU. It is the opposite operation than CPUToRDGBuilder_RenderThread.
	 *
	 * @see CPUToRDGBuilder_RenderThread, GPUToRDGBuilder_RenderThread, RDGBuilderToCPU_RenderThread
	 */
	void CPUToRDGBuilder_RenderThread(FRDGBuilder* InOutGraphBuilder);
	void GPUToRDGBuilder_RenderThread(FRDGBuilder* InOutGraphBuilder);
	void RDGBuilderToCPU_RenderThread(FRDGBuilder* InOutGraphBuilder);

	/**
	 * It allocates the GPU data, in particular the pooled buffer.
	 * Should be called on the render thread. Internally this blocks for reading back work on the render thread.
	 * @param InOutNativeResource Pointer to the ID3D12Resource obtained from PooledBuffer. This resource can be shared with the DirectML execution
	 * provider.
	 */
	bool InitPooledBufferForUEAndORTBackEnd(void** InOutNativeResource = nullptr);

	/**
	 * It allocates the GPU data, in particular the pooled buffer.
	 * Should be called on the render thread.
	 * @param GraphBuilder. RDG graph builder to use for allocation.
	 */
	void InitPooledBufferForUEAndORTBackEnd_RenderThread(FRDGBuilder& GraphBuilder);

	/**
	 * It returns a constant reference of PooledBuffer. @see PooledBuffer for more details.
	 */
	const TRefCountPtr<FRDGPooledBuffer>& GetPooledBuffer() const;

	/**
	 * It returns a const reference of BufferSRVRef. @see BufferSRVRef for more details.
	 */
	const FRDGBufferSRVRef GetBufferSRVRef() const;

	/**
	 * It returns a reference of BufferUAVRef. @see BufferUAVRef for more details.
	 */
	FRDGBufferUAVRef GetBufferUAVRef();

	/**
	 * It returns a FString with all the tensor information (underlying CPU data and optionally DataType, Sizes, Volume, etc.).
	 * @param InMaxNumberElementsToDisplay The maximum number of elements to display. If Volume <= InMaxNumberElementsToDisplay or
	 * InMaxNumberElementsToDisplay < 1, all elements are displayed. Otherwise, only the first and last InMaxNumberElementsToDisplay/2 are displayed.
	 * @param bInReturnOnlyData If false (default), it will print other information such as DataType, Sizes, Volume, etc. If true, it will only print
	 * the underlying CPU data values. E.g., "FNeuralTensor: Int64, Generic, volume=3, sizes={3}, data=[1 2 3]" vs. "[1 2 3]", respectively.
	 */
	FString ToString(const int64 InMaxNumberElementsToDisplay = -1, const bool bInReturnOnlyData = false) const;

protected:
	/**
	 * General variables and properties set by the FNeuralTensor constructor (or their setting functions) and saved on the FNeuralTensor UAsset.
	 * - DataType defines the underlying uint8 data type of the network (float, double, int32, etc).
	 * - Sizes defines the dimensions of the tensor.
	 * - Volume defines the total number of elements of the tensor. I.e., it is the product of the size of each dimension, or mathematically
	 *   Prod(Sizes[Index]) for all Indexes.
	 * - Name is used for GPU debugging and the ToString() function.
	 * - TensorType defines the type of the tensor on a network (Generic, Input, Intermediate(Not)Initialized, Output, Weight, etc.). It is currently
	 *   only used when moving memory to the GPU.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ENeuralDataType DataType;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int64> Sizes;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int64 Volume;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ENeuralTensorType TensorType;

private:
	/**
	 * The array containing the underlying CPU data. It is preallocated based on the combination of DataType and Sizes/Volume (if Sizes/Volume are
	 * not empty/zero): UnderlyingUInt8ArrayData.Num() == sizeof(data type) x Volume.
	 */
	UPROPERTY()
	TArray<uint8> UnderlyingUInt8ArrayData;
	/**
	 * GPU-related variables that are transient. They are not serialized and are re-configured every time a tensor is loaded.
	 * - bEnableGPU: By default false, meaning all GPU memory allocation will be disabled and GPU functions will do nothing. Enable to allow using
	 *   the GPU capabilities of the tensor.
	 * - PooledBuffer: FRDGPooledBuffer containing the GPU memory of this tensor.
	 * - BufferSRVRef: Casting of the PooledBuffer into a read-only SRV.
	 * - BufferUAVRef: Casting of the PooledBuffer into a read-write UAV.
	 */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Neural Network Inference")
	bool bEnableGPU;
	TSharedPtr<TRefCountPtr<FRDGPooledBuffer>> PooledBuffer;
	TSharedPtr<FRDGBufferSRVRef> BufferSRVRef;
	TSharedPtr<FRDGBufferUAVRef> BufferUAVRef;

	/**
	 * Auxiliary constructor for the template constructor that takes a TArray<T>& InArray.
	 * InSizeOfT and sizeof(InDataType) should match, as well as the volume of InValueNum and InSizes.
	 * It simply moves as much code as possible from the template constructor to the cpp file.
	 */
	FNeuralTensor(const ENeuralDataType InDataType, const void* const InValues, const int64 InSizeOfT, const int64 InValueNum,
		const TArray<int64>& InSizes, const FString& InName, const ENeuralTensorType InTensorType);

	/**
	 * Auxiliary function for the template SetFromArrayCopy().
	 * It simply moves as much code as possible from the template function to the cpp file.
	 */
	void SetFromVoidPointerCopy(const void* const InVoidPtr, const int64 InSizeOfT, const int64 InDataSize);

	/**
	 * Checks and warns whether the current data type T is the same than the one defined by DataType.
	 * CheckTAndDataTypeEquivalent() is the function to be used and CheckTAndDataTypeEquivalentAuxiliary its just its auxiliary and moves as much
	 * code as possible from the template function to the cpp file
	 */
	template<typename T>
	bool CheckTAndDataTypeEquivalent() const;
	bool CheckTAndDataTypeEquivalentAuxiliary(const bool bInCheckTAndDataTypeResult, const int64 InSizeOfT) const;

	/**
	 * This function has total control over the buffer flags (and ignores the TensorType flag). Conceptually speaking, it should be a static
	 * function, but that would add a lot of arguments to it. Thus, it is kept private and it should only be used by its public equivalent.
	 * Depending on the value of InBufferUsageFlags, it can perform GPU allocation, CPU-to-GPU upload, apply different settings, etc.
	 * @see CPUToRDGBuilder_RenderThread(FRDGBuilder* InOutGraphBuilder).
	 */
	void CPUToRDGBuilder_RenderThread(FRDGBuilder* InOutGraphBuilder, const EBufferUsageFlags InBufferUsageFlags, const bool bInShouldCopyFromCPU);
};



/* FNeuralTensor inlined and templated functions
 *****************************************************************************/

template <typename T>
FNeuralTensor::FNeuralTensor(const TArray<T>& InArray, const TArray<int64>& InSizes, const FString& InName, const ENeuralTensorType InTensorType)
	: FNeuralTensor(FNeuralDataTypeUtils::GetDataType<T>(), InArray.GetData(), sizeof(T), InArray.Num(),
		(InSizes.Num() > 0 ? InSizes : TArray<int64>({ (int64)InArray.Num() })), InName, InTensorType)
{}

bool FNeuralTensor::operator!=(const FNeuralTensor& InTensor) const
{
	return !(*this == InTensor);
}

ENeuralDataType FNeuralTensor::GetDataType() const
{
	return DataType;
}

const TArray<int64>& FNeuralTensor::GetSizes() const
{
	return Sizes;
}

int32 FNeuralTensor::GetNumberDimensions() const
{
	return Sizes.Num();
}

int64 FNeuralTensor::Num() const
{
	return Volume;
}

int64 FNeuralTensor::NumInBytes() const
{
	return UnderlyingUInt8ArrayData.Num();
}

bool FNeuralTensor::IsEmpty() const
{
	return UnderlyingUInt8ArrayData.IsEmpty();
}

const FString& FNeuralTensor::GetName() const
{
	return Name;
}

ENeuralTensorType FNeuralTensor::GetTensorType() const
{
	return TensorType;
}

void FNeuralTensor::SetEnableGPU(const bool bInEnableGPU)
{
	bEnableGPU = bInEnableGPU;
}

template <typename T, typename TInput>
T& FNeuralTensor::At(const TInput InIndex)
{
	checkf(CheckTAndDataTypeEquivalent<T>(), TEXT("FNeuralTensor::At(): CheckTAndDataType failed."));
	return ((T*)UnderlyingUInt8ArrayData.GetData())[InIndex];
}

template <typename T, typename TInput>
const T& FNeuralTensor::At(const TInput InIndex) const
{
	checkf(CheckTAndDataTypeEquivalent<T>(), TEXT("FNeuralTensor::At(): CheckTAndDataType failed."));
	return ((T*)UnderlyingUInt8ArrayData.GetData())[InIndex];
}

template <typename T>
TArray<T> FNeuralTensor::GetArrayCopy() const
{
	TArray<T> Array;
	if (CheckTAndDataTypeEquivalent<T>())
	{
		Array.SetNumUninitialized(Num());
		FMemory::Memcpy(Array.GetData(), UnderlyingUInt8ArrayData.GetData(), NumInBytes());
	}
	return Array;
}

const TArray<uint8>& FNeuralTensor::GetUnderlyingUInt8ArrayRef() const
{
	return UnderlyingUInt8ArrayData;
}

void* FNeuralTensor::GetData()
{
	return (void*)UnderlyingUInt8ArrayData.GetData();
}

const void* const FNeuralTensor::GetData() const
{
	return (void*)UnderlyingUInt8ArrayData.GetData();
}

template<typename T>
T* FNeuralTensor::GetDataCasted()
{
	checkf(CheckTAndDataTypeEquivalent<T>(), TEXT("FNeuralTensor::GetDataCasted(): CheckTAndDataType failed."));
	return (T*)UnderlyingUInt8ArrayData.GetData();
}

template<typename T>
const T* const FNeuralTensor::GetDataCasted() const
{
	checkf(CheckTAndDataTypeEquivalent<T>(), TEXT("FNeuralTensor::GetDataCasted(): CheckTAndDataType failed."));
	return (T*)UnderlyingUInt8ArrayData.GetData();
}

void FNeuralTensor::SetNumUninitialized(const ENeuralDataType InDataType, const int64 InVolume, const bool bInAllowShrinking)
{
	SetNumUninitialized(InDataType, (InVolume > 0 ? TArray<int64>({ InVolume }) : TArray<int64>({})), bInAllowShrinking);
}

template<typename T>
void FNeuralTensor::SetFromArrayCopy(const TArray<T>& InArray)
{
	if (CheckTAndDataTypeEquivalent<T>())
	{
		SetFromVoidPointerCopy(InArray.GetData(), sizeof(T), InArray.Num());
	}
}

template<typename T, typename TInput>
void FNeuralTensor::SetTo(const TInput InValue)
{
	if (CheckTAndDataTypeEquivalent<T>())
	{
		std::fill(GetDataCasted<T>(), GetDataCasted<T>() + Num(), (T)InValue); // There is no UE equivalent for std::fill
	}
}

template<typename T>
bool FNeuralTensor::CheckTAndDataTypeEquivalent() const
{
	return CheckTAndDataTypeEquivalentAuxiliary(FNeuralDataTypeUtils::GetDataType<T>() == DataType, sizeof(T));
}
