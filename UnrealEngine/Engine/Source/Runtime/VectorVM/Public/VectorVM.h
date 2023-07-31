// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ByteSwap.h"
#include "Templates/AlignmentTemplates.h"
#include "UObject/ObjectMacros.h"
#include "VectorVMCommon.h"
#include "VectorVMExperimental.h"
#include "VectorVMLegacy.h"

struct FVectorVMSerializeState;

#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY

class FVectorVMExternalFunctionContextProxy
{
public:
	const bool UsingExperimentalVM;
	FVectorVMExternalFunctionContextExperimental Experimental;
	FVectorVMExternalFunctionContextLegacy Legacy;

	FVectorVMExternalFunctionContextProxy(const FVectorVMExternalFunctionContextExperimental& InExperimental)
		: UsingExperimentalVM(true)
		, Experimental(InExperimental)
		, Legacy()
	{}

	FVectorVMExternalFunctionContextProxy(const FVectorVMExternalFunctionContextLegacy& InLegacy)
		: UsingExperimentalVM(false)
		, Experimental()
		, Legacy(InLegacy)
	{}

	FORCEINLINE int32* GetRandCounters()
	{
		return UsingExperimentalVM ? Experimental.GetRandCounters() : Legacy.GetRandCounters().GetData();
	}

	FORCEINLINE FRandomStream& GetRandStream()
	{
		return UsingExperimentalVM ? Experimental.GetRandStream() : Legacy.GetRandStream();
	}

	FORCEINLINE int32 GetNumInstances() const
	{
		return UsingExperimentalVM ? Experimental.GetNumInstances() : Legacy.GetNumInstances();
	}

	template<uint32 InstancesPerOp>
	FORCEINLINE int32 GetNumLoops() const
	{
		return UsingExperimentalVM ? Experimental.GetNumLoops<InstancesPerOp>() : Legacy.GetNumLoops<InstancesPerOp>();
	}

private:
	FVectorVMExternalFunctionContextProxy() = delete;
};

using FVectorVMExternalFunctionContext = FVectorVMExternalFunctionContextProxy;

#elif VECTORVM_SUPPORTS_EXPERIMENTAL

using FVectorVMExternalFunctionContext = FVectorVMExternalFunctionContextExperimental;

#elif VECTORVM_SUPPORTS_LEGACY

using FVectorVMExternalFunctionContext = FVectorVMExternalFunctionContextLegacy;

#else
#error "At least one of VECTORVM_SUPPORTS_EXPERIMENTAL | VECTORVM_SUPPORTS_LEGACY must be defined"
#endif

namespace VectorVM
{
	/** Get total number of op-codes */
	VECTORVM_API uint8 GetNumOpCodes();

#if WITH_EDITOR
	VECTORVM_API FString GetOpName(EVectorVMOp Op);
	VECTORVM_API FString GetOperandLocationName(EVectorVMOperandLocation Location);
#endif

	VECTORVM_API uint8 CreateSrcOperandMask(EVectorVMOperandLocation Type0, EVectorVMOperandLocation Type1 = EVectorVMOperandLocation::Register, EVectorVMOperandLocation Type2 = EVectorVMOperandLocation::Register);

	struct FVectorVMExecArgs
	{
		uint8 const* ByteCode = nullptr;
		uint8 const* OptimizedByteCode = nullptr;
		int32 NumTempRegisters = 0;
		int32 ConstantTableCount = 0;
		const uint8* const* ConstantTable = nullptr;
		const int32* ConstantTableSizes = nullptr;
		TArrayView<FDataSetMeta> DataSetMetaTable;
		const FVMExternalFunction* const* ExternalFunctionTable = nullptr;
		void** UserPtrTable = nullptr;
		int32 NumInstances = 0;
		bool bAllowParallel = true;
#if STATS
		TArrayView<FStatScopeData> StatScopes;
#elif ENABLE_STATNAMEDEVENTS
		TArrayView<const FString> StatNamedEventsScopes;
#endif
	};

#if VECTORVM_SUPPORTS_LEGACY
	/**
	 * Execute VectorVM bytecode.
	 */
	VECTORVM_API void Exec(FVectorVMExecArgs& Args, FVectorVMSerializeState *SerializeState);
	VECTORVM_API void OptimizeByteCode(const uint8* ByteCode, TArray<uint8>& OptimizedCode, TArrayView<uint8> ExternalFunctionRegisterCounts);
#endif

