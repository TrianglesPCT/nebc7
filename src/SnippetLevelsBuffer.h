#pragma once

#include "pch.h"
#include "Bc7Core.h"

#if defined(OPTION_COUNTERS)
inline std::atomic_int gEstimateFull, gEstimateShort;

inline std::atomic_int gLevels[17];
#endif

template<int MaxSize>
class LevelsBuffer final
{
public:
	int MinErr = kBlockMaximalAlphaError + kBlockMaximalColorError;
	int Count = 0;

	Node Err[MaxSize];

	static ALWAYS_INLINED int Min(int x, int y) noexcept
	{
		return (x < y) ? x : y;
	}

	static ALWAYS_INLINED int Max(int x, int y) noexcept
	{
		return (x > y) ? x : y;
	}

	ALWAYS_INLINED LevelsBuffer() noexcept = default;

	ALWAYS_INLINED void SetZeroError(int color) noexcept
	{
		MinErr = 0;
		Count = 1;

		Err[0].Error = 0;
		Err[0].Color = color;
	}

	template<int bits, int pbits, bool transparent, const uint8_t table[0x100][1 << 2 * (bits + int(pbits >= 0))], bool single = false>
	NOTINLINED void ComputeChannelLevelsReduced(const Area& area, const size_t offset, const int weight, const int water) noexcept
	{
		const uint8_t* values[16];

		size_t count;
		if constexpr (transparent)
		{
			count = area.Active;

			if (!count)
			{
				if constexpr (pbits < 0)
				{
					SetZeroError(0);
				}
				else
				{
					SetZeroError(pbits << (7 - bits));
				}
				return;
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
		}

		int top = (water + weight - 1) / weight;
		if (!top)
			return;

		int d = _mm_cvtsi128_si32(_mm_cvttps_epi32(_mm_sqrt_ss(_mm_cvtepi32_ps(_mm_cvtsi32_si128(top)))));
		d -= int(d * d >= top);
		d <<= kDenoise;
		d += (1 << kDenoise) - 1;

		constexpr int tailmask = (1 << (8 - bits)) - 1;
		constexpr int shift = bits + int(pbits >= 0);

		int L = ((const short*)&area.MinMax_U16)[offset + offset + 0];
		int H = ((const short*)&area.MinMax_U16)[offset + offset + 1];

		const bool reverse = (L != ((const short*)&area.Bounds_U16)[offset + offset + 0]);

		if constexpr ((pbits < 0) || ((pbits >> 8) == (pbits & 1)))
		{
			if (L == H)
			{
				if ((((L >> shift) ^ L) & ((1 << (8 - shift)) - 1)) == 0)
				{
					if constexpr (pbits < 0)
					{
						SetZeroError((H << 8) + L);
						return;
					}
					else if (((L >> (8 - shift)) & 1) == (pbits & 1))
					{
						SetZeroError((H << 8) + L);
						return;
					}
				}
			}
		}

#if defined(OPTION_COUNTERS)
		gLevels[count]++;
#endif

		int LH = Min(L + d, 255);
		int HL = Max(H - d, 0);

		LH = Min(H - (H >> shift) + tailmask, LH - (LH >> shift)) & ~tailmask;
		HL = Max(L - (L >> shift), HL - (HL >> shift) + tailmask) & ~tailmask;

		int HH = 0x100;

		LH >>= 8 - shift;
		HL >>= 8 - shift;
		HH >>= 8 - shift;

		if (top > 0xFFFF)
		{
#if defined(OPTION_COUNTERS)
			gEstimateFull++;
#endif
			top = 0xFFFF;
		}
		else
		{
#if defined(OPTION_COUNTERS)
			gEstimateShort++;
#endif
		}

		NodeShort nodes1[(1 << shift) * (1 << shift)];
		NodeShort nodes2[(1 << shift) * (1 << shift)];
		NodeShort* nodesPtr = nodes1;

		const __m128i mtop = _mm_shuffle_epi32(_mm_shufflelo_epi16(_mm_cvtsi32_si128(top), 0), 0);

		if constexpr (pbits < 0)
		{
			for (int iH = HL; iH < HH; iH++)
			{
				int cH = (iH << shift);

				int hL = (single ? Min(LH, iH) : LH) + cH;
				Estimate32Step8Short(nodesPtr, values, count, cH, hL, mtop);
			}
		}
		else
		{
			const int pH = reverse ? (pbits & 1) : (pbits >> 8);
			const int pL = reverse ? (pbits >> 8) : (pbits & 1);

			HL += pH;

			for (int iH = HL; iH < HH; iH += 2)
			{
				int cH = (iH << shift);

				int hL = (single ? Min(LH, iH) : LH) + cH;
				Estimate16PStep8Short(nodesPtr, values, count, cH, hL, mtop, pL);
			}
		}

		NodeShort* sorted = nodes1;
		if (int nodesCount = int(nodesPtr - nodes1); nodesCount)
		{
			if constexpr (single)
			{
				(void)nodes2;

				uint32_t best = sorted[0].ColorError;
				for (int i = 1; i < nodesCount; i++)
				{
					uint32_t item = sorted[i].ColorError;
					if (best > item)
					{
						best = item;
					}
				}
				sorted[0].ColorError = best;

				MinErr = (best >> 16) * weight;
				Count = 1;
			}
			else
			{
				sorted = radix_sort(nodes1, nodes2, (uint32_t)nodesCount);

				MinErr = (sorted[0].ColorError >> 16) * weight;
				Count = Min(nodesCount, MaxSize);
			}
		}

		if (reverse)
		{
			for (int i = 0, n = Count; i < n; i++)
			{
				uint32_t ce = sorted[i].ColorError;
				int e = ce >> 16;
				int c = ce & 0xFFFFu;

				{
					int cL = (c >> shift) << (8 - shift);
					int cH = (c & ((1 << shift) - 1)) << (8 - shift);

					c = ((cH + (cH >> shift)) << 8) + cL + (cL >> shift);
				}

				Err[i].Error = e * weight;
				Err[i].Color = c;
			}
		}
		else
		{
			for (int i = 0, n = Count; i < n; i++)
			{
				uint32_t ce = sorted[i].ColorError;
				int e = ce >> 16;
				int c = ce & 0xFFFFu;

				if constexpr (shift != 8)
				{
					int cH = (c >> shift) << (8 - shift);
					int cL = (c & ((1 << shift) - 1)) << (8 - shift);

					c = ((cH + (cH >> shift)) << 8) + cL + (cL >> shift);
				}

				Err[i].Error = e * weight;
				Err[i].Color = c;
			}
		}
	}

private:
	template<int step, int shift>
	static ALWAYS_INLINED void Store8(NodeShort*& nodesPtr, __m128i msum, int flags, const int c) noexcept
	{
		for (int i = 0; i < 8 * step; i += step)
		{
			nodesPtr->Init(_mm_cvtsi128_si64(msum), c + i);
			nodesPtr += flags & 1;

			msum = _mm_alignr_epi8(msum, msum, 2);
			flags >>= shift;
		}
	}

#if defined(OPTION_AVX512)

	static INLINED void Estimate32Step8Short(NodeShort*& nodesPtr, const uint8_t* values[16], const size_t count, const int cH, int hL, const __m128i mtop) noexcept
	{
		const __m512i wwater = _mm512_broadcastw_epi16(mtop);

		int c = cH;
		do
		{
			__m512i wsum = _mm512_setzero_si512();
			int tail = static_cast<int>(~0u >> Max(0, (c | 31) - (hL | 7)));
			int flags = 0;

			for (size_t i = 0; i < count; i++)
			{
				auto value = values[i];

				const __m256i* p = (const __m256i*)&value[c];

				__m256i vdelta = _mm256_load_si256(p);

				__m512i wadd = _mm512_cvtepu8_epi16(vdelta);

				wadd = _mm512_mullo_epi16(wadd, wadd);

				wsum = _mm512_adds_epu16(wsum, wadd);

				flags = static_cast<int>(_mm512_cmp_epu16_mask(wwater, wsum, _MM_CMPINT_GT));
				if (!(flags &= tail))
					goto next;
			}

			Store8<1, 1>(nodesPtr, _mm512_castsi512_si128(wsum), flags, c);
			Store8<1, 1>(nodesPtr, _mm512_extracti32x4_epi32(wsum, 1), flags >> 8, c + 8);
			Store8<1, 1>(nodesPtr, _mm512_extracti32x4_epi32(wsum, 2), flags >> 16, c + 16);
			Store8<1, 1>(nodesPtr, _mm512_extracti32x4_epi32(wsum, 3), flags >> 24, c + 24);

		next:
			c += 32;

		} while (c <= hL);
	}

	static INLINED void Estimate16PStep8Short(NodeShort*& nodesPtr, const uint8_t* values[16], const size_t count, const int cH, int hL, const __m128i mtop, const int pL) noexcept
	{
		const __m256i vtop = _mm256_broadcastw_epi16(mtop);

		const __m256i vmask = pL ?
			_mm256_set_epi8(
				-0x80, 15, -0x80, 13, -0x80, 11, -0x80, 9, -0x80, 7, -0x80, 5, -0x80, 3, -0x80, 1,
				-0x80, 15, -0x80, 13, -0x80, 11, -0x80, 9, -0x80, 7, -0x80, 5, -0x80, 3, -0x80, 1) :
			_mm256_set_epi8(
				-0x80, 14, -0x80, 12, -0x80, 10, -0x80, 8, -0x80, 6, -0x80, 4, -0x80, 2, -0x80, 0,
				-0x80, 14, -0x80, 12, -0x80, 10, -0x80, 8, -0x80, 6, -0x80, 4, -0x80, 2, -0x80, 0);

		int c = cH;
		do
		{
			__m256i vsum = _mm256_setzero_si256();

			for (size_t i = 0; i < count; i++)
			{
				auto value = values[i];

				const __m256i* p = (const __m256i*)&value[c];

				__m256i vdelta = _mm256_load_si256(p);

				__m256i vadd = _mm256_shuffle_epi8(vdelta, vmask);

				vadd = _mm256_mullo_epi16(vadd, vadd);

				vsum = _mm256_adds_epu16(vsum, vadd);
			}

			int flags = _mm256_cmp_epu16_mask(vtop, vsum, _MM_CMPINT_GT);

			flags &= (0xFFFF >> (Max(0, (c | 31) - (hL | 7)) >> 1));

			if (flags & 0xFF)
			{
				Store8<2, 1>(nodesPtr, _mm256_castsi256_si128(vsum), flags, c + pL);
			}

			if (flags & 0xFF00)
			{
				Store8<2, 1>(nodesPtr, _mm256_extracti128_si256(vsum, 1), flags >> 8, c + pL + 16);
			}

			c += 32;

		} while (c <= hL);
	}

#elif defined(OPTION_AVX2)

	static INLINED void Estimate32Step8Short(NodeShort*& nodesPtr, const uint8_t* values[16], const size_t count, const int cH, int hL, const __m128i mtop) noexcept
	{
		const __m256i vsign = _mm256_set1_epi16(-0x8000);
		const __m256i vwater = _mm256_xor_si256(_mm256_broadcastw_epi16(mtop), vsign);

		int c = cH;
		do
		{
			__m256i vsum = _mm256_setzero_si256();
			int tail = static_cast<int>(~0u >> (Max(0, (c | 15) - (hL | 7)) << 1));
			int flags = 0;

			for (size_t i = 0; i < count; i++)
			{
				auto value = values[i];

				const __m128i* p = (const __m128i*)&value[c];

				__m128i mdelta = _mm_load_si128(p);

				__m256i vadd = _mm256_cvtepu8_epi16(mdelta);

				vadd = _mm256_mullo_epi16(vadd, vadd);

				vsum = _mm256_adds_epu16(vsum, vadd);

				flags = _mm256_movemask_epi8(_mm256_cmpgt_epi16(vwater, _mm256_xor_si256(vsum, vsign)));
				if (!(flags &= tail))
					goto next;
			}

			Store8<1, 2>(nodesPtr, _mm256_castsi256_si128(vsum), flags, c);
			Store8<1, 2>(nodesPtr, _mm256_extracti128_si256(vsum, 1), flags >> 16, c + 8);

		next:
			c += 16;

		} while (c <= hL);
	}

	static INLINED void Estimate16PStep8Short(NodeShort*& nodesPtr, const uint8_t* values[16], const size_t count, const int cH, int hL, const __m128i mtop, const int pL) noexcept
	{
		const __m256i vsign = _mm256_set1_epi16(-0x8000);
		const __m256i vwater = _mm256_xor_si256(_mm256_broadcastw_epi16(mtop), vsign);

		const __m256i vmask = pL ?
			_mm256_set_epi8(
				-0x80, 15, -0x80, 13, -0x80, 11, -0x80, 9, -0x80, 7, -0x80, 5, -0x80, 3, -0x80, 1,
				-0x80, 15, -0x80, 13, -0x80, 11, -0x80, 9, -0x80, 7, -0x80, 5, -0x80, 3, -0x80, 1) :
			_mm256_set_epi8(
				-0x80, 14, -0x80, 12, -0x80, 10, -0x80, 8, -0x80, 6, -0x80, 4, -0x80, 2, -0x80, 0,
				-0x80, 14, -0x80, 12, -0x80, 10, -0x80, 8, -0x80, 6, -0x80, 4, -0x80, 2, -0x80, 0);

		int c = cH;
		do
		{
			__m256i vsum = _mm256_setzero_si256();

			for (size_t i = 0; i < count; i++)
			{
				auto value = values[i];

				const __m256i* p = (const __m256i*)&value[c];

				__m256i vdelta = _mm256_load_si256(p);

				__m256i vadd = _mm256_shuffle_epi8(vdelta, vmask);

				vadd = _mm256_mullo_epi16(vadd, vadd);

				vsum = _mm256_adds_epu16(vsum, vadd);
			}

			int flags = _mm256_movemask_epi8(_mm256_cmpgt_epi16(vwater, _mm256_xor_si256(vsum, vsign)));

			flags &= static_cast<int>(~0u >> Max(0, (c | 31) - (hL | 7)));

			if (flags & 0xFFFF)
			{
				Store8<2, 2>(nodesPtr, _mm256_castsi256_si128(vsum), flags, c + pL);
			}

			if (flags & 0xFFFF0000)
			{
				Store8<2, 2>(nodesPtr, _mm256_extracti128_si256(vsum, 1), flags >> 16, c + pL + 16);
			}

			c += 32;

		} while (c <= hL);
	}

#else

	static INLINED void Estimate32Step8Short(NodeShort*& nodesPtr, const uint8_t* values[16], const size_t count, const int cH, int hL, const __m128i mtop) noexcept
	{
		const __m128i msign = _mm_set1_epi16(-0x8000);
		const __m128i mwater = _mm_xor_si128(mtop, msign);

		int c = cH;
		do
		{
			__m128i msum = _mm_setzero_si128();
			int flags = 0;

			for (size_t i = 0; i < count; i++)
			{
				auto value = values[i];

				const __m128i* p = (const __m128i*)&value[c];

				__m128i mdelta = _mm_loadl_epi64(p);

				__m128i madd = _mm_cvtepu8_epi16(mdelta);

				madd = _mm_mullo_epi16(madd, madd);

				msum = _mm_adds_epu16(msum, madd);

				flags = _mm_movemask_epi8(_mm_cmpgt_epi16(mwater, _mm_xor_si128(msum, msign)));
				if (!flags)
					goto next;
			}

			Store8<1, 2>(nodesPtr, msum, flags, c);

		next:
			c += 8;

		} while (c <= hL);
	}

	static ALWAYS_INLINED void Store4P(NodeShort*& nodesPtr, __m128i msum, int flags, const int c) noexcept
	{
		for (int i = 0; i < 4; i++)
		{
			nodesPtr->Init(_mm_cvtsi128_si64(msum), c + i + i);
			nodesPtr += flags & 1;

			msum = _mm_shuffle_epi32(msum, _MM_SHUFFLE(0, 3, 2, 1));
			flags >>= 4;
		}
	}

	static INLINED void Estimate16PStep8Short(NodeShort*& nodesPtr, const uint8_t* values[16], const size_t count, const int cH, int hL, const __m128i mtop, const int pL) noexcept
	{
		const __m128i mwater = _mm_cvtepu16_epi32(mtop);

		const __m128i mmask = pL ?
			_mm_set_epi8(
				-0x80, -0x80, -0x80, 7, -0x80, -0x80, -0x80, 5, -0x80, -0x80, -0x80, 3, -0x80, -0x80, -0x80, 1) :
			_mm_set_epi8(
				-0x80, -0x80, -0x80, 6, -0x80, -0x80, -0x80, 4, -0x80, -0x80, -0x80, 2, -0x80, -0x80, -0x80, 0);

		int c = cH;
		do
		{
			__m128i msum = _mm_setzero_si128();

			for (size_t i = 0; i < count; i++)
			{
				auto value = values[i];

				const __m128i* p = (const __m128i*)&value[c];

				__m128i mdelta = _mm_loadl_epi64(p);

				__m128i madd = _mm_shuffle_epi8(mdelta, mmask);

				madd = _mm_mullo_epi16(madd, madd);

				msum = _mm_add_epi32(msum, madd);
			}

			int flags = _mm_movemask_epi8(_mm_cmpgt_epi32(mwater, msum));

			if (flags)
			{
				Store4P(nodesPtr, msum, flags, c + pL);
			}

			c += 8;

		} while (c <= hL);
	}

#endif
};
