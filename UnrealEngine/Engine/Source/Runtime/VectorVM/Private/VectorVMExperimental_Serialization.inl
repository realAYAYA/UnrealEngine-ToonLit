// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef VVM_INCLUDE_SERIALIZATION

enum EVVMRegFlags
{
	VVMRegFlag_Int      = 1,
	VVMRegFlag_Clean    = 32,
	VVMRegFlag_Index    = 64,
	VVMRegFlag_Mismatch = 128,
};


//prototypes for stuff shared between the original and experimental VM
static FVectorVMSerializeInstruction *VVMSerGetNextInstruction(FVectorVMSerializeState *SerializeState, int InstructionIdx, int GlobalChunkIdx, int OpStart, int NumOps, uint64 Dt, uint64 DtDecode);

VECTORVM_API void FreeVectorVMSerializeState(FVectorVMSerializeState *SerializeState)
{
	if (SerializeState->FreeFn == nullptr)
	{
		return;
	}
	for (uint32 i = 0; i < SerializeState->NumExternalData; ++i)
	{
		if (SerializeState->ExternalData[i].Name)
		{
			SerializeState->FreeFn(SerializeState->ExternalData[i].Name, __FILE__, __LINE__);
		}
	}
	SerializeState->FreeFn(SerializeState->TempRegFlags, __FILE__, __LINE__);
	SerializeState->FreeFn(SerializeState->ExternalData, __FILE__, __LINE__);
	SerializeState->FreeFn(SerializeState->Bytecode    , __FILE__, __LINE__);
	SerializeState->FreeFn(SerializeState->Chunks      , __FILE__, __LINE__);

	for (uint32 i = 0; i < SerializeState->NumDataSets; ++i)
	{
		if (SerializeState->DataSets[i].InputBuffers)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].InputBuffers, __FILE__, __LINE__);
		}
		if (SerializeState->DataSets[i].OutputBuffers)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].OutputBuffers, __FILE__, __LINE__);
		}		
		if (SerializeState->DataSets[i].InputIDTable)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].InputIDTable, __FILE__, __LINE__);
		}
		if (SerializeState->DataSets[i].InputFreeIDTable)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].InputFreeIDTable, __FILE__, __LINE__);
		}
		if (SerializeState->DataSets[i].InputSpawnedIDTable)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].InputSpawnedIDTable, __FILE__, __LINE__);
		}
		if (SerializeState->DataSets[i].OutputIDTable)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].OutputIDTable, __FILE__, __LINE__);
		}
		if (SerializeState->DataSets[i].OutputFreeIDTable)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].OutputFreeIDTable, __FILE__, __LINE__);
		}
		if (SerializeState->DataSets[i].OutputSpawnedIDTable)
		{
			SerializeState->FreeFn(SerializeState->DataSets[i].OutputSpawnedIDTable, __FILE__, __LINE__);
		}
	}
	SerializeState->FreeFn(SerializeState->DataSets, __FILE__, __LINE__);
	for (uint32 i = 0; i < SerializeState->NumInstructions; ++i)
	{
		SerializeState->FreeFn(SerializeState->Instructions[i].TempRegisters, __FILE__, __LINE__);
		SerializeState->FreeFn(SerializeState->Instructions[i].TempRegisterFlags, __FILE__, __LINE__);
	}
	SerializeState->FreeFn(SerializeState->Instructions, __FILE__, __LINE__);
	SerializeState->FreeFn(SerializeState->ConstTableSizesInBytes, __FILE__, __LINE__);
	SerializeState->FreeFn(SerializeState->PreExecConstData, __FILE__, __LINE__);
	SerializeState->FreeFn(SerializeState->PostExecConstData, __FILE__, __LINE__);
}

static uint32 VectorVMSerializeSetError_(FVectorVMSerializeState *SerializeState, uint32 Flags, uint32 LineNum)
{
	SerializeState->Error.Line = LineNum;
	if (SerializeState->Error.CallbackFn)
	{
		SerializeState->Error.Flags = SerializeState->Error.CallbackFn(SerializeState, SerializeState->Error.Flags | Flags);
	}
	else
	{
		SerializeState->Error.Flags |= Flags;
	}
	if (SerializeState->Error.Flags & VVMSerErr_Fatal)
	{
		check(false); //hit the debugger
		FreeVectorVMSerializeState(SerializeState);
	}
	return SerializeState->Error.Flags;
}

#define VectorVMSerializeSetError(SerializeState, Flags)     VectorVMSerializeSetError_(SerializeState, Flags, __LINE__)


#if VECTORVM_SUPPORTS_EXPERIMENTAL

/*********************************************************************************************************************************************************************************************************************************
*** Serialization stuff for the Experimental VM
*********************************************************************************************************************************************************************************************************************************/
#define VVMSer_batchStart(...)
#define VVMSer_batchEnd(...)

#define VVMSer_chunkStart(...)
#define VVMSer_chunkEnd(...)

#define VVMSer_insStart(...)
#define VVMSer_insEndDecode(...)
#define VVMSer_insEnd(...)

#define VVMSer_batchStartExp(SerializeState, ...)
#define VVMSer_batchEndExp(SerializeState, ...)

#ifdef VVM_SERIALIZE_NO_WRITE
#define VVMSer_chunkStartExp(...)
#define VVMSer_chunkEndExp(...)
#define VVMSer_insStartExp(...)
#define VVMSer_insEndDecodeExp(...)
#define VVMSer_insEndExp(...)
#define VVMSer_initSerializationState(...)
#define VVMSer_instruction(...)
#define VVMSer_regUsed(...)
#else //VVM_SERIALIZE_NO_WRITE

#define VVMIsRegIdxTempReg(Idx) ((Idx) < ExecCtx->VVMState->NumTempRegisters)
#define VVMIsRegIdxConst(Idx)   ((Idx) >= ExecCtx->VVMState->NumTempRegisters && (Idx) < ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstsRemapped)
#define VVMIsRegIdxInput(Idx)   ((Idx) >= ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstsRemapped)

#define VVMSer_instruction(Type, NumParams)		//for (int vi = 0; vi <= (int)(NumParams); ++vi) {												\
												//	int r = (int)((uint16 *)InsPtr)[vi];														\
												//	char c = 'R';																				\
												//	if (r >= (int)(ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers)) {	\
												//		c = 'I';																				\
												//		r -= ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers;			\
												//	} else if (r >= (int)ExecCtx->VVMState->NumTempRegisters) {									\
												//		c = 'C';																				\
												//		r -= ExecCtx->VVMState->NumTempRegisters;												\
												//	}																							\
												//	printf(",%c%d", c, r);																		\
												//}
