/// @ref core
/// @file glm/detail/qualifier.hpp

#pragma once

#include "setup.hpp"
#include "type_int.hpp"

namespace glm
{
	/// Qualify GLM types in term of alignment (packed, aligned) and precision in term of ULPs (lowp, mediump, highp)
	enum qualifier
	{
		packed_highp, ///< Typed data is tightly packed in memory and operations are executed with high precision in term of ULPs
		packed_mediump, ///< Typed data is tightly packed in memory  and operations are executed with medium precision in term of ULPs for higher performance
		packed_lowp, ///< Typed data is tightly packed in memory  and operations are executed with low precision in term of ULPs to maximize performance

#		if GLM_USE_ALIGNED_GENTYPES == GLM_ENABLE
			aligned_highp, ///< Typed data is aligned in memory allowing SIMD optimizations and operations are executed with high precision in term of ULPs
			aligned_mediump, ///< Typed data is aligned in memory allowing SIMD optimizations and operations are executed with high precision in term of ULPs for higher performance
			aligned_lowp, // ///< Typed data is aligned in memory allowing SIMD optimizations and operations are executed with high precision in term of ULPs to maximize performance
			aligned = aligned_highp, ///< By default aligned qualifier is also high precision
#		endif

		highp = packed_highp, ///< By default highp qualifier is also packed
		mediump = packed_mediump, ///< By default mediump qualifier is also packed
		lowp = packed_lowp, ///< By default lowp qualifier is also packed
		packed = packed_highp, ///< By default packed qualifier is also high precision

#		if GLM_USE_ALIGNED_GENTYPES == GLM_ENABLE && defined(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
			defaultp = aligned_highp
#		else
			defaultp = highp
#		endif
	};

	typedef qualifier precision;

	template<length_t L, typename T, qualifier Q = defaultp> struct vec;
	template<length_t C, length_t R, typename T, qualifier Q = defaultp> struct mat;

namespace detail
{
	template<glm::qualifier P>
	struct is_aligned
	{
		static const bool value = false;
	};

#	if GLM_USE_ALIGNED_GENTYPES == GLM_ENABLE
		template<>
		struct is_aligned<glm::aligned_lowp>
		{
			static const bool value = true;
		};

		template<>
		struct is_aligned<glm::aligned_mediump>
		{
			static const bool value = true;
		};

		template<>
		struct is_aligned<glm::aligned_highp>
		{
			static const bool value = true;
		};
#	endif

	template<length_t L, typename T, bool is_aligned>
	struct storage
	{
		typedef struct type {
			T data[L];
		} type;
	};

#	if GLM_HAS_ALIGNOF
		template<length_t L, typename T>
		struct storage<L, T, true>
		{
			typedef struct alignas(L * sizeof(T)) type {
				T data[L];
			} type;
		};

		template<typename T>
		struct storage<3, T, true>
		{
			typedef struct alignas(4 * sizeof(T)) type {
				T data[4];
			} type;
		};
#	endif

#	if GLM_ARCH & GLM_ARCH_SSE2_BIT
	template<>
	struct storage<4, float, true>
	{
		typedef glm_f32vec4 type;
	};

	template<>
	struct storage<4, int32, true>
	{
		typedef glm_i32vec4 type;
	};

	template<>
	struct storage<4, uint32, true>
	{
		typedef glm_u32vec4 type;
	};

	template<>
	struct storage<2, double, true>
	{
		typedef glm_f64vec2 type;
	};

	template<>
	struct storage<2, int64, true>
	{
		typedef glm_i64vec2 type;
	};

	template<>
	struct storage<2, uint64, true>
	{
		typedef glm_u64vec2 type;
	};
#	endif

#	if (GLM_ARCH & GLM_ARCH_AVX_BIT)
	template<>
	struct storage<4, double, true>
	{
		typedef glm_f64vec4 type;
	};
#	endif

#	if (GLM_ARCH & GLM_ARCH_AVX2_BIT)
	template<>
	struct storage<4, int64, true>
	{
		typedef glm_i64vec4 type;
	};

	template<>
	struct storage<4, uint64, true>
	{
		typedef glm_u64vec4 type;
	};
#	endif

	enum genTypeEnum
	{
		GENTYPE_VEC,
		GENTYPE_MAT,
		GENTYPE_QUAT
	};

	template <typename genType>
	struct genTypeTrait
	{};

	template <length_t C, length_t R, typename T>
	struct genTypeTrait<mat<C, R, T> >
	{
		static const genTypeEnum GENTYPE = GENTYPE_MAT;
	};

	template<typename genType, genTypeEnum type>
	struct init_gentype
	{
	};

	template<typename genType>
	struct init_gentype<genType, GENTYPE_QUAT>
	{
		GLM_FUNC_QUALIFIER static genType identity()
		{
			return genType(1, 0, 0, 0);
		}
	};

	template<typename genType>
	struct init_gentype<genType, GENTYPE_MAT>
	{
		GLM_FUNC_QUALIFIER static genType identity()
		{
			return genType(1);
		}
	};
}//namespace detail
}//namespace glm
