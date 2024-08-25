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
    MUTABLE_IMPLEMENT_POD_SERIALISABLE(FRomData);
	MUTABLE_IMPLEMENT_POD_SERIALISABLE(FImageLODRange);

	
    //---------------------------------------------------------------------------------------------
    void FProgram::Check()
    {
    #ifdef MUTABLE_DEBUG
		// Insert debug checks here.
    #endif
    }


    //---------------------------------------------------------------------------------------------
    void FProgram::LogHistogram() const
    {
#if 0
        uint64 countPerType[(int32)OP_TYPE::COUNT];
        mutable_memset(countPerType,0,sizeof(countPerType));

        for ( const uint32& o: m_opAddress )
        {
            OP_TYPE type = GetOpType(o);
            countPerType[(int32)type]++;
        }

		TArray< TPair<uint64,OP_TYPE> > sorted((int32)OP_TYPE::COUNT);
        for (int32 i=0; i<(int32)OP_TYPE::COUNT; ++i)
        {
            sorted[i].second = (OP_TYPE)i;
            sorted[i].first = countPerType[i];
        }

        std::sort(sorted.begin(),sorted.end(), []( const pair<uint64,OP_TYPE>& a, const pair<uint64,OP_TYPE>& b )
        {
            return a.first>b.first;
        });

        UE_LOG(LogMutableCore,Log, TEXT("Op histogram (%llu ops):"), m_opAddress.Num());
        for(int32 i=0; i<8; ++i)
        {
            float p = sorted[i].first/float(m_opAddress.Num())*100.0f;
            UE_LOG(LogMutableCore,Log, TEXT("  %3.2f%% : %d"), p, (int32)sorted[i].second );
        }
#endif
    }

	
	//---------------------------------------------------------------------------------------------
    void Model::Private::UnloadRoms()
    {
	    for (int32 RomIndex = 0; RomIndex < m_program.m_roms.Num(); ++RomIndex)
	    {
		    m_program.UnloadRom(RomIndex);
	    }
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
        OutputModelStream(ModelWriter* pStreamer )
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

        ModelWriter* m_pStreamer;
    };


    //---------------------------------------------------------------------------------------------
    void Model::Serialise( Model* p, ModelWriter& streamer )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		mu::FProgram& program = p->m_pD->m_program;

		TArray<TPair<int32, mu::ImagePtrConst>> InitialImages = program.ConstantImageLODs;
		TArray<TPair<int32, mu::MeshPtrConst>> InitialMeshes = program.ConstantMeshes;

		// Save images and unload from memory
		for (int32 ResourceIndex = 0; ResourceIndex < program.ConstantImageLODs.Num(); ++ResourceIndex)
		{
			TPair<int32, mu::ImagePtrConst>& ResData = program.ConstantImageLODs[ResourceIndex];

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

			streamer.OpenWriteFile(RomData.Id);
			streamer.Write(MemStream.GetBuffer(), MemStream.GetBufferSize());
			streamer.CloseWriteFile();

			// We clear it to avoid including it also with the main model data
			ResData.Value = nullptr;
		}

		// Save meshes and unload from memory
		for (int32 ResourceIndex = 0; ResourceIndex < program.ConstantMeshes.Num(); ++ResourceIndex)
		{
			TPair<int32, mu::MeshPtrConst>& ResData = program.ConstantMeshes[ResourceIndex];

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

			streamer.OpenWriteFile(RomData.Id);
			streamer.Write(MemStream.GetBuffer(), MemStream.GetBufferSize());
			streamer.CloseWriteFile();

			// We clear it to avoid including it also with the main model data
			ResData.Value = nullptr;
		}

		// Store the main data of the model
		{
			streamer.OpenWriteFile(0);
			OutputModelStream stream(&streamer);
			OutputArchive arch(&stream);

			arch << *p->m_pD;

			streamer.CloseWriteFile();
		}

		// Restore full data
		program.ConstantImageLODs = InitialImages;
		program.ConstantMeshes = InitialMeshes;
	}


    //---------------------------------------------------------------------------------------------
    bool Model::HasExternalData() const
    {
		return m_pD->m_program.m_roms.Num() > 0;
    }

