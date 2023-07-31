// Copyright Epic Games, Inc. All Rights Reserved.

//#if VECTORVM_SUPPORTS_COMPUTED_GOTO
#if 1
#define VVM_INS0_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]];

#define VVM_INS1_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]];           \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];           \
						uint32 Inc0 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[0]];

#define VVM_INS2_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]];           \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];           \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]];           \
						uint32 Inc0 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[0]]; \
						uint32 Inc1 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[1]];

#define VVM_INS3_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]];           \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];           \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]];           \
						uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];           \
						uint32 Inc0 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[0]]; \
						uint32 Inc1 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[1]]; \
						uint32 Inc2 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[2]];

#define VVM_INS4_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]];           \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];           \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]];           \
						uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];           \
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]];           \
						uint32 Inc0 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[0]]; \
						uint32 Inc1 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[1]]; \
						uint32 Inc2 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[2]]; \
						uint32 Inc3 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[3]];
#define VVM_INS5_PTRS	uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]];           \
						uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];           \
						uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]];           \
						uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];           \
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]];           \
						uint8 *p5 = BatchState->RegPtrTable[((uint16 *)InsPtr)[5]];           \
						uint32 Inc0 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[0]]; \
						uint32 Inc1 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[1]]; \
						uint32 Inc2 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[2]]; \
						uint32 Inc3 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[3]]; \
						uint32 Inc4 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[4]];
#define VVM_execCoreSetupIncVars
#define VVM_execCoreSetupPtrVars

#else
#define VVM_INS0_PTRS
#define VVM_INS1_PTRS
#define VVM_INS2_PTRS
#define VVM_INS3_PTRS	uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];           \
						uint32 Inc2 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[2]];
#define VVM_INS4_PTRS	uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];           \
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]];           \
						uint32 Inc2 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[2]]; \
						uint32 Inc3 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[3]];
#define VVM_INS5_PTRS	uint8 *p3 = BatchState->RegPtrTable[((uint16 *)InsPtr)[3]];           \
						uint8 *p4 = BatchState->RegPtrTable[((uint16 *)InsPtr)[4]];           \
						uint8 *p5 = BatchState->RegPtrTable[((uint16 *)InsPtr)[5]];           \
						uint32 Inc2 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[2]]; \
						uint32 Inc3 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[3]]; \
						uint32 Inc4 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[4]];

#define VVM_execCoreSetupIncVars        uint32 Inc0 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[0]]; \
										uint32 Inc1 = (uint32)BatchState->RegIncTable[((uint16 *)InsPtr)[1]];

#define VVM_execCoreSetupPtrVars        uint8 *p0 = BatchState->RegPtrTable[((uint16 *)InsPtr)[0]]; \
										uint8 *p1 = BatchState->RegPtrTable[((uint16 *)InsPtr)[1]]; \
										uint8 *p2 = BatchState->RegPtrTable[((uint16 *)InsPtr)[2]];
#endif

#	define VVM_execVecIns1f(ins_)					{                                                              \
														VVMSer_instruction(0, 1)                                   \
														VVM_INS1_PTRS                                              \
														InsPtr += 5;                                               \
                                                        uint8 *end = p1 + sizeof(FVecReg) * NumLoops;              \
														do                                                         \
														{                                                          \
															VectorRegister4f r0 = VectorLoad((float *)p0);         \
															p0 += Inc0;                                            \
															VectorRegister4f res = ins_(r0);                       \
															VectorStoreAligned(res, (float *)p1);                  \
															p1 += sizeof(FVecReg);                                 \
														} while (p1 < end);                                        \
													}

#	define VVM_execVecIns2f(ins_)					{                                                              \
														VVMSer_instruction(0, 2)                                   \
														VVM_INS2_PTRS                                              \
														InsPtr += 7;                                               \
                                                        uint8 *end = p2 + sizeof(FVecReg) * NumLoops;              \
														do                                                         \
														{                                                          \
															VectorRegister4f r0 = VectorLoad((float *)p0);         \
															VectorRegister4f r1 = VectorLoad((float *)p1);         \
															p0 += Inc0;                                            \
															p1 += Inc1;                                            \
															VectorRegister4f res = ins_(r0, r1);                   \
															VectorStoreAligned(res, (float *)p2);                  \
															p2 += sizeof(FVecReg);                                 \
														} while (p2 < end);                                        \
													}
