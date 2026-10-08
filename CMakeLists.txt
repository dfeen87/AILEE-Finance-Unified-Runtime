cmake_minimum_required(VERSION 3.15)
project(AILLE VERSION 24.0.0 LANGUAGES CXX)

# The public engine calls out-of-line advisory methods even when no advisory is
# bound. Export their implementations rather than an unusable header-only target.
add_library(aille STATIC
    aille_audit.cpp
    extensions/aille_btc.cpp
    extensions/aille_eth.cpp
    extensions/aille_oil.cpp
    extensions/aille_gold.cpp
    extensions/aille_silver.cpp
    extensions/aille_copper.cpp
    extensions/aille_natgas.cpp
    extensions/aille_platinum.cpp
    extensions/aille_forex_usd.cpp
    extensions/aille_volume_advisory.cpp
    extensions/aille_macro.cpp
    extensions/aille_stabilizer.cpp
    extensions/v7_execution_pipeline.cpp
    extensions/v7_2_pipeline.cpp
    extensions/v7_3_pipeline.cpp
    extensions/aille_spire.cpp
    extensions/aille_lantern.cpp
    extensions/aille_weathering.cpp
    extensions/aille_crown_walk.cpp
    extensions/aille_pilgrimage.cpp
    extensions/aille_arbitration.cpp
    extensions/aille_routing.cpp
    extensions/aille_governor_reconciliation.cpp
    extensions/aille_portfolio_constraints.cpp
    extensions/aille_temporal_consistency.cpp
    extensions/aille_stress_regime_override.cpp
    extensions/aille_meta_governance.cpp
    extensions/aille_membrane.cpp
    extensions/aille_anomaly.cpp
    extensions/aille_chart_intelligence.cpp
    extensions/aille_wnfs.cpp
    extensions/aille_unified_runtime.cpp
    extensions/aille_sync_adapter.cpp
)
add_library(AILLE::aille ALIAS aille)
target_compile_features(aille PUBLIC cxx_std_20)
target_include_directories(aille PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
find_package(Threads REQUIRED)
target_link_libraries(aille PUBLIC Threads::Threads)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS aille EXPORT AILLETargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES aille.hpp DESTINATION include)
install(DIRECTORY extensions/ DESTINATION include/extensions FILES_MATCHING PATTERN "*.hpp")
# Preserve relative includes used by the public advisory headers.
install(DIRECTORY include/ DESTINATION include/include FILES_MATCHING PATTERN "*.hpp")
install(EXPORT AILLETargets NAMESPACE AILLE:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AILLE)
configure_package_config_file(cmake/AILLEConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AILLEConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AILLE)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AILLEConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AILLEConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AILLEConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AILLE)
