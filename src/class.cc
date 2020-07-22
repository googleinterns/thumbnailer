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

// Let's implement stuff !
// TODO: to create an animation:
//       - create a WebPAnimEncoderOptions  config
//       - set the quality to something in your search space
bool Thumbnailer::AddFrame(const WebPPicture& pic, int timestamp_ms) {
  bool ok = true;

  if (frames.empty()) {
    enc = WebPAnimEncoderNew(pic.width, pic.height, &anim_config);
    ok = (enc != nullptr);
  } else {
    if (pic.width != (frames[0].pic).width ||
        pic.height != (frames[0].pic).height) {
      ok = false;
    }
  }

  FrameData new_frame = {pic, timestamp_ms};
  frames.push_back(new_frame);

  return ok;
}

//       - call WebPAnimEncoderNew, then fill the animation with WebPPicture
//         using WebPAnimEncoderAdd
//       - finalize with WebPAnimEncoderAssemble
bool Thumbnailer::GenerateAnimation(WebPData* const webp_data) {
  bool ok = true;
  // Rearrange frames
  std::sort(frames.begin(), frames.end(),
            [&](FrameData A, FrameData B) -> bool {
              return A.timestamp_ms < B.timestamp_ms;
            });

  // Fill the animation
  for (auto frame : frames) {
    ok = ok && WebPAnimEncoderAdd(enc, &frame.pic, frame.timestamp_ms, &config);
  }

  // Add last frame
  WebPAnimEncoderAdd(enc, NULL, frames.back().timestamp_ms, NULL);
  WebPAnimEncoderAssemble(enc, webp_data);

  // Set loop count
  WebPMuxAnimParams new_params;
  WebPMux* const mux = WebPMuxCreate(webp_data, 1);
  new_params.loop_count = loop_count;
  WebPMuxSetAnimationParams(mux, &new_params);
  WebPDataClear(webp_data);
  WebPMuxAssemble(mux, webp_data);
  WebPMuxDelete(mux);

  return ok;
}

}  // namespace libwebp
