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
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_format.h"
#include "thumbnailer.h"
#include "utils/thumbnailer_utils.h"

ABSL_FLAG(bool, verbose, false, "Print various encoding statistics.");
ABSL_FLAG(bool, equal_psnr, false,
          "Generate animation so that all frames have the same PSNR.");
ABSL_FLAG(
    bool, near_ll_diff, false,
    "Generate animation allowing near-lossless method. The pre-processing "
    "value for each near-lossless frames can be different.");
ABSL_FLAG(bool, near_ll_equal, false,
          "Generate animation allowing near-lossless method. Use the same "
          "pre-processing value for all near-lossless frames.");
ABSL_FLAG(bool, slope_optim, false,
          "Generate animation with slope optimization.");
ABSL_FLAG(int, m, 4,
          "Effort/speed trade-off (0=fast, 6=slower-better), default value 4.");
ABSL_FLAG(float, slope_dpsnr, 1.0,
          "Maximum PSNR change used in slope optimization, default value 1.0.");
ABSL_FLAG(std::string, o, "out.webp", "Output file name.");

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

  absl::SetProgramUsageMessage(
      absl::StrFormat("usage: %s %s", argv[0], "frame_list.txt"));
  absl::ParseCommandLine(argc, argv);

  // Parsing thumbnailer options.
  thumbnailer::ThumbnailerOption thumbnailer_option;
  if (absl::GetFlag(FLAGS_verbose)) thumbnailer_option.set_verbose(true);

  const int webp_method = absl::GetFlag(FLAGS_m);
  if (webp_method < 0 || webp_method > 6) {
    std::cerr << "Invalid -m value." << std::endl;
    return 1;
  }
  thumbnailer_option.set_method(webp_method);

  const float slope_dpsnr = absl::GetFlag(FLAGS_slope_dpsnr);
  if (slope_dpsnr < 0) {
    std::cerr << "Invalid -dpsnr value." << std::endl;
    return 1;
  }
  thumbnailer_option.set_slope_dpsnr(slope_dpsnr);

  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer(thumbnailer_option);

  // Process list of images and timestamps.
  int input_list_c = 0;
  for (int c = 1; c < argc; ++c) {
    if (argv[c][0] != '-') {
      input_list_c = c;
    }
  }
  if (input_list_c == 0) {
    std::cerr << "No input list specified." << std::endl;
    return 1;
  }

  std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> pics;
  std::ifstream input_list(argv[input_list_c]);
  std::string filename_str;
  int timestamp_ms;

  while (input_list >> filename_str >> timestamp_ms) {
    pics.emplace_back(new WebPPicture, libwebp::WebPPictureDelete);
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
  const std::string& output = absl::GetFlag(FLAGS_o);
  WebPData webp_data;
  WebPDataInit(&webp_data);

  // By default, use lossy encode and imposing the same quality to all frames.
  libwebp::Thumbnailer::Method method =
      libwebp::Thumbnailer::Method::kEqualQuality;

  if (absl::GetFlag(FLAGS_equal_psnr)) {
    method = libwebp::Thumbnailer::Method::kEqualPSNR;
  } else if (absl::GetFlag(FLAGS_near_ll_diff)) {
    method = libwebp::Thumbnailer::Method::kNearllDiff;
  } else if (absl::GetFlag(FLAGS_near_ll_equal)) {
    method = libwebp::Thumbnailer::Method::kNearllEqual;
  } else if (absl::GetFlag(FLAGS_slope_optim)) {
    method = libwebp::Thumbnailer::Method::kSlopeOptim;
  }

  libwebp::Thumbnailer::Status status =
      thumbnailer.GenerateAnimation(&webp_data, method);

  // Write animation to file.
  if (status == libwebp::Thumbnailer::Status::kOk) {
    ImgIoUtilWriteFile(output.c_str(), webp_data.bytes, webp_data.size);
  } else {
    std::cerr << "Error generating thumbnail." << std::endl;
  }
  WebPDataClear(&webp_data);

  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}
