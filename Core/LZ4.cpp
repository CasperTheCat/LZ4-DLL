#include "Core.h"
#include <iostream>

namespace LZ4
{
	//////////////////////////////////////////////////
	// Constructor
	//
	Processor::Processor()
	{
		this->dataArray = nullptr; /// This pointer lets me do more if I feel like it later!
		this->isInit = false;
		this->dataLength = 0;
	}

	//////////////////////////////////////////////////
	// Destructor
	//
	Processor::~Processor()
	{
		if (this->isInit) delete this->dataArray;
	}

	//////////////////////////////////////////////////
	// Compress LZ4 - from Yann Collet
	//
	int Processor::LZ4_compress_limitedOutput(const char* source, char* dest, int inputSize, int maxOutputSize)
	{
#if (HEAPMODE)
		void* ctx = ALLOCATOR(HASHNBCELLS4, 4);   /* Aligned on 4-bytes boundaries */
#else
		U32 ctx[1U << (MEMORY_USAGE - 2)] = { 0 };      /* Ensure data is aligned on 4-bytes boundaries */
#endif
		int result;

		if (inputSize < (int)LZ4_64KLIMIT)
			result = LZ4_compress_generic((void*)ctx, source, dest, inputSize, maxOutputSize, limited, byU16, noPrefix);
		else
			result = LZ4_compress_generic((void*)ctx, source, dest, inputSize, maxOutputSize, limited, (sizeof(void*) == 8) ? byU32 : byPtr, noPrefix);

#if (HEAPMODE)
		FREEMEM(ctx);
#endif
		return result;
	}

	//////////////////////////////////////////////////
	// Decompress LZ4 - from Yann Collet
	//
	int Processor::LZ4_decompress_safe(const char* source, char* dest, int inputSize, int maxOutputSize)
	{
		return LZ4_decompress_generic(source, dest, inputSize, maxOutputSize, endOnInputSize, noPrefix, full, 0);
	}

	//////////////////////////////////////////////////
	// Outer Facing Compression
	//
	int Processor::compress(const char* src, int inSize)
	{
		if(this->isInit) delete this->dataArray;
		this->isInit = true;

		/// Create Temporary locale for data
		this->dataArray = new char[inSize + sizeof(int)];

		/// Memcpy size to array
		memcpy(this->dataArray, &inSize, sizeof(int));

		/// Call Function - DO NOT EXPAND THE FILE
		this->dataLength = sizeof(int) + LZ4_compress_limitedOutput(src, this->dataArray + 4, inSize, inSize);
		
		/// Feedback
		return this->dataLength - sizeof(int);
	}

	//////////////////////////////////////////////////
	// Outer Facing Decompression
	//
	int Processor::decompress(char* src, int inSize)
	{
		if (this->isInit) delete this->dataArray;
		this->isInit = true;

		/// Get Array size
		int fSize;
		memcpy(&fSize,src,sizeof(int));
		std::cout << fSize << std::endl;

		/// Create decomp array
		this->dataArray = new char[fSize];

		/// Call Function
		this->dataLength = LZ4_decompress_safe(src + sizeof(int), this->dataArray, inSize - sizeof(int), fSize);

		/// Feedback
		return this->dataLength;
	}

	//////////////////////////////////////////////////
	// Flush Data Buffer
	//
	int Processor::flush()
	{
		if (!this->isInit) return 1;
		delete this->dataArray;
		this->dataLength = 0;
		return 0;
	}

	
	//////////////////////////////////////////////////
	// Data Pointer Accessor
	//
	char* Processor::ptr() const
	{
		/// Protect against nullptr
		return this->isInit ? this->dataArray : nullptr;
	}

	//////////////////////////////////////////////////
	// Data Length Accessor
	//
	long long Processor::len() const
	{
		return this->dataLength;
	}


