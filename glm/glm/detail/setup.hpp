/// @ref core
/// @file glm/detail/setup.hpp

#ifndef GLM_SETUP_INCLUDED

#include <cassert>
#include <cstddef>

#define GLM_VERSION_MAJOR			0
#define GLM_VERSION_MINOR			9
#define GLM_VERSION_PATCH			9
#define GLM_VERSION_REVISION		1
#define GLM_VERSION					991
#define GLM_VERSION_MESSAGE			"GLM: version 0.9.9.1"

#define GLM_SETUP_INCLUDED GLM_VERSION

///////////////////////////////////////////////////////////////////////////////////
// Active states

#define GLM_DISABLE		0
#define GLM_ENABLE		1

///////////////////////////////////////////////////////////////////////////////////
// Messages

#if defined(GLM_FORCE_MESSAGES)
#	define GLM_MESSAGES GLM_ENABLE
#else
#	define GLM_MESSAGES GLM_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Detect the platform

#include "../simd/platform.h"

///////////////////////////////////////////////////////////////////////////////////
// Build model

#if defined(__arch64__) || defined(__LP64__) || defined(_M_X64) || defined(__ppc64__) || defined(__x86_64__)
#	define GLM_MODEL	GLM_MODEL_64
#elif defined(__i386__) || defined(__ppc__)
#	define GLM_MODEL	GLM_MODEL_32
#else
#	define GLM_MODEL	GLM_MODEL_32
#endif//

#if !defined(GLM_MODEL) && GLM_COMPILER != 0
#	error "GLM_MODEL undefined, your compiler may not be supported by GLM. Add #define GLM_MODEL 0 to ignore this message."
#endif//GLM_MODEL

///////////////////////////////////////////////////////////////////////////////////
// C++ Version

// User defines: GLM_FORCE_CXX98, GLM_FORCE_CXX03, GLM_FORCE_CXX11, GLM_FORCE_CXX14, GLM_FORCE_CXX17, GLM_FORCE_CXX2A

#define GLM_LANG_CXX98_FLAG			(1 << 1)
#define GLM_LANG_CXX03_FLAG			(1 << 2)
#define GLM_LANG_CXX0X_FLAG			(1 << 3)
#define GLM_LANG_CXX11_FLAG			(1 << 4)
#define GLM_LANG_CXX14_FLAG			(1 << 5)
#define GLM_LANG_CXX17_FLAG			(1 << 6)
#define GLM_LANG_CXX2A_FLAG			(1 << 7)
#define GLM_LANG_CXXMS_FLAG			(1 << 8)
#define GLM_LANG_CXXGNU_FLAG		(1 << 9)

#define GLM_LANG_CXX98			GLM_LANG_CXX98_FLAG
#define GLM_LANG_CXX03			(GLM_LANG_CXX98 | GLM_LANG_CXX03_FLAG)
#define GLM_LANG_CXX0X			(GLM_LANG_CXX03 | GLM_LANG_CXX0X_FLAG)
#define GLM_LANG_CXX11			(GLM_LANG_CXX0X | GLM_LANG_CXX11_FLAG)
#define GLM_LANG_CXX14			(GLM_LANG_CXX11 | GLM_LANG_CXX14_FLAG)
#define GLM_LANG_CXX17			(GLM_LANG_CXX14 | GLM_LANG_CXX17_FLAG)
#define GLM_LANG_CXX2A			(GLM_LANG_CXX17 | GLM_LANG_CXX2A_FLAG)
#define GLM_LANG_CXXMS			GLM_LANG_CXXMS_FLAG
#define GLM_LANG_CXXGNU			GLM_LANG_CXXGNU_FLAG

#ifdef _MSC_EXTENSIONS
#	define GLM_LANG_EXT GLM_LANG_CXXMS_FLAG
#else
#	define GLM_LANG_EXT 0
#endif

