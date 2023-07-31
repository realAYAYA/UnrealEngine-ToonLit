// Copyright Epic Games, Inc. All Rights Reserved.


#if VECTORVM_SUPPORTS_COMPUTED_GOTO
#define VVM_OP_CASE(op)	jmp_lbl_##op:
#define VVM_OP_NEXT     goto *jmp_tbl[InsPtr[-1]]
#define VVM_OP_START	VVM_OP_NEXT;

static const void* jmp_tbl[] = {
#	define VVM_OP_XM(op, ...) &&jmp_lbl_##op,
	VVM_OP_XM_LIST
#	undef VVM_OP_XM
};
#else
#define VVM_OP_START    switch ((EVectorVMOp)InsPtr[-1])
#define VVM_OP_CASE(op)	case EVectorVMOp::op:
#define VVM_OP_NEXT     break
#endif

{ //set the pointers to deal with the correct chunk offset.
	int DstOffset = ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers;
	int SrcOffset = DstOffset + ExecCtx->VVMState->NumInputBuffers;
	for (int i = 0; i < (int)ExecCtx->VVMState->NumInputBuffers; ++i)
	{
		if (BatchState->RegIncTable[DstOffset + i] != 0) //don't offset the no-advance inputs
		{
			BatchState->RegPtrTable[DstOffset + i] = BatchState->RegPtrTable[SrcOffset + i] + (uint64)((uint64)StartInstanceThisChunk << 2ULL);
		}
		//int num_cache_lines = VVM_MIN(1 + (NumLoops >> 1), 4);
		//for (int j = 0; j < num_cache_lines; ++j) {
			//_mm_prefetch((char *)BatchState->RegPtrTable[DstOffset + i], _MM_HINT_T1);
		//}
	}
}
const uint8 *InsPtr        = ExecCtx->VVMState->Bytecode + 1;
const uint8 *InsPtrEnd     = InsPtr + ExecCtx->VVMState->NumBytecodeBytes;

for (uint32 i = 0; i < ExecCtx->VVMState->NumOutputDataSets; ++i)
{
	BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[i] = 0;
	BatchState->ChunkLocalData.NumOutputPerDataSet[i] = 0;
}

