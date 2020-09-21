
# Thumbnailer

Thumbnailer takes timestamped images as input and produces a WebP animation that fits a given size constraint.

## Building

#### Prerequisites

- [Bazel](https://docs.bazel.build/versions/master/install.html)
- [Clang](https://clang.llvm.org/get_started.html)

#### Build command

```
bazel build src:thumbnailer && bazel build src/utils:thumbnailer_compare
```

## Usage

### Thumbnailer

This tool reads a sequence of images and timestamps, and produces an animation in WebP format. By default, `thumbnailer` uses lossy compression and imposes the same quality factor to all frames.

##### Run the binary directly.

```
./bazel-bin/src/thumbnailer [options] frame_list.txt -o=output.webp
```

##### Alternatively, build and run with Bazel.

```
bazel run src:thumbnailer [options] frame_list.txt -o=output.webp
```

**`frame_list.txt`** is a text file containing the description for each frame. Each line contains the frame's filename and its ending timestamp in millisecond, in the following format: `filename timestamp`. For example:

``` 
/anim1/frame01.png 150
/anim1/frame02.png 300
/anim1/frame03.png 450
/anim1/frame04.png 600
```

#### Options:

| Option | Default Value | Description|
|--------|:-------------:|------------|
|`-soft_max_size`|153600|Desired (soft) maximum size limit (in bytes).|
|`-hard_max_size`|153600|Hard limit for maximum file size (in bytes).|
|`-loop_count`|0 (infinite loop)|Number of times the animation will loop.|
|`-min_lossy_quality`|0|Minimum lossy quality (0..100) to be used for encoding each frame.|
|`-m`|4|Effort/speed trade-off (0=fast, 6=slower-better). Similar to `cwebp -m`.|
|`-allow_mixed`|false|Use mixed lossy/lossless compression.|
|`-algorithm`|equal_quality|Algorithm to generate animation {equal_quality, equal_psnr, near_ll_diff, near_ll_equal, slope_optim}.|
|`-slope_dpsnr`|1.0|Maximum PSNR change (in dB) used in slope optimization.|
|`-verbose`|false|Print various encoding statistics.|

#### `-algorithm` flag description:

| | Algorithm | Description |
|-|-----------|-------------|
|0|`equal_quality`|Generate animation setting the same quality to all frames, using binary search.|
|1|`equal_psnr`|Generate animation so that all frames have the same PSNR.|
|2|`near_ll_diff`|Generate animation allowing near-lossless method, impose different pre-processing factor to near-losslessly-encoded frames.|
|3|`near_ll_equal`|Generate animation allowing near-lossless method, impose the same pre-processing factor to near-losslessly-encoded frames.|
|4|`slope_optim`|Generate animation with slope optimization.|

The **slope optimization** algorithm terminates the binary search of `equal_quality` early if the PSNR increase is not worth the size increase. The extra byte budget can then be used for near-lossless encoding.

---

### Thumbnailer Compare

This tool takes the same list of frames as [Thumbnailer](#thumbnailer-1), runs all algorithms, and prints several PSNR statistics (max PSNR increase, mean PSNR difference, etc.) with reference to `-equal_quality`.

#### Usage:

```
./bazel-bin/src/utils/thumbnailer_compare frames_list.txt
```

Option `-short` condenses the printed message.

---

### Thumbnailer Test

Unit tests of machine-generated image data, created with [Google Test](https://github.com/google/googletest).

These tests are built as a binary:

```
bazel run test:thumbnailer_test
```

Run `thumbnailer_test` with [valgrind](https://valgrind.org/) to check for memory leaks and memory errors.

```
valgrind --leak-check=full --show-leak-kinds=all ./bazel-bin/test/thumbnailer_test
```
