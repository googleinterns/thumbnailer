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

workspace(name = "thumbnailer")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

# compression libraries
http_archive(
    name = "zlib",
    build_file = "//third_party:zlib.BUILD",
    sha256 = "1cce3828ec2ba80ff8a4cac0ab5aa03756026517154c4b450e617ede751d41bd",
    strip_prefix = "zlib-cacf7f1d4e3d44d871b605da3b647f07d718623f",
    urls = ["https://github.com/madler/zlib/archive/cacf7f1d4e3d44d871b605da3b647f07d718623f.zip"],
)

http_archive(
    name = "xz",
    build_file = "//third_party:xz.BUILD",
    sha256 = "b512f3b726d3b37b6dc4c8570e137b9311e7552e8ccbab4d39d47ce5f4177145",
    strip_prefix = "xz-5.2.4",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/tukaani.org/xz/xz-5.2.4.tar.gz",
        "https://tukaani.org/xz/xz-5.2.4.tar.gz",
    ],
)

http_archive(
    name = "zstd",
    build_file = "//third_party:zstd.BUILD",
    sha256 = "a364f5162c7d1a455cc915e8e3cf5f4bd8b75d09bc0f53965b0c9ca1383c52c8",
    strip_prefix = "zstd-1.4.4",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/facebook/zstd/archive/v1.4.4.tar.gz",
        "https://github.com/facebook/zstd/archive/v1.4.4.tar.gz",
    ],
)

# image libraries
http_archive(
    name = "libwebp",
    build_file = "//third_party:libwebp.BUILD",
    sha256 = "6930edd25a8cc4031bcb9223022898d02239d8600aa90a0974d2b70d3d0dd161",
    strip_prefix = "libwebp-55a080e50af655d1fbe0a5c22954835cdd59ff92",
    urls = ["https://github.com/webmproject/libwebp/archive/55a080e50af655d1fbe0a5c22954835cdd59ff92.zip"],
)

http_archive(
    name = "libpng",
    build_file = "//third_party:libpng.BUILD",
    sha256 = "3d22d46c566b1761a0e15ea397589b3a5f36ac09b7c785382e6470156c04247f",
    strip_prefix = "libpng-1.6.35",
    urls = ["https://github.com/glennrp/libpng/archive/v1.6.35.zip"],
)

http_archive(
    name = "libjpeg_turbo",
    build_file = "//third_party:libjpeg_turbo.BUILD",
    sha256 = "7777c3c19762940cff42b3ba4d7cd5c52d1671b39a79532050c85efb99079064",
    strip_prefix = "libjpeg-turbo-2.0.4",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/libjpeg-turbo/libjpeg-turbo/archive/2.0.4.tar.gz",
        "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/2.0.4.tar.gz",
    ],
)

http_archive(
    name = "libtiff",
    build_file = "//third_party:libtiff.BUILD",
    sha256 = "5d29f32517dadb6dbcd1255ea5bbc93a2b54b94fbf83653b4d65c7d6775b8634",
    strip_prefix = "tiff-4.1.0",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/download.osgeo.org/libtiff/tiff-4.1.0.tar.gz",
        "https://download.osgeo.org/libtiff/tiff-4.1.0.tar.gz",
    ],
)

# googletest
git_repository(
    name = "gtest",
    commit = "5f8fcf4aa8ce30dfe76f9c9db97d2da4ce3737ef",
    remote = "https://github.com/google/googletest",
)

# protobuf
http_archive(
    name = "rules_proto",
    sha256 = "602e7161d9195e50246177e7c55b2f39950a9cf7366f74ed5f22fd45750cd208",
    strip_prefix = "rules_proto-97d8af4dc474595af3900dd85cb3a29ad28cc313",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
        "https://github.com/bazelbuild/rules_proto/archive/97d8af4dc474595af3900dd85cb3a29ad28cc313.tar.gz",
    ],
)

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()
