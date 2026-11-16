#ifndef RecoParticleFlow_PFClusterProducer_MLPFClusterPreProcessing_h
#define RecoParticleFlow_PFClusterProducer_MLPFClusterPreProcessing_h

#include <Eigen/Dense>
#include <vector>
#include <algorithm>

namespace Config
{
    constexpr int    ROWS          = 361;
    constexpr int    COLS          = 171;
    constexpr int    CROP_SIZE     = 7;
    constexpr double THRESHOLD     = 0.66;
    constexpr int    OVERLAP_LIMIT = 7;
    constexpr int    MAX_CLUSTERS  = 20;
}

// -----------------------------------------------------------------------------
// Data structures
// -----------------------------------------------------------------------------

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

struct Window
{
    Eigen::MatrixXf X;
    Eigen::Vector2i indices;
    double          deposited_energy;
};

struct EventWindows { std::vector<Window> windows; };

struct BatchedWindows { std::vector<std::vector<Eigen::MatrixXf>> X, indices; };

// -----------------------------------------------------------------------------
// Methods
// -----------------------------------------------------------------------------

inline std::vector<Eigen::Vector2i> find_seed_candidates(const float* X_ptr, int rows, int cols, double threshold)
{
    std::vector<Eigen::Vector2i> out;
    out.reserve(400);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (X_ptr[r * cols + c] > threshold)
            {
                out.emplace_back(r, c);
            }
    return out;
}

inline EventWindows extract_windows_for_event(const float* X_ptr, int rows, int cols, double threshold, int crop_size)
{
    EventWindows result;

    auto seed_candidates = find_seed_candidates(X_ptr, rows, cols, threshold);

    const int half_size = crop_size / 2;
    result.windows.reserve(seed_candidates.size());

    for (const auto& center : seed_candidates)
    {
        int row = center[0];
        int col = center[1];

        Window window;
        window.indices = center;
        window.deposited_energy = X_ptr[row * cols + col];
        window.X.resize(crop_size, crop_size);
        for (int r = 0; r < crop_size; ++r)
        {
            int src_row = (row - half_size + r - 1 + rows - 1) % (rows - 1) + 1; // wrap around in phi and make sure [1,360]
            for (int c = 0; c < crop_size; ++c)
            {
                int src_col = col - half_size + c;
                if (src_col < 0 || src_col >= cols) // out of bounds in eta, fill with -1
                {
                    window.X(r, c) = -1.0;
                }
                else
                {
                    window.X(r, c) = X_ptr[src_row * cols + src_col];
                }
            }
        }
        result.windows.push_back(std::move(window));
    }
    return result;
}

inline void sort_windows_by_energy(EventWindows& event)
// Descending energy order of seed windows
{
    std::sort(event.windows.begin(), event.windows.end(),
              [](const Window& a, const Window& b){
                  return a.deposited_energy > b.deposited_energy;
              });
}

inline std::vector<std::vector<int>> compute_and_cluster_windows(const std::vector<Window>& windows, int overlap_size)
// Aggregate windows in the same overlap window into a graph (cluster)
{
    int n = windows.size();
    if (n == 0) return {};

    std::vector<bool> processed(n, false);
    std::vector<std::vector<int>> clusters;
    int half_overlap = overlap_size / 2;

    for (int seed_id = 0; seed_id < n; ++seed_id) {
        if (processed[seed_id]) continue;
        std::vector<int> cluster = {seed_id};
        processed[seed_id] = true;

        for (int j = seed_id + 1; j < n; ++j) {
            if (processed[j]) continue;
            int dx = windows[seed_id].indices[0] - windows[j].indices[0];
            dx = ((dx + 180) % 360 + 360) % 360 - 180;  // wrap in phi
            dx = std::abs(dx);
            int dy = std::abs(windows[seed_id].indices[1] - windows[j].indices[1]);
            if (dx <= half_overlap && dy <= half_overlap) {
                cluster.push_back(j);
                processed[j] = true;
            }
        }

        clusters.push_back(std::move(cluster));
    }
    return clusters;
}

inline BatchedWindows batch_clusters(
    const EventWindows& event,
    const std::vector<std::vector<int>>& clusters,
    int max_per_cluster, int crop_size)
    // Create fixed-size batches for each event, padding with -1 where necessary
    // var[0] = X, var[1] = indices
{
    BatchedWindows batches;

    for (const auto& cluster : clusters)
    {
        size_t actual = std::min(cluster.size(), static_cast<size_t>(max_per_cluster));

        std::vector<Eigen::MatrixXf> X_b, ind_b;

        for (size_t i = 0; i < actual; ++i) {
            const Window& w = event.windows[cluster[i]];
            X_b.push_back(w.X);
            Eigen::MatrixXf idx(2,1); 
            idx << w.indices[0], w.indices[1];
            ind_b.push_back(idx);
        }

        for (size_t i = actual; i < static_cast<size_t>(max_per_cluster); ++i) {
            X_b.push_back(Eigen::MatrixXf::Constant(crop_size, crop_size, -1.0));
            Eigen::MatrixXf p2(2,1); 
            p2 << -1.0, -1.0;
            ind_b.push_back(p2);
        }

        batches.X.push_back(std::move(X_b));
        batches.indices.push_back(std::move(ind_b));
    }
    return batches;
}

inline ProcessingResult get_model_samples(
    const std::vector<float>& map, int rows, int cols, 
    double threshold, int crop_size, int overlap_limit, int max_cluster)
{
    const float* X_ptr = map.data();

    EventWindows windows = extract_windows_for_event(X_ptr, rows, cols, threshold, crop_size);

    sort_windows_by_energy(windows);

    auto clusters = compute_and_cluster_windows(windows.windows, overlap_limit);

    // Truncate clusters to max_cluster windows each
    std::vector<std::vector<int>> truncated;
    truncated.reserve(clusters.size());
    for (const auto& cluster : clusters) {
        size_t limit = std::min(cluster.size(), static_cast<size_t>(max_cluster));
        truncated.emplace_back(cluster.begin(), cluster.begin() + limit);
    }

    BatchedWindows batches = batch_clusters(windows, truncated, max_cluster, crop_size);

    std::vector<std::vector<Eigen::MatrixXf>> all_X, all_indices;

    for (size_t i = 0; i < batches.X.size(); ++i) {
        all_X.push_back(std::move(batches.X[i]));
        all_indices.push_back(std::move(batches.indices[i]));
    }

    ProcessingResult result;
    result.X = std::move(all_X);
    result.indices = std::move(all_indices);
    result.cluster_ids = std::move(truncated);
    return result;
}

#endif