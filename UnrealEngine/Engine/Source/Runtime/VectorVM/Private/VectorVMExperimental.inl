// Copyright Epic Games, Inc. All Rights Reserved.

/*
external functions to improve:
UNiagaraDataInterfaceSkeletalMesh stuff (4 wide randoms)

UNiagaraDataInterfaceSkeletalMesh::GetSkinnedBoneData
SetNumCells
GetNumCells
SetRenderTargetSize
FastMatrixToQuaternion

BasicSkinEmitter:
	- GetFilteredTriangle
	- GetSkinnedTriangleDataWS
	- GetTriUV

pathological case:
	- ComponentRendererTest_SpawnScript_0x4A6253BF_ue: Increases temp reg count


- batch reuse w/o going back to TaskGraph
- prefetch instruction

*/

/*
	- OptimizeContext:
		- static data cooked holds new bytecode, const and input remap data from the original bytecode to the new one.
	- VectorVMState
		- states to execute the VM that's independent of the number of instances required... mallocs memory once for the entire duration
		  of the execution of a script... can/will have more than one per OptCtx.  Caches the input and const remap table upon the first
		  exec() call
	- ExecContext
		- per-execution... can have more than one per VVMState
	- Batch State
		- per-batch.   Gets memory the batch memory cache... can have more than one per ExecCtx
*/

/*
The three steps to running the new VM are:
1. call OptimizeVectorVMScript() using the original bytecode and function bindings as input.
It will set up the FVectorVMOptimizeContext with:
	- New Bytecode
	- Const Remap Table
	- External Function Table (only containing the number of IO params... the function pointers 
	  are set in InitVectorVMState())
	- Some intermediate data for debugging.  These are not saved by default.
	- Number of Constant Buffers and Temporary Registers required

2. Fill out FVectorVMInitData including setting the FVectorVMOptimizeContext from step 1.  Call 
InitVectorVMState().  This will allocate the memory required for the FVectorVMState and the first
batch.

3. call ExecVectorVMState() with the FVectorVMState from step 2.

The VM operates on "Instances."  Instances are organized in groups of 4, (for now at least, 
with AVX-2 we would use groups of 8).  A group of 4 Instances is called a "Loop."  The thread 
hierarchy in the VM has three levels: Batches -> Chunks -> Loops.  A Batch is represented as a 
single Async TaskGraph task.  Batches contain one or more Chunks.  Batches loop over each Chunk, 
executing all of the Bytecode Instructions one Chunk at a time.  Chunks loop over each "Loop" 
executing the SIMD Instructions associated with the Bytecode. 
(More on Chunks and memory usage down below)

In general, (exceptions are discussed further down), data is input into the VM through either a 
DataSet or a Constant Buffer (ConstBuff).  The previous VM would copy all DataSet inputs into 
temporary registers (TempRegs) before operating on them.  This VM can operate directly on the 
Inputs from the DataSets.  Outputs are written to the Outputs in the DataSet.

Each VM Instruction has n Inputs and m Outputs (m is almost always 1 except external_func_call 
and acquire_id).  Inputs can be from one of three places: DataSetInput, ConstBuff, TempReg.  
Instructions always output to TempRegs.  TempRegs and ConstBuffs constitute the memory required 
for each Chunk.

The optimizer takes the bytecode from the original VM as input and outputs a new bytecode for the 
new VM.  The bytecodes are similar in that the first 100-ish Instructions are the same, but they 
are encoded differently.  There's a few new Instructions added as well.

The primary optimization concept is to minimize the number of TempRegs used in the VM in order to 
have a significantly smaller internal state size.  The original VM's Bytecode was bookended by all 
the input and output Instructions and internally all operations worked on TempRegs.  
This new VM has fuse_input* Instructions that combine the input Instruction with the operation 
Instruction so the input Instructions are mostly gone (update_id and external_func_call cannot 
currently fuse... I could add this, but I don't think it will provide much, if any, performance 
improvement).  Outputs are also batched to execute several at once.  Outputs that have no 
processing on them and are effectively a "copy" from the input and are handled with a new 
Instruction: copy_to_output, (they aren't strictly "copied" because the acquireindex Instruction 
could change which slot they get written to).

Instructions are also re-ordered to facilitate minimal TempReg usage.  The acquireindex instruction 
figures out which Instance gets written into which slot and writes these indices into a TempReg.
It effectively determines which Instances are discarded and which are kept.  Output Instructions 
utilize the TempReg written to by the acquireindex Instruction to write the contents of a TempReg
or ConstBuff to a DataSetOutput.  Output Instructions are re-ordered to execute immediately 
following the last Instruction that uses the TempReg it writes.

Constant buffers are used elsewhere in UE and have a fixed, static layout it memory. They have many
values interleaved together.  Some of these variables are required by the VM to execute a script, 
some are not.  This leads to gaps and random memory access when reading this sparse constant table.
The optimizer figures out exactly which constant buffers are required for the script, and how to 
map the constant buffer table that comes from UE into the smaller set required by the VM for a 
particular script. This map is saved in the OptimizerContext.  The constants are copied and 
broadcasted 4-wide to the VM's internal state in the VectorVMInit() function.

Most instructions in the original VM have a control byte immediately following the opcode to
specify whether a register used is a ConstBuff or a TempReg.  Input registers into external
functions used a different encoding: the high bit (of a 2 byte index) is set when a register is
temporary, or not set when it's constant.  The new VM uses a universal encoding for all registers
everywhere: 16 bit indices, high bit set = const, otherwise temp register.

Work for each execution of the VM gets broken up two ways: Batches and Chunks.  A Batch is
effectively a "thread" and represents a single Async Task in the TaskGraph.  A Batch can further be
split up into multiple Chunks.  The only reason to split work into Chunks is to minimize the memory 
footprint of each Batch.  A Batch will loop over each Chunk and execute the exact same instructions 
on each Chunk.  There are two CVars to control these: GVVMChunkSizeInBytes and 
GVVMMaxBatchesPerExec.  Chunk size is ideally the size of the L1, (VectorVMInit() will consider a 
little bit of overhead for the bytecode and stack when executing).  This should hopefully mean that 
all work done by the VM fits within the L1.  The number of Batches corresponds to how many 
TaskGraph tasks are created, and are thus a function of the the available hardware threads during 
runtime... a difficult thing to properly load balance.

For example if the L1 D$ is 32kb, and the script's Bytecode is 1kb, and we assume an overhead of 
512 bytes: We set the GVVMChunkSizeInBytes to 32768.  The FVectorVMInit() function will do: 
32768 - 1024 - 512 = 31232 bytes per chunk.
[@TODO: maybe we should remove the GVVMChunkSizeInBytes and just read the L1 size directly, or if 
GVVMChunkSizeInBytes is 0 it signals to use the L1 size]

The first Batch's memory is allocated directly following the VectorVMState in the VectorVMInit() 
function.  When ExecBatch() gets called from the TaskGraph it first attempts to reuse an existing 
batch's memory that's already finished executing.  If it can't find an existing batch that's 
finished it will allocate new memory for this batch.  Once a batch has its memory it will setup its
RegisterData pointer aligned to 64 bytes.  The RegisterData pointer holds all ConstBuffs and 
TempRegs required for the execution of a single Chunk; Batches will usually loop over several 
Chunks.  The RegisterData holds 4-wide, 32 bit variables only (16 bytes). In RegisterData the 
ConstBuffs come first, followed by the TempRegs.

When the first Batch's memory is allocated, the required ConstBuffs are broadcasted 4-wide into the
beginning of the Batch's RegisterData.  Only the constants that are required, as determined by 
FVectorVMOptimize(), are set there.  When the memory is allocated for all other batches the 
ConstBuffs are memcpy'd from the first batch.

The number of Instances a Chunk can operate on is a function of the number of bytes allocated to 
the Chunk and the number of TempRegs and ConstBuffs, (as determined by the VectorVMOptimize() 
function), and a per-chunk overhead.  For example, a script setup in the following manner:

GVVMChunkSizeInBytes: 16384 bytes
NumBytecodeBytes:       832 bytes
FixedChunkOverhead:     500 bytes
NumConstBuffers:         12
NumTempRegisters:         8
NumDataSets:              2
MaxRegsForExtFn           5

VectorVMInit() does the following computation:
	1.    16 bytes = NumDataSets * 8      <- track #outputs written for each DataSet
	2.    10 bytes = MaxRegsForExtFn * 2  <- registers for ext fns get pre-decoded
	3.   526 bytes = 500 + 16 + 10        <- BatchOverheadSize
	1. 15026 bytes = 16384 - 532 - 526    <- max size of internal VM state
	2.   192 bytes = 12 * 16              <- NumConstBuffers * sizeof(int32) * 4
	3. 14834 bytes = 15026 - 192          <- #bytes remaining for TempRegs
	4. 115 Loops   = 14843 / (8 * 16)     <- 8 TempRegs * 16 bytes per loop (4 instances)
	5. 460 Instances Per Chunk.

This particular script can execute 460 instances per chunk with a GVVMChunkSizeInBytes of 16384.

As described above the new VM has a universal register encoding using 16 bit indices with the high 
bit signifying whether the register the Instruction requires is a TempReg or ConstBuff.  This 
allows the VM to decode which registers are used by an operation very efficiently, 4 at a time 
using SIMD.  The equations to compute the pointers to registers required for operations are as 
follows: (in byte offsets from the beginning of the Batch's RegisterData)
	ConstBuff: RegisterData + 16 * ConstIdx
	TempReg  : RegisterData + 16 * NumConsts + NumLoops * TempRegIdx
	
In addition to computing the offsets the "increment" variable is computed when the Instruction is 
decoded.  The increment is 0xFFFFFFFF for TempRegs and 0 for ConstBuffs.  Each operation loops over
registers for each Instance in the Chunk (4 at a time), and the loop index is logically AND'd with 
the increment value such that ConstBuffs always read from the same place and TempRegs read from the 
normal loop index.

4 registers are always decoded for each Instruction regardless of how many (if any) are used by the 
Instruction.  External functions decode their instructions into a special buffer in the batch's 
ChunkLocalData.  If they have more than four operands, the VM loops as many times as necessary to 
decode all the registers.  This greatly simplifies the code required to decode the registers in 
user-defined functions.  All external functions are backwards compatible with the previous VM.

Memory and batches work different on the new VM compared to the old VM.  In the old VM the Exec() 
lambda is passed a BatchIdx which determines which instances to work on.  
The calculation was: BatchIdx * NumChunksPerBatch * NumLoopsPerChunk.  
This means that each BatchIdx will always work on the same set of instances.  This means that the 
memory for each batch must always be allocated and used only once.  In times of high thread 
contention batch memory could be sitting around unused.

The new VM works differently.  Each time ExecVVMBatch() is called from the TaskGraph it tries to 
reuse previously-allocated batches that have finished executing.  If it cannot reuse one, it will
allocate new memory and copy the ConstBuffs from the first batch.  The function
AssignInstancesToBatch() thread-safely grabs the next bunch of instances and assigns them to this 
batch.

There are 11 new fused_input instructions:
	fused_input1_1 //op has 1 input operand it's an input
	fused_input2_1 //op has 2 input operands, register 0 is an input
	fused_input2_2 //op has 2 input operands, register 1 is an input
	fused_input2_3 //op has 2 input operands, register 0 and 1 are inputs
	fused_input3_1 //op has 3 input operands, register 0 is an input
	fused_input3_2 //op has 3 input operands, register 1 is an input
	fused_input3_3 //op has 3 input operands, register 0 and 1 are inputs
	fused_input3_4 //op has 3 input operands, register 2 is an input
	fused_input3_5 //op has 3 input operands, register 0 and 2 are inputs
	fused_input3_6 //op has 3 input operands, register 1 and 2 are inputs
	fused_input3_7 //op has 3 input operands, register 0, 1 and 2 are inputs

Instructions generally have 1, 2 or 3 inputs.  They are usually TempRegs or ConstBuffs.  In some 
cases, one or more of the TempRegs can be changed to a DataSetInput.  In order to do that, the 
VectorVMOptimize() function injects the appropriate fusedinput operation before the Instruction.  
For example, if the add Instruction adds ConstBuff 6 to DataSetInput 9, the VectorVMOptimize() 
instruction will emit two Instructions: fused_input2_2, and add.  The first digit in the 
fused_input instruction is how many operands the instruction has, and the second digit is a binary
representation of which operands are be changed to DataSetInputs... in this case 2 = 2nd operand.  
As another example if an fmadd Instruction was in the original Bytecode that took DataSetInputs for 
operands 0 and 2 FVectorVMOptimize() would emit a fused_input3_5 instruction before the fmadd.

acquireindex logic is different from the original VM's.  The original VM wrote which slot the
to read from, and a -1 to indicate "skip".  This required a branch for each instance being written,
for output instruction.  If the keep/discard boolean was distributed similar to white noise there 
would be massive mispredict penalities.

The new VM's acquireindex instruction writes which slot to write into.  This allows for branch-free 
write Output Instructions.  For example: if it was determined that Instances 1, 3 and 4 were to be 
discarded, acquireindex would output:
	0, 1, 1, 2, 2, 2, 3

These correspond to the slots that get written to.  So the Output instructions will loop over each 
index, and write it into the slot specified by the index. ie:
	write Instance 0 into slot 0
	write Instance 1 into slot 1
	write Instance 2 into slot 1
	write Instance 3 into slot 2
	write Instance 4 into slot 2
	write Instance 5 into slot 2
	write Instance 6 into slot 3

In order to facilitate this change, aquire_id and update_id also needed to be changed.  update_id 
and acquire_id were completely re-written in order to be lock-free.  The original VM's DataSets had 
two separate arrays: FreeIDsTable and SpawnedIDsTable.  The FreeIDs table was pre-allocated to have 
enough room for the persistent IDs in the worst-case situation of every single instance being freed 
on a particular execution of the VM.  The acquire_id function pulls IDs out of the FreeIDs table 
into a TempReg and writes them to the SpawnedIDs table.  In order for elements to be put into 
SpawnedIDs they must first be removed from FreeIDs.  Therefore it is impossible for the counts of 
FreeIDs + SpawnedIDs to exceed the number of instances for a particlar execution of a VM -- the 
same number that is pre-allocated to the FreeIDs.  I removed the SpawnedIDs table and simply write 
the SpawnedIDs to the end of the FreeIDs table.  I keep a separate index: NumSpawnedIDs in the 
DataSet.  This allows for complete lock-free manipulation of both sets of data as it's just two 
numbers keeping track of the two.  ie:

DataSet->FreeIDsTable:
[------------0000000000000000000000000000000**********]
             ^ NumFreeIds                   ^ FreeIDsTable.Max() - NumSpawnedIDs
	- represents FreeIDs
	0 represents unused spaced
	* represents SpawnedIDs

Upon observing the Bytecode of dozens of scripts I recognized that DataSetInputs are often directly
written to DataSetOutputs.  The new VM has a new instruction called copy_to_output which takes a 
count and a list of DataSetInputs and DataSetOutputs and uses the acquireindex index to write 
directly between the two without requiring a TempReg. Additionally most outputs get grouped 
together.

I also added new output_batch* instructions to write more than one output at a time:
	output_batch8
	output_batch7
	output_batch4
	output_batch3
	output_batch2

7 and 3 may seem weird, but they're there to utilize the fact that the instruction decoded looks at
4 registers at a time, so decoding the index is free.
It is guaranteed by the optimizer that the index for output_batch8 and output_batch4 comes from a 
TempReg, not a ConstBuff so the decoding can be optimized.
*/

