# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "imageio_util",
    srcs = ["imageio_util.c"],
    hdrs = ["imageio_util.h"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//examples:unicode",
        "@libwebp",
    ],
)

cc_library(
    name = "imagedec",
    srcs = [
        "image_dec.c",
        "jpegdec.c",
        "metadata.c",
        "pngdec.c",
        "pnmdec.c",
        "tiffdec.c",
        "webpdec.c",
        "wicdec.c",
    ],
    hdrs = [
        "image_dec.h",
        "jpegdec.h",
        "metadata.h",
        "pngdec.h",
        "pnmdec.h",
        "tiffdec.h",
        "webpdec.h",
        "wicdec.h",
    ],
    copts = [
        "-DWEBP_HAVE_PNG",
        "-DWEBP_HAVE_JPEG",
        "-DWEBP_HAVE_TIFF",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":imageio_util",
        "@libjpeg_turbo",
        "@libpng",
        "@libtiff",
        "@libwebp",
    ],
)
