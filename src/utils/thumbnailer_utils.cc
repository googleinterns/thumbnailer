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

#include "thumbnailer_utils.h"

namespace libwebp {

// Returns true on success and false on failure.
bool ReadPicture(const char filename[], WebPPicture* const pic) {
  const uint8_t* data = NULL;
  size_t data_size = 0;
  if (!ImgIoUtilReadFile(filename, &data, &data_size)) return false;

  pic->use_argb = 1;  // force ARGB.

  WebPImageReader reader = WebPGuessImageReader(data, data_size);
  bool ok = reader(data, data_size, pic, 1, NULL);
  free((void*)data);
  return ok;
}

void WebPPictureDelete(WebPPicture* picture) {
  WebPPictureFree(picture);
  delete picture;
}

void WebPDataDelete(WebPData* webp_data) {
  WebPDataClear(webp_data);
  delete webp_data;
}

UtilsStatus AnimData2Frames(WebPData* const webp_data,
                            std::vector<Frame>* const frames) {
  std::unique_ptr<WebPAnimDecoder, void (*)(WebPAnimDecoder*)> dec(
      WebPAnimDecoderNew(webp_data, NULL), WebPAnimDecoderDelete);
  if (dec == NULL) {
    std::cerr << "Error parsing image." << std::endl;
    return kMemoryError;
  }

  WebPAnimInfo anim_info;
  if (!WebPAnimDecoderGetInfo(dec.get(), &anim_info)) {
    std::cerr << "Error getting global info about the animation." << std::endl;
    return kGenericError;
  }

  const int width = anim_info.canvas_width;
  const int height = anim_info.canvas_height;

  while (WebPAnimDecoderHasMoreFrames(dec.get())) {
    uint8_t* frame_rgba;
    int timestamp;
    if (!WebPAnimDecoderGetNext(dec.get(), &frame_rgba, &timestamp)) {
      std::cerr << "Error decoding frame." << std::endl;
      return kMemoryError;
    }
    frames->push_back(
        {EnclosedWebPPicture(new WebPPicture, WebPPictureFree), timestamp});
    WebPPicture* pic = frames->back().pic.get();
    if (!WebPPictureInit(pic)) return kMemoryError;
    pic->use_argb = 1;
    pic->width = width;
    pic->height = height;
    if (!WebPPictureImportRGBA(pic, frame_rgba, width * 4)) {
      return kMemoryError;
    }
  }
  return kOk;
}

UtilsStatus AnimData2PSNR(const std::vector<Frame>& original_frames,
                          WebPData* const webp_data,
                          ThumbnailStatsPSNR* const stats) {
  if (stats == nullptr) return kMemoryError;

  std::vector<Frame> new_frames;
  CHECK_UTILS_STATUS(AnimData2Frames(webp_data, &new_frames));

  // We assume that new_frames.size() <= original_frames.size(). For some cases,
  // consecutive frames of original_frames having the exact same WebPPicture are
  // merged into one in new_frames. Otherwise, no frames are merged; new_frames
  // and original_frames are equal in size.
  //
  // It is unlikely to have new_frames.size() > original_frames.size() for
  // thumbnails, as discussed:
  // https://github.com/googleinterns/step255-2020/pull/25#discussion_r476508818
  if (new_frames.size() > original_frames.size()) {
    return kGenericError;
  }

  std::vector<Frame>::size_type new_frame_index = 0;
  for (const Frame& original_frame : original_frames) {
    // Check if the next frame of new_frames matches original_frame,
    // based on their timestamps.
    while (new_frame_index + 1 < new_frames.size() &&
           new_frames[new_frame_index].timestamp < original_frame.timestamp) {
      ++new_frame_index;
    }
    const Frame& new_frame = new_frames[new_frame_index];
    if (new_frame.timestamp < original_frame.timestamp) {
      std::cerr << "Timestamp mismatched." << std::endl;
      return kGenericError;
    }

    float distortion_results[5];
    if (!WebPPictureDistortion(original_frame.pic.get(), new_frame.pic.get(), 0,
                               distortion_results)) {
      return kGenericError;
    } else {
      stats->psnr.push_back(distortion_results[4]);  // PSNR-all.
    }
  }

  // Record statistics.
  std::vector<float> new_psnr(stats->psnr);
  std::sort(new_psnr.begin(), new_psnr.end());
  stats->min_psnr = new_psnr.front();
  stats->max_psnr = new_psnr.back();
  stats->mean_psnr = std::accumulate(new_psnr.begin(), new_psnr.end(), 0.0) /
                     original_frames.size();
  stats->median_psnr = new_psnr[original_frames.size() / 2];

  return kOk;
}

UtilsStatus CompareThumbnail(const std::vector<Frame>& original_frames,
                             WebPData* const webp_data_1,
                             WebPData* const webp_data_2,
                             ThumbnailDiffPSNR* const diff) {
  if (original_frames.empty()) {
    std::cerr << "Thumbnail doesn't contain any frames." << std::endl;
    return kOk;
  }
  if (diff == nullptr) return kMemoryError;

  ThumbnailStatsPSNR stats_1, stats_2;
  CHECK_UTILS_STATUS(AnimData2PSNR(original_frames, webp_data_1, &stats_1));
  CHECK_UTILS_STATUS(AnimData2PSNR(original_frames, webp_data_2, &stats_2));

  const int frame_count = original_frames.size();
  for (int i = 0; i < frame_count; ++i) {
    diff->psnr_diff.push_back(stats_2.psnr[i] - stats_1.psnr[i]);
  }

  // Record statistics.
  std::vector<float> psnr_diff_copy(diff->psnr_diff);
  std::sort(psnr_diff_copy.begin(), psnr_diff_copy.end());
  diff->max_psnr_decrease = psnr_diff_copy.front();
  diff->max_psnr_increase = psnr_diff_copy.back();
  diff->mean_psnr_diff =
      std::accumulate(psnr_diff_copy.begin(), psnr_diff_copy.end(), 0.0) /
      frame_count;
  diff->median_psnr_diff = psnr_diff_copy[frame_count / 2];

  return kOk;
}

void PrintThumbnailStatsPSNR(const ThumbnailStatsPSNR& stats,
                             const UtilsOption& option) {
  if (stats.psnr.empty()) return;

  if (option.short_output) {
    std::cout << stats.min_psnr << ' ' << stats.max_psnr << ' '
              << stats.mean_psnr << ' ' << stats.median_psnr << std::endl;
    return;
  }

  std::cout << std::endl;
  std::cout << "Frame count: " << stats.psnr.size() << std::endl;

  std::cout << std::fixed << std::showpoint;
  std::cout << std::setprecision(3);

  for (std::vector<float>::size_type i = 0; i < stats.psnr.size(); ++i) {
    std::cout << stats.psnr[i] << ' ';
  }
  std::cout << std::endl;

  std::cout << std::setw(14) << std::left << "Min PSNR: " << stats.min_psnr
            << std::endl;
  std::cout << std::setw(14) << std::left << "Max PSNR: " << stats.max_psnr
            << std::endl;
  std::cout << std::setw(14) << std::left << "Mean PSNR: " << stats.mean_psnr
            << std::endl;
  std::cout << std::setw(14) << std::left
            << "Median PSNR: " << stats.median_psnr << std::endl;
  std::cout << std::resetiosflags(std::ios::fixed)
            << std::resetiosflags(std::ios::showpoint) << std::endl;
}

void PrintThumbnailDiffPSNR(const ThumbnailDiffPSNR& diff,
                            const UtilsOption& option) {
  if (diff.psnr_diff.empty()) return;

  if (option.short_output) {
    std::cout << diff.max_psnr_decrease << ' ' << diff.max_psnr_increase << ' '
              << diff.mean_psnr_diff << ' ' << diff.median_psnr_diff
              << std::endl;
    return;
  }

  std::cout << std::endl;
  std::cout << "Frame count: " << diff.psnr_diff.size() << std::endl;

  std::cout << std::showpos;
  std::cout << std::fixed << std::showpoint;
  std::cout << std::setprecision(3);

  for (std::vector<float>::size_type i = 0; i < diff.psnr_diff.size(); ++i) {
    std::cout << diff.psnr_diff[i] << ' ';
  }
  std::cout << std::endl;

  if (diff.max_psnr_decrease > 0) {
    std::cout << "All frames improved in PSNR." << std::endl;
  } else {
    std::cout << std::setw(21) << std::left
              << "Max PSNR decrease: " << diff.max_psnr_decrease << std::endl;
  }

  if (diff.max_psnr_increase < 0) {
    std::cout << "All frames worsened in PSNR." << std::endl;
  } else {
    std::cout << std::setw(21) << std::left
              << "Max PSNR increase: " << diff.max_psnr_increase << std::endl;
  }

  std::cout << std::setw(21) << std::left
            << "Mean PSNR change: " << diff.mean_psnr_diff << std::endl;
  std::cout << std::setw(21) << std::left
            << "Median PSNR change: " << diff.median_psnr_diff << std::endl;
  std::cout << std::resetiosflags(std::ios::showpos)
            << std::resetiosflags(std::ios::fixed)
            << std::resetiosflags(std::ios::showpoint) << std::endl;
}

}  // namespace libwebp