static void *VVMDefaultRealloc(void *Ptr, size_t NumBytes, const char *Filename, int LineNumber)
{
	return FMemory::Realloc(Ptr, NumBytes);
}

static void VVMDefaultFree(void *Ptr, const char *Filename, int LineNumber)
{
	return FMemory::Free(Ptr);
}

#include "./VectorVMExperimental_Serialization.inl"

#if VECTORVM_SUPPORTS_EXPERIMENTAL

#define VVM_CACHELINE_SIZE				64
#define VVM_CHUNK_FIXED_OVERHEAD_SIZE	512

#define VVM_MIN(a, b)               ((a) < (b) ? (a) : (b))
#define VVM_MAX(a, b)               ((a) > (b) ? (a) : (b))
#define VVM_CLAMP(v, min, max)      ((v) < (min) ? (min) : ((v) < (max) ? (v) : (max)))
#define VVM_ALIGN(num, alignment)   (((size_t)(num) + (alignment) - 1) & ~((alignment) - 1))
#define VVM_ALIGN_4(num)            (((size_t)(num) + 3) & ~3)
#define VVM_ALIGN_16(num)           (((size_t)(num) + 15) & ~15)
#define VVM_ALIGN_32(num)           (((size_t)(num) + 31) & ~31)
#define VVM_ALIGN_64(num)           (((size_t)(num) + 63) & ~63)
#define VVM_ALIGN_CACHELINE(num)	(((size_t)(num) + (VVM_CACHELINE_SIZE - 1)) & ~(VVM_CACHELINE_SIZE - 1))

#if VECTORVM_SUPPORTS_AVX
#include "ThirdParty/SSEMathFun/avx_mathfun.h"
#define VVM_PTR_ALIGN    VVM_ALIGN_32
#define VVM_REG_SIZE     sizeof(__m256)
#else
#define VVM_PTR_ALIGN    VVM_ALIGN_16
#define VVM_REG_SIZE     sizeof(FVecReg)
#endif

//to avoid memset/memcpy when statically initializing sse variables
#define VVMSet_m128Const(Name, V)                static const MS_ALIGN(16) float VVMConstVec4_##Name##4[4]   GCC_ALIGN(16) = { V, V, V, V }
#define VVMSet_m128Const4(Name, V0, V1, V2, V3)  static const MS_ALIGN(16) float VVMConstVec4_##Name##4[4]   GCC_ALIGN(16) = { V0, V1, V2, V3 }
#define VVMSet_m128iConst(Name, V)               static const MS_ALIGN(16) uint32 VVMConstVec4_##Name##4i[4] GCC_ALIGN(16) = { V, V, V, V }
#define VVMSet_m128iConst4(Name, V0, V1, V2, V3) static const MS_ALIGN(16) uint32 VVMConstVec4_##Name##4i[4] GCC_ALIGN(16) = { V0, V1, V2, V3 }	/* equiv to setr */

#define VVM_m128Const(Name)  (*(VectorRegister4f *)&(VVMConstVec4_##Name##4))
#define VVM_m128iConst(Name) (*(VectorRegister4i *)&(VVMConstVec4_##Name##4i))

#if VECTORVM_SUPPORTS_AVX
#define VVM_m256Const(Name)  (*(__m256 *)&(VVMConstVec8_##Name##8))
#define VVM_m256iConst(Name) (*(__m256i *)&(VVMConstVec8_##Name##8i))
#define VVMSet_m256Const(Name, V)                                static const MS_ALIGN(32) float VVMConstVec8_##Name##8[8]   GCC_ALIGN(32) = { V, V, V, V }
#define VVMSet_m256Const8(Name, V0, V1, V2, V3, V4, V5, V6, V7)  static const MS_ALIGN(32) float VVMConstVec8_##Name##8[8]   GCC_ALIGN(32) = { V0, V1, V2, V3, V4, V5, V6, V7 }
#define VVMSet_m256iConst(Name, V)                               static const MS_ALIGN(32) uint32 VVMConstVec8_##Name##8i[8] GCC_ALIGN(32) = { V, V, V, V }
#define VVMSet_m256iConst8(Name, V0, V1, V2, V3, V4, V5, V6, V7) static const MS_ALIGN(32) uint32 VVMConstVec8_##Name##8i[8] GCC_ALIGN(32) = { V0, V1, V2, V3, V4, V5, V6, V7 }	/* equiv to setr */
#else
#define VVM_m256Const(Name)
#define VVM_m256iConst(Name)
#define VVMSet_m256Const(Name, V)
#define VVMSet_m256Const8(Name, V0, V1, V2, V3, V4, V5, V6, V7)
#define VVMSet_m256iConst(Name, V)
#define VVMSet_m256iConst8(Name, V0, V1, V2, V3, V4, V5, V6, V7)
#endif

VVMSet_m128Const(   One             , 1.f);
VVMSet_m128Const(   NegativeOne     , -1.f);
VVMSet_m128Const(   OneHalf         , 0.5f);
VVMSet_m128Const(   Epsilon         , 1.e-8f);
VVMSet_m128Const(   HalfPi          , 3.14159265359f * 0.5f);
VVMSet_m128Const(   FastSinA        , 7.5894663844f);
VVMSet_m128Const(   FastSinB        , 1.6338434578f);
VVMSet_m128iConst(  FMask           , 0xFFFFFFFF);
VVMSet_m128iConst4( ZeroOneTwoThree , 0, 1, 2, 3);
VVMSet_m128iConst4( ZeroTwoFourSix  , 0, 2, 4, 6);
VVMSet_m128Const4(  ZeroOneTwoThree , 0.f, 1.f, 2.f, 3.f);
VVMSet_m128iConst(  RegOffsetMask   , 0x7FFF);
VVMSet_m128Const(   RegOneOverTwoPi , 1.f / 2.f / 3.14159265359f);
VVMSet_m128iConst(  AlmostTwoBits   , 0x3fffffff);

VVMSet_m256Const(   One                                  , 1.f);
VVMSet_m256Const(   NegativeOne                          , -1.f);
VVMSet_m256Const(   OneHalf                              , 0.5f);
VVMSet_m256Const(   Epsilon                              , 1.e-8f);
VVMSet_m256Const(   HalfPi                               , 3.14159265359f * 0.5f);
VVMSet_m256Const(   FastSinA                             , 7.5894663844f) ;
VVMSet_m256Const(   FastSinB                             , 1.6338434578f);
VVMSet_m256iConst(  FMask                                , 0xFFFFFFFF);
VVMSet_m256iConst(  One                                  , 1);
VVMSet_m256iConst(  NegativeOne                          , 0xFFFFFFFF);
VVMSet_m256iConst8( ZeroOneTwoThreeFourFiveSixSeven      , 0, 1, 2, 3, 4, 5, 6, 7);
VVMSet_m256iConst8( ZeroTwoFourSixEightTenTwelveFourteen , 0, 2, 4, 6, 8, 10, 12, 14);
VVMSet_m256Const8(  ZeroOneTwoThreeFourFiveSixSeven      , 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f);
VVMSet_m256iConst(  RegOffsetMask                        , 0x7FFF);
VVMSet_m256Const(   RegOneOverTwoPi                      , 1.f / 2.f / 3.14159265359f);
VVMSet_m256iConst(  AlmostTwoBits                        , 0x3fffffff);

#define VVM_vecStep(a, b)                  VectorStep(VectorSubtract(a, b))
#define VVM_vecFloatToBool(v)              VectorCompareGT(v, VectorZeroFloat())
#define VVM_vecBoolToFloat(v)              VectorSelect(v, VVM_m128Const(One), VectorZeroFloat());
#define VVM_vecIntToBool(v)                VectorIntCompareGT(v, VectorSetZero())
#define VVM_vecBoolToInt(v)                VectorIntSelect(v, VectorIntSet1(1), VectorSetZero())
//safe instructions -- handle divide by zero "gracefully" by returning 0
#define VVM_safeIns_div(v0, v1)            VectorSelect(VectorCompareGT(VectorAbs(v1), VVM_m128Const(Epsilon)), VectorDivide(v0, v1)                  , VectorZeroFloat())
#define VVM_safeIns_rcp(v)                 VectorSelect(VectorCompareGT(VectorAbs(v) , VVM_m128Const(Epsilon)), VectorVMAccuracy::Reciprocal(v)       , VectorZeroFloat())
#define VVM_safe_sqrt(v)                   VectorSelect(VectorCompareGT(v            , VVM_m128Const(Epsilon)), VectorVMAccuracy::Sqrt(v)             , VectorZeroFloat())
#define VVM_safe_log(v)                    VectorSelect(VectorCompareGT(v            , VectorZeroFloat())     , VectorLog(v)		                  , VectorZeroFloat())
#define VVM_safe_pow(v0, v1)               VectorSelect(VectorCompareGT(v0           , VVM_m128Const(Epsilon)), VectorPow(v0, v1)                     , VectorZeroFloat())
#define VVM_safe_rsq(v)                    VectorSelect(VectorCompareGT(v            , VVM_m128Const(Epsilon)), VectorReciprocalSqrt(v)               , VectorZeroFloat())
#define VVM_random(v)                      VectorMultiply(VectorSubtract(VectorRegister4f(VectorCastIntToFloat(VectorIntOr(VectorShiftRightImmLogical(VVMXorwowStep(BatchState), 9), VectorIntSet1(0x3F800000)))), VVM_m128Const(One)), v)
#define VVM_randomi(v)                     VectorFloatToInt(VectorMultiply(VectorSubtract(VectorRegister4f(VectorCastIntToFloat(VectorIntOr(VectorShiftRightImmLogical(VVMXorwowStep(BatchState), 9), VectorIntSet1(0x3F800000)))), VVM_m128Const(One)), *(VectorRegister4f *)&v))
#define VVM_vecACosFast(v)                 VectorATan2(VVM_safe_sqrt(VectorMultiply(VectorSubtract(VVM_m128Const(One), v), VectorAdd(VVM_m128Const(One), v))), v)

