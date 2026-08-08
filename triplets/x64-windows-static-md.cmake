# Overlay triplet: same settings as vcpkg's built-in community
# "x64-windows-static-md" triplet, plus one extra compile definition.
#
# Why this file exists:
# fmt 9.1.0 (pinned by this project's vcpkg baseline, as a transitive
# dependency of CommonLibSSE-NG) contains legacy code that references MSVC's
# `stdext::checked_array_iterator`, guarded behind `#if defined(_SECURE_SCL) && _SECURE_SCL`.
# Recent MSVC toolsets (Visual Studio 2022 17.8+ / Visual Studio 2026) removed
# that class from the STL entirely, so this branch fails to compile with:
#   error C2653: 'stdext': is not a class or namespace name
# `_SECURE_SCL` is still auto-defined truthy by MSVC's headers whenever
# `_ITERATOR_DEBUG_LEVEL > 0` (i.e. any Debug/`/MDd` build - and vcpkg builds
# BOTH debug and release variants of every dependency regardless of which
# CMake configuration your own project uses, so this bites Release builds too).
#
# Defining `_SECURE_SCL=0` on the command line short-circuits that dead
# branch (fmt's own maintainers confirm this workaround is safe - see
# https://github.com/fmtlib/fmt/issues/3540). It does not disable real
# iterator-debug-level checking, which modern MSVC implements separately.
#
# This only affects vcpkg's isolated builds of dependencies under this
# triplet - it has no effect on your own plugin code.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_C_FLAGS "${VCPKG_C_FLAGS} /D_SECURE_SCL=0")
set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} /D_SECURE_SCL=0")