//if (SerializeState)                                                                     \
												//{                                                                                       \
												//    for (int vi = 0; vi <= (int)(NumParams); ++vi)                                      \
												//    {                                                                                   \
												//        if (VVMIsRegIdxTempReg(InsPtr[vi]))                                             \
												//        {                                                                               \
												//            SerializeState->TempRegFlags[InsPtr[vi]] = VVMRegFlag_Clean + (Type);       \
												//        }                                                                               \
												//    }                                                                                   \
												//}

#define VVMSer_regUsed(RegIdx, Type)		//if (SerializeState && VVMIsRegIdxTempReg(RegIdx))                                           \
                                            //{                                                                                           \
                                            //    SerializeState->TempRegFlags[RegIdx] = VVMRegFlag_Clean + (Type);                       \
                                            //}


#define VVMSer_chunkStartExp(SerializeState, ChunkIdx_, BatchIdx_)                          \
	FVectorVMSerializeChunk *SerializeChunk = nullptr;                                      \
	int VVMSerNumInstructionsThisChunk = 0;                                                 \
	int VVMSerGlobalChunkIdx = ExecIdx * ExecCtx->Internal.MaxChunksPerBatch + ChunkIdx_;   \
	if (SerializeState)                                                                     \
	{                                                                                       \
		SerializeChunk = SerializeState->Chunks + VVMSerGlobalChunkIdx;                     \
		SerializeChunk->ChunkIdx = VVMSerGlobalChunkIdx;                                    \
		SerializeChunk->BatchIdx = BatchIdx_;                                               \
		SerializeChunk->NumInstances = NumInstancesThisChunk;                               \
		SerializeChunk->StartInstance = StartInstanceThisChunk;                             \
		SerializeChunk->StartThreadID = FPlatformTLS::GetCurrentThreadId();                 \
		SerializeChunk->StartClock = FPlatformTime::Cycles64();                             \
	}

#define VVMSer_chunkEndExp(SerializeState, ...)                                                                                 \
	if (SerializeChunk)                                                                                                         \
	{                                                                                                                           \
		FPlatformAtomics::InterlockedOr(&SerializeState->ChunkComplete, (int64)((1ULL << (uint64)SerializeChunk->ChunkIdx)));   \
		SerializeChunk->EndThreadID = FPlatformTLS::GetCurrentThreadId();                                                       \
		SerializeChunk->EndClock = FPlatformTime::Cycles64();                                                                   \
	}

#define VVMSer_insStartExp(SerializeState, ...)             \
	const uint8 *VVMSerStartOpPtr = InsPtr;                 \
	uint64 VVMSerStartCycles = FPlatformTime::Cycles64();


#define VVMSer_insEndDecodeExp(SerializeState, ...)             \
	uint64 VVMSerEndDecodeCycles = FPlatformTime::Cycles64();

#define VVMSer_insEndExp(SerializeState, OpStart, NumOps, ...)                                              \
	if (SerializeState)                                                                                     \
	{                                                                                                       \
		uint64 VVMSerEndExecCycles = FPlatformTime::Cycles64();                                             \
		VVMSer_serializeInstruction(SerializeState, ExecCtx, BatchState, VVMSerNumInstructionsThisChunk++,  \
			StartInstanceThisChunk, NumInstancesThisChunk, VVMSerGlobalChunkIdx, NumLoops,                  \
			OpStart, NumOps, VVMSerStartCycles, VVMSerEndDecodeCycles, VVMSerEndExecCycles);                \
		uint64 VVMSerEndSerializeCycles = FPlatformTime::Cycles64();                                        \
		SerializeState->SerializeDt += VVMSerEndSerializeCycles - VVMSerEndExecCycles;                      \
		SerializeChunk->InsExecTime += VVMSerEndExecCycles - VVMSerStartCycles;                             \
	}

static void VVMSer_serializeInstruction(FVectorVMSerializeState *SerializeState, FVectorVMState *VectorVMState, FVectorVMBatchState *BatchState, 
										int InstructionIdx, int StartInstance, int NumInstancesThisChunk, int GlobalChunkIdx, int NumLoops, 
										int OpStart, int NumOps, uint64 StartInsCycles, uint64 EndDecodeCycles, uint64 EndInsCycles)
{
	
	FVectorVMSerializeInstruction *Ins = VVMSerGetNextInstruction(SerializeState, InstructionIdx, GlobalChunkIdx, OpStart, NumOps, EndInsCycles - StartInsCycles, EndDecodeCycles - StartInsCycles);
	if (Ins != nullptr)
	{
		check(Ins->TempRegisters != nullptr);
		for (uint32 i = 0; i < SerializeState->NumTempRegisters; ++i)
		{
			FMemory::Memcpy(Ins->TempRegisters + SerializeState->NumInstances * i + StartInstance, 
				BatchState->RegisterData + SerializeState->OptimizeCtx->NumConstsRemapped + i * NumLoops,
				sizeof(uint32) * NumInstancesThisChunk);
			check(Ins->TempRegisters != nullptr);
			if (GlobalChunkIdx == 0)
			{
				Ins->TempRegisterFlags[i]  = SerializeState->TempRegFlags[i];
			}
		}
	}
}

#define VVMSer_initSerializationState(VectorVMState, SerializeState, InitData, Flags) VVMSer_initSerializationState_(VectorVMState, SerializeState, InitData, (uint32)Flags)

