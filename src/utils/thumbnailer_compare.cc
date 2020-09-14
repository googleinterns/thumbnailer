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

#include <fstream>
#include <string>

#include "../thumbnailer.h"
#include "thumbnailer_utils.h"

int main(int argc, char* argv[]) {
  if (argc == 1) {
    return 0;
  }
  libwebp::UtilsOption option;
  std::string list_filename;
  for (int c = 1; c < argc; ++c) {
    if (!strcmp(argv[c], "-short")) {
      option.short_output = true;
    } else {
      list_filename = argv[c];
    }
  }

  // Process list of images and timestamps.
  std::vector<libwebp::Frame> frames;
  std::ifstream input_list(list_filename);
  std::string frame_filename;
  int timestamp;
  while (input_list >> frame_filename >> timestamp) {
    frames.push_back(
        {EnclosedWebPPicture(new WebPPicture, libwebp::WebPPictureDelete),
         timestamp});
    WebPPicture* pic = frames.back().pic.get();
    WebPPictureInit(pic);
    if (!libwebp::ReadPicture(frame_filename.c_str(), pic)) {
      std::cerr << "Failed to read image " << frame_filename << std::endl;
      return 1;
    }
  }
  if (frames.empty()) {
    std::cerr << "No input frame(s) for generating animation." << std::endl;
    return 1;
  }

  // Generate reference thumbnail.
  libwebp::Thumbnailer thumbnailer_ref;
  for (const libwebp::Frame& frame : frames) {
    if (thumbnailer_ref.AddFrame(*frame.pic, frame.timestamp) !=
        libwebp::Thumbnailer::Status::kOk) {
      std::cerr << "Error adding frames." << std::endl;
      return 1;
    }
  }
  WebPData webp_data_ref;
  WebPDataInit(&webp_data_ref);
  if (thumbnailer_ref.GenerateAnimation(
          &webp_data_ref, libwebp::Thumbnailer::Method::kEqualQuality) !=
      libwebp::Thumbnailer::Status::kOk) {
    std::cerr << "Error generating reference thumbnail." << std::endl;
    WebPDataClear(&webp_data_ref);
    return 1;
  }

  // Generate new thumbnails and compare to the reference thumbnail.
  for (const libwebp::Thumbnailer::Method& method :
       libwebp::Thumbnailer::kMethodList) {
    if (!option.short_output) {
      std::cout << std::endl
                << "----- Method " << method << " -----" << std::endl;
    }
    libwebp::Thumbnailer thumbnailer;

    libwebp::Thumbnailer::Status status = libwebp::Thumbnailer::Status::kOk;
    for (const libwebp::Frame& frame : frames) {
      status = thumbnailer.AddFrame(*frame.pic, frame.timestamp);
      if (status != libwebp::Thumbnailer::Status::kOk) {
        break;
      }
    }
    if (status != libwebp::Thumbnailer::Status::kOk) {
      std::cerr << "Error adding frames." << std::endl;
      continue;
    }

    WebPData webp_data;
    WebPDataInit(&webp_data);
    if (thumbnailer.GenerateAnimation(&webp_data, method) ==
        libwebp::Thumbnailer::Status::kOk) {
      libwebp::ThumbnailDiffPSNR diff;
      if (libwebp::CompareThumbnail(frames, &webp_data_ref, &webp_data,
                                    &diff) == libwebp::UtilsStatus::kOk) {
        libwebp::PrintThumbnailDiffPSNR(diff, option);
      } else {
        std::cerr << "Comparison failed." << std::endl;
      }
    } else {
      std::cerr << "Error generating thumbnail." << std::endl;
    }
    WebPDataClear(&webp_data);
  }

  WebPDataClear(&webp_data_ref);
  return 0;
}
