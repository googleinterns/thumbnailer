# Modified from
# https://github.com/apache/incubator-pagespeed-mod/blob/master/bazel/libwebp.bzl

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # WebM license

exports_files(["COPYING"])

cc_library(
    name = "libwebp",
    srcs = glob([
        "src/dsp/*.c",
        "src/dsp/*.h",
        "src/utils/*.c",
        "src/utils/*.h",
        "src/dec/*.c",
        "src/dec/*.h",
        "src/mux/*.c",
        "src/mux/*.h",
        "src/demux/*.c",
        "src/demux/*.h",
        "src/enc/*.c",
        "src/enc/*.h",
    ]) + [
        "imageio/imageio_util.c",
        "imageio/webpdec.c",
        "imageio/metadata.c",
    ],
    hdrs = glob([
        "src/webp/*.h",
    ]) + [
        "imageio/webpdec.h",
        "imageio/metadata.h",
        "imageio/imageio_util.h",
        "examples/unicode.h",
    ],
    includes = ["src"],
    visibility = ["//visibility:public"],
)