#if defined(GLM_FORCE_CXX2A)
#	if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_MESSAGE_FORCE_CXX2A_DISPLAYED)
#		define GLM_MESSAGE_FORCE_CXX2A_DISPLAYED
#		pragma message("GLM: Force the use of C++2a only")
#	endif//GLM_MESSAGES
#	define GLM_LANG (GLM_LANG_CXX2A | GLM_LANG_EXT)
#	define GLM_LANG_STL11_FORCED
#elif defined(GLM_FORCE_CXX17)
#	if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_MESSAGE_FORCE_CXX17_DISPLAYED)
#		define GLM_MESSAGE_FORCE_CXX17_DISPLAYED
#		if (__cplusplus >= 201703L) || ((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC15_7)) || ((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC80)) || ((GLM_COMPILER & GLM_COMPILER_CLANG) && (GLM_COMPILER >= GLM_COMPILER_CLANG50))
#			pragma message("GLM: Force the use of C++17 only")
#		else
#			pragma message("GLM: Force the use of C++17 on a compiler that doesn't have full C++17 support")
#		endif
#	endif//GLM_MESSAGES
#	define GLM_LANG (GLM_LANG_CXX17 | GLM_LANG_EXT)
#	define GLM_LANG_STL11_FORCED
#elif defined(GLM_FORCE_CXX14)
#	if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_MESSAGE_FORCE_CXX14_DISPLAYED)
#		define GLM_MESSAGE_FORCE_CXX14_DISPLAYED
#		if (__cplusplus >= 201402L) || ((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC15)) || ((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC50)) || ((GLM_COMPILER & GLM_COMPILER_CLANG) && (GLM_COMPILER >= GLM_COMPILER_CLANG34))
#			pragma message("GLM: Force the use of C++14 only")
#		else
#			pragma message("GLM: Force the use of C++14 on a compiler that doesn't have full C++14 support")
#		endif
#	endif//GLM_MESSAGES
#	define GLM_LANG (GLM_LANG_CXX14 | GLM_LANG_EXT)
#	define GLM_LANG_STL11_FORCED
#elif defined(GLM_FORCE_CXX11)
#	if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_MESSAGE_FORCE_CXX11_DISPLAYED)
#		define GLM_MESSAGE_FORCE_CXX11_DISPLAYED
#		if (__cplusplus >= 201103L) || ((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC14)) || ((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC49)) || ((GLM_COMPILER & GLM_COMPILER_CLANG) && (GLM_COMPILER >= GLM_COMPILER_CLANG33))
#			pragma message("GLM: Force the use of C++11 only")
#		else
#			pragma message("GLM: Force the use of C++11 on a compiler that doesn't have full C++11 support")
#		endif
#	endif//GLM_MESSAGES
#	define GLM_LANG (GLM_LANG_CXX11 | GLM_LANG_EXT)
#	define GLM_LANG_STL11_FORCED
#elif defined(GLM_FORCE_CXX03)
#	define GLM_LANG GLM_LANG_CXX03
#elif defined(GLM_FORCE_CXX98)
#	define GLM_LANG GLM_LANG_CXX98
#else
#	if GLM_COMPILER & GLM_COMPILER_VC && defined(_MSVC_LANG)
#		if GLM_COMPILER >= GLM_COMPILER_VC15_7
#			define GLM_LANG_PLATFORM _MSVC_LANG
#		elif GLM_COMPILER >= GLM_COMPILER_VC15
#			if _MSVC_LANG > 201402L
#				define GLM_LANG_PLATFORM 201402L
#			else
#				define GLM_LANG_PLATFORM _MSVC_LANG
#			endif
#		else
#			define GLM_LANG_PLATFORM 0
#		endif
#	else
#		define GLM_LANG_PLATFORM 0
#	endif

#	if __cplusplus > 201703L || GLM_LANG_PLATFORM > 201703L
#		define GLM_LANG (GLM_LANG_CXX2A | GLM_LANG_EXT)
#	elif __cplusplus == 201703L || GLM_LANG_PLATFORM == 201703L
#		define GLM_LANG (GLM_LANG_CXX17 | GLM_LANG_EXT)
#	elif __cplusplus == 201402L || GLM_LANG_PLATFORM == 201402L
#		define GLM_LANG (GLM_LANG_CXX14 | GLM_LANG_EXT)
#	elif __cplusplus == 201103L || GLM_LANG_PLATFORM == 201103L
#		define GLM_LANG (GLM_LANG_CXX11 | GLM_LANG_EXT)
#	elif defined(__INTEL_CXX11_MODE__) || defined(_MSC_VER) || defined(__GXX_EXPERIMENTAL_CXX0X__)
#		define GLM_LANG (GLM_LANG_CXX0X | GLM_LANG_EXT)
#	elif __cplusplus == 199711L
#		define GLM_LANG (GLM_LANG_CXX98 | GLM_LANG_EXT)
#	else
#		define GLM_LANG (0 | GLM_LANG_EXT)
#	endif
#endif