	//////////////////////////////////////////////////
	// Generic LZ4 Decompress
	//
	int Processor::LZ4_decompress_generic(const char* source, char* dest, int inputSize, int outputSize, int endOnInput, int prefix64k, int partialDecoding, int targetOutputSize)
	{
		/* Local Variables */
		const BYTE* restrict ip = (const BYTE*)source;
		const BYTE* ref;
		const BYTE* const iend = ip + inputSize;

		BYTE* op = (BYTE*)dest;
		BYTE* const oend = op + outputSize;
		BYTE* cpy;
		BYTE* oexit = op + targetOutputSize;

		/*const size_t dec32table[] = {0, 3, 2, 3, 0, 0, 0, 0};   / static reduces speed for LZ4_decompress_safe() on GCC64 */
		const size_t dec32table[] = { 4 - 0, 4 - 3, 4 - 2, 4 - 3, 4 - 0, 4 - 0, 4 - 0, 4 - 0 };   /* static reduces speed for LZ4_decompress_safe() on GCC64 */
		static const size_t dec64table[] = { 0, 0, 0, (size_t)-1, 0, 1, 2, 3 };


		/* Special cases */
		if ((partialDecoding) && (oexit > oend - MFLIMIT)) oexit = oend - MFLIMIT;                        /* targetOutputSize too high => decode everything */
		if ((endOnInput) && (unlikely(outputSize == 0))) return ((inputSize == 1) && (*ip == 0)) ? 0 : -1;   /* Empty output buffer */
		if ((!endOnInput) && (unlikely(outputSize == 0))) return (*ip == 0 ? 1 : -1);


		/* Main Loop */
		while (1)
		{
			unsigned token;
			size_t length;

			/* get runlength */
			token = *ip++;
			if ((length = (token >> ML_BITS)) == RUN_MASK)
			{
				unsigned s = 255;
				while (((endOnInput) ? ip<iend : 1) && (s == 255))
				{
					s = *ip++;
					length += s;
				}
			}

			/* copy literals */
			cpy = op + length;
			if (((endOnInput) && ((cpy>(partialDecoding ? oexit : oend - MFLIMIT)) || (ip + length > iend - (2 + 1 + LASTLITERALS))))
				|| ((!endOnInput) && (cpy > oend - COPYLENGTH)))
			{
				if (partialDecoding)
				{
					if (cpy > oend) goto _output_error;                           /* Error : write attempt beyond end of output buffer */
					if ((endOnInput) && (ip + length > iend)) goto _output_error;   /* Error : read attempt beyond end of input buffer */
				}
				else
				{
					if ((!endOnInput) && (cpy != oend)) goto _output_error;       /* Error : block decoding must stop exactly there */
					if ((endOnInput) && ((ip + length != iend) || (cpy > oend))) goto _output_error;   /* Error : input must be consumed */
				}
				memcpy(op, ip, length);
				ip += length;
				op += length;
				break;                                       /* Necessarily EOF, due to parsing restrictions */
			}
			LZ4_WILDCOPY(op, ip, cpy); ip -= (op - cpy); op = cpy;

			/* get offset */
			LZ4_READ_LITTLEENDIAN_16(ref, cpy, ip); ip += 2;
			if ((prefix64k == noPrefix) && (unlikely(ref < (BYTE* const)dest))) goto _output_error;   /* Error : offset outside destination buffer */

																									  /* get matchlength */
			if ((length = (token&ML_MASK)) == ML_MASK)
			{
				while ((!endOnInput) || (ip < iend - (LASTLITERALS + 1)))   /* Ensure enough bytes remain for LASTLITERALS + token */
				{
					unsigned s = *ip++;
					length += s;
					if (s == 255) continue;
					break;
				}
			}

			/* copy repeated sequence */
			if (unlikely((op - ref) < (int)STEPSIZE))
			{
				const size_t dec64 = dec64table[(sizeof(void*) == 4) ? 0 : op - ref];
				op[0] = ref[0];
				op[1] = ref[1];
				op[2] = ref[2];
				op[3] = ref[3];
				/*op += 4, ref += 4; ref -= dec32table[op-ref];
				A32(op) = A32(ref);
				op += STEPSIZE-4; ref -= dec64;*/
				ref += dec32table[op - ref];
				A32(op + 4) = A32(ref);
				op += STEPSIZE; ref -= dec64;
			}
			else { LZ4_COPYSTEP(op, ref); }
			cpy = op + length - (STEPSIZE - 4);

			if (unlikely(cpy > oend - COPYLENGTH - (STEPSIZE - 4)))
			{
				if (cpy > oend - LASTLITERALS) goto _output_error;    /* Error : last 5 bytes must be literals */
				LZ4_SECURECOPY(op, ref, (oend - COPYLENGTH));
				while (op < cpy) *op++ = *ref++;
				op = cpy;
				continue;
			}
			LZ4_WILDCOPY(op, ref, cpy);
			op = cpy;   /* correction */
		}

		/* end of decoding */
		if (endOnInput)
			return (int)(((char*)op) - dest);     /* Nb of output bytes decoded */
		else
			return (int)(((char*)ip) - source);   /* Nb of input bytes read */

												  /* Overflow error detected */
	_output_error:
		return (int)(-(((char*)ip) - source)) - 1;
	}

