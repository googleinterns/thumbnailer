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

Thumbnailer::Thumbnailer() {
  WebPAnimEncoderOptionsInit(&anim_config_);
  loop_count_ = 0;
  byte_budget_ = 153600;
  minimum_lossy_quality_ = 0;
}

Thumbnailer::Thumbnailer(
    const thumbnailer::ThumbnailerOption& thumbnailer_option) {
  WebPAnimEncoderOptionsInit(&anim_config_);
  loop_count_ = thumbnailer_option.loop_count();
  byte_budget_ = thumbnailer_option.soft_max_size();
  minimum_lossy_quality_ = thumbnailer_option.min_lossy_quality();
  if (thumbnailer_option.allow_mixed()) {
    anim_config_.allow_mixed = 1;
  }
}

Thumbnailer::~Thumbnailer() { WebPAnimEncoderDelete(enc_); }

Thumbnailer::Status Thumbnailer::AddFrame(const WebPPicture& pic,
                                          int timestamp_ms) {
  // Verify dimension of frames.
  if (!frames_.empty() && (pic.width != frames_[0].pic.width ||
                           pic.height != frames_[0].pic.height)) {
    return kImageFormatError;
  }
  WebPConfig new_config;
  if (!WebPConfigInit(&new_config)) assert(false);
  new_config.show_compressed = 1;
  frames_.push_back({pic, timestamp_ms, new_config});
  return kOk;
}

Thumbnailer::Status Thumbnailer::GetPictureStats(const WebPPicture& pic,
                                                 const WebPConfig& config,
                                                 int* const pic_size,
                                                 float* const pic_PSNR) {
  WebPPicture new_pic;
  if (!WebPPictureCopy(&pic, &new_pic)) {
    WebPPictureFree(&new_pic);
    return kStatsError;
  }

  WebPAuxStats stats;
  new_pic.stats = &stats;
  if (!WebPEncode(&config, &new_pic)) {
    WebPPictureFree(&new_pic);
    return kStatsError;
  }

  *pic_size = new_pic.stats->coded_size;

  float distortion_result[5];
  if (!WebPPictureDistortion(&pic, &new_pic, 0, distortion_result)) {
    WebPPictureFree(&new_pic);
    return kStatsError;
  } else {
    *pic_PSNR = distortion_result[4];  // PSNR-all.
  }

  WebPPictureFree(&new_pic);

  return kOk;
}

Thumbnailer::Status Thumbnailer::SetLoopCount(WebPData* const webp_data) {
  std::unique_ptr<WebPMux, void (*)(WebPMux*)> mux(WebPMuxCreate(webp_data, 1),
                                                   WebPMuxDelete);
  if (mux == nullptr) return kWebPMuxError;

  WebPMuxAnimParams new_params;
  if (WebPMuxGetAnimationParams(mux.get(), &new_params) != WEBP_MUX_OK) {
    return kWebPMuxError;
  }

  new_params.loop_count = loop_count_;
  if (WebPMuxSetAnimationParams(mux.get(), &new_params) != WEBP_MUX_OK) {
    return kWebPMuxError;
  }

  WebPDataClear(webp_data);
  if (WebPMuxAssemble(mux.get(), webp_data) != WEBP_MUX_OK) {
    return kWebPMuxError;
  }

  return kOk;
}

Thumbnailer::Status Thumbnailer::GenerateAnimationNoBudget(
    WebPData* const webp_data) {
  // Delete the previous WebPAnimEncoder object and initialize a new one.
  WebPAnimEncoderDelete(enc_);
  enc_ = WebPAnimEncoderNew(frames_[0].pic.width, frames_[0].pic.height,
                            &anim_config_);
  if (enc_ == nullptr) return kMemoryError;

  // Fill the animation.
  for (auto& frame : frames_) {
    // Copy the 'frame.pic' to a new WebPPicture object and remain the original
    // 'frame.pic' for later comparison.
    WebPPicture new_pic;
    if (!WebPPictureCopy(&frame.pic, &new_pic) ||
        !WebPAnimEncoderAdd(enc_, &new_pic, frame.timestamp_ms,
                            &frame.config)) {
      WebPPictureFree(&new_pic);
      return kMemoryError;
    }
    WebPPictureFree(&new_pic);
  }

  // Add last frame.
  if (!WebPAnimEncoderAdd(enc_, NULL, frames_.back().timestamp_ms, NULL)) {
    return kMemoryError;
  }

  if (!WebPAnimEncoderAssemble(enc_, webp_data)) {
    return kMemoryError;
  }

  if (loop_count_ == 0) return kOk;
  return SetLoopCount(webp_data);
}

