
# Thumbnailer

Thumbnailer takes timestamped images as an input and produces an animation in WebP format that best fits under a given size constraint.

## Prerequisites

- [WebP Library](https://github.com/webmproject/libwebp)
- [Bazel Build System](https://docs.bazel.build/versions/master/bazel-overview.html)
- [Clang](https://clang.llvm.org/)

On Linux, the following would install all the dependencies you need for building the API:
```
sudo apt-get install bazel clang unzip zip libwebp-dev
```

## Building

In order to build the Thumbnailer binaries, you can use the following Linux command line:
```
bazel build "//src:thumbnailer" && bazel build "//src/utils:thumbnailer_compare"
```

## Usage

### Thumbnail Generating Tool

This tool reads a sequence of images and timestamps from a text file and produces an animation in WebP format.

**Usage:**

```
./bazel-bin/src/thumbnailer [options] frame_list.txt -o=output.webp
```

By default, the lossy compression method imposing the same quality to all frames will be used to produce animation. 

**Example:**

Command line to generate animation using various options:

```
./bazel-bin/src/thumbnailer -loop_count 5 -verbose -slope_optim frames.txt -o anim.webp
```

The following example shows the format of the text file containing frame filenames and timestamps:

```
./anim1/frame01.png 150
./anim1/frame02.png 300
./anim1/frame03.png 450
./anim1/frame04.png 600
```

**Options:**

| <img width="150"> Option | Default Value | <img width="600"> Description|
|---------------------------------------|:-------------:|------------|
|`-soft_max_size`|153600|Desired (soft) maximum size limit (in bytes).|
|`-hard_max_size`|153600|Hard limit for maximum file size (in bytes).|
|`-loop_count`|0 infinite loop|Number of times animation will loop.|
|`-min_lossy_quality`|0|Minimum lossy quality (0..100) to be used for encoding each frame.|
|`-m`|4|Effort/speed trade-off (0=fast, 6=slower-better).|
|`-allow_mixed`|false|Use mixed lossy/lossless compression.|
|`-algorithm`|equal_quality|Algorithm to generate animation {equal_quality, equal_psnr, near_ll_diff, near_ll_equal, slope_optim}.|
|`-slope_dpsnr`|1.0|Maximum PSNR change (in dB) used in slope optimization.|
|`-verbose`|false|Print various encoding statistics showed by chosen algorithm.|

**`-algorithm` flag description:**

| <img width="100"> Algorithm | <img width="600"> Description  |
|----------------------------|------------|
|`-equal_quality`|Generate animation setting the same quality to all frames.|
|`-equal_psnr`|Generate animation so that all frames have the same PSNR.|
|`-near_ll_diff`|Generate animation allowing near-lossless method, impose different pre-processing to near-losslessly-encoded frames.|
|`-near_ll_equal`|Generate animation allowing near-lossless method, impose the same pre-processing to near-losslessly-encoded frames.|
|`-slope_optim`|Generate animation with slope optimization.|


The *slope_optim* algorithm is proposed to manage more efficiently the resulting encoded frame size producing by the *equal_quality* algorithm. In fact, while the *equal_quality* algorithm finds the best quality for lossy encode, it inherently tries to use the byte budget intensively, as a result the extra budget for near-lossless-encoding extra step is very limited. The slope is the distortion-size ratio (in dB/bytes), larger slope corresponds to higher change in PSNR and lower change in frame size. Therefore, a limit slope to attain a fixed change (`dPSNR`) in distortion is chosen to control the quality increase to encode frame and save more extra budget for near-lossless encode. By default, `dPSNR = 1.0 dB`, you can change the `dPSNR` parameter using `-slope_dpsnr` option.

### Thumbnail Comparing Tool

This tools reads a sequence of images and timestamps from a text file and prints several statistical comparisons (e.g: Mean PSNR change, Max PSNR increase, etc) of the animations created by different algorithms. 

The lossy compression method imposing the same quality to all frames is chosen as the reference algorithm.

**Usage:**

```
./bazel-bin/src/utils/thumbnailer_compare frames.txt
```

Additionally, the following can help you condense the printed message:

```
./bazel-bin/src/utils/thumbnailer_compare -short frames.txt
```

## Testing

A set of unit tests was created from several random solid color or noise images in order to verify that the resulting animation fits the constraint, as well as to detect the memory errors.

These tests were created with [Google Test](https://github.com/google/googletest) and can be built with Bazel using:

```
bazel build --compilation_mode=opt "//test:thumbnailer_test"
```

On Linux, the following command line would run all the tests and also show all possible memory errors:

```
valgrind --leak-check=full --show-leak-kinds=all ./bazel-bin/test/thumbnailer_test
```

*Last updated: 09/14/2020.*