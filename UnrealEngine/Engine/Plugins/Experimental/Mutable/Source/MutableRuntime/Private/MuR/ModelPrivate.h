// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Model.h"
#include "MuR/System.h"

#include "MuR/SerialisationPrivate.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ImagePrivate.h"
#include "MuR/MeshPrivate.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/OpImageResize.h"
#include "MuR/OpImageMipmap.h"


namespace mu
{

	/** Used to debug and log. */
	constexpr bool bForceSingleThread = false;
	constexpr bool DebugRom = false;
	constexpr bool DebugRomAll = false;
	constexpr int32 DebugRomIndex = 44;
	constexpr int32 DebugImageIndex = 9;

    //! Data stored for a rom even if it is not loaded
    struct FRomData
    {
        //! This is used to identify a ROM file. It is usually a hash from its data.
        uint32 Id;

		//! Size of the rom
		uint32 Size;

		//! Index of the resource in its type-sepcific array
		uint32 ResourceIndex;

		//! Index of the resource in its type-sepcific array (one of DATATYPE values)
		uint32 ResourceType;        
    };

    MUTABLE_DEFINE_POD_SERIALISABLE(FRomData);

    //!
    template<typename DATA>
    inline void AppendCode(TArray<uint8>& code, const DATA& data )
    {
        int32 pos = code.Num();
        code.SetNum( pos+sizeof(DATA) );
		FMemory::Memcpy (&code[pos], &data, sizeof(DATA));
    }


	//!
	struct FImageLODRange
	{
		int32 FirstIndex = 0;
		int32 LODCount = 0;
		uint16 ImageSizeX = 0;
		uint16 ImageSizeY = 0;
	};
	MUTABLE_DEFINE_POD_SERIALISABLE(FImageLODRange);

    //!
    struct PROGRAM
    {
        PROGRAM()
        {
            // Add the null instruction at address 0.
            // TODO: Will do it in the linker
            AppendCode( m_byteCode, OP_TYPE::NONE );
            m_opAddress.Add(0);
        }

        struct STATE
        {
            //! Name of the state
            string m_name;

            //! First instruction of the full build of an instance in this state
            OP::ADDRESS m_root = 0;

            //! List of parameters index (to PROGRAM::m_parameters) of the runtime parameters of
            //! this state.
			TArray<int> m_runtimeParameters;

            //! List of instructions that need to be cached to efficiently update this state
			TArray<OP::ADDRESS> m_updateCache;

            //! List of root instructions for the dynamic resources that depend on the runtime
            //! parameters of this state, with a mask of relevant runtime parameters.
            //! The mask has a bit on for every runtime parameter in the m_runtimeParameters
            //! vector.
			TArray< TPair<OP::ADDRESS,uint64> > m_dynamicResources;

            //!
            inline void Serialise( OutputArchive& arch ) const
            {
                arch << m_name;
                arch << m_root;
                arch << m_runtimeParameters;
                arch << m_updateCache;
                arch << m_dynamicResources;
            }

            //!
            inline void Unserialise( InputArchive& arch )
            {
                arch >> m_name;
                arch >> m_root;
                arch >> m_runtimeParameters;
                arch >> m_updateCache;
                arch >> m_dynamicResources;
            }

            //! Returns the parameters mask
            uint64 IsDynamic( OP::ADDRESS at ) const
            {
                uint64 res = 0;

                for ( int32 i=0; !res && i<m_dynamicResources.Num(); ++i )
                {
                    if ( m_dynamicResources[i].Key==at )
                    {
                        res = m_dynamicResources[i].Value;
                    }
                }

                return res;
            }

            //!
            bool IsUpdateCache( OP::ADDRESS at ) const
            {
                bool res = false;

                for ( size_t i=0; !res && i<m_updateCache.Num(); ++i )
                {
                    if ( m_updateCache[i]==at )
                    {
                        res = true;
                    }
                }

                return res;
            }

            //!
            void AddUpdateCache( OP::ADDRESS at )
            {
                if ( !IsUpdateCache(at) )
                {
                    m_updateCache.Add( at );
                }
            }
        };

        //! Location in the m_byteCode of the beginning of each operation
		TArray<uint32> m_opAddress;

        //! Byte-coded representation of the program, using flexible-sized ops.
		TArray<uint8> m_byteCode;

        //!
		TArray<STATE> m_states;

		//! Data for every rom.
		TArray<FRomData> m_roms;

