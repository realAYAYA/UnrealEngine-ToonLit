// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "MuR/Image.h"
#include "MuR/Layout.h"
#include "MuR/MemoryPrivate.h"
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
			const Model* pModel, const Parameters* pParams,
			OP::ADDRESS at, uint32 lodMask, uint8 executionOptions, SCHEDULED_OP::EType );

    protected:

        //! Settings that may affect the execution of some operations, like image conversion
        //! quality.
        SettingsPtrConst m_pSettings;

        //! Type of data sometimes stored in the code runner heap to pass info between
        //! operation stages.
        struct SCHEDULED_OP_DATA
        {
            float bifactor=0.0f;
            int32 min=0, max=0;
            Ptr<const Layout> layout;
        };


        Ptr<RangeIndex> BuildCurrentOpRangeIndex( const SCHEDULED_OP& item,
                                                const Parameters* pParams,
                                                const Model* pModel,
                                                int parameterIndex );


        void RunCode( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel, uint32_t lodMask);

        void RunCode_Conditional( SCHEDULED_OP& item, const Model* pModel );
        void RunCode_Switch( SCHEDULED_OP& item, const Model* pModel );
        void RunCode_Instance( SCHEDULED_OP& item, const Model* pModel, uint32_t lodMask );
        void RunCode_InstanceAddResource( SCHEDULED_OP& item, const Model* pModel, const Parameters* pParams );
        void RunCode_ConstantResource( SCHEDULED_OP& item, const Model* pModel );
        void RunCode_Mesh( SCHEDULED_OP& item, const Model* pModel );
        void RunCode_Image( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel );
        void RunCode_Layout( SCHEDULED_OP& item, const Model* pModel );
        void RunCode_Bool( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel );
        void RunCode_Int( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel );
        void RunCode_Scalar( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel );        
        void RunCode_String( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel );
        void RunCode_Colour( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel );
        void RunCode_Projector( SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel );

		void RunCodeImageDesc(SCHEDULED_OP& item, const Parameters* pParams, const Model* pModel, uint32_t lodMask);


        ImagePtr LoadExternalImage( EXTERNAL_IMAGE_ID id );

        //! Heap of intermediate data pushed by some instructions and referred by others.
        //! It is not released until no operations are pending.
		TArray< SCHEDULED_OP_DATA > m_heapData;
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
			FTask(const SCHEDULED_OP& InOp) : Op(InOp) {}
			FTask(const SCHEDULED_OP& InOp, const SCHEDULED_OP& InDep0) : Op(InOp) { Deps.Add(InDep0); }
			FTask(const SCHEDULED_OP& InOp, const SCHEDULED_OP& InDep0, const SCHEDULED_OP& InDep1) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); }
			FTask(const SCHEDULED_OP& InOp, const SCHEDULED_OP& InDep0, const SCHEDULED_OP& InDep1, const SCHEDULED_OP& InDep2) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); Deps.Add(InDep2); }
			FTask(const SCHEDULED_OP& InOp, const SCHEDULED_OP& InDep0, const SCHEDULED_OP& InDep1, const SCHEDULED_OP& InDep2, const SCHEDULED_OP& InDep3) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); Deps.Add(InDep2); Deps.Add(InDep3); }
			FTask(const SCHEDULED_OP& InOp, const SCHEDULED_OP& InDep0, const SCHEDULED_OP& InDep1, const SCHEDULED_OP& InDep2, const SCHEDULED_OP& InDep3, const SCHEDULED_OP& InDep4) : Op(InOp) { Deps.Add(InDep0); Deps.Add(InDep1); Deps.Add(InDep2); Deps.Add(InDep3); Deps.Add(InDep4); }

			SCHEDULED_OP Op;
			TArray<CACHE_ADDRESS, TInlineAllocator<3>> Deps;
		};

		class FIssuedTask
		{
		public:
			SCHEDULED_OP Op;

#ifdef MUTABLE_USE_NEW_TASKGRAPH
			UE::Tasks::FTask Event = {};
#else
			FGraphEventRef Event = nullptr;
#endif

			virtual ~FIssuedTask() {}
			virtual bool Prepare(CodeRunner*, const Model*, bool& bOutFailed) { bOutFailed = false; return true; }
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
			FLoadMeshRomTask(SCHEDULED_OP InOp, int32 InRomIndex)
			{
				Op = InOp;
				RomIndex = InRomIndex;
			}

			// FIssuedTask interface
			bool Prepare(CodeRunner*, const Model*, bool& bOutFailed) override;
			void Complete(CodeRunner*) override;
			bool IsComplete(CodeRunner*) override;

		private:
			int32 RomIndex = -1;
		};

		class FLoadImageRomsTask : public CodeRunner::FIssuedTask
		{
		public:
			FLoadImageRomsTask(SCHEDULED_OP InOp, int32 InLODIndexIndex, int32 InLODIndexCount )
			{
				Op = InOp;
				LODIndexIndex = InLODIndexIndex;
				LODIndexCount = InLODIndexCount;
			}

			// FIssuedTask interface
			bool Prepare(CodeRunner*, const Model*, bool& bOutFailed) override;
			void Complete(CodeRunner*) override;
			bool IsComplete(CodeRunner*) override;

		private:
 			int32 LODIndexIndex = -1;
			int32 LODIndexCount = -1;
		};

		void AddOp(const SCHEDULED_OP& op)
		{
			OpenTasks.Add(op);
			ScheduledStagePerOp[op] = op.stage + 1;
		}

		void AddOp(const SCHEDULED_OP& op,
			const SCHEDULED_OP& dep0)
		{
			ClosedTasks.Add(FTask(op, dep0));
			ScheduledStagePerOp[op] = op.stage + 1;
			AddChildren(dep0);
		}

		void AddOp(const SCHEDULED_OP& op,
			const SCHEDULED_OP& dep0,
			const SCHEDULED_OP& dep1)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1));
			ScheduledStagePerOp[op] = op.stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
		}

		void AddOp(const SCHEDULED_OP& op,
			const SCHEDULED_OP& dep0,
			const SCHEDULED_OP& dep1,
			const SCHEDULED_OP& dep2)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2));
			ScheduledStagePerOp[op] = op.stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
		}

		void AddOp(const SCHEDULED_OP& op,
			const SCHEDULED_OP& dep0,
			const SCHEDULED_OP& dep1,
			const SCHEDULED_OP& dep2,
			const SCHEDULED_OP& dep3)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2, dep3));
			ScheduledStagePerOp[op] = op.stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
			AddChildren(dep3);
		}

		void AddOp(const SCHEDULED_OP& op,
			const SCHEDULED_OP& dep0,
			const SCHEDULED_OP& dep1,
			const SCHEDULED_OP& dep2,
			const SCHEDULED_OP& dep3,
			const SCHEDULED_OP& dep4)
		{
			ClosedTasks.Add(FTask(op, dep0, dep1, dep2, dep3, dep4));
			ScheduledStagePerOp[op] = op.stage + 1;
			AddChildren(dep0);
			AddChildren(dep1);
			AddChildren(dep2);
			AddChildren(dep3);
			AddChildren(dep4);
		}

		void AddOp(const SCHEDULED_OP& op, const TArray<SCHEDULED_OP>& deps)
		{
			FTask Task(op);
			for (const auto& d : deps)
			{
				Task.Deps.Add(d);
			}
			ClosedTasks.Add(Task);
			for (const auto& d : deps)
			{
				AddChildren(d);
			}
		}

		template<class RangeType>
		void AddOp(const SCHEDULED_OP& Op, const RangeType& Deps)
		{
			FTask Task(Op);

			for (const SCHEDULED_OP& D : Deps)
			{
				Task.Deps.Add(D);
			}

			ClosedTasks.Add(Task);

			ScheduledStagePerOp[Op] = Op.stage + 1;

			for (const SCHEDULED_OP& D : Deps)
			{
				AddChildren(D);
			}
		}


	protected:

		//! Stack of pending operations, and the execution stage they are in.
		TArray< FTask > ClosedTasks;
		TArray< SCHEDULED_OP > OpenTasks;
		CodeContainer<uint8> ScheduledStagePerOp;
		TArray< TSharedPtr<FIssuedTask> > IssuedTasks;

	// TODO: protect this.
	public:
		
		/** This flag is turned on when a streaming error or similar happens. Results are not usable.
		* This should only happen in-editor.
		*/
		bool bUnrecoverableError = false;

		System::Private* m_pSystem = nullptr;
		const Model* m_pModel = nullptr;
		const Parameters* m_pParams = nullptr;
		uint32 m_lodMask = 0;

		// Async rom loading control
		struct ROM_LOAD_OP
		{
			int32 m_romIndex = 0;
			DATATYPE ConstantType = DT_NONE;
			ModelStreamer::OPERATION_ID m_streamID;
			TArray<uint8> m_streamBuffer;
		};
		TArray<ROM_LOAD_OP> m_romLoadOps;

		//! Count of pending operations for every rom index
		TArray<uint16> m_romPendingOps;

	private:

		inline void AddChildren(const SCHEDULED_OP& dep)
		{
			CACHE_ADDRESS at(dep);
			if (dep.at && !GetMemory().IsValid(at))
			{
				if (ScheduledStagePerOp.get(at) <= dep.stage)
				{
					OpenTasks.Add(dep);
					ScheduledStagePerOp[at] = dep.stage + 1;
				}
			}

			m_pSystem->m_memory->IncreaseHitCount(at);
		}

		/** Try to create a concurrent task for the given op. Return null if not possible. */
		TSharedPtr<FIssuedTask> IssueOp(SCHEDULED_OP item);

		/** */
		void CompleteRomLoadOp(ROM_LOAD_OP& o);

    };

}
