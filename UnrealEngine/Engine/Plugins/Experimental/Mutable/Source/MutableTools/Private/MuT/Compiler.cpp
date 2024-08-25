// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/Compiler.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformCrt.h"
#include "Hash/CityHash.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Serialisation.h"
#include "MuR/MutableRuntimeModule.h"
#include "MuR/System.h"
#include "MuT/AST.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/CodeGenerator.h"
#include "MuT/CodeOptimiser.h"
#include "MuT/CompilerPrivate.h"
#include "MuT/ErrorLog.h"
#include "MuT/ErrorLogPrivate.h"
#include "MuT/Node.h"
#include "MuT/NodePrivate.h"
#include "MuT/Table.h"
#include "Trace/Detail/Channel.h"

#include <string>		// Required for deserialisation of old data
#include <inttypes.h>	// Required for 64-bit printf macros


namespace mu
{

    const char* CompilerOptions::GetTextureLayoutStrategyName( CompilerOptions::TextureLayoutStrategy s )
    {
        static const char* s_textureLayoutStrategyName[ 2 ] =
        {
            "Unscaled Pack",
            "No Packing",
        };

        return s_textureLayoutStrategyName[(int)s];
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    CompilerOptions::CompilerOptions()
    {
        m_pD = new Private();
    }


    //---------------------------------------------------------------------------------------------
    CompilerOptions::~CompilerOptions()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
    CompilerOptions::Private* CompilerOptions::GetPrivate() const
    {
        return m_pD;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetLogEnabled( bool bEnabled)
    {
        m_pD->bLog = bEnabled;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationEnabled( bool bEnabled)
    {
        m_pD->OptimisationOptions.bEnabled = bEnabled;
        if (bEnabled)
        {
            m_pD->OptimisationOptions.bConstReduction = true;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetConstReductionEnabled( bool bConstReductionEnabled )
    {
        m_pD->OptimisationOptions.bConstReduction = bConstReductionEnabled;
    }


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetUseDiskCache(bool bEnabled)
	{
		m_pD->OptimisationOptions.DiskCacheContext = bEnabled ? &m_pD->DiskCacheContext : nullptr;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetUseConcurrency(bool bEnabled)
	{
		m_pD->bUseConcurrency = bEnabled;
	}


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationMaxIteration( int32 maxIterations )
    {
        m_pD->OptimisationOptions.MaxOptimisationLoopCount = maxIterations;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetIgnoreStates( bool bIgnore )
    {
        m_pD->bIgnoreStates = bIgnore;
    }


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImageCompressionQuality(int32 Quality)
	{
		m_pD->ImageCompressionQuality = Quality;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImageTiling(int32 Tiling)
	{
		m_pD->ImageTiling = Tiling;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetDataPackingStrategy(int32 MinTextureResidentMipCount, uint64 EmbeddedDataBytesLimit, uint64 PackagedDataBytesLimit)
	{
		m_pD->EmbeddedDataBytesLimit = EmbeddedDataBytesLimit;
		m_pD->PackagedDataBytesLimit = PackagedDataBytesLimit;
		m_pD->MinTextureResidentMipCount = MinTextureResidentMipCount;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetEnableProgressiveImages(bool bEnabled)
	{
		m_pD->OptimisationOptions.bEnableProgressiveImages = bEnabled;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetImagePixelFormatOverride(const FImageOperator::FImagePixelFormatFunc& InFunc)
	{
		m_pD->ImageFormatFunc = InFunc;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetReferencedResourceCallback(const FReferencedResourceFunc& Provider, const FReferencedResourceGameThreadTickFunc& ProviderGameThreadTick)
	{
		m_pD->OptimisationOptions.ReferencedResourceProvider = Provider;
		m_pD->OptimisationOptions.ReferencedResourceProviderTick = ProviderGameThreadTick;
	}


	void CompilerOptions::LogStats() const
	{
		UE_LOG(LogMutableCore, Log, TEXT("   Cache Files Written : %" PRIu64), m_pD->DiskCacheContext.FilesWritten.load());
		UE_LOG(LogMutableCore, Log, TEXT("   Cache Files Read    : %" PRIu64), m_pD->DiskCacheContext.FilesRead.load());
		UE_LOG(LogMutableCore, Log, TEXT("   Cache MB Written    : %" PRIu64), m_pD->DiskCacheContext.BytesWritten.load() >> 20);
		UE_LOG(LogMutableCore, Log, TEXT("   Cache MB Read       : %" PRIu64), m_pD->DiskCacheContext.BytesRead.load()>>20);
	}


	void FObjectState::Serialise(OutputArchive& arch) const
    {
    	const int32 ver = 6;
    	arch << ver;

    	arch << m_name;
    	arch << m_optimisation;
    	arch << m_runtimeParams;
    }


	void FObjectState::Unserialise(InputArchive& arch)
    {
    	int32 ver = 0;
    	arch >> ver;
    	check( ver>=5 && ver<=6 );

    	if (ver <= 5)
    	{
    		std::string Temp;
    		arch >> Temp;
    		m_name = Temp.c_str();
    	}
    	else
    	{
    		arch >> m_name;
    	}
    	arch >> m_optimisation;

    	if (ver <= 5)
    	{
    		TArray<std::string> Temp;
    		arch >> Temp;
    		m_runtimeParams.SetNum(Temp.Num());
    		for ( int32 i=0; i<Temp.Num(); ++i)
    		{
    			m_runtimeParams[i] = Temp[i].c_str();
    		}
    	}
    	else
    	{
    		arch >> m_runtimeParams;
    	}
    }
	
	
	//---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    Compiler::Compiler( Ptr<CompilerOptions> options )
    {
        m_pD = new Private();
        m_pD->m_options = options;
        if (!m_pD->m_options)
        {
            m_pD->m_options = new CompilerOptions;
        }
    }


    //---------------------------------------------------------------------------------------------
    Compiler::~Compiler()
    {
        check( m_pD );
        delete m_pD;
        m_pD = 0;
    }


    //---------------------------------------------------------------------------------------------
	TSharedPtr<Model> Compiler::Compile( const Ptr<Node>& pNode )
    {
        MUTABLE_CPUPROFILER_SCOPE(Compile);

        TArray< FStateCompilationData > states;
        Ptr<ErrorLog> genErrorLog;
		TArray<FParameterDesc> Parameters;
        {
            CodeGenerator gen( m_pD->m_options->GetPrivate() );

            gen.GenerateRoot( pNode );

            check( !gen.m_states.IsEmpty() );

            for ( const auto& s: gen.m_states )
            {
                FStateCompilationData data;
                data.nodeState = s.Key;
                data.root = s.Value;
                data.state.Name = s.Key.m_name;
                states.Add( data );
            }

            genErrorLog = gen.m_pErrorLog;

			// Set the parameter list from the non-optimized data, so that we have them all even if they are optimized out
			int32 ParameterCount = gen.m_firstPass.ParameterNodes.Num();
			Parameters.SetNum(ParameterCount);
			int32 ParameterIndex = 0;
			for ( const TPair<Ptr<const Node>, Ptr<ASTOpParameter>>& Entry : gen.m_firstPass.ParameterNodes )
			{
				Parameters[ParameterIndex] = Entry.Value->parameter;
				++ParameterIndex;
			}

			// Sort the parameters as deterministically as possible.
			struct FParameterSortPredicate
			{
				bool operator()(const FParameterDesc& A, const FParameterDesc& B) const
				{
					if (A.m_name < B.m_name) return true;
					if (A.m_name > B.m_name) return false;
					return A.m_uid < B.m_uid;
				}
			};
			
			FParameterSortPredicate SortPredicate;
			Parameters.Sort(SortPredicate);
		}


        // Slow AST code verification for debugging.
        //TArray<Ptr<ASTOp>> roots;
        //for( const FStateCompilationData& s: states)
        //{
        //    roots.Add(s.root);
        //}
        //ASTOp::FullAssert(roots);

        // Optimize the generated code
        {
            CodeOptimiser optimiser( m_pD->m_options, states );
            optimiser.OptimiseAST( );
        }

        // Link the program and generate state data.
		TSharedPtr<Model> pResult = MakeShared<Model>();
        FProgram& program = pResult->GetPrivate()->m_program;

		check(program.m_parameters.IsEmpty());
		program.m_parameters = Parameters;

		// Preallocate ample memory
		program.m_byteCode.Reserve(16 * 1024 * 1024);
		program.m_opAddress.Reserve(1024 * 1024);

		// Keep the link options outside the scope because it is also used to cache constant data that has already been 
		// added and could be reused across states.
		FImageOperator ImOp = FImageOperator::GetDefault(m_pD->m_options->GetPrivate()->ImageFormatFunc);
		FLinkerOptions LinkerOptions(ImOp);

		for(FStateCompilationData& s: states )
        {
			LinkerOptions.MinTextureResidentMipCount = m_pD->m_options->GetPrivate()->MinTextureResidentMipCount;

            if (s.root)
            {
				s.state.m_root = ASTOp::FullLink(s.root, program, &LinkerOptions);
            }
            else
            {
                s.state.m_root = 0;
            }
        }

		program.m_byteCode.Shrink();
		program.m_opAddress.Shrink();

        // Set the runtime parameter indices.
        for(FStateCompilationData& s: states )
        {
            for ( int32 p=0; p<s.nodeState.m_runtimeParams.Num(); ++p )
            {
                int32 paramIndex = -1;
                for ( int32 i=0; paramIndex<0 && i<program.m_parameters.Num(); ++i )
                {
                    if ( program.m_parameters[i].m_name
                         ==
                         s.nodeState.m_runtimeParams[p] )
                    {
                        paramIndex = (int)i;
                    }
                }

                if (paramIndex>=0)
                {
                    s.state.m_runtimeParameters.Add( paramIndex );
                }
                else
                {
					FString Temp = FString::Printf(TEXT(
						"The state [%s] refers to a parameter [%s]  that has not been found in the model. This error can be "
						"safely dismissed in case of partial compilation."), 
						*s.nodeState.m_name,
						*s.nodeState.m_runtimeParams[p]);
                    m_pD->m_pErrorLog->GetPrivate()->Add(Temp, ELMT_WARNING, pNode->GetBasePrivate()->m_errorContext );
                }
            }

            // Generate the mask of update cache ops
            for ( const auto& a: s.m_updateCache )
            {
                s.state.m_updateCache.Emplace(a->linkedAddress);
            }

            // Sort the update cache addresses for performance and determinism
            s.state.m_updateCache.Sort();

            // Generate the mask of dynamic resources
            for ( const auto& a: s.m_dynamicResources )
            {
                uint64 relevantMask = 0;
                for ( const auto& b: a.Value )
                {
                    // Find the index in the model parameter list
                    int paramIndex = -1;
                    for ( int32 i=0; paramIndex<0 && i<program.m_parameters.Num(); ++i )
                    {
                        if ( program.m_parameters[i].m_name == b )
                        {
                            paramIndex = (int)i;
                        }
                    }
                    check(paramIndex>=0);

                    // Find the position in the state data vector.
                    int32 IndexInRuntimeList = s.state.m_runtimeParameters.Find( paramIndex );

                    if (IndexInRuntimeList !=INDEX_NONE )
                    {
                        relevantMask |= uint64(1) << IndexInRuntimeList;
                    }
                }

                // TODO: this shouldn't happen but it seems to happen. Investigate.
                // Maybe something with the difference of precision between the relevant parameters
                // in operation subtrees.
                //check(relevantMask!=0);
                if (relevantMask!=0)
                {
                    s.state.m_dynamicResources.Emplace( a.Key->linkedAddress, relevantMask );
                }
            }            

            // Sort for performance and determinism
            s.state.m_dynamicResources.Sort();

            program.m_states.Add(s.state);
        }

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("program size"), int64(program.m_opAddress.Num()));

        // Merge the log in the right order
        genErrorLog->Merge( m_pD->m_pErrorLog.get() );
        m_pD->m_pErrorLog = genErrorLog.get();

		// Pack data
		uint64 EmbeddedDataFileBytesLimit = m_pD->m_options->GetPrivate()->EmbeddedDataBytesLimit;
		m_pD->GenerateRoms(pResult.Get(), EmbeddedDataFileBytesLimit);

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("program size"), int64(program.m_opAddress.Num()));

        return pResult;
    }


    //---------------------------------------------------------------------------------------------
    ErrorLogPtrConst Compiler::GetLog() const
    {
        return m_pD->m_pErrorLog;
    }


	//---------------------------------------------------------------------------------------------
	namespace
	{
		inline void EnsureUniqueRomId(uint32& RomId, const mu::FProgram& program)
		{
			while (true)
			{
				bool bUnique = true;
				for (const FRomData& Rom : program.m_roms)
				{
					if (Rom.Id == RomId)
					{
						bUnique = false;
						break;
					}
				}

				if (bUnique)
				{
					break;
				}

				// Generate a new Id. It is not going to be stable accross build, which means it may hurt content patching
				// a little bit, but it shouldn't happen often.
				RomId++;
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	void Compiler::Private::GenerateRoms(Model* p, int32 EmbeddedDataBytesLimit)
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(Mutable_GenerateRoms);

		mu::FProgram& program = p->GetPrivate()->m_program;

		int32 NumRoms = 0;
		int32 NumEmbedded = 0;

		// Save images and unload from memory
		for (int32 ResourceIndex = 0; ResourceIndex < program.ConstantImageLODs.Num(); ++ResourceIndex)
		{
			TPair<int32, mu::ImagePtrConst>& ResData = program.ConstantImageLODs[ResourceIndex];

			// This shouldn't have been serialised with rom support before.
			check(ResData.Key < 0);

			// Serialize to memory, to find out final size of this rom
			OutputMemoryStream MemStream(1024 * 1024);
			OutputArchive MemoryArch(&MemStream);
			Image::Serialise(ResData.Value.get(), MemoryArch);

			// If the resource uses less memory than the threshold, don't save it in a separate rom.
			if (MemStream.GetBufferSize() <= EmbeddedDataBytesLimit)
			{
				NumEmbedded++;
				continue;
			}

			NumRoms++;

			FRomData RomData;
			RomData.ResourceType = DT_IMAGE;
			RomData.ResourceIndex = ResourceIndex;
			RomData.Size = MemStream.GetBufferSize();

			// Ensure that the Id is unique
			RomData.Id = CityHash32(static_cast<const char*>(MemStream.GetBuffer()), MemStream.GetBufferSize());
			EnsureUniqueRomId(RomData.Id, program);

			int32 RomIndex = program.m_roms.Add(RomData);
			ResData.Key = RomIndex;
		}

		// Save meshes and unload from memory
		for (int32 ResourceIndex = 0; ResourceIndex < program.ConstantMeshes.Num(); ++ResourceIndex)
		{
			TPair<int32, mu::MeshPtrConst>& ResData = program.ConstantMeshes[ResourceIndex];

			// This shouldn't have been serialised with rom support before.
			check(ResData.Key < 0);

			// Serialize to memory, to find out final size of this rom
			OutputMemoryStream MemStream(1024 * 1024);
			OutputArchive MemoryArch(&MemStream);
			Mesh::Serialise(ResData.Value.get(), MemoryArch);

			// If the resource uses less memory than the threshold, don't save it in a separate rom.
			if (MemStream.GetBufferSize() <= EmbeddedDataBytesLimit)
			{
				NumEmbedded++;
				continue;
			}

			NumRoms++;

			FRomData RomData;
			RomData.ResourceType = DT_MESH;
			RomData.ResourceIndex = ResourceIndex;
			RomData.Size = MemStream.GetBufferSize();

			// Ensure that the Id is unique
			RomData.Id = CityHash32(static_cast<const char*>(MemStream.GetBuffer()), MemStream.GetBufferSize());
			EnsureUniqueRomId(RomData.Id, program);

			int32 RomIndex = program.m_roms.Add(RomData);
			ResData.Key = RomIndex;
		}

		UE_LOG(LogMutableCore, Log, TEXT("Generated roms for model with %d embedded constants and %d in roms."), NumEmbedded, NumRoms);
	}


}

