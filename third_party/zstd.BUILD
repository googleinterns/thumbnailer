# Modified from
# https://github.com/tensorflow/io/blob/master/third_party/zstd.BUILD

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # BSD license

exports_files(["LICENSE"])

cc_library(
    name = "zstd",
    srcs = glob(
        [
            "lib/common/*.h",
            "lib/common/*.c",
            "lib/compress/*.c",
            "lib/compress/*.h",
            "lib/decompress/*.c",
            "lib/decompress/*.h",
        ],
        exclude = [
            "lib/common/xxhash.c",
        ],
    ),
    hdrs = [
        "lib/zstd.h",
    ],
    defines = [
        "XXH_PRIVATE_API",
    ],
    includes = [
        "lib",
        "lib/common",
    ],
    linkopts = [],
    textual_hdrs = [
        "lib/common/xxhash.c",
    ],
    visibility = ["//visibility:public"],
)