#	define VVM_execVecIns2f_2x(ins_)				{                                                              \
														VVMSer_instruction(0, 3)                                   \
														uint16 *InsPtr16 = (uint16 *)InsPtr;                       \
														VVM_INS3_PTRS                                              \
														uint8 *end = p2 + sizeof(FVecReg) * NumLoops;              \
														InsPtr += 9;                                               \
														ptrdiff_t p23d = p3 - p2;                                  \
														do                                                         \
														{                                                          \
															VectorRegister4f r0 = VectorLoad((float *)p0);         \
															VectorRegister4f r1 = VectorLoad((float *)p1);         \
															p0 += Inc0;                                            \
															p1 += Inc1;                                            \
															VectorRegister4f res = ins_(r0, r1);                   \
															VectorStoreAligned(res, (float *)p2);                  \
															VectorStoreAligned(res, (float *)(p2 + p23d));         \
															p3 += sizeof(FVecReg);                                 \
														} while (p3 < end);                                        \
													}
#	define VVM_execVecIns3f(ins_)					{                                                              \
														VVMSer_instruction(0, 3)                                   \
														uint16 *InsPtr16 = (uint16 *)InsPtr;                       \
														VVM_INS3_PTRS                                              \
														uint8 *end = p3 + sizeof(FVecReg) * NumLoops;              \
														InsPtr += 9;                                               \
														do                                                         \
														{                                                          \
															VectorRegister4f r0 = VectorLoad((float *)p0);         \
															VectorRegister4f r1 = VectorLoad((float *)p1);         \
															VectorRegister4f r2 = VectorLoad((float *)p2);         \
															p0 += Inc0;                                            \
															p1 += Inc1;                                            \
															p2 += Inc2;                                            \
															VectorRegister4f res = ins_(r0, r1, r2);               \
															VectorStoreAligned(res, (float *)p3);                  \
															p3 += sizeof(FVecReg);                                 \
														} while (p3 < end);                                        \
													}
//#	define VVM_execVecIns4f(ins_) check(false);
//#	define VVM_execVecIns5f(ins_) check(false);
#	define VVM_execVecIns4f(ins_)					{                                                              \
														VVMSer_instruction(0, 4)                                   \
														uint16 *InsPtr16 = (uint16 *)InsPtr;                       \
														VVM_INS4_PTRS                                              \
														uint8 *end = p4 + sizeof(FVecReg) * NumLoops;              \
														InsPtr += 11;                                              \
														do                                                         \
														{                                                          \
															VectorRegister4f r0 = VectorLoad((float *)p0);         \
															VectorRegister4f r1 = VectorLoad((float *)p1);         \
															VectorRegister4f r2 = VectorLoad((float *)p2);         \
															VectorRegister4f r3 = VectorLoad((float *)p3);         \
															p0 += Inc0;                                            \
															p1 += Inc1;                                            \
															p2 += Inc2;                                            \
															p3 += Inc3;                                            \
															VectorRegister4f res = ins_(r0, r1, r2, r3);           \
															VectorStoreAligned(res, (float *)p4);                  \
															p4 += sizeof(FVecReg);                                 \
														} while (p4 < end);                                        \
													}
#	define VVM_execVecIns5f(ins_)					{                                                              \
														VVMSer_instruction(0, 4)                                   \
														uint16 *InsPtr16 = (uint16 *)InsPtr;                       \
														VVM_INS5_PTRS                                              \
														uint8 *end = p5 + sizeof(FVecReg) * NumLoops;              \
														InsPtr += 13;                                              \
														do                                                         \
														{                                                          \
															VectorRegister4f r0 = VectorLoad((float *)p0);         \
															VectorRegister4f r1 = VectorLoad((float *)p1);         \
															VectorRegister4f r2 = VectorLoad((float *)p2);         \
															VectorRegister4f r3 = VectorLoad((float *)p3);         \
															VectorRegister4f r4 = VectorLoad((float *)p4);         \
															p0 += Inc0;                                            \
															p1 += Inc1;                                            \
															p2 += Inc2;                                            \
															p3 += Inc3;                                            \
															p4 += Inc4;                                            \
															VectorRegister4f res = ins_(r0, r1, r2, r3, r4);       \
															VectorStoreAligned(res, (float *)p5);                  \
															p5 += sizeof(FVecReg);                                 \
														} while (p5 < end);                                        \
													}

