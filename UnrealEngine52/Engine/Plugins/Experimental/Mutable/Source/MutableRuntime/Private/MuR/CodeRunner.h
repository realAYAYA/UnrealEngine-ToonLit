// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "MuR/Image.h"
#include "MuR/Layout.h"
#include "MuR/MutableMemory.h"
#include "MuR/Operations.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/Settings.h"
#include "MuR/System.h"
#include "MuR/SystemPrivate.h"
#include "MuR/Types.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"

// This define could come from MuR/System.h
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	#include "Tasks/Task.h"
#else
#endif


namespace  mu
{
	class Model;
	class Parameters;
	class RangeIndex;


    //---------------------------------------------------------------------------------------------
    //! Code execution of the mutable virtual machine.
    //---------------------------------------------------------------------------------------------
    class CodeRunner : public Base
    {
    public:
		CodeRunner(const SettingsPtrConst&, class System::Private*, 
			const TSharedPtr<const Model>&, const Parameters* pParams,
			OP::ADDRESS at, uint32 lodMask, uint8 executionOptions, FScheduledOp::EType );

    protected:

        //! Settings that may affect the execution of some operations, like image conversion
        //! quality.
        SettingsPtrConst m_pSettings;

        //! Type of data sometimes stored in the code runner heap to pass info between
        //! operation stages.
        struct FScheduledOpData
        {
			union
			{
				struct
				{
					float Bifactor;
					int32 Min, Max;
				} Interpolate;

				struct
				{
					int32 Iterations;
					EImageFormat OriginalBaseFormat;
					bool bBlendOnlyOneMip;
				} MultiLayer;

				struct
				{
					int32 ResultDescAt;
					int32 SourceDescAt;
				} ResizeLike;
			};
            Ptr<RefCounted> Resource;
        };


        Ptr<RangeIndex> BuildCurrentOpRangeIndex( const FScheduledOp&, const Parameters*, const Model*, int32 ParameterIndex );

        void RunCode( FScheduledOp&, const Parameters*, const Model*, uint32 LodMask);

        void RunCode_Conditional( FScheduledOp&, const Model* );
        void RunCode_Switch( FScheduledOp&, const Model* );
        void RunCode_Instance( FScheduledOp&, const Model*, uint32 LodMask );
        void RunCode_InstanceAddResource( FScheduledOp&, const Model*, const Parameters* );
        void RunCode_ConstantResource( FScheduledOp&, const Model* );
        void RunCode_Mesh( FScheduledOp&, const Model* );
        void RunCode_Image( FScheduledOp&, const Parameters*, const Model* );
        void RunCode_Layout( FScheduledOp&, const Model* );
        void RunCode_Bool( FScheduledOp&, const Parameters*, const Model* );
        void RunCode_Int( FScheduledOp&, const Parameters*, const Model* );
        void RunCode_Scalar( FScheduledOp&, const Parameters*, const Model* );        
        void RunCode_String( FScheduledOp&, const Parameters*, const Model* );
        void RunCode_Colour( FScheduledOp&, const Parameters*, const Model* );
        void RunCode_Projector( FScheduledOp&, const Parameters*, const Model* );

		void RunCodeImageDesc(FScheduledOp&, const Parameters*, const Model*, uint32 LodMask);

    public:

		//! Load an external image asynchronously, retuns an event to wait for complition and a cleanup function 
		//! that must be called once the event has completed.
        TTuple<FGraphEventRef, TFunction<void()>> LoadExternalImageAsync(EXTERNAL_IMAGE_ID Id, uint8 MipmapsToSkip, TFunction<void (Ptr<Image>)>& ResultCallback);
 	    mu::FImageDesc GetExternalImageDesc(EXTERNAL_IMAGE_ID Id, uint8 MipmapsToSkip);

    protected:
        //! Heap of intermediate data pushed by some instructions and referred by others.
        //! It is not released until no operations are pending.
		TArray< FScheduledOpData > m_heapData;
		TArray< FImageDesc > m_heapImageDesc;

	public:

		void Run();

		//!
		void GetImageDescResult(FImageDesc& OutDesc);

		//!
		FProgramCache& GetMemory();


