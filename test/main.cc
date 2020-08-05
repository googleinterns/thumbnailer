#include <random>

#include "../src/class.h"
#include "gtest/gtest.h"

const int kDefaultWidth = 80;
const int kDefaultHeight = 60;

class WebPSamplesGenerator {
 public:
  // Initializing.
  WebPSamplesGenerator()
      : pic_count(10),
        width(kDefaultWidth),
        height(kDefaultHeight),
        transparency(0xff),
        randomized(true) {}

  WebPSamplesGenerator(int pic_count, uint8_t transparency, bool randomized)
      : pic_count(pic_count),
        width(kDefaultWidth),
        height(kDefaultHeight),
        transparency(transparency),
        randomized(randomized) {}

  WebPSamplesGenerator(int pic_count, int width, int height,
                       uint8_t transparency, bool randomized)
      : pic_count(pic_count),
        width(width),
        height(height),
        transparency(transparency),
        randomized(randomized) {}

  // Returns RGBA values for WebPPicture.
  std::vector<uint8_t> GenerateRGBA(int seed) {
    std::mt19937 rng(seed);
    std::vector<uint8_t> rgba;

    int color_R = rng();
    int color_G = rng();
    int color_B = rng();

    for (int i = 0; i <= height; ++i) {
      for (int j = 0; j <= width * 4; ++j) {
        rgba.push_back(randomized ? rng() & 0xff : color_R);
        rgba.push_back(randomized ? rng() & 0xff : color_G);
        rgba.push_back(randomized ? rng() & 0xff : color_B);
        rgba.push_back(transparency);
      }
    }
    return rgba;
  }

  // Returns vector of WebPPicture(s).
  // randomized == true: picture is random noise
  // randomized == false: picture is random solid color
  std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>>
  GeneratePics() {
    std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> pics;
    for (int i = 0; i < pic_count; ++i) {
      pics.emplace_back(new WebPPicture, WebPPictureFree);
      auto& pic = pics.back();
      WebPPictureInit(pic.get());
      pic.get()->use_argb = 1;
      pic.get()->width = width;
      pic.get()->height = height;
      WebPPictureImportRGBA(pic.get(), GenerateRGBA(i).data(), width * 4);
    }
    return pics;
  }

 private:
  int pic_count;
  int width;
  int height;
  bool randomized;
  uint8_t transparency;
};

TEST(blank_image_solid, blank_images) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  int pic_count = 10;
  WebPSamplesGenerator Generator = WebPSamplesGenerator(pic_count, 0xff, false);
  auto test_pics = Generator.GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataClear);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());
  EXPECT_LE(webp_data.get()->size, 153600);
}

TEST(blank_image_transparent, blank_images) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  int pic_count = 10;
  WebPSamplesGenerator Generator = WebPSamplesGenerator(pic_count, 0xaf, false);
  auto test_pics = Generator.GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataClear);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());
  EXPECT_LE(webp_data.get()->size, 153600);
}

TEST(randomized_image_solid, randomized_images) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  int pic_count = 10;
  WebPSamplesGenerator Generator = WebPSamplesGenerator(pic_count, 0xff, true);
  auto test_pics = Generator.GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataClear);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());
  EXPECT_LE(webp_data.get()->size, 153600);
}

TEST(randomized_image_transparent, randomized_images) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  int pic_count = 10;
  WebPSamplesGenerator Generator = WebPSamplesGenerator(pic_count, 0xaf, true);
  auto test_pics = Generator.GeneratePics();

  for (int i = 0; i < pic_count; ++i) {
    thumbnailer.AddFrame(*test_pics[i].get(), i * 500);
  }
  std::unique_ptr<WebPData, void (*)(WebPData*)> webp_data(new WebPData,
                                                           WebPDataClear);
  WebPDataInit(webp_data.get());
  thumbnailer.GenerateAnimation(webp_data.get());
  EXPECT_LE(webp_data.get()->size, 153600);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