//new merged instructions
#define VVM_cmplt_select(v0, v1, v2, v3)    VectorSelect(VectorCompareLT(v0, v1), v2, v3)
#define VVM_cmple_select(v0, v1, v2, v3)    VectorSelect(VectorCompareLE(v0, v1), v2, v3)
#define VVM_cmpeq_select(v0, v1, v2, v3)    VectorSelect(VectorCompareEQ(v0, v1), v2, v3)
#define VVM_cmplti_select(v0, v1, v2, v3)   VectorSelect(VectorCastIntToFloat(VectorIntCompareLT(*(VectorRegister4i *)&v0, *(VectorRegister4i *)&v1)), v2, v3)
#define VVM_cmplei_select(v0, v1, v2, v3)   VectorSelect(VectorCastIntToFloat(VectorIntCompareLE(*(VectorRegister4i *)&v0, *(VectorRegister4i *)&v1)), v2, v3)
#define VVM_cmpeqi_select(v0, v1, v2, v3)   VectorSelect(VectorCastIntToFloat(VectorIntCompareEQ(*(VectorRegister4i *)&v0, *(VectorRegister4i *)&v1)), v2, v3)
#define VVM_cmplt_logic_and(v0, v1, v2)     VectorCastIntToFloat(VectorIntAnd(VectorCastFloatToInt(VectorCompareLT(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmple_logic_and(v0, v1, v2)     VectorCastIntToFloat(VectorIntAnd(VectorCastFloatToInt(VectorCompareLE(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpgt_logic_and(v0, v1, v2)     VectorCastIntToFloat(VectorIntAnd(VectorCastFloatToInt(VectorCompareGT(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpge_logic_and(v0, v1, v2)     VectorCastIntToFloat(VectorIntAnd(VectorCastFloatToInt(VectorCompareGE(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpeq_logic_and(v0, v1, v2)     VectorCastIntToFloat(VectorIntAnd(VectorCastFloatToInt(VectorCompareEQ(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpne_logic_and(v0, v1, v2)     VectorCastIntToFloat(VectorIntAnd(VectorCastFloatToInt(VectorCompareNE(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmplti_logic_and(v0, v1, v2)    VectorIntAnd(VectorIntCompareLT(v0, v1), v2)
#define VVM_cmplei_logic_and(v0, v1, v2)    VectorIntAnd(VectorIntCompareLE(v0, v1), v2)
#define VVM_cmpgti_logic_and(v0, v1, v2)    VectorIntAnd(VectorIntCompareGT(v0, v1), v2)
#define VVM_cmpgei_logic_and(v0, v1, v2)    VectorIntAnd(VectorIntCompareGE(v0, v1), v2)
#define VVM_cmpeqi_logic_and(v0, v1, v2)    VectorIntAnd(VectorIntCompareEQ(v0, v1), v2)
#define VVM_cmpnei_logic_and(v0, v1, v2)    VectorIntAnd(VectorIntCompareNEQ(v0, v1), v2)
#define VVM_cmplt_logic_or(v0, v1, v2)      VectorCastIntToFloat(VectorIntOr(VectorCastFloatToInt(VectorCompareLT(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmple_logic_or(v0, v1, v2)      VectorCastIntToFloat(VectorIntOr(VectorCastFloatToInt(VectorCompareLE(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpgt_logic_or(v0, v1, v2)      VectorCastIntToFloat(VectorIntOr(VectorCastFloatToInt(VectorCompareGT(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpge_logic_or(v0, v1, v2)      VectorCastIntToFloat(VectorIntOr(VectorCastFloatToInt(VectorCompareGE(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpeq_logic_or(v0, v1, v2)      VectorCastIntToFloat(VectorIntOr(VectorCastFloatToInt(VectorCompareEQ(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmpne_logic_or(v0, v1, v2)      VectorCastIntToFloat(VectorIntOr(VectorCastFloatToInt(VectorCompareNE(v0, v1)), *(VectorRegister4i *)&v2))
#define VVM_cmplti_logic_or(v0, v1, v2)     VectorIntOr(VectorIntCompareLT(v0, v1), v2)
#define VVM_cmplei_logic_or(v0, v1, v2)     VectorIntOr(VectorIntCompareLE(v0, v1), v2)
#define VVM_cmpgti_logic_or(v0, v1, v2)     VectorIntOr(VectorIntCompareGT(v0, v1), v2)
#define VVM_cmpgei_logic_or(v0, v1, v2)     VectorIntOr(VectorIntCompareGE(v0, v1), v2)
#define VVM_cmpeqi_logic_or(v0, v1, v2)     VectorIntOr(VectorIntCompareEQ(v0, v1), v2)
#define VVM_cmpnei_logic_or(v0, v1, v2)     VectorIntOr(VectorIntCompareNEQ(v0, v1), v2)
#define VVM_mad_add(v0, v1, v2, v3)         VectorAdd(VectorMultiplyAdd(v0, v1, v2), v3)
#define VVM_mad_sub0(v0, v1, v2, v3)        VectorSubtract(VectorMultiplyAdd(v0, v1, v2), v3)
#define VVM_mad_sub1(v0, v1, v2, v3)        VectorSubtract(v3, VectorMultiplyAdd(v0, v1, v2))
#define VVM_mad_mul(v0, v1, v2, v3)         VectorMultiply(VectorMultiplyAdd(v0, v1, v2), v3)
#define VVM_mad_sqrt(v0, v1, v2)            VectorSqrt(VectorMultiplyAdd(v0, v1, v2))
#define VVM_mad_mad0(v0, v1, v2, v3, v4)    VectorMultiplyAdd(v3, v4, VectorMultiplyAdd(v0, v1, v2))
#define VVM_mad_mad1(v0, v1, v2, v3, v4)    VectorMultiplyAdd(VectorMultiplyAdd(v0, v1, v2), v3, v4)
#define VVM_mul_mad0(v0, v1, v2, v3)        VectorMultiplyAdd(VectorMultiply(v0, v1), v2, v3)
#define VVM_mul_mad1(v0, v1, v2, v3)        VectorMultiplyAdd(v2, v3, VectorMultiply(v0, v1))
#define VVM_mul_add(v0, v1, v2)             VectorAdd(VectorMultiply(v0, v1), v2)
#define VVM_mul_sub0(v0, v1, v2)            VectorSubtract(VectorMultiply(v0, v1), v2)
#define VVM_mul_sub1(v0, v1, v2)            VectorSubtract(v2, VectorMultiply(v0, v1))
#define VVM_mul_mul(v0, v1, v2)             VectorMultiply(VectorMultiply(v0, v1), v2)
#define VVM_mul_max(v0, v1, v2)             VectorMax(VectorMultiply(v0, v1), v2)
#define VVM_add_mad1(v0, v1, v2, v3)        VectorMultiplyAdd(v2, v3, VectorAdd(v0, v1))
#define VVM_add_add(v0, v1, v2)             VectorAdd(VectorAdd(v0, v1), v2)
#define VVM_sub_cmplt1(v0, v1, v2)			VectorCompareLT(v2, VectorSubtract(v0, v1))
#define VVM_sub_neg(v0, v1)                 VectorNegate(VectorSubtract(v0, v1))
#define VVM_sub_mul(v0, v1, v2)				VectorMultiply(VectorSubtract(v0, v1), v2)
#define VVM_div_mad0(v0, v1, v2, v3)        VectorMultiplyAdd(VVM_safeIns_div(v0, v1), v2, v3)
#define VVM_div_f2i(v0, v1)                 VectorFloatToInt(VVM_safeIns_div(VectorCastIntToFloat(v0), VectorCastIntToFloat(v1)))
#define VVM_div_mul(v0, v1, v2)				VectorMultiply(VVM_safeIns_div(v0, v1), v2)
#define VVM_muli_addi(v0, v1, v2)           VectorIntAdd(VectorIntMultiply(v0, v1), v2)
#define VVM_addi_bit_rshift(v0, v1, v2)     VVMIntRShift(VectorIntAdd(v0, v1), v2)
#define VVM_addi_muli(v0, v1, v2)           VectorIntMultiply(VectorIntAdd(v0, v1), v2)
#define VVM_i2f_div0(v0, v1)                VVM_safeIns_div(VectorIntToFloat(VectorCastFloatToInt(v0)), v1)
#define VVM_i2f_div1(v0, v1)                VVM_safeIns_div(v1, VectorIntToFloat(VectorCastFloatToInt(v0))) 
#define VVM_i2f_mul(v0, v1)                 VectorMultiply(VectorIntToFloat(VectorCastFloatToInt(v0)), v1)
#define VVM_i2f_mad0(v0, v1, v2)            VectorMultiplyAdd(VectorIntToFloat(VectorCastFloatToInt(v0)), v1, v2)
#define VVM_i2f_mad1(v0, v1, v2)            VectorMultiplyAdd(v0, v1, VectorIntToFloat(VectorCastFloatToInt(v2)))
#define VVM_f2i_select1(mask, v0, v1)       VectorIntSelect(mask, VectorFloatToInt(VectorCastIntToFloat(v0)), v1)
#define VVM_f2i_maxi(v0, v1)                VectorIntMax(VectorFloatToInt(VectorCastIntToFloat(v0)), v1)
#define VVM_f2i_addi(v0, v1)                VectorIntAdd(VectorFloatToInt(VectorCastIntToFloat(v0)), v1)
#define VVM_fmod_add(v0, v1, v2)            VectorAdd(VectorMod(v0, v1), v2)
#define VVM_bit_and_i2f(v0, v1)             VectorIntToFloat(VectorIntAnd(VectorCastFloatToInt(v0), VectorCastFloatToInt(v1)))
#define VVM_bit_rshift_bit_and(v0, v1, v2)  VectorIntAnd(VVMIntRShift(v0, v1), v2)
#define VVM_neg_cmplt(v0, v1)               VectorCompareLT(VectorNegate(v0), v1)
#define VVM_bit_or_muli(v0, v1, v2)         VectorIntMultiply(VectorIntOr(v0, v1), v2)
#define VVM_bit_lshift_bit_or(v0, v1, v2)   VectorIntOr(VVMIntLShift(v0, v1), v2)
#define VVM_random_add(v0, v1)              VectorAdd(VVM_random(v0), v1)
#define VVM_max_f2i(v0, v1)                 VectorFloatToInt(VectorMax(VectorCastIntToFloat(v0), VectorCastIntToFloat(v1)))
#define VVM_select_mul(v0, v1, v2, v3)      VectorMultiply(VectorSelect(v0, v1, v2), v3)
#define VVM_select_add(v0, v1, v2, v3)      VectorAdd(VectorSelect(v0, v1, v2), v3)


#if VECTORVM_SUPPORTS_AVX
#define VVM_select_AVX(m, v0, v1)			_mm256_blendv_ps(v1, v0, m)
#define VVM_selecti_AVX(m, v0, v1)			_mm256_blendv_epi8(v1, v0, m)
#define VVM_abs_AVX(v)                      _mm256_and_ps(v, _mm256_castsi256_ps(_mm256_set1_epi32(~(1 << 31))))
#define VVM_pow_AVX(base, exp)               exp256_ps(_mm256_mul_ps(log256_ps(base), exp))
#define VVM_neg_AVX(v)                      _mm256_sub_ps(_mm256_setzero_ps(), v)

union VVMXXMYMMUnion
{
	__m256 ymm;
	__m128 xmm[2];
	__m256i ymmi;
	__m128i xmmi[2];
};

//AVX compare macros
#define VVM_cmplt_AVX(v0, v1)               _mm256_cmp_ps(v0, v1, _CMP_LT_OS)
#define VVM_cmple_AVX(v0, v1)               _mm256_cmp_ps(v0, v1, _CMP_LE_OS)
#define VVM_cmpgt_AVX(v0, v1)               _mm256_cmp_ps(v0, v1, _CMP_GT_OS)
#define VVM_cmpge_AVX(v0, v1)               _mm256_cmp_ps(v0, v1, _CMP_GE_OS)
#define VVM_cmpeq_AVX(v0, v1)               _mm256_cmp_ps(v0, v1, _CMP_EQ_OQ)
#define VVM_cmpneq_AVX(v0, v1)              _mm256_cmp_ps(v0, v1, _CMP_NEQ_OQ)

#define VVM_cmplti_AVX(v0, v1)              _mm256_cmpgt_epi32(v1, v0)
#define VVM_cmplei_AVX(v0, v1)              _mm256_or_si256(_mm256_cmpgt_epi32(v1, v0), _mm256_cmpeq_epi32(v0, v1))
#define VVM_cmpgti_AVX(v0, v1)              _mm256_cmpgt_epi32(v0, v1)
#define VVM_cmpgei_AVX(v0, v1)              _mm256_or_si256(_mm256_cmpgt_epi32(v0, v1), _mm256_cmpeq_epi32(v0, v1))
#define VVM_cmpeqi_AVX(v0, v1)              _mm256_cmpeq_epi32(v0, v1)
#define VVM_cmpneqi_AVX(v0, v1)             _mm256_xor_si256(_mm256_cmpeq_epi32(v0, v1), VVM_m256iConst(FMask))

#define VVM_vecStep_AVX(v0, v1)             VVM_select_AVX(VVM_cmpge_AVX(_mm256_sub_ps(v0, v1), _mm256_setzero_ps()), VVM_m256Const(One), _mm256_setzero_ps())
#define VVM_vecFloatToBool_AVX(v)           VVM_cmpgt_AVX  (v, _mm256_setzero_ps())
#define VVM_vecBoolToFloat_AVX(v)           VVM_select_AVX (v, VVM_m256Const(One), _mm256_setzero_ps());
#define VVM_vecIntToBool_AVX(v)             VVM_cmpgti_AVX (v, _mm256_setzero_si256())
#define VVM_vecBoolToInt_AVX(v)             VVM_selecti_AVX(v, _mm256_set1_epi32(1), _mm256_setzero_si256())
#define VVM_vecACosFast_AVX(v)              VectorATan2_AVX(_mm256_sqrt_ps(_mm256_mul_ps(_mm256_sub_ps(VVM_m256Const(One), v), _mm256_add_ps(VVM_m256Const(One), v))), v)
//VectorATan2(VectorSqrt(VectorMultiply(VectorSubtract(VVM_m128Const(One), v), VectorAdd(VVM_m128Const(One), v))), v)

//safe instructions -- handle divide by zero "gracefully" by returning 0
#define VVM_safeIns_div_AVX(v0, v1)        VVM_select_AVX(VVM_cmpgt_AVX(VVM_abs_AVX(v1), VVM_m256Const(Epsilon)), _mm256_div_ps(v0, v1)                 , _mm256_setzero_ps())
#define VVM_safeIns_rcp_AVX(v)             VVM_select_AVX(VVM_cmpgt_AVX(VVM_abs_AVX(v) , VVM_m256Const(Epsilon)), _mm256_rcp_ps(v)                      , _mm256_setzero_ps())
#define VVM_safe_sqrt_AVX(v)               VVM_select_AVX(VVM_cmpgt_AVX(v              , VVM_m256Const(Epsilon)), _mm256_sqrt_ps(v)                     , _mm256_setzero_ps())
#define VVM_safe_log_AVX(v)                VVM_select_AVX(VVM_cmpgt_AVX(v              , _mm256_setzero_ps())   , log256_ps(v)		                    , _mm256_setzero_ps())
#define VVM_safe_pow_AVX(v0, v1)           VVM_select_AVX(VVM_cmpgt_AVX(v1             , VVM_m256Const(Epsilon)), VVM_pow_AVX(v0, v1)                   , _mm256_setzero_ps())
#define VVM_safe_rsq_AVX(v)                VVM_select_AVX(VVM_cmpgt_AVX(v              , VVM_m256Const(Epsilon)), _mm256_rsqrt_ps (v)                   , _mm256_setzero_ps())
#define VVM_random_AVX(v)                  _mm256_mul_ps(_mm256_sub_ps(_mm256_castsi256_ps(_mm256_or_si256(_mm256_srli_epi32(VVMXorwowStep_AVX(BatchState), 9), _mm256_set1_epi32(0x3F800000))), VVM_m256Const(One)), v)
#define VVM_randomi_AVX(v)                 _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_sub_ps(_mm256_castsi256_ps(_mm256_or_si256(_mm256_srli_epi32(VVMXorwowStep_AVX(BatchState), 9), _mm256_set1_epi32(0x3F800000))), VVM_m256Const(One)), *(__m256 *)&v))

