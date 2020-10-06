// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "../src/thumbnailer.h"

#include <random>

#include "../src/utils/thumbnailer_utils.h"
#include "gtest/gtest.h"

const int kDefaultWidth = 160;
const int kDefaultHeight = 90;
const int kDefaultBudget = 153600;  // 150 kB

class WebPTestGenerator {
 public:
  // Initializing.
  WebPTestGenerator()
      : pic_count_(10),
        width_(kDefaultWidth),
        height_(kDefaultHeight),
        transparency_(0xff),
        randomized_(true) {}

  WebPTestGenerator(int pic_count, uint8_t transparency, bool randomized)
      : pic_count_(pic_count),
        width_(kDefaultWidth),
        height_(kDefaultHeight),
        transparency_(transparency),
        randomized_(randomized) {}

  WebPTestGenerator(int pic_count, int width, int height, uint8_t transparency,
                    bool randomized)
      : pic_count_(pic_count),
        width_(width),
        height_(height),
        transparency_(transparency),
        randomized_(randomized) {}

  // Returns vector of WebPPicture(s).
  // randomized_ == true: all pictures are random noise
  // randomized_ == false: all pictures are random solid color
  std::vector<EnclosedWebPPicture> GeneratePics() {
    std::vector<EnclosedWebPPicture> pics;
    for (int i = 0; i < pic_count_; ++i) {
      pics.emplace_back(new WebPPicture, libwebp::WebPPictureDelete);
      EnclosedWebPPicture& pic = pics.back();
      WebPPictureInit(pic.get());
      pic->use_argb = 1;
      pic->width = width_;
      pic->height = height_;
      WebPPictureImportRGBA(pic.get(), GenerateRGBA(i).data(), width_ * 4);
    }
    return pics;
  }

 private:
  int pic_count_;
  int width_;
  int height_;
  uint8_t transparency_;
  bool randomized_;

  // Returns RGBA values for WebPPicture.
  std::vector<uint8_t> GenerateRGBA(int seed) {
    std::mt19937 rng(seed);
    std::vector<uint8_t> rgba;

    const uint8_t color_R = rng() & 0xff;
    const uint8_t color_G = rng() & 0xff;
    const uint8_t color_B = rng() & 0xff;

    for (int i = 0; i < height_; ++i) {
      for (int j = 0; j < width_; ++j) {
        rgba.push_back(randomized_ ? rng() & 0xff : color_R);
        rgba.push_back(randomized_ ? rng() & 0xff : color_G);
        rgba.push_back(randomized_ ? rng() & 0xff : color_B);
        rgba.push_back(transparency_);
      }
    }
    return rgba;
  }
};

class GenerateAnimationTest
    : public ::testing::TestWithParam<
          std::tuple<int, uint8_t, bool, libwebp::Thumbnailer::Method>> {};

TEST_P(GenerateAnimationTest, IsGenerated) {
  const int pic_count = std::get<0>(GetParam());
  const uint8_t transparency = std::get<1>(GetParam());
  const bool use_randomized = std::get<2>(GetParam());
  const libwebp::Thumbnailer::Method method = std::get<3>(GetParam());

  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();
  std::vector<EnclosedWebPPicture> pics =
      WebPTestGenerator(pic_count, transparency, use_randomized).GeneratePics();
  for (int i = 0; i < pic_count; ++i) {
    ASSERT_EQ(thumbnailer.AddFrame(*pics[i], i * 500),
              libwebp::Thumbnailer::kOk);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(
      new WebPData, libwebp::WebPDataDelete);
  WebPDataInit(webp_data.get());

  ASSERT_EQ(thumbnailer.GenerateAnimation(webp_data.get(), method),
            libwebp::Thumbnailer::kOk);

  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

INSTANTIATE_TEST_CASE_P(
    ThumbnailerTest, GenerateAnimationTest,
    ::testing::Combine(::testing::Values(10), ::testing::Values(0xff, 0xaf),
                       ::testing::Values(false, true),
                       ::testing::ValuesIn(libwebp::Thumbnailer::kMethodList)));

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
