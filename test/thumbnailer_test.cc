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

#include "gtest/gtest.h"

const int kDefaultWidth = 160;
const int kDefaultHeight = 90;
const int kDefaultBudget = 153600;  // 150 kB

void WebPPictureDelete(WebPPicture* picture) {
  WebPPictureFree(picture);
  delete picture;
}

void WebPDataDelete(WebPData* webp_data) {
  WebPDataClear(webp_data);
  delete webp_data;
}

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

  // Returns true on success and false on failure.
  bool GenerateWebPData(WebPData* const webp_data, bool equal_psnr) {
    libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();
    auto test_pics = GeneratePics();
    for (int i = 0; i < pic_count_; ++i) {
      if (thumbnailer.AddFrame(*test_pics[i], i * 500) !=
          libwebp::Thumbnailer::kOk) {
        return false;
      }
    }
    if (!equal_psnr) {
      return thumbnailer.GenerateAnimation(webp_data) ==
             libwebp::Thumbnailer::kOk;
    } else {
      return thumbnailer.GenerateAnimationEqualPSNR(webp_data) ==
             libwebp::Thumbnailer::kOk;
    }
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

  // Returns vector of WebPPicture(s).
  // randomized_ == true: all pictures are random noise
  // randomized_ == false: all pictures are random solid color
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
};

TEST(ThumbnailerTest, BlankImageSolid) {
  const int pic_count = 10;
  const uint8_t transparency = 0xff;
  const bool use_randomized = false;
  const bool equal_psnr = false;
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  ASSERT_TRUE(WebPTestGenerator(pic_count, transparency, use_randomized)
                  .GenerateWebPData(webp_data.get(), equal_psnr));
  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerTest, BlankImageTransparent) {
  const int pic_count = 10;
  const uint8_t transparency = 0xaf;
  const bool use_randomized = false;
  const bool equal_psnr = false;
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  ASSERT_TRUE(WebPTestGenerator(pic_count, transparency, use_randomized)
                  .GenerateWebPData(webp_data.get(), equal_psnr));
  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerTest, NoisyImageSolid) {
  const int pic_count = 10;
  const uint8_t transparency = 0xff;
  const bool use_randomized = true;
  const bool equal_psnr = false;
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  ASSERT_TRUE(WebPTestGenerator(pic_count, transparency, use_randomized)
                  .GenerateWebPData(webp_data.get(), equal_psnr));
  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerTest, NoisyImageTransparent) {
  const int pic_count = 10;
  const uint8_t transparency = 0xaf;
  const bool use_randomized = true;
  const bool equal_psnr = false;
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  ASSERT_TRUE(WebPTestGenerator(pic_count, transparency, use_randomized)
                  .GenerateWebPData(webp_data.get(), equal_psnr));
  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerPSNRTest, NoisyImageSolid) {
  const int pic_count = 10;
  const uint8_t transparency = 0xff;
  const bool use_randomized = true;
  const bool equal_psnr = true;
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  WebPDataInit(webp_data.get());
  ASSERT_TRUE(WebPTestGenerator(pic_count, transparency, use_randomized)
                  .GenerateWebPData(webp_data.get(), equal_psnr));
  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

TEST(ThumbnailerPSNRTest, NoisyImageTransparent) {
  const int pic_count = 10;
  const uint8_t transparency = 0xaf;
  const bool use_randomized = true;
  const bool equal_psnr = true;
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataDelete);
  WebPDataInit(webp_data.get());
  ASSERT_TRUE(WebPTestGenerator(pic_count, transparency, use_randomized)
                  .GenerateWebPData(webp_data.get(), equal_psnr));
  EXPECT_LE(webp_data->size, kDefaultBudget);
  EXPECT_GT(webp_data->size, 0);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
