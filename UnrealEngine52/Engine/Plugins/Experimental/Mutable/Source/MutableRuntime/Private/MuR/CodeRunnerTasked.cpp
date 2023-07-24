// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/CodeRunner.h"
#include "MuR/Image.h"
#include "MuR/ImagePrivate.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/OpImageBlend.h"
#include "MuR/OpImageCompose.h"
#include "MuR/OpImageMipmap.h"
#include "MuR/OpImageNormalCombine.h"
#include "MuR/OpImagePixelFormat.h"
#include "MuR/OpImageResize.h"
#include "MuR/OpImageSwizzle.h"
#include "MuR/Operations.h"
#include "MuR/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/Settings.h"
#include "MuR/SettingsPrivate.h"
#include "MuR/SystemPrivate.h"
#include "Stats/Stats2.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "Trace/Detail/Channel.h"


DECLARE_CYCLE_STAT(TEXT("MutableCoreTask"), STAT_MutableCoreTask, STATGROUP_Game);


int32 CoreRunnerTaskPriority =
#ifdef PLATFORM_SWITCH // Required due to low end platforms hitches. Mutable is occupying all threads with long tasks preventing Niagara from executing its time sensitive tasks.
		5;
#else
		0;
#endif

static FAutoConsoleVariableRef CVarCoreRunnerTaskPriority(
	TEXT("mutable.CoreRunnerTaskPriority"),
	CoreRunnerTaskPriority,
	TEXT("0 AnyThread. 1 to 6, from hight to low priority."),
	ECVF_Default);


namespace mu
{

	void CodeRunner::CompleteRomLoadOp( FRomLoadOp& o )
	{
		if (DebugRom && (DebugRomAll || o.m_romIndex == DebugRomIndex))
			UE_LOG(LogMutableCore, Log, TEXT("CodeRunner::CompleteRomLoadOp for rom %d."), o.m_romIndex);

		m_pSystem->m_pStreamInterface->EndRead(o.m_streamID);

		FProgram& program = m_pModel->GetPrivate()->m_program;
		{
			MUTABLE_CPUPROFILER_SCOPE(Unserialise);

			InputMemoryStream stream(o.m_streamBuffer.GetData(), o.m_streamBuffer.Num());
			InputArchive arch(&stream);

			int32 ResIndex = program.m_roms[o.m_romIndex].ResourceIndex;
			switch (o.ConstantType)
			{
			case DATATYPE::DT_MESH:
			{
				check(!program.m_constantMeshes[ResIndex].Value);
				program.m_constantMeshes[ResIndex].Value = Mesh::StaticUnserialise(arch);
				check(program.m_constantMeshes[ResIndex].Value);
				break;
			}
			case DATATYPE::DT_IMAGE:
			{
				check(!program.m_constantImageLODs[ResIndex].Value);
				program.m_constantImageLODs[ResIndex].Value = Image::StaticUnserialise(arch);
				check(program.m_constantImageLODs[ResIndex].Value);
				break;
			}
			default:
				check(false);
				break;
			}

			o.m_romIndex = -1;
			o.m_streamBuffer.Empty();
		}
	}


