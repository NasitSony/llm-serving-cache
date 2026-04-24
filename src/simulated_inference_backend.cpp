#include "cache/inference_backend.h"
#include <algorithm>

class SimulatedInferenceBackend : public InferenceBackend {
public:
    InferenceResult Run(const InferenceRequest& req) override {
        const int routing_overhead_ms = 5;
        const int prefill_cost_per_token = 1;
        const int decode_cost_per_token = 1;

        int uncached_tokens = std::max(0, req.prompt_tokens - req.prefix_hit_tokens);

        int prefill_latency = uncached_tokens * prefill_cost_per_token;
        int decode_latency = req.output_tokens * decode_cost_per_token;

        return {
            uncached_tokens,
            prefill_latency,
            decode_latency,
            routing_overhead_ms + prefill_latency + decode_latency
        };
    }
};