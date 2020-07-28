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

#include "class.h"

namespace libwebp {

Thumbnailer::Thumbnailer() {
  WebPAnimEncoderOptionsInit(&anim_config);
  WebPConfigInit(&config);
}

Thumbnailer::~Thumbnailer() { WebPAnimEncoderDelete(enc); }

Thumbnailer::Status Thumbnailer::AddFrame(const WebPPicture& pic,
                                          int timestamp_ms) {
  // Verify dimension of frames.
  if (!frames.empty() && (pic.width != (frames[0].pic).width ||
                          pic.height != (frames[0].pic).height)) {
    return kImageFormatError;
  }

  FrameData new_frame = {pic, timestamp_ms};
  frames.push_back(new_frame);

  return kOk;
}

Thumbnailer::Status Thumbnailer::GenerateAnimation(WebPData* const webp_data) {
  // Initialize WebPAnimEncoder object.
  enc = WebPAnimEncoderNew((frames[0].pic).width, (frames[0].pic).height,
                           &anim_config);
  if (enc == nullptr) {
    return kMemoryError;
  }

  // Fill the animation.
  for (auto& frame : frames) {
    if (!WebPAnimEncoderAdd(enc, &frame.pic, frame.timestamp_ms, &config)) {
      return kMemoryError;
    }
  }

  // Add last frame.
  if (!WebPAnimEncoderAdd(enc, NULL, frames.back().timestamp_ms, NULL)) {
    return kMemoryError;
  }

  if (!WebPAnimEncoderAssemble(enc, webp_data)) {
    return kMemoryError;
  }

  if (loop_count == 0) return kOk;

  // Set loop count.
  WebPMuxError err;
  WebPMuxAnimParams new_params;
  uint32_t features;

  std::unique_ptr<WebPMux, void (*)(WebPMux*)> mux(WebPMuxCreate(webp_data, 1),
                                                   WebPMuxDelete);

  if (mux.get() == nullptr) return kMemoryError;

  if (WebPMuxGetFeatures(mux.get(), &features) != WEBP_MUX_OK) {
    return kMemoryError;
  }

  if (WebPMuxGetAnimationParams(mux.get(), &new_params) != WEBP_MUX_OK) {
    return kMemoryError;
  }

  new_params.loop_count = loop_count;
  if (WebPMuxSetAnimationParams(mux.get(), &new_params) != WEBP_MUX_OK) {
    return kMemoryError;
  }

  WebPDataClear(webp_data);
  if (WebPMuxAssemble(mux.get(), webp_data) != WEBP_MUX_OK) return kMemoryError;

  return kOk;
}

Thumbnailer::Status Thumbnailer::GenerateAnimationFittingBudget(
    WebPData* const webp_data) {
  // Rearrange frames.
  std::sort(frames.begin(), frames.end(),
            [](const FrameData& A, const FrameData& B) -> bool {
              return A.timestamp_ms < B.timestamp_ms;
            });

  // Use binary search to find the quality that makes the animation fit right
  // below the given byte budget.
  int min_quality = 0;
  int max_quality = 100;
  int final_quality = -1;
  WebPData new_webp_data;
  WebPDataInit(&new_webp_data);

  while (min_quality <= max_quality) {
    int middle = (min_quality + max_quality) / 2;

    config.quality = middle;
    GenerateAnimation(&new_webp_data);

    if (new_webp_data.size <= byte_budget) {
      final_quality = middle;
      *webp_data = new_webp_data;
      min_quality = middle + 1;
    } else {
      max_quality = middle - 1;
    }
  }

  if (final_quality == -1) {
    return kByteBudgetError;
  }

  config.quality = final_quality;

  return kOk;
}

}  // namespace libwebp
