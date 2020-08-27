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

#include "thumbnailer.h"

namespace libwebp {

Thumbnailer::Status Thumbnailer::TryNearLossless(WebPData* const webp_data) {
  int anim_size = GetAnimationSize(webp_data);

  std::cerr << std::endl
            << "Final near-lossless's pre-processing values:" << std::endl;

  int curr_ind = 0;
  for (auto& frame : frames_) {
    int curr_size = frame.encoded_size;
    float curr_psnr = frame.final_psnr;

    int min_near_ll = 0;
    int max_near_ll = 100;
    int final_near_ll = -1;

    frame.config.lossless = 1;
    frame.config.near_lossless = 0;
    frame.config.quality = 90;
    int new_size;
    float new_psnr;
    CHECK_THUMBNAILER_STATUS(GetPictureStats(curr_ind, &new_size, &new_psnr));

    // Only try binary search if near-lossles encoding with pre-processing = 0
    // is feasible in order to save execution time.
    if (anim_size - curr_size + new_size <= byte_budget_) {
      // Binary search for near-lossless's pre-processing value.
      while (min_near_ll <= max_near_ll) {
        int mid_near_ll = (min_near_ll + max_near_ll) / 2;
        frame.config.near_lossless = mid_near_ll;
        CHECK_THUMBNAILER_STATUS(
            GetPictureStats(curr_ind, &new_size, &new_psnr));
        if (anim_size - curr_size + new_size <= byte_budget_) {
          if (new_psnr > curr_psnr) {
            final_near_ll = mid_near_ll;
            frame.encoded_size = new_size;
            frame.final_psnr = new_psnr;
            frame.final_quality = frame.config.quality;
            frame.near_lossless = true;
            anim_size = anim_size - curr_size + new_size;
            curr_size = new_size;
            curr_psnr = new_psnr;
          }
          min_near_ll = mid_near_ll + 1;
        } else {
          max_near_ll = mid_near_ll - 1;
        }
      }
    }

    std::cerr << final_near_ll << " ";

    if (final_near_ll == -1) {
      frame.config.lossless = 0;
      frame.config.quality = frame.final_quality;
    } else {
      frame.config.near_lossless = final_near_ll;
    }

    ++curr_ind;
  }

  std::cerr << std::endl;

  WebPDataClear(webp_data);
  CHECK_THUMBNAILER_STATUS(GenerateAnimationNoBudget(webp_data));

  return (webp_data->size > 0) ? kOk : kByteBudgetError;
}

}  // namespace libwebp
