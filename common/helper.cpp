#include "helper.h"

// helper function to write the rendered image as PPM file
void writePPM(const char *fileName,
              const ospcommon::vec2i *size,
              const uint32_t *pixel)
{
  FILE *file = fopen(fileName, "wb");
  if (!file) {
    fprintf(stderr, "fopen('%s', 'wb') failed: %d", fileName, errno);
    return;
  }
  fprintf(file, "P6\n%i %i\n255\n", size->x, size->y);
  unsigned char *out = (unsigned char *)alloca(3*size->x);
  for (int y = 0; y < size->y; y++) {
    const unsigned char *in = (const unsigned char *)&pixel[(size->y-1-y)*size->x];
    for (int x = 0; x < size->x; x++) {
      out[3*x + 0] = in[4*x + 0];
      out[3*x + 1] = in[4*x + 1];
      out[3*x + 2] = in[4*x + 2];
    }
    fwrite(out, 3*size->x, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}
