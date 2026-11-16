// Decoder unit for the embedded flag pngs. SDL3_image in this build only carries
// jpeg, so png is decoded with the stb_image copy that already ships in
// dependencies/stb -- no extra dependency, lossless, and the pngs stay tiny.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>
