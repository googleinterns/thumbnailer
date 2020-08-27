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

#ifndef THUMBNAILER_SRC_UTILS_THUMBNAILER_UTILS_H_
#define THUMBNAILER_SRC_UTILS_THUMBNAILER_UTILS_H_

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "../imageio/image_dec.h"
#include "../imageio/imageio_util.h"
#include "webp/demux.h"
#include "webp/encode.h"
#include "webp/mux.h"

#define CHECK_UTILS_STATUS(status)  \
  do {                              \
    const UtilsStatus S = (status); \
    if (S != kOk) return S;         \
  } while (0);

typedef std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>
    EnclosedWebPPicture;

namespace libwebp {

enum [[nodiscard]] UtilsStatus{
    kOk = 0,       // On success.
    kMemoryError,  // In case of memory error.
    kGenericError  // For other errors.
};

struct Frame {
  EnclosedWebPPicture pic;
  int timestamp;  // Ending timestamp in milliseconds.
};

// Stores the PSNR values for a thumbnail with various statistics.
struct ThumbnailStatsPSNR {
  std::vector<float> psnr;
  float min_psnr, max_psnr, mean_psnr, median_psnr;
};

// Stores the difference in PSNR values between two thumbnails,
// with various statistics.
struct ThumbnailDiffPSNR {
  std::vector<float> psnr_diff;
  float max_psnr_increase;
  float max_psnr_decrease;
  float mean_psnr_diff;
  float median_psnr_diff;
};

// Converts WebPData (animation) into Frame(s).
UtilsStatus AnimData2Frames(WebPData* const webp_data,
                            std::vector<Frame>* const pics);

// Takes WebPData having original_frames as source, calls AnimData2Frames.
// Records PSNR values for every WebPPicture and various PSNR stats.
UtilsStatus AnimData2PSNR(const std::vector<Frame>& original_frames,
                          WebPData* const webp_data,
                          ThumbnailStatsPSNR* const stats);

// Takes the original frames, and two thumbnails as WebPData.
// Records the differences in PSNR between two thumbnails and various stats.
// Differences are with respect to webp_data_1.
UtilsStatus CompareThumbnail(const std::vector<Frame>& original_frames,
                             WebPData* const webp_data_1,
                             WebPData* const webp_data_2,
                             ThumbnailDiffPSNR* const stats);

void PrintThumbnailStatsPSNR(const ThumbnailStatsPSNR& stats);

void PrintThumbnailDiffPSNR(const ThumbnailDiffPSNR& diff);

}  // namespace libwebp

#endif  // THUMBNAILER_SRC_UTILS_THUMBNAILER_UTILS_H_
