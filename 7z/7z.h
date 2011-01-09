#ifndef __7Z_H
#define __7Z_H

/* #ifndef __cplusplus
extern "C++" {
#endif */

bool compress_deflate_7z(const unsigned char* in_data, unsigned in_size, unsigned char* out_data, unsigned& out_size, unsigned num_passes, unsigned num_fast_bytes) throw ();

/* #ifndef __cplusplus
}
#endif */

#endif