///////////////////////////////////////////////////////////////////////////////////
// Has of C++ features

// http://clang.llvm.org/cxx_status.html
// http://gcc.gnu.org/projects/cxx0x.html
// http://msdn.microsoft.com/en-us/library/vstudio/hh567368(v=vs.120).aspx

// Android has multiple STLs but C++11 STL detection doesn't always work #284 #564
#if GLM_PLATFORM == GLM_PLATFORM_ANDROID && !defined(GLM_LANG_STL11_FORCED)
#	define GLM_HAS_CXX11_STL 0
#elif GLM_COMPILER & GLM_COMPILER_CLANG
#	if (defined(_LIBCPP_VERSION) && GLM_LANG & GLM_LANG_CXX11_FLAG) || defined(GLM_LANG_STL11_FORCED)
#		define GLM_HAS_CXX11_STL 1
#	else
#		define GLM_HAS_CXX11_STL 0
#	endif
#else
#	define GLM_HAS_CXX11_STL ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_PLATFORM != GLM_PLATFORM_WINDOWS) && (GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL15))))
#endif

// N1720
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_STATIC_ASSERT __has_feature(cxx_static_assert)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_STATIC_ASSERT 1
#else
#	define GLM_HAS_STATIC_ASSERT ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_CUDA)) || \
		((GLM_COMPILER & GLM_COMPILER_VC))))
#endif

// N1988
#if GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_EXTENDED_INTEGER_TYPE 1
#else
#	define GLM_HAS_EXTENDED_INTEGER_TYPE (\
		((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (GLM_COMPILER & GLM_COMPILER_VC)) || \
		((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (GLM_COMPILER & GLM_COMPILER_CUDA)) || \
		((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (GLM_COMPILER & GLM_COMPILER_CLANG)))
#endif

// N2672 Initializer lists http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2672.htm
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_INITIALIZER_LISTS __has_feature(cxx_generalized_initializers)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_INITIALIZER_LISTS 1
#else
#	define GLM_HAS_INITIALIZER_LISTS ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC15)) || \
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL14)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA75))))
#endif

// N2544 Unrestricted unions http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2544.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_UNRESTRICTED_UNIONS __has_feature(cxx_unrestricted_unions)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_UNRESTRICTED_UNIONS 1
#else
#	define GLM_HAS_UNRESTRICTED_UNIONS (GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		(GLM_COMPILER & GLM_COMPILER_VC) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA75)))
#endif

// N2346
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_DEFAULTED_FUNCTIONS __has_feature(cxx_defaulted_functions)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_DEFAULTED_FUNCTIONS 1
#else
#	define GLM_HAS_DEFAULTED_FUNCTIONS ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_INTEL)) || \
		(GLM_COMPILER & GLM_COMPILER_CUDA)))
#endif

// N2118
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_RVALUE_REFERENCES __has_feature(cxx_rvalue_references)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_RVALUE_REFERENCES 1
#else
#	define GLM_HAS_RVALUE_REFERENCES ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA))))
#endif

// N2437 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2437.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_EXPLICIT_CONVERSION_OPERATORS __has_feature(cxx_explicit_conversions)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_EXPLICIT_CONVERSION_OPERATORS 1
#else
#	define GLM_HAS_EXPLICIT_CONVERSION_OPERATORS ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL14)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA))))
#endif

// N2258 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2258.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_TEMPLATE_ALIASES __has_feature(cxx_alias_templates)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_TEMPLATE_ALIASES 1
#else
#	define GLM_HAS_TEMPLATE_ALIASES ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA))))
#endif

// N2930 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2930.html
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_RANGE_FOR __has_feature(cxx_range_for)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_RANGE_FOR 1
#else
#	define GLM_HAS_RANGE_FOR ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL)) || \
		((GLM_COMPILER & GLM_COMPILER_VC)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA))))
#endif

// N2341 http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2341.pdf
#if GLM_COMPILER & GLM_COMPILER_CLANG
#	define GLM_HAS_ALIGNOF __has_feature(c_alignof)
#elif GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_ALIGNOF 1
#else
#	define GLM_HAS_ALIGNOF ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL15)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC14)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA) && (GLM_COMPILER >= GLM_COMPILER_CUDA70))))
#endif

