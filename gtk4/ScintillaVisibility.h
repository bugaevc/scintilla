#pragma once

/* See gtk:build-aux/meson/gen-visibility-macros.py */

#if (defined(_WIN32) || defined(__CYGWIN__))
#  define _SCINTILLA_EXPORT __declspec(dllexport)
#  define _SCINTILLA_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
#  define _SCINTILLA_EXPORT __attribute__((visibility("default")))
#  define _SCINTILLA_IMPORT __attribute__((visibility("default")))
#else
#  define _SCINTILLA_EXPORT
#  define _SCINTILLA_IMPORT
#endif

#ifdef SCINTILLA_COMPILATION
#  define _SCINTILLA_API _SCINTILLA_EXPORT
#else
#  define _SCINTILLA_API _SCINTILLA_IMPORT
#endif

#define _SCINTILLA_EXTERN _SCINTILLA_API extern

#define SCINTILLA_AVAILABLE_IN_ALL _SCINTILLA_EXTERN

#if defined(__cplusplus)
#define SCINTILLA_NOEXCEPT noexcept
#elif defined(__GNUC__)
#define SCINTILLA_NOEXCEPT __attribute__((nothrow))
#elif defined(_MSC_VER)
#define SCINTILLA_NOEXCEPT __declspec(nothrow)
#else
#define SCINTILLA_NOEXCEPT
#endif