static uint32 VVMSer_initSerializationState_(FVectorVMState *VVMState, FVectorVMSerializeState *SerializeState, FVectorVMOptimizeContext *OptimizeContext, FVectorVMExecContext *ExecContext,  uint32 Flags)
{
	if (SerializeState)
	{
		SerializeState->Error.Flags  = 0;

		SerializeState->Flags            = Flags;
		SerializeState->NumInstances     = ExecContext->NumInstances;
		SerializeState->NumTempRegisters = VVMState->NumTempRegisters;
		SerializeState->ConstDataCount   = VVMState->NumConstBuffers;
		SerializeState->ReallocFn        = VVMDefaultRealloc;
		SerializeState->FreeFn           = VVMDefaultFree;
		SerializeState->OptimizeCtx      = OptimizeContext;
		if (SerializeState->OptimizeCtx) {
			SerializeState->MaxExtFnRegisters = SerializeState->OptimizeCtx->MaxExtFnRegisters;
			SerializeState->MaxExtFnUsed = SerializeState->OptimizeCtx->MaxExtFnUsed >= 0 ? (uint32)SerializeState->OptimizeCtx->MaxExtFnUsed : 0;
		} else {
			SerializeState->MaxExtFnRegisters = 0;
			SerializeState->MaxExtFnUsed = 0;
		}

		SerializeState->NumTempRegFlags = VVMState->NumTempRegisters;
		for (int i = 0; i < ExecContext->DataSets.Num(); ++i)
		{
			if ((uint32)ExecContext->DataSets[i].InputRegisters.Num() > SerializeState->NumTempRegFlags)
			{
				SerializeState->NumTempRegFlags = ExecContext->DataSets[i].InputRegisters.Num();
			}
		}

		SerializeState->TempRegFlags = (uint8 *)SerializeState->ReallocFn(nullptr, SerializeState->NumTempRegFlags, __FILE__, __LINE__);
		if (SerializeState->TempRegFlags == nullptr)
		{
			return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_Init | VVMSerErr_Fatal);
		}
		for (uint32 i = 0; i < SerializeState->NumTempRegFlags; ++i)
		{
			SerializeState->TempRegFlags[i] = 0;
		}

		SerializeState->NumChunks = ExecContext->Internal.NumBatches * ExecContext->Internal.MaxChunksPerBatch;
		SerializeState->Chunks = (FVectorVMSerializeChunk *)SerializeState->ReallocFn(nullptr, sizeof(FVectorVMSerializeChunk) * SerializeState->NumChunks, __FILE__, __LINE__);
		if (SerializeState->Chunks == nullptr)
		{
			return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_Init | VVMSerErr_Fatal);
		}
		FMemory::Memset(SerializeState->Chunks, 0, sizeof(*SerializeState->Chunks) * SerializeState->NumChunks);
		
		if (SerializeVectorVMInputDataSets(SerializeState, ExecContext) != 0)
		{
			return SerializeState->Error.Flags;
		}
		
		

		int NumExternalFunctions = VVMState->NumExtFunctions;
		if (NumExternalFunctions != 0)
		{
			SerializeState->ExternalData = (FVectorVMSerializeExternalData *)SerializeState->ReallocFn(nullptr, sizeof(FVectorVMSerializeExternalData) * NumExternalFunctions, __FILE__, __LINE__);
			if (SerializeState->ExternalData == nullptr)
			{
				return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_Init | VVMSerErr_Fatal);
			}
			SerializeState->NumExternalData = NumExternalFunctions;
			for (int i = 0; i < NumExternalFunctions; ++i)
			{
				FVectorVMExtFunctionData *f = VVMState->ExtFunctionTable + i;
				FVectorVMSerializeExternalData *ExtData = SerializeState->ExternalData + i;
				ExtData->Name       = nullptr;
				ExtData->NameLen    = 0;
				ExtData->NumInputs  = f->NumInputs;
				ExtData->NumOutputs = f->NumOutputs;
			}
		}
		else
		{
			SerializeState->ExternalData    = nullptr;
			SerializeState->NumExternalData = 0;
		}

		SerializeState->Bytecode = (unsigned char *)SerializeState->ReallocFn(nullptr, OptimizeContext->NumBytecodeBytes, __FILE__, __LINE__);
		if (SerializeState->Bytecode == nullptr)
		{
			return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_Init | VVMSerErr_Fatal);
		}

		FMemory::Memcpy(SerializeState->Bytecode, OptimizeContext->OutputBytecode, OptimizeContext->NumBytecodeBytes);
		SerializeState->NumBytecodeBytes = OptimizeContext->NumBytecodeBytes;
	}
	return 0;
}
#endif //VVM_SERIALIZE_NO_WRITE

#else // VECTORVM_SUPPORTS_EXPERIMENTAL

/*********************************************************************************************************************************************************************************************************************************
*** Serialization stuff for the old VM
*********************************************************************************************************************************************************************************************************************************/
#define VVMSer_batchStartExp(...)
#define VVMSer_batchEndExp(...)

#define VVMSer_chunkStartExp(...)
#define VVMSer_chunkEndExp(...)

#define VVMSer_insStartExp(...)
#define VVMSer_insEndDecodeExp(...)
#define VVMSer_insEndExp(...)

#ifdef VVM_SERIALIZE_NO_WRITE
#define VVMSer_chunkStart(...)
#define VVMSer_chunkEnd(...)
#define VVMSer_insStart(...)
#define VVMSer_insEnd(...)
#else
#define VVMSer_chunkStart(Context, ChunkIdx, BatchIdx)                      \
	FVectorVMSerializeChunk *SerializeChunk = nullptr;                      \
	int VVMSerNumInstructionsThisChunk = 0;                                 \
	unsigned char *VVMSerStartCtxCode = (unsigned char*)Context.Code;       \
	if (SerializeState)                                                     \
	{                                                                       \
		SerializeChunk = SerializeState->Chunks + ChunkIdx;                 \
		SerializeChunk->ChunkIdx = ChunkIdx;                                \
		SerializeChunk->BatchIdx = BatchIdx;                                \
		SerializeChunk->ExecIdx  = BatchIdx;                                \
		SerializeChunk->NumInstances = NumInstancesThisChunk;               \
		SerializeChunk->StartThreadID = FPlatformTLS::GetCurrentThreadId(); \
		SerializeChunk->StartClock = FPlatformTime::Cycles64();             \
	}

#define VVMSer_chunkEnd(SerializeState, ...)																            \
	if (SerializeChunk)																						            \
	{																										            \
		FPlatformAtomics::InterlockedOr(&SerializeState->ChunkComplete, (1ULL << (uint64)SerializeChunk->ChunkIdx));    \
		SerializeChunk->EndThreadID = FPlatformTLS::GetCurrentThreadId();										        \
		SerializeChunk->EndClock = FPlatformTime::Cycles64();												            \
	}

#define VVMSer_insStart(Context, ...)										\
	uint64 VVMSerStartCycles = FPlatformTime::Cycles64();					\
	unsigned char *VVMSerCtxStartInsCode = (unsigned char*)Context.Code;	\

