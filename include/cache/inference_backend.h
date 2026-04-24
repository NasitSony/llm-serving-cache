#pragma once
#include <string>

struct InferenceRequest {
    std::string request_id;
    int prompt_tokens{0};
    int output_tokens{0};
    int prefix_hit_tokens{0};
    std::string prompt;   // 🔥 ADD THIS
};

struct InferenceResult {
    int uncached_tokens{0};
    int prefill_latency_ms{0};
    int decode_latency_ms{0};
    int total_latency_ms{0};
    std::string response_text;
};

class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;
    virtual InferenceResult Run(const InferenceRequest& req) = 0;
};