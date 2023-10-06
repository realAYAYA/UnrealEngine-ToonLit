/*
 * VTUtils.hpp
 * Copyright (c) 2014 Eran Pe'er.
 *
 * This program is made available under the terms of the MIT License.
 * 
 * Created on Jun 3, 2014
 */
#pragma once

#include <functional>
#include <type_traits>
#include "mockutils/VirtualOffestSelector.hpp"
#include "union_cast.hpp"

#pragma warning(disable:4191)

namespace fakeit {
    class NoVirtualDtor {
    };

    class VTUtils {
    public:

        template<typename C, typename R, typename ... arglist>
        static unsigned int getOffset(R (C::*vMethod)(arglist...)) {
            auto sMethod = reinterpret_cast<unsigned int (VirtualOffsetSelector::*)(int)>(vMethod);
            VirtualOffsetSelector offsetSelctor;
            return (offsetSelctor.*sMethod)(0);
        }

        template<typename C>
        static typename std::enable_if<std::has_virtual_destructor<C>::value, unsigned int>::type
        getDestructorOffset() {
            VirtualOffsetSelector offsetSelctor;
            union_cast<C *>(&offsetSelctor)->~C();
            return offsetSelctor.offset;
        }

        template<typename C>
        static typename std::enable_if<!std::has_virtual_destructor<C>::value, unsigned int>::type
        getDestructorOffset() {
            throw NoVirtualDtor();
        }

        template<typename C>
        static unsigned int getVTSize() {
            struct Derrived : public C {
                virtual void endOfVt() {
                }
            };

            unsigned int vtSize = getOffset(&Derrived::endOfVt);
            return vtSize;
        }
    };


}

#pragma warning(default:4191)
