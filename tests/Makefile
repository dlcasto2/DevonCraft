# Host-side unit tests for DevonCraft's pure-logic code.
# Paths in HOST_SRCS are relative to the repository root.
CXX ?= g++
CXXFLAGS = -std=c++11 -fno-rtti -Wall -Wextra -g -I.. -I../nGL -I.
HOST_SRCS = gameclock.cpp survival.cpp items.cpp blockdefs.cpp playerinventory.cpp physics.cpp entity.cpp mobs.cpp aabb.cpp saveio.cpp blockstorage.cpp itempile.cpp gamestate.cpp savefile.cpp progression.cpp player_motion.cpp nGL/fastmath.cpp nGL/texturetools.cpp lighting.cpp stresstest.cpp screenmove.cpp daylight.cpp mining.cpp drops.cpp oregen.cpp containers.cpp crafting.cpp furnace.cpp water.cpp biome.cpp abyssalgen.cpp trees.cpp village.cpp perlinnoise.cpp graphics_preset.cpp trader.cpp
TEST_SRCS = $(wildcard test_*.cpp)
EXTRA_SRCS = ngl_stubs.cpp

run_tests: test_main.cpp $(TEST_SRCS) $(EXTRA_SRCS) $(addprefix ../,$(HOST_SRCS))
	$(CXX) $(CXXFLAGS) $^ -lz -lm -o $@

test: run_tests
	./run_tests

.PHONY: test
