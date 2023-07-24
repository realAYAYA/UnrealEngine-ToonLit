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

#pragma once

#include "../sys/platform.h"
#include "../math/math.h"

namespace embree
{
  template<typename Ty>
    struct range 
    {
      __forceinline range () {}

      __forceinline range (const Ty& begin) 
      : _begin(begin), _end(begin+1) {}
      
      __forceinline range (const Ty& begin, const Ty& end) 
      : _begin(begin), _end(end) {}
      
      __forceinline Ty begin() const {
        return _begin;
      }
      
      __forceinline Ty end() const {
	return _end;
      }

      __forceinline range intersect(const range& r) const {
        return range (max(_begin,r._begin),min(_end,r._end));
      }

      __forceinline Ty size() const {
        return _end - _begin;
      }

      __forceinline bool empty() const { 
        return _end <= _begin; 
      }

      __forceinline std::pair<range,range> split() const 
      {
        const Ty _center = (_begin+_end)/2;
        return std::make_pair(range(_begin,_center),range(_center,_end));
      }

      __forceinline friend bool operator< (const range& r0, const range& r1) {
        return r0.size() < r1.size();
      }
	
      friend std::ostream& operator<<(std::ostream& cout, const range& r) {
        return cout << "range [" << r.begin() << ", " << r.end() << "]";
      }
      
      Ty _begin, _end;
    };

  template<typename Ty>
    range<Ty> make_range(const Ty& begin, const Ty& end) {
    return range<Ty>(begin,end);
  }

  template<typename Ty>
    struct extended_range 
    {
      __forceinline extended_range () {}

      __forceinline extended_range (const Ty& begin) 
        : _begin(begin), _end(begin+1), _ext_end(begin+1) {}
      
      __forceinline extended_range (const Ty& begin, const Ty& end) 
        : _begin(begin), _end(end), _ext_end(end) {}

      __forceinline extended_range (const Ty& begin, const Ty& end, const Ty& ext_end) 
        : _begin(begin), _end(end), _ext_end(ext_end) {}
      
      __forceinline Ty begin() const {
        return _begin;
      }
      
      __forceinline Ty end() const {
	return _end;
      }

      __forceinline Ty ext_end() const {
	return _ext_end;
      }

      __forceinline Ty size() const {
        return _end - _begin;
      }

      __forceinline Ty ext_size() const {
        return _ext_end - _begin;
      }

      __forceinline Ty ext_range_size() const {
        return _ext_end - _end;
      }

      __forceinline bool has_ext_range() const {
        assert(_ext_end >= _end);
        return (_ext_end - _end) > 0;
      }

      __forceinline void set_ext_range(const size_t ext_end){
        assert(ext_end >= _end);
        _ext_end = ext_end;
      }

      __forceinline void move_right(const size_t plus){
        _begin   += plus;
        _end     += plus;
        _ext_end += plus;
      }

      friend std::ostream& operator<<(std::ostream& cout, const extended_range& r) {
        return cout << "extended_range [" << r.begin() << ", " << r.end() <<  " (" << r.ext_end() << ")]";
      }
      
      Ty _begin, _end, _ext_end;
    };
}