//new merged instructions	
#define VVM_cmplt_select_AVX(v0, v1, v2, v3)    VVM_select_AVX(VVM_cmplt_AVX(v0, v1), v2, v3)
#define VVM_cmple_select_AVX(v0, v1, v2, v3)    VVM_select_AVX(VVM_cmple_AVX(v0, v1), v2, v3)
#define VVM_cmpeq_select_AVX(v0, v1, v2, v3)    VVM_select_AVX(VVM_cmpeq_AVX(v0, v1), v2, v3)
#define VVM_cmplti_select_AVX(v0, v1, v2, v3)   VVM_select_AVX(_mm256_castsi256_ps(VVM_cmplti_AVX(*(__m256i *)&v0, *(__m256i *)&v1)), v2, v3)
#define VVM_cmplei_select_AVX(v0, v1, v2, v3)   VVM_select_AVX(_mm256_castsi256_ps(VVM_cmplei_AVX(*(__m256i *)&v0, *(__m256i *)&v1)), v2, v3)
#define VVM_cmpeqi_select_AVX(v0, v1, v2, v3)   VVM_select_AVX(_mm256_castsi256_ps(VVM_cmpeqi_AVX(*(__m256i *)&v0, *(__m256i *)&v1)), v2, v3)
#define VVM_cmplt_logic_and_AVX(v0, v1, v2)     _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(VVM_cmplt_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmple_logic_and_AVX(v0, v1, v2)     _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(VVM_cmple_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpgt_logic_and_AVX(v0, v1, v2)     _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(VVM_cmpgt_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpge_logic_and_AVX(v0, v1, v2)     _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(VVM_cmpge_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpeq_logic_and_AVX(v0, v1, v2)     _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(VVM_cmpeq_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpne_logic_and_AVX(v0, v1, v2)     _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(VVM_cmpneq_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmplti_logic_and_AVX(v0, v1, v2)    _mm256_and_si256(VVM_cmplti_AVX(v0, v1), v2)
#define VVM_cmplei_logic_and_AVX(v0, v1, v2)    _mm256_and_si256(VVM_cmplei_AVX(v0, v1), v2)
#define VVM_cmpgti_logic_and_AVX(v0, v1, v2)    _mm256_and_si256(VVM_cmpgti_AVX(v0, v1), v2)
#define VVM_cmpgei_logic_and_AVX(v0, v1, v2)    _mm256_and_si256(VVM_cmpgei_AVX(v0, v1), v2)
#define VVM_cmpeqi_logic_and_AVX(v0, v1, v2)    _mm256_and_si256(VVM_cmpeqi_AVX(v0, v1), v2)
#define VVM_cmpnei_logic_and_AVX(v0, v1, v2)    _mm256_and_si256(VVM_cmpneqi_AVX(v0, v1), v2)
#define VVM_cmplt_logic_or_AVX(v0, v1, v2)      _mm256_castsi256_ps(_mm256_or_si256(_mm256_castps_si256(VVM_cmplt_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmple_logic_or_AVX(v0, v1, v2)      _mm256_castsi256_ps(_mm256_or_si256(_mm256_castps_si256(VVM_cmple_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpgt_logic_or_AVX(v0, v1, v2)      _mm256_castsi256_ps(_mm256_or_si256(_mm256_castps_si256(VVM_cmpgt_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpge_logic_or_AVX(v0, v1, v2)      _mm256_castsi256_ps(_mm256_or_si256(_mm256_castps_si256(VVM_cmpge_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpeq_logic_or_AVX(v0, v1, v2)      _mm256_castsi256_ps(_mm256_or_si256(_mm256_castps_si256(VVM_cmpeq_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmpne_logic_or_AVX(v0, v1, v2)      _mm256_castsi256_ps(_mm256_or_si256(_mm256_castps_si256(VVM_cmpneq_AVX(v0, v1)), *(__m256i *)&v2))
#define VVM_cmplti_logic_or_AVX(v0, v1, v2)     _mm256_or_si256(VVM_cmplti_AVX(v0, v1), v2)
#define VVM_cmplei_logic_or_AVX(v0, v1, v2)     _mm256_or_si256(VVM_cmplei_AVX(v0, v1), v2)
#define VVM_cmpgti_logic_or_AVX(v0, v1, v2)     _mm256_or_si256(VVM_cmpgti_AVX(v0, v1), v2)
#define VVM_cmpgei_logic_or_AVX(v0, v1, v2)     _mm256_or_si256(VVM_cmpgei_AVX(v0, v1), v2)
#define VVM_cmpeqi_logic_or_AVX(v0, v1, v2)     _mm256_or_si256(VVM_cmpeqi_AVX(v0, v1), v2)
#define VVM_cmpnei_logic_or_AVX(v0, v1, v2)     _mm256_or_si256(VVM_cmpneqi_AVX(v0, v1), v2)
#define VVM_mad_add_AVX(v0, v1, v2, v3)         _mm256_add_ps(_mm256_fmadd_ps(v0, v1, v2), v3)
#define VVM_mad_sub0_AVX(v0, v1, v2, v3)        _mm256_sub_ps(_mm256_fmadd_ps(v0, v1, v2), v3)
#define VVM_mad_sub1_AVX(v0, v1, v2, v3)        _mm256_sub_ps(v3, _mm256_fmadd_ps(v0, v1, v2))
#define VVM_mad_mul_AVX(v0, v1, v2, v3)         _mm256_mul_ps(_mm256_fmadd_ps(v0, v1, v2), v3)
#define VVM_mad_sqrt_AVX(v0, v1, v2)            _mm256_sqrt_ps(_mm256_fmadd_ps(v0, v1, v2))
#define VVM_mad_mad0_AVX(v0, v1, v2, v3, v4)    _mm256_fmadd_ps(_mm256_fmadd_ps(v0, v1, v2), v3, v4)
#define VVM_mad_mad1_AVX(v0, v1, v2, v3, v4)    _mm256_fmadd_ps(v3, v4, _mm256_fmadd_ps(v0, v1, v2))
#define VVM_mul_mad0_AVX(v0, v1, v2, v3)        _mm256_fmadd_ps(_mm256_mul_ps(v0, v1), v2, v3)
#define VVM_mul_mad1_AVX(v0, v1, v2, v3)        _mm256_fmadd_ps(v2, v3, _mm256_mul_ps(v0, v1))
#define VVM_mul_add_AVX(v0, v1, v2)             _mm256_add_ps(_mm256_mul_ps(v0, v1), v2)
#define VVM_mul_sub_AVX(v0, v1, v2)             _mm256_sub_ps(_mm256_mul_ps(v0, v1), v2)
#define VVM_mul_mul_AVX(v0, v1, v2)             _mm256_mul_ps(_mm256_mul_ps(v0, v1), v2)
#define VVM_mul_max_AVX(v0, v1, v2)             _mm256_max_ps(_mm256_mul_ps(v0, v1), v2)
#define VVM_add_mad1_AVX(v0, v1, v2, v3)        _mm256_fmadd_ps(v2, v3, _mm256_add_ps(v0, v1))
#define VVM_add_add_AVX(v0, v1, v2)             _mm256_add_ps(_mm256_add_ps(v0, v1), v2)
#define VVM_sub_cmplt1_AVX(v0, v1, v2)			VVM_cmplt_AVX(v2, _mm256_sub_ps(v0, v1))
#define VVM_sub_neg_AVX(v0, v1)                 VVM_neg_AVX(_mm256_sub_ps(v0, v1))
#define VVM_sub_mul_AVX(v0, v1, v2)				_mm256_mul_ps(_mm256_sub_ps(v0, v1), v2)
#define VVM_div_mad0_AVX(v0, v1, v2, v3)        _mm256_fmadd_ps(_mm256_div_ps(v0, v1), v2, v3)
#define VVM_div_f2i_AVX(v0, v1)                 _mm256_cvttps_epi32(_mm256_div_ps(_mm256_castsi256_ps(v0), _mm256_castsi256_ps(v1)))
#define VVM_div_mul_AVX(v0, v1, v2)				_mm256_mul_ps(_mm256_div_ps(v0, v1), v2)
#define VVM_muli_addi_AVX(v0, v1, v2)           _mm256_add_epi32(VectorIntMultiply_AVX(v1, v2), v2)
#define VVM_addi_bit_rshift_AVX(v0, v1, v2)     _mm256_srlv_epi32(_mm256_add_epi32(v1, v2), v2)
#define VVM_addi_muli_AVX(v0, v1, v2)           VectorIntMultiply_AVX(_mm256_add_epi32(v1, v2), v2)
#define VVM_i2f_div0_AVX(v0, v1)                _mm256_div_ps(_mm256_cvtepi32_ps(_mm256_castps_si256(v0)), v1)
#define VVM_i2f_div1_AVX(v0, v1)                _mm256_div_ps(v1, _mm256_cvtepi32_ps(_mm256_castps_si256(v0))) 
#define VVM_i2f_mul_AVX(v0, v1)                 _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_castps_si256(v0)), v1)
#define VVM_i2f_mad0_AVX(v0, v1, v2)            _mm256_fmadd_ps(_mm256_cvtepi32_ps(_mm256_castps_si256(v0)), v1, v2)
#define VVM_i2f_mad1_AVX(v0, v1, v2)            _mm256_fmadd_ps(v0, v1, _mm256_cvtepi32_ps(_mm256_castps_si256(v2)))
#define VVM_mul_sub0_AVX(v0, v1, v2)            _mm256_sub_ps(_mm256_mul_ps(v0, v1), v2)
#define VVM_mul_sub1_AVX(v0, v1, v2)            _mm256_sub_ps(v2, _mm256_mul_ps(v0, v1))
#define VVM_f2i_select1_AVX(mask, v0, v1)       _mm256_blendv_epi8(v1, _mm256_cvtps_epi32(_mm256_castsi256_ps(v0)), mask)
#define VVM_f2i_maxi_AVX(v0, v1)                _mm256_max_epi32(_mm256_cvtps_epi32(_mm256_castsi256_ps(v0)), v1)
#define VVM_f2i_addi_AVX(v0, v1)                _mm256_min_epi32(_mm256_cvtps_epi32(_mm256_castsi256_ps(v0)), v1)
#define VVM_fmod_add_AVX(v0, v1, v2)            _mm256_add_ps(VectorMod_AVX(v0, v1), v2)
#define VVM_bit_and_i2f_AVX(v0, v1)             _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_castps_si256(v0), _mm256_castps_si256(v1)))
#define VVM_bit_rshift_bit_and_AVX(v0, v1, v2)  _mm256_and_si256(VVMIntRShift_AVX(v0, v1), v2)
#define VVM_neg_cmplt_AVX(v0, v1)               VectorCompareLT_AVX(VectorNegate_AVX(v0), v1)
#define VVM_bit_or_muli_AVX(v0, v1, v2)         VectorIntMultiply_AVX(_mm256_or_si256(v0, v1), v2)
#define VVM_bit_lshift_bit_or_AVX(v0, v1, v2)   _mm256_or_si256(VVMIntLShift_AVX(v0, v1), v2)
#define VVM_random_add_AVX(v0, v1)              _mm256_add_ps(VVM_random_AVX(v0), v1)
#define VVM_max_f2i_AVX(v0, v1)                 _mm256_cvtps_epi32(VectorMax_AVX(_mm256_castsi256_ps(v0), _mm256_castsi256_ps(v1)))
#define VVM_select_mul_AVX(v0, v1, v2, v3)      _mm256_mul_ps(VectorSelect_AVX(v0, v1, v2), v3)
#define VVM_select_add_AVX(v0, v1, v2, v3)      _mm256_add_ps(VectorSelect_AVX(v0, v1, v2), v3)