#if WITH_EDITOR
	//---------------------------------------------------------------------------------------------
	bool Model::IsValid() const
	{
		return m_pD->m_program.bIsValid;
	}


	//---------------------------------------------------------------------------------------------
	void Model::Invalidate()
    {
		m_pD->m_program.bIsValid = false;
    }
#endif

    //---------------------------------------------------------------------------------------------
    void Model::UnloadExternalData()
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		for (int32 ResIndex = 0; ResIndex < m_pD->m_program.ConstantImageLODs.Num(); ++ResIndex)
		{
			if (m_pD->m_program.ConstantImageLODs[ResIndex].Key >= 0)
			{
				m_pD->m_program.ConstantImageLODs[ResIndex].Value = nullptr;
			}
		}

		for (int32 ResIndex = 0; ResIndex < m_pD->m_program.ConstantMeshes.Num(); ++ResIndex)
		{
			if (m_pD->m_program.ConstantMeshes[ResIndex].Key >= 0)
			{
				m_pD->m_program.ConstantMeshes[ResIndex].Value = nullptr;
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
    Model::Private* Model::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    bool Model::GetBoolDefaultValue(int32 Index) const
    {
    	check(m_pD->m_program.m_parameters.IsValidIndex(Index));
		check(m_pD->m_program.m_parameters[Index].m_type == PARAMETER_TYPE::T_BOOL);

        // Early out in case of invalid parameters
        if (!m_pD->m_program.m_parameters.IsValidIndex(Index) ||
            m_pD->m_program.m_parameters[Index].m_type != PARAMETER_TYPE::T_BOOL)
        {
            return false;
        }
		
        return m_pD->m_program.m_parameters[Index].m_defaultValue.Get<ParamBoolType>();
    }


    //---------------------------------------------------------------------------------------------
	int32 Model::GetIntDefaultValue(int32 Index) const
	{
		check(m_pD->m_program.m_parameters.IsValidIndex(Index));
		check(m_pD->m_program.m_parameters[Index].m_type == PARAMETER_TYPE::T_INT);

        // Early out in case of invalid parameters
        if (!m_pD->m_program.m_parameters.IsValidIndex(Index) ||
            m_pD->m_program.m_parameters[Index].m_type != PARAMETER_TYPE::T_INT)
        {
            return 0;
        }
		
        return m_pD->m_program.m_parameters[Index].m_defaultValue.Get<ParamIntType>();
	}


    //---------------------------------------------------------------------------------------------
	float Model::GetFloatDefaultValue(int32 Index) const
	{
    	check(m_pD->m_program.m_parameters.IsValidIndex(Index));
		check(m_pD->m_program.m_parameters[Index].m_type == PARAMETER_TYPE::T_FLOAT);

        // Early out in case of invalid parameters
        if (!m_pD->m_program.m_parameters.IsValidIndex(Index) ||
            m_pD->m_program.m_parameters[Index].m_type != PARAMETER_TYPE::T_FLOAT)
        {
            return 0.0f;
        }
		
        return m_pD->m_program.m_parameters[Index].m_defaultValue.Get<ParamFloatType>();
	}


	void Model::GetColourDefaultValue(int32 Index, float* R, float* G, float* B, float* A) const
    {
    	check(m_pD->m_program.m_parameters.IsValidIndex(Index));
		check(m_pD->m_program.m_parameters[Index].m_type == PARAMETER_TYPE::T_COLOUR);

        // Early out in case of invalid parameters
        if (!m_pD->m_program.m_parameters.IsValidIndex(Index) ||
            m_pD->m_program.m_parameters[Index].m_type != PARAMETER_TYPE::T_COLOUR)
        {
            return;
        }

        ParamColorType& Color = m_pD->m_program.m_parameters[Index].m_defaultValue.Get<ParamColorType>();
		
		if (R)
		{
			*R = Color[0];
		}

		if (G)
		{
			*G = Color[1];
		}

		if (B)
		{
			*B = Color[2];
		}

		if (A)
		{
			*A = Color[3];
		}
    }


	void Model::GetProjectorDefaultValue(int32 Index, PROJECTOR_TYPE* OutProjectionType, FVector3f* OutPos,
		FVector3f* OutDir, FVector3f* OutUp, FVector3f* OutScale, float* OutProjectionAngle) const
	{
    	check(m_pD->m_program.m_parameters.IsValidIndex(Index));
		check(m_pD->m_program.m_parameters[Index].m_type == PARAMETER_TYPE::T_PROJECTOR);

        // Early out in case of invalid parameters
        if (!m_pD->m_program.m_parameters.IsValidIndex(Index) ||
            m_pD->m_program.m_parameters[Index].m_type != PARAMETER_TYPE::T_PROJECTOR)
        {
            return;
        }

        const FProjector& Projector = m_pD->m_program.m_parameters[Index].m_defaultValue.Get<ParamProjectorType>();
        if (OutProjectionType) *OutProjectionType = Projector.type;
    	if (OutPos) *OutPos = Projector.position;
		if (OutDir) *OutDir = Projector.direction;
    	if (OutUp) *OutUp = Projector.up;
    	if (OutScale) *OutScale = Projector.scale;
    	if (OutProjectionAngle) *OutProjectionAngle = Projector.projectionAngle;
	}


	//---------------------------------------------------------------------------------------------
	FName Model::GetImageDefaultValue(int32 Index) const
    {
	    check(m_pD->m_program.m_parameters.IsValidIndex(Index));
		check(m_pD->m_program.m_parameters[Index].m_type == PARAMETER_TYPE::T_IMAGE);

        // Early out in case of invalid parameters
        if (!m_pD->m_program.m_parameters.IsValidIndex(Index) ||
            m_pD->m_program.m_parameters[Index].m_type != PARAMETER_TYPE::T_IMAGE)
        {
            return {};
        }
		
        return m_pD->m_program.m_parameters[Index].m_defaultValue.Get<ParamImageType>();
    }


    //---------------------------------------------------------------------------------------------
    int32 Model::GetRomCount() const
    {
    	return m_pD->m_program.m_roms.Num();
    }


    //---------------------------------------------------------------------------------------------
    uint32 Model::GetRomId(int32 Index) const
    {
    	return m_pD->m_program.m_roms[Index].Id;
    }


    //---------------------------------------------------------------------------------------------
    uint32 Model::GetRomSize(int32 Index) const
    {
    	return m_pD->m_program.m_roms[Index].Size;
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
                            float R, G, B, A;
                            pOld->GetColourValue( p, &R, &G, &B, &A );
                            pRes->SetColourValue( thisP, R, G, B, A );
                            break;
                        }

                        case PARAMETER_TYPE::T_PROJECTOR:
                        {
//							float m[16];
//							pOld->GetProjectorValue( p, m );
                            pRes->GetPrivate()->m_values[thisP].Set<ParamProjectorType>(pOld->GetPrivate()->m_values[p].Get<ParamProjectorType>());
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
    bool Model::IsParameterMultidimensional(const int32 ParamIndex) const
    {
		if (m_pD->m_program.m_parameters.IsValidIndex(ParamIndex))
		{
			return m_pD->m_program.m_parameters[ParamIndex].m_ranges.Num() > 0;
		}

		return false;
    }
	

    //---------------------------------------------------------------------------------------------
    int Model::GetStateCount() const
    {
        return (int)m_pD->m_program.m_states.Num();
    }


    //---------------------------------------------------------------------------------------------
    const FString& Model::GetStateName( int32 index ) const
    {
        const char* strRes = 0;

        if ( index>=0 && index<(int)m_pD->m_program.m_states.Num() )
        {
            return m_pD->m_program.m_states[index].Name;
        }

		static FString None;
        return None;
    }


    //---------------------------------------------------------------------------------------------
    int32 Model::FindState( const FString& Name ) const
    {
        int res = -1;

        for ( int i=0; res<0 && i<(int)m_pD->m_program.m_states.Num(); ++i )
        {
            if ( m_pD->m_program.m_states[i].Name == Name )
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
								res->SetColourValue(i, RandomGenerator(), RandomGenerator(), RandomGenerator(), RandomGenerator(), RangeIndex);
							}
						}
					}
					else
					{
						res->SetColourValue(i, RandomGenerator(), RandomGenerator(), RandomGenerator(), RandomGenerator());
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
							res->SetColourValue(i, randomGenerator(), randomGenerator(), randomGenerator(), randomGenerator(), 
												RangeIndex);
						}
					}
				}
				else
				{
					res->SetColourValue(i, randomGenerator(), randomGenerator(), randomGenerator(), randomGenerator());
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

