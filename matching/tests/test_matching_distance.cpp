#include "catch/catch.hpp"

#include <sstream>
#include <iostream>
#include <string>

#define MD_TEST_CODE

#include "common_util.h"
#include "simplex.h"
#include "matching_distance.h"

using Real = double;
using Point = md::Point<Real>;
using Bifiltration = md::Bifiltration<Real>;
using BifiltrationProxy = md::BifiltrationProxy<Real>;
using CalculationParams = md::CalculationParams<Real>;
using CellWithValue = md::CellWithValue<Real>;
using DualPoint = md::DualPoint<Real>;
using DualBox = md::DualBox<Real>;
using Simplex = md::Simplex<Real>;
using AbstractSimplex = md::AbstractSimplex;
using BoundStrategy = md::BoundStrategy;
using TraverseStrategy = md::TraverseStrategy;
using AxisType = md::AxisType;
using AngleType = md::AngleType;
using ValuePoint = md::ValuePoint;
using Column = md::Column;

using md::k_corner_vps;

TEST_CASE("Different bounds", "[bounds]")
{
    std::vector<Simplex> simplices;
    std::vector<Point> points;

    Real max_x = 10;
    Real max_y = 20;

    int simplex_id = 0;
    for(int i = 0; i <= max_x; ++i) {
        for(int j = 0; j <= max_y; ++j) {
            Point p(i, j);
            simplices.emplace_back(simplex_id++, p, 0, Column());
            points.push_back(p);
        }
    }

    Bifiltration bif_a(simplices.begin(), simplices.end());
    Bifiltration bif_b(simplices.begin(), simplices.end());

    CalculationParams params;
    params.initialization_depth = 2;

    BifiltrationProxy bifp_a(bif_a, params.dim);
    BifiltrationProxy bifp_b(bif_b, params.dim);

    md::DistanceCalculator<Real, BifiltrationProxy> calc(bifp_a, bifp_b, params);

//    REQUIRE(calc.max_x_ == Approx(max_x));
//    REQUIRE(calc.max_y_ == Approx(max_y));

    std::vector<DualBox> boxes;

    for(CellWithValue c : calc.get_refined_grid(5, false, false)) {
        boxes.push_back(c.dual_box());
    }

    // fill in boxes and points

    for(DualBox db : boxes) {
        Real local_bound = calc.get_local_dual_bound(db);
        Real local_bound_refined = calc.get_local_refined_bound(db);
        REQUIRE(local_bound >= local_bound_refined);
        for(Point p : points) {
            for(ValuePoint vp_a : k_corner_vps) {
                CellWithValue dual_cell(db, 1);
                DualPoint corner_a = dual_cell.value_point(vp_a);
                Real wp_a = corner_a.weighted_push(p);
                dual_cell.set_value_at(vp_a, wp_a);
                Real point_bound = calc.get_max_displacement_single_point(dual_cell, vp_a, p);
                for(ValuePoint vp_b : k_corner_vps) {
                    if (vp_b <= vp_a)
                        continue;
                    DualPoint corner_b = dual_cell.value_point(vp_b);
                    Real wp_b = corner_b.weighted_push(p);
                    Real diff = fabs(wp_a - wp_b);
                    if (not(point_bound <= Approx(local_bound_refined))) {
                        std::cerr << "ERROR point: " << p << ", box = " << db << ", point bound = " << point_bound
                                  << ", refined local = " << local_bound_refined << std::endl;
                        calc.get_max_displacement_single_point(dual_cell, vp_a, p);
                        calc.get_local_refined_bound(db);
                    }

                    REQUIRE(point_bound <= Approx(local_bound_refined));
                    REQUIRE(diff <= Approx(point_bound));
                    REQUIRE(diff <= Approx(local_bound_refined));
                }

                for(DualPoint l_random : db.random_points(100)) {
                    Real wp_random = l_random.weighted_push(p);
                    Real diff = fabs(wp_a - wp_random);
                    if (not(diff <= Approx(point_bound))) {
                        if (db.critical_points(p).size() > 4) {
                            std::cerr << "ERROR interesting case" << std::endl;
                        } else {
                            std::cerr << "ERROR boring case" << std::endl;
                        }
                        l_random.weighted_push(p);
                        std::cerr << "ERROR point: " << p << ", box = " << db << ", point bound = " << point_bound
                                  << ", refined local = " << local_bound_refined;
                        std::cerr << ", random_dual_point = " << l_random << ", wp_random = " << wp_random
                                  << ", diff = " << diff << std::endl;
                    }
                    REQUIRE(diff <= Approx(point_bound));
                }
            }
        }
    }
}

TEST_CASE("Bifiltrations from file", "[matching_distance][small_example]")
{
    std::string fname_a, fname_b;

    fname_a = "../tests/prism_1.bif";
    fname_b = "../tests/prism_2.bif";

    Bifiltration bif_a(fname_a);
    Bifiltration bif_b(fname_b);

    CalculationParams params;

    std::vector<BoundStrategy> bound_strategies {BoundStrategy::local_combined,
                                                 BoundStrategy::local_dual_bound_refined};

    std::vector<TraverseStrategy> traverse_strategies {TraverseStrategy::breadth_first, TraverseStrategy::depth_first};

    std::vector<double> scaling_factors {10, 1.0};

    for(auto bs : bound_strategies) {
        for(auto ts : traverse_strategies) {
            for(double lambda : scaling_factors) {
                Bifiltration bif_a_copy(bif_a);
                Bifiltration bif_b_copy(bif_b);
                bif_a_copy.scale(lambda);
                bif_b_copy.scale(lambda);
                params.bound_strategy = bs;
                params.traverse_strategy = ts;
                params.max_depth = 7;
                params.delta = 0.1;
                params.dim = 1;
                Real answer = matching_distance(bif_a_copy, bif_b_copy, params);
                Real correct_answer = lambda * 1.0;
                REQUIRE(fabs(answer - correct_answer) / correct_answer < params.delta);
            }
        }
    }
}
