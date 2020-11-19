#if 0
#include <libos/shared.h>
#include <libmsp/mem.h>

__nv uint8_t* backup_addr_src[BACKUP_SIZE];
__nv uint8_t* backup_addr_dst[BACKUP_SIZE];
__nv size_t backup_size[BACKUP_SIZE];
__nv unsigned backup_iter = 0;
__nv unsigned undo_counter = 0;

void restore_ulogs() {
	while (backup_iter) {
		unsigned iter = backup_iter - 1;
		memcpy(backup_addr_dst[iter], backup_addr_src[iter],
				backup_size[iter]);
		backup_iter = iter;
	}
}
#endif
