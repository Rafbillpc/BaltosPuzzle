#!/bin/sh
cmake -S . -B build/debug -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/release -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo
