#pragma once

#include "pch.h"
#include "Bc7Core.h"

namespace LevelsMinimum {

	static INLINED int Min(int x, int y) noexcept
	{
		return (x < y) ? x : y;
	}

	static INLINED int Max(int x, int y) noexcept
	{
		return (x > y) ? x : y;
	}

#if defined(OPTION_AVX512)

	static INLINED void Estimate32Short(__m512i& wbest, const uint16_t* values[16], const size_t count, const int c) noexcept
	{
		__m512i wsum = _mm512_setzero_si512();

		for (size_t i = 0; i < count; i++)
		{
			auto value = values[i];

			const __m512i* p = (const __m512i*)&value[c];

			__m512i wadd = _mm512_maskz_load_epi64(kFullMask8, p);

			wsum = _mm512_maskz_adds_epu16(kFullMask32, wsum, wadd);
		}

		wbest = _mm512_maskz_min_epu16(kFullMask32, wbest, wsum);
	}

	static INLINED void Estimate16Full(__m512i& wbest, const uint16_t* values[16], const size_t count, const int c) noexcept
	{
		__m512i wsum = _mm512_setzero_si512();

		for (size_t i = 0; i < count; i++)
		{
			auto value = values[i];

			const __m256i* p = (const __m256i*)&value[c];

			__m256i vadd = _mm256_load_si256(p);

			wsum = _mm512_maskz_add_epi32(kFullMask16, wsum, _mm512_maskz_cvtepu16_epi32(kFullMask16, vadd));
		}

		wbest = _mm512_maskz_min_epi32(kFullMask16, wbest, wsum);
	}

#elif defined(OPTION_AVX2)

	static INLINED void Estimate32Short(__m256i& vbest, const uint16_t* values[16], const size_t count, const int c) noexcept
	{
		__m256i vsum0 = _mm256_setzero_si256();
		__m256i vsum1 = _mm256_setzero_si256();

		for (size_t i = 0; i < count; i++)
		{
			auto value = values[i];

			const __m256i* p = (const __m256i*)&value[c];

			__m256i vadd0 = _mm256_load_si256(&p[0]);
			__m256i vadd1 = _mm256_load_si256(&p[1]);

			vsum0 = _mm256_adds_epu16(vsum0, vadd0);
			vsum1 = _mm256_adds_epu16(vsum1, vadd1);
		}

		vbest = _mm256_min_epu16(vbest, _mm256_min_epu16(vsum0, vsum1));
	}

	static INLINED void Estimate16Full(__m256i& vbest, const uint16_t* values[16], const size_t count, const int c) noexcept
	{
		__m256i vsum0 = _mm256_setzero_si256();
		__m256i vsum1 = _mm256_setzero_si256();

		__m256i vzero = _mm256_setzero_si256();

		for (size_t i = 0; i < count; i++)
		{
			auto value = values[i];

			const __m256i* p = (const __m256i*)&value[c];

			__m256i vadd0 = _mm256_load_si256(&p[0]);

			vsum0 = _mm256_add_epi32(vsum0, _mm256_unpacklo_epi16(vadd0, vzero));
			vsum1 = _mm256_add_epi32(vsum1, _mm256_unpackhi_epi16(vadd0, vzero));
		}

		vbest = _mm256_min_epi32(vbest, _mm256_min_epi32(vsum0, vsum1));
	}

#else

	static INLINED void Estimate32Short(__m128i& mbest, const uint16_t* values[16], const size_t count, const int c) noexcept
	{
		__m128i msum0 = _mm_setzero_si128();
		__m128i msum1 = _mm_setzero_si128();
		__m128i msum2 = _mm_setzero_si128();
		__m128i msum3 = _mm_setzero_si128();

		for (size_t i = 0; i < count; i++)
		{
			auto value = values[i];

			const __m128i* p = (const __m128i*)&value[c];

			__m128i madd0 = _mm_load_si128(&p[0]);
			__m128i madd1 = _mm_load_si128(&p[1]);
			__m128i madd2 = _mm_load_si128(&p[2]);
			__m128i madd3 = _mm_load_si128(&p[3]);

			msum0 = _mm_adds_epu16(msum0, madd0);
			msum1 = _mm_adds_epu16(msum1, madd1);
			msum2 = _mm_adds_epu16(msum2, madd2);
			msum3 = _mm_adds_epu16(msum3, madd3);
		}

		mbest = _mm_min_epu16(mbest, _mm_min_epu16(_mm_min_epu16(msum0, msum1), _mm_min_epu16(msum2, msum3)));
	}

	static INLINED void Estimate16Full(__m128i& mbest, const uint16_t* values[16], const size_t count, const int c) noexcept
	{
		__m128i msum0 = _mm_setzero_si128();
		__m128i msum1 = _mm_setzero_si128();
		__m128i msum2 = _mm_setzero_si128();
		__m128i msum3 = _mm_setzero_si128();

		__m128i mzero = _mm_setzero_si128();

		for (size_t i = 0; i < count; i++)
		{
			auto value = values[i];

			const __m128i* p = (const __m128i*)&value[c];

			__m128i madd0 = _mm_load_si128(&p[0]);
			__m128i madd1 = _mm_load_si128(&p[1]);

			msum0 = _mm_add_epi32(msum0, _mm_unpacklo_epi16(madd0, mzero));
			msum1 = _mm_add_epi32(msum1, _mm_unpackhi_epi16(madd0, mzero));
			msum2 = _mm_add_epi32(msum2, _mm_unpacklo_epi16(madd1, mzero));
			msum3 = _mm_add_epi32(msum3, _mm_unpackhi_epi16(madd1, mzero));
		}

		mbest = _mm_min_epi32(mbest, _mm_min_epi32(_mm_min_epi32(msum0, msum1), _mm_min_epi32(msum2, msum3)));
	}

#endif

