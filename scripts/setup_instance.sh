#!/bin/sh
apt update
apt upgrade -y
apt install tmux htop clang task-spooler cmake ninja-build libomp-dev mold -y