	VECTORVM_API void Init();

	#define VVM_EXT_FUNC_INPUT_LOC_BIT (unsigned short)(1<<15)
	#define VVM_EXT_FUNC_INPUT_LOC_MASK (unsigned short)~VVM_EXT_FUNC_INPUT_LOC_BIT

	template<typename T>
	struct FUserPtrHandler
	{
		int32 UserPtrIdx;
		T* Ptr;
		FUserPtrHandler(FVectorVMExternalFunctionContext& Context)
		{
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
			if (Context.UsingExperimentalVM)
			{
				int32 AdvanceOffset;
				int32 RegIdx;
				int32* ConstPtr = (int32*)Context.Experimental.GetNextRegister(&AdvanceOffset, &RegIdx);
				check(AdvanceOffset == 0); //must be constant
				UserPtrIdx = *ConstPtr;
				check(UserPtrIdx != INDEX_NONE);
				Ptr = (T*)Context.Experimental.GetUserPtrTable(UserPtrIdx);
			}
			else
			{
				const uint16 VariableOffset = Context.Legacy.DecodeU16();
				check(!(VariableOffset& VVM_EXT_FUNC_INPUT_LOC_BIT));

				const uint16 ConstantTableOffset = VariableOffset & VVM_EXT_FUNC_INPUT_LOC_MASK;
				UserPtrIdx = *Context.Legacy.GetConstant<int32>(ConstantTableOffset);
				check(UserPtrIdx != INDEX_NONE);

				Ptr = static_cast<T*>(Context.Legacy.GetUserPtrTable(UserPtrIdx));
			}
#elif VECTORVM_SUPPORTS_EXPERIMENTAL
			int32 AdvanceOffset;
			int32 RegIdx;
			int32* ConstPtr = (int32*)Context.GetNextRegister(&AdvanceOffset, &RegIdx);
			check(AdvanceOffset == 0); //must be constant
			UserPtrIdx = *ConstPtr;
			check(UserPtrIdx != INDEX_NONE);
			Ptr = (T*)Context.GetUserPtrTable(UserPtrIdx);
#elif VECTORVM_SUPPORTS_LEGACY
			const uint16 VariableOffset = Context.DecodeU16();
			check(!(VariableOffset & VVM_EXT_FUNC_INPUT_LOC_BIT));

			const uint16 ConstantTableOffset = VariableOffset & VVM_EXT_FUNC_INPUT_LOC_MASK;
			UserPtrIdx = *Context.GetConstant<int32>(ConstantTableOffset);
			check(UserPtrIdx != INDEX_NONE);

			Ptr = static_cast<T*>(Context.GetUserPtrTable(UserPtrIdx));
#else 
	#error "Not supported"
#endif
		}

		FORCEINLINE T* Get() { return Ptr; }
		FORCEINLINE T* operator->() { return Ptr; }
		FORCEINLINE operator T*() { return Ptr; }

		FORCEINLINE const T* Get() const { return Ptr; }
		FORCEINLINE const T* operator->() const { return Ptr; }
		FORCEINLINE operator const T* () const { return Ptr; }
	};

	// A flexible handler that can deal with either constant or register inputs.
	template<typename T>
	struct FExternalFuncInputHandler
	{
	private:
		/** Either byte offset into constant table or offset into register table deepening on VVM_INPUT_LOCATION_BIT */
		int32 InputOffset;
		const T* RESTRICT InputPtr;
		int32 AdvanceOffset;
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
		bool bIsRegister;
#endif

