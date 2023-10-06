/*******************************************************************************
* Author    :  Angus Johnson                                                   *
* Version   :  Clipper2 - ver.1.0.4                                            *
* Date      :  4 August 2022                                                   *
* Website   :  http://www.angusj.com                                           *
* Copyright :  Angus Johnson 2010-2022                                         *
* Purpose   :  This module provides a simple interface to the Clipper Library  *
* License   :  http://www.boost.org/LICENSE_1_0.txt                            *
*******************************************************************************/

// @UE BEGIN
#pragma once
// @UE END

// @UE BEGIN
// for int64, PI
#include "HAL/Platform.h"
#include "Math/UnrealMathUtility.h"
// @UE END

#include <cstdlib>
#include <vector>

// @UE BEGIN
// include paths
#include "ThirdParty/clipper/clipper.core.h"
#include "ThirdParty/clipper/clipper.engine.h"
#include "ThirdParty/clipper/clipper.offset.h"
#include "ThirdParty/clipper/clipper.minkowski.h"
// @UE END

namespace Clipper2Lib 
{
  static const Rect64 MaxInvalidRect64 = Rect64(
    (std::numeric_limits<int64>::max)(),
    (std::numeric_limits<int64>::max)(),
    (std::numeric_limits<int64>::lowest)(),
    (std::numeric_limits<int64>::lowest)());

  static const RectD MaxInvalidRectD = RectD(
      (std::numeric_limits<double>::max)(),
      (std::numeric_limits<double>::max)(),
      (std::numeric_limits<double>::lowest)(),
      (std::numeric_limits<double>::lowest)());

  inline Paths64 BooleanOp(ClipType cliptype, FillRule fillrule,
    const Paths64& subjects, const Paths64& clips)
  {
    Paths64 result;
    Clipper64 clipper;
    clipper.AddSubject(subjects);
    clipper.AddClip(clips);
    clipper.Execute(cliptype, fillrule, result);
    return result;
  }

  inline void BooleanOp(ClipType cliptype, FillRule fillrule,
    const Paths64& subjects, const Paths64& clips, PolyTree64& solution)
  {
    Paths64 sol_open;
    Clipper64 clipper;
    clipper.AddSubject(subjects);
    clipper.AddClip(clips);
    clipper.Execute(cliptype, fillrule, solution, sol_open);
  }
  inline PathsD BooleanOp(ClipType cliptype, FillRule fillrule,
    const PathsD& subjects, const PathsD& clips, int decimal_prec = 2)
  {
  	// @UE BEGIN
  	// No exceptions
    // if (decimal_prec > 8 || decimal_prec < -8)
    //  throw Clipper2Exception("invalid decimal precision");
    // @UE END
    PathsD result;
    ClipperD clipper(decimal_prec);
    clipper.AddSubject(subjects);
    clipper.AddClip(clips);
    clipper.Execute(cliptype, fillrule, result);
    return result;
  }

  inline Paths64 Intersect(const Paths64& subjects, const Paths64& clips, FillRule fillrule)
  {
    return BooleanOp(ClipType::Intersection, fillrule, subjects, clips);
  }
  
  inline PathsD Intersect(const PathsD& subjects, const PathsD& clips, FillRule fillrule, int decimal_prec = 2)
  {
    return BooleanOp(ClipType::Intersection, fillrule, subjects, clips, decimal_prec);
  }

  inline Paths64 Union(const Paths64& subjects, const Paths64& clips, FillRule fillrule)
  {
    return BooleanOp(ClipType::Union, fillrule, subjects, clips);
  }

  inline PathsD Union(const PathsD& subjects, const PathsD& clips, FillRule fillrule, int decimal_prec = 2)
  {
    return BooleanOp(ClipType::Union, fillrule, subjects, clips, decimal_prec);
  }

  inline Paths64 Union(const Paths64& subjects, FillRule fillrule)
  {
    Paths64 result;
    Clipper64 clipper;
    clipper.AddSubject(subjects);
    clipper.Execute(ClipType::Union, fillrule, result);
    return result;
  }

  inline PathsD Union(const PathsD& subjects, FillRule fillrule, int decimal_prec = 2)
  {
  	// @UE BEGIN
  	// No exceptions
    // if (decimal_prec > 8 || decimal_prec < -8)
    //  throw Clipper2Exception("invalid decimal precision");
    // @UE END
    PathsD result;
    ClipperD clipper(decimal_prec);
    clipper.AddSubject(subjects);
    clipper.Execute(ClipType::Union, fillrule, result);
    return result;
  }