		//! Constant image mip data: the first is the index in m_roms for each image mip or -1 if it is always loaded.
		TArray<TPair<int32, Ptr<const Image>>> m_constantImageLODs;

		//! Constant image mip chain indices: ranges in this array are defined in FImageLODRange and the indices here refer to m_constantImageLODs.
		TArray<uint32> m_constantImageLODIndices;

		//! Constant image data.
		TArray<FImageLODRange> m_constantImages;

        //! Constant mesh data: the first is the index in m_roms for each mesh or -1 if it is always loaded.
		TArray<TPair<int32, Ptr<const Mesh>>> m_constantMeshes;

        //! Constant string data
		TArray<string> m_constantStrings;

        //! Constant layout data
		TArray<Ptr<const Layout>> m_constantLayouts;

        //! Constant projectors
		TArray<PROJECTOR> m_constantProjectors;

        //! Constant matrices, usually used for transforms
		TArray<mat4f> m_constantMatrices;

		//! Constant projectors
		TArray<SHAPE> m_constantShapes;

        //! Constant curves
		TArray<Curve> m_constantCurves;

        //! Constant curves
		TArray<Ptr<const Skeleton>> m_constantSkeletons;

		//! Constant Physics Bodies
		TArray<Ptr<const PhysicsBody>> m_constantPhysicsBodies;

        //! Parameters of the model.
        //! The value stored here is the default value.
		TArray<PARAMETER_DESC> m_parameters;

        //! Ranges for interation of the model operations.
		TArray<RANGE_DESC> m_ranges;

        //! List of parameter lists. These are used in several places, like storing the
        //! pregenerated list of parameters influencing a resource.
        //! The parameter lists are sorted.
		TArray<TArray<uint16>> m_parameterLists;


        //!
        void Serialise( OutputArchive& arch ) const
        {
            arch << m_opAddress;
            arch << m_byteCode;
            arch << m_states;
			arch << m_roms;
			arch << m_constantImageLODs;
			arch << m_constantImageLODIndices;
			arch << m_constantImages;
			arch << m_constantMeshes;
			arch << m_constantStrings;
            arch << m_constantLayouts;
            arch << m_constantProjectors;
			arch << m_constantMatrices;
			arch << m_constantShapes;
            arch << m_constantCurves;
            arch << m_constantSkeletons;
			arch << m_constantPhysicsBodies;
            arch << m_parameters;
            arch << m_ranges;
            arch << m_parameterLists;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            arch >> m_opAddress;
            arch >> m_byteCode;
            arch >> m_states;
			arch >> m_roms;
			arch >> m_constantImageLODs;
			arch >> m_constantImageLODIndices;
			arch >> m_constantImages;
			arch >> m_constantMeshes;
			arch >> m_constantStrings;
            arch >> m_constantLayouts;
            arch >> m_constantProjectors;
			arch >> m_constantMatrices;
			arch >> m_constantShapes;
            arch >> m_constantCurves;
            arch >> m_constantSkeletons;
			arch >> m_constantPhysicsBodies;
            arch >> m_parameters;
            arch >> m_ranges;
            arch >> m_parameterLists;
        }

        //! Debug method that sanity-checks the program with a variety of tests.
        void Check();

        //! Debug method that logs the top used instruction types.
        void LogHistogram() const;

		//! Return true if the given ROM is loaded.
		inline bool IsRomLoaded( int32 RomIndex ) const
		{
			switch (m_roms[RomIndex].ResourceType)
			{
			case DT_IMAGE:
				return m_constantImageLODs[m_roms[RomIndex].ResourceIndex].Value.get() != nullptr;
			case DT_MESH:
				return m_constantMeshes[m_roms[RomIndex].ResourceIndex].Value.get() != nullptr;
			default:
				check(false);
				break;
			}
			return false;
		}

		//! Unload a rom resource
		inline void UnloadRom(int32 RomIndex)
		{
			if (DebugRom && (DebugRomAll||RomIndex == DebugRomIndex))
				UE_LOG(LogMutableCore, Log, TEXT("Unloading rom %d."), RomIndex);

			switch (m_roms[RomIndex].ResourceType)
			{
			case DT_IMAGE:
				m_constantImageLODs[m_roms[RomIndex].ResourceIndex].Value = nullptr;
				break;
			case DT_MESH:
				m_constantMeshes[m_roms[RomIndex].ResourceIndex].Value = nullptr;
				break;
			default:
				check(false);
				break;
			}
		}