	public:
		FExternalFuncInputHandler()
			: InputOffset(INDEX_NONE)
			, InputPtr(nullptr)
			, AdvanceOffset(0)
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
			, bIsRegister(false)
#endif
		{}

		FORCEINLINE FExternalFuncInputHandler(FVectorVMExternalFunctionContext& Context)
		{
			Init(Context);
		}

		void Init(FVectorVMExternalFunctionContext& Context)
		{
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
			if (Context.UsingExperimentalVM)
			{
				InputPtr = (T*)Context.Experimental.GetNextRegister(&AdvanceOffset, &InputOffset) + Context.Experimental.PerInstanceFnInstanceIdx;
				bIsRegister = !!AdvanceOffset;
			}
			else
			{
				InputOffset = Context.Legacy.DecodeU16();
				bIsRegister = !!(InputOffset & VVM_EXT_FUNC_INPUT_LOC_BIT);

				const int32 Offset = GetOffset();
				InputPtr = IsConstant() ? Context.Legacy.GetConstant<T>(Offset) : reinterpret_cast<T*>(Context.Legacy.GetTempRegister(Offset));
				AdvanceOffset = IsConstant() ? 0 : 1;

				//Hack: Offset into the buffer by the instance offset.
				InputPtr += Context.Legacy.GetExternalFunctionInstanceOffset() * AdvanceOffset;
			}
#elif VECTORVM_SUPPORTS_EXPERIMENTAL
			InputPtr = (T*)Context.GetNextRegister(&AdvanceOffset, &InputOffset) + Context.PerInstanceFnInstanceIdx;
#elif VECTORVM_SUPPORTS_LEGACY
			InputOffset = Context.DecodeU16();

			const int32 Offset = GetOffset();
			InputPtr = IsConstant() ? Context.GetConstant<T>(Offset) : reinterpret_cast<T*>(Context.GetTempRegister(Offset));
			AdvanceOffset = IsConstant() ? 0 : 1;

			//Hack: Offset into the buffer by the instance offset.
			InputPtr += Context.GetExternalFunctionInstanceOffset() * AdvanceOffset;
#else
	#error "Not supported"
#endif
		}

		FORCEINLINE bool IsConstant()const { return !IsRegister(); }
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
		FORCEINLINE bool IsRegister() const { return bIsRegister; }
#elif VECTORVM_SUPPORTS_EXPERIMENTAL
		FORCEINLINE bool IsRegister() const { return (bool)AdvanceOffset; }
#elif VECTORVM_SUPPORTS_LEGACY
		FORCEINLINE bool IsRegister() const { return (InputOffset & VVM_EXT_FUNC_INPUT_LOC_BIT) != 0; }
#else
	#error "Not supported"
#endif
		FORCEINLINE int32 GetOffset()const { return InputOffset & VVM_EXT_FUNC_INPUT_LOC_MASK; }

		FORCEINLINE const T Get() { return *InputPtr; }
		FORCEINLINE const T* GetDest() { return InputPtr; }
		FORCEINLINE void Advance() { InputPtr += AdvanceOffset; }
		FORCEINLINE const T GetAndAdvance()
		{
			const T* Ret = InputPtr;
			InputPtr += AdvanceOffset;
			return *Ret;
		}
		FORCEINLINE const T* GetDestAndAdvance()
		{
			const T* Ret = InputPtr;
			InputPtr += AdvanceOffset;
			return Ret;
		}
	};
	