#	define VVM_execVecIns1i(ins_)					{                                                              \
														VVMSer_instruction(1, 1)                                   \
														VVM_INS1_PTRS                                              \
														InsPtr += 5;                                               \
														uint8 *end = p1 + sizeof(FVecReg) * NumLoops;              \
														do                                                         \
														{                                                          \
															VectorRegister4i r0 = VectorIntLoad(p0);               \
															p0 += Inc0;                                            \
															VectorRegister4i res = ins_(r0);                       \
															VectorIntStoreAligned(res, p1);                        \
															p1 += sizeof(FVecReg);                                 \
														} while (p1 < end);                                        \
													}
#	define VVM_execVecIns1i_2x(ins_)				{                                                              \
														VVMSer_instruction(1, 2)                                   \
														VVM_INS2_PTRS                                              \
														InsPtr += 7;                                               \
														uint8 *end = p1 + sizeof(FVecReg) * NumLoops;              \
														do                                                         \
														{                                                          \
															VectorRegister4i r0 = VectorIntLoad(p0);               \
															p0 += Inc0;                                            \
															VectorRegister4i res = ins_(r0);                       \
															VectorIntStoreAligned(res, p1);                        \
															VectorIntStoreAligned(res, p2);                        \
															p1 += sizeof(FVecReg);                                 \
															p2 += sizeof(FVecReg);                                 \
														} while (p1 < end);                                        \
													}
#	define VVM_execVecIns2i(ins_)					{                                                              \
														VVMSer_instruction(1, 2)                                   \
														VVM_INS2_PTRS                                              \
														InsPtr += 7;                                               \
														uint8 *end = p2 + sizeof(FVecReg) * NumLoops;              \
														do                                                         \
														{                                                          \
															VectorRegister4i r0 = VectorIntLoad(p0);               \
															VectorRegister4i r1 = VectorIntLoad(p1);               \
															p0 += Inc0;                                            \
															p1 += Inc1;                                            \
															VectorRegister4i res = ins_(r0, r1);                   \
															VectorIntStoreAligned(res, p2);                        \
															p2 += sizeof(FVecReg);                                 \
														} while (p2 < end);                                        \
													}

#	define VVM_execVecIns3i(ins_)					{                                                              \
														VVMSer_instruction(1, 3)                                   \
														VVM_INS3_PTRS                                              \
														uint16 *InsPtr16 = (uint16 *)InsPtr;                       \
														uint8 *end = p3 + sizeof(FVecReg) * NumLoops;              \
														InsPtr += 9;											   \
														do                                                         \
														{                                                          \
															VectorRegister4i r0 = VectorIntLoad(p0);               \
															VectorRegister4i r1 = VectorIntLoad(p1);               \
															VectorRegister4i r2 = VectorIntLoad(p2);               \
															p0 += Inc0;                                            \
															p1 += Inc1;                                            \
															p2 += Inc2;                                            \
															VectorRegister4i res = ins_(r0, r1, r2);               \
															VectorIntStoreAligned(res, p3);                        \
															p3 += sizeof(FVecReg);                                 \
														} while (p3 < end);                                        \
													}
#define VVM_execVec_exec_index                      {                                                                                                                \
                                                    	VVM_INS0_PTRS                                                                                                \
                                                    	VVMSer_instruction(1, 0);                                                                                    \
                                                    	InsPtr += 3;                                                                                                 \
                                                    	VectorRegister4i Val = VectorIntAdd(VectorIntSet1(StartInstanceThisChunk), VVM_m128iConst(ZeroOneTwoThree)); \
                                                    	VectorRegister4i Four = VectorIntSet1(4);                                                                    \
                                                    	uint8 *end = p0 + sizeof(FVecReg) * NumLoops;                                                                \
                                                    	do {                                                                                                         \
                                                    		VectorIntStoreAligned(Val, p0);                                                                          \
                                                    		Val = VectorIntAdd(Val, Four);                                                                           \
                                                    		p0 += sizeof(FVecReg);                                                                                   \
                                                    	} while (p0 < end);                                                                                          \
                                                    }