for (;;)
{
	VVM_execCoreSetupIncVars
	VVM_execCoreSetupPtrVars

	VVM_OP_START {
		VVM_OP_CASE(done)                             return;
		VVM_OP_CASE(add)                              VVM_execVecIns2f(VectorAdd);            VVM_OP_NEXT;
		VVM_OP_CASE(sub)                              VVM_execVecIns2f(VectorSubtract);       VVM_OP_NEXT;
		VVM_OP_CASE(mul)                              VVM_execVecIns2f(VectorMultiply);       VVM_OP_NEXT;
		VVM_OP_CASE(div)                              VVM_execVecIns2f(VVM_safeIns_div);      VVM_OP_NEXT;
		VVM_OP_CASE(mad)                              VVM_execVecIns3f(VectorMultiplyAdd);    VVM_OP_NEXT;
		VVM_OP_CASE(lerp)                             VVM_execVecIns3f(VectorLerp);           VVM_OP_NEXT;
		VVM_OP_CASE(rcp)                              VVM_execVecIns1f(VVM_safeIns_rcp);      VVM_OP_NEXT;
		VVM_OP_CASE(rsq)                              VVM_execVecIns1f(VVM_safe_rsq);         VVM_OP_NEXT;
		VVM_OP_CASE(sqrt)                             VVM_execVecIns1f(VVM_safe_sqrt);        VVM_OP_NEXT;
		VVM_OP_CASE(neg)                              VVM_execVecIns1f(VectorNegate);         VVM_OP_NEXT;
		VVM_OP_CASE(abs)                              VVM_execVecIns1f(VectorAbs);            VVM_OP_NEXT;
		VVM_OP_CASE(exp)                              VVM_execVecIns1f(VectorExp);            VVM_OP_NEXT;
		VVM_OP_CASE(exp2)                             VVM_execVecIns1f(VectorExp2);           VVM_OP_NEXT;
		VVM_OP_CASE(log)                              VVM_execVecIns1f(VVM_safe_log);         VVM_OP_NEXT;
		VVM_OP_CASE(log2)                             VVM_execVecIns1f(VectorLog2);           VVM_OP_NEXT;
		VVM_OP_CASE(sin)                              VVM_execVecIns1f(VectorSin);            VVM_OP_NEXT;
		VVM_OP_CASE(cos)                              VVM_execVecIns1f(VectorCos);            VVM_OP_NEXT;
		VVM_OP_CASE(tan)                              VVM_execVecIns1f(VectorTan);            VVM_OP_NEXT;
		VVM_OP_CASE(asin)                             VVM_execVecIns1f(VectorASin);           VVM_OP_NEXT;
		VVM_OP_CASE(acos)                             VVM_execVecIns1f(VVM_vecACosFast);      VVM_OP_NEXT;
		VVM_OP_CASE(atan)                             VVM_execVecIns1f(VectorATan);           VVM_OP_NEXT;
		VVM_OP_CASE(atan2)                            VVM_execVecIns2f(VectorATan2);          VVM_OP_NEXT;
		VVM_OP_CASE(ceil)                             VVM_execVecIns1f(VectorCeil);           VVM_OP_NEXT;
		VVM_OP_CASE(floor)                            VVM_execVecIns1f(VectorFloor);          VVM_OP_NEXT;
		VVM_OP_CASE(fmod)                             VVM_execVecIns2f(VectorMod);            VVM_OP_NEXT;
		VVM_OP_CASE(frac)                             VVM_execVecIns1f(VectorFractional);     VVM_OP_NEXT;
		VVM_OP_CASE(trunc)                            VVM_execVecIns1f(VectorTruncate);       VVM_OP_NEXT;
		VVM_OP_CASE(clamp)                            VVM_execVecIns3f(VectorClamp);          VVM_OP_NEXT;
		VVM_OP_CASE(min)                              VVM_execVecIns2f(VectorMin);            VVM_OP_NEXT;
 		VVM_OP_CASE(max)                              VVM_execVecIns2f(VectorMax);            VVM_OP_NEXT;
		VVM_OP_CASE(pow)                              VVM_execVecIns2f(VVM_safe_pow);         VVM_OP_NEXT;
		VVM_OP_CASE(round)                            VVM_execVecIns1f(VectorRound);          VVM_OP_NEXT;
		VVM_OP_CASE(sign)                             VVM_execVecIns1f(VectorSign);           VVM_OP_NEXT;
		VVM_OP_CASE(step)                             VVM_execVecIns2f(VVM_vecStep);          VVM_OP_NEXT;
		VVM_OP_CASE(random)                           VVM_execVecIns1f(VVM_random);           VVM_OP_NEXT;
		VVM_OP_CASE(noise)                            check(false);                           VVM_OP_NEXT;
		VVM_OP_CASE(cmplt)                            VVM_execVecIns2f(VectorCompareLT);      VVM_OP_NEXT;
		VVM_OP_CASE(cmple)                            VVM_execVecIns2f(VectorCompareLE);      VVM_OP_NEXT;
		VVM_OP_CASE(cmpgt)                            VVM_execVecIns2f(VectorCompareGT);      VVM_OP_NEXT;
		VVM_OP_CASE(cmpge)                            VVM_execVecIns2f(VectorCompareGE);      VVM_OP_NEXT;
 		VVM_OP_CASE(cmpeq)                            VVM_execVecIns2f(VectorCompareEQ);      VVM_OP_NEXT;
		VVM_OP_CASE(cmpneq)                           VVM_execVecIns2f(VectorCompareNE);      VVM_OP_NEXT;
		VVM_OP_CASE(select)                           VVM_execVecIns3f(VectorSelect);         VVM_OP_NEXT;
		VVM_OP_CASE(addi)                             VVM_execVecIns2i(VectorIntAdd);         VVM_OP_NEXT;
		VVM_OP_CASE(subi)                             VVM_execVecIns2i(VectorIntSubtract);    VVM_OP_NEXT;
		VVM_OP_CASE(muli)                             VVM_execVecIns2i(VectorIntMultiply);    VVM_OP_NEXT;
		VVM_OP_CASE(divi)                             VVM_execVecIns2i(VVMIntDiv);            VVM_OP_NEXT;
		VVM_OP_CASE(clampi)                           VVM_execVecIns3i(VectorIntClamp);       VVM_OP_NEXT;
		VVM_OP_CASE(mini)                             VVM_execVecIns2i(VectorIntMin);         VVM_OP_NEXT;
		VVM_OP_CASE(maxi)                             VVM_execVecIns2i(VectorIntMax);         VVM_OP_NEXT;
		VVM_OP_CASE(absi)                             VVM_execVecIns1i(VectorIntAbs);         VVM_OP_NEXT;
		VVM_OP_CASE(negi)                             VVM_execVecIns1i(VectorIntNegate);      VVM_OP_NEXT;
		VVM_OP_CASE(signi)                            VVM_execVecIns1i(VectorIntSign);        VVM_OP_NEXT;
		VVM_OP_CASE(randomi)                          VVM_execVecIns1i(VVM_randomi);          VVM_OP_NEXT;
		VVM_OP_CASE(cmplti)                           VVM_execVecIns2i(VectorIntCompareLT);   VVM_OP_NEXT;
		VVM_OP_CASE(cmplei)                           VVM_execVecIns2i(VectorIntCompareLE);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpgti)                           VVM_execVecIns2i(VectorIntCompareGT);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpgei)                           VVM_execVecIns2i(VectorIntCompareGE);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpeqi)                           VVM_execVecIns2i(VectorIntCompareEQ);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpneqi)                          VVM_execVecIns2i(VectorIntCompareNEQ);  VVM_OP_NEXT;
		VVM_OP_CASE(bit_and)                          VVM_execVecIns2i(VectorIntAnd);         VVM_OP_NEXT;
		VVM_OP_CASE(bit_or)                           VVM_execVecIns2i(VectorIntOr);          VVM_OP_NEXT;
		VVM_OP_CASE(bit_xor)                          VVM_execVecIns2i(VectorIntXor);         VVM_OP_NEXT;
		VVM_OP_CASE(bit_not)                          VVM_execVecIns1i(VectorIntNot);         VVM_OP_NEXT;
		VVM_OP_CASE(bit_lshift)                       VVM_execVecIns2i(VVMIntLShift);         VVM_OP_NEXT;
		VVM_OP_CASE(bit_rshift)                       VVM_execVecIns2i(VVMIntRShift);         VVM_OP_NEXT;
		VVM_OP_CASE(logic_and)                        VVM_execVecIns2i(VectorIntAnd);         VVM_OP_NEXT;
		VVM_OP_CASE(logic_or)                         VVM_execVecIns2i(VectorIntOr);          VVM_OP_NEXT;
		VVM_OP_CASE(logic_xor)                        VVM_execVecIns2i(VectorIntXor);         VVM_OP_NEXT;
		VVM_OP_CASE(logic_not)                        VVM_execVecIns1i(VectorIntNot);         VVM_OP_NEXT;
		VVM_OP_CASE(f2i)                              VVM_execVecIns1i(VVMf2i);               VVM_OP_NEXT;
		VVM_OP_CASE(i2f)                              VVM_execVecIns1f(VVMi2f);               VVM_OP_NEXT;
		VVM_OP_CASE(f2b)                              VVM_execVecIns1f(VVM_vecFloatToBool);   VVM_OP_NEXT;
		VVM_OP_CASE(b2f)                              VVM_execVecIns1f(VVM_vecBoolToFloat);   VVM_OP_NEXT;
		VVM_OP_CASE(i2b)                              VVM_execVecIns1i(VVM_vecIntToBool);     VVM_OP_NEXT;
		VVM_OP_CASE(b2i)                              VVM_execVecIns1i(VVM_vecBoolToInt);     VVM_OP_NEXT;
		VVM_OP_CASE(outputdata_float)
		VVM_OP_CASE(outputdata_int32)                 VVM_output32;                           VVM_OP_NEXT;
		VVM_OP_CASE(outputdata_half)                  VVM_output16;                           VVM_OP_NEXT;
		VVM_OP_CASE(acquireindex)
		{
			VVM_INS1_PTRS
			uint8 DataSetIdx = InsPtr[4];
			uint32 InputInc  = (uint32)Inc0 >> 4;
			InsPtr += 6;

			uint32 *inp = (uint32 *)p0;
			int *outp = (int *)p1;
			for (int i = 0; i < NumInstancesThisChunk; ++i) {
				uint32 OutputInc = *inp >> 31;
				inp += InputInc;
				*outp = i;
				outp += OutputInc;
			}

			uint32 NumOutputInstances = (uint32)(outp - (int *)p1);

			//the new VM's indicies are generated to support brachless write-gather for the output instructions (instead of an in-signal flag as the original bytecode intended)
			//the above loop will write an invalid value into the last slot if we discard one or more instances.  This is normally okay, however if an update_id instruction is issued later, 
			//we will write incorrect values into the free id table there.  To avoid this (and potentially other problems that may come up if the bytecode is expanded, we correct the final slot here.
			if ((int)NumOutputInstances < NumInstancesThisChunk)
			{
				uint16 *InsPtr16 = (uint16 *)(InsPtr - 6);
				((uint32 *)BatchState->RegPtrTable[InsPtr16[1]])[NumOutputInstances] = NumInstancesThisChunk;
			}
			BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[DataSetIdx] = ExecCtx->DataSets[DataSetIdx].InstanceOffset + FPlatformAtomics::InterlockedAdd(ExecCtx->VVMState->NumOutputPerDataSet + DataSetIdx, NumOutputInstances);
			BatchState->ChunkLocalData.NumOutputPerDataSet[DataSetIdx] += NumOutputInstances;
		}
		VVM_OP_NEXT;
		VVM_OP_CASE(external_func_call)
		{
				
			int FnIdx = (int)*(uint16 *)(InsPtr);
			FVectorVMExtFunctionData *ExtFnData = ExecCtx->VVMState->ExtFunctionTable + FnIdx;
#if		0//				defined(VVM_INCLUDE_SERIALIZATION) && !defined(VVM_SERIALIZE_NO_WRITE)
			//if (SerializeState && (SerializeState->Flags & VVMSer_SyncExtFns) && CmpSerializeState && SerializeState->NumInstances == CmpSerializeState->NumInstances && (CmpSerializeState->NumInstructions > SerializeState->NumInstructions || VVMSerGlobalChunkIdx != 0))
			if (0)
			{
				//If we hit this branch we are using the output from the comparision state instead of running the external function itself.
				//AFAIK the VM is not speced to have the inputs and outputs in a particular order, and even if it is we shouldn't rely on
				//3rd party external function writers to follow the spec.  Therefore we don't just sync what we think is output, we sync
				//all temp registers that are used in the function.
				int NumRegisters = ExtFnData->NumInputs + ExtFnData->NumOutputs;
				FVectorVMSerializeInstruction *CmpIns = nullptr; //this instruction
				FVectorVMOptimizeInstruction *OptIns = nullptr;
				if (SerializeState->OptimizeCtx)
				{
					//instructions have been re-ordered, can't binary search, must linear search
					for (uint32 i = 0 ; i < SerializeState->OptimizeCtx->Intermediate.NumInstructions; ++i)
					{
						if (SerializeState->OptimizeCtx->Intermediate.Instructions[i].OpCode == EVectorVMOp::external_func_call && SerializeState->OptimizeCtx->Intermediate.Instructions[i].PtrOffsetInOptimizedBytecode == (int)(VVMSerStartOpPtr - ExecCtx->VVMState->Bytecode))
						{
							OptIns = SerializeState->OptimizeCtx->Intermediate.Instructions + i;
							break;
						}
					}
					if (OptIns)
					{
						for (uint32 i = 0; i < CmpSerializeState->NumInstructions; ++i)
						{
							if (CmpSerializeState->Bytecode[CmpSerializeState->Instructions[i].OpStart] == (uint8)EVectorVMOp::external_func_call && OptIns->PtrOffsetInOrigBytecode == CmpSerializeState->Instructions[i].OpStart)
							{
								CmpIns = CmpSerializeState->Instructions + i;
								break;
							}
						}
					}
				}
				else
				{
					check(false);
					//CmpIns = CmpSerializeState->Instructions + VVMSerNumInstructionsThisChunk;
				}

				if (OptIns && CmpIns)
				{
					for (int i = 0; i < NumRegisters; ++i)
					{
						uint8 RegIdx = InsPtr[1 + i];
						if (RegIdx != 0xFF) {
							uint32 * DstReg  = (uint32 *)BatchState->RegPtrTable[RegIdx];
							int DstInc       = (int)BatchState->RegIncTable[RegIdx];
							if (DstInc != 0) { //skip constants
								int SrcRegIdx = 0xFFFF;
								if (OptIns && (SerializeState->Flags & VVMSer_OptimizedBytecode) && !(CmpSerializeState->Flags & VVMSer_OptimizedBytecode))
								{
									SrcRegIdx = ((uint16 *)(CmpSerializeState->Bytecode + OptIns->PtrOffsetInOrigBytecode + 2))[i] & 0x7FFF; //high bit is register in original bytecode
								}
								if (SrcRegIdx != 0x7FFF) //invalid register, skipped by external function in the execution
								{
									uint8 *SrcReg = (uint8 *)(CmpIns->TempRegisters + SrcRegIdx * CmpSerializeState->NumInstances + StartInstanceThisChunk);
									VVMMemCpy(DstReg, SrcReg, sizeof(uint32) * NumInstancesThisChunk);
								}
							}
						}
					}
				}
				InsPtr += 3 + 2 * (ExtFnData->NumInputs + ExtFnData->NumOutputs);
			} else {
#						else //VVM_INCLUDE_SERIALIZATION
			{
#						endif //VVM_INCLUDE_SERIALIZATION
				check(*InsPtr < ExecCtx->VVMState->NumExtFunctions);
				check((uint32)(ExtFnData->NumInputs + ExtFnData->NumOutputs) <= ExecCtx->VVMState->MaxExtFnRegisters);
				const uint16 *RegIndices      = ((uint16 *)InsPtr) + 1;
				InsPtr += 3 + 2 * (ExtFnData->NumInputs + ExtFnData->NumOutputs);
				uint32 DummyRegCount = 0;
				for (int i = 0; i < ExtFnData->NumInputs + ExtFnData->NumOutputs; ++i) {
					if (RegIndices[i] != 0xFFFF)
					{
						BatchState->ChunkLocalData.ExtFnDecodedReg.RegData[i] = (uint32 *)BatchState->RegPtrTable[RegIndices[i]];
						BatchState->ChunkLocalData.ExtFnDecodedReg.RegInc[i]  = BatchState->RegIncTable[RegIndices[i]] >> 4; //external functions increment by 1 32 bit value at a time
					}
					else
					{
						BatchState->ChunkLocalData.ExtFnDecodedReg.RegData[i] = (uint32 *)(BatchState->ChunkLocalData.ExtFnDecodedReg.DummyRegs + DummyRegCount++);
						BatchState->ChunkLocalData.ExtFnDecodedReg.RegInc[i]  = 0;
					}
				}
				check(DummyRegCount <= ExecCtx->VVMState->NumDummyRegsRequired);

				FVectorVMExternalFunctionContextExperimental ExtFnCtx;

				ExtFnCtx.RegisterData             = BatchState->ChunkLocalData.ExtFnDecodedReg.RegData;
				ExtFnCtx.RegInc                   = BatchState->ChunkLocalData.ExtFnDecodedReg.RegInc;

				ExtFnCtx.RegReadCount             = 0;
				ExtFnCtx.NumRegisters             = ExtFnData->NumInputs + ExtFnData->NumOutputs;

				ExtFnCtx.StartInstance            = StartInstanceThisChunk;
				ExtFnCtx.NumInstances             = NumInstancesThisChunk;
				ExtFnCtx.NumLoops                 = NumLoops;
				ExtFnCtx.PerInstanceFnInstanceIdx = 0;

				ExtFnCtx.UserPtrTable             = ExecCtx->UserPtrTable.GetData();
				ExtFnCtx.NumUserPtrs              = ExecCtx->UserPtrTable.Num();
				ExtFnCtx.RandStream               = &BatchState->RandStream;

				ExtFnCtx.RandCounters             = &BatchState->ChunkLocalData.RandCounters;
				ExtFnCtx.DataSets                 = ExecCtx->DataSets;

#if VECTORVM_SUPPORTS_LEGACY
				FVectorVMExternalFunctionContext ProxyContext(ExtFnCtx);
				ExtFnData->Function->Execute(ProxyContext);
#else
				ExtFnData->Function->Execute(ExtFnCtx);
#endif
			}
		}
		VVM_OP_NEXT;
		VVM_OP_CASE(exec_index)                         VVM_execVec_exec_index;         VVM_OP_NEXT;
		VVM_OP_CASE(noise2D)							check(false);                   VVM_OP_NEXT;
		VVM_OP_CASE(noise3D)							check(false);                   VVM_OP_NEXT;
		VVM_OP_CASE(enter_stat_scope)					InsPtr += 2;                    VVM_OP_NEXT;
		VVM_OP_CASE(exit_stat_scope)                                                    VVM_OP_NEXT;
		VVM_OP_CASE(update_id)
		{
			VVM_INS1_PTRS
			int32 *R1             = (int32 *)p0;
			int32 *R2             = (int32 *)p1;
			uint8 DataSetIdx      = InsPtr[4];
			FDataSetMeta *DataSet = &ExecCtx->DataSets[DataSetIdx];
			InsPtr += 6;
			check(DataSetIdx < (uint32)ExecCtx->DataSets.Num());
			check(DataSet->IDTable);
			check(DataSet->IDTable->Num() >= DataSet->InstanceOffset + StartInstanceThisChunk + NumInstancesThisChunk);
			int NumOutputInstances = BatchState->ChunkLocalData.NumOutputPerDataSet[DataSetIdx];
			int NumFreed           = NumInstancesThisChunk - BatchState->ChunkLocalData.NumOutputPerDataSet[DataSetIdx];
				
			//compute this chunk's MaxID
			int MaxID = -1;
			if (NumOutputInstances > 4)
			{
				int NumOutput4 = (int)(((((uint32)NumOutputInstances + 3U) & ~3U) - 1) >> 2);
				VectorRegister4i Max4 = VectorIntSet1(-1);
				for (int i = 0; i < NumOutput4; ++i)
				{
					VectorRegister4i R4 = VectorIntLoad(R1 + (uint64)((uint64)i << 2ULL));
					Max4 = VectorIntMax(Max4, R4);
				}
				VectorRegister4i Last4 = VectorIntLoad(R1 + NumOutputInstances - 4);
				Max4 = VectorIntMax(Last4, Max4);
				int M4[4];
				VectorIntStore(Max4, M4);
				int m0 = M4[0] > M4[1] ? M4[0] : M4[1];
				int m1 = M4[2] > M4[3] ? M4[2] : M4[3];
				int m = m0 > m1 ? m0 : m1;
				if (m > MaxID)
				{
					MaxID = m;
				}
			}
			else
			{
				for (int i = 0; i < NumOutputInstances; ++i)
				{
					if (R1[i] > MaxID)
					{
						MaxID = R1[i];
					}
				}
			}
					
			// Update the actual index for this ID.  No thread safety is required as this ID slot can only ever be written by this instance
			// The index passed into this function is the same as that given to the output* instructions
			for (int i = 0; i < NumOutputInstances; ++i)
			{
				(*DataSet->IDTable)[R1[R2[i]]] = BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[DataSetIdx] + i; //BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[DataSetIdx] already has DataSet->InstanceOffset added to it
			}

			//Write the freed indices to the free table.
			if (NumFreed > 0)
			{
				int StartNumFreed = FPlatformAtomics::InterlockedAdd((volatile int32 *)DataSet->NumFreeIDs, NumFreed);
				int32 *FreeTableStart = DataSet->FreeIDTable->GetData() + StartNumFreed;
				int c = 0;
				int FreeCount = 0;
				while (FreeCount < NumFreed)
				{	
					check(c < NumInstancesThisChunk);
					int d = R2[c] - c - FreeCount; //check for a gap in the write index and the counter... if nothing is freed then the write index matches the counter
					if (d > 0)
					{
						VVMMemCpy(FreeTableStart + FreeCount, R1 + FreeCount + c, sizeof(int32) * d);
						FreeCount += d;
					}
					++c;
				}
				check(FreeCount == NumFreed);
			}

			//Set the DataSet's MaxID if this chunk's MaxID is bigger
			if (MaxID != -1)
			{
				int SanityCount = 0;
				do {
					int OldMaxID = FPlatformAtomics::AtomicRead(DataSet->MaxUsedID);
					if (MaxID <= OldMaxID)
					{
						break;
					}
					int NewMaxID = FPlatformAtomics::InterlockedCompareExchange((volatile int32 *)DataSet->MaxUsedID, MaxID, OldMaxID);
					if (NewMaxID == OldMaxID)
					{
						break;
					}
				} while (SanityCount++ < (1 << 30));
				VVMDebugBreakIf(SanityCount > (1 << 30) - 1);
			}
		}
		VVM_OP_NEXT;
		VVM_OP_CASE(acquire_id)
		{
			VVMSer_instruction(1, 2);
			VVM_INS1_PTRS
			uint8 DataSetIdx = InsPtr[4];
			InsPtr += 6;
			check(DataSetIdx < (uint32)ExecCtx->DataSets.Num());
			FDataSetMeta *DataSet = &ExecCtx->DataSets[DataSetIdx];
				
			{ //1. Get the free IDs into the temp register
				int SanityCount = 0;
				do
				{
					int OldNumFreeIDs = FPlatformAtomics::AtomicRead(DataSet->NumFreeIDs);
					check(OldNumFreeIDs >= NumInstancesThisChunk);
					int *OutPtr = (int *)p0;
					int *InPtr  = DataSet->FreeIDTable->GetData() + OldNumFreeIDs - NumInstancesThisChunk;
					for (int i = 0; i < NumInstancesThisChunk; ++i)
					{
						OutPtr[i] = InPtr[NumInstancesThisChunk - i - 1];
					}
					int NewNumFreeIDs = FPlatformAtomics::InterlockedCompareExchange((volatile int32 *)DataSet->NumFreeIDs, OldNumFreeIDs - NumInstancesThisChunk, OldNumFreeIDs);
					if (NewNumFreeIDs == OldNumFreeIDs)
					{
						break;
					}
				} while (SanityCount++ < (1 << 30));
				VVMDebugBreakIf(SanityCount >= (1 << 30) - 1);
			}
			{ //2. append the IDs we acquired in step 1 to the end of the free table array, representing spawned IDs
				//FreeID table is write-only as far as this invocation of the VM is concerned, so the interlocked add w/o filling
				//in the data is fine
				int StartNumSpawned = FPlatformAtomics::InterlockedAdd(DataSet->NumSpawnedIDs, NumInstancesThisChunk) + NumInstancesThisChunk;
				check(StartNumSpawned <= DataSet->FreeIDTable->Max());
				VVMMemCpy(DataSet->FreeIDTable->GetData() + DataSet->FreeIDTable->Max() - StartNumSpawned, p0, sizeof(int32) * NumInstancesThisChunk);
			}
			//3. set the tag
			VVMMemSet32(p1, DataSet->IDAcquireTag, NumInstancesThisChunk);
		}
		VVM_OP_NEXT;
		VVM_OP_CASE(half_to_float)
		{
			VVM_INS1_PTRS
			VVMSer_instruction(1, 1);
			uint32 InputInc = Inc0 >> 1;
			uint8 *end      = p1 + sizeof(FVecReg) * NumLoops;
			InsPtr += 5;
			do {
				FPlatformMath::VectorLoadHalf((float *)p1, (uint16 *)p0);
				p0 += InputInc;
				p1 += sizeof(FVecReg);
			} while (p1 < end);
		} VVM_OP_NEXT;
		VVM_OP_CASE(exec_indexf)                         VVM_execVec_exec_indexf;                 VVM_OP_NEXT;
		VVM_OP_CASE(exec_index_addi)                     VVM_execVec_exec_index_addi;             VVM_OP_NEXT;

		VVM_OP_CASE(cmplt_select)                        VVM_execVecIns4f(VVM_cmplt_select);      VVM_OP_NEXT;
		VVM_OP_CASE(cmple_select)                        VVM_execVecIns4f(VVM_cmple_select);      VVM_OP_NEXT;
		VVM_OP_CASE(cmpeq_select)                        VVM_execVecIns4f(VVM_cmpeq_select);      VVM_OP_NEXT;
		VVM_OP_CASE(cmplti_select)                       VVM_execVecIns4f(VVM_cmplti_select);     VVM_OP_NEXT;
		VVM_OP_CASE(cmplei_select)                       VVM_execVecIns4f(VVM_cmplei_select);     VVM_OP_NEXT;
		VVM_OP_CASE(cmpeqi_select)                       VVM_execVecIns4f(VVM_cmpeqi_select);     VVM_OP_NEXT;
		
		VVM_OP_CASE(cmplt_logic_and)                     VVM_execVecIns3f(VVM_cmplt_logic_and);   VVM_OP_NEXT;
		VVM_OP_CASE(cmple_logic_and)                     VVM_execVecIns3f(VVM_cmple_logic_and);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpgt_logic_and)                     VVM_execVecIns3f(VVM_cmpgt_logic_and);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpge_logic_and)                     VVM_execVecIns3f(VVM_cmpge_logic_and);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpeq_logic_and)                     VVM_execVecIns3f(VVM_cmpeq_logic_and);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpne_logic_and)                     VVM_execVecIns3f(VVM_cmpne_logic_and);   VVM_OP_NEXT;

		VVM_OP_CASE(cmplti_logic_and)                    VVM_execVecIns3i(VVM_cmplti_logic_and);  VVM_OP_NEXT;
		VVM_OP_CASE(cmplei_logic_and)                    VVM_execVecIns3i(VVM_cmplei_logic_and);  VVM_OP_NEXT;
		VVM_OP_CASE(cmpgti_logic_and)                    VVM_execVecIns3i(VVM_cmpgti_logic_and);  VVM_OP_NEXT;
		VVM_OP_CASE(cmpgei_logic_and)                    VVM_execVecIns3i(VVM_cmpgei_logic_and);  VVM_OP_NEXT;
		VVM_OP_CASE(cmpeqi_logic_and)                    VVM_execVecIns3i(VVM_cmpeqi_logic_and);  VVM_OP_NEXT;
		VVM_OP_CASE(cmpnei_logic_and)                    VVM_execVecIns3i(VVM_cmpnei_logic_and);  VVM_OP_NEXT;

		VVM_OP_CASE(cmplt_logic_or)                      VVM_execVecIns3f(VVM_cmplt_logic_or);    VVM_OP_NEXT;
		VVM_OP_CASE(cmple_logic_or)                      VVM_execVecIns3f(VVM_cmple_logic_or);    VVM_OP_NEXT;
		VVM_OP_CASE(cmpgt_logic_or)                      VVM_execVecIns3f(VVM_cmpgt_logic_or);    VVM_OP_NEXT;
		VVM_OP_CASE(cmpge_logic_or)                      VVM_execVecIns3f(VVM_cmpge_logic_or);    VVM_OP_NEXT;
		VVM_OP_CASE(cmpeq_logic_or)                      VVM_execVecIns3f(VVM_cmpeq_logic_or);    VVM_OP_NEXT;
		VVM_OP_CASE(cmpne_logic_or)                      VVM_execVecIns3f(VVM_cmpne_logic_or);    VVM_OP_NEXT;

		VVM_OP_CASE(cmplti_logic_or)                     VVM_execVecIns3i(VVM_cmplti_logic_or);   VVM_OP_NEXT;
		VVM_OP_CASE(cmplei_logic_or)                     VVM_execVecIns3i(VVM_cmplei_logic_or);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpgti_logic_or)                     VVM_execVecIns3i(VVM_cmpgti_logic_or);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpgei_logic_or)                     VVM_execVecIns3i(VVM_cmpgei_logic_or);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpeqi_logic_or)                     VVM_execVecIns3i(VVM_cmpeqi_logic_or);   VVM_OP_NEXT;
		VVM_OP_CASE(cmpnei_logic_or)                     VVM_execVecIns3i(VVM_cmpnei_logic_or);   VVM_OP_NEXT;
		
		VVM_OP_CASE(mad_add)                             VVM_execVecIns4f(VVM_mad_add);           VVM_OP_NEXT;
		VVM_OP_CASE(mad_sub0)                            VVM_execVecIns4f(VVM_mad_sub0);          VVM_OP_NEXT;
		VVM_OP_CASE(mad_sub1)                            VVM_execVecIns4f(VVM_mad_sub1);          VVM_OP_NEXT;
		VVM_OP_CASE(mad_mul)                             VVM_execVecIns4f(VVM_mad_mul);           VVM_OP_NEXT;
		VVM_OP_CASE(mad_sqrt)                            VVM_execVecIns3f(VVM_mad_sqrt);          VVM_OP_NEXT;
		VVM_OP_CASE(mad_mad0)                            VVM_execVecIns5f(VVM_mad_mad0);          VVM_OP_NEXT;
		VVM_OP_CASE(mad_mad1)                            VVM_execVecIns5f(VVM_mad_mad1);          VVM_OP_NEXT;

		VVM_OP_CASE(mul_mad0)                            VVM_execVecIns4f(VVM_mul_mad0);          VVM_OP_NEXT;
		VVM_OP_CASE(mul_mad1)                            VVM_execVecIns4f(VVM_mul_mad1);          VVM_OP_NEXT;
		VVM_OP_CASE(mul_add)                             VVM_execVecIns3f(VVM_mul_add);           VVM_OP_NEXT;
		VVM_OP_CASE(mul_sub0)                            VVM_execVecIns3f(VVM_mul_sub0);          VVM_OP_NEXT;
		VVM_OP_CASE(mul_sub1)                            VVM_execVecIns3f(VVM_mul_sub1);          VVM_OP_NEXT;
		VVM_OP_CASE(mul_mul)                             VVM_execVecIns3f(VVM_mul_mul);           VVM_OP_NEXT;
		VVM_OP_CASE(mul_max)                             VVM_execVecIns3f(VVM_mul_max);           VVM_OP_NEXT;
		VVM_OP_CASE(mul_2x)                              VVM_execVecIns2f_2x(VectorMultiply);     VVM_OP_NEXT;

		VVM_OP_CASE(add_mad1)                            VVM_execVecIns4f(VVM_add_mad1);          VVM_OP_NEXT;
		VVM_OP_CASE(add_add)                             VVM_execVecIns3f(VVM_add_add);           VVM_OP_NEXT;

		VVM_OP_CASE(sub_cmplt1)                          VVM_execVecIns3f(VVM_sub_cmplt1);        VVM_OP_NEXT;
		VVM_OP_CASE(sub_neg)                             VVM_execVecIns2f(VVM_sub_neg);           VVM_OP_NEXT;
		VVM_OP_CASE(sub_mul)                             VVM_execVecIns3f(VVM_sub_mul);           VVM_OP_NEXT;
		
		VVM_OP_CASE(div_mad0)                            VVM_execVecIns4f(VVM_div_mad0);          VVM_OP_NEXT;
		VVM_OP_CASE(div_f2i)                             VVM_execVecIns2i(VVM_div_f2i);           VVM_OP_NEXT;
		VVM_OP_CASE(div_mul)                             VVM_execVecIns3f(VVM_div_mul);           VVM_OP_NEXT;
		VVM_OP_CASE(muli_addi)                           VVM_execVecIns3i(VVM_muli_addi);         VVM_OP_NEXT;
		VVM_OP_CASE(addi_bit_rshift)                     VVM_execVecIns3i(VVM_addi_bit_rshift);   VVM_OP_NEXT;
		VVM_OP_CASE(addi_muli)                           VVM_execVecIns3i(VVM_addi_muli);         VVM_OP_NEXT;
		VVM_OP_CASE(b2i_2x)                              VVM_execVecIns1i_2x(VVM_vecBoolToInt);   VVM_OP_NEXT;
		VVM_OP_CASE(i2f_div0)                            VVM_execVecIns2f(VVM_i2f_div0);          VVM_OP_NEXT;
		VVM_OP_CASE(i2f_div1)                            VVM_execVecIns2f(VVM_i2f_div1);          VVM_OP_NEXT;
		VVM_OP_CASE(i2f_mul)                             VVM_execVecIns2f(VVM_i2f_mul);           VVM_OP_NEXT;
		VVM_OP_CASE(i2f_mad0)                            VVM_execVecIns3f(VVM_i2f_mad0);          VVM_OP_NEXT;
		VVM_OP_CASE(i2f_mad1)                            VVM_execVecIns3f(VVM_i2f_mad1);          VVM_OP_NEXT;
		
		VVM_OP_CASE(f2i_select1)                         VVM_execVecIns3i(VVM_f2i_select1);       VVM_OP_NEXT;
		VVM_OP_CASE(f2i_maxi)                            VVM_execVecIns2i(VVM_f2i_maxi);          VVM_OP_NEXT;
		VVM_OP_CASE(f2i_addi)                            VVM_execVecIns2i(VVM_f2i_addi);          VVM_OP_NEXT;
		VVM_OP_CASE(fmod_add)                            VVM_execVecIns3f(VVM_fmod_add);          VVM_OP_NEXT;
		VVM_OP_CASE(bit_and_i2f)                         VVM_execVecIns2f(VVM_bit_and_i2f);       VVM_OP_NEXT;
		VVM_OP_CASE(bit_rshift_bit_and)                  VVM_execVecIns3i(VVM_bit_rshift_bit_and);VVM_OP_NEXT;
		VVM_OP_CASE(neg_cmplt)                           VVM_execVecIns2f(VVM_neg_cmplt);         VVM_OP_NEXT;
		VVM_OP_CASE(bit_or_muli)                         VVM_execVecIns3i(VVM_bit_or_muli);       VVM_OP_NEXT;
		VVM_OP_CASE(bit_lshift_bit_or)                   VVM_execVecIns3i(VVM_bit_lshift_bit_or); VVM_OP_NEXT;
		VVM_OP_CASE(random_add)                          VVM_execVecIns2f(VVM_random_add);        VVM_OP_NEXT;
		VVM_OP_CASE(random_2x)                           VVM_execVec_random_2x;                   VVM_OP_NEXT;
		VVM_OP_CASE(max_f2i)                             VVM_execVecIns2i(VVM_max_f2i);           VVM_OP_NEXT;
		VVM_OP_CASE(select_mul)                          VVM_execVecIns4f(VVM_select_mul);        VVM_OP_NEXT;
		VVM_OP_CASE(select_add)                          VVM_execVecIns4f(VVM_select_add);        VVM_OP_NEXT;
		VVM_OP_CASE(sin_cos)                             VVM_execVec_sin_cos;                     VVM_OP_NEXT;
		VVM_OP_CASE(inputdata_float)
		VVM_OP_CASE(inputdata_int32)
		VVM_OP_CASE(inputdata_half)
		VVM_OP_CASE(inputdata_noadvance_float)
		VVM_OP_CASE(inputdata_noadvance_int32)
		VVM_OP_CASE(inputdata_noadvance_half) return;
	}
	//VVMSer_insEndExp(SerializeState, (int)(VVMSerStartOpPtr - VVMState->Bytecode), (int)(InsPtr - VVMSerStartOpPtr));
}
//VVMSer_chunkEndExp(SerializeState);

#undef VVM_OP_CASE
#undef VVM_OP_NEXT