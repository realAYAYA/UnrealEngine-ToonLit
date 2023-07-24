/*******************************************************************************
* Author    :  Angus Johnson                                                   *
* Version   :  Clipper2 - ver.1.0.4                                            *
* Date      :  14 August 2022                                                  *
* Website   :  http://www.angusj.com                                           *
* Copyright :  Angus Johnson 2010-2022                                         *
* Purpose   :  Path Offset (Inflate/Shrink)                                    *
* License   :  http://www.boost.org/LICENSE_1_0.txt                            *
*******************************************************************************/

// @UE BEGIN
// self as first header
#include "ThirdParty/clipper/clipper.offset.h"
// @UE END
#include <cmath>
// @UE BEGIN
// header path
#include "ThirdParty/clipper/clipper.h"
// @UE END

#ifndef PI
  #define PI 3.141592653589793238
#endif

namespace Clipper2Lib {

const double default_arc_tolerance = 0.25;
const double floating_point_tolerance = 1e-12;

//------------------------------------------------------------------------------
// Miscellaneous methods
//------------------------------------------------------------------------------

Paths64::size_type GetLowestPolygonIdx(const Paths64& paths)
{
	Paths64::size_type result = 0;
	Point64 lp = Point64(static_cast<int64>(0), 
		std::numeric_limits<int64>::min());

	for (Paths64::size_type i = 0 ; i < paths.size(); ++i)
		for (const Point64& p : paths[i])
			if (p.y > lp.y || (p.y == lp.y && p.x < lp.x))
			{
				result = i;
				lp = p;
			}	
	return result;
}

PointD GetUnitNormal(const Point64& pt1, const Point64& pt2)
{
	double dx, dy, inverse_hypot;
	if (pt1 == pt2) return PointD(0.0, 0.0);
	dx = static_cast<double>(pt2.x - pt1.x);
	dy = static_cast<double>(pt2.y - pt1.y);
	inverse_hypot = 1.0 / hypot(dx, dy);
	dx *= inverse_hypot;
	dy *= inverse_hypot;
	return PointD(dy, -dx);
}

inline bool AlmostZero(double value, double epsilon = 0.001)
{
	return std::fabs(value) < epsilon;
}

inline double Hypot(double x, double y) 
{
	//see https://stackoverflow.com/a/32436148/359538
	return std::sqrt(x * x + y * y);
}

inline PointD NormalizeVector(const PointD& vec)
{
	
	double h = Hypot(vec.x, vec.y);
	if (AlmostZero(h)) return PointD(0,0);
	double inverseHypot = 1 / h;
	return PointD(vec.x * inverseHypot, vec.y * inverseHypot);
}

inline PointD GetAvgUnitVector(const PointD& vec1, const PointD& vec2)
{
	return NormalizeVector(PointD(vec1.x + vec2.x, vec1.y + vec2.y));
}

inline bool IsClosedPath(EndType et)
{
	return et == EndType::Polygon || et == EndType::Joined;
}

//------------------------------------------------------------------------------
// ClipperOffset methods
//------------------------------------------------------------------------------

void ClipperOffset::AddPath(const Path64& path, JoinType jt_, EndType et_)
{
	Paths64 paths;
	paths.push_back(path);
	AddPaths(paths, jt_, et_);
}

void ClipperOffset::AddPaths(const Paths64 &paths, JoinType jt_, EndType et_)
{
	if (paths.size() == 0) return;
	groups_.push_back(Group(paths, jt_, et_));
}

void ClipperOffset::AddPath(const Clipper2Lib::PathD& path, JoinType jt_, EndType et_)
{
	PathsD paths;
	paths.push_back(path);
	AddPaths(paths, jt_, et_);
}

void ClipperOffset::AddPaths(const PathsD& paths, JoinType jt_, EndType et_)
{
	if (paths.size() == 0) return;
	groups_.push_back(Group(PathsDToPaths64(paths), jt_, et_));
}

void ClipperOffset::BuildNormals(const Path64& path)
{
	norms.clear();
	norms.reserve(path.size());
	if (path.size() == 0) return;
	Path64::const_iterator path_iter, path_last_iter = --path.cend();
	for (path_iter = path.cbegin(); path_iter != path_last_iter; ++path_iter)
		norms.push_back(GetUnitNormal(*path_iter,*(path_iter +1)));
	norms.push_back(GetUnitNormal(*path_last_iter, *(path.cbegin())));
}

inline PointD TranslatePoint(const PointD& pt, double dx, double dy)
{
	return PointD(pt.x + dx, pt.y + dy);
}

inline PointD ReflectPoint(const PointD& pt, const PointD& pivot)
{
	return PointD(pivot.x + (pivot.x - pt.x), pivot.y + (pivot.y - pt.y));
}

PointD IntersectPoint(const PointD& pt1a, const PointD& pt1b,
	const PointD& pt2a, const PointD& pt2b)
{
	if (pt1a.x == pt1b.x) //vertical
	{
		if (pt2a.x == pt2b.x) return PointD(0, 0);

		double m2 = (pt2b.y - pt2a.y) / (pt2b.x - pt2a.x);
		double b2 = pt2a.y - m2 * pt2a.x;
		return PointD(pt1a.x, m2 * pt1a.x + b2);
	}
	else if (pt2a.x == pt2b.x) //vertical
	{
		double m1 = (pt1b.y - pt1a.y) / (pt1b.x - pt1a.x);
		double b1 = pt1a.y - m1 * pt1a.x;
		return PointD(pt2a.x, m1 * pt2a.x + b1);
	}
	else
	{
		double m1 = (pt1b.y - pt1a.y) / (pt1b.x - pt1a.x);
		double b1 = pt1a.y - m1 * pt1a.x;
		double m2 = (pt2b.y - pt2a.y) / (pt2b.x - pt2a.x);
		double b2 = pt2a.y - m2 * pt2a.x;
		if (m1 == m2) return PointD(0, 0);
		double x = (b2 - b1) / (m1 - m2);
		return PointD(x, m1 * x + b1);
	}
}

inline Point64 GetPerpendic(const Point64& pt, const PointD norm, double delta)
{
	// @UE BEGIN
	// explicit casts
	return Point64(pt.x + static_cast<int64>(norm.x * delta), pt.y + static_cast<int64>(norm.y * delta));
	// @UE END
}

inline PointD GetPerpendicD(const Point64& pt, const PointD norm, double  delta)
{
	// @UE BEGIN
	// explicit casts
	return PointD(static_cast<double>(pt.x) + norm.x * delta, static_cast<double>(pt.y) + norm.y * delta);
	// @UE END
}

void ClipperOffset::DoSquare(Group& group, const Path64& path, size_t j, size_t k)
{
	PointD vec;
	if (j == k) 
		vec = PointD(norms[0].y, -norms[0].x);
	else
		vec = GetAvgUnitVector(
			PointD(-norms[k].y, norms[k].x),
			PointD(norms[j].y, -norms[j].x));

	// now offset the original vertex delta units along unit vector
	// @UE BEGIN
	// explicit casts
	PointD ptQ = PointD(static_cast<double>(path[j].x), static_cast<double>(path[j].y));
	// @UE END
	ptQ = TranslatePoint(ptQ, abs_group_delta_ * vec.x, abs_group_delta_ * vec.y);
	// get perpendicular vertices
	PointD pt1 = TranslatePoint(ptQ, group_delta_ * vec.y, group_delta_ * -vec.x);
	PointD pt2 = TranslatePoint(ptQ, group_delta_ * -vec.y, group_delta_ * vec.x);
	// get 2 vertices along one edge offset
	PointD pt3 = GetPerpendicD(path[k], norms[k], group_delta_);
	if (j == k)
	{
		PointD pt4 = PointD(pt3.x + vec.x * group_delta_, pt3.y + vec.y * group_delta_);
		PointD pt = IntersectPoint(pt1, pt2, pt3, pt4);
		//get the second intersect point through reflecion
		// @UE BEGIN
		// explicit casts
		const PointD ReflectedPoint = ReflectPoint(pt, ptQ);
		group.path_.push_back(Point64(static_cast<int64>(ReflectedPoint.x), static_cast<int64>(ReflectedPoint.y)));
		group.path_.push_back(Point64(static_cast<int64>(pt.x), static_cast<int64>(pt.y)));
		// @UE END
	}
	else
	{
		PointD pt4 = GetPerpendicD(path[j], norms[k], group_delta_);
		PointD pt = IntersectPoint(pt1, pt2, pt3, pt4);
		// @UE BEGIN
		// explicit casts
		group.path_.push_back(Point64(static_cast<int64>(pt.x), static_cast<int64>(pt.y)));
		//get the second intersect point through reflecion
		const PointD ReflectedPoint = ReflectPoint(pt, ptQ);
		group.path_.push_back(Point64(static_cast<int64>(ReflectedPoint.x), static_cast<int64>(ReflectedPoint.y)));
		// @UE END
	}
}

void ClipperOffset::DoMiter(Group& group, const Path64& path, size_t j, size_t k, double cos_a)
{
	double q = group_delta_ / (cos_a + 1);
	// @UE BEGIN
	// explicit casts
	group.path_.push_back(Point64(
		path[j].x + static_cast<int64>((norms[k].x + norms[j].x) * q),
		path[j].y + static_cast<int64>((norms[k].y + norms[j].y) * q)));
	// @UE END
}

void ClipperOffset::DoRound(Group& group, const Path64& path, size_t j, size_t k, double angle)
{
	//even though angle may be negative this is a convex join
	Point64 pt = path[j];
	int steps = static_cast<int>(std::ceil(steps_per_rad_ * std::abs(angle)));
	double step_sin = std::sin(angle / steps);
	double step_cos = std::cos(angle / steps);
	
	PointD pt2 = PointD(norms[k].x * group_delta_, norms[k].y * group_delta_);
	if (j == k) pt2.Negate();
	
	// @UE BEGIN
	// explicit casts
	group.path_.push_back(Point64(pt.x + static_cast<int64>(pt2.x), pt.y + static_cast<int64>(pt2.y)));
	// @UE END
	for (int i = 0; i < steps; i++)
	{
		pt2 = PointD(pt2.x * step_cos - step_sin * pt2.y,
			pt2.x * step_sin + pt2.y * step_cos);
	// @UE BEGIN
	// explicit casts
		group.path_.push_back(Point64(pt.x + static_cast<int64>(pt2.x), pt.y + static_cast<int64>(pt2.y)));
	}
	group.path_.push_back(GetPerpendic(path[j], norms[j], group_delta_));
	// @UE END
}

void ClipperOffset::OffsetPoint(Group& group, Path64& path, size_t j, size_t& k)
{
	// Let A = change in angle where edges join
	// A == 0: ie no change in angle (flat join)
	// A == PI: edges 'spike'
	// sin(A) < 0: right turning
	// cos(A) < 0: change in angle is more than 90 degree
	double sin_a = CrossProduct(norms[j], norms[k]);
	double cos_a = DotProduct(norms[j], norms[k]);
	if (sin_a > 1.0) sin_a = 1.0;
	else if (sin_a < -1.0) sin_a = -1.0;

	bool almostNoAngle = AlmostZero(sin_a) && cos_a > 0;
	// when there's almost no angle of deviation or it's concave
	if (almostNoAngle || (sin_a * group_delta_ < 0))
	{
		// @UE BEGIN
		// explicit casts
		Point64 p1 = Point64(
			static_cast<int64>(static_cast<double>(path[j].x) + norms[k].x * group_delta_),
			static_cast<int64>(static_cast<double>(path[j].y) + norms[k].y * group_delta_));
		Point64 p2 = Point64(
			static_cast<int64>(static_cast<double>(path[j].x) + norms[j].x * group_delta_),
			static_cast<int64>(static_cast<double>(path[j].y) + norms[j].y * group_delta_));
		// @UE END
		group.path_.push_back(p1);
		if (p1 != p2)
		{
			// when concave add an extra vertex to ensure neat clipping
			if (!almostNoAngle) group.path_.push_back(path[j]);
			group.path_.push_back(p2);
		}
	}
	else // it's convex 
	{
		if (join_type_ == JoinType::Round)
			DoRound(group, path, j, k, std::atan2(sin_a, cos_a));
		// else miter when the angle isn't too acute (and hence exceed ML)
		else if (join_type_ == JoinType::Miter && cos_a > temp_lim_ - 1)
			DoMiter(group, path, j, k, cos_a);
		// else only square angles that deviate > 90 degrees
		else if (cos_a < -0.001)
			DoSquare(group, path, j, k);
		else
			// don't square shallow angles that are safe to miter
			DoMiter(group, path, j, k, cos_a);
	}
	k = j;
}

void ClipperOffset::OffsetPolygon(Group& group, Path64& path)
{
	group.path_.clear();
	for (Path64::size_type i = 0, j = path.size() -1; i < path.size(); j = i, ++i)
		OffsetPoint(group, path, i, j);
	group.paths_out_.push_back(group.path_);
}

void ClipperOffset::OffsetOpenJoined(Group& group, Path64& path)
{
	OffsetPolygon(group, path);
	std::reverse(path.begin(), path.end());
	BuildNormals(path);
	OffsetPolygon(group, path);
}

void ClipperOffset::OffsetOpenPath(Group& group, Path64& path, EndType end_type)
{
	group.path_.clear();

	// do the line start cap
	switch (end_type)
	{
	case EndType::Butt:
		// @UE BEGIN
		// explicit casts
		group.path_.push_back(Point64(
			static_cast<int64>(static_cast<double>(path[0].x) + norms[0].x * group_delta_),
			static_cast<int64>(static_cast<double>(path[0].y) + norms[0].y * group_delta_)));
		group.path_.push_back(Point64(
			static_cast<int64>(static_cast<double>(path[0].x) - norms[0].x * group_delta_),
			static_cast<int64>(static_cast<double>(path[0].y) - norms[0].y * group_delta_)));
		// @UE END
		break;
	case EndType::Round:
		DoRound(group, path, 0, 0, PI);
		break;
	default:
		DoSquare(group, path, 0, 0);
		break;
	}
	
	size_t highI = path.size() - 1;
	
	// offset the left side going forward
	for (Path64::size_type i = 1, k = 0; i < highI; ++i)
		OffsetPoint(group, path, i, k);

	// reverse normals 
	for (size_t i = highI; i > 0; --i)
		norms[i] = PointD(-norms[i - 1].x, -norms[i - 1].y);
	norms[0] = norms[highI];

	// do the line end cap
	switch (end_type)
	{
	case EndType::Butt:
		// @UE BEGIN
		// explicit casts
		group.path_.push_back(Point64(
			static_cast<int64>(static_cast<double>(path[highI].x) + norms[1].x * group_delta_),
			static_cast<int64>(static_cast<double>(path[highI].y) + norms[1].y * group_delta_)));
		group.path_.push_back(Point64(
			static_cast<int64>(static_cast<double>(path[highI].x) - norms[1].x * group_delta_),
			static_cast<int64>(static_cast<double>(path[highI].y) - norms[1].y * group_delta_)));
		// @UE END
		break;
	case EndType::Round:
		DoRound(group, path, highI, highI, PI);
		break;
	default:
		DoSquare(group, path, highI, highI);
		break;
	}

	for (size_t i = highI, k = 0; i > 0; --i)
		OffsetPoint(group, path, i, k);

	group.paths_out_.push_back(group.path_);
}

void ClipperOffset::DoGroupOffset(Group& group, double delta)
{
	if (group.end_type_ != EndType::Polygon) delta = std::abs(delta) / 2;
	bool isClosedPaths = IsClosedPath(group.end_type_);

	if (isClosedPaths)
	{
		//the lowermost polygon must be an outer polygon. So we can use that as the
		//designated orientation for outer polygons (needed for tidy-up clipping)
		Paths64::size_type lowestIdx = GetLowestPolygonIdx(group.paths_in_);
		// nb: don't use the default orientation here ...
		// @UE BEGIN
		// explicit casts
		double area = static_cast<double>(Area(group.paths_in_[lowestIdx]));
		// @UE END
		if (area == 0) return;	
		group.is_reversed_ = (area < 0);
		if (group.is_reversed_) delta = -delta;
	}
	else
		group.is_reversed_ = false;

	group_delta_ = delta;
	abs_group_delta_ = std::abs(group_delta_);
	join_type_ = group.join_type_;

	double arcTol = (arc_tolerance_ > floating_point_tolerance ? arc_tolerance_
		: std::log10(2 + abs_group_delta_) * default_arc_tolerance); // empirically derived

	//calculate a sensible number of steps (for 360 deg for the given offset
	if (group.join_type_ == JoinType::Round || group.end_type_ == EndType::Round)
	{
		steps_per_rad_ = PI / std::acos(1 - arcTol / abs_group_delta_) / (PI *2);
		// @UE BEGIN
		// default calculation above results in far too many vertices
		steps_per_rad_ /= StepsPerRadianDivisor;
		// @UE END
	}

	bool is_closed_path = IsClosedPath(group.end_type_);
	Paths64::const_iterator path_iter;
	for(path_iter = group.paths_in_.cbegin(); path_iter != group.paths_in_.cend(); ++path_iter)
	{
		Path64 path = StripDuplicates(*path_iter, is_closed_path);
		Path64::size_type cnt = path.size();
		if (cnt == 0) continue;

		if (cnt == 1) // single point - only valid with open paths
		{
			group.path_ = Path64();
			//single vertex so build a circle or square ...
			if (group.join_type_ == JoinType::Round)
			{
				double radius = abs_group_delta_;
				group.path_ = Ellipse(path[0], radius, radius);
			}
			else
			{
				int d = (int)std::ceil(abs_group_delta_);
				Rect64 r = Rect64(path[0].x - d, path[0].y - d, path[0].x + d, path[0].y + d);
				group.path_ = r.AsPath();
			}
			group.paths_out_.push_back(group.path_);
		}
		else
		{
			BuildNormals(path);
			if (group.end_type_ == EndType::Polygon) OffsetPolygon(group, path);
			else if (group.end_type_ == EndType::Joined) OffsetOpenJoined(group, path);
			else OffsetOpenPath(group, path, group.end_type_);
		}
	}

	if (!merge_groups_)
	{
		//clean up self-intersections ...
		Clipper64 c;
		c.PreserveCollinear = false;
		//the solution should retain the orientation of the input
		c.ReverseSolution = reverse_solution_ != group.is_reversed_;
		c.AddSubject(group.paths_out_);
		if (group.is_reversed_)
			c.Execute(ClipType::Union, FillRule::Negative, group.paths_out_);
		else
			c.Execute(ClipType::Union, FillRule::Positive, group.paths_out_);
	}

	solution.reserve(solution.size() + group.paths_out_.size());
	copy(group.paths_out_.begin(), group.paths_out_.end(), back_inserter(solution));
	group.paths_out_.clear();
}

Paths64 ClipperOffset::Execute(double delta)
{
	solution.clear();
	if (std::abs(delta) < default_arc_tolerance)
	{
		for (const Group& group : groups_)
		{
			solution.reserve(solution.size() + group.paths_in_.size());
			copy(group.paths_in_.begin(), group.paths_in_.end(), back_inserter(solution));
		}
		return solution;
	}

	temp_lim_ = (miter_limit_ <= 1) ? 
		2.0 : 
		2.0 / (miter_limit_ * miter_limit_);

	std::vector<Group>::iterator groups_iter;
	for (groups_iter = groups_.begin(); 
		groups_iter != groups_.end(); ++groups_iter)
	{
		DoGroupOffset(*groups_iter, delta);
	}

	if (merge_groups_ && groups_.size() > 0)
	{
		//clean up self-intersections ...
		Clipper64 c;
		c.PreserveCollinear = false;
		//the solution should retain the orientation of the input
		c.ReverseSolution = reverse_solution_ != groups_[0].is_reversed_;

		c.AddSubject(solution);
		if (groups_[0].is_reversed_)
			c.Execute(ClipType::Union, FillRule::Negative, solution);
		else
			c.Execute(ClipType::Union, FillRule::Positive, solution);
	}
	return solution;
}

} // namespace
