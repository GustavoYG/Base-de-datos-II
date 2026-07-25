#pragma once

#include <chrono>
#include <string>

class Catalog;

class Timer {
public:
    void Start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    double ElapsedMs() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

namespace benchmark {

// Ejecuta el benchmark dinamico de 10,000 registros comparando Full Scan vs B+ Tree Index
void RunIndexBenchmark(Catalog& catalog, int recordCount = 10000);

} // namespace benchmark
