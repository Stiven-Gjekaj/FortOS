# Third party code

- `Box2D/`: the physics engine of Erin Catto, version 2.3.1. GANGFORT
  uses the same version through libGDX. It comes from
  <https://github.com/erincatto/box2d>, the tag `v2.3.1`. The SHA-256 of
  the archive `v2.3.1.tar.gz` is
  `58ffc8475a8650aadc351345aef696937747b40501ab78d72c197c5ff5b3035c`.
  The licence is zlib, in `Box2D/License.txt`. The `Rope` directory and
  the CMake files are not here. Do not change the other files. Put each
  change in the code that uses them, as `src/physics/fo_phys.cpp`.