//instructions called by the VM, named to match UE's style
#define VectorAdd_AVX(v0, v1)                   _mm256_add_ps(v0, v1)
#define VectorSubtract_AVX(v0, v1)              _mm256_sub_ps(v0, v1)
#define VectorMultiply_AVX(v0, v1)              _mm256_mul_ps(v0, v1)
#define VectorMultiplyAdd_AVX(v0, v1, v2)       _mm256_fmadd_ps(v0, v1, v2)
#define VectorAbs_AVX(v)                        VVM_abs_AVX(v)
#define VectorNegate_AVX(v)                     VVM_neg_AVX(v)
#define VectorExp_AVX(v)                        exp256_ps(v)
#define VectorExp2_AVX(v)                       _mm256_exp2_ps(v)
#define VectorLog2_AVX(v)                       log256_ps(v)
#define VectorSin_AVX(v)                        sin256_ps(v)
#define VectorCos_AVX(v)                        cos256_ps(v)
#define VectorLerp_AVX(v0, v1, v2)              _mm256_fmadd_ps(v1, v2, _mm256_mul_ps(v0, _mm256_sub_ps(VVM_m256Const(One), v2)))
#define VectorCeil_AVX(v)                       _mm256_ceil_ps(v)
#define VectorFloor_AVX(v)                      _mm256_floor_ps(v)
#define VectorTruncate_AVX(v)                   _mm256_round_ps(v, _MM_FROUND_TO_NEG_INF |_MM_FROUND_NO_EXC)
#define VectorFractional_AVX(v)                 _mm256_sub_ps(v, VectorTruncate_AVX(v))
#define VectorClamp_AVX(v0, v1, v2)             _mm256_min_ps(_mm256_max_ps(v0, v1), v2)
#define VectorMin_AVX(v0, v1)                   _mm256_min_ps(v0, v1)
#define VectorMax_AVX(v0, v1)                   _mm256_max_ps(v0, v1)
#define VectorRound_AVX(v)                      _mm256_round_ps(v, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC)
#define VectorSign_AVX(v)                       _mm256_blendv_ps(VVM_m256Const(NegativeOne), VVM_m256Const(One), _mm256_cmp_ps(v, _mm256_setzero_ps(), _CMP_GT_OS))
#define VectorSelect_AVX(m, v0, v1)				VVM_select_AVX(m, v0, v1)
#define VectorCompareLT_AVX(v0, v1)             VVM_cmplt_AVX(v0, v1)
#define VectorCompareLE_AVX(v0, v1)             VVM_cmple_AVX(v0, v1)
#define VectorCompareGT_AVX(v0, v1)             VVM_cmpgt_AVX(v0, v1)
#define VectorCompareGE_AVX(v0, v1)             VVM_cmpge_AVX(v0, v1)
#define VectorCompareEQ_AVX(v0, v1)             VVM_cmpeq_AVX(v0, v1)
#define VectorCompareNE_AVX(v0, v1)             VVM_cmpneq_AVX(v0, v1)
#define VectorIntAdd_AVX(v0, v1)                _mm256_add_epi32(v0, v1)
#define VectorIntSubtract_AVX(v0, v1)           _mm256_sub_epi32(v0, v1)
#define VectorIntMultiply_AVX(v0, v1)           _mm256_unpacklo_epi32(_mm256_shuffle_epi32(_mm256_mul_epu32(v0, v1), _MM_SHUFFLE(0, 0, 2, 0)), _mm256_shuffle_epi32(_mm256_mul_epu32(_mm256_srli_si256(v0, 4), _mm256_srli_si256(v1, 4)), _MM_SHUFFLE(0, 0, 2, 0)))
#define VectorIntClamp_AVX(v0, v1, v2)          _mm256_min_epi32(_mm256_max_epi32(v0, v1), v2)
#define VectorIntMin_AVX(v0, v1)                _mm256_min_epi32(v0, v1)
#define VectorIntMax_AVX(v0, v1)                _mm256_max_epi32(v0, v1)
#define VectorIntAbs_AVX(v)                     _mm256_abs_epi32(v)
#define VectorIntNegate_AVX(v)                  _mm256_sub_epi32(_mm256_setzero_si256(), v)
#define VectorIntSign_AVX(v)                    _mm256_blendv_epi8(VVM_m256iConst(NegativeOne), VVM_m256iConst(One), VVM_cmpgei_AVX(v, _mm256_setzero_si256()))
#define VectorIntCompareLT_AVX(v0, v1)          VVM_cmplti_AVX(v0, v1)
#define VectorIntCompareLE_AVX(v0, v1)          VVM_cmplei_AVX(v0, v1)
#define VectorIntCompareGT_AVX(v0, v1)          VVM_cmpgti_AVX(v0, v1)
#define VectorIntCompareGE_AVX(v0, v1)          VVM_cmpgei_AVX(v0, v1)
#define VectorIntCompareEQ_AVX(v0, v1)          VVM_cmpeqi_AVX(v0, v1)
#define VectorIntCompareNEQ_AVX(v0, v1)         VVM_cmpneqi_AVX(v0, v1)
#define VectorIntAnd_AVX(v0, v1)                _mm256_and_si256(v0, v1)
#define VectorIntOr_AVX(v0, v1)                 _mm256_or_si256(v0, v1)
#define VectorIntXor_AVX(v0, v1)                _mm256_xor_si256(v0, v1)
#define VectorIntNot_AVX(v)                     _mm256_xor_si256(v, VVM_m256iConst(FMask))
#define VVMIntLShift_AVX(v0, v1)                _mm256_sllv_epi32(v0, v1)
#define VVMIntRShift_AVX(v0, v1)                _mm256_srlv_epi32(v0, v1)
#define VVMf2i_AVX(v)                           _mm256_cvttps_epi32(_mm256_castsi256_ps(v))
#define VVMi2f_AVX(v)                           _mm256_cvtepi32_ps(_mm256_castps_si256(v))
#define VectorSinCos_AVX(v, OutSin, OutCos)     sincos256_ps(v, OutSin, OutCos)

static inline __m256 VectorTan_AVX(const __m256 &v)
{
	VVMXXMYMMUnion U;
	U.ymm = v;
	U.xmm[0] = VectorTan(U.xmm[0]);
	U.xmm[1] = VectorTan(U.xmm[1]);
	return U.ymm;
}

static inline __m256 VectorATan_AVX(const __m256 &v)
{
	VVMXXMYMMUnion U;
	U.ymm = v;
	U.xmm[0] = VectorATan(U.xmm[0]);
	U.xmm[1] = VectorATan(U.xmm[1]);
	return U.ymm;
}

static inline __m256 VectorATan2_AVX(const __m256 &v0, const __m256 &v1)
{
	VVMXXMYMMUnion U0, U1;
	U0.ymm = v0;
	U1.ymm = v1;
	U0.xmm[0] = VectorATan2(U0.xmm[0], U1.xmm[0]);
	U0.xmm[1] = VectorATan2(U0.xmm[1], U1.xmm[1]);
	return U0.ymm;
}

static inline __m256 VectorASin_AVX(const __m256 &v)
{
	VVMXXMYMMUnion U;
	U.ymm = v;
	U.xmm[0] = VectorASin(U.xmm[0]);
	U.xmm[1] = VectorASin(U.xmm[1]);
	return U.ymm;
}

static inline __m256 VectorMod_AVX(const __m256 &v0, const __m256 &v1)
{
	VVMXXMYMMUnion U0, U1;
	U0.ymm = v0;
	U1.ymm = v1;
	U0.xmm[0] = VectorMod(U0.xmm[0], U1.xmm[0]);
	U0.xmm[1] = VectorMod(U0.xmm[1], U1.xmm[1]);
	return U0.ymm;
}

struct FVVM_VU8
{
	union
	{
		__m256i i8;
		uint32 o8[8];
	}
};

static inline __m256i VVMIntDiv_AVX(const __m256i &X, const __m256i &Y)
{
	uint32 *v0_8 = (uint32 *)&X;
	uint32 *v1_8 = (uint32 *)&Y;

    FVVM_VU8 res;

	res.o8[0] = v1_8[0] == 0 ? 0 : (v0_8[0] / v1_8[0]);
    res.o8[1] = v1_8[1] == 0 ? 0 : (v0_8[1] / v1_8[1]);
    res.o8[2] = v1_8[2] == 0 ? 0 : (v0_8[2] / v1_8[2]);
    res.o8[3] = v1_8[3] == 0 ? 0 : (v0_8[3] / v1_8[3]);
	res.o8[4] = v1_8[4] == 0 ? 0 : (v0_8[4] / v1_8[4]);
    res.o8[5] = v1_8[5] == 0 ? 0 : (v0_8[5] / v1_8[5]);
    res.o8[6] = v1_8[6] == 0 ? 0 : (v0_8[6] / v1_8[6]);
    res.o8[7] = v1_8[7] == 0 ? 0 : (v0_8[7] / v1_8[7]);
	
	return res.i8;
}


#endif //VECTORVM_SUPPORTS_AVX

struct FVVM_VU4
{
	union
	{
		VectorRegister4i i4;
		uint32 o4[4];
	};
};

inline VectorRegister4i VVMIntRShift(VectorRegister4i v0, VectorRegister4i v1)
{
#if PLATFORM_ENABLE_VECTORINTRINSICS_NEON
	VectorRegister4i res = vshlq_u32(v0, vmulq_s32(v1, vdupq_n_s32(-1)));
	return res;
#else
	uint32 *v0_4 = (uint32 *)&v0;
	uint32 *v1_4 = (uint32 *)&v1;

	FVVM_VU4 res;

	res.o4[0] = v0_4[0] >> v1_4[0];
    res.o4[1] = v0_4[1] >> v1_4[1];
    res.o4[2] = v0_4[2] >> v1_4[2];
    res.o4[3] = v0_4[3] >> v1_4[3];
	
	return res.i4;
#endif
}

inline VectorRegister4i VVMIntLShift(VectorRegister4i v0, VectorRegister4i v1)
{
#if PLATFORM_ENABLE_VECTORINTRINSICS_NEON
	VectorRegister4i res = vshlq_u32(v0, v1);
	return res;
#else
	uint32 *v0_4 = (uint32 *)&v0;
	uint32 *v1_4 = (uint32 *)&v1;
    
	FVVM_VU4 res;

	res.o4[0] = v0_4[0] << v1_4[0];
    res.o4[1] = v0_4[1] << v1_4[1];
    res.o4[2] = v0_4[2] << v1_4[2];
    res.o4[3] = v0_4[3] << v1_4[3];
	
	return res.i4;
#endif
}

inline VectorRegister4i VVMIntDiv(VectorRegister4i v0, VectorRegister4i v1)
{
	uint32 *v0_4 = (uint32 *)&v0;
	uint32 *v1_4 = (uint32 *)&v1;

    FVVM_VU4 res;

	res.o4[0] = v1_4[0] == 0 ? 0 : (v0_4[0] / v1_4[0]);
    res.o4[1] = v1_4[1] == 0 ? 0 : (v0_4[1] / v1_4[1]);
    res.o4[2] = v1_4[2] == 0 ? 0 : (v0_4[2] / v1_4[2]);
    res.o4[3] = v1_4[3] == 0 ? 0 : (v0_4[3] / v1_4[3]);
	
	return res.i4;
}

inline VectorRegister4i VVMf2i(VectorRegister4i v0)
{
	FVecReg u;
	u.i = v0;
	VectorRegister4i res = VectorFloatToInt(u.v);
	return res;
}

inline VectorRegister4f VVMi2f(VectorRegister4f v0)
{
	FVecReg u;
	u.v = v0;
	VectorRegister4f res = VectorIntToFloat(u.i);
	return res;
}
#define VVMDebugBreakIf(expr)	if ((expr)) { PLATFORM_BREAK(); }

FORCEINLINE uint16 float_to_half_fast3_rtne(uint32 f_in)
{
	uint16 h_out;
	FPlatformMath::StoreHalf(&h_out, *reinterpret_cast<float*>(&f_in));

	return h_out;
}

FORCEINLINE float half_to_float(uint16 h_)
{
	return FPlatformMath::LoadHalf(&h_);
}

FORCEINLINE void VVMMemCpy(void *dst, void *src, size_t bytes)
{
	unsigned char *RESTRICT d      = (unsigned char *)dst;
	unsigned char *RESTRICT s      = (unsigned char *)src;
	unsigned char *RESTRICT s_end  = s + bytes;
	ptrdiff_t ofs_to_dest = d - s;
	if (bytes < 16)
	{
		if (bytes)
		{
			do
			{
				s[ofs_to_dest] = s[0];
				++s;
			} while (s < s_end);
		}
	}
	else
	{
		// do one unaligned to get us aligned for the stream out below
		VectorRegister4i i0 = VectorIntLoad(s);
		VectorIntStore(i0, d);
		s += 16 + 16 - ((size_t)d & 15); // S is 16 bytes ahead 
		while (s <= s_end)
		{
			i0 = VectorIntLoad(s - 16);
			VectorIntStoreAligned(i0, s - 16 + ofs_to_dest);
			s += 16;
		}
		// do one unaligned to finish the copy
		i0 = VectorIntLoad(s_end - 16);
		VectorIntStore(i0, s_end + ofs_to_dest - 16);
	}      
}

FORCEINLINE void VVMMemSet32(void *dst, uint32 val, size_t num_vals)
{
	if (num_vals <= 4)
	{
		uint32 *dst32 = (uint32 *)dst;
		switch (num_vals)
		{
			case 4:		VectorIntStore(VectorIntSet1(val), dst); break;
			case 3:		dst32[2] = val;  //intentional fallthrough
			case 2:		dst32[1] = val;  //intentional fallthrough
			case 1:		dst32[0] = val;  //intentional fallthrough
			case 0:		break;
		}
	}
	else
	{
		VectorRegister4i v4 = VectorIntSet1(val);
		char *RESTRICT ptr = (char *)dst;
		char *RESTRICT end_ptr = ptr + num_vals * 4 - 16;
		while (ptr < end_ptr) {
			VectorIntStore(v4, ptr);
			ptr += 16;
		}
		VectorIntStore(v4, end_ptr);
	}
}
#pragma warning(push)
#pragma warning(disable: 4426)
#include "VectorVMExperimental_Optimizer.inl" 
#include "VectorVMExperimental_Memory.inl"
#pragma warning(pop)

#if VECTORVM_SUPPORTS_AVX
static void SetRegIncTableFor8WideFrom4Wide(FVectorVMExecContext *ExecCtx, FVectorVMBatchState *BatchState)
{
	uint8 *TempRegIncTable  = BatchState->RegIncTable;
	uint8 *InputRegIncTable = BatchState->RegIncTable + ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers;
	for (uint32 i = 0; i < ExecCtx->VVMState->NumTempRegisters; ++i)
	{
		TempRegIncTable[i] <<= 1;
	}
	for (uint32 i = 0; i < ExecCtx->VVMState->NumInputBuffers; ++i)
	{
		InputRegIncTable[i] <<= 1;
	}
}