Thumbnailer::Status Thumbnailer::GenerateAnimation(WebPData* const webp_data) {
  // Sort frames.
  std::sort(frames_.begin(), frames_.end(),
            [](const FrameData& a, const FrameData& b) -> bool {
              return a.timestamp_ms < b.timestamp_ms;
            });

  // Use binary search to find the quality that makes the animation fit right
  // below the given byte budget.
  int min_quality = minimum_lossy_quality_;
  int max_quality = 100;
  int final_quality = -1;
  WebPData new_webp_data;
  WebPDataInit(&new_webp_data);

  while (min_quality <= max_quality) {
    int mid_quality = (min_quality + max_quality) / 2;
    for (auto& frame : frames_) {
      frame.config.quality = mid_quality;
    }

    if (GenerateAnimationNoBudget(&new_webp_data) != kOk) {
      return kMemoryError;
    }

    if (new_webp_data.size <= byte_budget_) {
      final_quality = mid_quality;
      WebPDataClear(webp_data);
      *webp_data = new_webp_data;
      min_quality = mid_quality + 1;
    } else {
      max_quality = mid_quality - 1;
      WebPDataClear(&new_webp_data);
    }
  }

  for (auto& frame : frames_) {
    frame.final_quality = final_quality;
    frame.config.quality = final_quality;
    if (GetPictureStats(frame.pic, frame.config, &frame.encoded_size,
                        &frame.final_psnr) != kOk) {
      return kStatsError;
    }
  }

  std::cout << "Final quality: " << final_quality << std::endl;

  return (final_quality == -1) ? kByteBudgetError : kOk;
}

Thumbnailer::Status Thumbnailer::GenerateAnimationEqualPSNR(
    WebPData* const webp_data) {
  if (GenerateAnimation(webp_data) != kOk) {
    return kByteBudgetError;
  }

  int high_psnr = -1;
  int low_psnr = -1;
  int final_psnr = -1;

  // Find PSNR search range.
  for (auto& frame : frames_) {
    int frame_psnr = std::floor(frame.final_psnr);
    if (high_psnr == -1 || frame_psnr > high_psnr) {
      high_psnr = frame_psnr;
    }
    if (low_psnr == -1 || frame_psnr < low_psnr) {
      low_psnr = frame_psnr;
    }
  }

  for (int target_psnr = high_psnr; target_psnr >= low_psnr; --target_psnr) {
    std::cerr << "Target PSNR: " << target_psnr << ". Quality: ";
    bool all_frames_iterated = true;

    WebPAnimEncoderDelete(enc_);
    enc_ = WebPAnimEncoderNew(frames_[0].pic.width, frames_[0].pic.height,
                              &anim_config_);
    if (enc_ == nullptr) return kMemoryError;
    // For each frame, find the quality value that produces WebPPicture
    // having PSNR close to target_psnr.
    for (auto& frame : frames_) {
      int frame_min_quality = 0;
      int frame_max_quality = 100;
      int frame_final_quality = -1;

      float frame_lowest_psnr;
      float frame_highest_psnr;
      int current_size;
      frame.config.quality = 0;
      if (GetPictureStats(frame.pic, frame.config, &current_size,
                          &frame_lowest_psnr) != kOk) {
        return kStatsError;
      }
      frame.config.quality = 100;
      if (GetPictureStats(frame.pic, frame.config, &current_size,
                          &frame_highest_psnr) != kOk) {
        return kStatsError;
      }

      // Target PSNR is out of range.
      if (target_psnr > std::floor(frame_highest_psnr) ||
          target_psnr < std::floor(frame_lowest_psnr)) {
        all_frames_iterated = false;
        std::cerr << "Target PSNR is out of range." << std::endl;
        break;
      }

      // Binary search for quality value.
      while (frame_min_quality <= frame_max_quality) {
        int frame_mid_quality = (frame_min_quality + frame_max_quality) / 2;
        frame.config.quality = frame_mid_quality;
        float current_psnr;
        if (GetPictureStats(frame.pic, frame.config, &current_size,
                            &current_psnr) != kOk) {
          return kStatsError;
        }
        if (std::floor(current_psnr) <= target_psnr) {
          frame_final_quality = frame_mid_quality;
          frame_min_quality = frame_mid_quality + 1;
        } else {
          frame_max_quality = frame_mid_quality - 1;
        }
      }

      frame.config.quality = frame_final_quality;

      WebPPicture new_pic;
      if (!WebPPictureCopy(&frame.pic, &new_pic) ||
          !WebPAnimEncoderAdd(enc_, &new_pic, frame.timestamp_ms,
                              &frame.config)) {
        WebPPictureFree(&new_pic);
        return kMemoryError;
      }
      WebPPictureFree(&new_pic);

      std::cerr << frame.config.quality << ' ';
    }

    // Add last frame.
    if (!WebPAnimEncoderAdd(enc_, NULL, frames_.back().timestamp_ms, NULL)) {
      return kMemoryError;
    }

    if (all_frames_iterated) {
      WebPData new_webp_data;
      WebPDataInit(&new_webp_data);
      if (!WebPAnimEncoderAssemble(enc_, &new_webp_data)) {
        return kMemoryError;
      }
      if (new_webp_data.size <= byte_budget_) {
        final_psnr = target_psnr;
        WebPDataClear(webp_data);
        *webp_data = new_webp_data;

        for (auto& frame : frames_) {
          if (GetPictureStats(frame.pic, frame.config, &frame.encoded_size,
                              &frame.final_psnr) != kOk) {
            return kStatsError;
          }
          frame.final_quality = frame.config.quality;
        }

        break;
      } else {
        WebPDataClear(&new_webp_data);
        std::cerr << std::endl;
      }
    }
  }

  std::cout << std::endl << "Final PSNR: " << final_psnr << std::endl;

  if (loop_count_ == 0) return kOk;
  return SetLoopCount(webp_data);
}