#define VVMSer_insEnd(Context, OpStart, NumOps, ...)																																												\
	if (SerializeState && Op != EVectorVMOp::done)                                                                                                                                                                                  \
	{																																												                                                \
		uint64 VVMSerEndExecCycles = FPlatformTime::Cycles64();																																										\
		VVMSer_serializeInstruction(SerializeState, Context, VVMSerNumInstructionsThisChunk++, StartInstance, NumInstancesThisChunk, ChunkIdx, OpStart, NumOps, VVMSerStartCycles, VVMSerEndExecCycles, VVMSerEndExecCycles);		\
		uint64 VVMSerEndSerializeCycles = FPlatformTime::Cycles64();																																								\
		SerializeState->SerializeDt += VVMSerEndSerializeCycles - VVMSerEndExecCycles;																																				\
		SerializeChunk->InsExecTime += VVMSerEndSerializeCycles - VVMSerEndExecCycles;																																				\
	}




static void VVMSer_serializeInstruction(FVectorVMSerializeState *SerializeState, FVectorVMContext &Context, 
										int InstructionIdx, int StartInstance, int NumInstancesThisChunk, int GlobalChunkIdx, 
										int OpStart, int NumOps, uint64 StartInsCycles, uint64 EndDecodeCycles, uint64 EndInsCycles)
{
	FVectorVMSerializeInstruction *Ins = VVMSerGetNextInstruction(SerializeState, InstructionIdx, GlobalChunkIdx, OpStart, NumOps, EndInsCycles - StartInsCycles, EndDecodeCycles - StartInsCycles);
	if (Ins)
	{
		for (uint32 i = 0; i < SerializeState->NumTempRegisters; ++i)
		{
			unsigned char *TempReg = (unsigned char *)Context.GetTempRegister(i);
			FMemory::Memcpy(Ins->TempRegisters + SerializeState->NumInstances * i + StartInstance, TempReg, sizeof(uint32) * NumInstancesThisChunk);
		}
	}
}
#endif //VVM_SERIALIZE_NO_WRITE

#endif // VECTORVM_SUPPORTS_EXPERIMENTAL

/*********************************************************************************************************************************************************************************************************************************
*** Stuff shared between the new and old VM
*********************************************************************************************************************************************************************************************************************************/

static FVectorVMSerializeInstruction *VVMSerGetNextInstruction(FVectorVMSerializeState *SerializeState, int InstructionIdx, int GlobalChunkIdx, int OpStart, int NumOps, uint64 Dt, uint64 DtDecode)
{
	FVectorVMSerializeInstruction *Ins = nullptr;
	if (GlobalChunkIdx == 0)
	{
		check(InstructionIdx == SerializeState->NumInstructions);
		if (SerializeState->NumInstructions + 1 >= SerializeState->NumInstructionsAllocated)
		{
			if (SerializeState->NumInstructionsAllocated == 0)
			{
				SerializeState->NumInstructionsAllocated = 1024;
				check(SerializeState->Instructions == nullptr);
			}
			else
			{
				SerializeState->NumInstructionsAllocated <<= 1;
			}
			FVectorVMSerializeInstruction *NewInstructions = (FVectorVMSerializeInstruction *)SerializeState->ReallocFn(SerializeState->Instructions, sizeof(FVectorVMSerializeInstruction) * SerializeState->NumInstructionsAllocated, __FILE__, __LINE__);
			if (NewInstructions == nullptr)
			{
				VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_Instruction | VVMSerErr_Fatal);
				return nullptr;
			}
			else
			{
				SerializeState->Instructions = NewInstructions;
			}
		}
		Ins = SerializeState->Instructions + SerializeState->NumInstructions++;

		//we only store this information on the first chunk
		Ins->OpStart       = OpStart;
		Ins->NumOps        = NumOps;
		Ins->Dt            = Dt;
		Ins->DtDecode      = DtDecode;
		Ins->TempRegisters = (uint32 *)SerializeState->ReallocFn(nullptr, sizeof(uint32) * SerializeState->NumInstances * SerializeState->NumTempRegisters, __FILE__, __LINE__); //alloc for *ALL* chunks
		if (Ins->TempRegisters == nullptr)
		{
			VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_Instruction | VVMSerErr_Fatal);
			return nullptr;
		}
		Ins->TempRegisterFlags = (unsigned char *)SerializeState->ReallocFn(nullptr, SerializeState->NumTempRegFlags, __FILE__, __LINE__);
		if (Ins->TempRegisterFlags == nullptr)
		{
			VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_Instruction | VVMSerErr_Fatal);
			return nullptr;
		}
		FMemory::Memset(Ins->TempRegisterFlags, 0, SerializeState->NumTempRegFlags);
	}
	else
	{
		if ((SerializeState->ChunkComplete & 1) == 0)
		{
			//we can't serialize the instructions until the first chunk is done since it's the one that allocates the memory.
			//if this ever becomes a bottleneck (who cares for serialization!?), but if it ever does I can the change the 
			//memory allocation such that each chunk's temp registers for each instruction gets grouped in per-chunk allocations
			//then the debugger can merge them all into a single array on load... but this is simpler and I doubt will become an issue
			uint32 SanityCount = 0;
			while ((SerializeState->ChunkComplete & 1) == 0 && SanityCount++ < (1ULL << 31))
			{
				FPlatformProcess::Yield();
			}
			check(SanityCount < (1ULL << 31) - 1);
		}
		check(InstructionIdx < (int)SerializeState->NumInstructions);
		Ins = SerializeState->Instructions + InstructionIdx;
	}
	return Ins;
}

