cmake_minimum_required(VERSION 3.20)
project(ARHelmet VERSION 0.1.0 LANGUAGES CXX C)

# ─────────────────────────────────────────────────────────────────────────────
# Proxy raíz: delega la compilación a core_engine.
# Uso:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
# ─────────────────────────────────────────────────────────────────────────────
add_subdirectory(core_engine)
