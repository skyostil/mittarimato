from PIL import Image

files = [
  'sprites/sprite1.PNG',
  'sprites/sprite2.PNG',
  'sprites/sprite3.PNG',
  'sprites/sprite4.PNG',
  'sprites/sprite5.PNG',
]

sprite_data = []
sprites = []
palette = [
  0x1a1c2c,
  0x5d275d,
  0xb13e53,
  0xef7d57,
  0xffcd75,
  0xa7f070,
  0x38b764,
  0x257179,
  0x29366f,
  0x3b5dc9,
  0x41a6f6,
  0x73eff7,
  0x333c57,
  0x566c86,
  0x94b0c2,
  0xf4f4f4,
]

for fn in files:
  img = Image.open(fn)
  if img.size[0] % 2:
    img = img.crop((-1, 0, img.size[0] + 2, img.size[1]))
  sprites.append((img.size[0], img.size[1], len(sprite_data) // 2))

  for y in range(img.size[1]):
    for x in range(img.size[0]):
      p = img.getpixel((x, y))
      c = p[2] | (p[1] << 8) | (p[0] << 16)
      if p[3] == 0:
        i = 0
      else:
        i = palette.index(c)
      sprite_data.append(i)

print(f'''
#pragma once

#include <esp_attr.h>

struct Sprite {{
 uint8_t width;
 uint8_t height;
 uint16_t offset;
}};
''')

print('constexpr uint8_t DRAM_ATTR kSpriteData[] = {')
n = 0
for p1, p2 in zip(sprite_data[::2], sprite_data[1::2]):
  print('  0x%02x,' % (p1 | (p2 << 4)), end='')
  n += 1
  if n == 8:
    print()
    n = 0
print('};')

print('constexpr Sprite DRAM_ATTR kSprites[] = {')
for s in sprites:
  print(f'  {{{s[0]}, {s[1]}, {s[2]}}},')
print('};')
