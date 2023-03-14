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
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableTrace.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"
#include "MuR/Serialisation.h"
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
#include "MuT/TaskManager.h"
#include "Trace/Detail/Channel.h"

#include <algorithm>
#include <memory>
#include <utility>


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
        m_pD->m_log = false;
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
    void CompilerOptions::SetLogEnabled( bool enabled )
    {
        m_pD->m_log = enabled;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationEnabled( bool enabled )
    {
        m_pD->m_optimisationOptions.m_enabled = enabled;
        if (enabled)
        {
            m_pD->m_optimisationOptions.m_constReduction = true;
        }
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetConstReductionEnabled( bool constReduction )
    {
        m_pD->m_optimisationOptions.m_constReduction = constReduction;
    }


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetEnablePartialOptimisation(bool bEnabled)
	{
		m_pD->m_enablePartialOptimise = bEnabled;
	}


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetEnableConcurrency(bool bEnabled)
	{
		m_pD->m_enableConcurrency = bEnabled;
	}


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetUseDiskCache( bool enabled )
    {
        m_pD->m_optimisationOptions.m_useDiskCache = enabled;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetOptimisationMaxIteration( int maxIterations )
    {
        m_pD->m_optimisationOptions.m_maxOptimisationLoopCount = maxIterations;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetIgnoreStates( bool ignore )
    {
        m_pD->m_ignoreStates = ignore;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetTextureLayoutStrategy( TextureLayoutStrategy strategy )
    {
        m_pD->m_textureLayoutStrategy = strategy;
    }


    //---------------------------------------------------------------------------------------------
    void CompilerOptions::SetImageCompressionQuality( int quality )
    {
        m_pD->m_imageCompressionQuality = quality;
    }


	//---------------------------------------------------------------------------------------------
	void CompilerOptions::SetDataPackingStrategy(int32 MinRomSize, int32 MinTextureResidentMipCount)
	{
		m_pD->MinRomSize = MinRomSize;
		m_pD->MinTextureResidentMipCount = MinTextureResidentMipCount;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    Compiler::Compiler( CompilerOptionsPtr options )
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
    ModelPtr Compiler::Compile( const Ptr<Node>& pNode )
    {
        MUTABLE_CPUPROFILER_SCOPE(Compile);

        TaskManager* pTaskManager = new TaskManager(m_pD->m_options->GetPrivate()->m_enableConcurrency);

        vector< STATE_COMPILATION_DATA > states;
        Ptr<ErrorLog> genErrorLog;
        {
            CodeGenerator gen( m_pD->m_options->GetPrivate() );

            gen.GenerateRoot( pNode, pTaskManager );

            check( !gen.m_states.IsEmpty() );

            for ( const auto& s: gen.m_states )
            {
                STATE_COMPILATION_DATA data;
                data.nodeState = s.first;
                data.root = s.second;
                data.state.m_name = s.first.m_name;
                data.optimisationFlags = s.first.m_optimisation;
                states.push_back( data );
            }

            genErrorLog = gen.m_pErrorLog;
        }

        vector<Ptr<ASTOp>> roots;
        for( const auto& s: states)
        {
            roots.push_back(s.root);
        }

        // Slow AST code verification for debugging.
        //ASTOp::FullAssert(roots);

        // Optimize the generated code
        {
            CodeOptimiser optimiser( m_pD->m_options, states );
            optimiser.OptimiseAST( pTaskManager );
        }


        // Link the program and generate state data.
        ModelPtr pResult = new Model();
        auto& program = pResult->GetPrivate()->m_program;
        for( auto& s: states )
        {
			FLinkerOptions LinkerOptions;
			LinkerOptions.MinTextureResidentMipCount = m_pD->m_options->GetPrivate()->MinTextureResidentMipCount;

            ASTOp::FullLink(s.root,program, &LinkerOptions);
            if (s.root)
            {
                s.state.m_root = s.root->linkedAddress;
            }
            else
            {
                s.state.m_root = 0;
            }
        }

        // Set the runtime parameter indices.
        for( auto& s: states )
        {
            for ( int32 p=0; p<s.nodeState.m_runtimeParams.Num(); ++p )
            {
                int paramIndex = -1;
                for ( size_t i=0; paramIndex<0 && i<program.m_parameters.Num(); ++i )
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
                    char temp[256];
                    mutable_snprintf( temp, 256,
                                      "The state [%s] refers to a parameter [%s] "
                                      "that has not been found in the model. This error can be "
                                      "safely dismissed in case of partial compilation.",
                                      s.nodeState.m_name.c_str(),
                                      s.nodeState.m_runtimeParams[p].c_str()
                                      );
                    m_pD->m_pErrorLog->GetPrivate()->Add
                            ( temp, ELMT_WARNING, pNode->GetBasePrivate()->m_errorContext );
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
                uint64_t relevantMask = 0;
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
                    int32 it = s.state.m_runtimeParameters.Find( paramIndex );

                    if ( it!=INDEX_NONE )
                    {
                        relevantMask |= uint64_t(1) << it;
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

        // Optimize the generated code
        {        
//            CodeOptimiser optimiser( m_pD->m_gpuPlatformProps,
//                                     m_pD->m_optimisationOptions,
//                                     states );
//            optimiser.Optimise( pResult.get() );

//            m_pD->m_pModelReport = optimiser.m_pModelReport;
        }

        // Merge the log in the right order
        genErrorLog->Merge( m_pD->m_pErrorLog.get() );
        m_pD->m_pErrorLog = genErrorLog.get();

		// Pack data
		int32 MinimumBytesPerRom = 1024; // \TODO: compilation parameter
		m_pD->GenerateRoms(pResult.get(),MinimumBytesPerRom);

		UE_LOG(LogMutableCore, Verbose, TEXT("(int) %s : %ld"), TEXT("program size"), int64(program.m_opAddress.Num()));

        delete pTaskManager;

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
		inline void EnsureUniqueRomId(uint32& RomId, const mu::PROGRAM& program)
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
	void Compiler::Private::GenerateRoms(Model* p, int32 MinRomSize)
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		MUTABLE_CPUPROFILER_SCOPE(Mutable_GenerateRoms);

		mu::PROGRAM& program = p->GetPrivate()->m_program;

		int32 NumRoms = 0;
		int32 NumEmbedded = 0;

		// Serialise roms in a separate files if possible
		if (MinRomSize >= 0)
		{
			// Save images and unload from memory
			for (int32 ResourceIndex = 0; ResourceIndex < program.m_constantImageLODs.Num(); ++ResourceIndex)
			{
				TPair<int32, mu::ImagePtrConst>& ResData = program.m_constantImageLODs[ResourceIndex];

				// This shouldn't have been serialised with rom support before.
				check(ResData.Key < 0);

				// Serialize to memory, to find out final size of this rom
				OutputMemoryStream MemStream(1024 * 1024);
				OutputArchive MemoryArch(&MemStream);
				Image::Serialise(ResData.Value.get(), MemoryArch);

				// If the resource uses less memory than the threshold, don't save it in a separate rom.
				if (MemStream.GetBufferSize() < MinRomSize)
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
			for (int32 ResourceIndex = 0; ResourceIndex < program.m_constantMeshes.Num(); ++ResourceIndex)
			{
				TPair<int32, mu::MeshPtrConst>& ResData = program.m_constantMeshes[ResourceIndex];

				// This shouldn't have been serialised with rom support before.
				check(ResData.Key < 0);

				// Serialize to memory, to find out final size of this rom
				OutputMemoryStream MemStream(1024 * 1024);
				OutputArchive MemoryArch(&MemStream);
				Mesh::Serialise(ResData.Value.get(), MemoryArch);

				// If the resource uses less memory than the threshold, don't save it in a separate rom.
				if (MemStream.GetBufferSize() < MinRomSize)
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
		}

		UE_LOG(LogMutableCore, Log, TEXT("Generated roms for model with %d embedded constants and %d in roms."), NumEmbedded, NumRoms);
	}


}

