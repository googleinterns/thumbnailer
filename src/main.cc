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

#include "class.h"

// Return true on success or false on failure.
bool ReadImage(const char filename[], WebPPicture* const pic) {
  const uint8_t* data = NULL;
  size_t data_size = 0;
  WebPImageReader reader;
  if (!ImgIoUtilReadFile(filename, &data, &data_size)) goto out;
  reader = WebPGuessImageReader(data, data_size);
  if (!reader(data, data_size, pic, 1, NULL)) goto out;
  return true;
out:
  free((void*)data);
  return false;
}

int main(int argc, char* argv[]) {
  // TODO: read images given as arguments
  // TODO: create an animation
  // TODO: write the animation to disk

  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();
  WebPPicture pic;
  WebPData webp_data;

  WebPDataInit(&webp_data);
  WebPPictureInit(&pic);

  std::ifstream input_list(argv[1]);
  const char* output = argv[2];
  std::string filename_str;
  int timestamp_ms;

  while (input_list >> filename_str >> timestamp_ms) {
    char filename[filename_str.length()];
    strcpy(filename, filename_str.c_str());
    std::cout << ReadImage(filename, &pic);
    thumbnailer.AddFrame(pic, timestamp_ms);
  }

  // thumbnailer.GenerateAnimation(&webp_data);
  // ImgIoUtilWriteFile(output, webp_data.bytes, webp_data.size);

  std::cout << "Compressing: hll wrld" << std::endl;
  return 0;
}