#define VVM_execVec_exec_indexf                     {                                                                                                                \
                                                    	VVM_INS0_PTRS                                                                                                \
                                                    	VVMSer_instruction(1, 0);                                                                                    \
                                                    	InsPtr += 3;                                                                                                 \
                                                    	VectorRegister4i Val = VectorIntAdd(VectorIntSet1(StartInstanceThisChunk), VVM_m128iConst(ZeroOneTwoThree)); \
                                                    	VectorRegister4i Four = VectorIntSet1(4);                                                                    \
                                                    	uint8 *end = p0 + sizeof(FVecReg) * NumLoops;                                                                \
                                                    	do {                                                                                                         \
                                                    		VectorStoreAligned(VectorIntToFloat(Val), (float *)p0);                                                   \
                                                    		Val = VectorIntAdd(Val, Four);                                                                           \
                                                    		p0 += sizeof(FVecReg);                                                                                   \
                                                    	} while (p0 < end);                                                                                          \
                                                    }

#define VVM_execVec_exec_index_addi                 {                                                                                                                \
                                                    	VVM_INS1_PTRS                                                                                                \
                                                    	VVMSer_instruction(2, 0);                                                                                    \
                                                    	InsPtr += 5;                                                                                                 \
                                                    	VectorRegister4i Val = VectorIntAdd(VectorIntSet1(StartInstanceThisChunk), VVM_m128iConst(ZeroOneTwoThree)); \
                                                    	VectorRegister4i Four = VectorIntSet1(4);                                                                    \
                                                    	uint8 *end = p1 + sizeof(FVecReg) * NumLoops;                                                                \
                                                    	do {                                                                                                         \
															VectorRegister4i r0 = VectorIntLoad(p0);                                                                 \
															p0 += Inc0;                                                                                              \
                                                    		VectorIntStoreAligned(VectorIntAdd(Val, r0), p1);                                                        \
                                                    		Val = VectorIntAdd(Val, Four);                                                                           \
                                                    		p1 += sizeof(FVecReg);                                                                                   \
                                                    	} while (p1 < end);                                                                                          \
                                                    }
#define VVM_execVec_random_2x						{                                                              \
														VVMSer_instruction(2, 2)                                   \
														VVM_INS2_PTRS                                              \
														uint16 *InsPtr16 = (uint16 *)InsPtr;                       \
														uint8 *end = p1 + sizeof(FVecReg) * NumLoops;              \
														InsPtr += 7;											   \
														do                                                         \
														{                                                          \
															VectorRegister4f r0 = VectorLoad((float *)p0);         \
															p0 += Inc0;                                            \
															VectorRegister4f res0 = VVM_random(r0);                \
															VectorRegister4f res1 = VVM_random(r0);                \
															VectorStoreAligned(res0, (float *)p1);                 \
															VectorStoreAligned(res1, (float *)p2);                 \
															p1 += sizeof(FVecReg);                                 \
															p2 += sizeof(FVecReg);                                 \
														} while (p1 < end);                                        \
													}
#define VVM_execVec_sin_cos                     {                                                                         \
                                                    VVMSer_instruction(0, 2)                                              \
													VVM_INS2_PTRS                                                         \
													InsPtr += 7;                                                          \
                                                    uint8 *end = p1 + sizeof(FVecReg) * NumLoops;                         \
													do                                                                    \
													{                                                                     \
														VectorRegister4f r0 = VectorLoad((float *)p0);                    \
														p0 += Inc0;                                                       \
														VectorSinCos((VectorRegister4f *)p1, (VectorRegister4f *)p2, &r0);\
														p1 += sizeof(FVecReg);                                            \
														p2 += sizeof(FVecReg);                                            \
													} while (p1 < end);                                                   \
                                                }