#ifndef VVM_SERIALIZE_NO_IO
VECTORVM_API void SerializeVectorVMWriteToFile(FVectorVMSerializeState *SerializeState, uint8 WhichStateWritten, const wchar_t *Filename)
{
	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle *File           = PlatformFile.OpenWrite((TCHAR *)Filename);
	if (File)
	{
		File->Write(&WhichStateWritten, 1);		//1 = Exp, 2 = UE.  OR for both (1|2 = 3)
		File->Write((uint8 *)&SerializeState->NumInstances     , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->Flags            , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->ExecDt           , sizeof(uint64));
		File->Write((uint8 *)&SerializeState->SerializeDt      , sizeof(uint64));
		File->Write((uint8 *)&SerializeState->NumTempRegisters , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->ConstDataCount    , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->NumBytecodeBytes , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->NumInstructions  , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->NumDataSets      , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->NumChunks        , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->NumExternalData  , sizeof(uint32));
		File->Write((uint8 *)&SerializeState->MaxExtFnRegisters, sizeof(uint32));
		File->Write((uint8 *)&SerializeState->MaxExtFnUsed     , sizeof(uint32));

		
		//write the datasets
		for (uint32 i = 0; i < SerializeState->NumDataSets; ++i)
		{
			FVectorVMSerializeDataSet *DataSet = SerializeState->DataSets + i;
			uint32 IOFlag = (DataSet->InputBuffers ? 1 : 0) + (DataSet->OutputBuffers ? 2 : 0);
			File->Write((uint8 *)&IOFlag                            , sizeof(uint32));
			File->Write((uint8 *)&DataSet->InputOffset[0]           , sizeof(uint32));
			File->Write((uint8 *)&DataSet->InputOffset[1]           , sizeof(uint32));
			File->Write((uint8 *)&DataSet->InputOffset[2]           , sizeof(uint32));
			File->Write((uint8 *)&DataSet->OutputOffset[0]          , sizeof(uint32));
			File->Write((uint8 *)&DataSet->OutputOffset[1]          , sizeof(uint32));
			File->Write((uint8 *)&DataSet->OutputOffset[2]          , sizeof(uint32));

			File->Write((uint8 *)&DataSet->InputInstanceOffset      , sizeof(int32));
			File->Write((uint8 *)&DataSet->InputDataSetAccessIndex  , sizeof(int32));
			File->Write((uint8 *)&DataSet->InputIDAcquireTag        , sizeof(int32));
			File->Write((uint8 *)&DataSet->InputNumFreeIDs          , sizeof(int32));
			File->Write((uint8 *)&DataSet->InputMaxUsedIDs          , sizeof(int32));
			File->Write((uint8 *)&DataSet->InputNumSpawnedIDs       , sizeof(int32));

			File->Write((uint8 *)&DataSet->InputIDTableNum          , sizeof(int32));
			File->Write((uint8 *)&DataSet->InputFreeIDTableNum      , sizeof(int32));
			File->Write((uint8 *)&DataSet->InputSpawnedIDTableNum   , sizeof(int32));

			File->Write((uint8 *)&DataSet->OutputInstanceOffset     , sizeof(int32));
			File->Write((uint8 *)&DataSet->OutputDataSetAccessIndex , sizeof(int32));
			File->Write((uint8 *)&DataSet->OutputIDAcquireTag       , sizeof(int32));
			File->Write((uint8 *)&DataSet->OutputNumFreeIDs         , sizeof(int32));
			File->Write((uint8 *)&DataSet->OutputMaxUsedIDs         , sizeof(int32));
			File->Write((uint8 *)&DataSet->OutputNumSpawnedIDs      , sizeof(int32));

			File->Write((uint8 *)&DataSet->OutputIDTableNum         , sizeof(int32));
			File->Write((uint8 *)&DataSet->OutputFreeIDTableNum     , sizeof(int32));
			File->Write((uint8 *)&DataSet->OutputSpawnedIDTableNum  , sizeof(int32));

			uint32 TotalNumInstances = SerializeState->NumInstances + DataSet->InputInstanceOffset; //the VM could run 100 instances, but the dataset could have 150, and we start at 50, so just write all instances in the data set
			//write input bufers
			if (DataSet->InputBuffers)
			{
				uint32 NumBuffers = DataSet->InputOffset[2];
				File->Write((uint8 *)DataSet->InputBuffers          , sizeof(uint32) * NumBuffers * TotalNumInstances);
			}
			if (DataSet->OutputBuffers)
			{
				uint32 NumBuffers = DataSet->OutputOffset[2];
				File->Write((uint8 *)DataSet->OutputBuffers         , sizeof(uint32) * NumBuffers * TotalNumInstances);
			}
			//input tables
			if (DataSet->InputIDTableNum > 0)
			{
				File->Write((uint8 *)DataSet->InputIDTable          , sizeof(int32) * DataSet->InputIDTableNum);
			}
			if (DataSet->InputFreeIDTableNum > 0)
			{
				File->Write((uint8 *)DataSet->InputFreeIDTable      , sizeof(int32) * DataSet->InputFreeIDTableNum);
			}
			if (DataSet->InputSpawnedIDTableNum > 0)
			{
				File->Write((uint8 *)DataSet->InputSpawnedIDTable   , sizeof(int32) * DataSet->InputSpawnedIDTableNum);
			}
			//output tables
			if (DataSet->OutputIDTableNum > 0)
			{
				File->Write((uint8 *)DataSet->OutputIDTable         , sizeof(int32) * DataSet->OutputIDTableNum);
			}
			if (DataSet->OutputFreeIDTableNum > 0)
			{
				File->Write((uint8 *)DataSet->OutputFreeIDTable     , sizeof(int32) * DataSet->OutputFreeIDTableNum);
			}
			if (DataSet->OutputSpawnedIDTableNum > 0)
			{
				File->Write((uint8 *)DataSet->OutputSpawnedIDTable  , sizeof(int32) * DataSet->OutputSpawnedIDTableNum);
			}
		}

		//write the chunk info
		for (uint32 i = 0; i < SerializeState->NumChunks; ++i)
		{
			FVectorVMSerializeChunk *Chunk = SerializeState->Chunks + i;
			File->Write((uint8 *)&Chunk->ChunkIdx		, sizeof(uint32));
			File->Write((uint8 *)&Chunk->BatchIdx		, sizeof(uint32));
			File->Write((uint8 *)&Chunk->NumInstances	, sizeof(uint32));
			File->Write((uint8 *)&Chunk->StartThreadID	, sizeof(uint32));
			File->Write((uint8 *)&Chunk->EndThreadID	, sizeof(uint32));
			File->Write((uint8 *)&Chunk->StartClock	    , sizeof(uint64));
			File->Write((uint8 *)&Chunk->EndClock		, sizeof(uint64));
		}

		{ //write const data
			int ConstTableSizesInBytes = 0;
			for (uint32 i = 0; i < SerializeState->ConstDataCount; ++i) {
				ConstTableSizesInBytes += SerializeState->ConstTableSizesInBytes[i];
			}
			File->Write((uint8 *)SerializeState->ConstTableSizesInBytes, sizeof(uint32) * SerializeState->ConstDataCount);
			File->Write((uint8 *)SerializeState->PreExecConstData      , ConstTableSizesInBytes);
			File->Write((uint8 *)SerializeState->PostExecConstData     , ConstTableSizesInBytes);
		}

		//write bytecode
		if (SerializeState->NumBytecodeBytes > 0)
		{
			File->Write((uint8 *)SerializeState->Bytecode, SerializeState->NumBytecodeBytes);
		}

		//write instructions
		for (uint32 i = 0; i < SerializeState->NumInstructions; ++i)
		{
			FVectorVMSerializeInstruction *Ins = SerializeState->Instructions + i;
			File->Write((uint8 *)&Ins->OpStart      , sizeof(uint32));
			File->Write((uint8 *)&Ins->NumOps       , sizeof(uint32));
			File->Write((uint8 *)&Ins->Dt           , sizeof(uint64));
			File->Write((uint8 *)&Ins->DtDecode     , sizeof(uint64));
			File->Write((uint8 *)Ins->TempRegisters , sizeof(uint32) * SerializeState->NumInstances * SerializeState->NumTempRegisters);
			if (SerializeState->NumBytecodeBytes)
			{
				File->Write((uint8 *)Ins->TempRegisterFlags, SerializeState->NumTempRegisters);
			}
		}

		//write external function data
		for (uint32 i = 0; i < SerializeState->NumExternalData; ++i)
		{
			FVectorVMSerializeExternalData *ExtData = SerializeState->ExternalData + i;
			File->Write((uint8 *)&ExtData->NameLen    , sizeof(uint16));
			File->Write((uint8 *)ExtData->Name        , sizeof(uint16) * ExtData->NameLen);
			File->Write((uint8 *)&ExtData->NumInputs  , sizeof(uint16));
			File->Write((uint8 *)&ExtData->NumOutputs , sizeof(uint16));
		}
		delete File;
	}
}