	//////////////////////////////////////////////////
	// Generic LZ4 Compress
	//
	int Processor::LZ4_compress_generic(void* ctx, const char* source, char* dest, int inputSize, int maxOutputSize, limitedOutput_directive limitedOutput, tableType_t tableType, prefix64k_directive prefix)
	{
		const BYTE* ip = (const BYTE*)source;
		const BYTE* const base = (prefix == withPrefix) ? ((LZ4_Data_Structure*)ctx)->base : (const BYTE*)source;
		const BYTE* const lowLimit = ((prefix == withPrefix) ? ((LZ4_Data_Structure*)ctx)->bufferStart : (const BYTE*)source);
		const BYTE* anchor = (const BYTE*)source;
		const BYTE* const iend = ip + inputSize;
		const BYTE* const mflimit = iend - MFLIMIT;
		const BYTE* const matchlimit = iend - LASTLITERALS;

		BYTE* op = (BYTE*)dest;
		BYTE* const oend = op + maxOutputSize;

		int length;
		const int skipStrength = SKIPSTRENGTH;
		U32 forwardH;

		/* Init conditions */
		if ((U32)inputSize > (U32)LZ4_MAX_INPUT_SIZE) return 0;                                /* Unsupported input size, too large (or negative) */
		if ((prefix == withPrefix) && (ip != ((LZ4_Data_Structure*)ctx)->nextBlock)) return 0;   /* must continue from end of previous block */
		if (prefix == withPrefix) ((LZ4_Data_Structure*)ctx)->nextBlock = iend;                    /* do it now, due to potential early exit */
		if ((tableType == byU16) && (inputSize >= (int)LZ4_64KLIMIT)) return 0;                  /* Size too large (not within 64K limit) */
		if (inputSize < LZ4_minLength) goto _last_literals;                                      /* Input too small, no compression (all literals) */

																							   /* First Byte */
		LZ4_putPosition(ip, ctx, tableType, base);
		ip++; forwardH = LZ4_hashPosition(ip, tableType);

		/* Main Loop */
		for (; ; )
		{
			int findMatchAttempts = (1U << skipStrength) + 3;
			const BYTE* forwardIp = ip;
			const BYTE* ref;
			BYTE* token;

			/* Find a match */
			do
			{
				U32 h = forwardH;
				int step = findMatchAttempts++ >> skipStrength;
				ip = forwardIp;
				forwardIp = ip + step;

				if (unlikely(forwardIp > mflimit)) { goto _last_literals; }

				forwardH = LZ4_hashPosition(forwardIp, tableType);
				ref = LZ4_getPositionOnHash(h, ctx, tableType, base);
				LZ4_putPositionOnHash(ip, h, ctx, tableType, base);

			} while ((ref + MAX_DISTANCE < ip) || (A32(ref) != A32(ip)));

			/* Catch up */
			while ((ip > anchor) && (ref > lowLimit) && (unlikely(ip[-1] == ref[-1]))) { ip--; ref--; }

			/* Encode Literal length */
			length = (int)(ip - anchor);
			token = op++;
			if ((limitedOutput) && (unlikely(op + length + (2 + 1 + LASTLITERALS) + (length / 255) > oend))) return 0;   /* Check output limit */
			if (length >= (int)RUN_MASK)
			{
				int len = length - RUN_MASK;
				*token = (RUN_MASK << ML_BITS);
				for (; len >= 255; len -= 255) *op++ = 255;
				*op++ = (BYTE)len;
			}
			else *token = (BYTE)(length << ML_BITS);

			/* Copy Literals */
			{ BYTE* end = (op)+(length); LZ4_WILDCOPY(op, anchor, end); op = end; }

		_next_match:
			/* Encode Offset */
			LZ4_WRITE_LITTLEENDIAN_16(op, (U16)(ip - ref));

			/* Start Counting */
			ip += MINMATCH; ref += MINMATCH;    /* MinMatch already verified */
			anchor = ip;
			while (likely(ip < matchlimit - (STEPSIZE - 1)))
			{
				size_t diff = AARCH(ref) ^ AARCH(ip);
				if (!diff) { ip += STEPSIZE; ref += STEPSIZE; continue; }
				ip += LZ4_NbCommonBytes(diff);
				goto _endCount;
			}
			if (LZ4_ARCH64) if ((ip < (matchlimit - 3)) && (A32(ref) == A32(ip))) { ip += 4; ref += 4; }
			if ((ip < (matchlimit - 1)) && (A16(ref) == A16(ip))) { ip += 2; ref += 2; }
			if ((ip<matchlimit) && (*ref == *ip)) ip++;
		_endCount:

			/* Encode MatchLength */
			length = (int)(ip - anchor);
			if ((limitedOutput) && (unlikely(op + (1 + LASTLITERALS) + (length >> 8) > oend))) return 0;    /* Check output limit */
			if (length >= (int)ML_MASK)
			{
				*token += ML_MASK;
				length -= ML_MASK;
				for (; length > 509; length -= 510) { *op++ = 255; *op++ = 255; }
				if (length >= 255) { length -= 255; *op++ = 255; }
				*op++ = (BYTE)length;
			}
			else *token += (BYTE)(length);

			/* Test end of chunk */
			if (ip > mflimit) { anchor = ip;  break; }

			/* Fill table */
			LZ4_putPosition(ip - 2, ctx, tableType, base);

			/* Test next position */
			ref = LZ4_getPosition(ip, ctx, tableType, base);
			LZ4_putPosition(ip, ctx, tableType, base);
			if ((ref + MAX_DISTANCE >= ip) && (A32(ref) == A32(ip))) { token = op++; *token = 0; goto _next_match; }

			/* Prepare next loop */
			anchor = ip++;
			forwardH = LZ4_hashPosition(ip, tableType);
		}

	_last_literals:
		/* Encode Last Literals */
		{
			int lastRun = (int)(iend - anchor);
			if ((limitedOutput) && (((char*)op - dest) + lastRun + 1 + ((lastRun + 255 - RUN_MASK) / 255) > (U32)maxOutputSize)) return 0;   /* Check output limit */
			if (lastRun >= (int)RUN_MASK) { *op++ = (RUN_MASK << ML_BITS); lastRun -= RUN_MASK; for (; lastRun >= 255; lastRun -= 255) *op++ = 255; *op++ = (BYTE)lastRun; }
			else *op++ = (BYTE)(lastRun << ML_BITS);
			memcpy(op, anchor, iend - anchor);
			op += iend - anchor;
		}

		/* End */
		return (int)(((char*)op) - dest);
	}

