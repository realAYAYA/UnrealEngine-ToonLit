// Copyright Epic Games, Inc. All Rights Reserved.

//#if VECTORVM_SUPPORTS_COMPUTED_GOTO
#if 1
#define VVM_INS0_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]];
#define VVM_INS1_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]]; \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];
#define VVM_INS2_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]]; \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]]; \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]];
#define VVM_INS3_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]]; \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]]; \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]]; \
						uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];
#define VVM_INS4_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]]; \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]]; \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]]; \
						uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];	\
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]];
#define VVM_INS5_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]]; \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]]; \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]]; \
						uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];	\
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]]; \
						uint8 *p5 = BatchState->RegPtrTable[((uint16 *)InsPtr)[5]];

#define VVM_execCoreSetupIncVars
#define VVM_execCoreSetupPtrVars
#else
#define VVM_INS1_PTRS
#define VVM_INS2_PTRS
#define VVM_INS3_PTRS	uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];
#define VVM_INS4_PTRS	uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]]; \
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]];
#define VVM_INS5_PTRS	uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]]; \
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]]; \
						uint8 *p5 = BatchState->RegPtrTable[((uint16 *)InsPtr)[5]];

#define VVM_execCoreSetupIncVars
#define VVM_execCoreSetupPtrVars        uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]]; \
										uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]]; \
										uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]];
#endif


#	define VVM_execVecIns1f(ins_)					{                                                              \
														VVMSer_instruction(0, 1)                                   \
														VVM_INS1_PTRS                                              \
														InsPtr += 5;                                               \
														VectorRegister4f r0 = VectorLoad((float *)p0);             \
														VectorRegister4f res = ins_(r0);                           \
														VectorStoreAligned(res, (float *)p1);                      \
													}

#	define VVM_execVecIns2f(ins_)					{                                                              \
														VVMSer_instruction(0, 2)                                   \
														VVM_INS2_PTRS                                              \
														InsPtr += 7;                                               \
														VectorRegister4f r0 = VectorLoad((float *)p0);             \
														VectorRegister4f r1 = VectorLoad((float *)p1);             \
														VectorRegister4f res = ins_(r0, r1);                       \
														VectorStoreAligned(res, (float *)p2);                      \
													}
#	define VVM_execVecIns2f_2x(ins_)				{                                                              \
														VVMSer_instruction(0, 3)                                   \
														VVM_INS3_PTRS                                              \
														InsPtr += 9;                                               \
														VectorRegister4f r0 = VectorLoad((float *)p0);             \
														VectorRegister4f r1 = VectorLoad((float *)p1);             \
														VectorRegister4f res = ins_(r0, r1);                       \
														VectorStoreAligned(res, (float *)p2);                      \
														VectorStoreAligned(res, (float *)p3);                      \
													}
#	define VVM_execVecIns3f(ins_)					{                                                              \
														VVMSer_instruction(0, 3)                                   \
														VVM_INS3_PTRS                                              \
														InsPtr += 9;                                               \
														VectorRegister4f r0 = VectorLoad((float *)p0);             \
														VectorRegister4f r1 = VectorLoad((float *)p1);             \
														VectorRegister4f r2 = VectorLoad((float *)p2);             \
														VectorRegister4f res = ins_(r0, r1, r2);                   \
														VectorStoreAligned(res, (float *)p3);                      \
													}

#	define VVM_execVecIns4f(ins_)					{                                                              \
														VVMSer_instruction(0, 4)                                   \
														VVM_INS4_PTRS                                              \
														InsPtr += 11;                                              \
														VectorRegister4f r0 = VectorLoad((float *)p0);             \
														VectorRegister4f r1 = VectorLoad((float *)p1);             \
														VectorRegister4f r2 = VectorLoad((float *)p2);             \
														VectorRegister4f r3 = VectorLoad((float *)p3);             \
														VectorRegister4f res = ins_(r0, r1, r2, r3);               \
														VectorStoreAligned(res, (float *)p4);                      \
													}