// N2235 Generalized Constant Expressions http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2235.pdf
// N3652 Extended Constant Expressions http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3652.html
#if (GLM_COMPILER & GLM_COMPILER_CLANG) && !(GLM_ARCH & GLM_ARCH_SIMD_BIT)
#	define GLM_HAS_CONSTEXPR __has_feature(cxx_relaxed_constexpr)
#elif (GLM_LANG & GLM_LANG_CXX14_FLAG) && !(GLM_ARCH & GLM_ARCH_SIMD_BIT)
#	define GLM_HAS_CONSTEXPR 1
#else
#	define GLM_HAS_CONSTEXPR ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && !(GLM_ARCH & GLM_ARCH_SIMD_BIT) && GLM_HAS_INITIALIZER_LISTS && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_COMPILER >= GLM_COMPILER_INTEL17)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC15))))
#endif

#if GLM_HAS_CONSTEXPR
#	define GLM_CONSTEXPR constexpr
#else
#	define GLM_CONSTEXPR
#endif

//
#if GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_ASSIGNABLE 1
#else
#	define GLM_HAS_ASSIGNABLE ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC15)) || \
		((GLM_COMPILER & GLM_COMPILER_GCC) && (GLM_COMPILER >= GLM_COMPILER_GCC49))))
#endif

//
#define GLM_HAS_TRIVIAL_QUERIES 0

//
#if GLM_LANG & GLM_LANG_CXX11_FLAG
#	define GLM_HAS_MAKE_SIGNED 1
#else
#	define GLM_HAS_MAKE_SIGNED ((GLM_LANG & GLM_LANG_CXX0X_FLAG) && (\
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC12)) || \
		((GLM_COMPILER & GLM_COMPILER_CUDA))))
#endif

#if defined(GLM_FORCE_PURE)
#	define GLM_HAS_BITSCAN_WINDOWS 0
#else
#	define GLM_HAS_BITSCAN_WINDOWS ((GLM_PLATFORM & GLM_PLATFORM_WINDOWS) && (\
		((GLM_COMPILER & GLM_COMPILER_INTEL)) || \
		((GLM_COMPILER & GLM_COMPILER_VC) && (GLM_COMPILER >= GLM_COMPILER_VC14) && (GLM_ARCH & GLM_ARCH_X86_BIT))))
#endif

///////////////////////////////////////////////////////////////////////////////////
// OpenMP
#ifdef _OPENMP
#	if GLM_COMPILER & GLM_COMPILER_GCC
#		if GLM_COMPILER >= GLM_COMPILER_GCC61
#			define GLM_HAS_OPENMP 45
#		elif GLM_COMPILER >= GLM_COMPILER_GCC49
#			define GLM_HAS_OPENMP 40
#		elif GLM_COMPILER >= GLM_COMPILER_GCC47
#			define GLM_HAS_OPENMP 31
#		else
#			define GLM_HAS_OPENMP 0
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_CLANG
#		if GLM_COMPILER >= GLM_COMPILER_CLANG38
#			define GLM_HAS_OPENMP 31
#		else
#			define GLM_HAS_OPENMP 0
#		endif
#	elif GLM_COMPILER & GLM_COMPILER_VC
#		define GLM_HAS_OPENMP 20
#	elif GLM_COMPILER & GLM_COMPILER_INTEL
#		if GLM_COMPILER >= GLM_COMPILER_INTEL16
#			define GLM_HAS_OPENMP 40
#		else
#			define GLM_HAS_OPENMP 0
#		endif
#	else
#		define GLM_HAS_OPENMP 0
#	endif
#else
#	define GLM_HAS_OPENMP 0
#endif

///////////////////////////////////////////////////////////////////////////////////
// nullptr

#if GLM_LANG & GLM_LANG_CXX0X_FLAG
#	define GLM_USE_NULLPTR GLM_ENABLE
#else
#	define GLM_USE_NULLPTR GLM_DISABLE
#endif

#if GLM_USE_NULLPTR == GLM_ENABLE
#	define GLM_NULLPTR nullptr
#else
#	define GLM_NULLPTR 0
#endif

