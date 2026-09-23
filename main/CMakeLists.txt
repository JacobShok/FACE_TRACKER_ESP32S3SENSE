file(GLOB TFLM_SOURCES
    "../edge-impulse-sdk/tensorflow/lite/kernels/*.cc"
    "../edge-impulse-sdk/tensorflow/lite/kernels/internal/*.cc"
    "../edge-impulse-sdk/tensorflow/lite/micro/*.cc"
    "../edge-impulse-sdk/tensorflow/lite/micro/kernels/*.cc"
    "../edge-impulse-sdk/tensorflow/lite/micro/memory_planner/*.cc"
    "../edge-impulse-sdk/tensorflow/lite/core/api/*.cc"
)

idf_component_register(SRCS "face_detect.cpp" "cam.c" "camera_module.c"
                        "../tflite-model/tflite_learn_1115932_19_compiled.cpp"
                        "../edge-impulse-sdk/porting/espressif/ei_classifier_porting.cpp"
                        "../edge-impulse-sdk/porting/espressif/debug_log.cpp"
                        "../edge-impulse-sdk/tensorflow/lite/c/common.c"
                        ${TFLM_SOURCES}
                       INCLUDE_DIRS "." "include" ".." "../edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/include"
                       REQUIRES esp_timer
                       PRIV_REQUIRES esp_driver_ledc
                       )

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers -Wno-error=maybe-uninitialized)