static void SetRegIncTableFor4WideFrom8Wide(FVectorVMExecContext *ExecCtx, FVectorVMBatchState *BatchState)
{
	uint8 *TempRegIncTable  = BatchState->RegIncTable;
	uint8 *InputRegIncTable = BatchState->RegIncTable + ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers;
	for (uint32 i = 0; i < ExecCtx->VVMState->NumTempRegisters; ++i)
	{
		TempRegIncTable[i] >>= 1;
	}
	for (uint32 i = 0; i < ExecCtx->VVMState->NumInputBuffers; ++i)
	{
		InputRegIncTable[i] >>= 1;
	}
}
#endif //VECTORVM_SUPPORTS_AVX
static uint8 *SetupBatchStatePtrs(FVectorVMExecContext *ExecCtx, FVectorVMBatchState *BatchState)
{
	uint8 *BatchDataPtr        = (uint8 *) VVM_ALIGN_64((size_t)BatchState + sizeof(FVectorVMBatchState));
	size_t NumPtrRegsInTable   = ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers + ExecCtx->VVMState->NumInputBuffers * 2;

	BatchState->RegisterData                                = (FVecReg *)BatchDataPtr;                  BatchDataPtr += ExecCtx->Internal.PerBatchRegisterDataBytesRequired;
	BatchState->RegPtrTable                                 = (uint8 **)BatchDataPtr;                   BatchDataPtr += NumPtrRegsInTable * sizeof(uint32 *);
	BatchState->RegIncTable                                 = (uint8 *)BatchDataPtr;                    BatchDataPtr += NumPtrRegsInTable;
	BatchState->ChunkLocalData.StartingOutputIdxPerDataSet	= (uint32 *) BatchDataPtr;                  BatchDataPtr += ExecCtx->VVMState->ChunkLocalDataOutputIdxNumBytes;
	BatchState->ChunkLocalData.NumOutputPerDataSet          = (uint32 *) BatchDataPtr;                  BatchDataPtr += ExecCtx->VVMState->ChunkLocalNumOutputNumBytes;

	{ //deal with the external function register decoding buffer
		size_t PtrBeforeExtFnDecodeReg = (size_t)BatchDataPtr;
		BatchState->ChunkLocalData.ExtFnDecodedReg.RegData       = (uint32 **)BatchDataPtr;  BatchDataPtr += sizeof(FVecReg *) * ExecCtx->VVMState->MaxExtFnRegisters;
		BatchState->ChunkLocalData.ExtFnDecodedReg.RegInc        = (uint8 *)BatchDataPtr;    BatchDataPtr += sizeof(uint8)     * ExecCtx->VVMState->MaxExtFnRegisters;
		BatchDataPtr = (uint8 *)VVM_PTR_ALIGN(BatchDataPtr);
		BatchState->ChunkLocalData.ExtFnDecodedReg.DummyRegs     = (FVecReg *)BatchDataPtr;  BatchDataPtr += sizeof(FVecReg)   * ExecCtx->VVMState->NumDummyRegsRequired;
		size_t PtrAfterExtFnDecodeReg = (size_t)BatchDataPtr;
	}

	{ //build the register pointer table which contains pointers (in order) to:
		uint32 **TempRegPtr     = (uint32 **)BatchState->RegPtrTable;                  //1. Temp Registers
		uint32 **ConstBuffPtr   = TempRegPtr   + ExecCtx->VVMState->NumTempRegisters;  //2. Constant Buffers
		uint32 **InputPtr       = ConstBuffPtr + ExecCtx->VVMState->NumConstBuffers;   //3. Input Registers (the 2x is because 
		uint32 NumLoops         = ExecCtx->Internal.MaxInstancesPerChunk >> 2;

		static_assert(sizeof(FVecReg) == 16);
		FMemory::Memset(BatchState->RegIncTable, sizeof(FVecReg), NumPtrRegsInTable);

		//temp regsiters
		for (uint32 i = 0; i < ExecCtx->VVMState->NumTempRegisters; ++i) {
			TempRegPtr[i] = (uint32 *)(BatchState->RegisterData + i * NumLoops);
		}
		//constant buffers
		for (uint32 i = 0; i < ExecCtx->VVMState->NumConstBuffers; ++i) {
			ConstBuffPtr[i] = (uint32 *)(ExecCtx->VVMState->ConstantBuffers + i);
			BatchState->RegIncTable[ExecCtx->VVMState->NumTempRegisters + i] = 0;
		}
		//inputs
		int NoAdvCounter = 0;
		for (uint32 i = 0; i < ExecCtx->VVMState->NumInputBuffers; ++i)
		{
			uint8 DataSetIdx      = ExecCtx->VVMState->InputMapCacheIdx[i];
			uint16 InputMapSrcIdx = ExecCtx->VVMState->InputMapCacheSrc[i];

			uint32 **DataSetInputBuffers = (uint32 **)ExecCtx->DataSets[DataSetIdx].InputRegisters.GetData();
			int32 InstanceOffset         = ExecCtx->DataSets[DataSetIdx].InstanceOffset;

			if (InputMapSrcIdx & 0x8000) //this is a noadvance input.  It points to data after the constant buffers
			{
				InputPtr[i                                     ] = (uint32 *)(ExecCtx->VVMState->ConstantBuffers + ExecCtx->VVMState->NumConstBuffers + NoAdvCounter);
				InputPtr[i + ExecCtx->VVMState->NumInputBuffers] = (uint32 *)(ExecCtx->VVMState->ConstantBuffers + ExecCtx->VVMState->NumConstBuffers + NoAdvCounter);
				++NoAdvCounter;
				
				BatchState->RegIncTable[ExecCtx->VVMState->NumTempRegisters + ExecCtx->VVMState->NumConstBuffers + i] = 0; //no advance inputs... don't advance obviously

				if (InputMapSrcIdx & 0x4000) //half input (@TODO: has never been tested)
				{
					uint16 *Ptr = (uint16 *)DataSetInputBuffers[InputMapSrcIdx & 0x3FFF] + InstanceOffset;
					float val = half_to_float(*Ptr);
					VectorRegister4f InputVal = VectorSet1(val);
					VectorStore(InputVal, (float *)InputPtr[i]);
				}
				else
				{
					uint32 *Ptr = (uint32 *)DataSetInputBuffers[InputMapSrcIdx & 0x3FFF] + InstanceOffset;
					VectorRegister4i InputVal4 = VectorIntSet1(*Ptr);
					VectorIntStore(InputVal4, InputPtr[i]);
				}
			}
			else //regular input, point directly to the input buffer
			{
				InputPtr[i                                     ] = DataSetInputBuffers[InputMapSrcIdx] + InstanceOffset;
				InputPtr[i + ExecCtx->VVMState->NumInputBuffers] = InputPtr[i]; //second copy of the "base" ptr so each chunk can start them at their correct starting offset
			}
		}
	}

	BatchState->ChunkLocalData.RandCounters = nullptr; //these get malloc'd separately if they're ever used... which they very rarely are

	if ((size_t)(BatchDataPtr - (uint8 *)BatchState) <= ExecCtx->Internal.NumBytesRequiredPerBatch)
	{
		return BatchDataPtr;
	}
	else
	{
		return nullptr;
	}
}

inline bool AssignInstancesToBatch(FVectorVMExecContext *ExecCtx, FVectorVMBatchState *BatchState)
{
	int SanityCount = 0;
	do
	{
		int OldNumAssignedInstances = ExecCtx->Internal.NumInstancesAssignedToBatches;
		int MaxInstancesPerBatch    = ExecCtx->Internal.MaxInstancesPerChunk * ExecCtx->Internal.MaxChunksPerBatch;
		int NumAssignedInstances    = FPlatformAtomics::InterlockedCompareExchange(&ExecCtx->Internal.NumInstancesAssignedToBatches, OldNumAssignedInstances + MaxInstancesPerBatch, OldNumAssignedInstances);
		if (NumAssignedInstances == OldNumAssignedInstances)
		{
			//we allow "overflow" on the thread-local BatchState to keep access to the contended variable: VVMState->NumInstancesAssignedToBatches 
			//down to a single atomic.  Letting BatchState->NumInstances go < 0 is fine because this will return false and Exec() will not run
			//for this BatchState.
			BatchState->StartInstance = OldNumAssignedInstances;
			BatchState->NumInstances  = MaxInstancesPerBatch;
			if (BatchState->StartInstance + BatchState->NumInstances > ExecCtx->NumInstances) {
				BatchState->NumInstances = ExecCtx->NumInstances - BatchState->StartInstance;
			}
			if (BatchState->NumInstances <= 0) {
				return false; //some other thread interrupted and finished the rest of the instances, we're done.
			}
			return true;
		}
	} while (SanityCount++ < (1 << 30));
	VVMDebugBreakIf(SanityCount > (1 << 30) - 1);
	return false;
}

VECTORVM_API void FreeVectorVMState(FVectorVMState *VVMState)
{
	if (VVMState != nullptr)
	{
		FMemory::Free(VVMState);
	}
}

static void SetupRandStateForBatch(FVectorVMBatchState *BatchState)
{
	uint64 pcg_state = FPlatformTime::Cycles64();
	uint64 pcg_inc   = (((uint64)BatchState << 32) ^ 0XCAFEF00DD15EA5E5U) | 1;
	pcg_state ^= (FPlatformTime::Cycles64() << 32ULL);
	//use psuedo-pcg to setup a state for xorwow
	for (int i = 0; i < 5; ++i) //loop for xorwow internal state
	{
#		if VECTORVM_SUPPORTS_AVX
		MS_ALIGN(32) uint32 Values[8] GCC_ALIGN(32);
		for (int j = 0; j < 8; ++j)
		{
			uint64 old_state   = pcg_state;
			pcg_state          = old_state * 6364136223846793005ULL + pcg_inc;
			uint32 xor_shifted = (uint32)(((old_state >> 18U) ^ old_state) >> 27U);
			uint32 rot         = old_state >> 59U;
			Values[j]          = (xor_shifted >> rot) | (xor_shifted << ((0U - rot) & 31));
		}
		_mm256_storeu_si256(BatchState->RandState.AVX.State + i, *(__m256i *)Values);
#		else
		MS_ALIGN(16) uint32 Values[4] GCC_ALIGN(16);
		for (int j = 0; j < 4; ++j)
		{
			uint64 old_state   = pcg_state;
			pcg_state          = old_state * 6364136223846793005ULL + pcg_inc;
			uint32 xor_shifted = (uint32)(((old_state >> 18U) ^ old_state) >> 27U);
			uint32 rot         = old_state >> 59U;
			Values[j]          = (xor_shifted >> rot) | (xor_shifted << ((0U - rot) & 31));
		}
		VectorIntStore(*(VectorRegister4i *)Values, BatchState->RandState.State + i);
#		endif
	}
	BatchState->RandState.Counters = MakeVectorRegisterInt64(pcg_inc, pcg_state);
	BatchState->RandStream.GenerateNewSeed();
}

static VectorRegister4i VVMXorwowStep(FVectorVMBatchState *BatchState)
{
	VectorRegister4i t = BatchState->RandState.State[4];
	VectorRegister4i s = BatchState->RandState.State[0];
	BatchState->RandState.State[4] = BatchState->RandState.State[3];
	BatchState->RandState.State[3] = BatchState->RandState.State[2];
	BatchState->RandState.State[2] = BatchState->RandState.State[1];
	BatchState->RandState.State[1] = s;
	t = VectorIntXor(t, VectorShiftRightImmLogical(t, 2));
	t = VectorIntXor(t, VectorShiftLeftImm(t, 1));
	t = VectorIntXor(t, VectorIntXor(s, VectorIntXor(s, VectorShiftLeftImm(s, 4))));
	BatchState->RandState.State[0] = t;
	BatchState->RandState.Counters = VectorIntAdd(BatchState->RandState.Counters, VectorIntSet1(362437));
	VectorRegister4i Result = VectorIntAdd(t, VectorIntLoad(&BatchState->RandState.Counters));
	return Result;
}
#if VECTORVM_SUPPORTS_AVX
static __m256i VVMXorwowStep_AVX(FVectorVMBatchState *BatchState)
{
	__m256i t = BatchState->RandState.AVX.State[4];
	__m256i s = BatchState->RandState.AVX.State[0];
	BatchState->RandState.AVX.State[4] = BatchState->RandState.AVX.State[3];
	BatchState->RandState.AVX.State[3] = BatchState->RandState.AVX.State[2];
	BatchState->RandState.AVX.State[2] = BatchState->RandState.AVX.State[1];
	BatchState->RandState.AVX.State[1] = s;
	t = _mm256_xor_si256(t, _mm256_srli_epi32(t, 2));
	t = _mm256_xor_si256(t, _mm256_slli_epi32(t, 1));
	t = _mm256_xor_si256(t, _mm256_xor_si256(s, _mm256_xor_si256(s, _mm256_slli_epi32(s, 4))));
	BatchState->RandState.AVX.State[0] = t;
	BatchState->RandState.AVX.Counters = _mm256_add_epi32(BatchState->RandState.AVX.Counters, _mm256_set1_epi32(362437));
	__m256i Result = _mm256_add_epi32(t, _mm256_loadu_si256(&BatchState->RandState.AVX.Counters));
	return Result;
}
#endif

