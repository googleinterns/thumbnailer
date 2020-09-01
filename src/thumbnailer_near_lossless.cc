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

Thumbnailer::Status Thumbnailer::NearLosslessDiff(WebPData* const webp_data) {
  int anim_size = GetAnimationSize(webp_data);

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

    if (final_near_ll == -1) {
      frame.config.lossless = 0;
      frame.config.quality = frame.final_quality;
    } else {
      frame.config.near_lossless = final_near_ll;
    }

    ++curr_ind;
  }

  std::cerr << "Final near-lossless's pre-processing values:" << std::endl;
  for (const auto& frame : frames_) {
    if (frame.config.lossless == 0) {
      std::cerr << -1 << ' ';
    } else {
      std::cerr << frame.config.near_lossless << ' ';
    }
  }
  std::cerr << std::endl;

  WebPDataClear(webp_data);
  CHECK_THUMBNAILER_STATUS(GenerateAnimationNoBudget(webp_data));

  return (webp_data->size > 0) ? kOk : kByteBudgetError;
}

Thumbnailer::Status Thumbnailer::NearLosslessEqual(WebPData* const webp_data) {
  const int num_frames = frames_.size();
  // anim_config_.allow_mixed = 1;
  // Encode frames following the ascending order of frame sizes.
  std::pair<int, int> encoding_order[num_frames];
  for (int i = 0; i < num_frames; ++i) {
    encoding_order[i] = std::make_pair(frames_[i].encoded_size, i);
  }
  std::sort(encoding_order, encoding_order + num_frames,
            [](const std::pair<int, int>& x, const std::pair<int, int>& y) {
              return x.first < y.first;
            });

  // Vector of frames encoded with near-lossless preprocessing 0.
  std::vector<int> near_ll_frames;
  int anim_size = GetAnimationSize(webp_data);
  // Find the maximum number of frames that can be encoded with near-lossless
  // preprocessing 0.
  for (int i = 0; i < num_frames; ++i) {
    const int curr_ind = encoding_order[i].second;
    frames_[curr_ind].config.lossless = 1;
    frames_[curr_ind].config.quality = 90;
    frames_[curr_ind].config.near_lossless = 0;
    int new_size;
    float new_psnr;
    CHECK_THUMBNAILER_STATUS(GetPictureStats(curr_ind, &new_size, &new_psnr));
    const int new_anim_size =
        anim_size - frames_[curr_ind].encoded_size + new_size;
    if (new_psnr >= frames_[curr_ind].final_psnr &&
        new_anim_size <= byte_budget_) {
      anim_size = new_anim_size;
      near_ll_frames.push_back(curr_ind);
      frames_[curr_ind].encoded_size = new_size;
      frames_[curr_ind].final_psnr = new_psnr;
      frames_[curr_ind].final_quality = 90;
      frames_[curr_ind].near_lossless = true;
    } else {
      frames_[curr_ind].config.lossless = 0;
      frames_[curr_ind].config.quality = frames_[curr_ind].final_quality;
    }
  }

  if (near_ll_frames.empty()) {
    std::cerr << "No near lossless frames to process." << std::endl;
    return kOk;
  }

  WebPDataClear(webp_data);
  CHECK_THUMBNAILER_STATUS(GenerateAnimationNoBudget(webp_data));

  // Use binary search to find the highest pre-processing value to encode all
  // frames in the 'near_ll_frames' vector.
  int min_near_ll = 1;
  int max_near_ll = 100;
  int final_near_ll = 0;
  while (min_near_ll <= max_near_ll) {
    anim_size = GetAnimationSize(webp_data);
    int mid_near_lossless = (min_near_ll + max_near_ll) / 2;
    // vector containing pair of (new size, new psnr) for all frames in the
    // 'near_ll_frames' vector.
    std::vector<std::pair<int, float>> new_size_psnr;

    for (int curr_ind : near_ll_frames) {
      frames_[curr_ind].config.near_lossless = mid_near_lossless;
      int new_size;
      float new_psnr;
      CHECK_THUMBNAILER_STATUS(GetPictureStats(curr_ind, &new_size, &new_psnr));
      const int new_anim_size =
          anim_size - frames_[curr_ind].encoded_size + new_size;
      if (new_psnr >= frames_[curr_ind].final_psnr &&
          new_anim_size <= byte_budget_) {
        new_size_psnr.push_back(std::make_pair(new_size, new_psnr));
        anim_size = new_anim_size;
      } else {
        break;
      }
    }

    if (new_size_psnr.size() == near_ll_frames.size()) {
      WebPDataClear(webp_data);
      CHECK_THUMBNAILER_STATUS(GenerateAnimationNoBudget(webp_data));
      for (int i = 0; i < new_size_psnr.size(); ++i) {
        const int curr_ind = near_ll_frames[i];
        frames_[curr_ind].encoded_size = new_size_psnr[i].first;
        frames_[curr_ind].final_psnr = new_size_psnr[i].second;
      }
      final_near_ll = mid_near_lossless;
      min_near_ll = mid_near_lossless + 1;
    } else {
      max_near_ll = mid_near_lossless - 1;
    }
  }

  std::cerr << "Final near-lossless pre-processing value: " << final_near_ll
            << std::endl;

  for (int curr_ind : near_ll_frames) {
    frames_[curr_ind].config.near_lossless = final_near_ll;
  }

  return (webp_data->size > 0) ? kOk : kByteBudgetError;
}

}  // namespace libwebp
