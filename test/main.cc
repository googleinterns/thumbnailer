#include "../src/class.h"
#include "gtest/gtest.h"

// Returns RGBA values for WebPPicture.
// R, G, B values can be randomized or set to a common fixed value.
std::vector<uint8_t> GenerateRGBA(int width, int height, bool randomized,
                                  int fixed, uint8_t transparency) {
  std::vector<uint8_t> rgba;
  for (int i = 0; i <= height; ++i) {
    for (int j = 0; j <= width * 4; ++j) {
      for (int k = 0; k < 3; ++k) {
        rgba.push_back(randomized ? rand() & 0xff : fixed);
      }
      rgba.push_back(transparency);
    }
  }
  return rgba;
}

// Returns vector of WebPPicture.
std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> GeneratePics(
    int pic_count, int width, int height, bool randomized = 0, int fixed = 0xff,
    uint8_t transparency = 0xff) {
  std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> pics;
  for (int i = 0; i < pic_count; ++i) {
    pics.emplace_back(new WebPPicture, WebPPictureFree);
    WebPPictureInit(pics.back().get());
    pics.back().get()->use_argb = 1;
    pics.back().get()->width = width;
    pics.back().get()->height = height;
    WebPPictureImportRGBA(
        pics.back().get(),
        GenerateRGBA(width, height, randomized, fixed, transparency).data(),
        width * 4);
  }
  return pics;
}

TEST(blank_image_solid, blank_images) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  int pic_count = 10;
  int width = 50;
  int height = 50;
  auto test_pics = GeneratePics(pic_count, width, height, 0, 0xaf);

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
  int width = 50;
  int height = 50;
  auto test_pics = GeneratePics(pic_count, width, height, 0, 0xff, 0xaf);

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
  int width = 50;
  int height = 50;
  auto test_pics = GeneratePics(pic_count, width, height, 1);

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
  int width = 50;
  int height = 50;
  auto test_pics = GeneratePics(pic_count, width, height, 1, 0, 0xaf);

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
  srand(time(0));
  return RUN_ALL_TESTS();
}
