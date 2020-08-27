// Copyright 2019 Google LLC
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

#include <fstream>
#include <iostream>
#include <string>

#include "thumbnailer.h"
#include "thumbnailer.pb.h"

// Returns true on success or false on failure.
static bool ReadImage(const char filename[], WebPPicture* const pic) {
  const uint8_t* data = NULL;
  size_t data_size = 0;
  if (!ImgIoUtilReadFile(filename, &data, &data_size)) return false;

  pic->use_argb = 1;  // force ARGB.

  WebPImageReader reader = WebPGuessImageReader(data, data_size);
  bool ok = reader(data, data_size, pic, 1, NULL);
  free((void*)data);

  return ok;
}

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  thumbnailer::ThumbnailerOption thumbnailer_option;

  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer(thumbnailer_option);

  // Process list of images and timestamps.
  std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> pics;
  std::ifstream input_list(argv[1]);
  std::string filename_str;
  int timestamp_ms;

  while (input_list >> filename_str >> timestamp_ms) {
    pics.emplace_back(new WebPPicture, WebPPictureFree);
    WebPPicture* current_frame = pics.back().get();
    WebPPictureInit(current_frame);

    ReadImage(filename_str.c_str(), current_frame);
    thumbnailer.AddFrame(*current_frame, timestamp_ms);
  }

  // Write animation to file.
  const char* output = argv[2];
  WebPData webp_data;
  WebPDataInit(&webp_data);

  bool try_equal_psnr = false;
  int try_near_lossless = -1;
  bool slope_optim = false;

  // Option-parsing pass.
  for (int c = 3; c < argc; c++) {
    if (!strcmp(argv[c], "psnr")) {
      // Generate animation so that all frames have the same PSNR.
      try_equal_psnr = true;
    } else if (!strcmp(argv[c], "near_lossless_diff_pre_processing")) {
      // Generate animation allowing near-lossless method. The pre-processing
      // value for each near-lossless frames can be different.
      try_near_lossless = 0;
    } else if (!strcmp(argv[c], "near_lossless_equal_pre_processing")) {
      // Generate animation allowing near-lossless method. Use the same
      // pre-processing value for all near-lossless frames.
      try_near_lossless = 1;
    } else if (!strcmp(argv[c], "slope_optimization")) {
      // Generate animation with slop optimization, 'try_equal_psnr' and
      // 'try_near_lossless' must be false when using this method.
      slope_optim = true;
    }
  }

  if (slope_optim) {
    thumbnailer.GenerateAnimationSlopeOptim(&webp_data);
  } else {
    if (try_equal_psnr) {
      thumbnailer.GenerateAnimationEqualPSNR(&webp_data);
    } else {
      thumbnailer.GenerateAnimationEqualQuality(&webp_data);
    }

    if (try_near_lossless == 0) {
      thumbnailer.NearLosslessDiffPreProcessing(&webp_data);
    } else if (try_near_lossless == 1) {
      thumbnailer.NearLosslessEqualPreProcessing(&webp_data);
    }
  }

  ImgIoUtilWriteFile(output, webp_data.bytes, webp_data.size);
  WebPDataClear(&webp_data);

  google::protobuf::ShutdownProtobufLibrary();

  return 0;
}
