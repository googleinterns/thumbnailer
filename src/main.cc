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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "../examples/example_util.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "thumbnailer.h"
#include "utils/thumbnailer_utils.h"

ABSL_FLAG(std::string, o, "out.webp", "Output file name.");

// Thumbnailer algorithm options.
ABSL_FLAG(uint32_t, soft_max_size, 153600,
          "Desired (soft) maximum size limit in bytes.");
ABSL_FLAG(uint32_t, hard_max_size, 153600,
          "Hard limit for maximum file size. If it is less than "
          "'soft_max_size', it will be set to 'soft_max_size'.");
ABSL_FLAG(float, slope_dpsnr, 1.0,
          "Maximum PSNR change used in slope optimization.");

// WebP encoding options.
ABSL_FLAG(uint32_t, loop_count, 0,
          "Number of times animation will loop (0 = infinite loop).");
ABSL_FLAG(uint32_t, min_lossy_quality, 0,
          "Minimum lossy quality to be used for encoding each frame.");
ABSL_FLAG(uint32_t, m, 4, "Effort/speed trade-off (0=fast, 6=slower-better).");
ABSL_FLAG(bool, allow_mixed, false, "Use mixed lossy/lossless compression.");

// Binary options.
ABSL_FLAG(bool, verbose, false, "Print various encoding statistics.");

// Thumbnailer methods.
ABSL_FLAG(bool, equal_quality, false,
          "Generate animation so that all frames have the same quality.");
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

// Returns false on invalid configurations.
bool ThumbnailerValidateOption(
    const thumbnailer::ThumbnailerOption& thumbnailer_option) {
  if (thumbnailer_option.min_lossy_quality() > 100) return false;
  if (thumbnailer_option.webp_method() > 6) return false;
  if (thumbnailer_option.slope_dpsnr() < 0) return false;
  if (thumbnailer_option.slope_dpsnr() > 99) return false;
  return true;
}

int main(int argc, char* argv[]) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  absl::SetProgramUsageMessage(
      "Usage: thumbnailer [options] frame_list.txt -o=output.webp\n\nBy "
      "default, use lossy encoding and impose the same quality to all frames.");
  std::vector<char*> positional_args = absl::ParseCommandLine(argc, argv);

  // Parse thumbnailer options.
  thumbnailer::ThumbnailerOption thumbnailer_option;

  thumbnailer_option.set_soft_max_size(absl::GetFlag(FLAGS_soft_max_size));
  thumbnailer_option.set_hard_max_size(std::max(
      absl::GetFlag(FLAGS_hard_max_size), thumbnailer_option.soft_max_size()));
  thumbnailer_option.set_loop_count(absl::GetFlag(FLAGS_loop_count));
  thumbnailer_option.set_min_lossy_quality(
      absl::GetFlag(FLAGS_min_lossy_quality));
  thumbnailer_option.set_allow_mixed(absl::GetFlag(FLAGS_allow_mixed));
  thumbnailer_option.set_verbose(absl::GetFlag(FLAGS_verbose));
  thumbnailer_option.set_webp_method(absl::GetFlag(FLAGS_m));
  thumbnailer_option.set_slope_dpsnr(
      std::abs(absl::GetFlag(FLAGS_slope_dpsnr)));

  if (!ThumbnailerValidateOption(thumbnailer_option)) {
    std::cerr << "Invalid thumbnailer configuration." << std::endl;
    return 1;
  }

  // Initialize thumbnailer.
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer(thumbnailer_option);

  // Process list of images and timestamps.
  if (positional_args.size() != 2) {  // including argv[0]
    std::cerr << "No input list specified." << std::endl;
    return 1;
  }

  std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> pics;
  std::ifstream input_list(positional_args.back());
  std::string filename;
  int timestamp_ms;

  while (input_list >> filename >> timestamp_ms) {
    pics.emplace_back(new WebPPicture, libwebp::WebPPictureDelete);
    WebPPicture* current_frame = pics.back().get();
    WebPPictureInit(current_frame);
    if (!libwebp::ReadPicture(filename.c_str(), current_frame)) {
      std::cerr << "Failed to read image " << filename << std::endl;
      return 1;
    }
    if (thumbnailer.AddFrame(*current_frame, timestamp_ms) !=
        libwebp::Thumbnailer::Status::kOk) {
      std::cerr << "Error adding frame " << filename << std::endl;
      return 1;
    }
  }

  if (pics.empty()) {
    std::cerr << "No input frame(s) for generating animation." << std::endl;
    return 1;
  }

  // Generate the animation.
  WebPData webp_data;
  WebPDataInit(&webp_data);

  libwebp::Thumbnailer::Method method =
      libwebp::Thumbnailer::Method::kEqualQuality;

  if (absl::GetFlag(FLAGS_equal_psnr)) {
    method = libwebp::Thumbnailer::Method::kEqualPSNR;
  } else if (absl::GetFlag(FLAGS_equal_quality)) {
    method = libwebp::Thumbnailer::Method::kEqualQuality;
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
  const std::string output = absl::GetFlag(FLAGS_o);
  if (status == libwebp::Thumbnailer::Status::kOk) {
    ImgIoUtilWriteFile(output.c_str(), webp_data.bytes, webp_data.size);
  } else {
    std::cerr << "Error generating thumbnail." << std::endl;
  }
  WebPDataClear(&webp_data);

  google::protobuf::ShutdownProtobufLibrary();
  return 0;
}