    //---------------------------------------------------------------------------------------------
    void CodeRunner::Run ()
    {
		bUnrecoverableError = false;

		m_heapData.SetNum(0, false);
		m_heapImageDesc.SetNum(0, false);

		// Profiling data. TODO: make optional
		bool bProfile = false;
		uint32 NumRunOps = 0;
		uint32 RunOpsPerType[size_t(OP_TYPE::COUNT)] = {};

        while( !OpenTasks.IsEmpty() || !ClosedTasks.IsEmpty() || !IssuedTasks.IsEmpty())
        {
			// Debug: log the amount of tasks that we'd be able to run concurrently:
			//{
			//	int32 ClosedReady = ClosedTasks.Num();
			//	for (int Index = ClosedTasks.Num() - 1; Index >= 0; --Index)
			//	{
			//		for (const FCacheAddress& Dep : ClosedTasks[Index].Deps)
			//		{
			//			if (Dep.at && !GetMemory().IsValid(Dep))
			//			{
			//				--ClosedReady;
			//				continue;
			//			}
			//		}
			//	}

			//	UE_LOG(LogMutableCore, Log, TEXT("Tasks: %5d open, %5d issued, %5d closed, %d closed ready"), OpenTasks.Num(), IssuedTasks.Num(), ClosedTasks.Num(), ClosedReady);
			//}

			for (int Index = 0; Index < IssuedTasks.Num(); )
			{
				check(IssuedTasks[Index]);

				bool WorkDone = IssuedTasks[Index]->IsComplete(this);
				if (WorkDone)
				{
					const FScheduledOp& item = IssuedTasks[Index]->Op;
					IssuedTasks[Index]->Complete(this);

					if (ScheduledStagePerOp[item] == item.Stage + 1)
					{
						// We completed everything that was requested, clear it otherwise if needed
						// again it is not going to be rebuilt.
						// \TODO: track rebuilds.
						ScheduledStagePerOp[item] = 0;
					}

					IssuedTasks.RemoveAt(Index); // with swap? changes order of execution.
				}
				else
				{
					++Index;
				}
			}


			while (!OpenTasks.IsEmpty())
			{
				FScheduledOp item = OpenTasks.Pop();

				// Special processing in case it is an ImageDesc operation
				if ( item.Type==FScheduledOp::EType::ImageDesc )
				{
					RunCodeImageDesc(item, m_pParams, m_pModel.Get(), m_lodMask);
					continue;
				}

				// Don't run it if we already have the result.
				FCacheAddress cat(item);
				if (GetMemory().IsValid(cat))
				{
					continue;
				}

				// See if we can schedule this item concurrently
				TSharedPtr<FIssuedTask> IssuedTask = IssueOp(item);
				if (IssuedTask)
				{
					bool bFailed = false;
					bool hasWork = IssuedTask->Prepare(this, m_pModel, bFailed);
					if (bFailed)
					{
						bUnrecoverableError = true;
						return;
					}

					// Launch it
					if (hasWork)
					{
						// Optional hack to ensure deterministic single threading.
						if (bForceSingleThread)
						{
							IssuedTask->Event = {};
							IssuedTask->DoWork();
						}
						else
						{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
							IssuedTask->Event = UE::Tasks::Launch(TEXT("MutableCore_Task"),
								[IssuedTask]() { IssuedTask->DoWork(); },
								UE::Tasks::ETaskPriority::Inherit );
#else

							ENamedThreads::Type Priority;
							switch (CoreRunnerTaskPriority)
							{
							default:
							case 0:
								Priority = ENamedThreads::AnyThread;
								break;
							case 1:
								Priority = ENamedThreads::AnyHiPriThreadHiPriTask;
								break;
							case 2:
								Priority = ENamedThreads::AnyHiPriThreadNormalTask;
								break;
							case 3:
								Priority = ENamedThreads::AnyNormalThreadHiPriTask;
								break;
							case 4:
								Priority = ENamedThreads::AnyNormalThreadNormalTask;
								break;
							case 5:
								Priority = ENamedThreads::AnyBackgroundHiPriTask;
								break;
							case 6:
								Priority = ENamedThreads::AnyBackgroundThreadNormalTask;
								break;								
							}
							
							IssuedTask->Event = FFunctionGraphTask::CreateAndDispatchWhenReady(
								[IssuedTask]() { IssuedTask->DoWork(); },
								TStatId{},
								nullptr,
								Priority);
#endif
						}
					}

					// Remember it for later processing.
					IssuedTasks.Add(IssuedTask);
				}
				else
				{
					// Run immediately
					RunCode(item, m_pParams, m_pModel.Get(), m_lodMask);

					if (ScheduledStagePerOp[item] == item.Stage + 1)
					{
						// We completed everything that was requested, clear it otherwise if needed
						// again it is not going to be rebuilt.
						// \TODO: track rebuilds.
						ScheduledStagePerOp[item] = 0;
					}
				}

				if (bProfile)
				{
					++NumRunOps;
					RunOpsPerType[size_t(m_pModel->GetPrivate()->m_program.GetOpType(item.At))]++;
				}
			}

			// Look for completed streaming ops and complete the rom loading
			for (FRomLoadOp& o : m_romLoadOps)
			{
				if (o.m_romIndex>=0 && m_pSystem->m_pStreamInterface->IsReadCompleted(o.m_streamID))
				{
					CompleteRomLoadOp(o);
				}
			}

			// Look for a closed task with dependencies satisfied and move them to open.
			bool bSomeWasReady = false;
			for (int Index = 0; Index<ClosedTasks.Num(); )
			{
				bool Ready = true;
				for ( const FCacheAddress& Dep: ClosedTasks[Index].Deps )
				{
					bool bDependencySatisfied = false;					
					bDependencySatisfied = Dep.At && !GetMemory().IsValid(Dep);
					
					if (bDependencySatisfied)
					{
						Ready = false;
						continue;
					}
				}

				if (Ready)
				{
					bSomeWasReady = true;
					FTask Task = ClosedTasks[Index];
					ClosedTasks.RemoveAt(Index); // with swap? would change order of execution.
					OpenTasks.Push(Task.Op);
				}
				else
				{
					++Index;
				}
			}

			// Debug: Did we dead-lock?
			bool bDeadLock = !(OpenTasks.Num() || IssuedTasks.Num() || !ClosedTasks.Num() || bSomeWasReady);
			if (bDeadLock)
			{
				// Log the task graph
				for (int Index = 0; Index < ClosedTasks.Num(); ++Index)
				{
					FString TaskDesc = FString::Printf(TEXT("Closed task %d-%d-%d depends on : "), ClosedTasks[Index].Op.At, ClosedTasks[Index].Op.ExecutionIndex, ClosedTasks[Index].Op.Stage );
					for (const FCacheAddress& Dep : ClosedTasks[Index].Deps)
					{
						if (Dep.At && !GetMemory().IsValid(Dep))
						{
							TaskDesc += FString::Printf(TEXT("%d-%d, "), Dep.At, Dep.ExecutionIndex);
						}
					}

					UE_LOG(LogMutableCore, Log, TEXT("%s"), *TaskDesc);
				}
				check(false);
			}

			// If at this point there is no open op and we haven't finished, we need to wait for an issued op to complete.
			if (OpenTasks.IsEmpty() && !IssuedTasks.IsEmpty())
			{
				MUTABLE_CPUPROFILER_SCOPE(CodeRunner_WaitIssued);

				for (int32 IssuedIndex = 0; IssuedIndex<IssuedTasks.Num(); ++IssuedIndex)
				{
					if (IssuedTasks[IssuedIndex]->Event.IsValid())
					{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
						IssuedTasks[IssuedIndex]->Event.Wait();
#else
						IssuedTasks[IssuedIndex]->Event->Wait();
#endif
						break;
					}
				}

				// If we reached here it means we didn't find an op to wait for. Try to wait for a loading op.
				// \todo: unify laoding ops with normal ones?
				for (FRomLoadOp& o : m_romLoadOps)
				{
					if (o.m_romIndex >= 0)
					{
						CompleteRomLoadOp(o);
						break;
					}

					// We should never reach this, since it would mean we deadlocked.
					//check(false);
				}

			}
		}

		if (bProfile)
		{
			UE_LOG(LogMutableCore, Log, TEXT("Mutable Heap Bytes: %d"), m_heapData.Num()* m_heapData.GetTypeSize());
			UE_LOG(LogMutableCore, Log, TEXT("Ran ops : %5d "), NumRunOps);

			constexpr int HistogramSize = 8;
			int32 MostCommonOps[HistogramSize] = {};
			for (size_t i=0; i<size_t(OP_TYPE::COUNT); ++i)
			{
				for (int32 h=0; h<HistogramSize;++h)
				{
					if (RunOpsPerType[i] > RunOpsPerType[MostCommonOps[h]])
					{
						// Displace others
						int32 ElementsToMove = HistogramSize - h - 1;
						if (ElementsToMove > 0)
						{
							FMemory::Memcpy(&MostCommonOps[h + 1], &MostCommonOps[h], sizeof(int32)* ElementsToMove);
						}
						// Set new value
						MostCommonOps[h] = i;
						break;
					}
				}
			}

			for (int h = 0; h < HistogramSize; ++h)
			{
				UE_LOG(LogMutableCore, Log, TEXT("    op %4d, %4d times."), MostCommonOps[h], RunOpsPerType[MostCommonOps[h]]);
			}
		}
    }


