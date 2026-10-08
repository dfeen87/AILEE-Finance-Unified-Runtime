#!/usr/bin/env sh
# Build, install, relocate, and consume the actual public CMake package.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake_cmd=${CMAKE:-cmake}
scratch_dir=$(mktemp -d)
trap 'rm -rf "$scratch_dir"' EXIT HUP INT TERM
mkdir -p "$scratch_dir/consumer"
cat > "$scratch_dir/consumer/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.15)
project(ExternalConsumer LANGUAGES CXX)
find_package(AILLE 24.0.0 CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE AILLE::aille)
EOF
cat > "$scratch_dir/consumer/main.cpp" <<'EOF'
#include <aille.hpp>
#include <extensions/aille_volume_advisory.hpp>
#include <cmath>
int main() {
    AILLE::AILLEEngine engine;
    AILLE::VolumeState volume;
    AILLE::VolumeAdvisory volume_advisory;
    engine.set_volume_state(&volume);
    engine.set_volume_advisory(&volume_advisory);
    AILLE::ModelSignal signals[] = {{0.1f, 0.9f, 0}, {0.1f, 0.9f, 1}};
    const auto decision = engine.makeDecision(signals, 2);
    if (decision.status != AILLE::DECISION_VALID || !std::isfinite(decision.final_value)) return 1;
    if (!std::isfinite(volume_advisory.recommended_weight)) return 3;
    AILLE::AuditLogger audit;
    audit.logDecision(decision, "CONSUMER", "test");
    audit.logDecision(decision, "CONSUMER", "test");
    if (audit.getAuditTrailSize() != 2 || !audit.verifyIntegrity()) return 2;
    return 0;
}
EOF
for mode in Debug Release; do
    "$cmake_cmd" -S "$repo_dir" -B "$scratch_dir/build-$mode" \
        -DCMAKE_BUILD_TYPE="$mode" -DCMAKE_INSTALL_PREFIX="$scratch_dir/prefix-$mode"
    "$cmake_cmd" --build "$scratch_dir/build-$mode" --parallel 2
    "$cmake_cmd" --install "$scratch_dir/build-$mode"
    mv "$scratch_dir/prefix-$mode" "$scratch_dir/relocated-$mode"
    "$cmake_cmd" -S "$scratch_dir/consumer" -B "$scratch_dir/consumer-$mode" \
        -DCMAKE_BUILD_TYPE="$mode" -DCMAKE_PREFIX_PATH="$scratch_dir/relocated-$mode"
    "$cmake_cmd" --build "$scratch_dir/consumer-$mode" --parallel 2
    "$scratch_dir/consumer-$mode/consumer"
    printf 'CMake %s installed and relocated consumer passed\n' "$mode"
done
