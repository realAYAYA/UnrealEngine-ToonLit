// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Model.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/System.h"
#include "MuR/Types.h"
#include "Templates/Tuple.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    void FProgram::Check()
    {
//    #ifdef MUTABLE_DEBUG
//        // Process all from root
//        if (m_states.size())
//        {
//            // Super debug: enable in case of emergencies.
////            InvariantCodeVisitor invariantVisitor;
////            invariantVisitor.Apply( *this, m_states[0].m_root );
//        }

//        for ( size_t i=0; i<m_code.size(); ++i )
//        {
//            if ( m_code[i].type == OP_TYPE::IM_PIXELFORMAT )
//            {
//                check( m_code[i].args.ImagePixelFormat.format != IF_NONE );
//                check( m_code[i].args.ImagePixelFormat.source != i );
//            }
//    //
//    //            if ( m_code[i].type == OP_TYPE::IM_MIPMAP )
//    //            {
//    //                check( m_code[i].args.ImageMipmap.blockLevels != 0 );
//    //            }

//            if ( m_code[i].type == OP_TYPE::ME_APPLYLAYOUT )
//            {
//                check( m_code[i].args.MeshApplyLayout.mesh != 0 );
//                check( m_code[i].args.MeshApplyLayout.mesh != i );
//            }

//            if ( m_code[i].type == OP_TYPE::ME_MORPH2 )
//            {
//                for (int j=0;j<MUTABLE_OP_MAX_MORPH2_TARGETS;++j)
//                {
//                    auto target = m_code[i].args.MeshMorph2.targets[j];
//                    check( m_code[target].type!=OP_TYPE::ME_APPLYLAYOUT );
//                }
//            }

//            if ( m_code[i].type == OP_TYPE::ME_CONDITIONAL ||
//                 m_code[i].type == OP_TYPE::IN_CONDITIONAL ||
//                 m_code[i].type == OP_TYPE::IM_CONDITIONAL ||
//                 m_code[i].type == OP_TYPE::LA_CONDITIONAL )
//            {
//                for (int j=0;j<MUTABLE_OP_MAX_MORPH2_TARGETS;++j)
//                {
//                    check( m_code[i].args.Conditional.condition!=i );
//                    check( m_code[i].args.Conditional.yes!=i );
//                    check( m_code[i].args.Conditional.no!=i );
//                }
//            }

//        }
//    #endif
    }


    //---------------------------------------------------------------------------------------------
    void FProgram::LogHistogram() const
    {
#if 0
        uint64 countPerType[(int)OP_TYPE::COUNT];
        mutable_memset(countPerType,0,sizeof(countPerType));

        for ( const auto& o: m_opAddress )
        {
            auto type = GetOpType(o);
            countPerType[(int)type]++;
        }

		TArray< TPair<uint64,OP_TYPE> > sorted((int)OP_TYPE::COUNT);
        for (int i=0; i<(int)OP_TYPE::COUNT; ++i)
        {
            sorted[i].second = (OP_TYPE)i;
            sorted[i].first = countPerType[i];
        }

        std::sort(sorted.begin(),sorted.end(), []( const pair<uint64,OP_TYPE>& a, const pair<uint64,OP_TYPE>& b )
        {
            return a.first>b.first;
        });

        UE_LOG(LogMutableCore,Log, TEXT("Op histogram (%llu ops):"), m_opAddress.Num());
        for(int i=0; i<8; ++i)
        {
            float p = sorted[i].first/float(m_opAddress.Num())*100.0f;
            UE_LOG(LogMutableCore,Log, TEXT("  %3.2f%% : %d"), p, (int)sorted[i].second );
        }
#endif
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    Model::Model()
    {
        m_pD = new Private();
    }


    //---------------------------------------------------------------------------------------------
    Model::~Model()
    {
		MUTABLE_CPUPROFILER_SCOPE(ModelDestructor);

        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    void Model::Serialise( const Model* p, OutputArchive& arch )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        arch << *p->m_pD;
    }

    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class OutputModelStream : public OutputStream
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------
        OutputModelStream( ModelStreamer* pStreamer )
            : m_pStreamer( pStreamer )
        {
        }

        //-----------------------------------------------------------------------------------------
        // InputStream interface
        //-----------------------------------------------------------------------------------------
        void Write( const void* pData, uint64 size ) override
        {
            m_pStreamer->Write( pData, size );
        }


    private:

        ModelStreamer* m_pStreamer;
    };


    //---------------------------------------------------------------------------------------------
    void Model::Serialise( Model* p, ModelStreamer& streamer )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		mu::FProgram& program = p->m_pD->m_program;

		TArray<TPair<int32, mu::ImagePtrConst>> InitialImages = program.m_constantImageLODs;
		TArray<TPair<int32, mu::MeshPtrConst>> InitialMeshes = program.m_constantMeshes;

		// Save images and unload from memory
		for (int32 ResourceIndex = 0; ResourceIndex < program.m_constantImageLODs.Num(); ++ResourceIndex)
		{
			TPair<int32, mu::ImagePtrConst>& ResData = program.m_constantImageLODs[ResourceIndex];

			// This shouldn't have been serialised with rom support before.
			if (ResData.Key < 0)
			{
				continue;
			}

			int32 RomIndex = ResData.Key;
			const FRomData& RomData = program.m_roms[RomIndex];
			check(RomData.ResourceType == DT_IMAGE);
			check(RomData.ResourceIndex == ResourceIndex);

			// Serialize to memory, to find out final size of this rom
			OutputMemoryStream MemStream(1024*1024);
			OutputArchive MemoryArch(&MemStream);
			Image::Serialise(ResData.Value.get(), MemoryArch);
			check(RomData.Size == MemStream.GetBufferSize());

			streamer.OpenWriteFile(p->GetLocation(), RomData.Id);
			streamer.Write(MemStream.GetBuffer(), MemStream.GetBufferSize());
			streamer.CloseWriteFile();

			// We clear it to avoid including it also with the main model data
			ResData.Value = nullptr;
		}

		// Save meshes and unload from memory
		for (int32 ResourceIndex = 0; ResourceIndex < program.m_constantMeshes.Num(); ++ResourceIndex)
		{
			TPair<int32, mu::MeshPtrConst>& ResData = program.m_constantMeshes[ResourceIndex];

			// This shouldn't have been serialised with rom support before.
			if (ResData.Key < 0)
			{
				continue;
			}

			int32 RomIndex = ResData.Key;
			const FRomData& RomData = program.m_roms[RomIndex];
			check(RomData.ResourceType == DT_MESH);
			check(RomData.ResourceIndex == ResourceIndex);

			// Serialize to memory, to find out final size of this rom
			OutputMemoryStream MemStream(1024 * 1024);
			OutputArchive MemoryArch(&MemStream);
			Mesh::Serialise(ResData.Value.get(), MemoryArch);
			check(RomData.Size == MemStream.GetBufferSize());

			streamer.OpenWriteFile(p->GetLocation(), RomData.Id);
			streamer.Write(MemStream.GetBuffer(), MemStream.GetBufferSize());
			streamer.CloseWriteFile();

			// We clear it to avoid including it also with the main model data
			ResData.Value = nullptr;
		}

		// Store the main data of the model
		{
			streamer.OpenWriteFile(p->GetLocation(), 0);
			OutputModelStream stream(&streamer);
			OutputArchive arch(&stream);

			arch << *p->m_pD;

			streamer.CloseWriteFile();
		}

		// Restore full data
		program.m_constantImageLODs = InitialImages;
		program.m_constantMeshes = InitialMeshes;
	}


    //---------------------------------------------------------------------------------------------
    bool Model::HasExternalData() const
    {
		return m_pD->m_program.m_roms.Num() > 0;
    }


    //---------------------------------------------------------------------------------------------
    void Model::UnloadExternalData()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		for (int32 ResIndex = 0; ResIndex < m_pD->m_program.m_constantImageLODs.Num(); ++ResIndex)
		{
			if (m_pD->m_program.m_constantImageLODs[ResIndex].Key >= 0)
			{
				m_pD->m_program.m_constantImageLODs[ResIndex].Value = nullptr;
			}
		}

		for (int32 ResIndex = 0; ResIndex < m_pD->m_program.m_constantMeshes.Num(); ++ResIndex)
		{
			if (m_pD->m_program.m_constantMeshes[ResIndex].Key >= 0)
			{
				m_pD->m_program.m_constantMeshes[ResIndex].Value = nullptr;
			}
		}
	}


    //---------------------------------------------------------------------------------------------
	TSharedPtr<Model> Model::StaticUnserialise( InputArchive& arch )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		TSharedPtr<Model> pResult = MakeShared<Model>();
        arch >> *pResult->m_pD;
        return pResult;
    }


    //---------------------------------------------------------------------------------------------
    const char* Model::GetLocation( ) const
    {
        return m_pD->m_location.c_str();
    }


    //---------------------------------------------------------------------------------------------
    void Model::SetLocation( const char* strLocation )
    {
        if (strLocation)
        {
            m_pD->m_location = strLocation;
        }
    }


    //---------------------------------------------------------------------------------------------
    Model::Private* Model::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    void Model::ClearCaches()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        if (m_pD)
        {
            m_pD->m_generatedResources.Empty();
        }
    }


    //---------------------------------------------------------------------------------------------
    ParametersPtr Model::NewParameters(TSharedPtr<const Model> Model, const Parameters* pOld )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        ParametersPtr pRes = new Parameters();

        pRes->GetPrivate()->m_pModel = Model;

		const FProgram& Program = Model->GetPrivate()->m_program;
        pRes->GetPrivate()->m_values.SetNum(Program.m_parameters.Num());
        for ( int32 p=0; p< Program.m_parameters.Num(); ++p )
        {
            pRes->GetPrivate()->m_values[p] = Program.m_parameters[p].m_defaultValue;
        }

        // Copy old values
        if ( pOld )
        {
            for ( int p=0; p<pOld->GetCount(); ++p )
            {
                int thisP = pRes->GetPrivate()->Find( pOld->GetName(p) );

                if ( thisP>=0 )
                {
                    if ( pOld->GetType(p)==pRes->GetType(thisP) )
                    {
                        switch ( pRes->GetType(thisP) )
                        {
						case PARAMETER_TYPE::T_BOOL:
                            pRes->SetBoolValue( thisP, pOld->GetBoolValue(p) );
                            break;

                        case PARAMETER_TYPE::T_INT:
                            pRes->SetIntValue( thisP, pOld->GetIntValue(p) );
                            break;

                        case PARAMETER_TYPE::T_FLOAT:
                            pRes->SetFloatValue( thisP, pOld->GetFloatValue(p) );
                            break;

                        case PARAMETER_TYPE::T_COLOUR:
                        {
                            float r,g,b;
                            pOld->GetColourValue( p, &r, &g, &b );
                            pRes->SetColourValue( thisP, r, g, b );
                            break;
                        }

                        case PARAMETER_TYPE::T_PROJECTOR:
                        {
//							float m[16];
//							pOld->GetProjectorValue( p, m );
                            pRes->GetPrivate()->m_values[thisP].m_projector =
                                    pOld->GetPrivate()->m_values[p].m_projector;
                            break;
                        }

                        case PARAMETER_TYPE::T_IMAGE:
                            pRes->SetImageValue( thisP, pOld->GetImageValue(p) );
                            break;

                        default:
                            check(false);
                            break;
                        }
                    }
                }
            }
        }

        return pRes;
    }


    //---------------------------------------------------------------------------------------------
    int Model::GetStateCount() const
    {
        return (int)m_pD->m_program.m_states.Num();
    }


    //---------------------------------------------------------------------------------------------
    const char* Model::GetStateName( int index ) const
    {
        const char* strRes = 0;

        if ( index>=0 && index<(int)m_pD->m_program.m_states.Num() )
        {
            strRes = m_pD->m_program.m_states[index].m_name.c_str();
        }

        return strRes;
    }


    //---------------------------------------------------------------------------------------------
    int Model::FindState( const char* strName ) const
    {
        int res = -1;

        for ( int i=0; res<0 && i<(int)m_pD->m_program.m_states.Num(); ++i )
        {
            if ( m_pD->m_program.m_states[i].m_name == strName )
            {
                res = i;
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    int Model::GetStateParameterCount( int stateIndex ) const
    {
        int res = -1;

        if ( stateIndex>=0 && stateIndex<(int)m_pD->m_program.m_states.Num() )
        {
            res = (int)m_pD->m_program.m_states[stateIndex].m_runtimeParameters.Num();
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    int Model::GetStateParameterIndex( int stateIndex, int paramIndex ) const
    {
        int res = -1;

        if ( stateIndex>=0 && stateIndex<(int)m_pD->m_program.m_states.Num() )
        {
            const FProgram::FState& state = m_pD->m_program.m_states[stateIndex];
            if ( paramIndex>=0 && paramIndex<(int)state.m_runtimeParameters.Num() )
            {
                res = (int)state.m_runtimeParameters[paramIndex];
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    static size_t AddMultiValueKeys(TArray<uint8_t>& parameterValuesBlob, size_t pos,
                                  const TMap< TArray<int>, PARAMETER_VALUE >& multi )
    {
        parameterValuesBlob.SetNum( pos+4 );
        uint32 s = uint32( multi.Num() );
        FMemory::Memcpy( &parameterValuesBlob[pos], &s, sizeof(uint32) );
        pos+=4;

        for(const auto& v: multi)
        {
            uint32 ds = uint32( v.Key.Num() );

            parameterValuesBlob.SetNum( pos + 4 + ds*4 );

			FMemory::Memcpy( &parameterValuesBlob[pos], &s, sizeof(uint32) );
            pos+=4;

			FMemory::Memcpy( &parameterValuesBlob[pos],v.Key.GetData(), ds*sizeof(int32) );
            pos += ds*sizeof(int32);
        }

        return pos;
    }


    //---------------------------------------------------------------------------------------------
    uint32 Model::Private::GetResourceKey( uint32 paramListIndex, OP::ADDRESS rootAt, const Parameters* pParams )
    {
		MUTABLE_CPUPROFILER_SCOPE(GetResourceKey)

        // Find the list of relevant parameters
        const TArray<uint16>* params = nullptr;
        if (paramListIndex<(uint32)m_program.m_parameterLists.Num())
        {
            params = &m_program.m_parameterLists[paramListIndex];
        }
        check(params);
        if (!params)
        {
            return 0xffff;
        }

        // Generate the relevant parameters blob
		TArray<uint8_t> parameterValuesBlob;
		parameterValuesBlob.Reserve(1024);
        for (int param: *params)
        {
            int32 pos = parameterValuesBlob.Num();
			int32 dataSize = 0;

            switch(m_program.m_parameters[param].m_type)
            {
            case PARAMETER_TYPE::T_BOOL:
                dataSize = 1;
                parameterValuesBlob.Add( pParams->GetPrivate()->m_values[param].m_bool ? 1 : 0 );
                pos += dataSize;

                // Multi-values
                if ( param < int(pParams->GetPrivate()->m_multiValues.Num()) )
                {
                    const auto& multi = pParams->GetPrivate()->m_multiValues[param];
                    pos = AddMultiValueKeys( parameterValuesBlob, pos, multi );
                    for(const auto& v: multi)
                    {
                        parameterValuesBlob.Add( v.Value.m_bool ? 1 : 0 );
                        pos += dataSize;
                    }
                }
                break;

            case PARAMETER_TYPE::T_INT:
                dataSize = sizeof(int32);
                parameterValuesBlob.SetNum( pos+dataSize );
                FMemory::Memcpy( &parameterValuesBlob[pos], &pParams->GetPrivate()->m_values[param].m_int, dataSize );
                pos += dataSize;

                // Multi-values
                if ( param < int(pParams->GetPrivate()->m_multiValues.Num()) )
                {
                    const auto& multi = pParams->GetPrivate()->m_multiValues[param];
                    pos = AddMultiValueKeys( parameterValuesBlob, pos, multi );
                    parameterValuesBlob.SetNum( pos+multi.Num()*dataSize );
                    for(const auto& v: multi)
                    {
						FMemory::Memcpy( &parameterValuesBlob[pos], &v.Value.m_int, dataSize );
                        pos += dataSize;
                    }
                }
                break;

            //! Floating point value in the range of 0.0 to 1.0
            case PARAMETER_TYPE::T_FLOAT:
                dataSize = sizeof(float);
                parameterValuesBlob.SetNum( pos+dataSize );
				FMemory::Memcpy( &parameterValuesBlob[pos], &pParams->GetPrivate()->m_values[param].m_float, dataSize );
                pos += dataSize;

                // Multi-values
                if ( param < pParams->GetPrivate()->m_multiValues.Num() )
                {
                    const auto& multi = pParams->GetPrivate()->m_multiValues[param];
                    pos = AddMultiValueKeys( parameterValuesBlob, pos, multi );
                    parameterValuesBlob.SetNum( pos+multi.Num()*dataSize );
                    for(const auto& v: multi)
                    {
						FMemory::Memcpy( &parameterValuesBlob[pos], &v.Value.m_float,dataSize );
                        pos += dataSize;
                    }
                }
                break;

            //! Floating point RGBA colour, with each channel ranging from 0.0 to 1.0
            case PARAMETER_TYPE::T_COLOUR:
                dataSize = 3*sizeof(float);
                parameterValuesBlob.SetNum( pos+dataSize );
				FMemory::Memcpy( &parameterValuesBlob[pos], &pParams->GetPrivate()->m_values[param].m_colour, dataSize );
                pos += dataSize;

                // Multi-values
                if ( param < int(pParams->GetPrivate()->m_multiValues.Num()) )
                {
                    const auto& multi = pParams->GetPrivate()->m_multiValues[param];
                    pos = AddMultiValueKeys( parameterValuesBlob, pos, multi );
                    parameterValuesBlob.SetNum( pos+multi.Num()*dataSize );
                    for(const auto& v: multi)
                    {
						FMemory::Memcpy( &parameterValuesBlob[pos], &v.Value.m_colour,dataSize );
                        pos += dataSize;
                    }
                }
                break;

            //! 3D Projector type, defining a position, scale and orientation. Basically used for
            //! projected decals.
            case PARAMETER_TYPE::T_PROJECTOR:
                dataSize = sizeof(FProjector);

                // \todo: padding will be random?
                parameterValuesBlob.SetNum( pos+dataSize );
				FMemory::Memcpy( &parameterValuesBlob[pos], &pParams->GetPrivate()->m_values[param].m_projector, dataSize );
				pos += dataSize;

                // Multi-values
                if ( param < int(pParams->GetPrivate()->m_multiValues.Num()) )
                {
                    const auto& multi = pParams->GetPrivate()->m_multiValues[param];
                    pos = AddMultiValueKeys( parameterValuesBlob, pos, multi );
                    parameterValuesBlob.SetNum( pos+multi.Num()*dataSize );
                    for(const auto& v: multi)
                    {
						FMemory::Memcpy( &parameterValuesBlob[pos], &v.Value.m_projector,dataSize );
                        pos += dataSize;
                    }
                }
                break;

            case PARAMETER_TYPE::T_IMAGE:
                dataSize = sizeof(EXTERNAL_IMAGE_ID);
                parameterValuesBlob.SetNum( pos+dataSize );
				FMemory::Memcpy( &parameterValuesBlob[pos],
                        &pParams->GetPrivate()->m_values[param].m_image,
                        dataSize );
				pos += dataSize;

                // Multi-values
                if ( param < int(pParams->GetPrivate()->m_multiValues.Num()) )
                {
                    const auto& multi = pParams->GetPrivate()->m_multiValues[param];
                    pos = AddMultiValueKeys( parameterValuesBlob, pos, multi );
                    parameterValuesBlob.SetNum( pos+multi.Num()*dataSize );
                    for(const auto& v: multi)
                    {
						FMemory::Memcpy( &parameterValuesBlob[pos], &v.Value.m_image,dataSize );
                        pos += dataSize;
                    }
                }
                break;

            default:
                // unsupported parameter type
                check(false);
            }
        }

        // Increase the request id
        ++m_lastResourceResquestId;

        // See if we already have this id
        size_t oldestCachePosition = 0;
        for (size_t i=0; i<m_generatedResources.Num(); ++i)
        {
            auto& key = m_generatedResources[i];
            if (key.m_rootAddress==rootAt
				&&
				key.m_parameterValuesBlob==parameterValuesBlob)
            {
                key.m_lastRequestId = m_lastResourceResquestId;
                return key.m_id;
            }
            else
            {
                if ( m_generatedResources[oldestCachePosition].m_lastRequestId
                     >
                     key.m_lastRequestId )
                {
                    oldestCachePosition = i;
                }
            }
        }

        // Generate a new id
        uint32 newId = ++m_lastResourceKeyId;
        RESOURCE_KEY newKey;
        newKey.m_id = newId;
        newKey.m_lastRequestId = m_lastResourceResquestId;
        newKey.m_rootAddress = rootAt;
        newKey.m_parameterValuesBlob = MoveTemp(parameterValuesBlob);

        // TODO: Move the constant to settings?
        const size_t maxGeneratedResourcesIDCacheSize = 1024;
        if (m_generatedResources.Num()>=maxGeneratedResourcesIDCacheSize)
        {
            m_generatedResources[oldestCachePosition] = MoveTemp(newKey);
        }
        else
        {
            m_generatedResources.Add(MoveTemp(newKey));
        }

        return newId;
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    ModelParametersGenerator::ModelParametersGenerator
        (
			TSharedPtr<const Model> pModel,
            System* pSystem
        )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        m_pD = new Private();
        m_pD->m_pModel = pModel;
        m_pD->m_pSystem = pSystem;

        size_t paramCount = pModel->GetPrivate()->m_program.m_parameters.Num();

		m_pD->m_instanceCount = 1;
        for (size_t i=0;i<paramCount; ++i)
        {
            const FParameterDesc& param =
                    m_pD->m_pModel->GetPrivate()->m_program.m_parameters[ i ];
            switch ( param.m_type )
            {

            case PARAMETER_TYPE::T_INT:
            {
                m_pD->m_instanceCount *= param.m_possibleValues.Num();
                break;
            }

            case PARAMETER_TYPE::T_BOOL:
            {
                m_pD->m_instanceCount *= 2;
                break;
            }

            default:
                // Parameter not discrete, so ignored for combinations.
                break;
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    ModelParametersGenerator::~ModelParametersGenerator()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    ModelParametersGenerator::Private* ModelParametersGenerator::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    int64 ModelParametersGenerator::GetInstanceCount()
    {
        return m_pD->m_instanceCount;
    }


    //---------------------------------------------------------------------------------------------
    Ptr<Parameters> ModelParametersGenerator::GetInstance( int64 index, TFunction<float()> RandomGenerator )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		Ptr<Parameters> res = Model::NewParameters(m_pD->m_pModel);

		const FProgram& Program = m_pD->m_pModel->GetPrivate()->m_program;
        int paramCount = Program.m_parameters.Num();
        int64 currentInstance = index;
        for (int i=0;i<paramCount;++i)
        {
            const FParameterDesc& param = Program.m_parameters[ i ];
			Ptr<RangeIndex> RangeIndex = res->NewRangeIndex(i);

            switch ( param.m_type )
            {

            case PARAMETER_TYPE::T_INT:
            {
				bool bIsRangeSize = Program.m_ranges.ContainsByPredicate([i](const FRangeDesc& r) { return r.m_dimensionParameter == i; });
				if (bIsRangeSize)
				{
					res->SetIntValue(i, m_pD->DefaultRangeDimension);
				}
				else
				{
					int numOptions = res->GetIntPossibleValueCount(i);
					int value = res->GetIntPossibleValue(i, (int)(currentInstance % numOptions));
					res->SetIntValue(i, value);
					currentInstance /= numOptions;
				}
                break;
            }

            case PARAMETER_TYPE::T_BOOL:
            {
                res->SetBoolValue( i, currentInstance%2!=0 );
                currentInstance /= 2;
                break;
            }

            case PARAMETER_TYPE::T_FLOAT:
			{
				bool bIsRangeSize = Program.m_ranges.ContainsByPredicate([i](const FRangeDesc& r) { return r.m_dimensionParameter == i; });
				if (bIsRangeSize)
				{
					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								RangeIndex->SetPosition(Dimensions, RangePosition);
								res->SetFloatValue(i, float(m_pD->DefaultRangeDimension), RangeIndex);
							}
						}
					}
					else
					{
						res->SetFloatValue(i, float(m_pD->DefaultRangeDimension));
					}
				}
				else if (RandomGenerator)
				{
					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								RangeIndex->SetPosition(Dimensions, RangePosition);
								res->SetFloatValue(i, RandomGenerator(), RangeIndex);
							}
						}
					}
					else
					{
						res->SetFloatValue(i, RandomGenerator());
					}
				}
				break;
			}

            case PARAMETER_TYPE::T_COLOUR:
                if (RandomGenerator)
                {
					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								RangeIndex->SetPosition(Dimensions, RangePosition);
								res->SetColourValue(i, RandomGenerator(), RandomGenerator(), RandomGenerator(), RangeIndex);
							}
						}
					}
					else
					{
						res->SetColourValue(i, RandomGenerator(), RandomGenerator(), RandomGenerator());
					}
                }
                break;

			case PARAMETER_TYPE::T_PROJECTOR:
				if (RandomGenerator)
				{
					// For projectors we just warp the position a little bit just to get something different
					FVector3f Position, Direction, Up, Scale;
					float Angle = 1.0f;
					res->GetProjectorValue(i, nullptr, &Position, &Direction, &Up, &Scale, &Angle);

					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								RangeIndex->SetPosition(Dimensions, RangePosition);
								Position.X *= 0.9 + RandomGenerator() * 0.2f;
								Position.Y *= 0.9 + RandomGenerator() * 0.2f;
								Position.Z *= 0.9 + RandomGenerator() * 0.2f;
								res->SetProjectorValue(i, Position, Direction, Up, Scale, Angle, RangeIndex);

							}
						}
					}
					else
					{
						Position.X *= 0.9 + RandomGenerator() * 0.2f;
						Position.Y *= 0.9 + RandomGenerator() * 0.2f;
						Position.Z *= 0.9 + RandomGenerator() * 0.2f;
						res->SetProjectorValue(i, Position, Direction, Up, Scale, Angle);
					}
				}
				break;

            default:
                break;
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    ParametersPtr ModelParametersGenerator::GetRandomInstance( TFunctionRef<float()> randomGenerator)
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        ParametersPtr res = Model::NewParameters(m_pD->m_pModel);

		const FProgram& Program = m_pD->m_pModel->GetPrivate()->m_program;
        int32 ParamCount = Program.m_parameters.Num();

        for (int i=0;i<ParamCount;++i)
        {
            const FParameterDesc& param = Program.m_parameters[ i ];
			Ptr<RangeIndex> RangeIndex = res->NewRangeIndex(i);

            switch ( param.m_type )
            {

            case PARAMETER_TYPE::T_INT:
            {
				bool bIsRangeSize = Program.m_ranges.ContainsByPredicate([i](const FRangeDesc& r) { return r.m_dimensionParameter == i; });
				if (bIsRangeSize)
				{
					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								RangeIndex->SetPosition(Dimensions, RangePosition);
								res->SetIntValue(i, m_pD->DefaultRangeDimension, RangeIndex);
							}
						}
					}
					else
					{
						res->SetIntValue(i, m_pD->DefaultRangeDimension);
					}
				}
				else
				{
					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								int numOptions = res->GetIntPossibleValueCount(i);
								int valueIndex = (int)(FMath::Min(numOptions - 1, int(randomGenerator() * numOptions)));
								int value = res->GetIntPossibleValue(i, valueIndex);
								res->SetIntValue(i, value, RangeIndex);
							}
						}
					}
					else
					{
						int numOptions = res->GetIntPossibleValueCount(i);
						int valueIndex = (int)(FMath::Min(numOptions - 1, int(randomGenerator() * numOptions)));
						int value = res->GetIntPossibleValue(i, valueIndex);
						res->SetIntValue(i, value);
					}
				}
                break;
            }

            case PARAMETER_TYPE::T_BOOL:
            {
                res->SetBoolValue( i, randomGenerator()>0.5f );
                break;
            }

            case PARAMETER_TYPE::T_FLOAT:
			{
				bool bIsRangeSize = Program.m_ranges.ContainsByPredicate([i](const FRangeDesc& r) { return r.m_dimensionParameter == i; });
				if (bIsRangeSize)
				{
					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								res->SetFloatValue(i, float(m_pD->DefaultRangeDimension), RangeIndex);
							}
						}
					}
					else
					{
						res->SetFloatValue(i, float(m_pD->DefaultRangeDimension));
					}
				}
				else
				{
					if (RangeIndex)
					{
						for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
						{
							for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
							{
								res->SetFloatValue(i, randomGenerator(), RangeIndex);
							}
						}
					}
					else
					{
						res->SetFloatValue(i, randomGenerator());
					}
				}
				break;
			}

            case PARAMETER_TYPE::T_COLOUR:
				if (RangeIndex)
				{
					for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
					{
						for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
						{
							res->SetColourValue(i, randomGenerator(), randomGenerator(), randomGenerator(),RangeIndex);
						}
					}
				}
				else
				{
					res->SetColourValue(i, randomGenerator(), randomGenerator(), randomGenerator());
				}
                break;

			case PARAMETER_TYPE::T_PROJECTOR:
			{
				// For projectors we just warp the position a little bit just to get something different
				FVector3f Position, Direction, Up, Scale;
				float Angle = 1.0f;
				res->GetProjectorValue(i, nullptr, &Position, &Direction, &Up, &Scale, &Angle);
				if (RangeIndex)
				{
					for (int32 Dimensions = 0; Dimensions < RangeIndex->GetRangeCount(); ++Dimensions)
					{
						for (int32 RangePosition = 0; RangePosition < m_pD->DefaultRangeDimension; ++RangePosition)
						{
							Position.X *= 0.9 + randomGenerator() * 0.2f;
							Position.Y *= 0.9 + randomGenerator() * 0.2f;
							Position.Z *= 0.9 + randomGenerator() * 0.2f;
							res->SetProjectorValue(i, Position, Direction, Up, Scale, Angle, RangeIndex);
						}
					}
				}
				else
				{
					Position.X *= 0.9 + randomGenerator() * 0.2f;
					Position.Y *= 0.9 + randomGenerator() * 0.2f;
					Position.Z *= 0.9 + randomGenerator() * 0.2f;
					res->SetProjectorValue(i, Position, Direction, Up, Scale, Angle);
				}
				break;
			}

            default:
                break;
            }
        }

        return res;
    }


}

