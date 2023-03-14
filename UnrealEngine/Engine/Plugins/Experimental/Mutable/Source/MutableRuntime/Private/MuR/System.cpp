// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/System.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "MuR/CodeRunner.h"
#include "MuR/CodeVisitor.h"
#include "MuR/InstancePrivate.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableString.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/Parameters.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Serialisation.h"
#include "MuR/SettingsPrivate.h"
#include "MuR/SystemPrivate.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "Trace/Detail/Channel.h"


namespace mu
{
    static_assert( sizeof(vec4f) == 16, "UNEXPECTED_STRUCT_PACKING" );
    static_assert( sizeof(mat4f) == 64, "UNEXPECTED_STRUCT_PACKING" );

    //!
    static std::atomic<mu::Error> s_error(mu::Error::None);


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    Error GetError()
    {
        return s_error;
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    void ClearError()
    {
        s_error = Error::None;
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    System::System( const SettingsPtr& pInSettings )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        SettingsPtr pSettings = pInSettings;

        if ( !pSettings )
        {
            pSettings = new Settings;
        }

        // Choose the implementation
        m_pD = new System::Private( pSettings );
    }


    //---------------------------------------------------------------------------------------------
    System::~System()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        check( m_pD );

        delete m_pD;
        m_pD = nullptr;
    }


    //---------------------------------------------------------------------------------------------
    System::Private* System::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    System::Private::Private( SettingsPtr pSettings )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        m_pSettings = pSettings;
        m_pStreamInterface = nullptr;
        m_pImageParameterGenerator = nullptr;
        m_maxMemory = 0;

        for(uint8 p=0; p< uint8(ProfileMetric::_Count); ++p)
        {
            m_profileMetrics[p]=0;
        }
	
