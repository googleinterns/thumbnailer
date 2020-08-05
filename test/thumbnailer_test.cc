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

#include <random>

#include "../src/class.h"
#include "gtest/gtest.h"

const int kDefaultWidth = 80;
const int kDefaultHeight = 60;
const int kDefaultBudget = 153600;  // 150 kB

void WebPPictureDelete(WebPPicture* picture) {
  WebPPictureFree(picture);
  delete picture;
}

void WebPDataDelete(WebPData* webp_data) {
  WebPDataClear(webp_data);
  delete webp_data;
}

class WebPSamplesGenerator {
 public:
  // Initializing.
  WebPSamplesGenerator()
      : pic_count_(10),
        width_(kDefaultWidth),
        height_(kDefaultHeight),
        transparency_(0xff),
        randomized_(true) {}

  WebPSamplesGenerator(int pic_count, uint8_t transparency, bool randomized)
      : pic_count_(pic_count),
        width_(kDefaultWidth),
        height_(kDefaultHeight),
        transparency_(transparency),
        randomized_(randomized) {}

  WebPSamplesGenerator(int pic_count, int width, int height,
                       uint8_t transparency, bool randomized)
      : pic_count_(pic_count),
        width_(width),
        height_(height),
        transparency_(transparency),
        randomized_(randomized) {}

  // Returns vector of WebPPicture(s).
  // randomized == true: picture is random noise
  // randomized == false: picture is random solid color
  std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>>
  GeneratePics() {
    std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> pics;
    for (int i = 0; i < pic_count_; ++i) {
      pics.emplace_back(new WebPPicture, WebPPictureDelete);
      auto& pic = pics.back();
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
  bool randomized_;
  uint8_t transparency_;

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

TEST(ThumbnailerTest, BlankImageSolid) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  const int pic_count = 10;
  const uint8_t transparency = 0xff;
  const bool use_randomized = false;
  auto test_pics = WebPSamplesGenerator(pic_count, transparency, use_randomized)
                       .GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());

  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerTest, BlankImageTransparent) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  const int pic_count = 10;
  const uint8_t transparency = 0xaf;
  const bool use_randomized = false;
  auto test_pics = WebPSamplesGenerator(pic_count, transparency, use_randomized)
                       .GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());

  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerTest, NoisyImageSolid) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  const int pic_count = 10;
  const uint8_t transparency = 0xff;
  const bool use_randomized = true;
  auto test_pics = WebPSamplesGenerator(pic_count, transparency, use_randomized)
                       .GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());

  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerTest, NoisyImageTransparent) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  const int pic_count = 10;
  const uint8_t transparency = 0xaf;
  const bool use_randomized = true;
  auto test_pics = WebPSamplesGenerator(pic_count, transparency, use_randomized)
                       .GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());

  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
