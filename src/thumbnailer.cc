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

#define CONVERT_WEBP_MUX_STATUS(webp_mux_error)     \
  do {                                              \
    const WebPMuxError error = (webp_mux_error);    \
    if (error != WEBP_MUX_OK) return kWebPMuxError; \
  } while (0);

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

  // No inter-frames.
  anim_config_.kmax = 1;
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

Thumbnailer::Status Thumbnailer::GetPictureStats(int ind, int* const pic_size,
                                                 float* const pic_PSNR) {
  const int quality = int(frames_[ind].config.quality);
  if (!frames_[ind].config.lossless &&
      frames_[ind].lossy_data[quality].first != -1) {
    *pic_size = frames_[ind].lossy_data[quality].first;
    *pic_PSNR = frames_[ind].lossy_data[quality].second;
    return kOk;
  }

  WebPPicture new_pic;
  if (!WebPPictureCopy(&frames_[ind].pic, &new_pic)) {
    WebPPictureFree(&new_pic);
    return kStatsError;
  }

  WebPAuxStats stats;
  new_pic.stats = &stats;
  if (!WebPEncode(&frames_[ind].config, &new_pic)) {
    WebPPictureFree(&new_pic);
    return kStatsError;
  }

  *pic_size = new_pic.stats->coded_size;

  float distortion_result[5];
  if (!WebPPictureDistortion(&frames_[ind].pic, &new_pic, 0,
                             distortion_result)) {
    WebPPictureFree(&new_pic);
    return kStatsError;
  } else {
    *pic_PSNR = distortion_result[4];  // PSNR-all.
  }

  if (!frames_[ind].config.lossless) {
    frames_[ind].lossy_data[quality] = std::make_pair(*pic_size, *pic_PSNR);
  }
  WebPPictureFree(&new_pic);

  return kOk;
}

int Thumbnailer::GetAnimationSize(WebPData* const webp_data) {
  // The webp_data->size and the sum of encoded-frame sizes are inconsistent,
  // therefore consider the bigger one as the current animation size to ensure
  // the resulting animation size fit the byte budget.
  int sum_frame_sizes = 0;
  for (auto& frame : frames_) {
    sum_frame_sizes += frame.encoded_size;
  }
  return std::max(sum_frame_sizes, int(webp_data->size));
}

Thumbnailer::Status Thumbnailer::SetLoopCount(WebPData* const webp_data) {
  std::unique_ptr<WebPMux, void (*)(WebPMux*)> mux(WebPMuxCreate(webp_data, 1),
                                                   WebPMuxDelete);
  if (mux == nullptr) return kWebPMuxError;

  WebPMuxAnimParams new_params;
  CONVERT_WEBP_MUX_STATUS(WebPMuxGetAnimationParams(mux.get(), &new_params));

  new_params.loop_count = loop_count_;
  CONVERT_WEBP_MUX_STATUS(WebPMuxSetAnimationParams(mux.get(), &new_params));

  WebPDataClear(webp_data);
  CONVERT_WEBP_MUX_STATUS(WebPMuxAssemble(mux.get(), webp_data));

  return kOk;
}

Thumbnailer::Status Thumbnailer::GenerateAnimation(WebPData* const webp_data,
                                                   Method method) {
  if (method == kEqualQuality) {
    return GenerateAnimationEqualQuality(webp_data);
  } else if (method == kEqualPSNR) {
    return GenerateAnimationEqualPSNR(webp_data);
  } else if (method == kSlopeOptim) {
    return GenerateAnimationSlopeOptim(webp_data);
  } else {
    std::cerr << "Invalid method." << std::endl;
    return kGenericError;
  }
}

