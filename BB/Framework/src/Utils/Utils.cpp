#include "Utils.h"

namespace BB
{
	void BB::Memory::MemCpy(void* __restrict  a_Destination, const void* __restrict  a_Source, size_t a_Size)
	{
		//Get the registry size for most optimal memcpy.
		uint8_t* __restrict  t_Dest = reinterpret_cast<uint8_t*>(a_Destination);
		const uint8_t* __restrict  t_Src = reinterpret_cast<const uint8_t*>(a_Source);

		//How many can we copy of 8 byte sized chunks?
		size_t t_Loopsize = (a_Size / sizeof(size_t));
		for (size_t i = 0; i < t_Loopsize; i++)
		{
			*reinterpret_cast<size_t*>(t_Dest) =
				*reinterpret_cast<const size_t*>(t_Src);

			t_Dest += sizeof(size_t);
			t_Src += sizeof(size_t);
		}

		//Again but then go by byte.
		t_Loopsize = (a_Size % sizeof(size_t));
		for (size_t i = a_Size - t_Loopsize; i < a_Size; i++)
		{
			*t_Dest = *t_Src;
			++t_Dest;
			++t_Src;
		}
	}
}