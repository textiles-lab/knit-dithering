# Co-Dithering for Jacquard Knitting

This repository contains our implementation of [*Co-Dithering for Jacquard Knitting*](https://textiles-lab.github.io/publications/2025-knit-dithering/). The latest version of these materials is available at https://github.com/textiles-lab/knit-dithering .

LICENSE: Copyright © 2025 James McCann (jmccann@cs.cmu.edu) and Yue Xu (yuex1@uw.edu). You are free to use this code for research, hobby, and other non-commercial purposes; as long as attribution is given. For a commercial use license, please contact the authors.

## Setup

The `knit-dither` utility can be created by compiling and linking all of the cpp files in the `src/` folder. We have included a `Makefile` which can be used on \*nix-ish systems to accomplish this task:

```
$ make
```

If you get errors about a missing `stb_image.h`, make sure to check out submodules (`git submodule update --init`).

---

The `knit-jacquard.js` script does not need to be compiled, but it does require the `pngjs` node module:
```
$ npm install pngjs
```

## Usage

The `knit-dither` utility "co-dithers" a pair of input images to produce a pair of quantized output images which obey constraints on the frequency of yarn use and bed crossings.

**Example** -- co-dither `example/hks-sky-M.png` and `example/hks-wave-M.png` using `5` colors selected from `example/yarn_measured_rayon_12.png`:
```
$ ./knit-dither --in-front example/hks-sky.png \
		--in-back example/hks-wave.png \
		--yarns example/yarn_measured_rayon_12.png \
		--select-yarns 5 \
		--out-front example/out-front.png \
		--out-back example/out-back.png
```

The operation of `knit-dither` is controlled by command-line arguments (use `--help` to have the program print this summary).

Input images: (use either `--in` or both of `--in-front` and `--in-back`)
  - `--in <in.png>` -- interleaved input image. Columns alternate front/back. Leftmost column is front. Width must be even.
  - `--in-front <in-front.png>` -- input front image. Must be the same size as the back image.
  - `--in-back <in-back.png>` -- input back image. Must be the same size as the front image.

Yarns: (at least `--yarns` is required)
  - `--yarns <yarns.png>` -- image containing one pixel per available yarn color.
  - `--select-yarns <Y>` -- ask the utility to heuristically select `Y` of the available yarns.

Output images: (specify at least one of these)
  - `--out <out.png>` -- interleaved output image. Columns alternate front/back. Leftmost column is front.
  - `--out-front <out-front.png>` -- front output image.
  - `--out-back <out-back.png>` -- back output image.

Dithering control: (optional)
  - `--use-within <U>` (integer >= 0, default 11, 0 disables) -- require every `U` stitches to contain at least one use of every yarn.
  - `--cross-within <X>` (integer >= 0, default 20, 0 disables) -- require every `X` stitches to contain at least one front and back use of the same yarn.
  - `--seed <S>` (integer >= 0, default 0, 0 always picks first, 1 always picks based on row) -- set the seed for the pseudo-random numbers used to pick between same-cost paths.
  - `--max-threads <T>` (integer >= 0, default 0, 0 picks automatically) -- limit the number of compute threads.
  - `--cost <srgb|linear|oklab|demo>` (default oklab) -- distance used to compute quantization cost.
  - `--method <optimal|greedy>` (default optimal) -- method used to [attempt to] optimize cost.
  - `--diffuse` / `--no-diffuse` (default is to diffuse) -- should quantization error be diffused to later rows.


*Note:* All input images should be in PNG format, and are assumed to be in the sRGB colorspace (usually true for png images).
(We used 8-bit, RGB PNG images for all testing. However, image loading is handled via [stb_image](https://github.com/nothings/stb) with `STB_ONLY_PNG` set, so you might be able to use [e.g.] indexed images or images with different color depths. They will be handled internally as 8-bit, however.)


### Output

To process the co-dithered output files into knitout, you can use the included `knit-jacquard.js` utility.

```
$ ./knit-jacquard.js example-front.png example-back.png --bindoff > example.k
```

Omit the flag `--bindoff` to skip the bindoff -- saves knitting time, but the result can unravel.

Note that this script uses an internal table to match up pixel colors with carrier indices; look for calls to `addCar`.

## Feedback

If you run into bugs, please file an [issue](https://github.com/textiles-lab/knit-dithering/issues).

If you make something cool, please send us an [e-mail](mailto:yuex1@uw.edu,jmccann@cs.cmu.edu?subject=Knit%20Co-Dithering).
