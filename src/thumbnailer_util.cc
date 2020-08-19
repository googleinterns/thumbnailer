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

#include "thumbnailer_util.h"

#include <iomanip>

namespace libwebp {

ThumbnailerUtil::Status ThumbnailerUtil::AnimData2Pictures(
    WebPData* const webp_data, std::vector<EnclosedWebPPicture>* const pics) {
  std::unique_ptr<WebPAnimDecoder, void (*)(WebPAnimDecoder*)> dec(
      WebPAnimDecoderNew(webp_data, NULL), WebPAnimDecoderDelete);
  if (dec == NULL) {
    std::cerr << "Error parsing image." << std::endl;
    return kMemoryError;
  }

  WebPAnimInfo anim_info;
  if (!WebPAnimDecoderGetInfo(dec.get(), &anim_info)) {
    std::cerr << "Error getting global info about the animation";
    return kMemoryError;
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
    pics->emplace_back(new WebPPicture, WebPPictureFree);
    WebPPicture* pic = pics->back().get();
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

ThumbnailerUtil::Status ThumbnailerUtil::AnimData2PSNR(
    const std::vector<EnclosedWebPPicture>& original_pics,
    WebPData* const webp_data, std::vector<float>* const psnr) {
  std::vector<EnclosedWebPPicture> pics;
  if (AnimData2Pictures(webp_data, &pics) != kOk) return kMemoryError;
  if (pics.size() != original_pics.size()) {
    std::cerr << "Frame count mismatched.";
    return kMemoryError;
  }

  const int frame_count = original_pics.size();

  for (int i = 0; i < frame_count; ++i) {
    const EnclosedWebPPicture& orig_pic = original_pics[i];
    const auto& pic = pics[i];
    float distortion_result[5];
    if (!WebPPictureDistortion(orig_pic.get(), pic.get(), 0,
                               distortion_result)) {
      break;
    } else {
      psnr->push_back(distortion_result[4]);  // PSNR-all.
    }
  }

  // Not all frames are processed.
  if (psnr->size() != frame_count) return kMemoryError;

  // Print statistics.
  std::vector<float> new_psnr(*psnr);
  std::sort(new_psnr.begin(), new_psnr.end());
  std::cerr << std::setw(14) << std::left << "Min PSNR: " << new_psnr.front()
            << '\n';
  std::cerr << std::setw(14) << std::left << "Max PSNR: " << new_psnr.back()
            << '\n';
  std::cerr << std::setw(14) << std::left << "Mean PSNR: "
            << std::accumulate(new_psnr.begin(), new_psnr.end(), 0.0) /
                   frame_count
            << '\n';
  std::cerr << std::setw(14) << std::left
            << "Median PSNR: " << new_psnr[frame_count / 2] << '\n';
  std::cerr << '\n';
  return kOk;
}

ThumbnailerUtil::Status ThumbnailerUtil::CompareThumbnail(
    const std::vector<EnclosedWebPPicture>& original_pics,
    WebPData* const webp_data_ref, WebPData* const webp_data) {
  if (original_pics.empty()) {
    std::cerr << "Thumbnail doesn't contain any frames.";
    return kOk;
  }
  std::vector<float> psnr_ref, psnr, psnr_diff;
  if (AnimData2PSNR(original_pics, webp_data_ref, &psnr_ref) != kOk) {
    return kMemoryError;
  }
  if (AnimData2PSNR(original_pics, webp_data, &psnr) != kOk) {
    return kMemoryError;
  }
  if (psnr_ref.size() != psnr.size()) return kMemoryError;

  const int frame_count = original_pics.size();

  for (int i = 0; i < frame_count; ++i) {
    psnr_diff.push_back(psnr[i] - psnr_ref[i]);
  }

  // Print statistics.
  std::sort(psnr_diff.begin(), psnr_diff.end());
  const float max_psnr_decrease = psnr_diff.front();
  const float max_psnr_increase = psnr_diff.back();
  const float sum_psnr_changes =
      std::accumulate(psnr_diff.begin(), psnr_diff.end(), 0.0);

  std::cerr << "Frame count: " << frame_count << '\n';

  if (max_psnr_increase < 0) {
    std::cerr << "All frames worsened in PSNR.\n";
  } else {
    std::cerr << std::setw(22) << std::left
              << "Max PSNR increase: " << max_psnr_increase << '\n';
  }
  if (max_psnr_decrease > 0) {
    std::cerr << "All frames improved in PSNR.\n";
  } else {
    std::cerr << std::setw(22) << std::left
              << "Max PSNR decrease: " << max_psnr_decrease << '\n';
  }
  std::cerr << std::setw(22) << std::left
            << "Sum of PSNR changes: " << sum_psnr_changes << '\n';
  std::cerr << std::setw(22) << std::left
            << "Mean PSNR change: " << sum_psnr_changes / frame_count << '\n';
  std::cerr << std::setw(22) << std::left
            << "Median PSNR change: " << psnr_diff[frame_count / 2] << '\n';

  return kOk;
}

}  // namespace libwebp