	template<int bits, bool transparent, const uint16_t table[0x100][(1 << bits) * (1 << bits)], const uint16_t tower[0x100][1 << bits]>
	NOTINLINED int EstimateChannelLevelsReduced(const Area& area, const size_t offset, const int weight, const int water) noexcept
	{
		const uint16_t* values[16];
		const uint16_t* cuts[16];

		size_t count;
		if constexpr (transparent)
		{
			count = area.Active;

			if (!count)
			{
				return 0;
			}
		}
		else
		{
			count = area.Count;
		}

		for (size_t i = 0; i < count; i++)
		{
			size_t value = ((const uint16_t*)&area.DataMask_I16[i])[offset];

			values[i] = table[value];
			cuts[i] = tower[value];
		}

		int top = (water + weight - 1) / weight;
		if (!top)
			return 0;

		int d = (int)sqrt(top);
		d -= int(d * d >= top);

		constexpr int tailmask = (1 << (8 - bits)) - 1;
		constexpr int shift = bits;

		int L = ((const short*)&area.MinMax_U16)[offset + offset + 0];
		int H = ((const short*)&area.MinMax_U16)[offset + offset + 1];

		const bool reverse = (L != ((const short*)&area.Bounds_U16)[offset + offset + 0]);

		if (L == H)
		{
			// Residual is always (0 or 1) * count
			return 0;
		}

		int LH = Min(L + d, 255);
		int HL = Max(H - d, 0);

		LH = Min(H - (H >> shift) + tailmask, LH - (LH >> shift)) & ~tailmask;
		HL = Max(L - (L >> shift), HL - (HL >> shift) + tailmask) & ~tailmask;

		int HH = 0x100;

		LH >>= 8 - shift;
		HL >>= 8 - shift;
		HH >>= 8 - shift;

		int best = top;

		if (top <= 0xFFFF)
		{
			alignas(64) uint16_t rows[1 << bits];

#if defined(OPTION_AVX512)
			for (int iH = HL & ~31; iH < HH; iH += 32)
			{
				__m512i wcut = _mm512_setzero_si512();

				for (size_t i = 0; i < count; i++)
				{
					auto value = cuts[i];

					const __m512i* p = (const __m512i*)&value[iH];

					__m512i wadd = _mm512_maskz_load_epi64(kFullMask8, p);

					wcut = _mm512_maskz_adds_epu16(kFullMask32, wcut, wadd);
				}

				_mm512_mask_store_epi64((__m512i*)&rows[iH], kFullMask8, wcut);
			}
#elif defined(OPTION_AVX2)
			for (int iH = HL & ~15; iH < HH; iH += 16)
			{
				__m256i vcut = _mm256_setzero_si256();

				for (size_t i = 0; i < count; i++)
				{
					auto value = cuts[i];

					const __m256i* p = (const __m256i*)&value[iH];

					__m256i vadd = _mm256_load_si256(p);

					vcut = _mm256_adds_epu16(vcut, vadd);
				}

				_mm256_store_si256((__m256i*)&rows[iH], vcut);
			}
#else
			for (int iH = HL & ~7; iH < HH; iH += 8)
			{
				__m128i mcut = _mm_setzero_si128();

				for (size_t i = 0; i < count; i++)
				{
					auto value = cuts[i];

					const __m128i* p = (const __m128i*)&value[iH];

					__m128i madd = _mm_load_si128(p);

					mcut = _mm_adds_epu16(mcut, madd);
				}

				_mm_store_si128((__m128i*)&rows[iH], mcut);
			}
#endif

			for (int iH = HL; iH < HH; iH++)
			{
				if (rows[iH] >= best)
					continue;

#if defined(OPTION_AVX512)
				__m512i wbest = _mm512_maskz_set1_epi16(kFullMask32, -1);
#elif defined(OPTION_AVX2)
				__m256i vbest = _mm256_set1_epi16(-1);
#else
				__m128i mbest = _mm_set1_epi16(-1);
#endif

				int cH = (iH << shift);

				for (int iL = cH, hL = Min(LH, iH) + cH; iL <= hL; iL += 32)
				{
#if defined(OPTION_AVX512)
					Estimate32Short(wbest, values, count, iL);
#elif defined(OPTION_AVX2)
					Estimate32Short(vbest, values, count, iL);
#else
					Estimate32Short(mbest, values, count, iL);
#endif
				}

#if defined(OPTION_AVX512)
				__m256i vbest = _mm256_min_epu16(_mm512_maskz_extracti64x4_epi64(kFullMask8, wbest, 1), _mm512_castsi512_si256(wbest));
				__m128i mbest = _mm_min_epu16(_mm256_extracti128_si256(vbest, 1), _mm256_castsi256_si128(vbest));
#elif defined(OPTION_AVX2)
				__m128i mbest = _mm_min_epu16(_mm256_extracti128_si256(vbest, 1), _mm256_castsi256_si128(vbest));
#endif

				best = Min(best, _mm_extract_epi16(_mm_minpos_epu16(mbest), 0));
			}
		}
		else
		{
			alignas(64) int rows[1 << bits];

			__m128i mzero = _mm_setzero_si128();

#if defined(OPTION_AVX512)
			for (int iH = HL & ~15; iH < HH; iH += 16)
			{
				__m512i wcut = _mm512_setzero_si512();

				for (size_t i = 0; i < count; i++)
				{
					auto value = cuts[i];

					const __m256i* p = (const __m256i*)&value[iH];

					__m256i vadd = _mm256_load_si256(p);

					wcut = _mm512_maskz_add_epi32(kFullMask16, wcut, _mm512_maskz_cvtepu16_epi32(kFullMask16, vadd));
				}

				_mm512_mask_store_epi64((__m512i*)&rows[iH], kFullMask8, wcut);
			}
#elif defined(OPTION_AVX2)
			for (int iH = HL & ~7; iH < HH; iH += 8)
			{
				__m256i vcut = _mm256_setzero_si256();

				for (size_t i = 0; i < count; i++)
				{
					auto value = cuts[i];

					const __m128i* p = (const __m128i*)&value[iH];

					__m128i madd = _mm_load_si128(p);

					vcut = _mm256_add_epi32(vcut, _mm256_cvtepu16_epi32(madd));
				}

				_mm256_store_si256((__m256i*)&rows[iH], vcut);
			}
#else
			for (int iH = HL & ~7; iH < HH; iH += 8)
			{
				__m128i mcut0 = _mm_setzero_si128();
				__m128i mcut1 = _mm_setzero_si128();

				for (size_t i = 0; i < count; i++)
				{
					auto value = cuts[i];

					const __m128i* p = (const __m128i*)&value[iH];

					__m128i madd = _mm_load_si128(p);

					mcut0 = _mm_add_epi32(mcut0, _mm_unpacklo_epi16(madd, mzero));
					mcut1 = _mm_add_epi32(mcut1, _mm_unpackhi_epi16(madd, mzero));
				}

				_mm_store_si128((__m128i*)&rows[iH + 0], mcut0);
				_mm_store_si128((__m128i*)&rows[iH + 4], mcut1);
			}
#endif

			for (int iH = HL; iH < HH; iH++)
			{
				if (rows[iH] >= best)
					continue;

#if defined(OPTION_AVX512)
				__m512i wbest = _mm512_maskz_set1_epi32(kFullMask16, kBlockMaximalAlphaError + kBlockMaximalColorError);
#elif defined(OPTION_AVX2)
				__m256i vbest = _mm256_set1_epi32(kBlockMaximalAlphaError + kBlockMaximalColorError);
#else
				__m128i mbest = _mm_set1_epi32(kBlockMaximalAlphaError + kBlockMaximalColorError);
#endif

				int cH = (iH << shift);

				for (int iL = cH, hL = Min(LH, iH) + cH; iL <= hL; iL += 16)
				{
#if defined(OPTION_AVX512)
					Estimate16Full(wbest, values, count, iL);
#elif defined(OPTION_AVX2)
					Estimate16Full(vbest, values, count, iL);
#else
					Estimate16Full(mbest, values, count, iL);
#endif
				}

#if defined(OPTION_AVX512)
				__m256i vbest = _mm256_min_epi32(_mm512_maskz_extracti64x4_epi64(kFullMask8, wbest, 1), _mm512_castsi512_si256(wbest));
				__m128i mbest = _mm_min_epi32(_mm256_extracti128_si256(vbest, 1), _mm256_castsi256_si128(vbest));
#elif defined(OPTION_AVX2)
				__m128i mbest = _mm_min_epi32(_mm256_extracti128_si256(vbest, 1), _mm256_castsi256_si128(vbest));
#endif

				mbest = _mm_min_epi32(mbest, _mm_shuffle_epi32(mbest, _MM_SHUFFLE(2, 3, 0, 1)));
				mbest = _mm_min_epi32(mbest, _mm_shuffle_epi32(mbest, _MM_SHUFFLE(1, 0, 3, 2)));

				best = Min(best, (int)_mm_cvtsi128_si64(mbest));
			}
		}

		return best * weight;
	}

} // namespace LevelsMinimum