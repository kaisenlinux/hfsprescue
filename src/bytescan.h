#ifndef _BYTESCAN_H_
#define _BYTESCAN_H_

#include <vector>

void ByteScan (char *file, char *find_string, std::vector <char *> *file_names, int num_bytes, uint64_t offset);

#endif