VECTORVM_API FVectorVMState *AllocVectorVMState(FVectorVMOptimizeContext *OptimizeCtx) {
	if (OptimizeCtx == nullptr || OptimizeCtx->Error.Line != 0) {
		return nullptr;
	}
	//compute the number of overhead bytes for this VVM State
	uint32 ConstBufferOffset         = VVM_ALIGN_32(sizeof(FVectorVMState));

	size_t ConstantBufferNumBytes    = VVM_PTR_ALIGN(VVM_REG_SIZE                     * (OptimizeCtx->NumConstsRemapped + OptimizeCtx->NumNoAdvanceInputs)  );
	size_t ExtFnTableNumBytes        = VVM_PTR_ALIGN(sizeof(FVectorVMExtFunctionData) * (OptimizeCtx->MaxExtFnUsed + 1)                                     );
	size_t OutputPerDataSetNumBytes  = VVM_PTR_ALIGN(sizeof(volatile int32)           * OptimizeCtx->NumOutputDataSets                                      );
	size_t ConstMapCacheNumBytes     = VVM_PTR_ALIGN((sizeof(uint8) + sizeof(uint16)) * OptimizeCtx->NumConstsRemapped                                      );
	size_t InputMapCacheNumBytes     = VVM_PTR_ALIGN((sizeof(uint8) + sizeof(uint16)) * OptimizeCtx->NumInputsRemapped                                      );
	size_t VVMStateTotalNumBytes     = ConstBufferOffset + ConstantBufferNumBytes + ExtFnTableNumBytes + OutputPerDataSetNumBytes + ConstMapCacheNumBytes + InputMapCacheNumBytes;

	uint8 *StatePtr = (uint8 *)FMemory::Malloc(VVMStateTotalNumBytes, 16);
	if (StatePtr == nullptr)
	{
		return nullptr;
	}

	FVectorVMState *VVMState = (FVectorVMState *)StatePtr;
	
	{ //setup the pointers that are allocated in conjunction with the VVMState
		uint32 ExtFnTableOffset       = (uint32)(ConstBufferOffset      + ConstantBufferNumBytes);
		uint32 OutputPerDataSetOffset = (uint32)(ExtFnTableOffset       + ExtFnTableNumBytes);
		uint32 ConstMapCacheOffset    = (uint32)(OutputPerDataSetOffset + OutputPerDataSetNumBytes);
		uint32 InputMapCacheOffset    = (uint32)(ConstMapCacheOffset    + ConstMapCacheNumBytes);

		VVMState->ConstantBuffers           = (FVecReg *)                   (StatePtr + ConstBufferOffset     );
		VVMState->ExtFunctionTable          = (FVectorVMExtFunctionData *)  (StatePtr + ExtFnTableOffset      );
		VVMState->NumOutputPerDataSet       = (volatile int32 *)            (StatePtr + OutputPerDataSetOffset);
		VVMState->ConstMapCacheIdx          = (uint8 *)                     (StatePtr + ConstMapCacheOffset   );
		VVMState->InputMapCacheIdx          = (uint8 *)                     (StatePtr + InputMapCacheOffset   );
		VVMState->ConstMapCacheSrc          = (uint16 *)                    (VVMState->ConstMapCacheIdx + OptimizeCtx->NumConstsRemapped);
		VVMState->InputMapCacheSrc          = (uint16 *)                    (VVMState->InputMapCacheIdx + OptimizeCtx->NumInputsRemapped);

		check((size_t)((uint8 *)VVMState->InputMapCacheSrc - StatePtr) + sizeof(uint16) * OptimizeCtx->NumInputsRemapped <= VVMStateTotalNumBytes);

		VVMState->NumInstancesExecCached    = 0;
		for (uint32 i = 0; i < OptimizeCtx->NumExtFns; ++i)
		{
			VVMState->ExtFunctionTable[i].Function   = nullptr;
			VVMState->ExtFunctionTable[i].NumInputs  = OptimizeCtx->ExtFnTable[i].NumInputs;
			VVMState->ExtFunctionTable[i].NumOutputs = OptimizeCtx->ExtFnTable[i].NumOutputs;
		}
	}

	{ //setup the pointers from the optimize context
		VVMState->ConstRemapTable      = OptimizeCtx->ConstRemap[1];
		VVMState->InputRemapTable      = OptimizeCtx->InputRemapTable;
		VVMState->InputDataSetOffsets  = OptimizeCtx->InputDataSetOffsets;

		VVMState->NumTempRegisters     = OptimizeCtx->NumTempRegisters;
		VVMState->NumConstBuffers      = OptimizeCtx->NumConstsRemapped;
		VVMState->NumInputBuffers      = OptimizeCtx->NumInputsRemapped;
		VVMState->Bytecode             = OptimizeCtx->OutputBytecode;
		VVMState->NumBytecodeBytes     = OptimizeCtx->NumBytecodeBytes;
		VVMState->NumInputDataSets     = OptimizeCtx->NumInputDataSets;
		VVMState->NumOutputDataSets    = OptimizeCtx->NumOutputDataSets;
		VVMState->NumDummyRegsRequired = OptimizeCtx->NumDummyRegsReq;
		VVMState->Flags                = OptimizeCtx->Flags & ~VVMFlag_DataMapCacheSetup;

		VVMState->NumExtFunctions      = OptimizeCtx->NumExtFns;
		VVMState->MaxExtFnRegisters    = OptimizeCtx->MaxExtFnRegisters;
	}

#	if VECTORVM_SUPPORTS_AVX
	{ //@TODO check CPUID stuff the Unreal way here
		VVMState->Flags |= VVMFlag_SupportsAVX;
	}
#	endif

	{ //compute fixed batch size
		const size_t NumPtrRegsInTable = VVMState->NumTempRegisters + 
		                                 VVMState->NumConstBuffers +
		                                 VVMState->NumInputBuffers * 2;

		const size_t ChunkLocalDataOutputIdxNumBytes     = sizeof(uint32) * VVMState->NumOutputDataSets;
		const size_t ChunkLocalNumOutputNumBytes         = sizeof(uint32) * VVMState->NumOutputDataSets;
		const size_t ChunkLocalNumExtFnDecodeRegNumBytes = (sizeof(FVecReg *) + sizeof(uint8)) * VVMState->MaxExtFnRegisters + VVM_REG_SIZE * VVMState->NumDummyRegsRequired;
		const size_t RegPtrTableNumBytes                 = (sizeof(uint32 *) + sizeof(uint8)) * NumPtrRegsInTable;
		const size_t BatchOverheadSize                   = VVM_ALIGN_64(sizeof(FVectorVMBatchState))              +
			                                                            ChunkLocalDataOutputIdxNumBytes           +
		                                                                ChunkLocalNumOutputNumBytes               +
			                                                            ChunkLocalNumExtFnDecodeRegNumBytes       +
			                                                            RegPtrTableNumBytes                       +
			                                                            VVM_CHUNK_FIXED_OVERHEAD_SIZE;

		VVMState->BatchOverheadSize                   = (uint32)BatchOverheadSize;
		VVMState->ChunkLocalDataOutputIdxNumBytes     = (uint32)ChunkLocalDataOutputIdxNumBytes;
		VVMState->ChunkLocalNumOutputNumBytes         = (uint32)ChunkLocalNumOutputNumBytes;
	}

	return VVMState;
}

#include "VVMExec_SingleLoop.inl"
#include "VVMExec_MultipleLoops.inl"
#if VECTORVM_SUPPORTS_AVX
#include "VVMExec_MultipleLoopsAVX.inl"
#endif

