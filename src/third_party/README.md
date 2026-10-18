# Third party code

- `Box2D/`: the physics engine of Erin Catto, version 2.3.1. GANGFORT
  uses the same version through libGDX. It comes from
  <https://github.com/erincatto/box2d>, the tag `v2.3.1`. The SHA-256 of
  the archive `v2.3.1.tar.gz` is
  `58ffc8475a8650aadc351345aef696937747b40501ab78d72c197c5ff5b3035c`.
  The licence is zlib, in `Box2D/License.txt`. The `Rope` directory and
  the CMake files are not here. Do not change the other files. Put each
  change in the code that uses them, as `src/physics/fo_phys.cpp`.
- `stb_vorbis.c`: the Ogg Vorbis decoder of Sean Barrett, version 1.22.
  It decodes the sounds of the APK on the phone, because iOS 6 has no
  Vorbis decoder. It is the same file as in Balatro4S, from
  <https://github.com/nothings/stb> at commit
  `1ee679ca2ef753a528db5ba6801e1067b40481b8`. The SHA-256 of the file is
  `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636`.
  The licence is MIT or public domain, as the end of the file tells. Do
  not change the file. Put each change in the code that uses it, as
  `src/core/fo_mixer.c`.