		struct FTask
		{
			FTask() {}
			FTask(const FScheduledOp& InOp) : Op(InOp) {}
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0) : Op(InOp) { Deps.Add(InDep0); }
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); }
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1, const FScheduledOp& InDep2) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); Deps.Add(InDep2); }
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1, const FScheduledOp& InDep2, const FScheduledOp& InDep3) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); Deps.Add(InDep2); Deps.Add(InDep3); }
			FTask(const FScheduledOp& InOp, const FScheduledOp& InDep0, const FScheduledOp& InDep1, const FScheduledOp& InDep2, const FScheduledOp& InDep3, const FScheduledOp& InDep4) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); Deps.Add(InDep2); Deps.Add(InDep3); Deps.Add(InDep4); }

			FScheduledOp Op;
			TArray<FCacheAddress, TInlineAllocator<3>> Deps;
		};

		class FIssuedTask
		{
		public:
			FScheduledOp Op;

#ifdef MUTABLE_USE_NEW_TASKGRAPH
			UE::Tasks::FTask Event = {};
#else
			FGraphEventRef Event = nullptr;
#endif

			virtual ~FIssuedTask() {}
			virtual bool Prepare(CodeRunner*, const TSharedPtr<const Model>&, bool& bOutFailed) { bOutFailed = false; return true; }
			virtual void DoWork() {}
			virtual void Complete(CodeRunner*) = 0;
			virtual bool IsComplete(CodeRunner*)
			{ 
				// Event can be null if we forced single-threaded execution.
#ifdef MUTABLE_USE_NEW_TASKGRAPH
				return !Event.IsValid() || Event.IsCompleted(); 
#else
				return !Event.IsValid() || Event->IsComplete();
#endif
			}
		};

		class FLoadMeshRomTask : public CodeRunner::FIssuedTask
		{
		public:
			FLoadMeshRomTask(FScheduledOp InOp, int32 InRomIndex)
			{
				Op = InOp;
				RomIndex = InRomIndex;
			}

			// FIssuedTask interface
			bool Prepare(CodeRunner*, const TSharedPtr<const Model>&, bool& bOutFailed) override;
			void Complete(CodeRunner*) override;
			bool IsComplete(CodeRunner*) override;

		private:
			int32 RomIndex = -1;
		};

		class FLoadImageRomsTask : public CodeRunner::FIssuedTask
		{
		public:
			FLoadImageRomsTask(FScheduledOp InOp, int32 InLODIndexIndex, int32 InLODIndexCount )
			{
				Op = InOp;
				LODIndexIndex = InLODIndexIndex;
				LODIndexCount = InLODIndexCount;
			}

			// FIssuedTask interface
			bool Prepare(CodeRunner*, const TSharedPtr<const Model>&, bool& bOutFailed) override;
			void Complete(CodeRunner*) override;
			bool IsComplete(CodeRunner*) override;

		private:
 			int32 LODIndexIndex = -1;
			int32 LODIndexCount = -1;
		};

		void AddOp(const FScheduledOp& op)
		{
			OpenTasks.Add(op);
			ScheduledStagePerOp[op] = op.Stage + 1;
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0)
		{
			ClosedTasks.Add(FTask(op, dep0));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1,
			const FScheduledOp& dep2)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1,
			const FScheduledOp& dep2,
			const FScheduledOp& dep3)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2, dep3));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
			AddChildren(dep3);
		}

		void AddOp(const FScheduledOp& op,
			const FScheduledOp& dep0,
			const FScheduledOp& dep1,
			const FScheduledOp& dep2,
			const FScheduledOp& dep3,
			const FScheduledOp& dep4)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2, dep3, dep4));
			ScheduledStagePerOp[op] = op.Stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
			AddChildren(dep3);
			AddChildren(dep4);
		}

		template<class RangeType>
		void AddOp(const FScheduledOp& Op, const RangeType& Deps)
		{
			FTask Task(Op);

			for (const FScheduledOp& D : Deps)
			{
				Task.Deps.Add(D);
			}

			ClosedTasks.Add(Task);

			ScheduledStagePerOp[Op] = Op.Stage + 1;

			for (const FScheduledOp& D : Deps)
			{
				AddChildren(D);
			}
		}


	protected:

		//! Stack of pending operations, and the execution stage they are in.
		TArray< FTask > ClosedTasks;
		TArray< FScheduledOp > OpenTasks;
		CodeContainer<uint8> ScheduledStagePerOp;
		TArray< TSharedPtr<FIssuedTask> > IssuedTasks;

	// TODO: protect this.
	public:
		
		/** This flag is turned on when a streaming error or similar happens. Results are not usable.
		* This should only happen in-editor.
		*/
		bool bUnrecoverableError = false;

		System::Private* m_pSystem = nullptr;
		TSharedPtr<const Model> m_pModel = nullptr;
		const Parameters* m_pParams = nullptr;
		uint32 m_lodMask = 0;

		// Async rom loading control
		struct FRomLoadOp
		{
			int32 m_romIndex = 0;
			DATATYPE ConstantType = DT_NONE;
			ModelStreamer::OPERATION_ID m_streamID;
			TArray<uint8> m_streamBuffer;
		};
		TArray<FRomLoadOp> m_romLoadOps;

		//! Count of pending operations for every rom index
		TArray<uint16> m_romPendingOps;

	private:

		inline void AddChildren(const FScheduledOp& dep)
		{
			FCacheAddress at(dep);
			if (dep.At && !GetMemory().IsValid(at))
			{
				if (ScheduledStagePerOp.get(at) <= dep.Stage)
				{
					OpenTasks.Add(dep);
					ScheduledStagePerOp[at] = dep.Stage + 1;
				}
			}

			m_pSystem->m_memory->IncreaseHitCount(at);
		}

		/** Try to create a concurrent task for the given op. Return null if not possible. */
		TSharedPtr<FIssuedTask> IssueOp(FScheduledOp item);

		/** */
		void CompleteRomLoadOp(FRomLoadOp& o);

    };

}
