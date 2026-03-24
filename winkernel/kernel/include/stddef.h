#pragma once

#include <stdint.h>

typedef uint64_t    size_t;
typedef int64_t     ptrdiff_t;

#define NULL        ((void*)0)
#define offsetof(type, member) __builtin_offsetof(type, member)