        //! Adds a constant data and returns its constant index.
		int32 AddConstant(Ptr<const Image> pImage, int32 MinTextureResidentMipCount)
		{
			check(pImage->GetSizeX()*pImage->GetSizeY()>0);

			// Mips to store
			int32 MipsToStore = 1;
			if (MinTextureResidentMipCount > 0 && MinTextureResidentMipCount<255)
			{
				int32 MaxMipmaps = Image::GetMipmapCount( pImage->GetSizeX(), pImage->GetSizeY() );
				
				// This is not true. We may want the full mipmaps for fragments of images, regardless of the resident mip size.
				//MipsToStore = FMath::Max(1, MaxMipmaps-MinTextureResidentMipCount);
				// \TODO: Calculate the mip ranges that makes sense to store.
				MipsToStore = MaxMipmaps;
			}

			int32 FirstLODIndexIndex = m_constantImageLODIndices.Num();

			// Some images cannot be resized or mipmaped
			bool bCannotBeScaled = pImage->m_flags & Image::IF_CANNOT_BE_SCALED;
			if (bCannotBeScaled)
			{
				MipsToStore = 1;
			}

			// TODO:
			int32 CompressionQuality = 4;

			// TODO: Not efficient if we don't make mips (no need to clone base)
			// TODO: If the image already has mips, we will be duplicating them...
			Ptr<Image> pMip = pImage->ExtractMip(0);			
			for ( int Mip=0; Mip<MipsToStore; ++Mip )
			{
				check(pMip->GetFormat() == pImage->GetFormat());

				// Shrink the buffer to the minimum necessary size.
				uint32 CalculatedSize = pMip->CalculateDataSize();
				if (CalculatedSize)
				{
					pMip->m_data.SetNum(CalculatedSize);
				}

				// Ensure unique at mip level
				int32 MipIndex = -1;
				for (int32 CandidateMipIndex = 0; CandidateMipIndex < m_constantImageLODs.Num(); ++CandidateMipIndex)
				{
					const Image* pCandidate = m_constantImageLODs[CandidateMipIndex].Value.get();
					if (*pCandidate == *pMip)
					{
						// Reuse mip
						MipIndex = CandidateMipIndex;
					}
				}

				if (MipIndex < 0)
				{
					check(pMip->GetLODCount()==1);
					MipIndex = m_constantImageLODs.Add(TPair<int32, Ptr<const Image>>(-1, pMip ));
				}

				m_constantImageLODIndices.Add(uint32(MipIndex));

				// Generate next mip if necessary
				if (Mip + 1 < MipsToStore)
				{
					pMip = pMip->ExtractMip(1);
					check(pMip);
				}
			}

			int32 ImageIndex = m_constantImages.Add({ FirstLODIndexIndex, MipsToStore, pImage->GetSizeX(), pImage->GetSizeY() });
			return ImageIndex;
		}


		int32 AddConstant(Ptr<const Mesh> pMesh)
		{
			// Ensure unique
			for (int32 i = 0; i < m_constantMeshes.Num(); ++i)
			{
				const Mesh* pCandidate = m_constantMeshes[i].Value.get();
				if (*pCandidate == *pMesh)
				{
					return i;
				}
			}

			return m_constantMeshes.Add(TPair<int32, Ptr<const Mesh>>( -1, pMesh.get() ));
		}


		OP::ADDRESS AddConstant( Ptr<const Layout> pLayout )
        {
            // Ensure unique
            for ( size_t i=0; i<m_constantLayouts.Num(); ++i)
            {
                if (m_constantLayouts[i]==pLayout)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantLayouts.Num() );
            m_constantLayouts.Add( pLayout );
            return index;
        }

        OP::ADDRESS AddConstant( Ptr<const Skeleton> pSkeleton )
        {
            // Ensure unique
            for ( size_t i=0; i<m_constantSkeletons.Num(); ++i)
            {
                if ( m_constantSkeletons[i]==pSkeleton
                     ||
                     *m_constantSkeletons[i]==*pSkeleton
                     )
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantSkeletons.Num() );
            m_constantSkeletons.Add( pSkeleton );
            return index;
        }

        OP::ADDRESS AddConstant( Ptr<const PhysicsBody> pPhysicsBody )
        {
            // Ensure unique
            for ( size_t i=0; i<m_constantPhysicsBodies.Num(); ++i)
            {
                if ( m_constantPhysicsBodies[i]==pPhysicsBody
                     ||
                     *m_constantPhysicsBodies[i]==*pPhysicsBody
                     )
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantPhysicsBodies.Num() );
            m_constantPhysicsBodies.Add( pPhysicsBody );
            return index;
        }