///////////////////////////////////////////////////////////////////////////////////
// Static assert

#if GLM_HAS_STATIC_ASSERT
#	define GLM_STATIC_ASSERT(x, message) static_assert(x, message)
#elif GLM_COMPILER & GLM_COMPILER_VC
#	define GLM_STATIC_ASSERT(x, message) typedef char __CASSERT__##__LINE__[(x) ? 1 : -1]
#else
#	define GLM_STATIC_ASSERT(x, message)
#	define GLM_STATIC_ASSERT_NULL
#endif//GLM_LANG

///////////////////////////////////////////////////////////////////////////////////
// Qualifiers

#if GLM_COMPILER & GLM_COMPILER_CUDA
#	define GLM_CUDA_FUNC_DEF __device__ __host__
#	define GLM_CUDA_FUNC_DECL __device__ __host__
#else
#	define GLM_CUDA_FUNC_DEF
#	define GLM_CUDA_FUNC_DECL
#endif

#if defined(GLM_FORCE_INLINE)
#	if GLM_COMPILER & GLM_COMPILER_VC
#		define GLM_INLINE __forceinline
#		define GLM_NEVER_INLINE __declspec((noinline))
#	elif GLM_COMPILER & (GLM_COMPILER_GCC | GLM_COMPILER_CLANG)
#		define GLM_INLINE inline __attribute__((__always_inline__))
#		define GLM_NEVER_INLINE __attribute__((__noinline__))
#	elif GLM_COMPILER & GLM_COMPILER_CUDA
#		define GLM_INLINE __forceinline__
#		define GLM_NEVER_INLINE __noinline__
#	else
#		define GLM_INLINE inline
#		define GLM_NEVER_INLINE
#	endif//GLM_COMPILER
#else
#	define GLM_INLINE inline
#	define GLM_NEVER_INLINE
#endif//defined(GLM_FORCE_INLINE)

#define GLM_FUNC_DECL GLM_CUDA_FUNC_DECL
#define GLM_FUNC_QUALIFIER GLM_CUDA_FUNC_DEF GLM_INLINE

///////////////////////////////////////////////////////////////////////////////////
// Swizzle operators

// User defines: GLM_FORCE_SWIZZLE

#define GLM_SWIZZLE_OPERATOR 1
#define GLM_SWIZZLE_FUNCTION 2

#if defined(GLM_FORCE_SWIZZLE) && (GLM_LANG & GLM_LANG_CXXMS_FLAG)
#	define GLM_SWIZZLE GLM_SWIZZLE_OPERATOR
#elif defined(GLM_FORCE_SWIZZLE)
#	define GLM_SWIZZLE GLM_SWIZZLE_FUNCTION
#else
#	define GLM_SWIZZLE GLM_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Allows using not basic types as genType

// #define GLM_FORCE_UNRESTRICTED_GENTYPE

#ifdef GLM_FORCE_UNRESTRICTED_GENTYPE
#	define GLM_UNRESTRICTED_GENTYPE GLM_ENABLE
#else
#	define GLM_UNRESTRICTED_GENTYPE GLM_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Clip control, define GLM_FORCE_DEPTH_ZERO_TO_ONE before including GLM
// to use a clip space between 0 to 1.

#define GLM_DEPTH_ZERO_TO_ONE				0x00000001
#define GLM_DEPTH_NEGATIVE_ONE_TO_ONE		0x00000002

#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
#	define GLM_DEPTH_CLIP_SPACE GLM_DEPTH_ZERO_TO_ONE
#else
#	define GLM_DEPTH_CLIP_SPACE GLM_DEPTH_NEGATIVE_ONE_TO_ONE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Coordinate system, define GLM_FORCE_LEFT_HANDED before including GLM
// to use left handed coordinate system by default.

#define GLM_LEFT_HANDED				0x00000001	// For DirectX, Metal, Vulkan
#define GLM_RIGHT_HANDED			0x00000002	// For OpenGL, default in GLM

#ifdef GLM_FORCE_LEFT_HANDED
#	define GLM_COORDINATE_SYSTEM GLM_LEFT_HANDED
#else
#	define GLM_COORDINATE_SYSTEM GLM_RIGHT_HANDED
#endif

///////////////////////////////////////////////////////////////////////////////////
// Qualifiers

