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

Thumbnailer::Status Thumbnailer::GenerateAnimationSlopeOptim(
    WebPData* const webp_data) {
  Thumbnailer::Status anim_status = LossyEncodeSlopeOptim(webp_data);
  if (anim_status == kOk) {
    anim_status = TryNearLossless(webp_data);
  }

  if (anim_status == kOk) {
    anim_status = LossyEncodeNoSlopeOptim(webp_data);
  }

  return anim_status;
}

Thumbnailer::Status Thumbnailer::FindMedianSlope(float* const median_slope) {
  std::vector<float> slopes;

  for (auto& frame : frames_) {
    frame.config.quality = 100;
    float psnr_100;  // pic's psnr value with quality = 100.
    int size_100;    // pic'size with quality = 100.
    if (GetPictureStats(frame.pic, frame.config, &size_100, &psnr_100) != kOk) {
      return kStatsError;
    };

    int min_quality = 0;
    int max_quality = 100;
    float pic_final_slope;

    // Use binary search to find the leftmost point on the curve so that the
    // difference in PSNR between this point and the one with quality value 100
    // is roughly 1.
    while (min_quality <= max_quality) {
      int mid_quality = (min_quality + max_quality) / 2;
      frame.config.quality = mid_quality;
      float new_psnr;
      int new_size;

      if (GetPictureStats(frame.pic, frame.config, &new_size, &new_psnr) !=
          kOk) {
        return kStatsError;
      }

      if (psnr_100 - new_psnr <= 1.0) {
        pic_final_slope = (psnr_100 - new_psnr) / float(size_100 - new_size);
        max_quality = mid_quality - 1;
      } else {
        min_quality = mid_quality + 1;
      }
    }

    slopes.push_back(pic_final_slope);
  }

  std::sort(slopes.begin(), slopes.end());
  *median_slope = slopes[slopes.size() / 2];

  return kOk;
}

Thumbnailer::Status Thumbnailer::ComputeSlope(const int& ind,
                                              const int& low_quality,
                                              const int& high_quality,
                                              float* const slope) {
  frames_[ind].config.quality = low_quality;
  int low_size;
  float low_psnr;
  if (GetPictureStats(frames_[ind].pic, frames_[ind].config, &low_size,
                      &low_psnr) != kOk) {
    return kStatsError;
  }

  frames_[ind].config.quality = high_quality;
  int high_size;
  float high_psnr;
  if (GetPictureStats(frames_[ind].pic, frames_[ind].config, &high_size,
                      &high_psnr) != kOk) {
    return kStatsError;
  }

  *slope = (high_psnr - low_psnr) / float(high_size - low_size);

  return kOk;
}

Thumbnailer::Status Thumbnailer::LossyEncodeSlopeOptim(
    WebPData* const webp_data) {
  // Sort frames.
  std::sort(frames_.begin(), frames_.end(),
            [](const FrameData& a, const FrameData& b) -> bool {
              return a.timestamp_ms < b.timestamp_ms;
            });

  float limit_slope;
  if (FindMedianSlope(&limit_slope) != kOk) {
    return kSlopeOptimError;
  }

  int min_quality = minimum_lossy_quality_;
  int max_quality = 100;
  WebPData new_webp_data;
  WebPDataInit(&new_webp_data);

  std::queue<int> optim_list;  // Queue of frames needed to find the quality in
                               // the next binary search loop.
  for (int i = 0; i < frames_.size(); i++) {
    optim_list.push(i);
  }

  // Use binary search with slope optimization to find quality values that makes
  // the animation fit the given byte budget. The quality value for each frame
  // can be different.
  while (min_quality <= max_quality && !optim_list.empty()) {
    int mid_quality = (min_quality + max_quality) / 2;

    std::queue<int> new_optim_list;

    while (!optim_list.empty()) {
      int curr_frame = optim_list.front();
      optim_list.pop();

      float curr_slope;
      if (ComputeSlope(curr_frame, min_quality, max_quality, &curr_slope) !=
          kOk) {
        return kSlopeOptimError;
      }

      if (frames_[curr_frame].final_quality == -1 || curr_slope > limit_slope) {
        frames_[curr_frame].config.quality = mid_quality;
        new_optim_list.push(curr_frame);
      }
    }

    if (new_optim_list.empty()) break;

    if (GenerateAnimationNoBudget(&new_webp_data) != kOk) {
      return kMemoryError;
    }

    if (new_webp_data.size <= byte_budget_) {
      while (!new_optim_list.empty()) {
        int curr_frame = new_optim_list.front();
        new_optim_list.pop();
        frames_[curr_frame].final_quality = mid_quality;
        optim_list.push(curr_frame);
      }
      WebPDataClear(webp_data);
      *webp_data = new_webp_data;
      min_quality = mid_quality + 1;
    } else {
      optim_list = new_optim_list;
      max_quality = mid_quality - 1;
      WebPDataClear(&new_webp_data);
    }
  }

  std::cout << "Final qualities with slope optimization: ";

  for (auto& frame : frames_) {
    frame.config.quality = frame.final_quality;
    if (GetPictureStats(frame.pic, frame.config, &frame.encoded_size,
                        &frame.final_psnr) != kOk) {
      return kStatsError;
    }
    std::cerr << frame.config.quality << " ";
  }

  std::cout << std::endl;

  return (webp_data->size > 0) ? kOk : kByteBudgetError;
}

Thumbnailer::Status Thumbnailer::LossyEncodeNoSlopeOptim(
    WebPData* const webp_data) {
  int anim_size = 0;
  for (auto& frame : frames_) {
    anim_size += frame.encoded_size;
  }

  WebPData new_webp_data;
  WebPDataInit(&new_webp_data);

  // For each frame, find the best quality value that can produce the higher
  // PSNR than the current one if possible and make the animation fit right
  // below the byte budget.
  for (auto& frame : frames_) {
    int min_quality = 0;
    int max_quality = 100;
    frame.config.lossless = 0;
    while (min_quality <= max_quality) {
      int mid_quality = (min_quality + max_quality) / 2;
      int new_size;
      float new_psnr;
      frame.config.quality = mid_quality;
      if (GetPictureStats(frame.pic, frame.config, &new_size, &new_psnr) !=
          kOk) {
        return kStatsError;
      }

      if (new_psnr > frame.final_psnr || ((new_psnr == frame.final_psnr) &&
                                          (new_size <= frame.encoded_size))) {
        if (anim_size - frame.encoded_size + new_size <= byte_budget_) {
          anim_size = anim_size - frame.encoded_size + new_size;
          frame.encoded_size = new_size;
          frame.final_psnr = new_psnr;
          frame.final_quality = mid_quality;
          frame.near_lossless = false;
          min_quality = mid_quality + 1;
        } else {
          max_quality = mid_quality - 1;
        }
      } else {
        min_quality = mid_quality + 1;
      }
    }
  }

  for (auto& frame : frames_) {
    frame.config.quality = frame.final_quality;
    frame.config.lossless = frame.near_lossless;
  }

  if (GenerateAnimationNoBudget(&new_webp_data) != kOk) {
    return kMemoryError;
  }

  std::cout << std::endl << "(Final quality, Near-lossless) :" << std::endl;
  for (auto& frame : frames_) {
    std::cerr << "(" << frame.final_quality << ", " << frame.near_lossless
              << ") ";
  }
  std::cout << std::endl;

  return (webp_data->size > 0) ? kOk : kByteBudgetError;
}
}  // namespace libwebp
