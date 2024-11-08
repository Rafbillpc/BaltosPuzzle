#pragma once
#include "header.hpp"

struct perm_t {
  i32 modified;
  i32 size;
  u8  data[8];
};

inline
vector<perm_t> perms;

void find_perms();

