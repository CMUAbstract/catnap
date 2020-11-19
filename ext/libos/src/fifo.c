#include <libos/fifo.h>

uint8_t is_fifo_empty(unsigned src, unsigned dst)
{
	//PRINTF("%u %u\r\n", src, dst);
	return src == dst;
}
