#include <stdio.h>

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

// EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE

static uint16_t* input_buf;
static size_t* buf_size;

// Callback function declaration
static int get_signal_data(size_t offset, size_t length, float *out_ptr);

// Convert one RGB565 pixel into a packed 0xRRGGBB value (as EI's image DSP block expects)
static uint32_t rgb565_to_packed_rgb(uint16_t px) {
    uint8_t r5 = (px >> 11) & 0x1F;
    uint8_t g6 = (px >> 5)  & 0x3F;
    uint8_t b5 =  px        & 0x1F;

    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g6 << 2) | (g6 >> 4);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    return (r8 << 16) | (g8 << 8) | b8;
}

extern "C" int run_face_detection(uint16_t *cur_frame, size_t cur_frame_size) {

    signal_t signal;            // Wrapper for raw input buffer
    ei_impulse_result_t result; // Used to store inference output
    EI_IMPULSE_ERROR res;       // Return code from inference

    input_buf = cur_frame;
    buf_size = &cur_frame_size;

    // Make sure that the length of the buffer matches expected input length
    if (*buf_size != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        printf("ERROR: The size of the input buffer is not correct.\r\n");
        printf("Expected %d items, but got %d\r\n",
                EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
                (int)*buf_size);
        return 1;
    }

    // Assign callback function to fill buffer used for preprocessing/inference
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = &get_signal_data;

    // Perform DSP pre-processing and inference
    res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        printf("ERROR: Failed to run classifier (%d)\r\n", res);
        return res;
    }

    printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
            result.timing.dsp,
            result.timing.classification,
            result.timing.anomaly);

    // Print the prediction results (object detection)
    #if EI_CLASSIFIER_OBJECT_DETECTION == 1
        printf("Object detection bounding boxes:\r\n");
        for (uint32_t i = 0; i < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; i++) {
            ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
            if (bb.value == 0) {
                continue;
            }
            printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                    bb.label,
                    bb.value,
                    (unsigned int)bb.x,
                    (unsigned int)bb.y,
                    (unsigned int)bb.width,
                    (unsigned int)bb.height);
        }

        // Print the prediction results (classification)
    #else
        printf("Predictions:\r\n");
        for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            printf("  %s: ", ei_classifier_inferencing_categories[i]);
            printf("%.5f\r\n", result.classification[i].value);
        }
    #endif

        // Print anomaly result (if it exists)
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
        printf("Anomaly prediction: %.3f\r\n", result.anomaly);
    #endif

    return 0;
}

// Callback: fill a section of the out_ptr buffer when requested
static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = (float)rgb565_to_packed_rgb(input_buf[offset + i]);
    }

    return EIDSP_OK;
}