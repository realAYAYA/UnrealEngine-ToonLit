// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "parallel_for.h"

namespace embree
{
  template<typename Index, typename Value, typename Func, typename Reduction>
    __forceinline Value sequential_reduce( const Index first, const Index last, const Value& identity, const Func& func, const Reduction& reduction ) 
  {
    return func(range<Index>(first,last));
  }

  template<typename Index, typename Value, typename Func, typename Reduction>
    __forceinline Value sequential_reduce( const Index first, const Index last, const Index minStepSize, const Value& identity, const Func& func, const Reduction& reduction )
  {
    return func(range<Index>(first,last));
  }

  template<typename Index, typename Value, typename Func, typename Reduction>
    __noinline Value parallel_reduce_internal( Index taskCount, const Index first, const Index last, const Index minStepSize, const Value& identity, const Func& func, const Reduction& reduction )
  {
#if defined(__MIC__)
    const size_t maxTasks = 256;
#else
    const size_t maxTasks = 64; // FIXME: increase!!!!
#endif
    const size_t threadCount = TaskSchedulerTBB::threadCount();
    taskCount = min(taskCount,threadCount,maxTasks);

    /* parallel invokation of all tasks */
    Value values[maxTasks];
    parallel_for(taskCount, [&](const size_t taskIndex) {
        const size_t k0 = first+(taskIndex+0)*(last-first)/taskCount;
        const size_t k1 = first+(taskIndex+1)*(last-first)/taskCount;
        values[taskIndex] = func(range<Index>(k0,k1));
      });

    /* perform reduction over all tasks */
    Value v = identity;
    for (size_t i=0; i<taskCount; i++) v = reduction(v,values[i]);
    return v;
  }

  template<typename Index, typename Value, typename Func, typename Reduction>
    __forceinline Value parallel_reduce( const Index first, const Index last, const Index minStepSize, const Value& identity, const Func& func, const Reduction& reduction )
  {
#if defined(TASKING_LOCKSTEP) || defined(TASKING_TBB_INTERNAL)

    /* fast path for small number of iterations */
    Index taskCount = (last-first+minStepSize-1)/minStepSize;
    if (likely(taskCount == 1)) {
      return func(range<Index>(first,last));
    }
    return parallel_reduce_internal(taskCount,first,last,minStepSize,identity,func,reduction);

#else 
    return tbb::parallel_reduce(tbb::blocked_range<Index>(first,last,minStepSize),identity,
      [&](const tbb::blocked_range<Index>& r, const Value& start) { return reduction(start,func(range<Index>(r.begin(),r.end()))); },
      reduction);
    if (tbb::task::self().is_cancelled())
      throw std::runtime_error("task group cancelled");
#endif
  }

  template<typename Index, typename Value, typename Func, typename Reduction>
    __forceinline Value parallel_reduce( const Index first, const Index last, const Value& identity, const Func& func, const Reduction& reduction )
  {
    return parallel_reduce(first,last,Index(1),identity,func,reduction);
  }
}