  inline Paths64 Difference(const Paths64& subjects, const Paths64& clips, FillRule fillrule)
  {
    return BooleanOp(ClipType::Difference, fillrule, subjects, clips);
  }

  inline PathsD Difference(const PathsD& subjects, const PathsD& clips, FillRule fillrule, int decimal_prec = 2)
  {
    return BooleanOp(ClipType::Difference, fillrule, subjects, clips, decimal_prec);
  }

  inline Paths64 Xor(const Paths64& subjects, const Paths64& clips, FillRule fillrule)
  {
    return BooleanOp(ClipType::Xor, fillrule, subjects, clips);
  }

  inline PathsD Xor(const PathsD& subjects, const PathsD& clips, FillRule fillrule, int decimal_prec = 2)
  {
    return BooleanOp(ClipType::Xor, fillrule, subjects, clips, decimal_prec);
  }

  inline bool IsFullOpenEndType(EndType et)
  {
    return (et != EndType::Polygon) && (et != EndType::Joined);
  }

  inline Paths64 InflatePaths(const Paths64& paths, double delta,
    JoinType jt, EndType et, double miter_limit = 2.0)
  {
    ClipperOffset clip_offset(miter_limit);
    clip_offset.AddPaths(paths, jt, et);
    return clip_offset.Execute(delta);
  }

  // @UE BEGIN
  // Exceptions not supported, but unused function
  /*
  inline PathsD InflatePaths(const PathsD& paths, double delta,
    JoinType jt, EndType et, double miter_limit = 2.0, double precision = 2)
  {
    if (precision < -8 || precision > 8)
      throw new Clipper2Exception("Error: Precision exceeds the allowed range.");
    const double scale = std::pow(10, precision);
    ClipperOffset clip_offset(miter_limit);
    clip_offset.AddPaths(ScalePaths<int64,double>(paths, scale), jt, et);
    Paths64 tmp = clip_offset.Execute(delta * scale);
    return ScalePaths<double, int64>(tmp, 1 / scale);
  }
  */
  // @UE END

  inline Path64 TranslatePath(const Path64& path, int64 dx, int64 dy)
  {
    Path64 result;
    result.reserve(path.size());
    for (const Point64& pt : path)
      result.push_back(Point64(pt.x + dx, pt.y + dy));
    return result;
  }

  inline PathD TranslatePath(const PathD& path, double dx, double dy)
  {
    PathD result;
    result.reserve(path.size());
    for (const PointD& pt : path)
      result.push_back(PointD(pt.x + dx, pt.y + dy));
    return result;
  }

  inline Paths64 TranslatePaths(const Paths64& paths, int64 dx, int64 dy)
  {
    Paths64 result;
    result.reserve(paths.size());
    for (const Path64& path : paths)
      result.push_back(TranslatePath(path, dx, dy));
    return result;
  }

  inline PathsD TranslatePaths(const PathsD& paths, double dx, double dy)
  {
    PathsD result;
    result.reserve(paths.size());
    for (const PathD& path : paths)
      result.push_back(TranslatePath(path, dx, dy));
    return result;
  }

  inline Rect64 Bounds(const Path64& path)
  {
    Rect64 rec = MaxInvalidRect64;
    for (const Point64& pt : path)
    {
      if (pt.x < rec.left) rec.left = pt.x;
      if (pt.x > rec.right) rec.right = pt.x;
      if (pt.y < rec.top) rec.top = pt.y;
      if (pt.y > rec.bottom) rec.bottom = pt.y;
    }
    if (rec.IsEmpty()) return Rect64();
    return rec;
  }
  
  inline Rect64 Bounds(const Paths64& paths)
  {
    Rect64 rec = MaxInvalidRect64;
    for (const Path64& path : paths)
      for (const Point64& pt : path)
      {
        if (pt.x < rec.left) rec.left = pt.x;
        if (pt.x > rec.right) rec.right = pt.x;
        if (pt.y < rec.top) rec.top = pt.y;
        if (pt.y > rec.bottom) rec.bottom = pt.y;
      }
    if (rec.IsEmpty()) return Rect64();
    return rec;
  }

  inline RectD Bounds(const PathD& path)
  {
    RectD rec = MaxInvalidRectD;
    for (const PointD& pt : path)
    {
      if (pt.x < rec.left) rec.left = pt.x;
      if (pt.x > rec.right) rec.right = pt.x;
      if (pt.y < rec.top) rec.top = pt.y;
      if (pt.y > rec.bottom) rec.bottom = pt.y;
    }
    if (rec.IsEmpty()) return RectD();
    return rec;
  }