		m_modelCache.m_romBudget = pSettings->GetPrivate()->m_streamingCacheBytes;
	}


    //---------------------------------------------------------------------------------------------
    System::Private::~Private()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        delete m_pStreamInterface;
        m_pStreamInterface = nullptr;

        delete m_pImageParameterGenerator;
        m_pImageParameterGenerator = nullptr;
    }


    //---------------------------------------------------------------------------------------------
    void System::SetStreamingInterface( ModelStreamer *pInterface )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        (void)pInterface;
       
        delete m_pD->m_pStreamInterface;
        m_pD->m_pStreamInterface = pInterface;
    }


    //---------------------------------------------------------------------------------------------
    void System::SetStreamingCache( uint64 bytes )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        m_pD->SetStreamingCache( bytes );
    }


    //---------------------------------------------------------------------------------------------
    void System::SetImageParameterGenerator( ImageParameterGenerator* pInterface )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        delete m_pD->m_pImageParameterGenerator;
        m_pD->m_pImageParameterGenerator = pInterface;
    }


    //---------------------------------------------------------------------------------------------
    void System::SetMemoryLimit( uint32 mem )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        m_pD->m_maxMemory = mem;

        // TODO: Clear cache if we are over the new limit
    }


    //---------------------------------------------------------------------------------------------
    void System::ClearCaches()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        // TODO
    }


    //---------------------------------------------------------------------------------------------
    uint64 System::GetProfileMetric( ProfileMetric m ) const
    {
        size_t metric = size_t(m);

        if (metric>=size_t(ProfileMetric::_Count)) return 0;

        return m_pD->m_profileMetrics[metric];
    }


    //---------------------------------------------------------------------------------------------
    Instance::ID System::NewInstance( const ModelPtrConst& pModel )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(NewInstance);

		Private::FLiveInstance instanceData;
		instanceData.m_instanceID = ++m_pD->m_lastInstanceID;
		instanceData.m_pInstance = nullptr;
		instanceData.m_pModel = pModel;
		instanceData.m_state = -1;
		instanceData.m_memory = MakeShared<FProgramCache>();
		m_pD->m_liveInstances.Add(instanceData);

		m_pD->m_profileMetrics[size_t(System::ProfileMetric::LiveInstanceCount)]++;

		return instanceData.m_instanceID;
	}


    //---------------------------------------------------------------------------------------------
    const Instance* System::BeginUpdate( Instance::ID instanceID,
                                     const ParametersPtrConst& pParams,
                                     int32 stateIndex,
                                     uint32 lodMask )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(SystemBeginUpdate);

		if (!pParams)
		{
			UE_LOG(LogMutableCore, Error, TEXT("Invalid parameters in mutable update."));
			return nullptr;
		}

		Private::FLiveInstance* pLiveInstance = m_pD->FindLiveInstance(instanceID);
		if (!pLiveInstance)
		{
			UE_LOG(LogMutableCore, Error, TEXT("Invalid instance id in mutable update."));
			return nullptr;
		}

		m_pD->m_memory = pLiveInstance->m_memory;

		PROGRAM& program = pLiveInstance->m_pModel->GetPrivate()->m_program;

		bool validState = stateIndex >= 0 && stateIndex < (int)program.m_states.Num();
		if (!validState)
		{
			UE_LOG(LogMutableCore, Error, TEXT("Invalid state in mutable update."));
			return nullptr;
		}

		// This may free resources that allow us to use less memory.
		pLiveInstance->m_pInstance = nullptr;

		bool fullBuild = (stateIndex != pLiveInstance->m_state);

		pLiveInstance->m_state = stateIndex;

		// If we changed parameters that are not in this state, we need to rebuild all.
		if (!fullBuild)
		{
			fullBuild = m_pD->CheckUpdatedParameters(pLiveInstance, pParams.get(), pLiveInstance->m_updatedParameters);
		}

		// Remove cached data
		pLiveInstance->m_memory->ClearCacheLayer0();
		if (fullBuild)
		{
			pLiveInstance->m_memory->ClearCacheLayer1();
		}

		OP::ADDRESS rootAt = pLiveInstance->m_pModel->GetPrivate()->m_program.m_states[stateIndex].m_root;

		m_pD->PrepareCache(pLiveInstance->m_pModel, stateIndex);
		pLiveInstance->m_pOldParameters = pParams->Clone();

		m_pD->RunCode(pLiveInstance->m_pModel.get(), pParams.get(), rootAt, lodMask);

		InstancePtrConst pResult = pLiveInstance->m_memory->GetInstance(CACHE_ADDRESS(rootAt, 0, 0));

		pLiveInstance->m_pInstance = pResult;
		if (pResult)
		{
			pResult->GetPrivate()->m_id = pLiveInstance->m_instanceID;
		}

		m_pD->m_profileMetrics[size_t(System::ProfileMetric::InstanceUpdateCount)]++;

		m_pD->m_memory = nullptr;

		return pResult.get();
	}


	//---------------------------------------------------------------------------------------------
	ImagePtrConst System::GetImage(Instance::ID instanceID, RESOURCE_ID imageId, int32 MipsToSkip)
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(SystemGetImage);

		ImagePtrConst pResult;

		// Find the live instance
		Private::FLiveInstance* pLiveInstance = m_pD->FindLiveInstance(instanceID);
		check(pLiveInstance);
		m_pD->m_memory = pLiveInstance->m_memory;

		// Find the resource id in the model's resource cache
		for (const Model::Private::RESOURCE_KEY& res : pLiveInstance->m_pModel->GetPrivate()->m_generatedResources)
		{
			if (res.m_id == imageId)
			{
				pResult = m_pD->BuildImage(pLiveInstance->m_pModel,
					pLiveInstance->m_pOldParameters.get(),
					res.m_rootAddress, MipsToSkip);

				// We always need ot return something valid.
				if (!pResult)
				{
					pResult = new mu::Image(16, 16, 1, EImageFormat::IF_RGBA_UBYTE);
				}

				pResult->m_internalId = imageId;

				break;
			}
		}

		m_pD->m_memory = nullptr;

		return pResult;
	}


	// Temporarily make the Image DescCache clear at every image because otherwise it makes some textures 
	// not evaluate their layout and be of size 0 and 0 lods, making them incorrectly evaluate MipsToSkip
	static TAutoConsoleVariable<int32> CVarClearImageDescCache(
		TEXT("mutable.ClearImageDescCache"),
		1,
		TEXT("If different than 0, clear the image desc cache at every image."),
		ECVF_Scalability);


	//---------------------------------------------------------------------------------------------
	void System::GetImageDesc(Instance::ID instanceID, RESOURCE_ID imageId, FImageDesc& OutDesc)
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(SystemGetImageDesc);

		OutDesc = FImageDesc();

		// Find the live instance
		Private::FLiveInstance* pLiveInstance = m_pD->FindLiveInstance(instanceID);
		check(pLiveInstance);
		m_pD->m_memory = pLiveInstance->m_memory;

		// Find the resource id in the model's resource cache
		for (const Model::Private::RESOURCE_KEY& res : pLiveInstance->m_pModel->GetPrivate()->m_generatedResources)
		{
			if (res.m_id == imageId)
			{
				const mu::Model* Model = pLiveInstance->m_pModel.get();
				const mu::PROGRAM& program = Model->GetPrivate()->m_program;

				int32 VarValue = CVarClearImageDescCache.GetValueOnAnyThread();
				if (VarValue != 0)
				{
					m_pD->m_memory->m_descCache.Reset();
				}

				m_pD->m_memory->m_descCache.SetNum(program.m_opAddress.Num());

				OP::ADDRESS at = res.m_rootAddress;
				mu::OP_TYPE opType = program.GetOpType(at);
				if (GetOpDataType(opType) == DT_IMAGE)
				{
					int8 executionOptions = 0;
					CodeRunner Runner(m_pD->m_pSettings, m_pD, Model, pLiveInstance->m_pOldParameters.get(), at, System::AllLODs, executionOptions, SCHEDULED_OP::EType::ImageDesc);
					Runner.Run();
					Runner.GetImageDescResult(OutDesc);
				}

				break;
			}
		}

		m_pD->m_memory = nullptr;
	}


    //---------------------------------------------------------------------------------------------
    MeshPtrConst System::GetMesh( Instance::ID instanceID,
                                   RESOURCE_ID meshId )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(SystemGetMesh);

		MeshPtrConst pResult;

		// Find the live instance
		Private::FLiveInstance* pLiveInstance = m_pD->FindLiveInstance(instanceID);
		check(pLiveInstance);
		m_pD->m_memory = pLiveInstance->m_memory;

		// Find the resource id in the model's resource cache
		for (const Model::Private::RESOURCE_KEY& res : pLiveInstance->m_pModel->GetPrivate()->m_generatedResources)
		{
			if (res.m_id == meshId)
			{
				pResult = m_pD->BuildMesh(pLiveInstance->m_pModel,
					pLiveInstance->m_pOldParameters.get(),
					res.m_rootAddress);

				// If the mesh is null it means empty, but we still need to return a valid one
				if (!pResult)
				{
					pResult = new Mesh();
				}

				pResult->m_internalId = meshId;

				break;
			}
		}

		m_pD->m_memory = nullptr;
		return pResult;
	}


    //---------------------------------------------------------------------------------------------
    void System::EndUpdate( Instance::ID instanceID )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(EndUpdate);

		// Reduce the cache until it fits the limit.
		uint64 totalMemory = m_pD->m_modelCache.EnsureCacheBelowBudget(0, [](const Model*, int) {return false;});
		m_pD->m_profileMetrics[size_t(System::ProfileMetric::StreamingCacheBytes)] = totalMemory;


		Private::FLiveInstance* pLiveInstance = m_pD->FindLiveInstance(instanceID);
		if (pLiveInstance)
		{
			pLiveInstance->m_pInstance = nullptr;
			pLiveInstance->m_memory->ClearCacheLayer1();
		}
	}


    //---------------------------------------------------------------------------------------------
    void System::ReleaseInstance( Instance::ID instanceID )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(ReleaseInstance);

		int Removed = m_pD->m_liveInstances.RemoveAllSwap(
			[instanceID](const Private::FLiveInstance& Instance)
			{
				return (Instance.m_instanceID == instanceID);
			});
		m_pD->m_profileMetrics[int32(System::ProfileMetric::LiveInstanceCount)]-=Removed;
	}


    //---------------------------------------------------------------------------------------------
    ImagePtrConst System::BuildParameterAdditionalImage( const ModelPtrConst& pModel,
                                                         const ParametersPtrConst& pParams,
                                                         int parameter,
                                                         int image )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(BuildParameterAdditionalImage);

		check(parameter >= 0
			&&
			parameter < (int)pModel->GetPrivate()->m_program.m_parameters.Num());

		const PARAMETER_DESC& param =
			pModel->GetPrivate()->m_program.m_parameters[parameter];
		check(image >= 0 && image < (int)param.m_descImages.Num());

		OP::ADDRESS imageAddress = param.m_descImages[image];

		ImagePtrConst pResult;

		if (imageAddress)
		{
			m_pD->BeginBuild(pModel);
			pResult = m_pD->BuildImage( pModel, pParams.get(), imageAddress, 0 );
			m_pD->EndBuild();
		}

		return pResult;
	}


    //---------------------------------------------------------------------------------------------
    class RelevantParameterVisitor : public UniqueDiscreteCoveredCodeVisitor<>
    {
    public:

        RelevantParameterVisitor
            (
                System::Private* pSystem,
                const Ptr<const Model>& pModel,
                const Ptr<const Parameters>& pParams,
                bool* pFlags
            )
            : UniqueDiscreteCoveredCodeVisitor<>( pSystem, pModel, pParams, 0xffffffff )
        {
            m_pFlags = pFlags;

            memset( pFlags, 0, sizeof(bool)*pParams->GetCount() );

            OP::ADDRESS at = pModel->GetPrivate()->m_program.m_states[0].m_root;

            pSystem->BeginBuild(pModel);

            Run( at );

			pSystem->EndBuild();
        }


        bool Visit( OP::ADDRESS at, PROGRAM& program ) override
        {
            switch ( program.GetOpType(at) )
            {
            case OP_TYPE::BO_PARAMETER:
            case OP_TYPE::NU_PARAMETER:
            case OP_TYPE::SC_PARAMETER:
            case OP_TYPE::CO_PARAMETER:
            case OP_TYPE::PR_PARAMETER:
            case OP_TYPE::IM_PARAMETER:
            {
				OP::ParameterArgs args = program.GetOpArgs<OP::ParameterArgs>(at);
				OP::ADDRESS param = args.variable;
                m_pFlags[param] = true;
                break;
            }

            default:
                break;

            }

            return UniqueDiscreteCoveredCodeVisitor<>::Visit( at, program );
        }

    private:

        //! Non-owned result buffer
        bool* m_pFlags;
    };


    //---------------------------------------------------------------------------------------------
    void System::GetParameterRelevancy( const ModelPtrConst& pModel,
                                        const ParametersPtrConst& pParameters,
                                        bool* pFlags )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        RelevantParameterVisitor visitor( m_pD, pModel, pParameters, pFlags );
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    bool System::Private::CheckUpdatedParameters( const FLiveInstance* pLiveInstance,
                                 const Ptr<const Parameters>& pParams,
                                 uint64& updatedParameters )
    {
        bool fullBuild = false;

		if (!pLiveInstance->m_pOldParameters)
		{
			updatedParameters = 0xffffffff;
			return true;
		}

        // check what parameters have changed
        updatedParameters = 0;
        const PROGRAM& program = pLiveInstance->m_pModel->GetPrivate()->m_program;
        const TArray<int>& runtimeParams = program.m_states[ pLiveInstance->m_state ].m_runtimeParameters;

        check( pParams->GetCount() == (int)program.m_parameters.Num() );
        check( !pLiveInstance->m_pOldParameters
                        ||
                        pParams->GetCount() == pLiveInstance->m_pOldParameters->GetCount() );

        for ( int p=0; p<(int)program.m_parameters.Num() && !fullBuild; ++p )
        {
            bool isRuntime = runtimeParams.Contains( p );
            bool changed = !pParams->HasSameValue( p, pLiveInstance->m_pOldParameters, p );

            if (changed && isRuntime)
            {
				uint64 runtimeIndex = runtimeParams.IndexOfByKey(p);
                updatedParameters |= uint64(1) << runtimeIndex;
            }
            else if (changed)
            {
                // A non-runtime parameter has changed, we need a full build.
                // TODO: report, or log somehow.
                fullBuild = true;
                updatedParameters = 0xffffffffffffffff;
            }
        }

        return fullBuild;
    }


	//---------------------------------------------------------------------------------------------
	void System::Private::SetStreamingCache(uint64 bytes)
	{
		m_modelCache.m_romBudget = bytes;
		m_modelCache.EnsureCacheBelowBudget(0);
	}


	//---------------------------------------------------------------------------------------------
	void System::Private::BeginBuild(const Ptr<const Model>& pModel)
	{
		// We don't have a FLiveInstance, let's create the memory
		// \TODO: There is no clear moment to remove this... EndBuild?
		m_memory = MakeShared<FProgramCache>();
		m_memory->Init(pModel->GetPrivate()->m_program.m_opAddress.Num());

		PrepareCache(pModel, -1);
	}


	//---------------------------------------------------------------------------------------------
	void System::Private::EndBuild()
	{
		m_memory = nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void System::Private::RunCode(const Ptr<const Model>& InModel,
		const Parameters* InParameters, OP::ADDRESS InCodeRoot, uint32 InLODs, uint8 executionOptions)
	{
		CodeRunner Runner(m_pSettings, this, InModel.get(), InParameters, InCodeRoot, InLODs, 
			executionOptions, SCHEDULED_OP::EType::Full);
		Runner.Run();
		bUnrecoverableError = Runner.bUnrecoverableError;
	}


	//---------------------------------------------------------------------------------------------
	bool System::Private::BuildBool(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at)
	{
		RunCode(pModel.get(), pParams, at);
		if (bUnrecoverableError)
		{
			return false;
		}
		return m_memory->GetBool(CACHE_ADDRESS(at, 0, 0));
	}


	//---------------------------------------------------------------------------------------------
	float System::Private::BuildScalar(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at)
	{
		RunCode(pModel.get(), pParams, at);
		if (bUnrecoverableError)
		{
			return 0.0f;
		}
		return m_memory->GetScalar(CACHE_ADDRESS(at, 0, 0));
	}


	//---------------------------------------------------------------------------------------------
	int System::Private::BuildInt(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at)
	{
		RunCode(pModel.get(), pParams, at);
		if (bUnrecoverableError)
		{
			return 0;
		}

		return m_memory->GetInt(CACHE_ADDRESS(at, 0, 0));
	}


	//---------------------------------------------------------------------------------------------
	void System::Private::BuildColour(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at,
		float* pR,
		float* pG,
		float* pB)
	{
		vec4f col;

		mu::OP_TYPE opType = pModel->GetPrivate()->m_program.GetOpType(at);
		if (GetOpDataType(opType) == DT_COLOUR)
		{
			RunCode(pModel.get(), pParams, at);
			if (bUnrecoverableError)
			{
				if (pR) *pR = 0.0f;
				if (pG) *pG = 0.0f;
				if (pB) *pB = 0.0f;
			}

			col = m_memory->GetColour(CACHE_ADDRESS(at, 0, 0));
		}

		if (pR) *pR = col[0];
		if (pG) *pG = col[1];
		if (pB) *pB = col[2];
	}

	
	//---------------------------------------------------------------------------------------------
	Ptr<const Projector> System::Private::BuildProjector(const Ptr<const Model>& pModel, const Parameters* pParams, OP::ADDRESS at)
	{
    	RunCode(pModel.get(), pParams, at);
		if (bUnrecoverableError)
		{
			return nullptr;
		}
    	return m_memory->GetProjector(CACHE_ADDRESS(at, 0, 0));
	}

	
	//---------------------------------------------------------------------------------------------
	Ptr<const Image> System::Private::BuildImage(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at, int MipsToSkip)
	{
		mu::OP_TYPE opType = pModel->GetPrivate()->m_program.GetOpType(at);
		if (GetOpDataType(opType) == DT_IMAGE)
		{
			m_memory->ClearCacheLayer0();
			RunCode(pModel.get(), pParams, at, System::AllLODs, uint8(MipsToSkip));
			if (bUnrecoverableError)
			{
				return nullptr;
			}
			ImagePtrConst Result = m_memory->GetImage(CACHE_ADDRESS(at, 0, MipsToSkip));
			return Result;
		}

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
	MeshPtrConst System::Private::BuildMesh(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at)
	{
		mu::OP_TYPE opType = pModel->GetPrivate()->m_program.GetOpType(at);
		if (GetOpDataType(opType) == DT_MESH)
		{
			m_memory->ClearCacheLayer0();
			RunCode(pModel.get(), pParams, at);
			if (bUnrecoverableError)
			{
				return nullptr;
			}
			MeshPtrConst pResult = m_memory->GetMesh(CACHE_ADDRESS(at, 0, 0));
			return pResult;
		}

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
	LayoutPtrConst System::Private::BuildLayout(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at)
	{
		LayoutPtrConst  pResult;

		if (pModel->GetPrivate()->m_program.m_states[0].m_root)
		{
			mu::OP_TYPE opType = pModel->GetPrivate()->m_program.GetOpType(at);
			if (GetOpDataType(opType) == DT_LAYOUT)
			{
				RunCode(pModel.get(), pParams, at);
				if (bUnrecoverableError)
				{
					return nullptr;
				}
				pResult = m_memory->GetLayout(CACHE_ADDRESS(at, 0, 0));
			}
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	Ptr<const String> System::Private::BuildString(const Ptr<const Model>& pModel,
		const Parameters* pParams,
		OP::ADDRESS at)
	{
		Ptr<const String> pResult;

		if (pModel->GetPrivate()->m_program.m_states[0].m_root)
		{
			mu::OP_TYPE opType = pModel->GetPrivate()->m_program.GetOpType(at);
			if (GetOpDataType(opType) == DT_STRING)
			{
				RunCode(pModel.get(), pParams, at);
				if (bUnrecoverableError)
				{
					return nullptr;
				}
				pResult = m_memory->GetString(CACHE_ADDRESS(at, 0, 0));
			}
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void System::Private::PrepareCache(const Ptr<const Model>& pModel, int state)
	{
		MUTABLE_CPUPROFILER_SCOPE(PrepareCache);

		PROGRAM& program = pModel->GetPrivate()->m_program;
		size_t opCount = program.m_opAddress.Num();
		m_memory->m_opHitCount.clear();
		m_memory->Init(opCount);

		// Mark the resources that have to be cached to update the instance in this state
		if (state >= 0)
		{
			const PROGRAM::STATE& s = program.m_states[state];
			for (uint32 a : s.m_updateCache)
			{
				m_memory->SetForceCached(a);
			}
		}
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
	FModelCache::FModelCacheEntry& FModelCache::GetModelCache( const Model* m )
    {
        check(m);

        for(FModelCacheEntry& c:m_cachePerModel)
        {
            mu::Ptr<mu::Model> pCandidate = c.m_pModel.Pin();
            if (pCandidate)
            {
                if (pCandidate==m)
                {
                    return c;
                }
            }
            else
            {
                // Free stray data. TODO: remove vector entry.
                c.m_romWeight.Empty();
            }
        }

        // Not found. Add new
		FModelCacheEntry n;
        n.m_pModel = WeakPtr<Model>(m);
        m_cachePerModel.Add(n);
        return m_cachePerModel.Last();
    }


    //---------------------------------------------------------------------------------------------
    uint64 FModelCache::EnsureCacheBelowBudget( uint64 additionalMemory,
												TFunctionRef<bool(const Model*,int)> isRomLockedFunc )
    {

		uint64 totalMemory = 0;
        for (FModelCacheEntry& m : m_cachePerModel)
        {
			mu::Ptr<mu::Model> pCacheModel = m.m_pModel.Pin();
            if (pCacheModel)
            {
				mu::PROGRAM& program = pCacheModel->GetPrivate()->m_program;
				for (int32 RomIndex = 0; RomIndex < program.m_roms.Num(); ++RomIndex)
				{
					if (program.IsRomLoaded(RomIndex))
					{
						totalMemory += program.m_roms[RomIndex].Size;
					}
				}
			}
        }

        if (totalMemory>0)
        {
            totalMemory += additionalMemory;

            bool finished = totalMemory < m_romBudget;
            while (!finished)
            {
                ModelPtr lowestPriorityModel;
                int32 lowestPriorityRom = -1;
                float lowestPriority = 0.0f;
                for (FModelCacheEntry& modelCache : m_cachePerModel)
                {
					mu::Ptr<mu::Model> pCacheModel = modelCache.m_pModel.Pin();
                    if (pCacheModel)
                    {
						mu::PROGRAM& program = pCacheModel->GetPrivate()->m_program;
                        check( modelCache.m_romWeight.Num() == program.m_roms.Num());

                        for (int32 RomIndex=0; RomIndex <program.m_roms.Num(); ++RomIndex)
                        {
							const FRomData& Rom = program.m_roms[RomIndex];
							bool bIsLoaded = program.IsRomLoaded(RomIndex);

                            if (bIsLoaded
                                &&
                                (!isRomLockedFunc(pCacheModel.get(),RomIndex)) )
                            {
                                constexpr float factorWeight = 100.0f;
                                constexpr float factorTime = -1.0f;
                                float priority =
                                        factorWeight * float(modelCache.m_romWeight[RomIndex].Key)
                                        +
                                        factorTime * float((m_romTick-modelCache.m_romWeight[RomIndex].Value));

                                if (lowestPriorityRom<0 || priority<lowestPriority)
                                {
                                    lowestPriorityRom = RomIndex;
                                    lowestPriority = priority;
                                    lowestPriorityModel = pCacheModel;
                                }
                            }
                        }
                    }
                }

                if (lowestPriorityRom<0)
                {
                    // None found
                    finished = true;
                    UE_LOG(LogMutableCore,Log, TEXT("EnsureCacheBelowBudget failed to free all the necessary memory."));
                }
                else
                {
                    //UE_LOG(LogMutableCore,Log, "Unloading rom because of memory budget: %d.", lowestPriorityRom);
					const FRomData& Rom = lowestPriorityModel->GetPrivate()->m_program.m_roms[lowestPriorityRom];
                    lowestPriorityModel->GetPrivate()->m_program.UnloadRom(lowestPriorityRom);
                    totalMemory -= lowestPriorityModel->GetPrivate()->m_program.m_roms[lowestPriorityRom].Size;
                    finished = totalMemory < m_romBudget;
                }
            }
        }

        return totalMemory;
    }


    //---------------------------------------------------------------------------------------------
    void FModelCache::MarkRomUsed( int romIndex, const Model* pModel )
    {
        check(pModel);

        PROGRAM& program = pModel->GetPrivate()->m_program;

        // If budget is zero, we don't unload anything here, and we assume it is managed
        // somewhere else.
        if ( !m_romBudget )
        {
            return;
        }

        ++m_romTick;

        // Update current cache
        {
			FModelCacheEntry& modelCache = GetModelCache(pModel);

            while (modelCache.m_romWeight.Num()<program.m_roms.Num())
            {
				modelCache.m_romWeight.Add({ 0,0 });
            }

            modelCache.m_romWeight[romIndex].Key++;
            modelCache.m_romWeight[romIndex].Value = m_romTick;
        }
    }


    //---------------------------------------------------------------------------------------------
    void FModelCache::UpdateForLoad( int romIndex, const Model* pModel,
                                     TFunctionRef<bool(const Model*,int)> isRomLockedFunc )
    {
		MarkRomUsed( romIndex, pModel);

		PROGRAM& program = pModel->GetPrivate()->m_program;
        EnsureCacheBelowBudget(program.m_roms[romIndex].Size, isRomLockedFunc);
    }

}
