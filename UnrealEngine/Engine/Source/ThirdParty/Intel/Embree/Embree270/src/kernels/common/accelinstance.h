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

#include "accel.h"
#include "builder.h"

namespace embree
{
  class AccelInstance : public Accel
  {
  public:
    AccelInstance (AccelData* accel, Builder* builder, Intersectors& intersectors)
      : accel(accel), builder(builder), Accel(AccelData::TY_ACCEL_INSTANCE,intersectors) {}

    void immutable () {
      delete builder; builder = nullptr;
    }

    ~AccelInstance() {
      delete builder; builder = nullptr;
      delete accel;   accel = nullptr;
    }

  public:
    void build (size_t threadIndex, size_t threadCount) {
      if (builder) builder->build(threadIndex,threadCount);
      bounds = accel->bounds;
    }

    void clear() {
      accel->clear();
      builder->clear();
    }

  private:
    AccelData* accel;
    Builder* builder;
  };
}