#	define VVM_execVecIns5f(ins_)					{                                                              \
														VVMSer_instruction(0, 5)                                   \
														VVM_INS5_PTRS                                              \
														InsPtr += 13;                                              \
														VectorRegister4f r0 = VectorLoad((float *)p0);             \
														VectorRegister4f r1 = VectorLoad((float *)p1);             \
														VectorRegister4f r2 = VectorLoad((float *)p2);             \
														VectorRegister4f r3 = VectorLoad((float *)p3);             \
														VectorRegister4f r4 = VectorLoad((float *)p4);             \
														VectorRegister4f res = ins_(r0, r1, r2, r3, r4);           \
														VectorStoreAligned(res, (float *)p5);                      \
													}
#	define VVM_execVecIns1i(ins_)					{                                                              \
														VVMSer_instruction(1, 1)                                   \
														VVM_INS1_PTRS                                              \
														InsPtr += 5;                                               \
														VectorRegister4i r0 = VectorIntLoad(p0);                   \
														VectorRegister4i res = ins_(r0);                           \
														VectorIntStoreAligned(res, p1);                            \
													}
#	define VVM_execVecIns1i_2x(ins_)				{                                                              \
														VVMSer_instruction(1, 2)                                   \
														VVM_INS2_PTRS                                              \
														InsPtr += 7;                                               \
														VectorRegister4i r0 = VectorIntLoad(p0);                   \
														VectorRegister4i res = ins_(r0);                           \
														VectorIntStoreAligned(res, p1);                            \
														VectorIntStoreAligned(res, p2);                            \
													}
#	define VVM_execVecIns2i(ins_)					{                                                              \
														VVMSer_instruction(1, 2)                                   \
														VVM_INS2_PTRS                                              \
														InsPtr += 7;                                               \
														VectorRegister4i r0 = VectorIntLoad(p0);                   \
														VectorRegister4i r1 = VectorIntLoad(p1);                   \
														VectorRegister4i res = ins_(r0, r1);                       \
														VectorIntStoreAligned(res, p2);                            \
													}

#	define VVM_execVecIns3i(ins_)					{                                                              \
														VVMSer_instruction(1, 3)                                   \
														VVM_INS3_PTRS                                              \
														InsPtr += 9;											   \
														VectorRegister4i r0 = VectorIntLoad(p0);                   \
														VectorRegister4i r1 = VectorIntLoad(p1);                   \
														VectorRegister4i r2 = VectorIntLoad(p2);                   \
														VectorRegister4i res = ins_(r0, r1, r2);                   \
														VectorIntStoreAligned(res, p3);                            \
													}

#define VVM_execVec_exec_index                      {                                                              \
                                                    	VVM_INS0_PTRS                                              \
                                                    	VVMSer_instruction(1, 0);                                  \
                                                    	InsPtr += 3;                                               \
														uint32 *out = (uint32 *)p0;                                \
														out[0] = 0;                                                \
														out[1] = 1;                                                \
														out[2] = 2;                                                \
														out[3] = 3;                                                \
                                                    }
#define VVM_execVec_exec_indexf                     {                                                              \
                                                    	VVM_INS0_PTRS                                              \
                                                    	VVMSer_instruction(1, 0);                                  \
                                                    	InsPtr += 3;                                               \
														float *out = (float *)p0;                                  \
														out[0] = 0.f;                                              \
														out[1] = 1.f;                                              \
														out[2] = 2.f;                                              \
														out[3] = 3.f;                                              \
                                                    }
#define VVM_execVec_exec_index_addi					{                                                              \
                                                    	VVM_INS1_PTRS                                              \
                                                    	VVMSer_instruction(2, 0);                                  \
                                                    	InsPtr += 5;                                               \
                                                        int *in  = (int *)p0;                                      \
														int *out = (int *)p1;                                      \
														out[0] = in[0] + 0;                                        \
														out[1] = in[1] + 1;                                        \
														out[2] = in[2] + 2;                                        \
														out[3] = in[3] + 3;                                        \
                                                    }
