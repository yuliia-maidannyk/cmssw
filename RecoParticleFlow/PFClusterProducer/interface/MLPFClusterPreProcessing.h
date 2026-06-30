#pragma once

#include <vector>
#include <Eigen/Dense>

namespace Config
{
    constexpr int    ROWS          = 361;
    constexpr int    COLS          = 171;
    constexpr int    CROP_SIZE     = 7;
    constexpr double THRESHOLD     = 0.66;
    constexpr int    OVERLAP_LIMIT = 7;
    constexpr int    MAX_CLUSTERS  = 20;
}

struct ProcessingResult {
    std::vector<std::vector<Eigen::MatrixXf>> X;
    std::vector<std::vector<Eigen::MatrixXf>> indices;
    std::vector<std::vector<int>> cluster_ids;
};

ProcessingResult get_model_samples(
    const std::vector<float>& map,
    int rows = Config::ROWS, int cols = Config::COLS,
    double threshold   = Config::THRESHOLD,
    int crop_size      = Config::CROP_SIZE,
    int overlap_limit  = Config::OVERLAP_LIMIT,
    int max_cluster    = Config::MAX_CLUSTERS);