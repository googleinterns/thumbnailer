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
  // Process list of images and timestamps.
  std::vector<libwebp::Frame> frames;
  std::ifstream input_list(argv[1]);
  std::string filename_str;
  int timestamp;
  while (input_list >> filename_str >> timestamp) {
    frames.push_back(
        {EnclosedWebPPicture(new WebPPicture, WebPPictureFree), timestamp});
    WebPPicture* pic = frames.back().pic.get();
    WebPPictureInit(pic);
    ReadImage(filename_str.c_str(), pic);
  }
  if (frames.empty()) {
    std::cerr << "No input file(s) for generating animation." << std::endl;
    return 1;
  }

  // Generate reference thumbnail.
  libwebp::Thumbnailer thumbnailer_ref;
  for (libwebp::Frame& frame : frames) {
    thumbnailer_ref.AddFrame(*frame.pic, frame.timestamp);
  }
  WebPData webp_data_ref;
  WebPDataInit(&webp_data_ref);
  thumbnailer_ref.GenerateAnimation(&webp_data_ref);

  // Generate new thumbnails and compare to the reference thumbnail.
  int options_count = 1;
  WebPData webp_data;
  WebPDataInit(&webp_data);
  for (int i = 0; i < options_count; ++i) {
    libwebp::Thumbnailer thumbnailer;
    for (libwebp::Frame& frame : frames) {
      thumbnailer.AddFrame(*frame.pic, frame.timestamp);
    }
    thumbnailer.GenerateAnimationEqualPSNR(&webp_data);
    libwebp::ThumbnailDiffPSNR diff;
    libwebp::CompareThumbnail(frames, &webp_data_ref, &webp_data, &diff);
    libwebp::PrintThumbnailDiffPSNR(diff);
    WebPDataClear(&webp_data);
  }

  WebPDataClear(&webp_data_ref);
  return 0;
}