        OP::ADDRESS AddConstant( const string& str )
        {            
            // Ensure unique
            for ( size_t i=0; i<m_constantStrings.Num(); ++i)
            {
                if (m_constantStrings[i]==str)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantStrings.Num() );
            m_constantStrings.Add( str );
            return index;
        }

        OP::ADDRESS AddConstant( const mat4f& m )
        {
            // Ensure unique
            for ( size_t i=0; i<m_constantMatrices.Num(); ++i)
            {
                if (m_constantMatrices[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantMatrices.Num() );
            m_constantMatrices.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const SHAPE& m )
        {
            // Ensure unique
            for ( size_t i=0; i<m_constantShapes.Num(); ++i)
            {
                if (m_constantShapes[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantShapes.Num() );
            m_constantShapes.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const PROJECTOR& m )
        {
            // Ensure unique
            for ( size_t i=0; i<m_constantProjectors.Num(); ++i)
            {
                if (m_constantProjectors[i]==m)
                {
                    return (OP::ADDRESS)i;
                }
            }

            OP::ADDRESS index = OP::ADDRESS( m_constantProjectors.Num() );
            m_constantProjectors.Add( m );
            return index;
        }

        OP::ADDRESS AddConstant( const Curve& m )
        {
            // Ensure unique
//            for ( size_t i=0; i<m_constantCurves.Num(); ++i)
//            {
//                if (m_constantCurves[i]==m)
//                {
//                    return (OP::ADDRESS)i;
//                }
//            }

            OP::ADDRESS index = OP::ADDRESS( m_constantCurves.Num() );
            m_constantCurves.Add( m );
            return index;
        }


        //! Get a constant image, assuming it is fully loaded. The image constant will be composed with lodaded mips if necessary.
        void GetConstant( int32 ConstantIndex, ImagePtrConst& res, int32 MipsToSkip) const
        {
			int32 ReallySkippedLODs = FMath::Min(m_constantImages[ConstantIndex].LODCount - 1, MipsToSkip);
			int32 FirstLODIndexIndex = m_constantImages[ConstantIndex].FirstIndex;
			int32 ResultLODIndexIndex = FirstLODIndexIndex + ReallySkippedLODs;
			int32 FinalLODs = m_constantImages[ConstantIndex].LODCount - ReallySkippedLODs;
			check(FinalLODs > 0);

			// Get the first mip
			int32 ResultLODIndex = m_constantImageLODIndices[ResultLODIndexIndex];
			Ptr<const Image> CurrentMip = m_constantImageLODs[ResultLODIndex].Value;
			check(CurrentMip);
				
			// Shortcut if we only want one mip
			if (FinalLODs == 1)
			{
				res = CurrentMip;
				return;
			}

			// Compose the result image
			{
				MUTABLE_CPUPROFILER_SCOPE(ComposeConstantImage);

				Ptr<Image> Result = new Image( CurrentMip->GetSizeX(), CurrentMip->GetSizeY(), FinalLODs, CurrentMip->GetFormat() );
				Result->m_flags = CurrentMip->m_flags;

				// Some non-block pixel formats require separate memory size calculation
				if (!Result->GetDataSize())
				{
					int32 TotalSize = 0;
					for (int32 LOD = 0; LOD < FinalLODs; ++LOD)
					{
						int LODIndex = m_constantImageLODIndices[ResultLODIndexIndex + LOD];
						int32 MipSizeBytes = m_constantImageLODs[LODIndex].Value->GetDataSize();
						TotalSize += MipSizeBytes;
					}
					Result->m_data.SetNum(TotalSize);
				}

				int32 WrittenDataBytes = 0;
				uint8* Data = Result->GetData();
				for (int32 LOD=0; LOD<FinalLODs; ++LOD)
				{
					check(CurrentMip->GetLODCount() == 1);
					check(CurrentMip->GetFormat() == Result->GetFormat());
					int32 MipSizeBytes = CurrentMip->GetDataSize();
					check( Result->GetDataSize()>=WrittenDataBytes+MipSizeBytes);

					FMemory::Memcpy( Data, CurrentMip->GetData(), MipSizeBytes);
					Data += MipSizeBytes;
					WrittenDataBytes += MipSizeBytes;

					if (LOD + 1 < FinalLODs)
					{
						ResultLODIndex = m_constantImageLODIndices[ResultLODIndexIndex + LOD + 1];
						CurrentMip = m_constantImageLODs[ResultLODIndex].Value;
						check(CurrentMip);
					}
				}

				res = Result;
			}
		}


        void GetConstant(int32 ConstantIndex, MeshPtrConst& res ) const
        {
			res = m_constantMeshes[ConstantIndex].Value;
		}


        inline OP_TYPE GetOpType( OP::ADDRESS at ) const
        {
            if (at>=OP::ADDRESS(m_opAddress.Num())) return OP_TYPE::NONE;

            OP_TYPE result;
            uint64 byteCodeAddress = m_opAddress[at];
			FMemory::Memcpy( &result, &m_byteCode[byteCodeAddress], sizeof(OP_TYPE) );
            check( result<OP_TYPE::COUNT);
            return result;
        }

        template<typename ARGS>
        inline const ARGS GetOpArgs( OP::ADDRESS at ) const
        {
            ARGS result;
            uint64 byteCodeAddress = m_opAddress[at];
            byteCodeAddress += sizeof(OP_TYPE);
			FMemory::Memcpy( &result, &m_byteCode[byteCodeAddress], sizeof(ARGS));
            return result;
        }

        inline const uint8* GetOpArgsPointer( OP::ADDRESS at ) const
        {
            uint64 byteCodeAddress = m_opAddress[at];
            byteCodeAddress += sizeof(OP_TYPE);
            const uint8* pData = (const uint8*)&m_byteCode[byteCodeAddress];
            return pData;
        }

        inline uint8* GetOpArgsPointer( OP::ADDRESS at )
        {
            uint64 byteCodeAddress = m_opAddress[at];
            byteCodeAddress += sizeof(OP_TYPE);
            uint8* pData = (uint8*)&m_byteCode[byteCodeAddress];
            return pData;
        }
    };


    //!
    class MUTABLERUNTIME_API Model::Private : public Base
    {
    public:

        //!
        PROGRAM m_program;

        //! Non-persistent, streamer-specific location information
        string m_location;

        //!
        void Serialise( OutputArchive& arch ) const
        {
            uint32 ver = 0;
            arch << ver;

            arch << m_program;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32 ver;
            arch >> ver;
            check(ver==0);

            arch >> m_program;
        }

        //! Management of generated resources
        //! @{

        //! This is used to uniquely identify a generated resource like meshes or images.
        struct RESOURCE_KEY
        {
            //! The id assigned to the generated resource.
            uint32 m_id;

            //! The last request operation for this resource
            uint32 m_lastRequestId;

            //! The address that generated this resource
            OP::ADDRESS m_rootAddress;

            //! An opaque blob with the values of the relevant parameters
			TArray<uint8> m_parameterValuesBlob;
        };

        //! The last id generated for a resource
        uint32 m_lastResourceKeyId = 0;

        //! The last id generated for a resource request. This is used to check the
        //! relevancy of the resources when flushing the cache
        uint32 m_lastResourceResquestId = 0;

        //! Cached ids for returned assets
        //! This is non-persistent runtime data
		TArray<RESOURCE_KEY> m_generatedResources;

        //! Get a resource key for a given resource with given parameter values.
        uint32 GetResourceKey( uint32 paramListIndex, OP::ADDRESS rootAt, const Parameters* pParams );

        //! @}

    };


    //!
    class MUTABLERUNTIME_API ModelParametersGenerator::Private : public Base
    {
    public:

        //!
        ModelPtrConst m_pModel;
        SystemPtr m_pSystem;

        //! Number of possible instances
        int64 m_instanceCount;

        //!
        struct PARAMETER_INTERVAL_VALUE
        {
            //! Minimum instance index to use these increments
            int m_minIndex;

            //! Parameter value in this interval
            int m_value;
        };

        //!
        struct PARAMETER_INTERVALS
        {
            //! As many entries as necessary
            //! Sorted by PARAMETER_INCREMENTS_PER_VALUE::m_minIndex
			TArray<PARAMETER_INTERVAL_VALUE> m_intervalValue;
        };

        //! An entry for every parameter in the model
		TArray< PARAMETER_INTERVALS > m_intervals;

        //! Whether to use brute force, or consider the parameter relevancy
        bool m_considerRelevancy;

        //! Build the interval information
		uint32 BuildIntervals( uint32 currentInstanceIndex, uint32 currentParameter, TArray<int>& currentValues );

        //! Get the parameter values for a particular instance index
		TArray<int> GetParameters( int instanceIndex );
    };

}
