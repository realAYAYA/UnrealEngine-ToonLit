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
#pragma once
// @UE END

// @UE BEGIN
// for int64
#include "HAL/Platform.h"
// @UE END

#include "ThirdParty/clipper/clipper.core.h"

namespace Clipper2Lib {

// @UE BEGIN
// global standard / default constant controlling the number of vertices used for round joins and ends
constexpr static double StandardStepsPerRadianMultiplier = 3.0;
// as a safety, define a hard limit on max steps per radian
constexpr static double StepsPerRadianHardLimit = 2000.0;
// @UE END

enum class JoinType { Square, Round, Miter };

enum class EndType {Polygon, Joined, Butt, Square, Round};
//Butt   : offsets both sides of a path, with square blunt ends
//Square : offsets both sides of a path, with square extended ends
//Round  : offsets both sides of a path, with round extended ends
//Joined : offsets both sides of a path, with joined ends
//Polygon: offsets only one side of a closed path

class ClipperOffset {
private:
	class Group {
	public:
		Paths64 paths_in_;
		Paths64 paths_out_;
		Path64 path_;
		bool is_reversed_ = false;
		JoinType join_type_;
		EndType end_type_;
		Group(const Paths64& paths, JoinType join_type, EndType end_type) :
			paths_in_(paths), join_type_(join_type), end_type_(end_type) {}
	};

	double group_delta_ = 0.0;
	double abs_group_delta_ = 0.0;
	double temp_lim_ = 0.0;
	double steps_per_rad_ = 0.0;
	PathD norms;
	Paths64 solution;
	std::vector<Group> groups_;
	JoinType join_type_ = JoinType::Square;
	
	double miter_limit_ = 0.0;
	double arc_tolerance_ = 0.0;
	bool merge_groups_ = true;
	bool preserve_collinear_ = false;
	bool reverse_solution_ = false;

	// @UE BEGIN
	// additional controls over the number of steps for Round Joins
	// We scale every bounding box to the same range for computation, but need to know the actual scale when deciding how much resolution to give to round joins/ends
	double BoundsScaleFactor = 1.0;
	// Allow custom scaling of the default number of vertices per radian
	double CustomStepsPerRadScaleFactor = 1.0;
	// Set a hard max on the number of steps, as a safety feature
	double MaxStepsPerRadian = StepsPerRadianHardLimit;
	// @UE END

	void DoSquare(Group& group, const Path64& path, size_t j, size_t k);
	void DoMiter(Group& group, const Path64& path, size_t j, size_t k, double cos_a);
	void DoRound(Group& group, const Path64& path, size_t j, size_t k, double angle);
	void BuildNormals(const Path64& path);
	void OffsetPolygon(Group& group, Path64& path);
	void OffsetOpenJoined(Group& group, Path64& path);
	void OffsetOpenPath(Group& group, Path64& path, EndType endType);
	void OffsetPoint(Group& group, Path64& path, size_t j, size_t& k);
	void DoGroupOffset(Group &group, double delta);
public:
	ClipperOffset(double miter_limit = 2.0,
		double arc_tolerance = 0.0,
		bool preserve_collinear = false, 
		bool reverse_solution = false) :
		miter_limit_(miter_limit), arc_tolerance_(arc_tolerance),
		preserve_collinear_(preserve_collinear),
		reverse_solution_(reverse_solution) { };

	~ClipperOffset() { Clear(); };

	// @UE BEGIN
	// additional controls over the number of steps for Round Joins
	void SetRoundScaleFactors(double InputRangeIntRangeRatio, double InStepsPerRadScaleFactor, double InMaxStepsPerRadian = -1);
	// @UE END

	void AddPath(const Path64& path, JoinType jt_, EndType et_);
	void AddPaths(const Paths64& paths, JoinType jt_, EndType et_);
	void AddPath(const PathD &p, JoinType jt_, EndType et_);
	void AddPaths(const PathsD &p, JoinType jt_, EndType et_);
	void Clear() { groups_.clear(); norms.clear(); };
	
	Paths64 Execute(double delta);

	double MiterLimit() const { return miter_limit_; }
	void MiterLimit(double miter_limit) { miter_limit_ = miter_limit; }

	//ArcTolerance: needed for rounded offsets (See offset_triginometry2.svg)
	double ArcTolerance() const { return arc_tolerance_; }
	void ArcTolerance(double arc_tolerance) { arc_tolerance_ = arc_tolerance; }

	//MergeGroups: A path group is one or more paths added via the AddPath or
	//AddPaths methods. By default these path groups will be offset
	//independently of other groups and this may cause overlaps (intersections).
	//However, when MergeGroups is enabled, any overlapping offsets will be
	//merged (via a clipping union operation) to remove overlaps.
	bool MergeGroups() const { return merge_groups_; }
	void MergeGroups(bool merge_groups) { merge_groups_ = merge_groups; }

	bool PreserveCollinear() const { return preserve_collinear_; }
	void PreserveCollinear(bool preserve_collinear){preserve_collinear_ = preserve_collinear;}
	
	bool ReverseSolution() const { return reverse_solution_; }
	void ReverseSolution(bool reverse_solution) {reverse_solution_ = reverse_solution;}
};

}
