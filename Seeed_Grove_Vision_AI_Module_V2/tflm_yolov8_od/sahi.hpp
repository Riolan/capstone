#ifndef SAHI_HPP
#define SAHI_HPP

#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <memory>
#include <numeric> 

#include "xprintf.h"
#include "send_result.h"
// For box information
#include "yolo_postprocessing.h"


#ifndef DETECTION_CLS_YOLOV8
typedef struct detection_cls_yolov8{
    box bbox;
    float confidence;
    float index;

} detection_cls_yolov8;
#define DETECTION_CLS_YOLOV8 1
#endif 

namespace SAHI {

struct Slice {
    int x, y, width, height;
    int slice_idx;
};

// Calculate Intersection over Union (IoU) between two boxes
/*float calculate_iou(const box& box1, const box& box2) {
    // Calculate box coordinates
    float b1_x1 = box1.x - box1.w / 2;
    float b1_y1 = box1.y - box1.h / 2;
    float b1_x2 = box1.x + box1.w / 2;
    float b1_y2 = box1.y + box1.h / 2;

    float b2_x1 = box2.x - box2.w / 2;
    float b2_y1 = box2.y - box2.h / 2;
    float b2_x2 = box2.x + box2.w / 2;
    float b2_y2 = box2.y + box2.h / 2;

    // Calculate intersection coordinates
    float inter_x1 = std::fmax(b1_x1, b2_x1);
    float inter_y1 = std::fmax(b1_y1, b2_y1);
    float inter_x2 = std::fmin(b1_x2, b2_x2);
    float inter_y2 = std::fmin(b1_y2, b2_y2);

    // Calculate intersection area
    float inter_area = std::fmax(inter_x2 - inter_x1, 0.0f) * 
                       std::fmax(inter_y2 - inter_y1, 0.0f);

    // Calculate union area
    float b1_area = box1.w * box1.h;
    float b2_area = box2.w * box2.h;
    float union_area = b1_area + b2_area - inter_area;

    return inter_area / union_area;
}*/

// Create overlapping slices from original image
std::vector<Slice> create_slices(
    int img_width, 
    int img_height, 
    int slice_width, 
    int slice_height, 
    float overlap_ratio
) {
    std::vector<Slice> slices;

    // Handle case where image is smaller than slice size
    if (slice_width >= img_width && slice_height >= img_height) {
        slices.push_back({0, 0, img_width, img_height, 0});
        return slices;
    }

    // Calculate steps with overlap
    int x_step = static_cast<int>(slice_width * (1 - overlap_ratio));
    int y_step = static_cast<int>(slice_height * (1 - overlap_ratio));

    x_step = std::max(x_step, 1);
    y_step = std::max(y_step, 1);

    // Calculate number of slices
    int x_slices = (img_width - slice_width) / x_step + 1;
    int y_slices = (img_height - slice_height) / y_step + 1;

    // Ensure coverage of image edges
    if ((img_width - slice_width) % x_step != 0) x_slices++;
    if ((img_height - slice_height) % y_step != 0) y_slices++;

    int slice_idx = 0;
    for (int y = 0; y < y_slices; y++) {
        for (int x = 0; x < x_slices; x++) {
            int start_x = x * x_step;
            int start_y = y * y_step;

            // Adjust for image boundaries
            start_x = std::min(start_x, img_width - slice_width);
            start_y = std::min(start_y, img_height - slice_height);

            slices.push_back({
                start_x, start_y, 
                slice_width, slice_height, 
                slice_idx++
            });
        }
    }

    return slices;
}

// Map detection coordinates from slice to original image
void map_slice_coordinates(
    std::vector<detection_cls_yolov8>& detections, 
    const Slice& slice, 
    int img_width, 
    int img_height
) {
    for (auto& det : detections) {
        // Scale and translate box coordinates
        // Assuming box coordinates are center-based (x, y, w, h)
        det.bbox.x = (det.bbox.x * slice.width + slice.x) / static_cast<float>(img_width);
        det.bbox.y = (det.bbox.y * slice.height + slice.y) / static_cast<float>(img_height);
        det.bbox.w /= static_cast<float>(img_width);
        det.bbox.h /= static_cast<float>(img_height);
    }
    xprintf("detections.size(): %d\r\n", detections.size());
}

/*
   // Merge and apply Non-Maximum Suppression across slices
std::vector<detection_cls_yolov8> merge_detections(
    const std::vector<detection_cls_yolov8>& all_detections,
    float iou_threshold,
    float conf_threshold // Apply confidence threshold *before* NMS
) {
    std::vector<detection_cls_yolov8> filtered_boxes;

    // 1. Filter by confidence threshold first
    for (const auto& det : all_detections) {
        if (det.confidence >= conf_threshold) {
            filtered_boxes.push_back(det);
        }
    }
        xprintf("SAHI Merge: Boxes after confidence filter (>=%.2f): %d\n", conf_threshold, filtered_boxes.size());

    if (filtered_boxes.empty()) {
        return {}; // Return empty vector
    }

    // 2. Sort by confidence (descending) - NMS typically processes higher scores first
    std::sort(filtered_boxes.begin(), filtered_boxes.end(),
        [](const detection_cls_yolov8& a, const detection_cls_yolov8& b) {
            return a.confidence > b.confidence;
        });

    // 3. Apply Non-Maximum Suppression
    std::vector<detection_cls_yolov8> final_detections;
    std::vector<bool> suppressed(filtered_boxes.size(), false);

    for (size_t i = 0; i < filtered_boxes.size(); ++i) {
        if (suppressed[i]) {
            continue; // Skip if already suppressed
        }

        // Keep this box
        final_detections.push_back(filtered_boxes[i]);

        // Suppress other boxes of the same class with high IoU
        for (size_t j = i + 1; j < filtered_boxes.size(); ++j) {
            if (suppressed[j]) {
                continue;
            }

            // Check if classes match (using integer comparison of index)
            if (static_cast<int>(std::round(filtered_boxes[i].index)) == static_cast<int>(std::round(filtered_boxes[j].index))) {
                float iou = calculate_iou(filtered_boxes[i].bbox, filtered_boxes[j].bbox);
                if (iou > iou_threshold) {
                    suppressed[j] = true; // Suppress this box
                        xprintf("  NMS Suppress: Box %d (conf %.2f) suppressed by Box %d (conf %.2f) due to IoU %.3f > %.3f\n",
                               j, filtered_boxes[j].confidence, i, filtered_boxes[i].confidence, iou, iou_threshold);
                     
                }
            }
        }
    }
        xprintf("SAHI Merge: Boxes after cross-slice NMS (IoU > %.2f): %d\n", iou_threshold, final_detections.size());
    

    return final_detections;
}
*/
/*
// Merge and apply Non-Maximum Suppression
std::vector<detection_cls_yolov8> merge_detections(
    const std::vector<std::vector<detection_cls_yolov8>>& all_detections, 
    float iou_threshold, 
    float conf_threshold
) {
    std::vector<detection_cls_yolov8> all_boxes;

    // Collect detections above confidence threshold
    for (const auto& slice_detections : all_detections) {
        for (const auto& det : slice_detections) {
            if (det.confidence >= conf_threshold) {
                all_boxes.push_back(det);
            }
        }
    }
    xprintf("all_boxes.size(): %d\r\n", all_boxes.size());
    xprintf("SAHI Merge: Boxes after confidence filter (>=%.2f): %d\n", conf_threshold, filtered_boxes.size());



    // Sort by confidence (descending)
    std::sort(all_boxes.begin(), all_boxes.end(), 
        [](const detection_cls_yolov8& a, const detection_cls_yolov8& b) {
            return a.confidence > b.confidence;
        });

    // Non-Maximum Suppression
    std::vector<detection_cls_yolov8> final_detections;
    for (const auto& current_det : all_boxes) {
        bool keep = true;
        for (const auto& kept_det : final_detections) {
            // Compare IoU and class index (using index as class identifier)
            if (std::abs(current_det.index - kept_det.index) < 0.001f && 
                calculate_iou(current_det.bbox, kept_det.bbox) > iou_threshold) {
                keep = false;
                break;
            }
        }
        if (keep) {
            final_detections.push_back(current_det);
        }
    }

    xprintf("final_detections.size(): %d\r\n", final_detections.size());

    return final_detections;
}*/