#define VVM_output32					{                                                                                                                         \
											uint8  RegType            = InsPtr[-1] - (uint8)EVectorVMOp::outputdata_float;										  \
											int NumOutputLoops        = InsPtr[0];																				  \
											uint8 DataSetIdx          = InsPtr[1];																				  \
											uint32 NumOutputInstances = BatchState->ChunkLocalData.NumOutputPerDataSet[DataSetIdx];								  \
											uint32 InstanceOffset     = BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[DataSetIdx];						  \
											uint32 RegTypeOffset      = ExecCtx->DataSets[DataSetIdx].OutputRegisterTypeOffsets[RegType];						  \
																																								  \
											const uint16 * RESTRICT SrcIndices       = (uint16 *)(InsPtr + 4);													  \
											const uint16 * RESTRICT DstIndices       = SrcIndices + NumOutputLoops;												  \
											const uint32 * RESTRICT DstIdxReg        = (uint32 *)BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];                  \
											uint32 *RESTRICT *RESTRICT OutputBuffers = (uint32 **)ExecCtx->DataSets[DataSetIdx].OutputRegisters.GetData();		  \
											InsPtr += 5 + 4 * NumOutputLoops;																					  \
											if (NumOutputInstances == NumInstancesThisChunk) { /*all outputs are written*/										  \
												for (int j = 0; j < NumOutputLoops; ++j) {																		  \
													int     SrcInc = BatchState->RegIncTable[SrcIndices[j]];													  \
													uint32 *SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];											  \
													uint32 *DstReg = OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;								  \
													if (SrcInc == 0) { /*setting from a constant*/																  \
														VVMMemSet32(DstReg, *SrcReg, NumOutputInstances);														  \
													} else {																									  \
														VVMMemCpy(DstReg, SrcReg, sizeof(uint32) * NumOutputInstances);											  \
													}																											  \
												}																												  \
											} else if (NumOutputInstances > 0) {																				  \
												/*not all outputs are written*/																					  \
												for (int j = 0; j < NumOutputLoops; ++j) {																		  \
													int     SrcInc = BatchState->RegIncTable[SrcIndices[j]];													  \
													uint32 * RESTRICT SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];								  \
													uint32 * RESTRICT DstReg = (uint32 *)OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;			  \
													if (SrcInc == 0) { /*setting from a constant*/																  \
														VVMMemSet32(DstReg, *SrcReg, NumOutputInstances);														  \
													} else {																									  \
														uint32 * RESTRICT IdxReg = (uint32 *)DstIdxReg;															  \
														for (uint32 i = 0; i < NumOutputInstances; ++i)															  \
														{																										  \
															DstReg[i] = SrcReg[IdxReg[i]];																		  \
														}																										  \
													}																											  \
												}																												  \
											}																													  \
										}
