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

#include "../examples/example_util.h"
#include "thumbnailer.h"
#include "thumbnailer.pb.h"

// Returns true on success and false on failure.
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

  bool try_equal_psnr = false;
  int try_near_lossless = -1;
  bool slope_optim = false;

  for (int c = 3; c < argc; c++) {
    if (!strcmp(argv[c], "-verbose")) {
      thumbnailer_option.set_verbose(true);
    }
    int parse_error = 0;
    // Parsing generating method.
    if (!strcmp(argv[c], "-equal_psnr")) {
      // Generate animation so that all frames have the same PSNR.
      try_equal_psnr = true;
    } else if (!strcmp(argv[c], "-near_ll_diff")) {
      // Generate animation allowing near-lossless method. The pre-processing
      // value for each near-lossless frames can be different.
      try_near_lossless = 0;
    } else if (!strcmp(argv[c], "-near_ll_equal")) {
      // Generate animation allowing near-lossless method. Use the same
      // pre-processing value for all near-lossless frames.
      try_near_lossless = 1;
    } else if (!strcmp(argv[c], "-slope_optim")) {
      // Generate animation with slope optimization, ignore 'try_equal_psnr'
      // and 'try_near_lossless'.
      slope_optim = true;
    } else if (!strcmp(argv[c], "-m")) {
      // Effort/speed trade-off (0=fast, 6=slower-better), default value 4.
      thumbnailer_option.set_method(ExUtilGetInt(argv[++c], 0, &parse_error));
    } else if (!strcmp(argv[c], "-slope_dpsnr")) {
      // Maximum PSNR change used in slope optimization, default value 1.0.
      thumbnailer_option.set_slope_dpsnr(
          ExUtilGetFloat(argv[++c], &parse_error));
    }
    if (parse_error) {
      std::cerr << "Error parsing options." << std::endl;
      return 1;
    }
  }

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

    if (!ReadImage(filename_str.c_str(), current_frame)) {
      std::cerr << "Failed to read image " << filename_str << std::endl;
      return 1;
    }

    if (thumbnailer.AddFrame(*current_frame, timestamp_ms) !=
        libwebp::Thumbnailer::Status::kOk) {
      std::cerr << "Error adding frame " << filename_str << std::endl;
      return 1;
    }
  }

  if (pics.empty()) {
    std::cerr << "No input frame(s) for generating animation." << std::endl;
    return 1;
  }

  // Generate animation.
  const char* output = argv[2];
  WebPData webp_data;
  WebPDataInit(&webp_data);

  libwebp::Thumbnailer::Status status;
  if (slope_optim) {
    status = thumbnailer.GenerateAnimationSlopeOptim(&webp_data);
  } else {
    if (try_equal_psnr) {
      status = thumbnailer.GenerateAnimationEqualPSNR(&webp_data);
    } else {
      status = thumbnailer.GenerateAnimationEqualQuality(&webp_data);
    }

    if (status == libwebp::Thumbnailer::Status::kOk && try_near_lossless == 0) {
      status = thumbnailer.NearLosslessDiff(&webp_data);
    } else if (try_near_lossless == 1) {
      status = thumbnailer.NearLosslessEqual(&webp_data);
    }
  }

  // Write animation to file.
  if (status == libwebp::Thumbnailer::Status::kOk) {
    ImgIoUtilWriteFile(output, webp_data.bytes, webp_data.size);
  } else {
    std::cerr << "Error generating thumbnail." << std::endl;
  }
  WebPDataClear(&webp_data);

  google::protobuf::ShutdownProtobufLibrary();

  return 0;
}
