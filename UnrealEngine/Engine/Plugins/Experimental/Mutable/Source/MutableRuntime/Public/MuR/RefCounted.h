// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MutableMemory.h"
#include "MuR/Ptr.h"

#include "HAL/PlatformAtomics.h"
#include "HAL/LowLevelMemTracker.h"

#include <atomic>

namespace mu
{


    //! \brief %Base class for all reference counted objects.
    //!
    //! Any subclass of this class can be managed using smart pointers through the Ptr<T> template.
    //! \warning This base allow multi-threaded manipulation of smart pointers, since the count
    //! increments and decrements are atomic.
    //! \ingroup runtime
    class MUTABLERUNTIME_API RefCounted : public Base
    {
    public:

        inline void IncRef() const
        {
			FPlatformAtomics::InterlockedIncrement(&m_refCount);
        }

        inline void DecRef() const
        {
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

            // Warning: This has to be in the same line: we cannot check m_refCount after decrement
            // since another thread could have decremented it
            if (FPlatformAtomics::InterlockedDecrement(&m_refCount)==0)
            {
                delete this;
            }
        }

        inline int GetRefCount() const
        {
            return int( FPlatformAtomics::InterlockedExchange(&m_refCount, m_refCount) );
        }

        RefCounted( const RefCounted& ) = delete;
        RefCounted( const RefCounted&& ) = delete;
        RefCounted& operator=( const RefCounted& ) = delete;
        RefCounted& operator=( const RefCounted&& ) = delete;

    protected:

        RefCounted()
        {
            m_refCount = 0;
        }

        inline virtual ~RefCounted() = default;

    private:

        mutable volatile int32 m_refCount;

    };


    inline void mutable_ptr_add_ref( const RefCounted* p )
    {
        if (p) p->IncRef();
    }


    inline void mutable_ptr_release( const RefCounted* p )
    {
        if (p) p->DecRef();
    }


    //! RefCounted with weak pointer support
    class MUTABLERUNTIME_API RefCountedWeak : public Base
    {
    public:

        inline void IncRef() const
        {
			FPlatformAtomics::InterlockedIncrement(&m_refCount);
        }

        inline void DecRef() const
        {
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

            // Lock the control block to prevent creation of pointers while we work
            Ptr<ControlBlock> pControlBlock = m_pControlBlock;
            while (pControlBlock->m_lock.test_and_set(std::memory_order_acquire));

            // Warning: This has to be in the same line: we cannot check m_refCount after decrement
            // since another thread could have decremented it
            if (FPlatformAtomics::InterlockedDecrement(&m_refCount)==0)
            {
                delete this;
            }

            pControlBlock->m_lock.clear();
        }

        inline int GetRefCount() const
        {
            return int(FPlatformAtomics::InterlockedExchange(&m_refCount, m_refCount));
        }

        RefCountedWeak( const RefCountedWeak& ) = delete;
        RefCountedWeak( const RefCountedWeak&& ) = delete;
        RefCountedWeak& operator=( const RefCountedWeak& ) = delete;
        RefCountedWeak& operator=( const RefCountedWeak&& ) = delete;

    protected:

        RefCountedWeak()
        {
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

            m_refCount = 0;
            m_pControlBlock = new ControlBlock;
            m_pControlBlock->m_pObject = this;
        }

        inline virtual ~RefCountedWeak()
        {
            // Control block is locked as per DecRef
            m_pControlBlock->m_pObject = nullptr;
        }


    private:

        // Normal intrusive smart pointer support
        mutable volatile int32 m_refCount;

        // Weak pointer support
        class ControlBlock : public RefCounted
        {
        private:
            RefCountedWeak* m_pObject = nullptr;
            std::atomic_flag m_lock = ATOMIC_FLAG_INIT;

            friend class RefCountedWeak;
            template<class T> friend class WeakPtr;
        };

        Ptr<ControlBlock> m_pControlBlock;

        template<class T> friend class WeakPtr;

    };


    inline void mutable_ptr_add_ref( const RefCountedWeak* p )
    {
        if (p) p->IncRef();
    }


    inline void mutable_ptr_release( const RefCountedWeak* p )
    {
        if (p) p->DecRef();
    }


    //!
    template<class T>
    class WeakPtr
    {
    private:
        Ptr<RefCountedWeak::ControlBlock> m_pControlBlock;

    public:

        WeakPtr() = default;

        WeakPtr( const Ptr<const T>& ptr )
        {
            if (ptr)
            {
                m_pControlBlock = ptr->m_pControlBlock;
            }
        }

        inline Ptr<T> Pin()
        {
            Ptr<T> result;

            if (m_pControlBlock)
            {
                while (m_pControlBlock->m_lock.test_and_set(std::memory_order_acquire));
                result = (T*)m_pControlBlock->m_pObject;
                m_pControlBlock->m_lock.clear();
            }

            return result;
        }

		inline Ptr<const T> Pin() const
        {
            Ptr<const T> result;

            if (m_pControlBlock)
            {
                while (m_pControlBlock->m_lock.test_and_set(std::memory_order_acquire));
                result = (const T*)m_pControlBlock->m_pObject;
                m_pControlBlock->m_lock.clear();
            }

            return result;
        }

		inline void Reset()
        {
            m_pControlBlock = nullptr;
        }

		inline bool IsNull() const
        {
            return Pin().get()==nullptr;
        }
    };

}