#define VVM_execVec_random_2x						{                                                              \
														VVMSer_instruction(2, 2)                                   \
														VVM_INS2_PTRS                                              \
														uint16 *InsPtr16 = (uint16 *)InsPtr;                       \
														InsPtr += 7;											   \
														VectorRegister4f r0 = VectorLoad((float *)p0);             \
														VectorRegister4f res0 = VVM_random(r0);                    \
														VectorRegister4f res1 = VVM_random(r0);                    \
														VectorStoreAligned(res0, (float *)p1);                     \
														VectorStoreAligned(res1, (float *)p2);                     \
													}
#define VVM_execVec_sin_cos                         {                                                                      \
														VVMSer_instruction(0, 2)                                           \
														VVM_INS3_PTRS                                                      \
														InsPtr += 7;                                                       \
														VectorRegister4f r0 = VectorLoad((float *)p0);                     \
														VectorSinCos((VectorRegister4f *)p1, (VectorRegister4f *)p2, &r0); \
													}
#define VVM_output32								{                                                                                                                                        \
														uint8  RegType            = InsPtr[-1] - (uint8)EVectorVMOp::outputdata_float;														 \
														int NumOutputLoops        = InsPtr[0];																								 \
														uint8 DataSetIdx          = InsPtr[1];																								 \
														uint32 NumOutputInstances = BatchState->ChunkLocalData.NumOutputPerDataSet[DataSetIdx];												 \
														uint32 InstanceOffset     = BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[DataSetIdx];										 \
														uint32 RegTypeOffset      = ExecCtx->DataSets[DataSetIdx].OutputRegisterTypeOffsets[RegType];										 \
														const uint16 * RESTRICT SrcIndices       = (uint16 *)(InsPtr + 4);																	 \
														const uint16 * RESTRICT DstIndices       = SrcIndices + NumOutputLoops;																 \
														const uint32 * RESTRICT DstIdxReg        = (uint32 *)BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];                                 \
														uint32 *RESTRICT *RESTRICT OutputBuffers = (uint32 **)ExecCtx->DataSets[DataSetIdx].OutputRegisters.GetData();						 \
														/*for (int i = 0; i < NumOutputLoops; ++i) { printf(",%d", (int)SrcIndices[i]); }*/                                                  \
														InsPtr += 5 + 4 * NumOutputLoops;																									 \
														if (NumOutputInstances == NumInstancesThisChunk) { /*all outputs are written*/														 \
															/*we know that NumInstancesThisChunk must be between 1 and 4 in the single loop case*/											 \
															for (int j = 0; j < NumOutputLoops; ++j) {																						 \
																int     SrcInc = BatchState->RegIncTable[SrcIndices[j]];																	 \
																uint32 *SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];															 \
																uint32 *DstReg = OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;												 \
																																															 \
																check((int)RegTypeOffset + (int)DstIndices[j] < (int)ExecCtx->DataSets[DataSetIdx].OutputRegisters.Num());					 \
																check(NumOutputInstances <= 4);																								 \
																																															 \
																if (SrcInc == 0) { /*setting from a constant*/																				 \
																	switch (NumOutputInstances) {																							 \
																		case 4: DstReg[3] = SrcReg[0]; /*intentional fallthrough*/															 \
																		case 3: DstReg[2] = SrcReg[0]; /*intentional fallthrough*/															 \
																		case 2: DstReg[1] = SrcReg[0]; /*intentional fallthrough*/															 \
																		case 1: DstReg[0] = SrcReg[0];																						 \
																	}																														 \
																} else {							switch (NumOutputInstances) {															 \
																		case 4: DstReg[3] = SrcReg[3]; /*intentional fallthrough*/															 \
																		case 3: DstReg[2] = SrcReg[2]; /*intentional fallthrough*/															 \
																		case 2: DstReg[1] = SrcReg[1]; /*intentional fallthrough*/															 \
																		case 1: DstReg[0] = SrcReg[0];																						 \
																	}																														 \
																}																															 \
															}																																 \
														} else if (NumOutputInstances > 0) {																								 \
															/*not all outputs are written*/																									 \
															for (int j = 0; j < NumOutputLoops; ++j) {																						 \
																int     SrcInc = BatchState->RegIncTable[SrcIndices[j]];																	 \
																uint32 * RESTRICT SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];												 \
																uint32 * RESTRICT DstReg = (uint32 *)OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;							 \
																if (SrcInc == 0) { /*setting from a constant*/																				 \
																	switch (NumOutputInstances) {																							 \
																		case 4: DstReg[3] = SrcReg[0]; /* intentional fallthrough */														 \
																		case 3: DstReg[2] = SrcReg[0]; /* intentional fallthrough */														 \
																		case 2: DstReg[1] = SrcReg[0]; /* intentional fallthrough */														 \
																		case 1: DstReg[0] = SrcReg[0];																						 \
																	}																														 \
																} else {																													 \
																	uint32 * RESTRICT IdxReg = (uint32 *)DstIdxReg;																			 \
																	for (uint32 i = 0; i < NumOutputInstances; ++i)																			 \
																	{																														 \
																		DstReg[i] = SrcReg[IdxReg[i]];																						 \
																	}																														 \
																}																															 \
															}																																 \
														}                                                                                                                                    \
													}
