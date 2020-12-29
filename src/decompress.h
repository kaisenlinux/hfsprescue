#ifndef _DECOMPRESS_H_
#define _DECOMPRESS_H_

bool RestoreCompressedFile (int fd, uint64_t fork_size, uint32_t fork_block, uint32_t num_blocks, uint64_t offset, char *file_name, uint32_t file_number);

#endif