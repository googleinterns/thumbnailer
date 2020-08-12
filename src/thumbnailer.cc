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
  WebPConfigInit(&config_);
  loop_count_ = 0;
  byte_budget_ = 153600;
  minimum_lossy_quality_ = 0;
}

Thumbnailer::Thumbnailer(
    const thumbnailer::ThumbnailerOption& thumbnailer_option) {
  WebPAnimEncoderOptionsInit(&anim_config_);
  WebPConfigInit(&config_);
  loop_count_ = thumbnailer_option.loop_count();
  byte_budget_ = thumbnailer_option.soft_max_size();
  minimum_lossy_quality_ = thumbnailer_option.min_lossy_quality();
  if (thumbnailer_option.allow_mixed()) {
    anim_config_.allow_mixed = 1;
    config_.lossless = 0;
  }
}

Thumbnailer::~Thumbnailer() { WebPAnimEncoderDelete(enc_); }

Thumbnailer::Status Thumbnailer::AddFrame(const WebPPicture& pic,
                                          int timestamp_ms) {
  // Verify dimension of frames.
  if (!frames_.empty() && (pic.width != (frames_[0].pic).width ||
                           pic.height != (frames_[0].pic).height)) {
    return kImageFormatError;
  }
  frames_.push_back({pic, timestamp_ms});
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
    if (!WebPAnimEncoderAdd(enc_, &frame.pic, frame.timestamp_ms, &config_)) {
      return kMemoryError;
    }
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
    int middle = (min_quality + max_quality) / 2;
    config_.quality = middle;
    GenerateAnimationNoBudget(&new_webp_data);

    if (new_webp_data.size <= byte_budget_) {
      final_quality = middle;
      WebPDataClear(webp_data);
      *webp_data = new_webp_data;
      min_quality = middle + 1;
    } else {
      max_quality = middle - 1;
      WebPDataClear(&new_webp_data);
    }
  }

  config_.quality = final_quality;

  if (final_quality == -1) {
    return kByteBudgetError;
  }

  return kOk;
}

int Thumbnailer::GetPSNR(WebPPicture* const pic, int quality) {
  WebPPicture new_pic;
  WebPPictureInit(&new_pic);
  WebPPictureCopy(pic, &new_pic);

  WebPAuxStats stats;
  new_pic.stats = &stats;

  config_.quality = quality;
  WebPEncode(&config_, &new_pic);
  int result_psnr = std::round(new_pic.stats->PSNR[3]);  // PSNR-all.
  WebPPictureFree(&new_pic);

  return result_psnr;
}

Thumbnailer::Status Thumbnailer::GenerateAnimationEqualPSNR(
    WebPData* const webp_data) {
  GenerateAnimation(webp_data);
  if (config_.quality == -1) return kByteBudgetError;

  WebPData new_webp_data;
  WebPDataInit(&new_webp_data);

  int high_psnr = -1;
  int low_psnr = -1;
  int final_psnr = -1;

  // Find PSNR search range.
  for (auto& frame : frames_) {
    int frame_psnr = GetPSNR(&frame.pic, config_.quality);
    if (high_psnr == -1 || frame_psnr > high_psnr) {
      high_psnr = frame_psnr;
    }
    if (low_psnr == -1 || frame_psnr < low_psnr) {
      low_psnr = frame_psnr;
    }
  }

  for (int target_psnr = high_psnr; target_psnr >= low_psnr; --target_psnr) {
    std::cerr << "Target PSNR: " << target_psnr << ". ";
    bool ok = true;

    WebPAnimEncoderDelete(enc_);
    enc_ = WebPAnimEncoderNew(frames_[0].pic.width, frames_[0].pic.height,
                              &anim_config_);
    if (enc_ == nullptr) ok = 0;

    // For each frame, find the quality value that produces WebPPicture
    // having PSNR close to target_psnr.
    if (ok)
      for (auto& frame : frames_) {
        int frame_min_quality = 0;
        int frame_max_quality = 100;
        int frame_final_quality = -1;

        int frame_lowest_psnr = GetPSNR(&frame.pic, frame_min_quality);
        int frame_highest_psnr = GetPSNR(&frame.pic, frame_max_quality);

        // Target PSNR is out of range.
        if (target_psnr > frame_highest_psnr ||
            target_psnr < frame_lowest_psnr) {
          ok = false;
          std::cerr << "Target PSNR is out of range." << std::endl;
          break;
        }

        const int quality_tolerance = 1;

        // Binary search for quality value.
        while (frame_min_quality + quality_tolerance <= frame_max_quality) {
          int frame_mid_quality = (frame_min_quality + frame_max_quality) / 2;
          int current_psnr = GetPSNR(&frame.pic, frame_mid_quality);

          if (current_psnr <= target_psnr) {
            frame_final_quality = frame_mid_quality;
            frame_min_quality = frame_mid_quality + 1;
          } else {
            frame_max_quality = frame_mid_quality - 1;
          }
        }

        if (frame_final_quality == -1) {
          ok = false;
          break;
        }

        config_.quality = frame_final_quality;
        std::cerr << config_.quality << ' ';
        if (!WebPAnimEncoderAdd(enc_, &frame.pic, frame.timestamp_ms,
                                &config_)) {
          ok = false;
          break;
        }
      }

    // Add last frame.
    ok =
        ok && WebPAnimEncoderAdd(enc_, NULL, frames_.back().timestamp_ms, NULL);
    ok = ok && WebPAnimEncoderAssemble(enc_, &new_webp_data);

    if (ok && new_webp_data.size <= byte_budget_) {
      final_psnr = target_psnr;
      WebPDataClear(webp_data);
      *webp_data = new_webp_data;
      break;
    } else {
      WebPDataClear(&new_webp_data);
      std::cerr << std::endl;
    }
  }
  if (final_psnr == -1) {
    return kByteBudgetError;
  }

  std::cerr << std::endl;
  std::cerr << "Final PSNR: " << final_psnr << std::endl;

  if (loop_count_ == 0) return kOk;
  return SetLoopCount(webp_data);
}

Thumbnailer::Status Thumbnailer::SetLoopCount(WebPData* const webp_data) {
  std::unique_ptr<WebPMux, void (*)(WebPMux*)> mux(WebPMuxCreate(webp_data, 1),
                                                   WebPMuxDelete);
  if (mux == nullptr) return kMemoryError;

  WebPMuxAnimParams new_params;
  if (WebPMuxGetAnimationParams(mux.get(), &new_params) != WEBP_MUX_OK) {
    return kMemoryError;
  }

  new_params.loop_count = loop_count_;
  if (WebPMuxSetAnimationParams(mux.get(), &new_params) != WEBP_MUX_OK) {
    return kMemoryError;
  }

  WebPDataClear(webp_data);
  if (WebPMuxAssemble(mux.get(), webp_data) != WEBP_MUX_OK) return kMemoryError;

  return kOk;
}

}  // namespace libwebp
