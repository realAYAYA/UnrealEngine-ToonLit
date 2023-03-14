// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <utility>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace terse {

template<class ArchiveImpl>
class Archive {
    public:
        explicit Archive(ArchiveImpl* impl_) : impl{impl_} {
        }

        template<typename ... Args>
        void operator()(Args&& ... args) {
            dispatch(std::forward<Args>(args)...);
        }

        template<class TSerializable>
        ArchiveImpl& operator<<(TSerializable& source) {
            dispatch(source);
            return *impl;
        }

        template<class TSerializable>
        ArchiveImpl& operator>>(TSerializable& dest) {
            dispatch(dest);
            return *impl;
        }

    protected:
        template<typename Head>
        void dispatch(Head&& head) {
            impl->process(std::forward<Head>(head));
        }

        template<typename Head, typename ... Tail>
        void dispatch(Head&& head, Tail&& ... tail) {
            dispatch(std::forward<Head>(head));
            dispatch(std::forward<Tail>(tail)...);
        }

    private:
        ArchiveImpl* impl;
};

}  // namespace terse
