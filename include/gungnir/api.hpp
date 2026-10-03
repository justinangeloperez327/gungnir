#pragma once

#if defined(__has_cpp_attribute)
#  if __has_cpp_attribute(deprecated)
#    define GUNGNIR_DEPRECATED(message) [[deprecated(message)]]
#  else
#    define GUNGNIR_DEPRECATED(message)
#  endif
#else
#  define GUNGNIR_DEPRECATED(message)
#endif

// GUNGNIR_API is the native binary visibility annotation.
//
// Shared-library builds currently preserve broad symbol visibility for the
// pre-1.0 ABI line. Public declarations can use this macro without baking
// platform-specific dllexport/visibility syntax into application code.
#if defined(_WIN32) && defined(GUNGNIR_SHARED)
#  if defined(GUNGNIR_BUILDING_LIBRARY)
#    define GUNGNIR_API __declspec(dllexport)
#  else
#    define GUNGNIR_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(GUNGNIR_SHARED)
#  define GUNGNIR_API __attribute__((visibility("default")))
#else
#  define GUNGNIR_API
#endif