#if (GLM_COMPILER & GLM_COMPILER_VC) || ((GLM_COMPILER & GLM_COMPILER_INTEL) && (GLM_PLATFORM & GLM_PLATFORM_WINDOWS))
#	define GLM_DEPRECATED __declspec(deprecated)
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef __declspec(align(alignment)) type name
#elif GLM_COMPILER & (GLM_COMPILER_GCC | GLM_COMPILER_CLANG | GLM_COMPILER_INTEL)
#	define GLM_DEPRECATED __attribute__((__deprecated__))
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef type name __attribute__((aligned(alignment)))
#elif GLM_COMPILER & GLM_COMPILER_CUDA
#	define GLM_DEPRECATED
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef type name __align__(x)
#else
#	define GLM_DEPRECATED
#	define GLM_ALIGNED_TYPEDEF(type, name, alignment) typedef type name
#endif

///////////////////////////////////////////////////////////////////////////////////

#ifdef GLM_FORCE_EXPLICIT_CTOR
#	define GLM_EXPLICIT explicit
#else
#	define GLM_EXPLICIT
#endif

///////////////////////////////////////////////////////////////////////////////////
// Length type: all length functions returns a length_t type.
// When GLM_FORCE_SIZE_T_LENGTH is defined, length_t is a typedef of size_t otherwise
// length_t is a typedef of int like GLSL defines it.

// User define: GLM_FORCE_SIZE_T_LENGTH

namespace glm
{
	using std::size_t;
#	if defined(GLM_FORCE_SIZE_T_LENGTH)
		typedef size_t length_t;
#	else
		typedef int length_t;
#	endif
}//namespace glm

///////////////////////////////////////////////////////////////////////////////////
// countof

#if GLM_HAS_CONSTEXPR
	namespace glm
	{
		template<typename T, std::size_t N>
		constexpr std::size_t countof(T const (&)[N])
		{
			return N;
		}
	}//namespace glm
#	define GLM_COUNTOF(arr) glm::countof(arr)
#elif defined(_MSC_VER)
#	define GLM_COUNTOF(arr) _countof(arr)
#else
#	define GLM_COUNTOF(arr) sizeof(arr) / sizeof(arr[0])
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of defaulted initialized types

#define GLM_CTOR_INITIALIZER_LIST	(1 << 1)
#define GLM_CTOR_INITIALISATION		(1 << 2)

#if defined(GLM_FORCE_CTOR_INIT) && GLM_HAS_INITIALIZER_LISTS
#	define GLM_USE_CTOR_INIT GLM_CTOR_INITIALIZER_LIST
#elif defined(GLM_FORCE_CTOR_INIT) && !GLM_HAS_INITIALIZER_LISTS
#	define GLM_USE_CTOR_INIT GLM_CTOR_INITIALISATION
#else
#	define GLM_USE_CTOR_INIT GLM_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of defaulted function

#if GLM_HAS_DEFAULTED_FUNCTIONS && GLM_USE_CTOR_INIT == GLM_DISABLE
#	define GLM_USE_DEFAULTED_FUNCTIONS GLM_ENABLE
#	define GLM_DEFAULT = default
#else
#	define GLM_USE_DEFAULTED_FUNCTIONS GLM_DISABLE
#	define GLM_DEFAULT
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of aligned gentypes

#if defined(GLM_FORCE_ALIGNED_GENTYPES) && GLM_HAS_ALIGNOF && (GLM_LANG & GLM_LANG_CXXMS_FLAG)
#	define GLM_USE_ALIGNED_GENTYPES GLM_ENABLE
#else
#	define GLM_USE_ALIGNED_GENTYPES GLM_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Use SIMD instruction sets

#if (GLM_LANG & GLM_LANG_CXXMS_FLAG) && (GLM_ARCH & GLM_ARCH_SIMD_BIT)
#define GLM_USE_SIMD GLM_ENABLE
#else
#define GLM_USE_SIMD GLM_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Configure the use of anonymous structure as implementation detail

#if ((GLM_USE_SIMD == GLM_ENABLE) || (GLM_SWIZZLE == GLM_SWIZZLE_OPERATOR) || (GLM_USE_ALIGNED_GENTYPES == GLM_ENABLE))
#	define GLM_USE_ANONYMOUS_STRUCT GLM_ENABLE
#else
#	define GLM_USE_ANONYMOUS_STRUCT GLM_DISABLE
#endif