 // Map detection coordinates from model input space to original image space and add to list
 void map_and_add_detections(
    std::vector<detection_cls_yolov8>& all_mapped_detections, // Output list (append)
    const std::vector<detection_cls_yolov8>& slice_detections, // Input detections for one slice
    const Slice& slice,                  // Slice info (offset and dims)
    int model_input_width,               // Width the slice_detections are relative to
    int model_input_height,              // Height the slice_detections are relative to
    int original_img_width,              // Full image width
    int original_img_height              // Full image height
) {
    // Calculate scaling factor from model input coords to slice coords
    // Note: slice.width/height might differ from model_input_width/height if resizing was needed
    float scale_x = static_cast<float>(slice.width) / model_input_width;
    float scale_y = static_cast<float>(slice.height) / model_input_height;

    #if SAHI_DBG_LOG >= 3 // Very verbose
        xprintf("MapSlice %d: Scale factors x=%.3f, y=%.3f\n", slice.slice_idx, scale_x, scale_y);
    #endif

    for (const auto& det_slice : slice_detections) {
        detection_cls_yolov8 det_mapped = det_slice; // Copy confidence, index

        // Coordinates from slice_detections are relative to model_input_width/height
        // (center x, y, w, h format assumed)
        float center_x_model = det_slice.bbox.x;
        float center_y_model = det_slice.bbox.y;
        float width_model = det_slice.bbox.w;
        float height_model = det_slice.bbox.h;

        // 1. Scale coordinates to the slice's dimensions
        float center_x_in_slice = center_x_model * scale_x;
        float center_y_in_slice = center_y_model * scale_y;
        float width_in_slice = width_model * scale_x;
        float height_in_slice = height_model * scale_y;

        // 2. Translate coordinates by the slice's offset in the original image
        det_mapped.bbox.x = center_x_in_slice + slice.x;
        det_mapped.bbox.y = center_y_in_slice + slice.y;
        det_mapped.bbox.w = width_in_slice;
        det_mapped.bbox.h = height_in_slice;

        // 3. (Optional but recommended) Clamp mapped coordinates to original image boundaries
        // Convert to x1,y1,x2,y2 for easier clamping
        float x1 = det_mapped.bbox.x - det_mapped.bbox.w / 2.0f;
        float y1 = det_mapped.bbox.y - det_mapped.bbox.h / 2.0f;
        float x2 = det_mapped.bbox.x + det_mapped.bbox.w / 2.0f;
        float y2 = det_mapped.bbox.y + det_mapped.bbox.h / 2.0f;

        x1 = std::fmax(0.0f, std::fmin(static_cast<float>(original_img_width), x1));
        y1 = std::fmax(0.0f, std::fmin(static_cast<float>(original_img_height), y1));
        x2 = std::fmax(0.0f, std::fmin(static_cast<float>(original_img_width), x2));
        y2 = std::fmax(0.0f, std::fmin(static_cast<float>(original_img_height), y2));

        // Convert back to center x,y,w,h and update if clamping changed things
        det_mapped.bbox.w = x2 - x1;
        det_mapped.bbox.h = y2 - y1;
        det_mapped.bbox.x = x1 + det_mapped.bbox.w / 2.0f;
        det_mapped.bbox.y = y1 + det_mapped.bbox.h / 2.0f;

        // Only add if the box has a valid area after clamping
        if (det_mapped.bbox.w > 0 && det_mapped.bbox.h > 0) {
            all_mapped_detections.push_back(det_mapped);
                xprintf("  MappedDet: Cls=%.0f, Conf=%.2f, OrigBox=[%.1f, %.1f, %.1f, %.1f]\n",
                       det_mapped.index, det_mapped.confidence,
                       det_mapped.bbox.x, det_mapped.bbox.y, det_mapped.bbox.w, det_mapped.bbox.h);
        } else {
                xprintf("  Discarded mapped box with zero area after clamping.\n");
             
        }
    }
}



/*
std::vector<detection_cls_yolov8> convert_to_detections(const struct_yolov8_ob_algoResult& result, int max_count = MAX_TRACKED_YOLOV8_ALGO_RES) {
    std::vector<detection_cls_yolov8> detections;

    for (int i = 0; i < max_count; ++i) {
        const auto& ob = result.obr[i];

        if (ob.confidence == 0.0f) continue;

        // TODO: Maybe just change to use their internal representation rather than trying top use
        // this postprocessing types.
        detection_cls_yolov8 det;
        det.bbox.x = static_cast<float>(ob.bbox.x);
        det.bbox.y = static_cast<float>(ob.bbox.y);
        det.bbox.w = static_cast<float>(ob.bbox.width);
        det.bbox.h = static_cast<float>(ob.bbox.height);

        det.confidence = ob.confidence;
        det.index = static_cast<float>(ob.class_idx);  // assuming you use .index as class id
        detections.push_back(det);
    }

    return detections;
}*/


/**
 * @brief Converts the results from the SAHI-aware post-processing
 * into a vector of detection_cls_yolov8 objects.
 *
 * @param result The output from yolov8_ob_post_processing_sahi.
 * @param max_count The maximum number of results to check in result.obr.
 * @return std::vector<detection_cls_yolov8> A vector of detections.
 */
std::vector<detection_cls_yolov8> convert_to_detections(
    struct_yolov8_ob_algoResult *result,
    int max_count = MAX_TRACKED_YOLOV8_ALGO_RES)
{
    std::vector<detection_cls_yolov8> detections;
    detections.reserve(max_count); // Reserve space for efficiency

    for (int i = 0; i < max_count; ++i) {
        const auto& ob = result->obr[i];

        // Check for valid detection (confidence > 0 indicates it was filled)
        if (ob.confidence <= 0.0f) continue;

        detection_cls_yolov8 det;
        // Assuming detection_cls_yolov8.bbox uses top-left x, y, width, height
        det.bbox.x = static_cast<float>(ob.bbox.x);
        det.bbox.y = static_cast<float>(ob.bbox.y);
        det.bbox.w = static_cast<float>(ob.bbox.width);
        det.bbox.h = static_cast<float>(ob.bbox.height);

        det.confidence = ob.confidence;
        det.index = static_cast<float>(ob.class_idx); // Class ID
        detections.push_back(det);
    }
    return detections;
}



/**
 * @brief Calculates Intersection over Union (IoU) between two boxes.
 * ASSUMES box1 and box2 store top-left coordinates (x, y) and dimensions (w, h).
 *
 * @param box1 First bounding box (top-left x, y, width, height).
 * @param box2 Second bounding box (top-left x, y, width, height).
 * @return float The IoU value [0.0, 1.0].
 */
/*float calculate_iou_tl(const box& box1, const box& box2) {
    // Calculate coordinates for box1
    float b1_x1 = box1.x;
    float b1_y1 = box1.y;
    float b1_x2 = box1.x + box1.w;
    float b1_y2 = box1.y + box1.h;

    // Calculate coordinates for box2
    float b2_x1 = box2.x;
    float b2_y1 = box2.y;
    float b2_x2 = box2.x + box2.w;
    float b2_y2 = box2.y + box2.h;

    // Calculate intersection coordinates
    float inter_x1 = std::fmax(b1_x1, b2_x1);
    float inter_y1 = std::fmax(b1_y1, b2_y1);
    float inter_x2 = std::fmin(b1_x2, b2_x2);
    float inter_y2 = std::fmin(b1_y2, b2_y2);

    // Calculate intersection area (ensure width/height are non-negative)
    float inter_w = std::fmax(inter_x2 - inter_x1, 0.0f);
    float inter_h = std::fmax(inter_y2 - inter_y1, 0.0f);
    float inter_area = inter_w * inter_h;

    // Calculate union area
    float b1_area = box1.w * box1.h;
    float b2_area = box2.w * box2.h;
    float union_area = b1_area + b2_area - inter_area;

    // Calculate IoU - handle division by zero
    if (union_area <= 0.0f) {
        return 0.0f; // Or handle as appropriate (e.g., return 1.0 if both areas are 0 and they match)
    }
    return inter_area / union_area;
}*/


/**
 * @brief Merges detections from multiple slices using NMS.
 *
 * @param all_detections Vector where each inner vector contains detections from one slice
 * (in original image coordinates, top-left format).
 * @param iou_threshold IoU threshold for merging/suppressing boxes.
 * @param conf_threshold Confidence threshold to consider boxes for merging.
 * @return std::vector<detection_cls_yolov8> Final merged detections.
 */
/*std::vector<detection_cls_yolov8> merge_detections(
     std::vector<std::vector<detection_cls_yolov8>>& all_detections,
    float iou_threshold,
    float conf_threshold
) {
    std::vector<detection_cls_yolov8> collected_boxes;

    // Collect all detections above the confidence threshold
    for (const auto& slice_detections : all_detections) {
        for (const auto& det : slice_detections) {
            if (det.confidence >= conf_threshold) {
                collected_boxes.push_back(det);
            }
        }
    }

    #if YOLOV8N_OB_DBG_APP_LOG
        xprintf("SAHI Merge: Total boxes before merge NMS (Conf >= %.2f): %zu\n",
                conf_threshold, collected_boxes.size());
    #endif

    // Sort by confidence (descending) - crucial for NMS
    std::sort(collected_boxes.begin(), collected_boxes.end(),
        [](const detection_cls_yolov8& a, const detection_cls_yolov8& b) {
            return a.confidence > b.confidence;
        });

    // Non-Maximum Suppression across all collected boxes
    std::vector<detection_cls_yolov8> final_detections;
    std::vector<bool> suppressed(collected_boxes.size(), false);

    for (size_t i = 0; i < collected_boxes.size(); ++i) {
        if (suppressed[i]) {
            continue; // Skip if already suppressed
        }
        // Keep this box
        final_detections.push_back(collected_boxes[i]);

        // Suppress other boxes that overlap significantly with this one
        for (size_t j = i + 1; j < collected_boxes.size(); ++j) {
            if (suppressed[j]) {
                continue;
            }

            // Check if classes match (using index as class ID)
            // Use a small tolerance for float comparison
            if (std::abs(collected_boxes[i].index - collected_boxes[j].index) < 0.001f) {
                // Calculate IoU using the function that expects top-left coordinates
                float iou = calculate_iou_tl(collected_boxes[i].bbox, collected_boxes[j].bbox);

                if (iou > iou_threshold) {
                    suppressed[j] = true; // Suppress the lower-confidence box
                }
            }
        }
    }

    #if YOLOV8N_OB_DBG_APP_LOG
        xprintf("SAHI Merge: Final boxes after merge NMS (IoU <= %.2f): %zu\n",
                iou_threshold, final_detections.size());
    #endif

    return final_detections;
}

*/









#include <vector>
#include <algorithm> // For std::sort, std::copy
#include <cmath>     // For std::max, std::min

// --- Helper Function: Calculate IoU ---
// Needs to handle the x, y, w, h format in el_box_t
float calculate_iou(const el_box_t& box1, const el_box_t& box2) {
    // Convert to x1, y1, x2, y2 format
    uint16_t box1_x1 = box1.x;
    uint16_t box1_y1 = box1.y;
    uint16_t box1_x2 = box1.x + box1.w;
    uint16_t box1_y2 = box1.y + box1.h;

    uint16_t box2_x1 = box2.x;
    uint16_t box2_y1 = box2.y;
    uint16_t box2_x2 = box2.x + box2.w;
    uint16_t box2_y2 = box2.y + box2.h;

    // Calculate intersection coordinates
    uint16_t inter_x1 = std::max(box1_x1, box2_x1);
    uint16_t inter_y1 = std::max(box1_y1, box2_y1);
    uint16_t inter_x2 = std::min(box1_x2, box2_x2);
    uint16_t inter_y2 = std::min(box1_y2, box2_y2);

    // Calculate intersection area (handle case where boxes don't overlap)
    int intersection_w = std::max(0, inter_x2 - inter_x1); // Use int to avoid overflow before check
    int intersection_h = std::max(0, inter_y2 - inter_y1);
    float intersection_area = static_cast<float>(intersection_w) * intersection_h;

    if (intersection_area <= 0.f) {
        return 0.0f;
    }

    // Calculate union area
    float box1_area = static_cast<float>(box1.w) * box1.h;
    float box2_area = static_cast<float>(box2.w) * box2.h;
    float union_area = box1_area + box2_area - intersection_area;

    if (union_area <= 0.f) { // Avoid division by zero
        return 0.0f;
    }

    // Calculate IoU
    float iou = intersection_area / union_area;
    return iou;
}


// --- NMS Function ---
std::vector<el_box_t> non_maximum_suppression(
    std::forward_list<el_box_t>& all_boxes,
    float iou_threshold,
    bool class_agnostic = false // Set true to ignore target/class
) {
    // 1. Copy to vector
    std::vector<el_box_t> boxes_vec;
    std::copy(all_boxes.begin(), all_boxes.end(), std::back_inserter(boxes_vec));

    if (boxes_vec.empty()) {
        return {};
    }

    // 2. Sort by score (descending)
    std::sort(boxes_vec.begin(), boxes_vec.end(), [](const el_box_t& a, const el_box_t& b) {
        return a.score > b.score;
    });

    // 3. Perform NMS
    std::vector<el_box_t> keep_boxes;
    std::vector<bool> suppressed(boxes_vec.size(), false); // Track suppressed boxes

    for (size_t i = 0; i < boxes_vec.size(); ++i) {
        if (suppressed[i]) {
            continue; // Skip if already suppressed
        }

        // Keep the current box
        keep_boxes.push_back(boxes_vec[i]);

        // Suppress overlapping boxes with lower scores
        for (size_t j = i + 1; j < boxes_vec.size(); ++j) {
            if (suppressed[j]) {
                continue; // Skip if already suppressed
            }

            // Check class compatibility if not class-agnostic
            if (!class_agnostic && boxes_vec[i].target != boxes_vec[j].target) {
                continue; // Different classes, don't suppress
            }

            float iou = calculate_iou(boxes_vec[i], boxes_vec[j]);
            if (iou > iou_threshold) {
                suppressed[j] = true; // Suppress this box
            }
        }
    }

    return keep_boxes; // Return the vector of final boxes
}

#include <vector>
#include <forward_list> // Make sure it's included
#include <algorithm>
#include <cmath>
#include <iterator> // For std::back_inserter / std::distance (optional)

// Assume el_box_t struct is defined as before
// Assume calculate_iou(const el_box_t& box1, const el_box_t& box2) is defined as before

// --- NMS Function Returning std::forward_list ---

std::forward_list<el_box_t> non_maximum_suppression_fwd_list_out(
    const std::forward_list<el_box_t>& all_boxes, // Input list
    float iou_threshold,
    bool class_agnostic = false // Set true to ignore target/class
) {
    // 1. Copy to vector for efficient processing
    std::vector<el_box_t> boxes_vec;
    // Optional: Reserve capacity if you can estimate the size
    // size_t approx_size = std::distance(all_boxes.begin(), all_boxes.end());
    // if (approx_size > 0) boxes_vec.reserve(approx_size);
    std::copy(all_boxes.begin(), all_boxes.end(), std::back_inserter(boxes_vec));

    // Handle empty input case
    if (boxes_vec.empty()) {
        return {}; // Return empty forward_list
    }

    // 2. Sort the vector by score (descending)
    std::sort(boxes_vec.begin(), boxes_vec.end(), [](const el_box_t& a, const el_box_t& b) {
        return a.score > b.score; // Higher score comes first
    });

    // 3. Perform NMS using the vector, but store results in a forward_list
    std::forward_list<el_box_t> keep_list; // <--- Result list
    std::vector<bool> suppressed(boxes_vec.size(), false); // Track suppressed boxes

    for (size_t i = 0; i < boxes_vec.size(); ++i) {
        if (suppressed[i]) {
            continue; // Skip if already suppressed
        }

        // Keep the current box: Add it to the front of the result list
        keep_list.push_front(boxes_vec[i]); // <--- Store kept box in forward_list

        // Suppress overlapping boxes with lower scores
        for (size_t j = i + 1; j < boxes_vec.size(); ++j) {
            if (suppressed[j]) {
                continue; // Skip if already suppressed
            }

            // Check class compatibility if not class-agnostic
            if (!class_agnostic && boxes_vec[i].target != boxes_vec[j].target) {
                continue; // Different classes, don't suppress
            }

            float iou = calculate_iou(boxes_vec[i], boxes_vec[j]);
            if (iou > iou_threshold) {
                suppressed[j] = true; // Suppress this box
            }
        }
    }

    // 4. Reverse the keep_list (Important!)
    // Because we used push_front, the boxes are added in reverse order
    // (lowest score kept box is first). Reversing puts the highest score box first.
    keep_list.reverse();

    return keep_list; // Return the final forward_list
}




} // namespace SAHI

#endif // SAHI_HPP