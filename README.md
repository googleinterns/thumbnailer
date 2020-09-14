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
In order to build the Thumbnailer API, you can use the following Linux command line:
```
bazel build "//src:thumbnailer" && bazel build "//src/utils:thumbnailer_compare"
```

## Usage
### Thumbnail generating tool
This tool reads a sequence of images and timestamps from a text file and produces an animation in WebP format.

**Usage:**
```
./bazel-bin/src/thumbnailer [options] frame_list.txt -o=output.webp
```
By default, the lossy compression method imposing the same quality to all frames is used to produce animation.

**Example:**
```
./bazel-bin/src/thumbnailer -min_lossy_quality 20 -loop_count 5 -verbose -slope_optim frames.txt -o anim.webp
```

**Options:**
|Type | Name | Default Value | Description|
|--------|-------|-------------------|--------------|
|uint32_t|-soft_max_size|153600|Desired (soft) maximum size limit (in bytes).|
|uint32_t|-hard_max_size|153600|Hard limit for maximum file size (in bytes).|
|uint32_t|-loop_count|0 (infinite loop)|Number of times animation will loop.|
|uint32_t|-min_lossy_quality|0|Minimum lossy quality (0..100) to be used for encoding each frame.|
|uint32_t|-m|4|Effort/speed trade-off (0=fast, 6=slower-better).|
|bool|-allow_mixed|false|Use mixed lossy/lossless compression.|
|bool|-verbose|false|Print various encoding statistics.|
|bool|-equal_quality|true|Generate animation setting the same quality to all frames.|
|bool|-equal_psnr|false|Generate animation so that all frames have the same PSNR.|
|bool|-near_ll_diff|false|Generate animation allowing near-lossless method, the pre-processing value for each near-losslessly-encoded frames can be different.|
|bool|-near_ll_equal|false|Generate animation allowing near-lossless method. Use the same pre-processing value for all near-losslessly-encoded frames.|
|bool|-slope_optim|false|Generate animation with slope optimization.|
|float|-slope_dpsnr|1.0|Maximum PSNR change (in dB) used in slope optimization.|

### Thumbnail comparing tool
This tools reads a sequence of images and timestamps from a text file and print several  statistical comparisons (e.g: Mean PSNR change, Max PSNR increase, etc) of the animations created by different algorithms. The reference animation is created by the lossy compression method and imposing the same quality to all frames.

**Usage:**
```
thumbnailer_compare frames.txt
```
Additionally, the following can help you condense the printed message :
```
thumbnailer_compare -short frames.txt
```

## Testing
A set of unit tests was created from several sets of solid color or noise images in order to verify that the resulting animation fits the constraint, as well as to detect the memory errors.
These tests were created with [Google Test](https://github.com/google/googletest) and can be built with Bazel using:
```
bazel build --compilation_mode=opt "//test:thumbnailer_test"
```
On Linux, the following command line would run all the tests and also show all the possible memory errors:
```
valgrind --leak-check=full --show-leak-kinds=all ./bazel-bin/test/thumbnailer_test
```

***Last updated: 09/14/2020.***