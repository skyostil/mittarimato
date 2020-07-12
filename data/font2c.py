from PIL import Image

f = open('font.sfl')

name = f.readline().strip()
dimensions = f.readline().strip()
png_name = f.readline().strip()
glyph_count = int(f.readline())

img = Image.open(png_name)

glyph_data = []
glyphs = []
first_glyph = None

for i in range(glyph_count):
  char, x, y, w, h, _, _, advance = f.readline().strip().split(' ')
  x = int(x)
  y = int(y)
  w = int(w)
  h = int(h)

  if first_glyph is None:
    first_glyph = char

  glyphs.append((w, h, len(glyph_data)))

  g = img.crop((x, y, w, h))
  for gy in range(h):
    bits = '0b'
    bit_count = 0
    for gx in range(w):
      p = img.getpixel((gx, gy))
      if p[3] >= 128:
        bits += '1'
      else:
        bits += '0'
      bit_count += 1
      if bit_count == 32:
        glyph_data.append(bits)
        bits = '0b'
        bit_count = 0
    if bit_count:
      if bit_count < 32:
        bits = '0b' + '0' * (32 - bit_count) + bits[2:]
      glyph_data.append(bits)

print(f'''
#pragma once

#include <esp_attr.h>

struct Glyph {{
 uint8_t width;
 uint8_t height;
 uint16_t offset;
}};
''')

print('constexpr uint32_t DRAM_ATTR glyph_data[] = {')
for g in glyph_data:
  print(f'  {g},')
print('};')

print(f'constexpr uint8_t first_glyph = {first_glyph};')
print('constexpr Glyph DRAM_ATTR glyphs[] = {')
for g in glyphs:
  print(f'  {{{g[0]}, {g[1]}, {g[2]}}},')
print('};')