#else

VECTORVM_API void SerializeVectorVMWriteToFile(FVectorVMSerializeState *SerializeState, uint8 WhichStateWritten, const wchar_t *Filename)
{

}

#endif //VVM_SERIALIZE_NO_IO


static uint32 SerializeConstData(FVectorVMSerializeState *SerializeState, const uint8 * const *ConstantTableData, const int *ConstantTableSizes, int32 ConstantTableCount, uint32 **OutConstData, uint32 **OutConstTableSizes)
{
	int TotalConstNumBytes = 0;
	for (int i = 0; i < ConstantTableCount; ++i)
	{
		TotalConstNumBytes += ConstantTableSizes[i];
	}
	*OutConstData = (uint32 *)SerializeState->ReallocFn(nullptr, TotalConstNumBytes, __FILE__, __LINE__);
	if (*OutConstData == nullptr)
	{
		VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_ConstData | VVMSerErr_Fatal);
		return 0;
	}
	if (OutConstTableSizes) {
		*OutConstTableSizes = (uint32 *)SerializeState->ReallocFn(nullptr, sizeof(uint32) * ConstantTableCount, __FILE__, __LINE__);
		if (*OutConstTableSizes == nullptr)
		{
			VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_ConstData | VVMSerErr_Fatal);
			return 0;
		}
		for (int i = 0; i < ConstantTableCount; ++i)
		{
			(*OutConstTableSizes)[i] = ConstantTableSizes[i];
		}
	}
	uint8 *CPtr = (uint8 *)*OutConstData;
	for (int i = 0; i < ConstantTableCount; ++i)
	{
		FMemory::Memcpy(CPtr, ConstantTableData[i], ConstantTableSizes[i]);
		CPtr += ConstantTableSizes[i];
	}
	return ConstantTableCount;
}