#define VVM_output16								{                                                                                                                                        \
														/*@TODO: half data is pretty inefficient.  The output loops could be written much more efficiently.*/								 \
														/*if there's lots of half data I should do this.*/																					 \
														int NumOutputLoops        = InsPtr[0];																								 \
														uint8 DataSetIdx         = InsPtr[1];																								 \
														uint32 NumOutputInstances = BatchState->ChunkLocalData.NumOutputPerDataSet[DataSetIdx];												 \
														uint32 InstanceOffset     = BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[DataSetIdx];										 \
														uint32 RegTypeOffset      = ExecCtx->DataSets[DataSetIdx].OutputRegisterTypeOffsets[2];												 \
														const uint16 * RESTRICT SrcIndices = (uint16 *)(InsPtr + 4);																		 \
														const uint16 * RESTRICT DstIndices = SrcIndices + NumOutputLoops;																	 \
														const uint32 * RESTRICT DstIdxReg  = (uint32 *)BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];                                       \
														uint32 **OutputBuffers             = (uint32 **)ExecCtx->DataSets[DataSetIdx].OutputRegisters.GetData();							 \
																																															 \
														InsPtr += 5 + 4 * NumOutputLoops;																									 \
														if (NumOutputInstances == NumInstancesThisChunk) { /* all outputs are written */													 \
															for (int j = 0; j < NumOutputLoops; ++j) {																						 \
																int     SrcInc = BatchState->RegIncTable[SrcIndices[j]] >> 4;																 \
																uint32 *SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];															 \
																uint16 *DstReg = (uint16 *)OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;									 \
																uint16 *DstEnd = DstReg + NumOutputInstances;																				 \
																																															 \
																if (SrcInc == 0) { /*setting from a constant*/																				 \
																	uint16 HalfVal = float_to_half_fast3_rtne(*SrcReg);																		 \
																	while (DstReg < DstEnd) {																								 \
																		*DstReg++ = HalfVal;																								 \
																	}																														 \
																} else {																													 \
																	if (SrcIndices[j] >= ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers) {						 \
																		/*coming from a half input buffer, so no conversion necessary*/														 \
																		uint16 *SrcReg16 = (uint16 *)SrcReg;																				 \
																		while (DstReg < DstEnd) {																							 \
																			*DstReg++ = *SrcReg++;																							 \
																		}																													 \
																	} else {																												 \
																		/*coming from a temp register, we must do the half->float conversion*/												 \
																		/*@TODO: scalar half->float... we can do 4-wide if this is too slow*/												 \
																		while (DstReg < DstEnd) {																							 \
																			*DstReg++ = float_to_half_fast3_rtne(*SrcReg);																	 \
																			SrcReg += SrcInc;																								 \
																		}																													 \
																	}																														 \
																}																															 \
															}																																 \
														} else {																															 \
															/*not all outputs are written*/																									 \
															for (int j = 0; j < NumOutputLoops; ++j) {																						 \
																int     SrcInc = BatchState->RegIncTable[SrcIndices[j]];																	 \
																uint32 *SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];															 \
																uint16 *DstReg = (uint16 *)OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;									 \
																uint16 *DstEnd = DstReg + NumOutputInstances;																				 \
																if (SrcInc == 0) { /*setting from a constant*/																				 \
																	uint16 HalfVal = float_to_half_fast3_rtne(*SrcReg);																		 \
																	while (DstReg < DstEnd) {																								 \
																		*DstReg++ = HalfVal;																								 \
																	}																														 \
																} else {																													 \
																	uint32 *IdxReg = (uint32 *)DstIdxReg;																					 \
																	if (SrcIndices[j] >= ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers) {						 \
																		uint16 *SrcReg16 = (uint16 *)SrcReg;																				 \
																		for (uint32 i = 0; i < NumOutputInstances; ++i)																		 \
																		{																													 \
																			DstReg[i] = SrcReg16[IdxReg[i]];																				 \
																		}																													 \
																	} else {																												 \
																		for (uint32 i = 0; i < NumOutputInstances; ++i)																		 \
																		{																													 \
																			/*@TODO: we're doing the float->half conversion in scalar... we could do this 4-wide get the next 4 as needed*/	 \
																			/*that's a bit more complicated and I don't know if it's worth it... if we find this is slow we can do that.*/	 \
																			DstReg[i] = float_to_half_fast3_rtne(SrcReg[IdxReg[i]]);														 \
																		}																													 \
																	}																														 \
																}																															 \
															}																																 \
														}																																	 \
													}