	//////////////////////////////////////////////////
	// Put Position
	// 
	void Processor::LZ4_putPosition(const BYTE* p, void* tableBase, tableType_t tableType, const BYTE* srcBase)
	{
		U32 h = LZ4_hashPosition(p, tableType);
		LZ4_putPositionOnHash(p, h, tableBase, tableType, srcBase);
	}

	//////////////////////////////////////////////////
	// Put Pos on Hash
	void Processor::LZ4_putPositionOnHash(const BYTE* p, U32 h, void* tableBase, tableType_t tableType, const BYTE* srcBase)
	{
		switch (tableType)
		{
		case byPtr: { const BYTE** hashTable = (const BYTE**)tableBase; hashTable[h] = p; break; }
		case byU32: { U32* hashTable = (U32*)tableBase; hashTable[h] = (U32)(p - srcBase); break; }
		case byU16: { U16* hashTable = (U16*)tableBase; hashTable[h] = (U16)(p - srcBase); break; }
		}
	}

	//////////////////////////////////////////////////
	// Hash Seq
	//
	int Processor::LZ4_hashSequence(U32 sequence, tableType_t tableType)
	{
		if (tableType == byU16)
			return (((sequence)* 2654435761U) >> ((MINMATCH * 8) - (LZ4_HASHLOG + 1)));
		else
			return (((sequence)* 2654435761U) >> ((MINMATCH * 8) - LZ4_HASHLOG));
	}

	//////////////////////////////////////////////////
	// Hash Position
	//
	int Processor::LZ4_hashPosition(const BYTE* p, tableType_t tableType)
	{
		return LZ4_hashSequence(A32(p), tableType);
	}

	//////////////////////////////////////////////////
	// Get Pos on Hash
	//
	const BYTE* Processor::LZ4_getPositionOnHash(U32 h, void* tableBase, tableType_t tableType, const BYTE* srcBase)
	{
		if (tableType == byPtr) { const BYTE** hashTable = (const BYTE**)tableBase; return hashTable[h]; }
		if (tableType == byU32) { U32* hashTable = (U32*)tableBase; return hashTable[h] + srcBase; }
		{ U16* hashTable = (U16*)tableBase; return hashTable[h] + srcBase; }   /* default, to ensure a return */
	}

	//////////////////////////////////////////////////
	// Get Pos
	//
	const BYTE* Processor::LZ4_getPosition(const BYTE* p, void* tableBase, tableType_t tableType, const BYTE* srcBase)
	{
		U32 h = LZ4_hashPosition(p, tableType);
		return LZ4_getPositionOnHash(h, tableBase, tableType, srcBase);
	}
}
