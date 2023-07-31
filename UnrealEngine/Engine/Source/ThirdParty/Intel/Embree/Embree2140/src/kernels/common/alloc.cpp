// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#include "alloc.h"
#include "../common/sys/thread.h"

namespace embree
{
  struct fast_allocator_regression_test : public RegressionTest
  {
    BarrierSys barrier;
    std::atomic<size_t> numFailed;
    FastAllocator alloc;

    fast_allocator_regression_test() 
      : RegressionTest("fast_allocator_regression_test"), numFailed(0), alloc(nullptr,false) 
    {
      registerRegressionTest(this);
    }

    static void thread_alloc(fast_allocator_regression_test* This)
    {
      FastAllocator::ThreadLocal2* threadalloc = This->alloc.threadLocal2();

      size_t* ptrs[1000];
      for (size_t j=0; j<1000; j++)
      {
        This->barrier.wait();
        for (size_t i=0; i<1000; i++) {
          ptrs[i] = (size_t*) threadalloc->alloc0->malloc(sizeof(size_t)+(i%32));
          *ptrs[i] = size_t(threadalloc) + i;
        }
        for (size_t i=0; i<1000; i++) {
          if (*ptrs[i] != size_t(threadalloc) + i) 
            This->numFailed++;
        }
        This->barrier.wait();
      }
    }
    
    bool run ()
    {
      numFailed.store(0);

      size_t numThreads = getNumberOfLogicalThreads();
      barrier.init(numThreads+1);

      /* create threads */
      std::vector<thread_t> threads;
      for (size_t i=0; i<numThreads; i++)
        threads.push_back(createThread((thread_func)thread_alloc,this));

      /* run test */ 
      for (size_t i=0; i<1000; i++)
      {
        alloc.reset();
        barrier.wait();
        barrier.wait();
      }

      /* destroy threads */
      for (size_t i=0; i<numThreads; i++)
        join(threads[i]);

      return numFailed == 0;
    }
  };

  fast_allocator_regression_test fast_allocator_regression;
}