///////////////////////////////////////////////////////////////////////////////////
// Check inclusions of different versions of GLM

#elif ((GLM_SETUP_INCLUDED != GLM_VERSION) && !defined(GLM_FORCE_IGNORE_VERSION))
#	error "GLM error: A different version of GLM is already included. Define GLM_FORCE_IGNORE_VERSION before including GLM headers to ignore this error."
#elif GLM_SETUP_INCLUDED == GLM_VERSION

///////////////////////////////////////////////////////////////////////////////////
// Messages

#if GLM_MESSAGES == GLM_ENABLE && !defined(GLM_MESSAGE_DISPLAYED)
#	define GLM_MESSAGE_DISPLAYED

	// Report GLM version
#	pragma message (GLM_VERSION_MESSAGE)

	// Report C++ language
#	if GLM_LANG & GLM_LANG_CXX2A_FLAG
#		if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#			pragma message("GLM: C++ 2A with extensions")
#		else
#			pragma message("GLM: C++ 2A")
#		endif
#	elif GLM_LANG & GLM_LANG_CXX17_FLAG
#		if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#			pragma message("GLM: C++ 17 with extensions")
#		else
#			pragma message("GLM: C++ 17")
#		endif
#	elif GLM_LANG & GLM_LANG_CXX14_FLAG
#		if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#			pragma message("GLM: C++ 14 with extensions")
#		else
#			pragma message("GLM: C++ 14")
#		endif
#	elif GLM_LANG & GLM_LANG_CXX11_FLAG
#		if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#			pragma message("GLM: C++ 11 with extensions")
#		else
#			pragma message("GLM: C++ 11")
#		endif
#	elif GLM_LANG & GLM_LANG_CXX0X_FLAG
#		if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#			pragma message("GLM: C++ 0x with extensions")
#		else
#			pragma message("GLM: C++ 0x")
#		endif
#	elif GLM_LANG & GLM_LANG_CXX03_FLAG
#		if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#			pragma message("GLM: C++ 03 with extensions")
#		else
#			pragma message("GLM: C++ 03")
#		endif
#	elif GLM_LANG & GLM_LANG_CXX98_FLAG
#		if GLM_LANG & (GLM_LANG_CXXGNU_FLAG | GLM_LANG_CXXMS_FLAG)
#			pragma message("GLM: C++ 98 with extensions")
#		else
#			pragma message("GLM: C++ 98")
#		endif
#	else
#		pragma message("GLM: C++ language undetected")
#	endif//GLM_LANG

	// Report compiler detection
#	if GLM_COMPILER & GLM_COMPILER_CUDA
#		pragma message("GLM: CUDA compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_VC
#		pragma message("GLM: Visual C++ compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_CLANG
#		pragma message("GLM: Clang compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_INTEL
#		pragma message("GLM: Intel Compiler detected")
#	elif GLM_COMPILER & GLM_COMPILER_GCC
#		pragma message("GLM: GCC compiler detected")
#	else
#		pragma message("GLM: Compiler not detected")
#	endif

	// Report build target
#	if GLM_ARCH & GLM_ARCH_AVX2_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits with AVX2 instruction set build target")
#		else
#			pragma message("GLM: x86 32 bits with AVX2 instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_AVX_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits with AVX instruction set build target")
#		else
#			pragma message("GLM: x86 32 bits with AVX instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_SSE42_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits with SSE4.2 instruction set build target")
#		else
#			pragma message("GLM: x86 32 bits with SSE4.2 instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_SSE41_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits with SSE4.1 instruction set build target")
#		else
#			pragma message("GLM: x86 32 bits with SSE4.1 instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_SSSE3_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits with SSSE3 instruction set build target")
#		else
#			pragma message("GLM: x86 32 bits with SSSE3 instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_SSE3_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits with SSE3 instruction set build target")
#		else
#			pragma message("GLM: x86 32 bits with SSE3 instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_SSE2_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits with SSE2 instruction set build target")
#		else
#			pragma message("GLM: x86 32 bits with SSE2 instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_X86_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: x86 64 bits build target")
#		else
#			pragma message("GLM: x86 32 bits build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_NEON_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: ARM 64 bits with Neon instruction set build target")
#		else
#			pragma message("GLM: ARM 32 bits with Neon instruction set build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_ARM_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: ARM 64 bits build target")
#		else
#			pragma message("GLM: ARM 32 bits build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_MIPS_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: MIPS 64 bits build target")
#		else
#			pragma message("GLM: MIPS 32 bits build target")
#		endif
#	elif GLM_ARCH & GLM_ARCH_PPC_BIT
#		if GLM_MODEL == GLM_MODEL_64
#			pragma message("GLM: PowerPC 64 bits build target")
#		else
#			pragma message("GLM: PowerPC 32 bits build target")
#		endif
#	else
#		pragma message("GLM: Unknown build target")
#	endif//GLM_ARCH

	// Report platform name