  inline RectD Bounds(const PathsD& paths)
  {
    RectD rec = MaxInvalidRectD;
    for (const PathD& path : paths)
      for (const PointD& pt : path)
      {
        if (pt.x < rec.left) rec.left = pt.x;
        if (pt.x > rec.right) rec.right = pt.x;
        if (pt.y < rec.top) rec.top = pt.y;
        if (pt.y > rec.bottom) rec.bottom = pt.y;
      }
    if (rec.IsEmpty()) return RectD();
    return rec;
  }

  namespace details
  {

    template <typename T>
    inline void InternalPolyNodeToPaths(const PolyPath<T>& polypath, Paths<T>& paths)
    {
      paths.push_back(polypath.Polygon());
      for (auto child : polypath)
        InternalPolyNodeToPaths(*child, paths);
    }

    inline bool InternalPolyPathContainsChildren(const PolyPath64& pp)
    {
      for (auto child : pp)
      {
        for (const Point64& pt : child->Polygon())
          if (PointInPolygon(pt, pp.Polygon()) == PointInPolygonResult::IsOutside)
            return false;
        if (child->Count() > 0 && !InternalPolyPathContainsChildren(*child))
          return false;
      }
      return true;
    }

    inline bool GetInt(std::string::const_iterator& iter, const
      std::string::const_iterator& end_iter, int64& val)
    {
      val = 0;
      bool is_neg = *iter == '-';
      if (is_neg) ++iter;
      std::string::const_iterator start_iter = iter;
      while (iter != end_iter &&
        ((*iter >= '0') && (*iter <= '9')))
      {
        val = val * 10 + (static_cast<int64>(*iter++) - '0');
      }
      if (is_neg) val = -val;
      return (iter != start_iter);
    }

    inline bool GetFloat(std::string::const_iterator& iter, const 
      std::string::const_iterator& end_iter, double& val)
    {
      val = 0;
      bool is_neg = *iter == '-';
      if (is_neg) ++iter;
      int dec_pos = -1;
      std::string::const_iterator start_iter = iter;
      while (iter != end_iter && (*iter == '.' ||
        ((*iter >= '0') && (*iter <= '9'))))
      {
        if (*iter == '.')
        {
          if (dec_pos >= 0) return false;
          dec_pos = 0;
          ++iter;
          continue;
        }

        if (dec_pos >= 0) dec_pos++;
        // @UE BEGIN
        // explicit casts
        val = val * 10 + static_cast<double>((int64)*iter++ - '0');
        // @UE END
      }
      if (iter == start_iter || dec_pos == 0) return false;
      if (dec_pos > 0)
        val *= std::pow(10, -dec_pos);
      if (is_neg)
        val *= -1;
      return true;
    }

    inline void SkipWhiteSpace(std::string::const_iterator& iter, 
      const std::string::const_iterator& end_iter)
    {
      while (iter != end_iter && *iter <= ' ') ++iter;
    }

    inline void SkipSpacesWithOptionalComma(std::string::const_iterator& iter, 
      const std::string::const_iterator& end_iter)
    {
      bool comma_seen = false;
      while (iter != end_iter)
      {
        if (*iter == ' ') ++iter;
        else if (*iter == ',')
        {
          if (comma_seen) return; // don't skip 2 commas!
          comma_seen = true;
          ++iter;
        }
        else return;                
      }
    }

    inline bool has_one_match(const char c, char* chrs)
    {
      while (*chrs > 0 && c != *chrs) ++chrs;
      if (!*chrs) return false;
      *chrs = ' '; // only match once per char
      return true;
    }


    inline void SkipUserDefinedChars(std::string::const_iterator& iter,
      const std::string::const_iterator& end_iter, const std::string& skip_chars)
    {
      const size_t MAX_CHARS = 16;
      char buff[MAX_CHARS] = {0};
      std::copy(skip_chars.cbegin(), skip_chars.cend(), &buff[0]);
      while (iter != end_iter && 
        (*iter <= ' ' || has_one_match(*iter, buff))) ++iter;
      return;
    }

  } // end details namespace 

  template <typename T>
  inline Paths<T> PolyTreeToPaths(const PolyTree<T>& polytree)
  {
    Paths<T> result;
    for (auto child : polytree)
      details::InternalPolyNodeToPaths(*child, result);
    return result;
  }

