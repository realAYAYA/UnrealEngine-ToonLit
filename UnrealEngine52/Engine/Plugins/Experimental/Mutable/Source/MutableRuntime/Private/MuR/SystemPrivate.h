// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/System.h"

#include "MuR/Settings.h"
#include "MuR/Operations.h"
#include "MuR/ImagePrivate.h"
#include "MuR/MutableString.h"
#include "MuR/MeshPrivate.h"
#include "MuR/InstancePrivate.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{

	//! Reference-counted colour to be stored in the cache
	class Colour : public RefCounted
	{
	public:
		Colour(FVector4f v = FVector4f()) : m_colour(v) {}
		FVector4f m_colour;
	};
	typedef Ptr<Colour> ColourPtr;


	//! Reference-counted bool to be stored in the cache
	class Bool : public RefCounted
	{
	public:
		Bool(bool v = false) : m_value(v) {}
		bool m_value;
	};
	typedef Ptr<Bool> BoolPtr;


	//! Reference-counted scalar to be stored in the cache
	class Scalar : public RefCounted
	{
	public:
		Scalar(float v = 0.0f) : m_value(v) {}
		float m_value;
	};
	typedef Ptr<Scalar> ScalarPtr;


	//! Reference-counted scalar to be stored in the cache
	class Int : public RefCounted
	{
	public:
		Int(int32 v = 0) : m_value(v) {}
		int32 m_value;
	};
	typedef Ptr<Int> IntPtr;


	//! Reference-counted scalar to be stored in the cache
	class Projector : public RefCounted
	{
	public:
		FProjector m_value;
	};
	typedef Ptr<Projector> ProjectorPtr;


	//! ExecutinIndex stores the location inside all ranges of the execution of a specific
	//! operation. The first integer on each pair is the dimension/range index in the program
	//! array of ranges, and the second integer is the value inside that range.
	//! The vector order is undefined.
	class ExecutionIndex : public  TArray<TPair<int32, int32>>
	{
	public:
		//! Set or add a value to the index
		void SetFromModelRangeIndex(uint16 rangeIndex, int rangeValue)
		{
			auto Index = IndexOfByPredicate([=](const ElementType& v) { return v.Key >= rangeIndex; });
			if (Index != INDEX_NONE && (*this)[Index].Key == rangeIndex)
			{
				// Update
				(*this)[Index].Value = rangeValue;
			}
			else
			{
				// Add new
				Push(ElementType(rangeIndex, rangeValue));
			}
		}

		//! Get the value of the index from the range index in the model.
		int GetFromModelRangeIndex(int modelRangeIndex) const
		{
			for (const auto& e : *this)
			{
				if (e.Key == modelRangeIndex)
				{
					return e.Value;
				}
			}
			return 0;
		}
	};


	/** This structure stores the data about an ongoing mutable operation that needs to be executed. */
	struct FScheduledOp
	{
		inline FScheduledOp()
		{
			Stage = 0;
			Type = EType::Full;
		}

		inline FScheduledOp(OP::ADDRESS InAt, const FScheduledOp& InOpTemplate, uint8 InStage = 0, uint32 InCustomState = 0)
		{
			check(InStage < 120);
			At = InAt;
			ExecutionOptions = InOpTemplate.ExecutionOptions;
			ExecutionIndex = InOpTemplate.ExecutionIndex;
			Stage = InStage;
			CustomState = InCustomState;
			Type = InOpTemplate.Type;
		}

		static inline FScheduledOp FromOpAndOptions(OP::ADDRESS InAt, const FScheduledOp& InOpTemplate, uint8 InExecutionOptions)
		{
			FScheduledOp r;
			r.At = InAt;
			r.ExecutionOptions = InExecutionOptions;
			r.ExecutionIndex = InOpTemplate.ExecutionIndex;
			r.Stage = 0;
			r.CustomState = InOpTemplate.CustomState;
			r.Type = InOpTemplate.Type;
			return r;
		}

		//! Address of the operation
		OP::ADDRESS At = 0;

		//! Additional custom state data that the operation can store. This is usually used to pass information
		//! between execution stages of an operation.
		uint32 CustomState = 0;

		//! Index of the operation execution: This is used for iteration of different ranges.
		//! It is an index into the CodeRunner::GetMemory()::m_rangeIndex vector.
		//! executionIndex 0 is always used for empty ExecutionIndex, which is the most common
		//! one.
		uint16 ExecutionIndex = 0;

		//! Additional execution options. Set externally to this op, it usually alters the result.
		//! For example, this is used to keep track of the mipmaps to skip in image operations.
		uint8 ExecutionOptions = 0;

		//! Internal stage of the operation.
		//! Stage 0 is usually scheduling of children, and 1 is execution. Some instructions
		//! may have more steges to schedule children that are optional for execution, etc.
		uint8 Stage : 7;

		//! Type of calculation we are requestinf for this operation.
		enum class EType : uint8
		{
			//! Execute the operation to calculate the full result
			Full,

			//! Execute the operation to obtain the descriptor of an image.
			ImageDesc
		};
		EType Type : 1;
	};

	inline uint32 GetTypeHash(const FScheduledOp& Op)
	{
		return HashCombine(::GetTypeHash(Op.At), HashCombine(::GetTypeHash(Op.Stage), ::GetTypeHash(Op.ExecutionIndex)));
	}


	//! A cache address is the operation plus the context of execution (iteration indices, etc...).
	struct FCacheAddress
	{
		/** The meaning of all these fields is the same than the FScheduledOp struct. */
		OP::ADDRESS At = 0;
		uint16 ExecutionIndex = 0;
		uint8 ExecutionOptions = 0;
		FScheduledOp::EType Type = FScheduledOp::EType::Full;

		FCacheAddress() {}

		FCacheAddress(OP::ADDRESS InAt, uint16 InExecutionIndex, uint8 InExecutionOptions)
		{
			At = InAt;
			ExecutionIndex = InExecutionIndex;
			ExecutionOptions = InExecutionOptions;
		}

		FCacheAddress(OP::ADDRESS InAt, const FScheduledOp& Item)
		{
			At = InAt;
			ExecutionIndex = Item.ExecutionIndex;
			ExecutionOptions = Item.ExecutionOptions;
			Type = Item.Type;
		}

		FCacheAddress(const FScheduledOp& Item)
		{
			At = Item.At;
			ExecutionIndex = Item.ExecutionIndex;
			ExecutionOptions = Item.ExecutionOptions;
			Type = Item.Type;
		}
	};


	inline uint32 GetTypeHash(const FCacheAddress& a)
	{
		return HashCombine(::GetTypeHash(a.At), a.ExecutionIndex);
	}


	//! Container that stores data per executable code operation (indexed by address and execution
	//! index).
	template<class DATA>
	class CodeContainer
	{
	public:

		void resize(size_t s)
		{
			m_index0.SetNum(s);
			FMemory::Memzero(m_index0.GetData(), s * sizeof(DATA));
		}

		uint32 size_code() const
		{
			return uint32(m_index0.Num());
		}

		void clear()
		{
			m_index0.Empty();
			m_otherIndex.Empty();
		}

		inline void erase(const FCacheAddress& at)
		{
			if (at.ExecutionIndex == 0 && at.ExecutionOptions == 0)
			{
				m_index0[at.At] = nullptr;
			}
			else
			{
				m_otherIndex.Remove(at);
			}
		}

		inline DATA get(const FCacheAddress& at) const
		{
			if (at.ExecutionIndex == 0 && at.ExecutionOptions == 0)
			{
				if (at.At < uint32(m_index0.Num()))
				{
					return m_index0[at.At];
				}
				else
				{
					return 0;
				}
			}
			else
			{
				const DATA* it = m_otherIndex.Find(at);
				if (it)
				{
					return *it;
				}
			}
			return 0;
		}

		inline const DATA* get_ptr(const FCacheAddress& at) const
		{
			if (at.ExecutionIndex == 0 && at.ExecutionOptions == 0)
			{
				if (at.At < uint32(m_index0.Num()))
				{
					return &m_index0[at.At];
				}
				else
				{
					return nullptr;
				}
			}
			else
			{
				const DATA* it = m_otherIndex.Find(at);
				if (it)
				{
					return it;
				}
			}
			return nullptr;
		}

		inline DATA& operator[](const FCacheAddress& at)
		{
			if (at.ExecutionIndex == 0 && at.ExecutionOptions == 0)
			{
				return m_index0[at.At];
			}
			else
			{
				return m_otherIndex.FindOrAdd(at);
			}
		}

		struct iterator
		{
		private:
			friend class CodeContainer<DATA>;
			const CodeContainer<DATA>* container;
			typename TArray<DATA>::TIterator it0;
			typename TMap<FCacheAddress, DATA>::TIterator it1;

			iterator(CodeContainer<DATA>* InContainer)
				: container(InContainer)
				, it0(InContainer->m_index0.CreateIterator())
				, it1(InContainer->m_otherIndex.CreateIterator())
			{
			}

		public:

			inline void operator++(int)
			{
				if (it0)
				{
					++it0;
				}
				else
				{
					++it1;
				}
			}

			iterator operator++()
			{
				if (it0)
				{
					++it0;
				}
				else
				{
					++it1;
				}
				return *this;
			}

			inline bool operator!=(const iterator& o) const
			{
				return it0 != o.it0 || it1 != o.it1;
			}

			inline DATA& operator*()
			{
				if (it0)
				{
					return *it0;
				}
				else
				{
					return it1->Value;
				}
			}

			inline FCacheAddress get_address() const
			{
				if (it0)
				{
					return { uint32_t(it0.GetIndex()), 0, 0};
				}
				else
				{
					return it1->Key;
				}
			}

			inline bool IsValid() const
			{
				return bool(it0) && bool(it1);
			}
		};

		inline iterator begin()
		{
			iterator it(this);
			return it;
		}

	private:
		// For index 0
		TArray<DATA> m_index0;

		// For index>0
		TMap<FCacheAddress, DATA> m_otherIndex;
	};


	/** Interface for storage of data while Mutable code is being executed. */
	class FProgramCache
	{
	private:

		TArray< ExecutionIndex, TInlineAllocator<4> > m_usedRangeIndices;

		// first value of the pair:
		// 0 : value not valid (not set).
		// 1 : valid, not worth freeing for memory
		// 2 : valid, worth freeing
		CodeContainer< TPair<int32, Ptr<const RefCounted>> > m_resources;

		// TODO
	public:

		//! Addressed with OP::ADDRESS. It is true if value for an image desc is valid.
		TArray<bool> m_descCache;

		//! The number of times an operation will be run for the current build operation.
		//! \todo: this could be optimised by merging in other CodeContainers here.
		CodeContainer<int> m_opHitCount;

	public:


		inline const ExecutionIndex& GetRageIndex(uint32_t i)
		{
			// Make sure we have the default element.
			if (m_usedRangeIndices.IsEmpty())
			{
				m_usedRangeIndices.Push(ExecutionIndex());
			}

			check(i < uint32_t(m_usedRangeIndices.Num()));
			return m_usedRangeIndices[i];
		}

		//!
		inline uint32_t GetRageIndexIndex(const ExecutionIndex& rangeIndex)
		{
			if (rangeIndex.IsEmpty())
			{
				return 0;
			}

			// Make sure we have the default element.
			if (m_usedRangeIndices.IsEmpty())
			{
				m_usedRangeIndices.Push(ExecutionIndex());
			}

			// Look for or add the new element.
			auto ElemIndex = m_usedRangeIndices.Find(rangeIndex);
			if (ElemIndex != INDEX_NONE)
			{
				return ElemIndex;
			}

			m_usedRangeIndices.Push(rangeIndex);
			return uint32_t(m_usedRangeIndices.Num()) - 1;
		}

		void Init(size_t size)
		{
			m_resources.resize(size);
			m_opHitCount.resize(size);
		}

		void SetUnused(FCacheAddress at)
		{
			//UE_LOG(LogMutableCore, Log, TEXT("memory SetUnused : %5d "), at.At);
			if (m_resources[at].Key >= 2)
			{
				// Keep the result anyway if it doesn't use any memory.
				if (m_resources[at].Value)
				{
					m_resources[at].Value = nullptr;
					m_resources[at].Key = 0;
				}
			}
		}


		bool IsValid(FCacheAddress at)
		{
			if (at.At == 0) return false;

			// Is it a desc data query?
			if (at.Type == FScheduledOp::EType::ImageDesc)
			{
				if (uint32(m_descCache.Num()) > at.At)
				{
					return m_descCache[at.At];
				}
				else
				{
					return false;
				}
			}

			// It's a full data query.
			auto d = m_resources.get_ptr(at);
			return d && d->Key != 0;
		}


		void ClearCacheLayer0()
		{
			MUTABLE_CPUPROFILER_SCOPE(ClearLayer0);

			CodeContainer<int>::iterator it = m_opHitCount.begin();
			for (; it.IsValid(); ++it)
			{
				int32 Count = *it;
				if (Count < 3000000)
				{
					SetUnused(it.get_address());
					*it = 0;
				}
			}
		}


		void ClearCacheLayer1()
		{
			MUTABLE_CPUPROFILER_SCOPE(ClearLayer1);

			CodeContainer<int>::iterator it = m_opHitCount.begin();
			for (; it.IsValid(); ++it)
			{
				m_resources[it.get_address()].Value = nullptr;
				m_resources[it.get_address()].Key = 0;
			}

			m_descCache.SetNum(0);
		}


		void Clear()
		{
			MUTABLE_CPUPROFILER_SCOPE(ProgramCacheClear);

			size_t codeSize = m_resources.size_code();
			m_resources.clear();
			m_resources.resize(codeSize);
			m_descCache.SetNum(0);
			m_opHitCount.clear();
			m_opHitCount.resize(codeSize);
		}


		bool GetBool(FCacheAddress at)
		{
			if (!at.At) return false;
			auto d = m_resources.get_ptr(at);
			if (!d) return false;

			Ptr<const Bool> pResult;
			if (at.At)
			{
				pResult = (const Bool*)d->Value.get();
			}
			return pResult ? pResult->m_value : false;
		}

		float GetScalar(FCacheAddress at)
		{
			if (!at.At) return 0.0f;
			auto d = m_resources.get_ptr(at);
			if (!d) return 0.0f;

			Ptr<const Scalar> pResult;
			if (at.At)
			{
				pResult = (const Scalar*)d->Value.get();
			}
			return pResult ? pResult->m_value : 0.0f;
		}

		int GetInt(FCacheAddress at)
		{
			if (!at.At) return 0;
			auto d = m_resources.get_ptr(at);
			if (!d) return 0;

			Ptr<const Int> pResult;
			if (at.At)
			{
				pResult = (const Int*)d->Value.get();
			}
			return pResult ? pResult->m_value : 0;
		}

		FVector4f GetColour(FCacheAddress at)
		{
			if (!at.At) return FVector4f();
			auto d = m_resources.get_ptr(at);
			if (!d) return FVector4f();

			Ptr<const Colour> pResult;
			if (at.At)
			{
				pResult = (const Colour*)d->Value.get();
			}
			return pResult ? pResult->m_colour : FVector4f();
		}

		Ptr<const Projector> GetProjector(FCacheAddress at)
		{
			if (!at.At) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Projector> pResult;
			if (at.At)
			{
				pResult = (const Projector*)d->Value.get();
			}
			return pResult;
		}

		Ptr<const Instance> GetInstance(FCacheAddress at)
		{
			if (!at.At) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Instance> pResult = (const Instance*)d->Value.get();

			// We need to decrease the hitcount even if the result is null.
			// Lower hit counts means we shouldn't clear the value
			if (m_opHitCount[at] > 0)
			{
				--m_opHitCount[at];

				if (m_opHitCount[at] <= 0)
				{
					SetUnused(at);
				}
			}

			return pResult;
		}


		Ptr<const Layout> GetLayout(FCacheAddress at)
		{
			if (!at.At)
			{
				return nullptr;
			}
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Layout> pResult;
			if (at.At)
			{
				pResult = (const Layout*)d->Value.get();
			}
			return pResult;
		}

		Ptr<const String> GetString(FCacheAddress at)
		{
			if (!at.At)
			{
				return nullptr;
			}
			auto d = m_resources.get_ptr(at);
			if (!d)
				return nullptr;

			Ptr<const String> pResult;
			if (at.At)
			{
				pResult = (const String*)d->Value.get();
			}
			return pResult;
		}

		void SetBool(FCacheAddress at, bool v)
		{
			check(at.At < m_resources.size_code());

			Ptr<Bool> pResult = new Bool;
			pResult->m_value = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetValidDesc(FCacheAddress at)
		{
			check(at.Type == FScheduledOp::EType::ImageDesc);
			check(at.At < uint32(m_descCache.Num()));

			m_descCache[at.At] = true;
		}

		void SetScalar(FCacheAddress at, float v)
		{
			check(at.At < m_resources.size_code());

			Ptr<Scalar> pResult = new Scalar;
			pResult->m_value = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetInt(FCacheAddress at, int v)
		{
			check(at.At < m_resources.size_code());

			Ptr<Int> pResult = new Int;
			pResult->m_value = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetColour(FCacheAddress at, const FVector4f& v)
		{
			check(at.At < m_resources.size_code());

			Ptr<Colour> pResult = new Colour;
			pResult->m_colour = v;
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, pResult);
		}

		void SetProjector(FCacheAddress at, Ptr<const Projector> v)
		{
			check(at.At < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, v);
		}

		void SetInstance(FCacheAddress at, Ptr<const Instance> v)
		{
			check(at.At < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(2, v);
		}

		void SetImage(FCacheAddress at, Ptr<const Image> v)
		{
			check(at.At < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(2, v);
		}

		void SetMesh(FCacheAddress at, Ptr<const Mesh> v)
		{
			// debug
//            if (v)
//            {
//                v->GetPrivate()->CheckIntegrity();
//            }

			check(at.At < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(2, v);
		}

		void SetLayout(FCacheAddress at, Ptr<const Layout> v)
		{
			check(at.At < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, v);
		}

		void SetString(FCacheAddress at, Ptr<const String> v)
		{
			check(at.At < m_resources.size_code());
			m_resources[at] = TPair<int, Ptr<const RefCounted>>(1, v);
		}


		inline void IncreaseHitCount(FCacheAddress at)
		{
			m_opHitCount[at] += 1;
		}

		inline void SetForceCached(OP::ADDRESS at)
		{
			m_opHitCount[{at, 0, 0}] = 0xffffff;
		}


		Ptr<const Image> GetImage(FCacheAddress at)
		{
			if (!at.At) return nullptr;
			if (size_t(at.At) >= m_resources.size_code()) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Image> pResult = (const Image*)d->Value.get();

			// We need to decrease the hit count even if the result is null.
			// Lower hit counts means we shouldn't clear the value
			if ( m_opHitCount[at] > 0)
			{
				--m_opHitCount[at];
				if (m_opHitCount[at] <= 0)
				{
					SetUnused(at);
				}
			}

			return pResult;
		}

		Ptr<const Mesh> GetMesh(FCacheAddress at)
		{
			if (!at.At) return nullptr;
			if (size_t(at.At) >= m_resources.size_code()) return nullptr;
			auto d = m_resources.get_ptr(at);
			if (!d) return nullptr;

			Ptr<const Mesh> pResult = (const Mesh*)d->Value.get();

			// We need to decrease the hitcount even if the result is null.
			// Lower hit counts means we shouldn't clear the value
			if ( m_opHitCount[at] > 0)
			{
				--m_opHitCount[at];

				if (m_opHitCount[at] <= 0)
				{
					SetUnused(at);
				}
			}

			return pResult;
		}
	};


	inline bool operator==(const FCacheAddress& a, const FCacheAddress& b)
	{
		return a.At == b.At
			&&
			a.ExecutionIndex == b.ExecutionIndex
			&&
			a.ExecutionOptions == b.ExecutionOptions
			&&
			a.Type == b.Type;
	}

	inline bool operator<(const FCacheAddress& a, const FCacheAddress& b)
	{
		if (a.At < b.At) return true;
		if (a.At > b.At) return false;
		if (a.ExecutionIndex < b.ExecutionIndex) return true;
		if (a.ExecutionIndex > b.ExecutionIndex) return false;
		if (a.ExecutionOptions < b.ExecutionOptions) return true;
		if (a.ExecutionOptions > b.ExecutionOptions) return false;
		return a.Type < b.Type;
	}



    //---------------------------------------------------------------------------------------------
    //! Struct to manage models for which we have streamed rom data.
    //---------------------------------------------------------------------------------------------
    struct FModelCache
    {
		~FModelCache()
		{
			MUTABLE_CPUPROFILER_SCOPE(FModelCacheDestructor);
		}

        struct FModelCacheEntry
        {
            //!
            TWeakPtr<const Model> m_pModel;

            // Rom streaming management
            TArray< TPair<uint64,uint64> > m_romWeight;
        };

        // Rom streaming management
        uint64 m_romBudget = 0;

        uint64 m_romTick = 0;
        TArray< FModelCacheEntry > m_cachePerModel;

        //! Make sure the cached memory is below the internal budget, even counting with the
        //! passed additional memory.
		uint64 EnsureCacheBelowBudget(uint64 AdditionalMemory, 
			TFunctionRef<bool(const Model*, int)> IsRomLockedFunc = [](const Model*, int) {return false;});

        //!
        void UpdateForLoad( int romIndex, const TSharedPtr<const Model>& pModel, TFunctionRef<bool(const Model*,int)> isRomLockedFunc );
        void MarkRomUsed( int romIndex, const TSharedPtr<const Model>& pModel );

        //! Private helper
		FModelCacheEntry& GetModelCache(const TSharedPtr<const Model>&);
    };


    //---------------------------------------------------------------------------------------------
    //! Abstract private system interface
    //---------------------------------------------------------------------------------------------
    class System::Private : public Base
    {
    public:

        Private( SettingsPtr pSettings );
        virtual ~Private();

        //-----------------------------------------------------------------------------------------
        //! Own interface
        //-----------------------------------------------------------------------------------------

		/** This method can be used to internally prepare for code execution. */
		MUTABLERUNTIME_API void BeginBuild(const TSharedPtr<const Model>&);
		MUTABLERUNTIME_API void EndBuild();

		MUTABLERUNTIME_API bool BuildBool(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API int BuildInt(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API float BuildScalar(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API void BuildColour(const TSharedPtr<const Model>&, const Parameters*, OP::ADDRESS, float* OutR, float* OutG, float* OutB, float* OutA) ;
		MUTABLERUNTIME_API Ptr<const String> BuildString(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API Ptr<const Image> BuildImage(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at, int32 MipsToSkip) ;
		MUTABLERUNTIME_API Ptr<const Mesh> BuildMesh(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		MUTABLERUNTIME_API Ptr<const Layout> BuildLayout(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
    	MUTABLERUNTIME_API Ptr<const Projector> BuildProjector(const TSharedPtr<const Model>&, const Parameters* pParams, OP::ADDRESS at) ;
		void ClearCache() ;
		void SetStreamingCache(uint64 bytes) ;

        //!
        SettingsPtrConst m_pSettings;

        //! Data streaming interface, if any.
        //! It is owned by this system
        ModelStreamer* m_pStreamInterface;

        //! It is not owned by this system
        ImageParameterGenerator* m_pImageParameterGenerator;

        //! Maximum amount of memory (bytes) if a limit has been set. If 0, it means there is no
        //! limit.
        uint32 m_maxMemory;

        //! Current instances management
        //! @{

		/** Data for an instance that is currently being processed in the mutable system. This means it is
		* between a BeginUpdate and EndUpdate, or during an atmoic operation (like generate a single resource).
		*/
        struct FLiveInstance
        {
            Instance::ID m_instanceID;
            InstancePtrConst m_pInstance;
            TSharedPtr<const Model> m_pModel;

            int m_state;
            ParametersPtr m_pOldParameters;

			//! Mask of the parameters that have changed since the last update.
			//! Every bit represents a state parameter.
			uint64 m_updatedParameters = 0;

			//! An entry for every instruction in the program to cache resources (meshes, images) if necessary.
			TSharedPtr<FProgramCache> m_memory;

			~FLiveInstance()
			{
				// Manually done to trace mem deallocations
				MUTABLE_CPUPROFILER_SCOPE(LiveInstanceDestructor);
				m_memory = nullptr;
				m_pOldParameters = nullptr;
				m_pInstance = nullptr;
				m_pModel = nullptr;
			}
        };

        TArray<FLiveInstance> m_liveInstances;

		/** Temporary reference to the memory of the current instance being updated. */
		TSharedPtr<FProgramCache> m_memory;

		/** Counter used to generate unique IDs for every new instance created in the system. */
        Instance::ID m_lastInstanceID = 0;

		/** The pointer returned by this function is only valid for the duration of the current mutable operation. */
        inline FLiveInstance* FindLiveInstance( Instance::ID id )
        {
            for ( int32 i=0; i<m_liveInstances.Num(); ++i )
            {
                if (m_liveInstances[i].m_instanceID==id)
                {
                    return &m_liveInstances[i];
                }
            }
            return nullptr;
        }

        //! @}

        //!
        bool CheckUpdatedParameters( const FLiveInstance* pLiveInstance,
			const Ptr<const Parameters>& pParams,
			uint64& OutUpdatedParameters );

        // Profile metrics
        std::atomic<uint64> m_profileMetrics[ size_t(ProfileMetric::_Count) ];


	public:

		void RunCode(const TSharedPtr<const Model>& pModel,
			const Parameters* pParams,
			OP::ADDRESS at, uint32 LODs = System::AllLODs, uint8 executionOptions = 0);

		//!
		void PrepareCache(const Model*, int32 State);

		//! Cache related members
		//! @{

		//! Data for each used model.
		FModelCache m_modelCache;

		//! @}

	private:

		/** This flag is turned on when a streaming error or similar happens. Results are not usable.
		* This should only happen in-editor.
		*/
		bool bUnrecoverableError = false;

    };

}
