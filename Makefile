# AILLE Framework - Makefile
# One command to build everything

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3

# Paths & Common Sources
INCLUDES = -I. \
           -I./ailee_plugins \
           -I./extensions \
           -I./telemetry \
           -I./src \
           -I./examples

HTTPLIB_INCLUDES = -I./external
THREAD_FLAGS = -pthread

# Core Extension Sources
EXT_SRCS = extensions/aille_btc.cpp \
           extensions/aille_eth.cpp \
           extensions/aille_oil.cpp \
           extensions/aille_gold.cpp \
           extensions/aille_silver.cpp \
           extensions/aille_copper.cpp \
           extensions/aille_natgas.cpp \
           extensions/aille_platinum.cpp \
           extensions/aille_forex_usd.cpp \
           extensions/aille_macro.cpp \
           extensions/v7_2_pipeline.cpp \
           extensions/v7_3_pipeline.cpp \
           extensions/aille_spire.cpp \
           extensions/aille_lantern.cpp \
           extensions/aille_weathering.cpp

# Target Source Files
REST_API_SRC = examples/rest_api_server.cpp \
               aille_framework.cpp \
               aille_audit.cpp \
               $(EXT_SRCS)

# Build REST API server
rest_api_server: $(REST_API_SRC)
    $(CXX) $(CXXFLAGS) $(INCLUDES) $(HTTPLIB_INCLUDES) $(THREAD_FLAGS) \
        $(REST_API_SRC) -o rest_api_server

# Clean
clean:
    rm -f rest_api_server