#define VVM_output16					{																																		   \
											/*@TODO: half data is pretty inefficient.  The output loops could be written much more efficiently.*/								   \
											/*if there's lots of half data I should do this.*/																					   \
											int NumOutputLoops        = InsPtr[0];																								   \
											uint8 DataSetIdx          = InsPtr[1];																								   \
											uint32 NumOutputInstances = BatchState->ChunkLocalData.NumOutputPerDataSet[DataSetIdx];												   \
											uint32 InstanceOffset     = BatchState->ChunkLocalData.StartingOutputIdxPerDataSet[DataSetIdx];										   \
											uint32 RegTypeOffset      = ExecCtx->DataSets[DataSetIdx].OutputRegisterTypeOffsets[2];												   \
																																												   \
											const uint16 * RESTRICT SrcIndices = (uint16 *)(InsPtr + 4);																		   \
											const uint16 * RESTRICT DstIndices = SrcIndices + NumOutputLoops;																	   \
											const uint32 * RESTRICT DstIdxReg  = (uint32 *)BatchState->RegPtrTable[((uint16 *)InsPtr)[1]];                                         \
											uint32 **OutputBuffers             = (uint32 **)ExecCtx->DataSets[DataSetIdx].OutputRegisters.GetData();							   \
											InsPtr += 5 + 4 * NumOutputLoops;																									   \
											if (NumOutputInstances == NumInstancesThisChunk) { /*all outputs are written*/														   \
												for (int j = 0; j < NumOutputLoops; ++j) {																						   \
													int     SrcInc = BatchState->RegIncTable[SrcIndices[j]] >> 4;																   \
													uint32 *SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];															   \
													uint16 *DstReg = (uint16 *)OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;									   \
													uint16 *DstEnd = DstReg + NumOutputInstances;																				   \
																																												   \
													if (SrcInc == 0) { /*setting from a constant*/																				   \
														uint16 HalfVal = float_to_half_fast3_rtne(*SrcReg);																		   \
														while (DstReg < DstEnd) {																								   \
															*DstReg++ = HalfVal;																								   \
														}																														   \
													} else {																													   \
														if (SrcIndices[j] >= ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers) {						   \
															/*coming from a half input buffer, so no conversion necessary*/														   \
															uint16 *SrcReg16 = (uint16 *)SrcReg;																				   \
															while (DstReg < DstEnd) {																							   \
																*DstReg++ = *SrcReg++;																							   \
															}																													   \
														} else {																												   \
															/*coming from a temp register, we must do the half->float conversion*/												   \
															/*@TODO: scalar half->float... we can do 4-wide if this is too slow*/												   \
															while (DstReg < DstEnd) {																							   \
																*DstReg++ = float_to_half_fast3_rtne(*SrcReg);																	   \
																SrcReg += SrcInc;																								   \
															}																													   \
														}																														   \
													}																															   \
												}																																   \
											} else {																															   \
												/*not all outputs are written*/																									   \
												for (int j = 0; j < NumOutputLoops; ++j) {																						   \
													int     SrcInc = BatchState->RegIncTable[SrcIndices[j]];																	   \
													uint32 *SrcReg = (uint32 *)BatchState->RegPtrTable[SrcIndices[j]];															   \
													uint16 *DstReg = (uint16 *)OutputBuffers[RegTypeOffset + DstIndices[j]] + InstanceOffset;									   \
													uint16 *DstEnd = DstReg + NumOutputInstances;																				   \
													if (SrcInc == 0) { /*setting from a constant*/																				   \
														uint16 HalfVal = float_to_half_fast3_rtne(*SrcReg);																		   \
														while (DstReg < DstEnd) {																								   \
															*DstReg++ = HalfVal;																								   \
														}																														   \
													} else {																													   \
														uint32 *IdxReg = (uint32 *)DstIdxReg;																					   \
														if (SrcIndices[j] >= ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers) {						   \
															uint16 *SrcReg16 = (uint16 *)SrcReg;																				   \
															for (uint32 i = 0; i < NumOutputInstances; ++i)																		   \
															{																													   \
																DstReg[i] = SrcReg16[IdxReg[i]];																				   \
															}																													   \
														} else {																												   \
															for (uint32 i = 0; i < NumOutputInstances; ++i)																		   \
															{																													   \
																/*@TODO: we're doing the float->half conversion in scalar... we could do this 4-wide get the next 4 as needed*/	   \
																/*that's a bit more complicated and I don't know if it's worth it... if we find this is slow we can do that.*/	   \
																DstReg[i] = float_to_half_fast3_rtne(SrcReg[IdxReg[i]]);														   \
															}																													   \
														}																														   \
													}																															   \
												}																																   \
											}                                                                                                                                      \
										}


static void execChunkMultipleLoops(FVectorVMExecContext *ExecCtx, FVectorVMBatchState *BatchState, int StartInstanceThisChunk, int NumInstancesThisChunk, int NumLoops, FVectorVMSerializeState *SerializeState, FVectorVMSerializeState *CmpSerializeState)
{
#include "VectorVMExecCore.inl"
}
#undef VVM_execCoreSetupIncVars
#undef VVM_execCoreSetupPtrVars

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

#undef VVM_execVec_exec_index
#undef VVM_execVec_exec_indexf
#undef VVM_execVec_random_2x
#undef VVM_execVec_exec_index_addi
#undef VVM_execVec_exec_index_2x
#undef VVM_execVec_sin_cos

#undef VVM_output32
#undef VVM_output16

