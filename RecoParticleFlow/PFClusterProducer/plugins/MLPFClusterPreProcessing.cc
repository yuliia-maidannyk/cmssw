#define EIGEN_DONT_VECTORIZE

#include <Eigen/Dense>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include "RecoParticleFlow/PFClusterProducer/interface/MLPFClusterPreProcessing.h"

#define PRINT_DEBUG 0

struct DiagnosticCounters
{
    std::atomic<int> total_events{0};
    std::atomic<int> total_clusters{0};
    std::atomic<int> seed_found{0};

    void print_summary() const
    {
        int ev = total_events.load();
        int sf = seed_found.load();
        int tc = total_clusters.load();
        std::cout << "\n";
        std::cout << "+------------------------------------------------------+\n";
        std::cout << "|          PREPROCESSING DIAGNOSTICS                   |\n";
        std::cout << "+------------------------------------------------------+\n";
        std::cout << "|  INPUT                                               |\n";
        std::cout << "|    Events processed       : " << std::setw(7) << ev  << "                   |\n";
        std::cout << "|    Seeds found            : " << std::setw(7) << sf  << "                   |\n";
        std::cout << "|    Total clusters         : " << std::setw(7) << tc  << "                   |\n";
        std::cout << "+------------------------------------------------------+\n";
    }
};

// -----------------------------------------------------------------------------
// Data structures
// -----------------------------------------------------------------------------

struct Window
{
    Eigen::MatrixXf X;
    Eigen::Vector2i indices;
    double          deposited_energy;
};

struct EventWindows { std::vector<Window> windows; };

struct BatchedWindows
{
    std::vector<std::vector<Eigen::MatrixXf>> X, indices;
};

// -----------------------------------------------------------------------------
// Methods
// -----------------------------------------------------------------------------

std::vector<Eigen::Vector2i> find_seed_candidates(
    const float* X_ptr, int rows, int cols, double threshold, DiagnosticCounters& diag)
{
    std::vector<Eigen::Vector2i> out;
    out.reserve(100);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (X_ptr[r * cols + c] > threshold)
            {
                out.emplace_back(r, c);
                diag.seed_found++;
            }
    return out;
}

EventWindows extract_windows_for_event(
    const float* X_ptr, int rows, int cols, double threshold, int crop_size, DiagnosticCounters& diag)
{
    EventWindows result;

    auto seed_candidates = find_seed_candidates(X_ptr, rows, cols, threshold, diag);

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

std::vector<std::vector<int>> compute_and_cluster_windows(const std::vector<Window>& windows, int overlap_size)
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

        for (size_t i = 0; i < cluster.size(); ++i) {
            int cur = cluster[i];
            for (int j = seed_id + 1; j < n; ++j) {
                if (processed[j]) continue;
                int dx = std::abs(windows[cur].indices[0] - windows[j].indices[0]);
                int dy = std::abs(windows[cur].indices[1] - windows[j].indices[1]);
                if (dx <= half_overlap && dy <= half_overlap) {
                    cluster.push_back(j);
                    processed[j] = true;
                }
            }
        }
        clusters.push_back(std::move(cluster));
    }
    return clusters;
}

BatchedWindows batch_clusters(
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
            Eigen::MatrixXf idx(2,1), en(1,1); 
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

ProcessingResult get_model_samples(
    const std::vector<float>& map, int rows, int cols, 
    double threshold, int crop_size, int overlap_limit, int max_cluster)
{
    const float* X_ptr = map.data();

    DiagnosticCounters diag;
    diag.total_events = 1;

    EventWindows windows = extract_windows_for_event(X_ptr, rows, cols, threshold, crop_size, diag);

    sort_windows_by_energy(windows);

    auto clusters = compute_and_cluster_windows(windows.windows, overlap_limit);
    diag.total_clusters = static_cast<int>(clusters.size());

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

    if (PRINT_DEBUG) {
        std::cout << "\n";
        diag.print_summary();
    }

    ProcessingResult result;
    result.X = std::move(all_X);
    result.indices = std::move(all_indices);
    result.cluster_ids = std::move(truncated);
    return result;
}