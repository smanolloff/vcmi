/*
 * NNModel.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>

#include "StdInc.h"
#include "vstd/CLoggerBase.h"
#include "json/JsonNode.h"
#include "NNModel.h"

#ifdef _WIN32
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif

    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif

    #include <windows.h>
#endif

namespace MMAI::BAI {

constexpr int LT_COUNT = EI(MMAI::Schema::V13::LinkType::_count);

namespace {
    template<class... Args>
    [[noreturn]] inline void throwf(const std::string& fmt, Args&&... args) {
        boost::format f("NNModel: " + fmt);
        (void)std::initializer_list<int>{ ( (f % std::forward<Args>(args)), 0 )... };
        throw std::runtime_error(f.str());
    }

    inline std::basic_string<ORTCHAR_T> ToOrtPath(const std::string &utf8_path) {
        // https://github.com/microsoft/onnxruntime/discussions/21915
        #ifdef _WIN32
            if (utf8_path.empty())
                return std::basic_string<ORTCHAR_T>();

            int size_needed = MultiByteToWideChar(
                CP_UTF8, 0,
                utf8_path.c_str(), static_cast<int>(utf8_path.size()),
                nullptr, 0
            );

            std::wstring wpath(size_needed, L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0,
                utf8_path.c_str(), static_cast<int>(utf8_path.size()),
                &wpath[0], size_needed
            );

            return wpath;  // ORTCHAR_T == wchar_t on Windows
        #else
            // On non-Windows, ORTCHAR_T == char, so no conversion needed.
            return utf8_path;
        #endif
    }

    struct ScopedTimer {
        std::string name;
        std::chrono::steady_clock::time_point t0;
        explicit ScopedTimer(const std::string& n) : name(n), t0(std::chrono::steady_clock::now()) {}
        ~ScopedTimer() {
            auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            logAi->info("%s: %lld ms", name, dt);
        }
    };

    struct IndexContainer {
        std::array<std::vector<int32_t>, 2> ei;     // edge_index
        std::vector<float> ea;                      // edge_attributes
        std::array<std::vector<int32_t>, 165> nbrs; // node neighbourhoods
    };

    struct SampleResult {
        int index;
        double prob;
        bool fallback;
    };

    struct TripletSample {
        int act0;
        int hex1;
        int hex2;
        double confidence;
    };

    // ---------- ONNX Runtime tensor helpers ----------

    inline std::vector<int64_t> shape_of(const Ort::Value& v) {
        if (!v.IsTensor()) throwf("Expected tensor Ort::Value");
        Ort::TensorTypeAndShapeInfo info = v.GetTensorTypeAndShapeInfo();
        return info.GetShape();
    }

    template <typename T>
    inline std::vector<T> to_vector(const Ort::Value& v) {
        if (!v.IsTensor()) throwf("Expected tensor Ort::Value");
        Ort::TensorTypeAndShapeInfo info = v.GetTensorTypeAndShapeInfo();
        const std::vector<int64_t> shp = info.GetShape();
        size_t n = 1;
        for (auto d : shp) {
            if (d < 0) throwf("Dynamic dim not supported here");
            n *= static_cast<size_t>(d);
        }
        const T* p = v.GetTensorData<T>();            // pointer used only for the copy
        return std::vector<T>(p, p + n);              // everything below uses vectors only
    }

    inline int argmax(const std::vector<double>& xs) {
        if (xs.empty()) throwf("argmax on empty vector");
        size_t best = 0;
        for (size_t i = 1; i < xs.size(); ++i) {
            if (xs.at(i) > xs.at(best)) best = i;
        }
        return static_cast<int>(best);
    }

    inline std::vector<double> softmax(const std::vector<double>& logits) {
        if (logits.empty()) return {};
        double m = -std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < logits.size(); ++i) m = std::max(m, logits.at(i));
        std::vector<double> exps(logits.size(), 0.0);
        double sum = 0.0;
        for (size_t i = 0; i < logits.size(); ++i) {
            const double v = logits.at(i) - m;
            const double e = std::isfinite(v) ? std::exp(v) : 0.0;
            exps.at(i) = e;
            sum += e;
        }
        if (sum == 0.0) return std::vector<double>(logits.size(), 0.0);
        for (size_t i = 0; i < exps.size(); ++i) exps.at(i) /= sum;
        return exps;
    }

    // Masked categorical sampling given a logits vector
    inline SampleResult sample_masked_logits(
        const std::vector<float>& logits_1d,
        const std::vector<int32_t>& mask_1d,
        bool throw_if_empty,
        double temperature,
        std::mt19937& rng
    ) {
        const size_t K = logits_1d.size();
        if (K == 0 || mask_1d.size() != K) throwf("Invalid logits/mask sizes");
        if (temperature < 0.0) throwf("Negative temperature");

        int n_valid = 0;
        for (size_t i = 0; i < K; ++i) n_valid += (mask_1d.at(i) != 0);
        if (n_valid == 0) {
            if (throw_if_empty) throwf("No valid options available");
            return {0, 0.0, true};
        }

        const double neginf = -std::numeric_limits<double>::infinity();

        // Promote to double and mask with -inf
        std::vector<double> masked_logits(K, neginf);
        for (size_t i = 0; i < K; ++i) {
            if (mask_1d.at(i)) masked_logits.at(i) = static_cast<double>(logits_1d.at(i));
        }

        int idx_chosen = 0;
        double p_chosen = 0.0;

        if (temperature > 1e8) {
            std::vector<double> probs(K, 0.0);
            const double p = 1.0 / static_cast<double>(n_valid);
            for (size_t i = 0; i < K; ++i) if (mask_1d.at(i)) probs.at(i) = p;

            std::discrete_distribution<int> dist(probs.begin(), probs.end());
            idx_chosen = dist(rng);
            p_chosen = probs.at(static_cast<size_t>(idx_chosen));
        } else if (temperature < 1e-8) {
            idx_chosen = argmax(masked_logits);
            p_chosen = 1.0;
        } else {
            std::vector<double> scaled(K, neginf);
            for (size_t i = 0; i < K; ++i) {
                if (mask_1d.at(i)) scaled.at(i) = masked_logits.at(i) / temperature;
            }

            const std::vector<double> probs = softmax(scaled);
            for (size_t i = 0; i < probs.size(); ++i) {
                if (!std::isfinite(probs.at(i))) throwf("Non-finite probabilities");
            }

            std::discrete_distribution<int> dist(probs.begin(), probs.end());
            idx_chosen = dist(rng);
            p_chosen = probs.at(static_cast<size_t>(idx_chosen));
        }

        return {idx_chosen, p_chosen, false};
    }

    //
    // Samples a {action, hex1, hex2} triplet given output logits and masks
    //
    // Expected shapes:
    //   act0_logits: [1, 4]            float32
    //   hex1_logits: [1, 165]          float32
    //   hex2_logits: [1, 165]          float32
    //   mask_act0:   [1, 4]            int32
    //   mask_hex1:   [1, 4, 165]       int32
    //   mask_hex2:   [1, 4, 165, 165]  int32
    //
    inline TripletSample sample_triplet(
        const Ort::Value& act0_logits,
        const Ort::Value& hex1_logits,
        const Ort::Value& hex2_logits,
        const Ort::Value& mask_act0,
        const Ort::Value& mask_hex1,
        const Ort::Value& mask_hex2,
        double temperature,
        std::mt19937& rng
    ) {
        const std::vector<int64_t> s_a0 = shape_of(act0_logits);
        const std::vector<int64_t> s_h1 = shape_of(hex1_logits);
        const std::vector<int64_t> s_h2 = shape_of(hex2_logits);
        const std::vector<int64_t> s_m0 = shape_of(mask_act0);
        const std::vector<int64_t> s_m1 = shape_of(mask_hex1);
        const std::vector<int64_t> s_m2 = shape_of(mask_hex2);

        if (s_a0 != std::vector<int64_t>({1, 4})) throwf("act0_logits must be [1,4]");
        if (s_h1 != std::vector<int64_t>({1, 165})) throwf("hex1_logits must be [1,165]");
        if (s_h2 != std::vector<int64_t>({1, 165})) throwf("hex2_logits must be [1,165]");
        if (s_m0 != std::vector<int64_t>({1, 4})) throwf("mask_act0 must be [1,4]");
        if (s_m1 != std::vector<int64_t>({1, 4, 165})) throwf("mask_hex1 must be [1,4,165]");
        if (s_m2 != std::vector<int64_t>({1, 4, 165, 165})) throwf("mask_hex2 must be [1,4,165,165]");

        // Materialize host vectors and squeeze batch
        std::vector<float> a0_log = to_vector<float>(act0_logits);  // 4
        std::vector<float> h1_log = to_vector<float>(hex1_logits);  // 165
        std::vector<float> h2_log = to_vector<float>(hex2_logits);  // 165

        std::vector<int32_t> m_a0 = to_vector<int32_t>(mask_act0);  // 4
        std::vector<int32_t> m_h1 = to_vector<int32_t>(mask_hex1);  // 4*165
        std::vector<int32_t> m_h2 = to_vector<int32_t>(mask_hex2);  // 4*165*165

        // ---- act0 ----
        const SampleResult act0 = sample_masked_logits(a0_log, m_a0, true, temperature, rng);

        // ---- hex1 mask slice for chosen act0 ----
        const size_t h1_row_offset = static_cast<size_t>(act0.index) * static_cast<size_t>(165);
        std::vector<int32_t> m_h1_for_act0(static_cast<size_t>(165), 0);
        for (size_t k = 0; k < static_cast<size_t>(165); ++k) {
            m_h1_for_act0.at(k) = m_h1.at(h1_row_offset + k);
        }

        // ---- hex1 ----
        const SampleResult hex1 = sample_masked_logits(
            h1_log, m_h1_for_act0, false, temperature, rng);

        // ---- hex2 mask slice for (act0, hex1) ----
        // index = ((act0 * 165) + hex1) * 165 + k
        const size_t base = (static_cast<size_t>(act0.index) * static_cast<size_t>(165)
                           + static_cast<size_t>(hex1.index)) * static_cast<size_t>(165);
        std::vector<int32_t> m_h2_for_pair(static_cast<size_t>(165), 0);
        for (size_t k = 0; k < static_cast<size_t>(165); ++k) {
            m_h2_for_pair.at(k) = m_h2.at(base + k);
        }

        // ---- hex2 ----
        const SampleResult hex2 = sample_masked_logits(
            h2_log, m_h2_for_pair, false, temperature, rng);

        // ---- joint confidence ----
        const double confidence =
            act0.prob * (hex1.fallback ? 1.0 : hex1.prob) * (hex2.fallback ? 1.0 : hex2.prob);

        return {act0.index, hex1.index, hex2.index, confidence};
    }

    std::array<std::vector<int32_t>, 165> buildNBR_unpadded(const std::vector<int64_t>& dst) {
        // Pass 1: validate and count degrees per node
        std::array<int, 165> deg{};
        for (size_t e = 0; e < dst.size(); ++e) {
            int v = static_cast<int>(dst[e]);
            if (v < 0 || v >= 165)
                throwf("dst contains node id out of range: ", v);
            ++deg[v];
        }

        std::array<std::vector<int32_t>, 165> nbr{};
        for (int v = 0; v < 165; ++v) nbr[v].reserve(deg[v]);
        for (size_t e = 0; e < dst.size(); ++e) {
            int v = static_cast<int>(dst[e]);
            nbr[v].push_back(static_cast<int32_t>(e));
        }

        return nbr;
    }

    struct BuildOutputs {
        int size_index = -1;                                // chosen index in all_sizes
        std::array<int32_t, LT_COUNT> emax{};               // chosen emax per link type
        std::array<int32_t, LT_COUNT> kmax{};               // chosen kmax per link type

        std::array<std::vector<int32_t>, 2> ei_flat;        // each length sum(emax)
        std::vector<float> ea_flat;                         // length sum(emax)
        std::array<std::vector<int32_t>, 165> nbrs_flat;    // each length sum(kmax)
    };

    // all_sizes: S x LT_COUNT x 2, where [s][l] = {emax, kmax}
    BuildOutputs build_flattened(
        const std::array<IndexContainer, LT_COUNT>& containers,
        const std::vector<std::vector<std::vector<int32_t>>>& all_sizes,
        int bucket
    ) {
        BuildOutputs out{};

        // Required per-linktype capacities from data
        std::array<size_t, LT_COUNT> e_req{};
        std::array<size_t, LT_COUNT> k_req{};
        for (int l = 0; l < LT_COUNT; ++l) {
            e_req[l] = containers[l].ea.size();
            size_t km = 0;
            for (int v = 0; v < 165; ++v)
                km = std::max(km, containers[l].nbrs[v].size());
            k_req[l] = km;
        }

        // 1) Find smallest valid bucket size index
        int chosen = -1;
        std::array<int32_t, LT_COUNT> emax{}, kmax{};
        for (int s = 0; s < static_cast<int>(all_sizes.size()); ++s) {
            const auto& sz = all_sizes[s];
            if (sz.size() != LT_COUNT) continue;  // skip malformed
            bool ok = true;
            for (int l = 0; l < LT_COUNT && ok; ++l) {
                if (sz[l].size() != 2) { ok = false; break; }
                int32_t emax_l = sz[l][0];
                int32_t kmax_l = sz[l][1];
                if (emax_l < static_cast<int32_t>(e_req[l]) ||
                    kmax_l < static_cast<int32_t>(k_req[l])) {
                    ok = false;
                }
            }
            ok = ok && (bucket == -1 || s == bucket);
            if (ok) {
                chosen = s;
                for (int l = 0; l < LT_COUNT; ++l) {
                    emax[l] = sz[l][0];
                    kmax[l] = sz[l][1];
                }
                break;
            }
        }

        if (chosen < 0)
            throwf("too many units on the battlefield");

        logAi->debug("Size: %d", chosen);
        for (int i=0; i<LT_COUNT; ++i)
            logAi->debug("  %d: [%ld, %ld] -> [%lld, %lld]", i, e_req[i], k_req[i], all_sizes[chosen][i][0], all_sizes[chosen][i][1]);


        out.size_index = chosen;
        out.emax = emax;
        out.kmax = kmax;

        // Precompute sums
        const size_t sum_emax = std::accumulate(emax.begin(), emax.end(), 0);
        const size_t sum_kmax = std::accumulate(kmax.begin(), kmax.end(), 0);

        // 2) Build ei_flat and ea_flat (concat each layer's ei/ea, zero-padded to emax[l])
        out.ei_flat.at(0).clear();
        out.ei_flat.at(1).clear();
        out.ea_flat.clear();

        out.ei_flat.at(0).reserve(sum_emax);
        out.ei_flat.at(1).reserve(sum_emax);
        out.ea_flat.reserve(sum_emax);
        for (int l = 0; l < LT_COUNT; ++l) {
            const auto& ei = containers[l].ei;
            const auto& ea = containers[l].ea;

            out.ei_flat.at(0).insert(out.ei_flat.at(0).end(), ei.at(0).begin(), ei.at(0).end());
            out.ei_flat.at(1).insert(out.ei_flat.at(1).end(), ei.at(1).begin(), ei.at(1).end());
            out.ea_flat.insert(out.ea_flat.end(), ea.begin(), ea.end());

            size_t need = static_cast<size_t>(emax[l]) - ei.at(0).size();
            if (need > 0) out.ei_flat.at(0).insert(out.ei_flat.at(0).end(), need, 0);

            need = static_cast<size_t>(emax[l]) - ei.at(1).size();
            if (need > 0) out.ei_flat.at(1).insert(out.ei_flat.at(1).end(), need, 0);

            need = static_cast<size_t>(emax[l]) - ea.size();
            if (need > 0) out.ea_flat.insert(out.ea_flat.end(), need, 0.0f);
        }
        // Sanity
        if (out.ei_flat.at(0).size() != sum_emax)
            throwf("ei_flat.at(0) size mismatch: want: %d, have: %zu", sum_emax, out.ei_flat.at(0).size());
        if (out.ei_flat.at(1).size() != sum_emax)
            throwf("ei_flat.at(1) size mismatch: want: %d, have: %zu", sum_emax, out.ei_flat.at(1).size());
        if (out.ea_flat.size() != sum_emax)
            throwf("ea_flat size mismatch: want: %d, have: %zu", sum_emax, out.ea_flat.size());

        // 3) Build nbrs_flat per node: concat layer l, pad to kmax[l] with -1
        for (int v = 0; v < 165; ++v) {
            auto& dst = out.nbrs_flat[v];
            dst.clear();
            dst.reserve(sum_kmax);
            for (int l = 0; l < LT_COUNT; ++l) {
                const auto& src = containers[l].nbrs[v];
                dst.insert(dst.end(), src.begin(), src.end());
                const size_t need = static_cast<size_t>(kmax[l]) - src.size();
                if (need > 0) dst.insert(dst.end(), need, -1);
            }

            if (dst.size() != sum_kmax) {
                throwf("nbrs_flat row size mismatch: want: %zu, have: %zu", sum_kmax, dst.size());
            }
        }

        return out;
    }
} // namespace {}


NNModel::NNModel(std::string &path, float temperature, uint64_t seed)
: path(path)
, temperature(temperature)
, meminfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
{
    logAi->info("MMAI params: seed=%1%, temperature=%2%, model=%3%", seed, temperature, path);

    if (seed == 0) {
        seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        logAi->info("Seed is 0, using %1%", seed);
    }
    rng = std::mt19937(seed);

    auto opts = Ort::SessionOptions();
    opts.SetIntraOpNumThreads(4);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

    model = std::make_unique<Ort::Session>(ort_env(), ToOrtPath(path).c_str(), opts);
    auto md = model->GetModelMetadata();

    {
        Ort::AllocatedStringPtr v = md.LookupCustomMetadataMapAllocated("version", allocator);
        if (!v) throw std::runtime_error("metadata error: version: no such key");
        std::string vs(v.get());
        try {
            version = std::stoi(vs);
        } catch (...) {
            throw std::runtime_error("metadata error: version: not an int");
        }
    }

    {
        //
        // side
        //   dtype=int
        //   shape=scalar
        //
        // Battlefield side the model was trained on (see Schema::Side enum).
        //
        Ort::AllocatedStringPtr v = md.LookupCustomMetadataMapAllocated("side", allocator);
        if (!v) throw std::runtime_error("metadata error: side: no such key");
        std::string vs(v.get());
        try {
            side = Schema::Side(std::stoi(vs));
        } catch (...) {
            throw std::runtime_error("metadata error: side: not an int");
        }
    }

    if (version != 13)
        throwf("unsupported model version: want: 13, have: %d", version);

    {
        //
        // buckets
        //   dtype=int
        //   shape=[5, 7, 2]:
        //     d1: bucket size (S, M, L, XL, XXL)
        //     d2: edge type (see Schema::V13::LinkType enum)
        //     d3: pairs of [emax, kmax]:
        //      emax = max number of outbound node edges
        //      kmax = max number of inbound node edges
        //
        // Stats (10K steps):
        //
        //        Num edges (E)   avg   max   p99   p90   p75   p50   p25
        // -----------------------------------------------------------------
        //             ADJACENT   888   888   888   888   888   888   888
        //                REACH   355   988   820   614   478   329   209
        //           RANGED_MOD   408   2403  1285  646   483   322   162
        //          ACTS_BEFORE   51    268   203   118   75    35    15
        //        MELEE_DMG_REL   43    198   160   103   60    31    14
        //        RETAL_DMG_REL   27    165   113   67    38    18    8
        //       RANGED_DMG_REL   12    133   60    29    18    9     4
        //
        //    Inbound edges (K)   avg   max   p99   p90   p75   p50   p25
        // -----------------------------------------------------------------
        //             ADJACENT   5.4   6     6     6     6     6     6
        //                REACH   2.2   13    10    8     6     4     3
        //           RANGED_MOD   2.5   15    8     4     3     2     1
        //          ACTS_BEFORE   0.3   23    19    15    12    8     5
        //        MELEE_DMG_REL   0.3   10    9     8     7     5     3
        //        RETAL_DMG_REL   0.2   10    9     8     6     5     3
        //       RANGED_DMG_REL   0.1   8     6     3     2     2     1
        //
        // Approx. sizes are S=p50 / M=p90 / L=p99 / XL=max / XXL=fallback
        // Exact values defined in the vcmi-gym project and are subject to change.
        //
        Ort::AllocatedStringPtr ab = md.LookupCustomMetadataMapAllocated("all_sizes", allocator);
        if (!ab) throw std::runtime_error("metadata key 'all_sizes' missing");
        const std::string jsonstr(ab.get());
        try {
            const void* raw = static_cast<const void*>(jsonstr.data());
            const std::byte* bytes = static_cast<const std::byte*>(raw);
            auto jn = JsonNode(bytes, jsonstr.size(), "<ONNX metadata: all_sizes>");

            if (!jn.isVector())
                throwf("all_buckets: bad JsonType: want: %d, have: %d", EI(JsonNode::JsonType::DATA_VECTOR), EI(jn.getType()));

            for (auto &jv0 : jn.Vector()) {
                auto vec1 = std::vector<std::vector<int32_t>> {};
                for (auto &jv1 : jv0.Vector()) {
                    auto vec2 = std::vector<int32_t> {};
                    for (auto &jv2 : jv1.Vector()) {
                        if (!jv2.isNumber()) {
                            throwf("invalid data type: want: %d, got: %d", EI(JsonNode::JsonType::DATA_INTEGER), EI(jv2.getType()));
                        }
                        vec2.push_back(static_cast<int32_t>(jv2.Integer()));
                    }
                    vec1.emplace_back(vec2);
                }
                all_buckets.emplace_back(vec1);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("failed to parse 'all_buckets' JSON: ") + e.what());
        }

        if (all_buckets.size() != 5) throwf("all_buckets: bad size for d1: want: 5, have: %zu", all_buckets.size());
        if (all_buckets[0].size() != 7) throwf("all_buckets: bad size for d2: want: 7, have: %zu", all_buckets[0].size());
        if (all_buckets[0][0].size() != 2) throwf("all_buckets: bad size for d3: want: 2, have: %zu", all_buckets[0][0].size());
    }

    {
        //
        // action_table
        //   dtype=int
        //   shape=[4, 165, 165]:
        //     d1: action (WAIT, MOVE, AMOVE, SHOOT)
        //     d2: target hex for MOVE, AMOVE or SHOOT
        //     d3: target hex for AMOVE (attack destination)
        //
        Ort::AllocatedStringPtr ab = md.LookupCustomMetadataMapAllocated("action_table", allocator);
        if (!ab) throw std::runtime_error("metadata key 'action_table' missing");
        const std::string jsonstr(ab.get());

        try {
            auto jn = JsonNode(reinterpret_cast<const std::byte*>(jsonstr.data()), jsonstr.size(), "<ONNX metadata: all_sizes>");

            for (auto &jv0 : jn.Vector()) {
                auto vec1 = std::vector<std::vector<int32_t>> {};
                for (auto &jv1 : jv0.Vector()) {
                    auto vec2 = std::vector<int32_t> {};
                    for (auto &jv2 : jv1.Vector()) {
                        if (!jv2.isNumber()) {
                            throwf("invalid data type: want: %d, got: %d", EI(JsonNode::JsonType::DATA_INTEGER), EI(jv2.getType()));
                        }
                        vec2.push_back(static_cast<int32_t>(jv2.Integer()));
                    }
                    vec1.emplace_back(vec2);
                }
                action_table.emplace_back(vec1);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("failed to parse 'action_table' JSON: ") + e.what());
        }

        if (action_table.size() != 4) throwf("action_table: bad size for d1: want: 4, have: %zu", action_table.size());
        if (action_table[0].size() != 165) throwf("action_table: bad size for d2: want: 165, have: %zu", action_table[0].size());
        if (action_table[0][0].size() != 165) throwf("action_table: bad size for d3: want: 165, have: %zu", action_table[0][0].size());
    }

    {
        // Model inputs
        auto icount = model->GetInputCount();
        if (icount != 4)
            throwf("wrong input count: want: %d, have: %lld", 4, icount);

        input_name_ptrs.reserve(icount);
        input_names.reserve(icount);
        for (size_t i = 0; i < icount; ++i) {
            input_name_ptrs.emplace_back(model->GetInputNameAllocated(i, allocator));
            input_names.push_back(input_name_ptrs.back().get());
        }
    }

    {
        // Model outputs
        auto ocount = model->GetOutputCount();
        if (ocount != 10)
            throwf("wrong output count: want: %d, have: %lld", ocount, ocount);

        output_name_ptrs.reserve(ocount);
        output_names.reserve(ocount);

        for (size_t i = 0; i < ocount; ++i) {
            output_name_ptrs.emplace_back(model->GetOutputNameAllocated(i, allocator));
            output_names.push_back(output_name_ptrs.back().get());
        }
    }

    logAi->info("MMAI version " + std::to_string(version) + "initialized on side=" + std::to_string(EI(side)));
}

Schema::ModelType NNModel::getType() {
    return Schema::ModelType::NN;
};

std::string NNModel::getName() {
    return "MMAI_MODEL";
};

int NNModel::getVersion() {
    return version;
};

Schema::Side NNModel::getSide() {
    return side;
};

int NNModel::getAction(const MMAI::Schema::IState * s) {
    auto timer = ScopedTimer("getAction");
    auto any = s->getSupplementaryData();

    if (s->version() != version)
        throwf("getAction: unsupported IState version: want: %d, have: %d", version, s->version());

    if(!any.has_value()) throw std::runtime_error("extractSupplementaryData: supdata is empty");
    auto err = MMAI::Schema::AnyCastError(any, typeid(const MMAI::Schema::V13::ISupplementaryData*));
    if(!err.empty())
        throwf("getAction: anycast failed: %s", err);

    const auto *sup = std::any_cast<const MMAI::Schema::V13::ISupplementaryData*>(any);

    if (sup->getIsBattleEnded())
        return MMAI::Schema::ACTION_RESET;

    auto [inputs, size_idx] = prepareInputsV13(s, sup);

    auto outputs = model->Run(
        Ort::RunOptions(),
        input_names.data(),
        inputs.data(),
        inputs.size(),
        output_names.data(),
        output_names.size()
    );

    if (outputs.size() != 10)
        throwf("getAction: bad output size: want: 10, have: %d", outputs.size());

    // deterministic action (useful for debugging)
    auto action = t2v<int32_t>("getAction: t_action", outputs[0], 1).at(0);

    auto sample = sample_triplet(
        outputs[1], // [1, 4]               t_act0_logits
        outputs[2], // [1, 165]             t_hex1_logits
        outputs[3], // [1, 165]             t_hex2_logits
        outputs[4], // [1, 4]               t_mask_act0
        outputs[5], // [1, 4, 165]          t_mask_hex1
        outputs[6], // [1, 4, 165, 165]     t_mask_hex2
        temperature,
        rng
    );

    auto s_action = action_table.at(sample.act0).at(sample.hex1).at(sample.hex2);

    if (s_action != action)
        logAi->debug("Sampled a non-greedy action: %d != %d", s_action, action);

    timer.name = boost::str(boost::format("MMAI action: %d (confidence=%.2f)") % action % sample.confidence);

    return static_cast<MMAI::Schema::Action>(action);
};

double NNModel::getValue(const MMAI::Schema::IState * s) {
    return 0;
}

std::pair<std::vector<Ort::Value>, int> NNModel::prepareInputsV13(
    const MMAI::Schema::IState * s,
    const MMAI::Schema::V13::ISupplementaryData* sup,
    int bucket
) {
    // XXX: if needed, support for other versions may be added via conditionals
    if (version != 13)
        throwf("unsupported version: want: 13, have: %d", version);

    auto containers = std::array<IndexContainer, LT_COUNT> {};

    int count = 0;

    for (const auto &[type, links] : sup->getAllLinks()) {
        // assert order
        if (EI(type) != count)
            throwf("unexpected link type: want: %d, have: %d", count, EI(type));

        auto &c = containers.at(count);

        const auto srcinds = links->getSrcIndex();
        const auto dstinds = links->getDstIndex();
        const auto attrs = links->getAttributes();

        auto nlinks = srcinds.size();

        if (dstinds.size() != nlinks)
            throwf("unexpected dstinds.size() for LinkType(%d): want: %d, have: %d", EI(type), nlinks, dstinds.size());

        if (attrs.size() != nlinks)
            throwf("unexpected attrs.size() for LinkType(%d): want: %d, have: %d", EI(type), nlinks, attrs.size());

        // c.e_max = nlinks;
        // c.k_max = k_max;

        c.ei.at(0).reserve(nlinks);
        c.ei.at(1).reserve(nlinks);
        c.ei.at(0).insert(c.ei.at(0).end(), srcinds.begin(), srcinds.end());
        c.ei.at(1).insert(c.ei.at(1).end(), dstinds.begin(), dstinds.end());

        c.ea.reserve(nlinks);
        c.ea.insert(c.ea.end(), attrs.begin(), attrs.end());

        c.nbrs = buildNBR_unpadded(dstinds);

        ++count;
    }

    if (count != LT_COUNT)
        throwf("unexpected links count: want: %d, have: %d", LT_COUNT, count);

    auto build = build_flattened(containers, all_buckets, bucket);

    const auto *state = s->getBattlefieldState();
    auto estate = std::vector<float>(state->size());
    std::copy(state->begin(), state->end(), estate.begin());

    int sum_e = build.ei_flat.at(0).size();
    int sum_k = build.nbrs_flat.at(0).size();

    if (build.ei_flat.at(0).size() != sum_e)
        throwf("unexpected build.ei_flat.at(0).size(): want: %d, have: %d", sum_e, build.ei_flat.at(0).size());
    if (build.ei_flat.at(1).size() != sum_e)
        throwf("unexpected build.ei_flat.at(1).size(): want: %d, have: %d", sum_e, build.ei_flat.at(1).size());
    if (build.ea_flat.size() != sum_e)
        throwf("unexpected build.ea_flat.size(): want: %d, have: %d", sum_e, build.ea_flat.size());
    for (int i=0; i<165; ++i) {
        if (build.nbrs_flat.at(i).size() != sum_k) {
            throwf("unexpected build.nbrs_flat.at(%d).size(): want: %d, have: %d", i, sum_k, build.nbrs_flat.at(i).size());
        }
    }

    auto einds = std::vector<int32_t> {};
    einds.reserve(2*sum_e);
    for (auto &eind : build.ei_flat)
        einds.insert(einds.end(), eind.begin(), eind.end());

    auto nbrs = std::vector<int32_t> {};
    nbrs.reserve(165*sum_k);
    for (auto &nbr : build.nbrs_flat)
        nbrs.insert(nbrs.end(), nbr.begin(), nbr.end());

    std::vector<Ort::AllocatedStringPtr> input_name_ptrs;
    std::vector<const char*> input_names;
    input_name_ptrs.reserve(4);
    input_names.reserve(4);

    for (size_t i = 0; i < 4; ++i) {
        input_name_ptrs.emplace_back(model->GetInputNameAllocated(i, allocator));
        input_names.push_back(input_name_ptrs.back().get());
    }

    auto tensors = std::vector<Ort::Value> {};
    tensors.push_back(toTensor("state", estate, {static_cast<int64_t>(estate.size())}));
    tensors.push_back(toTensor("ei_flat", einds, {2, sum_e}));
    tensors.push_back(toTensor("ea_flat", build.ea_flat, {sum_e, 1}));
    tensors.push_back(toTensor("nbr_flat", nbrs, {165, sum_k}));

    return {std::move(tensors), build.size_index};
}

template <typename T>
Ort::Value NNModel::toTensor(const std::string& name, std::vector<T>& vec, const std::vector<int64_t>& shape) {
    // Sanity check
    int64_t numel = 1;
    for (int64_t d : shape)
        numel *= d;

    if (numel != vec.size())
        throwf("toTensor: %s: numel check failed: want: %d, have: %d", name, numel, vec.size());

    // Create a memory-owning tensor then copy data
    auto res = Ort::Value::CreateTensor<T>(allocator, shape.data(), shape.size());
    T* dst = res.template GetTensorMutableData<T>();
    std::memcpy(dst, vec.data(), vec.size() * sizeof(T));
    return res;
}

// tensor-to-vector convenience
template <typename T>
std::vector<T> NNModel::t2v(const std::string& name, const Ort::Value& tensor, int numel) {
    // Expect int32 tensor of shape {1}
    auto type_info = tensor.GetTensorTypeAndShapeInfo();
    auto dtype = type_info.GetElementType();

    if constexpr (std::is_same_v<T, float>) {
        if (dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
            throwf("t2v: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT), EI(dtype));
        }
    } else if constexpr (std::is_same_v<T, int>) {
        if (dtype != ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32) {
            throwf("t2v: %s: bad dtype: want: %d, have: %d", name, EI(ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32), EI(dtype));
        }
    } else {
        throwf("t2v: %s: can only work with float and int", name);
    }

    auto shape = type_info.GetShape();
    if (shape.size() != 1) {
        throwf("t2v: %s: expected ndim=1, got: %d", name, shape.size());
    }

    if (shape != std::vector<int64_t>{numel}) {
        throwf("t2v: %s: bad shape", name);
    }

    const T* data = tensor.GetTensorData<T>();
    // int32_t result = out_data[0];

    auto res = std::vector<T>{};
    res.reserve(numel);
    res.assign(data, data + numel);   // v now owns a copy
    return res;
}

} // namespace MMAI::BAI
