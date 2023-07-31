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
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/OpImageAlphaOverlay.h"
#include "MuR/OpImageBlend.h"
#include "MuR/OpImageBurn.h"
#include "MuR/OpImageCompose.h"
#include "MuR/OpImageDodge.h"
#include "MuR/OpImageHardLight.h"
#include "MuR/OpImageMipmap.h"
#include "MuR/OpImageMultiply.h"
#include "MuR/OpImageNormalCombine.h"
#include "MuR/OpImageOverlay.h"
#include "MuR/OpImagePixelFormat.h"
#include "MuR/OpImageResize.h"
#include "MuR/OpImageScreen.h"
#include "MuR/OpImageSoftLight.h"
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

namespace mu
{

	void CodeRunner::CompleteRomLoadOp( ROM_LOAD_OP& o )
	{
		if (DebugRom && (DebugRomAll || o.m_romIndex == DebugRomIndex))
			UE_LOG(LogMutableCore, Log, TEXT("CodeRunner::CompleteRomLoadOp for rom %d."), o.m_romIndex);

		m_pSystem->m_pStreamInterface->EndRead(o.m_streamID);

		PROGRAM& program = m_pModel->GetPrivate()->m_program;
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
			//		for (const CACHE_ADDRESS& Dep : ClosedTasks[Index].Deps)
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
					const SCHEDULED_OP& item = IssuedTasks[Index]->Op;
					IssuedTasks[Index]->Complete(this);

					if (ScheduledStagePerOp[item] == item.stage + 1)
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
				SCHEDULED_OP item = OpenTasks.Pop();

				// Special processing in case it is an ImageDesc operation
				if ( item.Type==SCHEDULED_OP::EType::ImageDesc )
				{
					RunCodeImageDesc(item, m_pParams, m_pModel, m_lodMask);
					continue;
				}

				// Don't run it if we already have the result.
				CACHE_ADDRESS cat(item);
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
							IssuedTask->Event = FFunctionGraphTask::CreateAndDispatchWhenReady(
								[IssuedTask]() { IssuedTask->DoWork(); },
								TStatId{},
								nullptr,
								ENamedThreads::AnyThread);
#endif
						}
					}

					// Remember it for later processing.
					IssuedTasks.Add(IssuedTask);
				}
				else
				{
					// Run immediately
					RunCode(item, m_pParams, m_pModel, m_lodMask);

					if (ScheduledStagePerOp[item] == item.stage + 1)
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
					RunOpsPerType[size_t(m_pModel->GetPrivate()->m_program.GetOpType(item.at))]++;
				}
			}

			// Look for completed streaming ops and complete the rom loading
			for (auto& o : m_romLoadOps)
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
				for ( const CACHE_ADDRESS& Dep: ClosedTasks[Index].Deps )
				{
					bool bDependencySatisfied = false;					
					bDependencySatisfied = Dep.at && !GetMemory().IsValid(Dep);
					
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
					FString TaskDesc = FString::Printf(TEXT("Closed task %d-%d-%d depends on : "), ClosedTasks[Index].Op.at, ClosedTasks[Index].Op.executionIndex, ClosedTasks[Index].Op.stage );
					for (const CACHE_ADDRESS& Dep : ClosedTasks[Index].Deps)
					{
						if (Dep.at && !GetMemory().IsValid(Dep))
						{
							TaskDesc += FString::Printf(TEXT("%d-%d, "), Dep.at, Dep.executionIndex);
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
				for (auto& o : m_romLoadOps)
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
		FImageLayerTask(SCHEDULED_OP, FProgramCache&, const OP::ImageLayerArgs&, int imageCompressionQuality );

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
	FImageLayerTask::FImageLayerTask(SCHEDULED_OP InOp, FProgramCache& Memory, const OP::ImageLayerArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.base, InOp.executionIndex, InOp.executionOptions });
		m_blended = Memory.GetImage({ Args.blended, InOp.executionIndex, InOp.executionOptions });
		if (Args.mask)
		{
			m_mask = Memory.GetImage({ Args.mask, InOp.executionIndex, InOp.executionOptions });
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageLayerTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImageLayerTask);

		// Warning: This runs in a worker thread.

		// TODO: a bug may cause this (issue T97)
		EImageFormat finalFormat = EImageFormat::IF_NONE;
		if (IsCompressedFormat(m_base->GetFormat()))
		{
			UE_LOG(LogMutableCore, Error, TEXT("Image layer on compressed image is not supported. Ignored."));
			finalFormat = m_base->GetFormat();
			auto uncompressedFormat = GetUncompressedFormat(finalFormat);
			m_base = ImagePixelFormat(m_imageCompressionQuality, m_base.get(), uncompressedFormat);
		}

		if (m_blended && m_base->GetFormat() != m_blended->GetFormat())
		{
			m_blended =
				ImagePixelFormat(m_imageCompressionQuality, m_blended.get(), m_base->GetFormat());
		}

		bool applyToAlpha = (Args.flags & OP::ImageLayerArgs::F_APPLY_TO_ALPHA) != 0;

		ImagePtr pNew = new Image(m_base->GetSizeX(), m_base->GetSizeY(),
			m_base->GetLODCount(), m_base->GetFormat());

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

			if (m_blended->GetLODCount() < m_base->GetLODCount())
			{
				MUTABLE_CPUPROFILER_SCOPE(ImageLayer_MipBlendedEmergencyFix);

				int32 levelCount = m_base->GetLODCount();
				ImagePtr pDest = new Image(m_blended->GetSizeX(), m_blended->GetSizeY(),
					levelCount,
					m_blended->GetFormat());

				SCRATCH_IMAGE_MIPMAP scratch;
				FMipmapGenerationSettings settings{};

				ImageMipmap_PrepareScratch(pDest.get(), m_blended.get(), levelCount, &scratch);
				ImageMipmap(m_imageCompressionQuality, pDest.get(), m_blended.get(), levelCount,
					&scratch, settings);

				m_blended = pDest;
			}

			if (Args.mask)
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
				case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_mask.get(), m_blended.get()); break;
				case EBlendType::BT_SOFTLIGHT: ImageSoftLight(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_HARDLIGHT: ImageHardLight(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_BURN: ImageBurn(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_DODGE: ImageDodge(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_SCREEN: ImageScreen(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_OVERLAY: ImageOverlay(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_ALPHA_OVERLAY: ImageAlphaOverlay(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_MULTIPLY: ImageMultiply(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_BLEND: ImageBlend(pNew.get(), m_base.get(), m_mask.get(), m_blended.get(), applyToAlpha); break;
				default: check(false);
				}

			}
			else
			{
				switch (EBlendType(Args.blendType))
				{
				case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_blended.get()); break;
				case EBlendType::BT_SOFTLIGHT: ImageSoftLight(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_HARDLIGHT: ImageHardLight(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_BURN: ImageBurn(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_DODGE: ImageDodge(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_SCREEN: ImageScreen(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_OVERLAY: ImageOverlay(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_ALPHA_OVERLAY: ImageAlphaOverlay(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_MULTIPLY: ImageMultiply(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				case EBlendType::BT_BLEND: ImageBlend(pNew.get(), m_base.get(), m_blended.get(), applyToAlpha); break;
				default: check(false);
				}
			}
		}

		if (finalFormat != EImageFormat::IF_NONE)
		{
			pNew = ImagePixelFormat(m_imageCompressionQuality, pNew.get(), finalFormat);
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
		FImageLayerColourTask(SCHEDULED_OP, FProgramCache&, const OP::ImageLayerColourArgs&, int imageCompressionQuality);

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		int m_imageCompressionQuality = 0;
		OP::ImageLayerColourArgs Args;
		Ptr<const Image> m_base;
		vec4<float> m_col;
		Ptr<const Image> m_mask;
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImageLayerColourTask::FImageLayerColourTask(SCHEDULED_OP InOp, FProgramCache& Memory, const OP::ImageLayerColourArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.base, InOp.executionIndex, InOp.executionOptions });
		m_col = Memory.GetColour({ Args.colour, InOp.executionIndex, 0 });
		if (Args.mask)
		{
			m_mask = Memory.GetImage({ Args.mask, InOp.executionIndex, InOp.executionOptions });
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageLayerColourTask::DoWork()
	{
		MUTABLE_CPUPROFILER_SCOPE(FImageLayerColourTask);

		// Warning: This runs in a worker thread.

		ImagePtr pNew = new Image(m_base->GetSizeX(), m_base->GetSizeY(),
			m_base->GetLODCount(), m_base->GetFormat());

		if (Args.mask)
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

			// \todo: precalculated tables for softlight
			switch (EBlendType(Args.blendType))
			{
			case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_SOFTLIGHT: ImageSoftLight(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz(), nullptr); break;
			case EBlendType::BT_HARDLIGHT: ImageHardLight(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_BURN: ImageBurn(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_DODGE: ImageDodge(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_SCREEN: ImageScreen(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_OVERLAY: ImageOverlay(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_ALPHA_OVERLAY: ImageAlphaOverlay(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_MULTIPLY: ImageMultiply(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			case EBlendType::BT_BLEND: ImageBlend(pNew.get(), m_base.get(), m_mask.get(), m_col.xyz()); break;
			default: check(false);
			}

		}
		else
		{
			switch (EBlendType(Args.blendType))
			{
			case EBlendType::BT_NORMAL_COMBINE: ImageNormalCombine(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_SOFTLIGHT: ImageSoftLight(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_HARDLIGHT: ImageHardLight(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_BURN: ImageBurn(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_DODGE: ImageDodge(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_SCREEN: ImageScreen(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_OVERLAY: ImageOverlay(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_ALPHA_OVERLAY: ImageAlphaOverlay(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_MULTIPLY: ImageMultiply(pNew.get(), m_base.get(), m_col.xyz()); break;
			case EBlendType::BT_BLEND: FillPlainColourImage(pNew.get(), m_col); break;
			default: check(false);
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
		FImagePixelFormatTask(SCHEDULED_OP, FProgramCache&, const OP::ImagePixelFormatArgs&, int imageCompressionQuality);

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
	FImagePixelFormatTask::FImagePixelFormatTask(SCHEDULED_OP InOp, FProgramCache& Memory, const OP::ImagePixelFormatArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.source, InOp.executionIndex, InOp.executionOptions });
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
			vec2<int> resultSize;
			int resultLODCount = 0;

			resultSize[0] = m_base->GetSizeX();
			resultSize[1] = m_base->GetSizeY();
			resultLODCount = m_base->GetLODCount();

			Ptr<Image> TempResult = ImagePixelFormat(m_imageCompressionQuality, m_base.get(), targetFormat , -1);
			TempResult->m_flags = m_base->m_flags;

			Result = TempResult;
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
		FImageMipmapTask(SCHEDULED_OP, FProgramCache&, const OP::ImageMipmapArgs&, int imageCompressionQuality);

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
	FImageMipmapTask::FImageMipmapTask(SCHEDULED_OP InOp, FProgramCache& Memory, const OP::ImageMipmapArgs& InArgs, int imageCompressionQuality)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_base = Memory.GetImage({ Args.source, InOp.executionIndex, InOp.executionOptions });
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
		FImageSwizzleTask(SCHEDULED_OP, FProgramCache&, const OP::ImageSwizzleArgs&);

		// FIssuedTask interface
		void DoWork() override;
		void Complete(CodeRunner* staged) override;

	private:
		OP::ImageSwizzleArgs Args;
		Ptr<const Image> Sources[MUTABLE_OP_MAX_SWIZZLE_CHANNELS];
		Ptr<const Image> Result;
	};


	//---------------------------------------------------------------------------------------------
	FImageSwizzleTask::FImageSwizzleTask(SCHEDULED_OP InOp, FProgramCache& Memory, const OP::ImageSwizzleArgs& InArgs)
	{
		Op = InOp;
		Args = InArgs;
		for (int i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			if (Args.sources[i])
			{
				Sources[i] = Memory.GetImage({ Args.sources[i], InOp.executionIndex, InOp.executionOptions });
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
		FImageComposeTask(SCHEDULED_OP, FProgramCache&, const OP::ImageComposeArgs&, int imageCompressionQuality, const Ptr<const Layout>& );

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
	FImageComposeTask::FImageComposeTask(SCHEDULED_OP InOp, FProgramCache& Memory, const OP::ImageComposeArgs& InArgs, int imageCompressionQuality, const Ptr<const Layout>& InLayout)
	{
		Op = InOp;
		m_imageCompressionQuality = imageCompressionQuality;
		Args = InArgs;
		m_layout = InLayout;

		m_base = Memory.GetImage({ Args.base, Op.executionIndex, InOp.executionOptions });
		
		int relBlockIndex = m_layout->FindBlock(Args.blockIndex);

		if (relBlockIndex >= 0)
		{
			m_block = Memory.GetImage({ Args.blockImage, Op.executionIndex, InOp.executionOptions });
			if (Args.mask)
			{
				m_mask = Memory.GetImage({ Args.mask, Op.executionIndex, InOp.executionOptions });
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
	bool CodeRunner::FLoadMeshRomTask::Prepare(CodeRunner* runner, const Model* InModel, bool& bOutFailed )
	{
		MUTABLE_CPUPROFILER_SCOPE(FLoadMeshRomTask_Prepare);

		// This runs in th CodeRunner thread
		bOutFailed = false;

		PROGRAM& program = InModel->GetPrivate()->m_program;

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
						return (pModel == InModel)
							&&
							runner->m_romPendingOps[romInd] > 0;
					});
			}

			uint32 RomSize = program.m_roms[RomIndex].Size;
			check(RomSize>0);

			CodeRunner::ROM_LOAD_OP* op = nullptr;
			for (auto& o : runner->m_romLoadOps)
			{
				if (o.m_romIndex < 0)
				{
					op = &o;
				}
			}

			if (!op)
			{
				runner->m_romLoadOps.Add(CodeRunner::ROM_LOAD_OP());
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
			op->m_streamID = runner->m_pSystem->m_pStreamInterface->BeginReadBlock(InModel, RomId, op->m_streamBuffer.GetData(), RomSize);
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
		runner->RunCode(Op, runner->m_pParams, runner->m_pModel, runner->m_lodMask);

		runner->m_pSystem->m_modelCache.MarkRomUsed(RomIndex, runner->m_pModel);
		--runner->m_romPendingOps[RomIndex];
	}


	//---------------------------------------------------------------------------------------------
	bool CodeRunner::FLoadMeshRomTask::IsComplete(CodeRunner* runner)
	{ 
		PROGRAM& program = runner->m_pModel->GetPrivate()->m_program;
		return program.IsRomLoaded(RomIndex);
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	bool CodeRunner::FLoadImageRomsTask::Prepare(CodeRunner* runner, const Model* InModel, bool& bOutFailed )
	{
		MUTABLE_CPUPROFILER_SCOPE(FLoadImageRomsTask_Prepare);

		// This runs in th CodeRunner thread
		bOutFailed = false;

		PROGRAM& program = InModel->GetPrivate()->m_program;

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
						return (pModel == InModel)
							&&
							runner->m_romPendingOps[romInd] > 0;
					});
			}

			uint32 RomSize = program.m_roms[RomIndex].Size;
			check(RomSize > 0);

			CodeRunner::ROM_LOAD_OP* op = nullptr;
			for (auto& o : runner->m_romLoadOps)
			{
				if (o.m_romIndex < 0)
				{
					op = &o;
				}
			}

			if (!op)
			{
				runner->m_romLoadOps.Add(CodeRunner::ROM_LOAD_OP());
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
			op->m_streamID = runner->m_pSystem->m_pStreamInterface->BeginReadBlock(InModel, RomId, op->m_streamBuffer.GetData(), RomSize);
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
		runner->RunCode(Op, runner->m_pParams, runner->m_pModel, runner->m_lodMask);

		PROGRAM& program = runner->m_pModel->GetPrivate()->m_program;
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
		PROGRAM& program = runner->m_pModel->GetPrivate()->m_program;
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
	TSharedPtr<CodeRunner::FIssuedTask> CodeRunner::IssueOp(SCHEDULED_OP item)
	{
		TSharedPtr<FIssuedTask> Issued;

		PROGRAM& program = m_pModel->GetPrivate()->m_program;

		auto type = program.GetOpType(item.at);

		switch (type)
		{
		case OP_TYPE::ME_CONSTANT:
		{
			OP::MeshConstantArgs args = program.GetOpArgs<OP::MeshConstantArgs>(item.at);
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
			OP::ResourceConstantArgs args = program.GetOpArgs<OP::ResourceConstantArgs>(item.at);
			int32 MipsToSkip = item.executionOptions;
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

		case OP_TYPE::IM_PIXELFORMAT:
		{
			if (item.stage == 1)
			{
				OP::ImagePixelFormatArgs args = program.GetOpArgs<OP::ImagePixelFormatArgs>(item.at);
				Issued = MakeShared<FImagePixelFormatTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_LAYERCOLOUR:
		{
			if (item.stage == 1)
			{
				OP::ImageLayerColourArgs args = program.GetOpArgs<OP::ImageLayerColourArgs>(item.at);
				Issued = MakeShared<FImageLayerColourTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_LAYER:
		{
			if (item.stage == 1)
			{
				OP::ImageLayerArgs args = program.GetOpArgs<OP::ImageLayerArgs>(item.at);
				Issued = MakeShared<FImageLayerTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_MIPMAP:
		{
			if (item.stage == 1)
			{
				OP::ImageMipmapArgs args = program.GetOpArgs<OP::ImageMipmapArgs>(item.at);
				Issued = MakeShared<FImageMipmapTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality);
			}
			break;
		}

		case OP_TYPE::IM_SWIZZLE:
		{
			if (item.stage == 1)
			{
				OP::ImageSwizzleArgs args = program.GetOpArgs<OP::ImageSwizzleArgs>(item.at);
				Issued = MakeShared<FImageSwizzleTask>(item, GetMemory(), args);
			}
			break;
		}

		case OP_TYPE::IM_COMPOSE:
		{
			if (item.stage == 2)
			{
				OP::ImageComposeArgs args = program.GetOpArgs<OP::ImageComposeArgs>(item.at);
				Ptr<const Layout> Layout = m_heapData[(size_t)item.customState].layout;
				Issued = MakeShared<FImageComposeTask>(item, GetMemory(), args, m_pSettings->GetPrivate()->m_imageCompressionQuality, Layout);
			}
			break;
		}

		default:
			break;
		}

		return Issued;
	}


} // namespace mu