VECTORVM_API uint32 SerializeVectorVMInputDataSets(FVectorVMSerializeState *SerializeState, TArrayView<FDataSetMeta> DataSets, const uint8 * const *ConstantTableData, const int *ConstantTableSizes, int32 ConstantTableCount)
{
	SerializeState->NumDataSets = DataSets.Num();
	SerializeState->DataSets = (FVectorVMSerializeDataSet *)SerializeState->ReallocFn(nullptr, sizeof(FVectorVMSerializeDataSet) * SerializeState->NumDataSets, __FILE__, __LINE__);
	if (SerializeState->DataSets == nullptr)
	{
		return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_InputDataSets | VVMSerErr_Fatal);
	}
	FMemory::Memset(SerializeState->DataSets, 0, sizeof(FVectorVMSerializeDataSet) * SerializeState->NumDataSets);
	for (uint32 i = 0; i < SerializeState->NumDataSets; ++i)
	{
		FVectorVMSerializeDataSet *DsDst = SerializeState->DataSets + i;
		FDataSetMeta *DsSrc = &DataSets[i];
		DsDst->InputInstanceOffset      = DsSrc->InstanceOffset;
		DsDst->InputDataSetAccessIndex  = DsSrc->DataSetAccessIndex;
		DsDst->InputIDAcquireTag        = DsSrc->IDAcquireTag;
		DsDst->InputNumFreeIDs          = *DsSrc->NumFreeIDs;
		DsDst->InputMaxUsedIDs          = *DsSrc->MaxUsedID;
		DsDst->InputNumSpawnedIDs       = *DsSrc->NumSpawnedIDs;
		
		//we serialize the whole dataset... even if there's more instances than we use just so the offsets are correct when we sim
		uint32 TotalNumInstances        = SerializeState->NumInstances + DsDst->InputInstanceOffset;
		uint32 **SrcInputBuffers        = (uint32 **)DsSrc->InputRegisters.GetData();
		if (SrcInputBuffers)
		{
			uint32 TotalNumBuffers = DsSrc->InputRegisterTypeOffsets[2];
			DsDst->InputBuffers = (uint32 *)SerializeState->ReallocFn(nullptr, sizeof(uint32) * TotalNumBuffers * TotalNumInstances, __FILE__, __LINE__);
			if (DsDst->InputBuffers == nullptr)
			{
				return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_InputDataSets | VVMSerErr_Fatal);
			}
			for (uint32 j = 0; j < TotalNumBuffers; ++j)
			{
				FMemory::Memcpy(DsDst->InputBuffers + j * TotalNumInstances, SrcInputBuffers[j], sizeof(uint32) * TotalNumInstances);
			}
			DsDst->InputOffset[0] = DsSrc->InputRegisterTypeOffsets[0];
			DsDst->InputOffset[1] = DsSrc->InputRegisterTypeOffsets[1];
			DsDst->InputOffset[2] = DsSrc->InputRegisterTypeOffsets[2];
		}
		else
		{
			DsDst->InputOffset[0] = 0;
			DsDst->InputOffset[1] = 0;
			DsDst->InputOffset[2] = 0;
			DsDst->InputBuffers   = nullptr;
		}
		
		//IDTable
		if (DsSrc->IDTable && DsSrc->IDTable->Num() > 0)
		{
			DsDst->InputIDTable = (int32 *)SerializeState->ReallocFn(nullptr, sizeof(int) * DsSrc->IDTable->Num(), __FILE__, __LINE__);
			if (DsDst->InputIDTable == nullptr)
			{
				return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_InputDataSets | VVMSerErr_Fatal);
			}
			FMemory::Memcpy(DsDst->InputIDTable, DsSrc->IDTable->GetData(), sizeof(int32) * DsSrc->IDTable->Num());
			DsDst->InputIDTableNum = DsSrc->IDTable->Num();
		}
		else
		{
			DsDst->InputIDTable    = nullptr;
			DsDst->InputIDTableNum = 0;
		}

		//FreeIDTable
		if (DsSrc->FreeIDTable && DsSrc->FreeIDTable->Num() > 0)
		{
			DsDst->InputFreeIDTable = (int32 *)SerializeState->ReallocFn(nullptr, sizeof(int) * DsSrc->FreeIDTable->Num(), __FILE__, __LINE__);
			if (DsDst->InputFreeIDTable == nullptr)
			{
				return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_InputDataSets | VVMSerErr_Fatal);
			}
			FMemory::Memcpy(DsDst->InputFreeIDTable, DsSrc->FreeIDTable->GetData(), sizeof(int32) * DsSrc->FreeIDTable->Num());
			DsDst->InputFreeIDTableNum = DsSrc->FreeIDTable->Num();
		}
		else
		{
			DsDst->InputFreeIDTable    = nullptr;
			DsDst->InputFreeIDTableNum = 0;
		}

		//SpawnedIDTable
		if (DsSrc->SpawnedIDsTable && DsSrc->SpawnedIDsTable->Num() > 0)
		{
			DsDst->InputSpawnedIDTable = (int32 *)SerializeState->ReallocFn(nullptr, sizeof(int) * DsSrc->SpawnedIDsTable->Num(), __FILE__, __LINE__);
			if (DsDst->InputSpawnedIDTable == nullptr)
			{
				return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_InputDataSets | VVMSerErr_Fatal);
			}
			FMemory::Memcpy(DsDst->InputSpawnedIDTable, DsSrc->SpawnedIDsTable->GetData(), sizeof(int32) * DsSrc->SpawnedIDsTable->Num());
			DsDst->InputSpawnedIDTableNum = DsSrc->SpawnedIDsTable->Num();
		}
		else
		{
			DsDst->InputSpawnedIDTable    = nullptr;
			DsDst->InputSpawnedIDTableNum = 0;
		}
	}
	SerializeState->ConstDataCount = SerializeConstData(SerializeState, ConstantTableData, ConstantTableSizes, ConstantTableCount, &SerializeState->PreExecConstData, &SerializeState->ConstTableSizesInBytes);
	return SerializeState->Error.Flags;
}

