#include "cache/inference_backend.h"
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

class OllamaInferenceBackend : public InferenceBackend {
public:
    InferenceResult Run(const InferenceRequest& req) override {
        std::string prompt_json = nlohmann::json(req.prompt).dump();

        std::string cmd =
            "curl -s http://localhost:11434/api/generate -d '{"
            "\"model\": \"llama3.1:8b\","
            "\"prompt\": " + prompt_json + ","
            "\"stream\": false"
            "}'";

        std::string output = exec(cmd);
        if (output.empty()) {
            throw std::runtime_error("Ollama returned empty response");
        }

        auto json = nlohmann::json::parse(output);

        int total_ms =
            static_cast<int>(json.value("total_duration", 0LL) / 1000000LL);
        int prompt_ms =
            static_cast<int>(json.value("prompt_eval_duration", 0LL) / 1000000LL);
        int eval_ms =
            static_cast<int>(json.value("eval_duration", 0LL) / 1000000LL);

        InferenceResult result;
        result.uncached_tokens = std::max(0, req.prompt_tokens - req.prefix_hit_tokens);
        result.prefill_latency_ms = prompt_ms;
        result.decode_latency_ms = eval_ms;
        result.total_latency_ms = total_ms;
        result.response_text = json.value("response", "");
        return result;
    }

private:
    std::string exec(const std::string& cmd) {
        std::array<char, 256> buffer{};
        std::string result;

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("Failed to execute curl command");
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        return result;
    }
};