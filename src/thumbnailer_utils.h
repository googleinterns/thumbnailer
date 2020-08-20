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

#ifndef THUMBNAILER_SRC_THUMBNAILER_UTILS_H_
#define THUMBNAILER_SRC_THUMBNAILER_UTILS_H_

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

typedef std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>
    EnclosedWebPPicture;

namespace libwebp {

enum UtilsStatus {
  kOk = 0,       // On success.
  kMemoryError,  // In case of memory error.
  kGenericError  // For other errors.
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
  float sum_psnr_diff;
  float mean_psnr_diff;
  float median_psnr_diff;
};

// Converts WebPData (animation) into WebPPictures.
UtilsStatus AnimData2Pictures(WebPData* const webp_data,
                              std::vector<EnclosedWebPPicture>* const pics);

// Takes WebPData having original_pics as source, calls AnimData2Pictures.
// Records PSNR values for every WebPPictures and various PSNR stats.
UtilsStatus AnimData2PSNR(const std::vector<EnclosedWebPPicture>& original_pics,
                          WebPData* const webp_data,
                          ThumbnailStatsPSNR* const stats);

// Takes two thumbnails as WebPData and original frames as WebPPicture(s).
// Records the differences in PSNR between two thumbnails and various stats.
// Differences are with respect to webp_data_1.
UtilsStatus CompareThumbnail(
    const std::vector<EnclosedWebPPicture>& original_pics,
    WebPData* const webp_data_1, WebPData* const webp_data_2,
    ThumbnailDiffPSNR* const stats);

void PrintThumbnailStatsPSNR(const ThumbnailStatsPSNR& stats);

void PrintThumbnailDiffPSNR(const ThumbnailDiffPSNR& diff);

}  // namespace libwebp

#endif  // THUMBNAILER_SRC_THUMBNAILER_UTILS_H_