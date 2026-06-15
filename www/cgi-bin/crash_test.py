#!/usr/bin/env python3
import ctypes

# Force a segfault
ctypes.string_at(0)