Thumbnailer::Status Thumbnailer::GenerateAnimationNoBudget(
    WebPData* const webp_data) {
  // Delete the previous WebPAnimEncoder object and initialize a new one.
  WebPAnimEncoderDelete(enc_);
  enc_ = WebPAnimEncoderNew(frames_[0].pic.width, frames_[0].pic.height,
                            &anim_config_);
  if (enc_ == nullptr) return kMemoryError;

  // Fill the animation.
  int prev_timestamp = 0;
  for (auto& frame : frames_) {
    // Copy the 'frame.pic' to a new WebPPicture object and remain the original
    // 'frame.pic' for later comparison.
    WebPPicture new_pic;

    // WebPAnimEncoderAdd uses starting timestamps instead of ending timestamps.
    if (!WebPPictureCopy(&frame.pic, &new_pic) ||
        !WebPAnimEncoderAdd(enc_, &new_pic, prev_timestamp, &frame.config)) {
      WebPPictureFree(&new_pic);
      return kMemoryError;
    }
    WebPPictureFree(&new_pic);
    prev_timestamp = frame.timestamp_ms;
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

Thumbnailer::Status Thumbnailer::GenerateAnimationEqualQuality(
    WebPData* const webp_data) {
  // Sort frames.
  std::sort(frames_.begin(), frames_.end(),
            [](const FrameData& a, const FrameData& b) -> bool {
              return a.timestamp_ms < b.timestamp_ms;
            });

  for (auto& frame : frames_) {
    // Initialize 'lossy_data' array.
    std::fill(frame.lossy_data, frame.lossy_data + 101,
              std::make_pair(-1, -1.0));
  }

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

    CHECK_THUMBNAILER_STATUS(GenerateAnimationNoBudget(&new_webp_data));

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

  int curr_ind = 0;
  for (auto& frame : frames_) {
    frame.final_quality = final_quality;
    frame.config.quality = final_quality;
    CHECK_THUMBNAILER_STATUS(
        GetPictureStats(curr_ind++, &frame.encoded_size, &frame.final_psnr));
  }

  std::cout << "Final quality: " << final_quality << std::endl;

  return (final_quality == -1) ? kByteBudgetError : kOk;
}

Thumbnailer::Status Thumbnailer::GenerateAnimationEqualPSNR(
    WebPData* const webp_data) {
  CHECK_THUMBNAILER_STATUS(GenerateAnimationEqualQuality(webp_data));

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
    bool all_frames_iterated = true;

    WebPAnimEncoderDelete(enc_);
    enc_ = WebPAnimEncoderNew(frames_[0].pic.width, frames_[0].pic.height,
                              &anim_config_);
    if (enc_ == nullptr) return kMemoryError;

    int curr_ind = 0;
    // For each frame, find the quality value that produces WebPPicture
    // having PSNR close to target_psnr.
    int prev_timestamp = 0;
    for (auto& frame : frames_) {
      int frame_min_quality = 0;
      int frame_max_quality = 100;
      int frame_final_quality = -1;

      float frame_lowest_psnr;
      float frame_highest_psnr;
      int current_size;
      frame.config.quality = 0;
      CHECK_THUMBNAILER_STATUS(
          GetPictureStats(curr_ind, &current_size, &frame_lowest_psnr));
      frame.config.quality = 100;
      CHECK_THUMBNAILER_STATUS(
          GetPictureStats(curr_ind, &current_size, &frame_highest_psnr));

      // Target PSNR is out of range.
      if (target_psnr > std::floor(frame_highest_psnr) ||
          target_psnr < std::floor(frame_lowest_psnr)) {
        all_frames_iterated = false;
        break;
      }

      // Binary search for quality value.
      while (frame_min_quality <= frame_max_quality) {
        int frame_mid_quality = (frame_min_quality + frame_max_quality) / 2;
        frame.config.quality = frame_mid_quality;
        float current_psnr;
        CHECK_THUMBNAILER_STATUS(
            GetPictureStats(curr_ind, &current_size, &current_psnr));
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
          !WebPAnimEncoderAdd(enc_, &new_pic, prev_timestamp, &frame.config)) {
        WebPPictureFree(&new_pic);
        return kMemoryError;
      }
      WebPPictureFree(&new_pic);
      prev_timestamp = frame.timestamp_ms;
      ++curr_ind;
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

        int curr_ind = 0;
        for (auto& frame : frames_) {
          CHECK_THUMBNAILER_STATUS(GetPictureStats(
              curr_ind++, &frame.encoded_size, &frame.final_psnr));
          frame.final_quality = frame.config.quality;
        }

        break;
      } else {
        WebPDataClear(&new_webp_data);
      }
    }
  }

  std::cout << "Final PSNR: " << final_psnr << std::endl;

  if (loop_count_ == 0) return kOk;
  return SetLoopCount(webp_data);
}

}  // namespace libwebp