	template<typename T>
	struct FExternalFuncRegisterHandler
	{
	private:
		int32 RegisterIndex;
		int32 AdvanceOffset;
		T Dummy;
		T* RESTRICT Register;
	public:
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
		FORCEINLINE FExternalFuncRegisterHandler(FVectorVMExternalFunctionContext& Context)
		{
			if (Context.UsingExperimentalVM)
			{
				Register = (T*)Context.Experimental.GetNextRegister(&AdvanceOffset, &RegisterIndex) + Context.Experimental.PerInstanceFnInstanceIdx;
			}
			else
			{
				RegisterIndex = (Context.Legacy.DecodeU16() & VVM_EXT_FUNC_INPUT_LOC_MASK);
				AdvanceOffset = (IsValid() ? 1 : 0);
				{
					if (IsValid())
					{
						checkSlow(RegisterIndex < Context.Legacy.GetNumTempRegisters());
						Register = (T*)Context.Legacy.GetTempRegister(RegisterIndex);

						//Hack: Offset into the buffer by the instance offset.
						Register += Context.Legacy.GetExternalFunctionInstanceOffset() * AdvanceOffset;
					}
					else
					{
						Register = &Dummy;
					}
				}
			}
		}
#elif VECTORVM_SUPPORTS_EXPERIMENTAL
		FORCEINLINE FExternalFuncRegisterHandler(FVectorVMExternalFunctionContext& Context) {
			Register = (T*)Context.GetNextRegister(&AdvanceOffset, &RegisterIndex) + Context.PerInstanceFnInstanceIdx;
		}
#elif VECTORVM_SUPPORTS_LEGACY
		FORCEINLINE FExternalFuncRegisterHandler(FVectorVMExternalFunctionContext& Context)
			: RegisterIndex(Context.DecodeU16() & VVM_EXT_FUNC_INPUT_LOC_MASK)
			, AdvanceOffset(IsValid() ? 1 : 0)
		{
			if (IsValid())
			{
				checkSlow(RegisterIndex < Context.GetNumTempRegisters());
				Register = (T*)Context.GetTempRegister(RegisterIndex);

				//Hack: Offset into the buffer by the instance offset.
				Register += Context.GetExternalFunctionInstanceOffset() * AdvanceOffset;
			}
			else
			{
				Register = &Dummy;
			}
		}
#else
	#error "Not supported"
#endif

		FORCEINLINE bool IsValid() const { return RegisterIndex != (uint16)VVM_EXT_FUNC_INPUT_LOC_MASK; }

		FORCEINLINE const T Get() { return *Register; }
		FORCEINLINE T* GetDest() { return Register; }
		FORCEINLINE void Advance() { Register += AdvanceOffset; }
		FORCEINLINE const T GetAndAdvance()
		{
			T* Ret = Register;
			Register += AdvanceOffset;
			return *Ret;
		}
		FORCEINLINE T* GetDestAndAdvance()
		{
			T* Ret = Register;
			Register += AdvanceOffset;
			return Ret;
		}
	};

	template<typename T>
	struct FExternalFuncConstHandler
	{
		uint16 ConstantIndex;
		T Constant;

#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
		FExternalFuncConstHandler(FVectorVMExternalFunctionContext& Context)
		{
			if (Context.UsingExperimentalVM)
			{
				check(false);
			}
			else
			{
				ConstantIndex = (Context.Legacy.DecodeU16() & VVM_EXT_FUNC_INPUT_LOC_MASK);
				Constant = (*Context.Legacy.GetConstant<T>(ConstantIndex));
			}
		}
#elif VECTORVM_SUPPORTS_EXPERIMENTAL
		FExternalFuncConstHandler(FVectorVMExternalFunctionContext& Context)
		{
			check(false);
		}
#elif VECTORVM_SUPPORTS_LEGACY
		FExternalFuncConstHandler(FVectorVMExternalFunctionContext& Context)
			: ConstantIndex(Context.DecodeU16() & VVM_EXT_FUNC_INPUT_LOC_MASK)
			, Constant(*Context.GetConstant<T>(ConstantIndex))
		{}
#endif
		FORCEINLINE const T& Get() { return Constant; }
		FORCEINLINE const T& GetAndAdvance() { return Constant; }
		FORCEINLINE void Advance() { }
	};
} // namespace VectorVM
