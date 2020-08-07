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

#ifndef THUMBNAILER_SRC_CLASS_H_
#define THUMBNAILER_SRC_CLASS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "../imageio/image_dec.h"
#include "../imageio/imageio_util.h"
#include "thumbnailer.pb.h"
#include "webp/encode.h"
#include "webp/mux.h"

namespace libwebp {

// Takes time stamped images as an input and produces an animation.
class Thumbnailer {
 public:
  Thumbnailer();
  Thumbnailer(const thumbnailer::ThumbnailerOption& thumbnailer_option);
  ~Thumbnailer();

  // Status codes for adding frame and generating animation.
  enum Status {
    kOk = 0,            // On success.
    kMemoryError,       // In case of memory error.
    kImageFormatError,  // If frame dimensions are mismatched.
    kByteBudgetError  // If there is no quality that makes the animation fit the
                      // byte budget.
  };

  // Adds a frame with a timestamp (in millisecond). The 'pic' argument must
  // outlive the last GenerateAnimation() call.
  Status AddFrame(const WebPPicture& pic, int timestamp_ms);

  // Generates the animation.
  Status GenerateAnimationNoBudget(WebPData* const webp_data);

  // Finds the best quality that makes the animation fit right below the given
  // byte budget and generates the animation. The 'webp_data' argument is
  // expected to be initialized (otherwise WebPDataClear() might free some
  // random memory somewhere because the pointer is undefined).
  Status GenerateAnimation(WebPData* const webp_data);

 private:
  struct FrameData {
    WebPPicture pic;
    int timestamp_ms;
  };
  std::vector<FrameData> frames_;
  WebPAnimEncoder* enc_ = NULL;
  WebPAnimEncoderOptions anim_config_;
  WebPConfig config_;
  int loop_count_;
  int byte_budget_;
  int minimum_lossy_quality_;
};

}  // namespace libwebp

#endif  // THUMBNAILER_SRC_CLASS_H_