  inline bool CheckPolytreeFullyContainsChildren(const PolyTree64& polytree)
  {
    for (auto child : polytree)
      if (child->Count() > 0 && !details::InternalPolyPathContainsChildren(*child))
        return false;
    return true;
  }

  inline Path64 MakePath(const std::string& s)
  {
  	const std::string skip_chars = " ,(){}[]";
    Path64 result;
    std::string::const_iterator s_iter = s.cbegin();
    details::SkipUserDefinedChars(s_iter, s.cend(), skip_chars);
    while (s_iter != s.cend())
    {
      int64 y = 0, x = 0;
      if (!details::GetInt(s_iter, s.cend(), x)) break;
      details::SkipSpacesWithOptionalComma(s_iter, s.cend());
      if (!details::GetInt(s_iter, s.cend(), y)) break;
      result.push_back(Point64(x, y));
      details::SkipUserDefinedChars(s_iter, s.cend(), skip_chars);
    }
    return result;
  }
  
  inline PathD MakePathD(const std::string& s)
  {
    const std::string skip_chars = " ,(){}[]";
    PathD result;
    std::string::const_iterator s_iter = s.cbegin();
    details::SkipUserDefinedChars(s_iter, s.cend(), skip_chars);
    while (s_iter != s.cend())
    {
      double y = 0, x = 0;
      if (!details::GetFloat(s_iter, s.cend(), x)) break;
      details::SkipSpacesWithOptionalComma(s_iter, s.cend());
      if (!details::GetFloat(s_iter, s.cend(), y)) break;
      result.push_back(PointD(x, y));
      details::SkipUserDefinedChars(s_iter, s.cend(), skip_chars);
    }
    return result;
  }

  inline Path64 TrimCollinear(const Path64& p, bool is_open_path = false)
  {
    size_t len = p.size();
    if (len < 3)
    {
      if (!is_open_path || len < 2 || p[0] == p[1]) return Path64();
      else return p;
    }

    Path64 dst;
    dst.reserve(len);
    Path64::const_iterator srcIt = p.cbegin(), prevIt, stop = p.cend() - 1;

    if (!is_open_path)
    {
      // @UE BEGIN
      // explicit casts
      while (srcIt != stop && !static_cast<bool>(CrossProduct(*stop, *srcIt, *(srcIt + 1))))
        ++srcIt;
      while (srcIt != stop && !static_cast<bool>(CrossProduct(*(stop - 1), *stop, *srcIt)))
        --stop;
      // @UE END
      if (srcIt == stop) return Path64();
    }

    prevIt = srcIt++;
    dst.push_back(*prevIt);
    for (; srcIt != stop; ++srcIt)
    {
      // @UE BEGIN
      // explicit casts
      if (static_cast<bool>(CrossProduct(*prevIt, *srcIt, *(srcIt + 1))))
      {
      // @UE END
        prevIt = srcIt;
        dst.push_back(*prevIt);
      }
    }

    if (is_open_path)
      dst.push_back(*srcIt);
    // @UE BEGIN
    // explicit casts
    else if (static_cast<bool>(CrossProduct(*prevIt, *stop, dst[0])))
    // @UE END
      dst.push_back(*stop);
    else
    {
      while (dst.size() > 2 &&
        // @UE BEGIN
        // explicit casts
        !static_cast<bool>(CrossProduct(dst.end()[-1], dst.end()[-2], dst[0])))
        // @UE END
        dst.pop_back();
      if (dst.size() < 3) return Path64();
    }
    return dst;
  }

  // @UE BEGIN
  // Exceptions not supported, but unused function
  /*
  inline PathD TrimCollinear(const PathD& path, int precision, bool is_open_path = false)
  {
    if (precision > 8 || precision < -8) 
      throw new Clipper2Exception("Error: Precision exceeds the allowed range.");
    const double scale = std::pow(10, precision);
    Path64 p = ScalePath<int64, double>(path, scale);
    p = TrimCollinear(p, is_open_path);
    return ScalePath<double, int64>(p, 1/scale);
  }
  */
  // @UE END

  template <typename T>
  inline double Distance(const Point<T> pt1, const Point<T> pt2)
  {
    return std::sqrt(DistanceSqr(pt1, pt2));
  }

  template <typename T>
  inline double Length(const Path<T>& path, bool is_closed_path = false)
  {
    double result = 0.0;
    if (path.size() < 2) return result;
    auto it = path.cbegin(), stop = path.end() - 1;
    for (; it != stop; ++it)
      result += Distance(*it, *(it + 1));
    if (is_closed_path)
      result += Distance(*stop, *path.cbegin());
    return result;
  }