VECTORVM_API uint32 SerializeVectorVMOutputDataSets(FVectorVMSerializeState *SerializeState, TArrayView<FDataSetMeta> DataSets, const uint8 * const *ConstantTableData, const int *ConstantTableSizes, int32 ConstantTableCount)
{
	if (SerializeState->NumDataSets == DataSets.Num()) //must have already serialized the input data sets
	{
		for (int i = 0; i < DataSets.Num(); ++i)
		{
			FVectorVMSerializeDataSet *DsDst = SerializeState->DataSets + i;
			FDataSetMeta *DsSrc = &DataSets[i];
			DsDst->OutputOffset[0]			= DsSrc->OutputRegisterTypeOffsets[0];
			DsDst->OutputOffset[1]			= DsSrc->OutputRegisterTypeOffsets[1];
			DsDst->OutputOffset[2]			= DsSrc->OutputRegisterTypeOffsets[2];
			DsDst->OutputInstanceOffset		= DsSrc->InstanceOffset;
			DsDst->OutputDataSetAccessIndex	= DsSrc->DataSetAccessIndex;
			DsDst->OutputIDAcquireTag		= DsSrc->IDAcquireTag;
			DsDst->OutputNumFreeIDs         = *DsSrc->NumFreeIDs;
			DsDst->OutputMaxUsedIDs         = *DsSrc->MaxUsedID;
			DsDst->OutputNumSpawnedIDs      = *DsSrc->NumSpawnedIDs;

			uint32 TotalNumInstances = SerializeState->NumInstances + DsDst->OutputInstanceOffset;
			uint32 **SrcOutputBuffers = (uint32 **)DsSrc->OutputRegisters.GetData();
			if (SrcOutputBuffers)
			{
				uint32 TotalNumBuffers = DsSrc->OutputRegisterTypeOffsets[2];
				DsDst->OutputBuffers = (uint32 *)SerializeState->ReallocFn(nullptr, sizeof(uint32) * TotalNumBuffers * TotalNumInstances, __FILE__, __LINE__);
				if (DsDst->OutputBuffers == nullptr)
				{
					return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_OutputDataSets | VVMSerErr_Fatal);
				}
				for (uint32 j = 0; j < TotalNumBuffers; ++j)
				{
					FMemory::Memcpy(DsDst->OutputBuffers + j * TotalNumInstances, SrcOutputBuffers[j], sizeof(uint32) * TotalNumInstances);
				}
			}
			else
			{
				DsDst->OutputBuffers = nullptr;
			}

			//IDTable
			if (DsSrc->IDTable && DsSrc->IDTable->Num() > 0)
			{
				DsDst->OutputIDTable = (int32 *)SerializeState->ReallocFn(nullptr, sizeof(int) * DsSrc->IDTable->Num(), __FILE__, __LINE__);
				if (DsDst->OutputIDTable == nullptr)
				{
					return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_OutputDataSets | VVMSerErr_Fatal);
				}
				FMemory::Memcpy(DsDst->OutputIDTable, DsSrc->IDTable->GetData(), sizeof(int32) * DsSrc->IDTable->Num());
				DsDst->OutputIDTableNum = DsSrc->IDTable->Num();
			}
			else
			{
				DsDst->OutputIDTable    = nullptr;
				DsDst->OutputIDTableNum = 0;
			}

			//FreeIDTable
			if (DsSrc->FreeIDTable && DsSrc->FreeIDTable->Num() > 0)
			{
				DsDst->OutputFreeIDTable = (int32 *)SerializeState->ReallocFn(nullptr, sizeof(int) * DsSrc->FreeIDTable->Num(), __FILE__, __LINE__);
				if (DsDst->OutputFreeIDTable == nullptr)
				{
					return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_OutputDataSets | VVMSerErr_Fatal);
				}
				FMemory::Memcpy(DsDst->OutputFreeIDTable, DsSrc->FreeIDTable->GetData(), sizeof(int32) * DsSrc->FreeIDTable->Num());
				DsDst->OutputFreeIDTableNum = DsSrc->FreeIDTable->Num();
			}
			else
			{
				DsDst->OutputFreeIDTable    = nullptr;
				DsDst->OutputFreeIDTableNum = 0;
			}

			//SpawnedIDTable
			if (DsSrc->SpawnedIDsTable && DsSrc->SpawnedIDsTable->Num() > 0)
			{
				DsDst->OutputSpawnedIDTable = (int32 *)SerializeState->ReallocFn(nullptr, sizeof(int) * DsSrc->SpawnedIDsTable->Num(), __FILE__, __LINE__);
				if (DsDst->OutputSpawnedIDTable == nullptr)
				{
					return VectorVMSerializeSetError(SerializeState, VVMSerErr_OutOfMemory | VVMSerErr_OutputDataSets | VVMSerErr_Fatal);
				}
				FMemory::Memcpy(DsDst->OutputSpawnedIDTable, DsSrc->SpawnedIDsTable->GetData(), sizeof(int32) * DsSrc->SpawnedIDsTable->Num());
				DsDst->OutputSpawnedIDTableNum = DsSrc->SpawnedIDsTable->Num();
			}
			else
			{
				DsDst->OutputSpawnedIDTable    = nullptr;
				DsDst->OutputSpawnedIDTableNum = 0;
			}

		}
	}
	else
	{
		VectorVMSerializeSetError(SerializeState, VVMSerErr_OutputDataSets);
		for (uint32 i = 0; i < SerializeState->NumDataSets; ++i)
		{
			FVectorVMSerializeDataSet *DataSet = SerializeState->DataSets + i;
			DataSet->OutputOffset[0] = 0;
			DataSet->OutputOffset[1] = 0;
			DataSet->OutputOffset[2] = 0;
			DataSet->OutputBuffers = nullptr;
			DataSet->OutputIDTable = nullptr;
			DataSet->OutputIDTableNum = 0;
			DataSet->OutputFreeIDTable = nullptr;
			DataSet->OutputFreeIDTableNum = 0;
			DataSet->OutputSpawnedIDTable = nullptr;
			DataSet->OutputSpawnedIDTableNum = 0;
		}
	}
	if (SerializeState->Error.Flags == 0)
	{
		SerializeState->ConstDataCount = SerializeConstData(SerializeState, ConstantTableData, ConstantTableSizes, ConstantTableCount, &SerializeState->PostExecConstData, nullptr);
	}
	return SerializeState->Error.Flags;
}

#if VECTORVM_SUPPORTS_EXPERIMENTAL
VECTORVM_API uint32 SerializeVectorVMInputDataSets(FVectorVMSerializeState *SerializeState, FVectorVMExecContext *ExecContext)
{
	return SerializeVectorVMInputDataSets(SerializeState, ExecContext->DataSets, ExecContext->ConstantTableData, ExecContext->ConstantTableSizes, ExecContext->ConstantTableCount);
}

VECTORVM_API uint32 SerializeVectorVMOutputDataSets(FVectorVMSerializeState *SerializeState, FVectorVMExecContext *ExecContext)
{
	return SerializeVectorVMOutputDataSets(SerializeState, ExecContext->DataSets, ExecContext->ConstantTableData, ExecContext->ConstantTableSizes, ExecContext->ConstantTableCount);
}
#endif // #if VECTORVM_SUPPORTS_EXPERIMENTAL

#else //VVM_INCLUDE_SERIALIZATION

#define VVMSer_batchStart(...)
#define VVMSer_batchEnd(...)

#define VVMSer_chunkStart(...)
#define VVMSer_chunkEnd(...)

#define VVMSer_insStart(...)
#define VVMSer_insEndDecode(...)
#define VVMSer_insEnd(...)

#define VVMSer_batchStartExp(...)
#define VVMSer_batchEndExp(...)

#define VVMSer_chunkStartExp(...)
#define VVMSer_chunkEndExp(...)

#define VVMSer_insStartExp(...)
#define VVMSer_insEndDecodeExp(...)
#define VVMSer_insEndExp(...)

#define VVMSer_instruction(...)
#define VVMSer_regUsed(...)

#define VVMSer_initSerializationState(...)
#define FreeVectorVMSerializeState(...)

VECTORVM_API uint64 VVMSer_cmpStates(FVectorVMSerializeState *S0, FVectorVMSerializeState *S1, uint32 Flags)
{
	return 0;
}

VECTORVM_API uint32  SerializeVectorVMInputDataSets  (FVectorVMSerializeState *SerializeState, struct FVectorVMExecContext *ExecContext) {
	return 0;
}

VECTORVM_API uint32  SerializeVectorVMOutputDataSets (FVectorVMSerializeState *SerializeState, struct FVectorVMExecContext *ExecContext) {
	return 0;
}

VECTORVM_API uint32 SerializeVectorVMInputDataSets(FVectorVMSerializeState *SerializeState, TArrayView<FDataSetMeta>, const uint8 * const *ConstantTableData, const int *ConstantTableSizes, int32 ConstantTableCount)
{
	return 0;
}

VECTORVM_API uint32 SerializeVectorVMOutputDataSets(FVectorVMSerializeState *SerializeState, TArrayView<FDataSetMeta>, const uint8 * const *ConstantTableData, const int *ConstantTableSizes, int32 ConstantTableCount)
{
	return 0;
}

#endif //VVM_INCLUDE_SERIALIZATION