Thumbnailer::Status Thumbnailer::TryNearLossless(WebPData* const webp_data) {
  // The webp_data->size is not used here instead since it is higher than the
  // sum of encoded-frame sizes.
  int anim_size = 0;
  for (auto& frame : frames_) {
    anim_size += frame.encoded_size;
  }

  WebPAnimEncoderDelete(enc_);
  enc_ = WebPAnimEncoderNew(frames_[0].pic.width, frames_[0].pic.height,
                            &anim_config_);
  if (enc_ == nullptr) return kMemoryError;

  std::cerr << std::endl
            << "Final near-lossless's pre-processing values:" << std::endl;

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
    if (GetPictureStats(frame.pic, frame.config, &new_size, &new_psnr) != kOk) {
      return kStatsError;
    }

    // Only try binary search if near-lossles encoding with pre-processing = 0
    // is feasible in order to save execution time.
    if (anim_size - curr_size + new_size <= byte_budget_) {
      // Binary search for near-lossless's pre-processing value.
      while (min_near_ll <= max_near_ll) {
        int mid_near_ll = (min_near_ll + max_near_ll) / 2;
        frame.config.near_lossless = mid_near_ll;
        if (GetPictureStats(frame.pic, frame.config, &new_size, &new_psnr) !=
            kOk) {
          return kStatsError;
        }
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

    WebPPicture new_pic;
    if (!WebPPictureCopy(&frame.pic, &new_pic) ||
        !WebPAnimEncoderAdd(enc_, &new_pic, frame.timestamp_ms,
                            &frame.config)) {
      WebPPictureFree(&new_pic);
      return kMemoryError;
    }
    WebPPictureFree(&new_pic);
  }

  std::cerr << std::endl;

  // Add last frame.
  if (!WebPAnimEncoderAdd(enc_, NULL, frames_.back().timestamp_ms, NULL)) {
    return kMemoryError;
  }

  WebPDataClear(webp_data);
  if (!WebPAnimEncoderAssemble(enc_, webp_data)) {
    return kMemoryError;
  }

  if (loop_count_ == 0) return kOk;
  return SetLoopCount(webp_data);
}

}  // namespace libwebp