	//---------------------------------------------------------------------------------------------
	void CodeRunner::GetImageDescResult(FImageDesc& OutDesc)
	{
		check( m_heapImageDesc.Num()>0 );
		OutDesc = m_heapImageDesc[0];
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class FImageLayerTask : public CodeRunner::FIssuedTask
	{
	public:
		FImageLayerTask(FScheduledOp, FProgramCache&, const OP::ImageLayerArgs&, int imageCompressionQuality );

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		int m_imageCompressionQuality = 0;
		OP::ImageLayerArgs Args;
		Ptr<const Image> m_base;
		Ptr<const Image> m_blended;
		Ptr<const Image> m_mask;
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImageLayerTask::FImageLayerTask(FScheduledOp InOp, FProgramCache& Memory, const OP::ImageLayerArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.base, InOp.ExecutionIndex, InOp.ExecutionOptions });
		check(!m_base || m_base->GetFormat() < EImageFormat::IF_COUNT);

		m_blended = Memory.GetImage({ Args.blended, InOp.ExecutionIndex, InOp.ExecutionOptions });
		if (Args.mask)
		{
			m_mask = Memory.GetImage({ Args.mask, InOp.ExecutionIndex, InOp.ExecutionOptions });
			check(!m_mask || m_mask->GetFormat() < EImageFormat::IF_COUNT);
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageLayerTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImageLayerTask);

		// Warning: This runs in a worker thread.

		if (!m_base)
		{
			// This shouldn't really happen if data is correct.
			return;
		}

		EImageFormat InitialFormat = m_base->GetFormat();
		check(InitialFormat < EImageFormat::IF_COUNT);

		if (IsCompressedFormat(InitialFormat))
		{
			UE_LOG(LogMutableCore, Error, TEXT("Image layer on compressed image is not supported. Ignored."));
			EImageFormat UncompressedFormat = GetUncompressedFormat(InitialFormat);
			m_base = ImagePixelFormat(m_imageCompressionQuality, m_base.get(), UncompressedFormat);
		}

		if (m_blended && InitialFormat != m_blended->GetFormat())
		{
			m_blended = ImagePixelFormat(m_imageCompressionQuality, m_blended.get(), m_base->GetFormat());
		}

		bool bApplyColorBlendToAlpha = (Args.flags & OP::ImageLayerArgs::F_APPLY_TO_ALPHA) != 0;

		ImagePtr pNew = mu::CloneOrTakeOver<Image>(m_base.get());
		//ImagePtr pNew = m_base->Clone();
		check(pNew->GetDataSize() == m_base->GetDataSize());

		bool bValid = pNew->GetSizeX() > 0 && pNew->GetSizeY() > 0;
		if (bValid && m_blended)
		{
			// TODO: This shouldn't happen, but check it to be safe
			if (m_base->GetSize() != m_blended->GetSize())
			{
				MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyFix);
				m_blended = ImageResize(m_blended.get(), m_base->GetSize());
			}

			check(pNew->GetDataSize() == m_base->GetDataSize());

			bool bOnlyOneMip = false;
			if (m_blended->GetLODCount() < m_base->GetLODCount())
			{
				bOnlyOneMip = true;
			}

			bool bDone = false;

			bool bUseMaskFromBlendAlpha = (Args.flags & OP::ImageLayerArgs::F_USE_MASK_FROM_BLENDED);

			if (!m_mask && bUseMaskFromBlendAlpha
				&&
				Args.blendType == uint8(EBlendType::BT_BLEND)
				&&
				Args.blendTypeAlpha == uint8(EBlendType::BT_LIGHTEN))
			{
				// This is a frequent critical-path case because of multilayer projectors.
				bDone = true;

				BufferLayerComposite<BlendChannelMasked, LightenChannel, false>(pNew.get(), m_blended.get(), bOnlyOneMip);
			}


			if (!bDone && Args.mask)
			{
				if (m_base->GetSize() != m_mask->GetSize())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyFix);
					m_mask = ImageResizeLinear(m_imageCompressionQuality, m_mask.get(), m_base->GetSize());
				}