#	if(GLM_PLATFORM & GLM_PLATFORM_QNXNTO)
#		pragma message("GLM: QNX platform detected")
//#	elif(GLM_PLATFORM & GLM_PLATFORM_IOS)
//#		pragma message("GLM: iOS platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_APPLE)
#		pragma message("GLM: Apple platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_WINCE)
#		pragma message("GLM: WinCE platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_WINDOWS)
#		pragma message("GLM: Windows platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_CHROME_NACL)
#		pragma message("GLM: Native Client detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_ANDROID)
#		pragma message("GLM: Android platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_LINUX)
#		pragma message("GLM: Linux platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_UNIX)
#		pragma message("GLM: UNIX platform detected")
#	elif(GLM_PLATFORM & GLM_PLATFORM_UNKNOWN)
#		pragma message("GLM: platform unknown")
#	else
#		pragma message("GLM: platform not detected")
#	endif

	// Report swizzle operator support
#	if GLM_SWIZZLE == GLM_SWIZZLE_OPERATOR
#		pragma message("GLM: GLM_FORCE_SWIZZLE is defined, swizzling operators enabled")
#	elif GLM_SWIZZLE == GLM_SWIZZLE_FUNCTION
#		pragma message("GLM: GLM_FORCE_SWIZZLE is defined, swizzling functions enabled. Enable compiler C++ language extensions to enable swizzle operators.")
#	else
#		pragma message("GLM: GLM_FORCE_SWIZZLE is undefined. swizzling functions or operators are disabled.")
#	endif

	// Report .length() type
#	if defined GLM_FORCE_SIZE_T_LENGTH
#		pragma message("GLM: GLM_FORCE_SIZE_T_LENGTH is defined. .length() returns a glm::length_t, a typedef of std::size_t instead of int.")
#	else
#		pragma message("GLM: GLM_FORCE_SIZE_T_LENGTH is undefined. .length() returns a glm::length_t, a typedef of int following the GLSL specification. Define GLM_FORCE_SIZE_T_LENGTH to make glm::length_t, a typedef of std::size_t.")
#	endif

#	ifdef GLM_FORCE_UNRESTRICTED_GENTYPE
#		pragma message("GLM: GLM_FORCE_UNRESTRICTED_GENTYPE is defined. Removes GLSL specification restrictions on valid function genTypes.")
#	else
#		pragma message("GLM: GLM_FORCE_UNRESTRICTED_GENTYPE is undefined. Follows strictly GLSL specification on valid function genTypes.")
#	endif

#	ifdef GLM_FORCE_SINGLE_ONLY
#		pragma message("GLM: GLM_FORCE_SINGLE_ONLY is defined. Using only single precision floating-point types")
#	endif

#	if GLM_DEPTH_CLIP_SPACE == GLM_DEPTH_ZERO_TO_ONE
#		pragma message("GLM: GLM_FORCE_DEPTH_ZERO_TO_ONE is defined. Using zero to one depth clip space.")
#	else
#		pragma message("GLM: GLM_FORCE_DEPTH_ZERO_TO_ONE is undefined. Using negative one to one depth clip space.")
#	endif

#	if GLM_COORDINATE_SYSTEM == GLM_LEFT_HANDED
#		pragma message("GLM: GLM_FORCE_LEFT_HANDED is defined. Using left handed coordinate system.")
#	else
#		pragma message("GLM: GLM_FORCE_LEFT_HANDED is undefined. Using right handed coordinate system.")
#	endif
#endif//GLM_MESSAGES

#endif//GLM_SETUP_INCLUDED
