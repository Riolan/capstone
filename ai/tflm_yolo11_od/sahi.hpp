#ifndef SAHI_HPP
#define SAHI_HPP

#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <memory>

// For box information
#include "yolo_postprocessing.h"

namespace SAHI {

struct Slice {
    int x, y, width, height;
    int slice_idx;
};

struct SAHIConfig {
    int slice_height = 320;
    int slice_width = 320;
    float overlap_ratio = 0.2f;
    float confidence_threshold = 0.5f;
    float iou_threshold = 0.45f;
};

// Calculate Intersection over Union (IoU) between two boxes
float calculate_iou(const box& box1, const box& box2) {
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
}

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
    std::vector<detection_cls_yolo11>& detections, 
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
}

// Merge and apply Non-Maximum Suppression
std::vector<detection_cls_yolo11> merge_detections(
    const std::vector<std::vector<detection_cls_yolo11>>& all_detections, 
    float iou_threshold, 
    float conf_threshold
) {
    std::vector<detection_cls_yolo11> all_boxes;

    // Collect detections above confidence threshold
    for (const auto& slice_detections : all_detections) {
        for (const auto& det : slice_detections) {
            if (det.confidence >= conf_threshold) {
                all_boxes.push_back(det);
            }
        }
    }

    // Sort by confidence (descending)
    std::sort(all_boxes.begin(), all_boxes.end(), 
        [](const detection_cls_yolo11& a, const detection_cls_yolo11& b) {
            return a.confidence > b.confidence;
        });

    // Non-Maximum Suppression
    std::vector<detection_cls_yolo11> final_detections;
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

    return final_detections;
}

} // namespace SAHI

#endif // SAHI_HPP