				// emergy fix c36adf47-e40d-490f-b709-41142bafad78
				if (m_mask->GetLODCount() < m_base->GetLODCount())
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageLayer_EmergencyFix);

					int32 levelCount = m_base->GetLODCount();
					ImagePtr pDest = new Image(m_mask->GetSizeX(), m_mask->GetSizeY(),
						levelCount,
						m_mask->GetFormat());

					SCRATCH_IMAGE_MIPMAP scratch;
					FMipmapGenerationSettings settings{};

					ImageMipmap_PrepareScratch(pDest.get(), m_mask.get(), levelCount, &scratch);
					ImageMipmap(m_imageCompressionQuality, pDest.get(), m_mask.get(), levelCount,
						&scratch, settings);

					m_mask = pDest;
				}

				switch (EBlendType(Args.blendType))
				{
				case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), bOnlyOneMip); break;
				case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannelMasked, SoftLightChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannelMasked, HardLightChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_BURN: BufferLayer<BurnChannelMasked, BurnChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_DODGE: BufferLayer<DodgeChannelMasked, DodgeChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_SCREEN: BufferLayer<ScreenChannelMasked, ScreenChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannelMasked, OverlayChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannelMasked, LightenChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannelMasked, MultiplyChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_BLEND: BufferLayer<BlendChannelMasked, BlendChannel, true>(pNew->GetData(), m_base.get(), m_mask.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				default: check(false);
				}

			}
			else if (!bDone && bUseMaskFromBlendAlpha)
			{
				// Apply blend without to RGB using mask in blended alpha
				switch (EBlendType(Args.blendType))
				{
				case EBlendType::BT_NORMAL_COMBINE: check(false); break;
				case EBlendType::BT_SOFTLIGHT: BufferLayerEmbeddedMask<SoftLightChannelMasked, SoftLightChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_HARDLIGHT: BufferLayerEmbeddedMask<HardLightChannelMasked, HardLightChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_BURN: BufferLayerEmbeddedMask<BurnChannelMasked, BurnChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_DODGE: BufferLayerEmbeddedMask<DodgeChannelMasked, DodgeChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_SCREEN: BufferLayerEmbeddedMask<ScreenChannelMasked, ScreenChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_OVERLAY: BufferLayerEmbeddedMask<OverlayChannelMasked, OverlayChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_LIGHTEN: BufferLayerEmbeddedMask<LightenChannelMasked, LightenChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_MULTIPLY: BufferLayerEmbeddedMask<MultiplyChannelMasked, MultiplyChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_BLEND: BufferLayerEmbeddedMask<BlendChannelMasked, BlendChannel, false>(pNew->GetData(), pNew.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				default: check(false);
				}
			}
			else if (!bDone)
			{
				// Apply blend without mask to RGB
				switch (EBlendType(Args.blendType))
				{
				case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_blended.get(), bOnlyOneMip); break;
				case EBlendType::BT_SOFTLIGHT: BufferLayer<SoftLightChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_HARDLIGHT: BufferLayer<HardLightChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_BURN: BufferLayer<BurnChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_DODGE: BufferLayer<DodgeChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_SCREEN: BufferLayer<ScreenChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_OVERLAY: BufferLayer<OverlayChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_LIGHTEN: BufferLayer<LightenChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_MULTIPLY: BufferLayer<MultiplyChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				case EBlendType::BT_BLEND: BufferLayer<BlendChannel, true>(pNew.get(), m_base.get(), m_blended.get(), bApplyColorBlendToAlpha, bOnlyOneMip); break;
				default: check(false);
				}
			}

			// Apply the separate blend operation for alpha
			if (!bDone && !bApplyColorBlendToAlpha && Args.blendTypeAlpha != uint8(EBlendType::BT_NONE))
			{
				// Separate alpha operation ignores the mask.
				switch (EBlendType(Args.blendTypeAlpha))
				{
				case EBlendType::BT_SOFTLIGHT: BufferLayerInPlace<SoftLightChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_HARDLIGHT: BufferLayerInPlace<HardLightChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_BURN: BufferLayerInPlace<BurnChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_DODGE: BufferLayerInPlace<DodgeChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_SCREEN: BufferLayerInPlace<ScreenChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_OVERLAY: BufferLayerInPlace<OverlayChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_LIGHTEN: BufferLayerInPlace<LightenChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_MULTIPLY: BufferLayerInPlace<MultiplyChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				case EBlendType::BT_BLEND: BufferLayerInPlace<BlendChannel, false, 1>(pNew.get(), m_blended.get(), bOnlyOneMip, 3, 3); break;
				default: check(false);
				}
			}

			if (bOnlyOneMip)
			{
				MUTABLE_CPUPROFILER_SCOPE(ImageLayer_MipFix);
				FMipmapGenerationSettings DummyMipSettings{};
				ImageMipmapInPlace(m_imageCompressionQuality, pNew.get(), DummyMipSettings);
			}
		}

		if (InitialFormat != pNew->GetFormat())
		{
			pNew = ImagePixelFormat(m_imageCompressionQuality, pNew.get(), InitialFormat);
		}

		// Free resources
		m_base = nullptr;
		m_blended = nullptr;
		m_mask = nullptr;
		Result = pNew;
	}


	//---------------------------------------------------------------------------------------------
	void FImageLayerTask::Complete( CodeRunner* runner )
	{
		// This runs in the runner thread
		runner->GetMemory().SetImage(Op, Result);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class FImageLayerColourTask : public CodeRunner::FIssuedTask
	{
	public:
		FImageLayerColourTask(FScheduledOp, FProgramCache&, const OP::ImageLayerColourArgs&, int imageCompressionQuality);

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		int m_imageCompressionQuality = 0;
		OP::ImageLayerColourArgs Args;
		FVector4f m_col;
		Ptr<const Image> m_base;
		Ptr<const Image> m_mask;
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImageLayerColourTask::FImageLayerColourTask(FScheduledOp InOp, FProgramCache& Memory, const OP::ImageLayerColourArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.base, InOp.ExecutionIndex, InOp.ExecutionOptions });
		check(!m_base || m_base->GetFormat() < EImageFormat::IF_COUNT);

		m_col = Memory.GetColour({ Args.colour, InOp.ExecutionIndex, 0 });
		if (Args.mask)
		{
			m_mask = Memory.GetImage({ Args.mask, InOp.ExecutionIndex, InOp.ExecutionOptions });
			check(!m_mask || m_mask->GetFormat() < EImageFormat::IF_COUNT);
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageLayerColourTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImageLayerColourTask);

		// Warning: This runs in a worker thread.
		Ptr<Image> pNew;
		if (!m_base)
		{
			return;
		}

		EImageFormat InitialFormat = m_base->GetFormat();
		check(InitialFormat < EImageFormat::IF_COUNT);

		if (Args.mask && m_mask)
		{
			if (m_base->GetSize() != m_mask->GetSize())
			{
				MUTABLE_CPUPROFILER_SCOPE(ImageResize_EmergencyFix);
				m_mask = ImageResizeLinear(m_imageCompressionQuality, m_mask.get(), m_base->GetSize());
			}

			// emergy fix c36adf47-e40d-490f-b709-41142bafad78
			if (m_mask->GetLODCount() < m_base->GetLODCount())
			{
				//MUTABLE_CPUPROFILER_SCOPE(ImageLayerColour_EmergencyFix);

				int32 levelCount = m_base->GetLODCount();
				Ptr<Image> pDest = new Image(m_mask->GetSizeX(), m_mask->GetSizeY(), levelCount, m_mask->GetFormat());

				SCRATCH_IMAGE_MIPMAP scratch;
				FMipmapGenerationSettings settings{};

				ImageMipmap_PrepareScratch(pDest.get(), m_mask.get(), levelCount, &scratch);
				ImageMipmap(m_imageCompressionQuality, pDest.get(), m_mask.get(), levelCount, &scratch, settings);
				m_mask = pDest;
			}
		}

		bool bOnlyOneMip = false;

		// Does it apply to colour?
		if (EBlendType(Args.blendType) != EBlendType::BT_NONE)
		{
			// TODO: It could be done "in-place"
			pNew = mu::CloneOrTakeOver<mu::Image>(m_base.get());
			//pNew = new Image(m_base->GetSizeX(), m_base->GetSizeY(), m_base->GetLODCount(), m_base->GetFormat());

			if (Args.mask && m_mask)
			{			
				// Not implemented yet
				check(Args.flags==0);

				// \todo: precalculated tables for softlight
				switch (EBlendType(Args.blendType))
				{
				case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_SOFTLIGHT: BufferLayerColour<SoftLightChannelMasked, SoftLightChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_HARDLIGHT: BufferLayerColour<HardLightChannelMasked, HardLightChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_BURN: BufferLayerColour<BurnChannelMasked, BurnChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_DODGE: BufferLayerColour<DodgeChannelMasked, DodgeChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_SCREEN: BufferLayerColour<ScreenChannelMasked, ScreenChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_OVERLAY: BufferLayerColour<OverlayChannelMasked, OverlayChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_LIGHTEN: BufferLayerColour<LightenChannelMasked, LightenChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_MULTIPLY: BufferLayerColour<MultiplyChannelMasked, MultiplyChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				case EBlendType::BT_BLEND: BufferLayerColour<BlendChannelMasked, BlendChannel>(pNew->GetData(), m_base.get(), m_mask.get(), m_col); break;
				default: check(false);
				}

			}
			else
			{
				// Not implemented yet
				if (Args.flags & OP::ImageLayerArgs::FLAGS::F_BASE_RGB_FROM_ALPHA)
				{
					switch (EBlendType(Args.blendType))
					{
					case EBlendType::BT_NORMAL_COMBINE: check(false); break;
					case EBlendType::BT_SOFTLIGHT: BufferLayerColourFromAlpha<SoftLightChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_HARDLIGHT: BufferLayerColourFromAlpha<HardLightChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_BURN: BufferLayerColourFromAlpha<BurnChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_DODGE: BufferLayerColourFromAlpha<DodgeChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_SCREEN: BufferLayerColourFromAlpha<ScreenChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_OVERLAY: BufferLayerColourFromAlpha<OverlayChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_LIGHTEN: BufferLayerColourFromAlpha<LightenChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_MULTIPLY: BufferLayerColourFromAlpha<MultiplyChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_BLEND: check(false); break;
					default: check(false);
					}
				}
				else
				{
					switch (EBlendType(Args.blendType))
					{
					case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_SOFTLIGHT: BufferLayerColour<SoftLightChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_HARDLIGHT: BufferLayerColour<HardLightChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_BURN: BufferLayerColour<BurnChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_DODGE: BufferLayerColour<DodgeChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_SCREEN: BufferLayerColour<ScreenChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_OVERLAY: BufferLayerColour<OverlayChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_LIGHTEN: BufferLayerColour<LightenChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_MULTIPLY: BufferLayerColour<MultiplyChannel>(pNew.get(), m_base.get(), m_col); break;
					case EBlendType::BT_BLEND: FillPlainColourImage(pNew.get(), m_col); break;
					default: check(false);
					}
				}
			}
		}

		// Does it apply to alpha?
		if (EBlendType(Args.blendTypeAlpha) != EBlendType::BT_NONE)
		{
			if (!pNew)
			{
				pNew = mu::CloneOrTakeOver<mu::Image>(m_base.get());
			}

			uint32 AlphaOffset = 3;
			if (pNew->GetFormat() == EImageFormat::IF_L_UBYTE)
			{
				AlphaOffset = 1;
			}

			if (Args.mask && m_mask)
			{
				// \todo: precalculated tables for softlight
				switch (EBlendType(Args.blendTypeAlpha))
				{
				case EBlendType::BT_NORMAL_COMBINE: check(false); break;
				case EBlendType::BT_SOFTLIGHT: BufferLayerColourInPlace<SoftLightChannelMasked, SoftLightChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_HARDLIGHT: BufferLayerColourInPlace<HardLightChannelMasked, HardLightChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_BURN: BufferLayerColourInPlace<BurnChannelMasked, BurnChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_DODGE: BufferLayerColourInPlace<DodgeChannelMasked, DodgeChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_SCREEN: BufferLayerColourInPlace<ScreenChannelMasked, ScreenChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_OVERLAY: BufferLayerColourInPlace<OverlayChannelMasked, OverlayChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_LIGHTEN: BufferLayerColourInPlace<LightenChannelMasked, LightenChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_MULTIPLY: BufferLayerColourInPlace<MultiplyChannelMasked, MultiplyChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_BLEND: BufferLayerColourInPlace<BlendChannelMasked, BlendChannel, 1>(pNew.get(), m_mask.get(), m_col, bOnlyOneMip, 3); break;
				default: check(false);
				}

			}
			else
			{
				switch (EBlendType(Args.blendTypeAlpha))
				{
				case EBlendType::BT_NORMAL_COMBINE: check(false); break;
				case EBlendType::BT_SOFTLIGHT: BufferLayerColourInPlace<SoftLightChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_HARDLIGHT: BufferLayerColourInPlace<HardLightChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_BURN: BufferLayerColourInPlace<BurnChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_DODGE: BufferLayerColourInPlace<DodgeChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_SCREEN: BufferLayerColourInPlace<ScreenChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_OVERLAY: BufferLayerColourInPlace<OverlayChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_LIGHTEN: BufferLayerColourInPlace<LightenChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_MULTIPLY: BufferLayerColourInPlace<MultiplyChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				case EBlendType::BT_BLEND: BufferLayerColourInPlace<BlendChannel, 1>(pNew.get(), m_col, bOnlyOneMip, 3); break;
				default: check(false);
				}
			}

		}

		m_base = nullptr;
		m_mask = nullptr;
		Result = pNew;
	}


	//---------------------------------------------------------------------------------------------
	void FImageLayerColourTask::Complete(CodeRunner* runner)
	{
		// This runs in the runner thread
		runner->GetMemory().SetImage(Op, Result);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class FImagePixelFormatTask : public CodeRunner::FIssuedTask
	{
	public:
		FImagePixelFormatTask(FScheduledOp, FProgramCache&, const OP::ImagePixelFormatArgs&, int imageCompressionQuality);

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		int m_imageCompressionQuality = 0;
		OP::ImagePixelFormatArgs Args;
		Ptr<const Image> m_base;
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImagePixelFormatTask::FImagePixelFormatTask(FScheduledOp InOp, FProgramCache& Memory, const OP::ImagePixelFormatArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.source, InOp.ExecutionIndex, InOp.ExecutionOptions });
	}


	//---------------------------------------------------------------------------------------------
	void FImagePixelFormatTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImagePixelFormatTask);

		// Warning: This runs in a worker thread.

		if (!m_base)
			return;

		EImageFormat targetFormat = Args.format;
		if (Args.formatIfAlpha != EImageFormat::IF_NONE
			&&
			GetImageFormatData(m_base->GetFormat()).m_channels > 3)
		{
			// If the alpha channel is all white, we don't need to compress in a format with
			// alpha support, as the default value is always white in shaders, and mutable
			// code. This assumption exist in other commercial engines as well.
			bool hasNonWhiteAlpha = !m_base->IsFullAlpha();
			if (hasNonWhiteAlpha)
			{
				targetFormat = Args.formatIfAlpha;
			}
		}

		if (targetFormat != EImageFormat::IF_NONE)
		{
			if (targetFormat == m_base->GetFormat())
			{
				Result = CloneOrTakeOver<>(m_base.get());
			}
			else
			{
				Ptr<Image> TempResult = ImagePixelFormat(m_imageCompressionQuality, m_base.get(), targetFormat, -1);
				TempResult->m_flags = m_base->m_flags;
				Result = TempResult;
			}
		}
		else
		{
			Result = m_base;
		}

		m_base = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FImagePixelFormatTask::Complete(CodeRunner* runner)
	{
		// This runs in the runner thread
		runner->GetMemory().SetImage(Op, Result);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class FImageMipmapTask : public CodeRunner::FIssuedTask
	{
	public:
		FImageMipmapTask(FScheduledOp, FProgramCache&, const OP::ImageMipmapArgs&, int imageCompressionQuality);

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		int m_imageCompressionQuality = 0;
		OP::ImageMipmapArgs Args;
		Ptr<const Image> m_base;
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImageMipmapTask::FImageMipmapTask(FScheduledOp InOp, FProgramCache& Memory, const OP::ImageMipmapArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.source, InOp.ExecutionIndex, InOp.ExecutionOptions });
	}


	//---------------------------------------------------------------------------------------------
	void FImageMipmapTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImageMipmapTask);

		// Warning: This runs in a worker thread.

		if (!m_base)
		{
			return;
		}

		int32 levelCount = Args.levels;
		int32 maxLevelCount = Image::GetMipmapCount(m_base->GetSizeX(), m_base->GetSizeY());
		if (levelCount == 0)
		{
			levelCount = maxLevelCount;
		}
		else if (levelCount > maxLevelCount)
		{
			// If code generation is smart enough, this should never happen.
			// \todo But apparently it does, sometimes.
			levelCount = maxLevelCount;
		}

		// At least keep the levels we already have.
		int startLevel = m_base->GetLODCount();
		levelCount = FMath::Max(startLevel, levelCount);

		ImagePtr pDest = new Image(m_base->GetSizeX(), m_base->GetSizeY(), levelCount, m_base->GetFormat());
		pDest->m_flags = m_base->m_flags;

		SCRATCH_IMAGE_MIPMAP scratch;
		FMipmapGenerationSettings settings{};

		ImageMipmap_PrepareScratch(pDest.get(), m_base.get(), levelCount, &scratch);
		ImageMipmap(m_imageCompressionQuality, pDest.get(), m_base.get(), levelCount, &scratch, settings);

		m_base = nullptr;
		Result = pDest;
	}


	//---------------------------------------------------------------------------------------------
	void FImageMipmapTask::Complete(CodeRunner* runner)
	{
		// This runs in the runner thread
		runner->GetMemory().SetImage(Op, Result);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class FImageSwizzleTask : public CodeRunner::FIssuedTask
	{
	public:
		FImageSwizzleTask(FScheduledOp, FProgramCache&, const OP::ImageSwizzleArgs&);

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		OP::ImageSwizzleArgs Args;
		Ptr<const Image> Sources[MUTABLE_OP_MAX_SWIZZLE_CHANNELS];
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImageSwizzleTask::FImageSwizzleTask(FScheduledOp InOp, FProgramCache& Memory, const OP::ImageSwizzleArgs& InArgs)
	{
		Op = InOp;
		Args = InArgs;
		for (int i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			if (Args.sources[i])
			{
				Sources[i] = Memory.GetImage({ Args.sources[i], InOp.ExecutionIndex, InOp.ExecutionOptions });
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageSwizzleTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImageSwizzleTask);

		// Warning: This runs in a worker thread.

		// Be defensive: ensure image sizes match.
		for (int i = 1; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			if (Sources[i] && Sources[i]->GetSize() != Sources[0]->GetSize())
			{
				MUTABLE_CPUPROFILER_SCOPE(ImageResize_ForSwizzle);
				Sources[i] = ImageResizeLinear(0, Sources[i].get(), Sources[0]->GetSize());
			}
		}


		EImageFormat format = (EImageFormat)Args.format;
		Result = ImageSwizzle(format, Sources, Args.sourceChannels);
		for (int i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			Sources[i] = nullptr;
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageSwizzleTask::Complete(CodeRunner* runner)
	{
		// This runs in the runner thread
		runner->GetMemory().SetImage(Op, Result);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class FImageComposeTask : public CodeRunner::FIssuedTask
	{
	public:
		FImageComposeTask(FScheduledOp, FProgramCache&, const OP::ImageComposeArgs&, int imageCompressionQuality, const Ptr<const Layout>& );

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		int m_imageCompressionQuality = 0;
		OP::ImageComposeArgs Args;
		Ptr<const Image> m_base;
		Ptr<const Layout> m_layout;
		Ptr<const Image> m_block;
		Ptr<const Image> m_mask;
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImageComposeTask::FImageComposeTask(FScheduledOp InOp, FProgramCache& Memory, const OP::ImageComposeArgs& InArgs, int imageCompressionQuality, const Ptr<const Layout>& InLayout)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_layout = InLayout;

		m_base = Memory.GetImage({ Args.base, Op.ExecutionIndex, InOp.ExecutionOptions });
		
		int relBlockIndex = m_layout->FindBlock(Args.blockIndex);

		if (relBlockIndex >= 0)
		{
			m_block = Memory.GetImage({ Args.blockImage, Op.ExecutionIndex, InOp.ExecutionOptions });
			if (Args.mask)
			{
				m_mask = Memory.GetImage({ Args.mask, Op.ExecutionIndex, InOp.ExecutionOptions });
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageComposeTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImageComposeTask);

		// Warning: This runs in a worker thread.
		int relBlockIndex = m_layout->FindBlock(Args.blockIndex);

		if (relBlockIndex < 0)
		{
			Result = m_base;
		}
		else
		{
			// Try to avoid the clone here
			ImagePtr pComposed = CloneOrTakeOver<>(m_base.get());
			pComposed->m_flags = 0;

			box< vec2<int> > rectInblocks;
			m_layout->GetBlock
			(
				relBlockIndex,
				&rectInblocks.min[0], &rectInblocks.min[1],
				&rectInblocks.size[0], &rectInblocks.size[1]
			);

			// Convert the rect from blocks to pixels
			FIntPoint grid = m_layout->GetGridSize();

			int blockSizeX = m_base->GetSizeX() / grid[0];
			int blockSizeY = m_base->GetSizeY() / grid[1];
			box< vec2<int> > rect = rectInblocks;
			rect.min[0] *= blockSizeX;
			rect.min[1] *= blockSizeY;
			rect.size[0] *= blockSizeX;
			rect.size[1] *= blockSizeY;

			//
			if (m_block && rect.size[0] && rect.size[1] && m_block->GetSizeX() && m_block->GetSizeY())
			{
				bool useMask = Args.mask != 0;

				if (!useMask)
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageComposeWithoutMask);

					// Resize image if it doesn't fit in the new block size
					if (m_block->GetSizeX() != rect.size[0] ||
						m_block->GetSizeY() != rect.size[1])
					{
						// This now happens more often since the generation of specific mips on request. For this reason
						// this warning is usually acceptable.
						//UE_LOG(LogMutableCore, Log, TEXT("Required image resize for ImageCompose: performance warning."));
						FImageSize blockSize((uint16)rect.size[0], (uint16)rect.size[1]);
						m_block = ImageResizeLinear(m_imageCompressionQuality, m_block.get(), blockSize);
					}

					// Change the block image format if it doesn't match the composed image
					// This is usually enforced at object compilation time.
					if (pComposed->GetFormat() != m_block->GetFormat())
					{
						MUTABLE_CPUPROFILER_SCOPE(ImageComposeReformat);

						EImageFormat format = GetMostGenericFormat(pComposed->GetFormat(), m_block->GetFormat());
						if (pComposed->GetFormat() != format)
						{
							pComposed = ImagePixelFormat(m_imageCompressionQuality, pComposed.get(), format);
						}
						if (m_block->GetFormat() != format)
						{
							m_block = ImagePixelFormat(m_imageCompressionQuality, m_block.get(), format);
						}
					}

					// Compose without a mask
					ImageCompose(pComposed.get(), m_block.get(), rect);
				}
				else
				{
					MUTABLE_CPUPROFILER_SCOPE(ImageComposeWithMask);

					// Compose with a mask
					ImageBlendOnBaseNoAlpha(pComposed.get(), m_mask.get(), m_block.get(), rect);
				}

			}

			Result = pComposed;
		}

		m_base = nullptr;
		m_block = nullptr;
		m_layout = nullptr;
		m_mask = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void FImageComposeTask::Complete(CodeRunner* runner)
	{
		// This runs in the runner thread
		runner->GetMemory().SetImage(Op, Result);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	bool CodeRunner::FLoadMeshRomTask::Prepare(CodeRunner* runner, const TSharedPtr<const Model>& InModel, bool& bOutFailed )
	{
		MUTABLE_CPUPROFILER_SCOPE(FLoadMeshRomTask_Prepare);

		// This runs in th CodeRunner thread
		bOutFailed = false;

		FProgram& program = InModel->GetPrivate()->m_program;

		check(RomIndex < program.m_roms.Num());
		bool bRomIsLoaded = program.IsRomLoaded(RomIndex);
		bool bRomHasPendingOps = runner->m_romPendingOps[RomIndex]!=0;
		if (!bRomIsLoaded && !bRomHasPendingOps)
		{
			check(runner->m_pSystem->m_pStreamInterface);

			// Free roms if necessary
			{
				MUTABLE_CPUPROFILER_SCOPE(FreeingRoms);
				runner->m_pSystem->m_modelCache.UpdateForLoad(RomIndex, InModel,
					[&](const Model* pModel, int romInd)
					{
						return (pModel == InModel.Get())
							&&
							runner->m_romPendingOps[romInd] > 0;
					});
			}

			uint32 RomSize = program.m_roms[RomIndex].Size;
			check(RomSize>0);

			CodeRunner::FRomLoadOp* op = nullptr;
			for (FRomLoadOp& o : runner->m_romLoadOps)
			{
				if (o.m_romIndex < 0)
				{
					op = &o;
				}
			}

			if (!op)
			{
				runner->m_romLoadOps.Add(CodeRunner::FRomLoadOp());
				op = &runner->m_romLoadOps.Last();
			}

			op->m_romIndex = RomIndex;
			op->ConstantType = DT_MESH;

			if (uint32(op->m_streamBuffer.Num()) < RomSize)
			{
				// This could happen in 32-bit platforms
				check(RomSize < std::numeric_limits<size_t>::max());
				op->m_streamBuffer.SetNum((size_t)RomSize);
			}

			uint32 RomId = program.m_roms[RomIndex].Id;
			op->m_streamID = runner->m_pSystem->m_pStreamInterface->BeginReadBlock(InModel.Get(), RomId, op->m_streamBuffer.GetData(), RomSize);
			if (op->m_streamID < 0)
			{
				bOutFailed = true;
				return false;
			}
		}
		++runner->m_romPendingOps[RomIndex];

		//UE_LOG(LogMutableCore, Log, TEXT("FLoadMeshRomTask::Prepare romindex %d pending %d."), RomAt.m_rom, runner->m_romPendingOps[RomAt.m_rom]);

		// No worker thread work
		return false;
	}


	//---------------------------------------------------------------------------------------------
	void CodeRunner::FLoadMeshRomTask::Complete(CodeRunner* runner)
	{
		MUTABLE_CPUPROFILER_SCOPE(FLoadMeshRomTask_Complete);

		// This runs in the runner thread

		// Process the constant op normally, now that the rom is loaded.
		runner->RunCode(Op, runner->m_pParams, runner->m_pModel.Get(), runner->m_lodMask);

		runner->m_pSystem->m_modelCache.MarkRomUsed(RomIndex, runner->m_pModel);
		--runner->m_romPendingOps[RomIndex];
	}


	//---------------------------------------------------------------------------------------------
	bool CodeRunner::FLoadMeshRomTask::IsComplete(CodeRunner* runner)
	{ 
		FProgram& program = runner->m_pModel->GetPrivate()->m_program;
		return program.IsRomLoaded(RomIndex);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	bool CodeRunner::FLoadImageRomsTask::Prepare(CodeRunner* runner, const TSharedPtr<const Model>& InModel, bool& bOutFailed )
	{
		MUTABLE_CPUPROFILER_SCOPE(FLoadImageRomsTask_Prepare);

		// This runs in th CodeRunner thread
		bOutFailed = false;

		FProgram& program = InModel->GetPrivate()->m_program;

		for (int32 i = 0; i<LODIndexCount; ++i )
		{
			int32 CurrentIndexIndex = LODIndexIndex + i;
			int32 CurrentIndex = program.m_constantImageLODIndices[CurrentIndexIndex];

			if (program.m_constantImageLODs[CurrentIndex].Key<0)
			{
				// This data is always resident.
				continue;
			}

			int32 RomIndex = program.m_constantImageLODs[CurrentIndex].Key;
			check(RomIndex < program.m_roms.Num());

			bool bRomIsLoaded = program.IsRomLoaded(RomIndex);
			bool bRomHasAlreadyBeenRequested = runner->m_romPendingOps[RomIndex] != 0;
			++runner->m_romPendingOps[RomIndex];

			if (DebugRom && (DebugRomAll || RomIndex == DebugRomIndex))
				UE_LOG(LogMutableCore, Log, TEXT("Preparing rom %d, now peding ops is %d."), RomIndex, runner->m_romPendingOps[RomIndex]);

			if (bRomIsLoaded || bRomHasAlreadyBeenRequested)
			{
				continue;
			}

			check(runner->m_pSystem->m_pStreamInterface);

			// Free roms if necessary
			{
				MUTABLE_CPUPROFILER_SCOPE(FreeingRoms);
				runner->m_pSystem->m_modelCache.UpdateForLoad(RomIndex, InModel,
					[&](const Model* pModel, int romInd)
					{
						return (pModel == InModel.Get())
							&&
							runner->m_romPendingOps[romInd] > 0;
					});
			}

			uint32 RomSize = program.m_roms[RomIndex].Size;
			check(RomSize > 0);

			CodeRunner::FRomLoadOp* op = nullptr;
			for (FRomLoadOp& o : runner->m_romLoadOps)
			{
				if (o.m_romIndex < 0)
				{
					op = &o;
				}
			}

			if (!op)
			{
				runner->m_romLoadOps.Add(CodeRunner::FRomLoadOp());
				op = &runner->m_romLoadOps.Last();
			}

			op->m_romIndex = RomIndex;
			op->ConstantType = DT_IMAGE;

			if (uint32(op->m_streamBuffer.Num()) < RomSize)
			{
				// This could happen in 32-bit platforms
				check(RomSize < std::numeric_limits<size_t>::max());
				op->m_streamBuffer.SetNum(RomSize);
			}

			uint32 RomId = program.m_roms[RomIndex].Id;
			op->m_streamID = runner->m_pSystem->m_pStreamInterface->BeginReadBlock(InModel.Get(), RomId, op->m_streamBuffer.GetData(), RomSize);
			if (op->m_streamID < 0)
			{
				bOutFailed = true;
				return false;
			}
		}

		//UE_LOG(LogMutableCore, Log, TEXT("FLoadImageRomsTask::Prepare romindex %d pending %d."), RomAt.m_rom, runner->m_romPendingOps[RomAt.m_rom]);

		// No worker thread work
		return false;
	}


	//---------------------------------------------------------------------------------------------
	void CodeRunner::FLoadImageRomsTask::Complete(CodeRunner* runner)
	{
		MUTABLE_CPUPROFILER_SCOPE(FLoadImageRomsTask_Complete);

		// This runs in the runner thread

		// Process the constant op normally, now that the rom is loaded.
		runner->RunCode(Op, runner->m_pParams, runner->m_pModel.Get(), runner->m_lodMask);

		FProgram& program = runner->m_pModel->GetPrivate()->m_program;
		for (int32 i = 0; i < LODIndexCount; ++i)
		{
			int32 CurrentIndexIndex = LODIndexIndex + i;
			int32 CurrentIndex = program.m_constantImageLODIndices[CurrentIndexIndex];

			if (program.m_constantImageLODs[CurrentIndex].Key < 0)
			{
				// This data is always resident.
				continue;
			}

			int32 RomIndex = program.m_constantImageLODs[CurrentIndex].Key;
			check(RomIndex < program.m_roms.Num());

			runner->m_pSystem->m_modelCache.MarkRomUsed(RomIndex, runner->m_pModel);
			--runner->m_romPendingOps[RomIndex];

			if (DebugRom && (DebugRomAll || RomIndex==DebugRomIndex))
				UE_LOG(LogMutableCore, Log, TEXT("FLoadImageRomsTask::Complete rom %d, now peding ops is %d."), RomIndex, runner->m_romPendingOps[RomIndex]);
		}
	}


	//---------------------------------------------------------------------------------------------
	bool CodeRunner::FLoadImageRomsTask::IsComplete(CodeRunner* runner)
	{
		FProgram& program = runner->m_pModel->GetPrivate()->m_program;
		for (int32 i = 0; i < LODIndexCount; ++i)
		{
			int32 CurrentIndexIndex = LODIndexIndex + i;
			int32 CurrentIndex = program.m_constantImageLODIndices[CurrentIndexIndex];

			if (program.m_constantImageLODs[CurrentIndex].Key < 0)
			{
				// This data is always resident.
				continue;
			}

			int32 RomIndex = program.m_constantImageLODs[CurrentIndex].Key;
			check(RomIndex < program.m_roms.Num());
			if (!program.IsRomLoaded(RomIndex))
			{
				return false;
			}
		}

		return true;
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	class FImageExternalLoadTask : public CodeRunner::FIssuedTask
	{
	public:
		FImageExternalLoadTask(FScheduledOp InItem, uint8 InMipmapsToSkip, EXTERNAL_IMAGE_ID InId);

		// FIssuedTask interface
		virtual bool Prepare(CodeRunner*, const TSharedPtr<const Model>&, bool& bOutFailed) override;
		virtual void Complete(CodeRunner* Runner) override;
		
	private:
		FScheduledOp Item;
		uint8 MipmapsToSkip;
		EXTERNAL_IMAGE_ID Id;

		Ptr<Image> Result;
		
		TFunction<void()> ExternalCleanUpFunc;
	};


	//---------------------------------------------------------------------------------------------
	FImageExternalLoadTask::FImageExternalLoadTask(FScheduledOp InItem,  uint8 InMipmapsToSkip, EXTERNAL_IMAGE_ID InId)
	{
		Item = InItem;
		MipmapsToSkip = InMipmapsToSkip;
		Id = InId;
	}

	//---------------------------------------------------------------------------------------------
	bool FImageExternalLoadTask::Prepare(CodeRunner* Runner, const TSharedPtr<const Model>&, bool& bOutFailed)
	{
		// LoadExternalImageAsync will always generate some image even if it is a dummy one.
		bOutFailed = false;

		// Capturing this here should not be a problem. The lifetime of the callback lambda is tied to 
		// the task and the later will always outlive the former. 
		
		// Probably we could simply pass a reference to the result image. 
		TFunction<void (Ptr<Image>)> ResultCallback = [this](Ptr<Image> InResult)
		{
			Result = InResult;
		};

		Tie(Event, ExternalCleanUpFunc) = Runner->LoadExternalImageAsync(Id, MipmapsToSkip, ResultCallback);

		// return false indicating there is no work to do so Event is not overriden by a DoWork task.
		return false;
	}

	//---------------------------------------------------------------------------------------------
	void FImageExternalLoadTask::Complete(CodeRunner* Runner)
	{
		if (ExternalCleanUpFunc)
		{
			Invoke(ExternalCleanUpFunc);
		}

		Runner->GetMemory().SetImage(Item, Result);
	}	

	
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	TSharedPtr<CodeRunner::FIssuedTask> CodeRunner::IssueOp(FScheduledOp item)
	{
		TSharedPtr<FIssuedTask> Issued;

		FProgram& program = m_pModel->GetPrivate()->m_program;

		OP_TYPE type = program.GetOpType(item.At);

		switch (type)
		{
		case OP_TYPE::ME_CONSTANT:
		{
			OP::MeshConstantArgs args = program.GetOpArgs<OP::MeshConstantArgs>(item.At);
			int32 RomIndex = program.m_constantMeshes[args.value].Key;
			if (RomIndex >= 0 && !program.m_constantMeshes[args.value].Value )
			{
				Issued = MakeShared<FLoadMeshRomTask>(item, RomIndex);
			}
			else
			{
				// If already available, the rest of the constant code will run right away.
			}
			break;
		}

		case OP_TYPE::IM_CONSTANT:
		{
			OP::ResourceConstantArgs args = program.GetOpArgs<OP::ResourceConstantArgs>(item.At);
			int32 MipsToSkip = item.ExecutionOptions;
			int32 ImageIndex = args.value;
			int32 ReallySkip = FMath::Min(MipsToSkip, program.m_constantImages[ImageIndex].LODCount - 1);
			int32 LODIndexIndex = program.m_constantImages[ImageIndex].FirstIndex + ReallySkip;
			int32 LODIndexCount = program.m_constantImages[ImageIndex].LODCount - ReallySkip;
			check(LODIndexCount > 0);

			bool bAnyMissing = false;
			for (int32 i=0; i<LODIndexCount; ++i)
			{
				uint32 LODIndex = program.m_constantImageLODIndices[LODIndexIndex+i];
				if ( !program.m_constantImageLODs[LODIndex].Value )
				{
					bAnyMissing = true;
					break;
				}
			}

			if (bAnyMissing)
			{
				Issued = MakeShared<FLoadImageRomsTask>(item, LODIndexIndex, LODIndexCount);

				if (DebugRom && (DebugRomAll || ImageIndex == DebugImageIndex))
					UE_LOG(LogMutableCore, Log, TEXT("Issuing image %d skipping %d ."), ImageIndex, ReallySkip);
			}
			else
			{
				// If already available, the rest of the constant code will run right away.
				if (DebugRom && (DebugRomAll || ImageIndex == DebugImageIndex))
					UE_LOG(LogMutableCore, Log, TEXT("Image %d skipping %d is already loaded."), ImageIndex, ReallySkip);
			}
			break;
		}		

		case OP_TYPE::IM_PARAMETER:
		{
			OP::ParameterArgs args = program.GetOpArgs<OP::ParameterArgs>(item.At);
			Ptr<RangeIndex> Index = BuildCurrentOpRangeIndex(item, m_pParams, m_pModel.Get(), args.variable);

			const EXTERNAL_IMAGE_ID Id = m_pParams->GetImageValue(args.variable, Index);
			const uint8 MipmapsToSkip = item.ExecutionOptions;
			
			Issued = MakeShared<FImageExternalLoadTask>(item, MipmapsToSkip, Id);

			break;
		}
			
		case OP_TYPE::IM_PIXELFORMAT:
		{
			if (item.Stage == 1)
			{
				OP::ImagePixelFormatArgs args = program.GetOpArgs<OP::ImagePixelFormatArgs>(item.At);
				Issued = MakeShared<FImagePixelFormatTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_LAYERCOLOUR:
		{
			if (item.Stage == 1)
			{
				OP::ImageLayerColourArgs args = program.GetOpArgs<OP::ImageLayerColourArgs>(item.At);
				Issued = MakeShared<FImageLayerColourTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_LAYER:
		{
			if (item.Stage == 1)
			{
				OP::ImageLayerArgs args = program.GetOpArgs<OP::ImageLayerArgs>(item.At);
				Issued = MakeShared<FImageLayerTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_MIPMAP:
		{
			if (item.Stage == 1)
			{
				OP::ImageMipmapArgs args = program.GetOpArgs<OP::ImageMipmapArgs>(item.At);
				Issued = MakeShared<FImageMipmapTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_SWIZZLE:
		{
			if (item.Stage == 1)
			{
				OP::ImageSwizzleArgs args = program.GetOpArgs<OP::ImageSwizzleArgs>(item.At);
				Issued = MakeShared<FImageSwizzleTask>(item, GetMemory(), args);
			}
			break;
		}

		case OP_TYPE::IM_COMPOSE:
		{
			if (item.Stage == 2)
			{
				OP::ImageComposeArgs args = program.GetOpArgs<OP::ImageComposeArgs>(item.At);
				Ptr<const Layout> ComposeLayout = static_cast<const Layout*>( m_heapData[item.CustomState].Resource.get());
				Issued = MakeShared<FImageComposeTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality, ComposeLayout);
			}
			break;
		}

		default:
			break;
		}

		return Issued;
	}


} // namespace mu