static void ExecVVMBatch(FVectorVMExecContext *ExecCtx, int ExecIdx, FVectorVMSerializeState *SerializeState, FVectorVMSerializeState *CmpSerializeState) {
	if (ExecCtx->VVMState->Bytecode == nullptr) {
		return;
	}
	VVMRAIIPageHandle PageHandle;
	FVectorVMBatchState *BatchState = (FVectorVMBatchState *)VVMAllocBatch(ExecCtx->VVMState->BatchOverheadSize + ExecCtx->Internal.PerBatchRegisterDataBytesRequired, &PageHandle);

	bool RegIncSetForAVX = false;
	if (SetupBatchStatePtrs(ExecCtx, BatchState) == nullptr)
	{
		return;
	}
	if (ExecCtx->VVMState->Flags & VVMFlag_HasRandInstruction)
	{
		SetupRandStateForBatch(BatchState);
	}

	//Loop until all available batches have been completed... BatchCounter will likely be very small, and if AssignInstancesToBatch return null
	//then we return from the function, so just loop a lot!  This prevents the thread from exiting only to cause another thread to re-create
	//batch state.  We can re-use everything as many times as necessary in times of high thread contention.
	for (int BatchCounter = 0; BatchCounter < 1024; ++BatchCounter) {
		if (!AssignInstancesToBatch(ExecCtx, BatchState))
		{
			return; //no more instances to do, we're done
		}
		int StartInstanceThisChunk = BatchState->StartInstance;
		int NumChunksThisBatch     = (BatchState->NumInstances + ExecCtx->Internal.MaxInstancesPerChunk - 1) / ExecCtx->Internal.MaxInstancesPerChunk;
		for (int ChunkIdxThisBatch = 0; ChunkIdxThisBatch < NumChunksThisBatch; ++ChunkIdxThisBatch, StartInstanceThisChunk += ExecCtx->Internal.MaxInstancesPerChunk)
		{
			int NumInstancesThisChunk = VVM_MIN((int)ExecCtx->Internal.MaxInstancesPerChunk, BatchState->StartInstance + BatchState->NumInstances - StartInstanceThisChunk);
			int NumLoops               = (int)((NumInstancesThisChunk + 3) & ~3) >> 2; //assumes 4-wide ops
			
			if (NumLoops == 1)
			{
#				if VECTORVM_SUPPORTS_AVX
				if (RegIncSetForAVX) {
					SetRegIncTableFor4WideFrom8Wide(ExecCtx, BatchState);
					RegIncSetForAVX = false;
				}
#				endif
				execChunkSingleLoop(ExecCtx, BatchState, StartInstanceThisChunk, NumInstancesThisChunk, SerializeState, CmpSerializeState);
			}
			else if (NumLoops >= 1)
			{
#				if VECTORVM_SUPPORTS_AVX
				if (NumLoops >= 4 && (ExecCtx->VVMState->Flags & VVMFlag_SupportsAVX))
				{
					//SetupBatchPointers sets the register increment table to 16 bytes, with AVX we want 32 bytes.
					if (!RegIncSetForAVX)
					{
						SetRegIncTableFor8WideFrom4Wide(ExecCtx, BatchState);
						RegIncSetForAVX = true;
					}
					int NumAVXLoops = NumLoops / 2;
					execChunkMultipleLoopsAVX(ExecCtx, BatchState, StartInstanceThisChunk, NumInstancesThisChunk, NumAVXLoops, SerializeState, CmpSerializeState);
					if (NumAVXLoops * 2 != NumLoops)
					{
						check(false); //does this even work!? need to run it to find out
						//we have an extra single loop to do at the end
						NumLoops = 1;
						SetRegIncTableFor4WideFrom8Wide(ExecCtx, BatchState);
						RegIncSetForAVX = false;
						execChunkSingleLoop(ExecCtx, BatchState, NumAVXLoops * 8, NumInstancesThisChunk - NumAVXLoops * 8, SerializeState, CmpSerializeState);
					}
				}
				else 
				{
					if (RegIncSetForAVX)
					{
						SetRegIncTableFor4WideFrom8Wide(ExecCtx, BatchState);
						RegIncSetForAVX = false;
					}
#				else
				{
#				endif
					execChunkMultipleLoops(ExecCtx, BatchState, StartInstanceThisChunk, NumInstancesThisChunk, NumLoops, SerializeState, CmpSerializeState);
				}
			}
		}
	
		VVMSer_batchEndExp(SerializeState);

		if (BatchState->ChunkLocalData.RandCounters)
		{
			FMemory::Free(BatchState->ChunkLocalData.RandCounters);
			BatchState->ChunkLocalData.RandCounters = NULL;
		}

		FPlatformAtomics::InterlockedAdd(&ExecCtx->Internal.NumInstancesCompleted, BatchState->NumInstances);
	}
}

static void VVMBuildMapTableCaches(FVectorVMExecContext *ExecCtx)
{
	//constant buffers
	check(ExecCtx->VVMState->NumConstBuffers < 0xFF);
	for (uint32 i = 0; i < ExecCtx->VVMState->NumConstBuffers; ++i)
	{
		uint32 RemappedIdx = ExecCtx->VVMState->ConstRemapTable[i];
		uint32 ConstCountAcc = 0;
		for (int j = 0; j < ExecCtx->ConstantTableCount; ++j)
		{
			const uint32 NumDWords = (uint32)ExecCtx->ConstantTableSizes[j] >> 2;
			if (ConstCountAcc + NumDWords > RemappedIdx)
			{
				const uint32 *SrcConstArray = (uint32 *)ExecCtx->ConstantTableData[j];
				uint32 Idx = RemappedIdx - ConstCountAcc;
				check(Idx < 0xFFFF);
				ExecCtx->VVMState->ConstMapCacheIdx[i] = j;
				ExecCtx->VVMState->ConstMapCacheSrc[i] = (uint16)(Idx);
				break;
			}
			ConstCountAcc += NumDWords;
		}
	}

	//inputs
	if (ExecCtx->VVMState->NumInputDataSets > 0)
	{
		int InputCounter     = 0;
		int NumInputDataSets = VVM_MIN(ExecCtx->VVMState->NumInputDataSets, (uint32)ExecCtx->DataSets.Num()); //Niagara can pass in any amount of datasets, but we only care about the highest one actually used as determined by the optimizer
		for (int i = 0; i < NumInputDataSets; ++i)
		{
			uint32 **DataSetInputBuffers = (uint32 **)ExecCtx->DataSets[i].InputRegisters.GetData();
			int32 InstanceOffset = ExecCtx->DataSets[i].InstanceOffset;

			//regular inputs: float, int and half
			for (int j = 0; j < 3; ++j)
			{
				int NumInputsThisType = ExecCtx->VVMState->InputDataSetOffsets[(i << 3) + j + 1] - ExecCtx->VVMState->InputDataSetOffsets[(i << 3) + j];
				int TypeOffset = ExecCtx->DataSets[i].InputRegisterTypeOffsets[j];
				for (int k = 0; k < NumInputsThisType; ++k)
				{
					int RemapIdx = ExecCtx->VVMState->InputDataSetOffsets[(i << 3) + j] + k;
					ExecCtx->VVMState->InputMapCacheIdx[InputCounter] = i;
					ExecCtx->VVMState->InputMapCacheSrc[InputCounter] = TypeOffset + ExecCtx->VVMState->InputRemapTable[RemapIdx];
					++InputCounter;
				}
			}

			if (ExecCtx->VVMState->InputDataSetOffsets[(i << 3) + 7] > 0)
			{
				//no advance inputs: float, int and half
				//no advance inputs point directly after the constant buffers
				for (int j = 0; j < 3; ++j)
				{
					int NumInputsThisType = ExecCtx->VVMState->InputDataSetOffsets[(i << 3) + j + 4] - ExecCtx->VVMState->InputDataSetOffsets[(i << 3) + j + 3];
					int TypeOffset = ExecCtx->DataSets[i].InputRegisterTypeOffsets[j];
					for (int k = 0; k < NumInputsThisType; ++k)
					{
						int RemapIdx     = ExecCtx->VVMState->InputDataSetOffsets[(i << 3) + j + 3] + k;
						ExecCtx->VVMState->InputMapCacheIdx[InputCounter] = i;
						ExecCtx->VVMState->InputMapCacheSrc[InputCounter] = (TypeOffset + ExecCtx->VVMState->InputRemapTable[RemapIdx]) | 0x8000 | (((j & 2) << 13)); //high bit is no advance input, 2nd high bit is whether it's half
						++InputCounter;
					}
				}
			}
		}
		check(InputCounter == ExecCtx->VVMState->NumInputBuffers);
	}
}

VECTORVM_API void ExecVectorVMState(FVectorVMExecContext *ExecCtx, FVectorVMSerializeState *SerializeState, FVectorVMSerializeState *CmpSerializeState)
{
	if (ExecCtx->ExtFunctionTable.Num() != ExecCtx->VVMState->NumExtFunctions) {
		return;
	}
	for (uint32 i = 0; i < ExecCtx->VVMState->NumExtFunctions; ++i) {
		ExecCtx->VVMState->ExtFunctionTable[i].Function = ExecCtx->ExtFunctionTable[i];
	}
	for (uint32 i = 0; i < ExecCtx->VVMState->NumOutputDataSets; ++i) {
		ExecCtx->VVMState->NumOutputPerDataSet[i] = 0;
	}

	//cache the mappings from niagara data buffers to internal minimized set
	if (!(ExecCtx->VVMState->Flags & VVMFlag_DataMapCacheSetup)) {
		VVMBuildMapTableCaches(ExecCtx);
		ExecCtx->VVMState->Flags |= VVMFlag_DataMapCacheSetup;
	}

	for (uint32 i = 0; i < ExecCtx->VVMState->NumConstBuffers; ++i) {
		ExecCtx->VVMState->ConstantBuffers[i].i = VectorIntSet1(((uint32 *)ExecCtx->ConstantTableData[ExecCtx->VVMState->ConstMapCacheIdx[i]])[ExecCtx->VVMState->ConstMapCacheSrc[i]]);
	}

	//if the number of instances hasn't changed since the last exec, we don't have to re-compute the internal state
	if (ExecCtx->NumInstances == ExecCtx->VVMState->NumInstancesExecCached) 
	{
		ExecCtx->Internal.NumBytesRequiredPerBatch          = ExecCtx->VVMState->ExecCtxCache.NumBytesRequiredPerBatch;
		ExecCtx->Internal.PerBatchRegisterDataBytesRequired = ExecCtx->VVMState->ExecCtxCache.PerBatchRegisterDataBytesRequired;
		ExecCtx->Internal.NumBatches                        = ExecCtx->VVMState->ExecCtxCache.NumBatches;
		ExecCtx->Internal.MaxChunksPerBatch                 = ExecCtx->VVMState->ExecCtxCache.MaxChunksPerBatch;
		ExecCtx->Internal.MaxInstancesPerChunk              = ExecCtx->VVMState->ExecCtxCache.MaxInstancesPerChunk;
	}
	else
	{ //calculate Batch & Chunk division and all internal execution state required before executing
		static const uint32 MaxChunksPerBatch                              = 4; //*MUST BE POW 2* arbitrary 4 chunks per batch... this is harder to load balance because it depends on CPU cores available during execution
		static_assert(MaxChunksPerBatch > 0 && (MaxChunksPerBatch & (MaxChunksPerBatch - 1)) == 0);

		size_t PageSizeInBytes = (uint64_t)GVVMPageSizeInKB << 10;

		size_t PerBatchRegisterDataBytesRequired                           = 0;
		int NumBatches                                                     = 1;
		int NumChunksPerBatch                                              = (int)MaxChunksPerBatch;
		uint32 MaxLoopsPerChunk                                            = 0;
		{ //compute the number of bytes required per batch
			const uint32 TotalNumLoopsRequired              = VVM_MAX(((uint32)ExecCtx->NumInstances + 3) >> 2, 1);
			const size_t NumBytesRequiredPerLoop            = VVM_REG_SIZE * ExecCtx->VVMState->NumTempRegisters;
			if (ExecCtx->VVMState->BatchOverheadSize + 64 > (uint32)PageSizeInBytes)
			{
				//either the chunk size is way too small, or there's an insane number of consts or data sets required... we just revert to the previous VM's default
				MaxLoopsPerChunk                            = 128 >> 2;
				NumBatches                                  = (TotalNumLoopsRequired + MaxLoopsPerChunk - 1) / MaxLoopsPerChunk;
			}
			else
			{
				size_t NumBytesPerBatchAvailableForTempRegs = PageSizeInBytes - ExecCtx->VVMState->BatchOverheadSize;
				size_t TotalNumLoopBytesRequired            = VVM_ALIGN(TotalNumLoopsRequired, MaxChunksPerBatch) * NumBytesRequiredPerLoop;
				if (NumBytesPerBatchAvailableForTempRegs < TotalNumLoopBytesRequired)
				{
					//Not everything fits into a single chunk, so we have to compute everything here
					int NumChunksRequired                   = (int)(TotalNumLoopBytesRequired + NumBytesPerBatchAvailableForTempRegs - 1) / (int)NumBytesPerBatchAvailableForTempRegs;
					check(NumChunksRequired > 1);
					if (NumChunksRequired < MaxChunksPerBatch) //everything fits in a single batch
					{
						NumChunksPerBatch                   = NumChunksRequired;
						//take as little memory as possible and execute it in equal sized chunks
						MaxLoopsPerChunk                    = (TotalNumLoopsRequired + NumChunksRequired - 1) / NumChunksRequired;
					}
					else //not everything fits in a single batch, we have to thread this
					{
						MaxLoopsPerChunk                    = (uint32)(NumBytesPerBatchAvailableForTempRegs / NumBytesRequiredPerLoop);
						uint32 NumLoopsPerBatch             = MaxLoopsPerChunk * NumChunksPerBatch;
						NumBatches                          = (TotalNumLoopsRequired + NumLoopsPerBatch - 1) / NumLoopsPerBatch;
						if (GVVMMaxThreadsPerScript > 0 && NumBatches > GVVMMaxThreadsPerScript)
						{
							//number of batches exceed the number of threads allowed.. increase the number of chunks per batch
							NumLoopsPerBatch                = (TotalNumLoopsRequired + GVVMMaxThreadsPerScript - 1) / GVVMMaxThreadsPerScript;
							NumChunksPerBatch               = (int)(NumLoopsPerBatch + MaxLoopsPerChunk - 1) / MaxLoopsPerChunk;
							NumBatches                      = GVVMMaxThreadsPerScript;
							check(NumBatches * NumChunksPerBatch * MaxLoopsPerChunk >= TotalNumLoopsRequired);
						}
					}
				}
				else
				{
					//everything fits into a single chunk
					NumChunksPerBatch = 1;
					MaxLoopsPerChunk = TotalNumLoopsRequired;
				}
			}
			PerBatchRegisterDataBytesRequired = MaxLoopsPerChunk * NumBytesRequiredPerLoop;
		}

		size_t NumBytesRequiredPerBatch = ExecCtx->VVMState->BatchOverheadSize + PerBatchRegisterDataBytesRequired;
		ExecCtx->Internal.NumBytesRequiredPerBatch          = (uint32)NumBytesRequiredPerBatch;
		ExecCtx->Internal.PerBatchRegisterDataBytesRequired = (uint32)PerBatchRegisterDataBytesRequired;
		ExecCtx->Internal.NumBatches                        = NumBatches;
		ExecCtx->Internal.MaxChunksPerBatch                 = NumChunksPerBatch;
		ExecCtx->Internal.MaxInstancesPerChunk              = MaxLoopsPerChunk << 2;

		ExecCtx->VVMState->ExecCtxCache.NumBytesRequiredPerBatch          = ExecCtx->Internal.NumBytesRequiredPerBatch;
		ExecCtx->VVMState->ExecCtxCache.PerBatchRegisterDataBytesRequired = ExecCtx->Internal.PerBatchRegisterDataBytesRequired;
		ExecCtx->VVMState->ExecCtxCache.NumBatches                        = ExecCtx->Internal.NumBatches;
		ExecCtx->VVMState->ExecCtxCache.MaxChunksPerBatch                 = ExecCtx->Internal.MaxChunksPerBatch;
		ExecCtx->VVMState->ExecCtxCache.MaxInstancesPerChunk              = ExecCtx->Internal.MaxInstancesPerChunk;

		ExecCtx->VVMState->NumInstancesExecCached = ExecCtx->NumInstances;
	}
	ExecCtx->Internal.NumInstancesAssignedToBatches = 0;
	ExecCtx->Internal.NumInstancesCompleted         = 0;

	for (uint32 i = 0; i < ExecCtx->VVMState->NumOutputDataSets; ++i)
	{
		ExecCtx->VVMState->NumOutputPerDataSet[i] = 0;
	}

#if defined(VVM_INCLUDE_SERIALIZATION) && !defined(VVM_SERIALIZE_NO_WRITE)
	uint64 StartTime = FPlatformTime::Cycles64();
	if (SerializeState)
	{
		SerializeState->ExecDt = 0;
		SerializeState->SerializeDt = 0;
	}
#endif //VVM_INCLUDE_SERIALIZATION
	if (ExecCtx->Internal.NumBatches > 1)
	{
#ifdef VVM_USE_OFFLINE_THREADING
		if (parallelJobFn)
		{
			for (uint32 i = 0; i < ExecCtx->Internal.NumBatches; ++i)
			{
				parallelJobFn(ExecVVMBatch, ExecCtx, i, SerializeState, CmpSerializeState);
			}
		}
		else
		{
			for (uint32 i = 0; i < ExecCtx->Internal.NumBatches; ++i)
			{
				ExecVVMBatch(ExecCtx, i, SerializeState, CmpSerializeState);
			}
		}
#else
		auto ExecChunkBatch = [&](int32 BatchIdx)
		{
			ExecVVMBatch(ExecCtx, BatchIdx, SerializeState, CmpSerializeState);
		};
		//for (uint32 i = 0; i < ExecCtx->Internal.NumBatches; ++i)
		//{
		//	ExecVVMBatch(ExecCtx, i, SerializeState, CmpSerializeState);
		//}
		ParallelFor(ExecCtx->Internal.NumBatches, ExecChunkBatch, true);// GbParallelVVM == 0 || !bParallel);
#endif
	}
	else
	{
		ExecVVMBatch(ExecCtx, 0, SerializeState, CmpSerializeState);
	}

#ifdef VVM_USE_OFFLINE_THREADING
	//Unreal's ParallelFor() will block the executing thread until it's finished.  That isn't guaranteed
	//outside of UE, (ie: the debugger, the only other thing that uses this as of this writing) so block.
	while (ExecCtx->Internal.NumInstancesCompleted < ExecCtx->NumInstances)
	{
		FPlatformProcess::Yield();
	}
#endif

	for (uint32 i = 0; i < ExecCtx->VVMState->NumOutputDataSets; ++i)
	{
		ExecCtx->DataSets[i].DataSetAccessIndex = ExecCtx->VVMState->NumOutputPerDataSet[i] - 1;
	}

#if defined(VVM_INCLUDE_SERIALIZATION) && !defined(VVM_SERIALIZE_NO_WRITE)
	uint64 EndTime = FPlatformTime::Cycles64();
	if (SerializeState)
	{
		SerializeState->ExecDt = EndTime - StartTime;
	}
#endif //VVM_INCLUDE_SERIALIZATION
}

#undef VVM_MIN
#undef VVM_MAX
#undef VVM_CLAMP
#undef VVM_ALIGN
#undef VVM_ALIGN_4
#undef VVM_ALIGN_16
#undef VVM_ALIGN_32
#undef VVM_ALIGN_64

#undef VVMSet_m128Const
#undef VVMSet_m128iConst
#undef VVMSet_m128iConst4
#undef VVM_m128Const
#undef VVM_m128iConst


#else //NIAGARA_EXP_VM

VECTORVM_API FVectorVMState *AllocVectorVMState(struct FVectorVMOptimizeContext *OptimizeCtx)
{
	return nullptr;
}

VECTORVM_API void FreeVectorVMState(FVectorVMState *VVMState)
{

}

VECTORVM_API void ExecVectorVMState(FVectorVMState *VVMState, FVectorVMSerializeState *SerializeState, FVectorVMSerializeState *CmpSerializeState)
{

}

#endif //NIAGARA_EXP_VM

