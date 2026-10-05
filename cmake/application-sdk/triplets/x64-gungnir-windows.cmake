set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
# Generated applications use /MD in both configurations. Ship the matching
# Release dependencies, without requiring a vcpkg toolchain in consumer apps.
set(VCPKG_BUILD_TYPE release)
