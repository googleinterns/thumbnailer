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

#ifndef THUMBNAILER_SRC_THUMBNAILER_UTIL_H_
#define THUMBNAILER_SRC_THUMBNAILER_UTIL_H_

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

class ThumbnailerUtil {
 public:
  enum Status {
    kOk = 0,       // On success.
    kMemoryError,  // In case of memory error.
    kGenericError  // For other errors.
  };

  // Stores the PSNR values for a thumbnail.
  struct ThumbnailStatPSNR {
    std::vector<float> psnr;
    float min_psnr, max_psnr, mean_psnr, median_psnr;
  };

  // Stores the differences in PSNR values between two thumbnails,
  struct ThumbnailDiffPSNR {
    std::vector<float> psnr_diff;
    float max_psnr_increase;
    float max_psnr_decrease;
    float sum_psnr_diff;
    float mean_psnr_diff;
    float median_psnr_diff;
  };

  // Converts WebPData (animation) into WebPPictures.
  Status AnimData2Pictures(WebPData* const webp_data,
                           std::vector<EnclosedWebPPicture>* const pics);

  // Takes WebPData having original_pics as source, calls AnimData2Pictures.
  // Records PSNR values for every WebPPictures and various PSNR stats.
  Status AnimData2PSNR(const std::vector<EnclosedWebPPicture>& original_pics,
                       WebPData* const webp_data,
                       ThumbnailStatPSNR* const stats);

  // Takes two thumbnails as WebPData and original frames as WebPPicture(s).
  // Records the differences in PSNR between two thumbnails and various stats.
  Status CompareThumbnail(const std::vector<EnclosedWebPPicture>& original_pics,
                          WebPData* const webp_data_ref,
                          WebPData* const webp_data,
                          ThumbnailDiffPSNR* const stats);

  void PrintThumbnailStatPSNR(const ThumbnailStatPSNR& stats);

  void PrintThumbnailDiffPSNR(const ThumbnailDiffPSNR& stats);
};

}  // namespace libwebp

#endif  // THUMBNAILER_SRC_THUMBNAILER_UTIL_H_