  template <typename T>
  inline bool NearCollinear(const Point<T>& pt1, const Point<T>& pt2, const Point<T>& pt3, double sin_sqrd_min_angle_rads)
  {
    double cp = std::abs(CrossProduct(pt1, pt2, pt3));
    return (cp * cp) / (DistanceSqr(pt1, pt2) * DistanceSqr(pt2, pt3)) < sin_sqrd_min_angle_rads;
  }
  
  template <typename T>
  inline Path<T> Ellipse(const Rect<T>& rect, int steps = 0)
  {
    return Ellipse(rect.MidPoint(), 
      static_cast<double>(rect.Width()) *0.5, 
      static_cast<double>(rect.Height()) * 0.5, steps);
  }

  template <typename T>
  inline Path<T> Ellipse(const Point<T>& center,
    double radiusX, double radiusY = 0, int steps = 0)
  {
    if (radiusX <= 0) return Path<T>();
    if (radiusY <= 0) radiusY = radiusX;
    if (steps <= 2)
      steps = static_cast<int>(PI * sqrt((radiusX + radiusY) / 2));

    // @UE BEGIN
    // explicit casts
    double si = std::sin(2 * PI / static_cast<double>(steps));
    double co = std::cos(2 * PI / static_cast<double>(steps));
    // @UE END
    double dx = co, dy = si;
    Path<T> result;
    result.reserve(steps);
    // @UE BEGIN
    // explicit casts
    result.push_back(Point<T>(static_cast<T>(static_cast<double>(center.x) + radiusX), static_cast<T>(center.y)));
    // @UE END
    for (int i = 1; i < steps; ++i)
    {
      // @UE BEGIN
      // explicit casts
      result.push_back(Point<T>(static_cast<T>(static_cast<double>(center.x) + radiusX * dx), static_cast<T>(static_cast<double>(center.y) + radiusY * dy)));
      // @UE END
      double x = dx * co - dy * si;
      dy = dy * co + dx * si;
      dx = x;
    }
    return result;
  }

  template <typename T>
  inline double PerpendicDistFromLineSqrd(const Point<T>& pt,
    const Point<T>& line1, const Point<T>& line2)
  {
    double a = static_cast<double>(pt.x - line1.x);
    double b = static_cast<double>(pt.y - line1.y);
    double c = static_cast<double>(line2.x - line1.x);
    double d = static_cast<double>(line2.y - line1.y);
    if (c == 0 && d == 0) return 0;
    return Sqr(a * d - c * b) / (c * c + d * d);
  }

  template <typename T>
  inline void RDP(const Path<T> path, std::size_t begin,
    std::size_t end, double epsSqrd, std::vector<bool>& flags)
  {
    typename Path<T>::size_type idx = 0;
    double max_d = 0;
    while (end > begin && path[begin] == path[end]) flags[end--] = false;
    for (typename Path<T>::size_type i = begin + 1; i < end; ++i)
    {
      // PerpendicDistFromLineSqrd - avoids expensive Sqrt()
      double d = PerpendicDistFromLineSqrd(path[i], path[begin], path[end]);
      if (d <= max_d) continue;
      max_d = d;
      idx = i;
    }
    if (max_d <= epsSqrd) return;
    flags[idx] = true;
    if (idx > begin + 1) RDP(path, begin, idx, epsSqrd, flags);
    if (idx < end - 1) RDP(path, idx, end, epsSqrd, flags);
  }

  template <typename T>
  inline Path<T> RamerDouglasPeucker(const Path<T>& path, double epsilon)
  {
    const typename Path<T>::size_type len = path.size();
    if (len < 5) return Path<T>(path);
    std::vector<bool> flags(len);
    flags[0] = true;
    flags[len - 1] = true;
    RDP(path, 0, len - 1, Sqr(epsilon), flags);
    Path<T> result;
    result.reserve(len);
    for (typename Path<T>::size_type i = 0; i < len; ++i)
      if (flags[i])
        result.push_back(path[i]);
    return result;
  }

  template <typename T>
  inline Paths<T> RamerDouglasPeucker(const Paths<T>& paths, double epsilon)
  {
    Paths<T> result;
    result.reserve(paths.size());
    for (const Path<T>& path : paths)
      result.push_back(RamerDouglasPeucker<T>(path, epsilon));
    return result;
  }

}  // end Clipper2Lib namespace