static void execChunkSingleLoop(FVectorVMExecContext *ExecCtx, FVectorVMBatchState *BatchState, int StartInstanceThisChunk, int NumInstancesThisChunk, FVectorVMSerializeState *SerializeState, FVectorVMSerializeState *CmpSerializeState)
{
	static const int NumLoops = 1;
	static const uint32 Inc0  = 16; //even reading from a const this is safe since the const is 4 wide and we're only working on 4 instances MAX (1 loop)

#include "VectorVMExecCore.inl"
}

#undef VVM_execCoreSetupPtrVars
#undef VVM_execCoreSetupIncVars
#undef VVM_INS0_PTRS
#undef VVM_INS1_PTRS
#undef VVM_INS2_PTRS
#undef VVM_INS3_PTRS
#undef VVM_INS4_PTRS
#undef VVM_INS5_PTRS

#undef VVM_execVecIns1f
#undef VVM_execVecIns2f
#undef VVM_execVecIns2f_2x
#undef VVM_execVecIns3f
#undef VVM_execVecIns4f
#undef VVM_execVecIns5f
#undef VVM_execVecIns1i
#undef VVM_execVecIns1i_2x
#undef VVM_execVecIns2i
#undef VVM_execVecIns3i
#undef VVM_execVec_random_2x
#undef VVM_execVec_exec_index
#undef VVM_execVec_exec_indexf
#undef VVM_execVec_exec_index_addi
#undef VVM_execVec_exec_index_2x
#undef VVM_execVec_sin_cos

#undef VVM_output32
#undef VVM_output16

