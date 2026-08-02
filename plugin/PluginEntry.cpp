#include "LinearPlugin.h"
#include "LabelEditorWin.h"

#include "linear/Detection.h"
#include "linear/Graphics.h"
#include "linear/PathGeometry.h"
#include "linear/Pixel.h"
#include "linear/Regions.h"
#include "linear/Stabilization.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iterator>
#include <list>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace linear::plugin {
namespace {

constexpr int kThresholdDefault = 18;
constexpr int kMinimumAreaDefault = 64;
constexpr int kStrokeWidthDefault = 2;
constexpr int kOpacityDefault = 100;
constexpr int kMaximumConnectionsDefault = 128;
constexpr int kAlphaThresholdDefault = 1;
constexpr int kMorphologyRadiusDefault = 1;
constexpr int kGridColumnsDefault = 5;
constexpr int kGridRowsDefault = 5;
constexpr int kOrbitPaddingDefault = 4;
constexpr int kRepeaterSpacingDefault = 16;
constexpr int kSmoothingStrengthDefault = 65;
constexpr int kConnectionRadiusDefault = 160;
constexpr int kMotionThresholdDefault = 8;
constexpr int kMotionSoftnessDefault = 4;
constexpr int kSceneCutDefault = 65;

constexpr std::uint32_t kSequenceMagic = 0x4C494E52U;

struct SequenceDataV1 {
    std::uint32_t magic{kSequenceMagic};
    std::uint32_t version{1U};
    std::array<char, 64U> customText{};
};

struct SequenceData {
    std::uint32_t magic{kSequenceMagic};
    std::uint32_t version{2U};
    std::array<char, 256U> customText{};
    std::array<char, 64U> fontFamily{};
};

static_assert(std::is_trivially_copyable_v<SequenceDataV1> &&
              std::is_trivially_copyable_v<SequenceData>,
              "Adobe sequence data must remain a flat POD payload");

template <std::size_t Capacity>
void storeString(std::array<char, Capacity>& destination,
                 const std::string& source) noexcept {
    destination.fill('\0');
    std::size_t count = std::min(source.size(), Capacity - 1U);
    while (count > 0U && count < source.size() &&
           (static_cast<unsigned char>(source[count]) & 0xC0U) == 0x80U) {
        --count;
    }
    std::memcpy(destination.data(), source.data(), count);
}

SequenceData defaultSequenceData() {
    SequenceData data{};
    storeString(data.customText, "CUSTOM");
    return data;
}

LabelEditorValues decodeSequenceData(const void* memory) {
    LabelEditorValues values{};
    if (memory == nullptr) return values;
    const auto* header = static_cast<const SequenceDataV1*>(memory);
    if (header->magic != kSequenceMagic) return values;
    if (header->version >= 2U) {
        const auto* data = static_cast<const SequenceData*>(memory);
        values.text.assign(
            data->customText.data(),
            strnlen(data->customText.data(), data->customText.size()));
        values.fontFamily.assign(
            data->fontFamily.data(),
            strnlen(data->fontFamily.data(), data->fontFamily.size()));
    } else {
        values.text.assign(
            header->customText.data(),
            strnlen(header->customText.data(), header->customText.size()));
    }
    if (values.text.empty()) values.text = "CUSTOM";
    return values;
}

SequenceData encodeSequenceData(const LabelEditorValues& values) {
    SequenceData data = defaultSequenceData();
    storeString(data.customText, values.text.empty() ? "CUSTOM" : values.text);
    storeString(data.fontFamily, values.fontFamily);
    return data;
}

PF_Err allocateSequenceData(
    PF_InData* inputData, PF_OutData* outputData,
    const SequenceData& source) {
    AEGP_SuiteHandler suites(inputData->pica_basicP);
    PF_Handle handle = suites.HandleSuite1()->host_new_handle(
        static_cast<A_long>(sizeof(SequenceData)));
    if (handle == nullptr) return PF_Err_OUT_OF_MEMORY;
    auto* destination = static_cast<SequenceData*>(
        suites.HandleSuite1()->host_lock_handle(handle));
    if (destination == nullptr) {
        suites.HandleSuite1()->host_dispose_handle(handle);
        return PF_Err_OUT_OF_MEMORY;
    }
    *destination = source;
    suites.HandleSuite1()->host_unlock_handle(handle);
    outputData->sequence_data = handle;
    return PF_Err_NONE;
}

PF_Err sequenceSetup(PF_InData* inputData, PF_OutData* outputData) {
    return allocateSequenceData(inputData, outputData, defaultSequenceData());
}

PF_Err sequenceSetdown(PF_InData* inputData, PF_OutData* outputData) {
    if (inputData->sequence_data != nullptr) {
        AEGP_SuiteHandler suites(inputData->pica_basicP);
        suites.HandleSuite1()->host_dispose_handle(inputData->sequence_data);
    }
    outputData->sequence_data = nullptr;
    return PF_Err_NONE;
}

PF_Err copySequenceData(
    PF_InData* inputData, PF_OutData* outputData,
    bool disposeInput) {
    if (inputData->sequence_data == nullptr) {
        return sequenceSetup(inputData, outputData);
    }
    AEGP_SuiteHandler suites(inputData->pica_basicP);
    PF_HandleSuite1* handleSuite = suites.HandleSuite1();
    const A_HandleSize sourceSize =
        handleSuite->host_get_handle_size(inputData->sequence_data);
    const void* source =
        suites.HandleSuite1()->host_lock_handle(inputData->sequence_data);
    if (source == nullptr || sourceSize < sizeof(std::uint32_t) * 2U) {
        if (source != nullptr) {
            handleSuite->host_unlock_handle(inputData->sequence_data);
        }
        const PF_Err result = allocateSequenceData(
            inputData, outputData, defaultSequenceData());
        if (result == PF_Err_NONE && disposeInput) {
            handleSuite->host_dispose_handle(inputData->sequence_data);
        }
        return result;
    }
    const SequenceData copy = encodeSequenceData(decodeSequenceData(source));
    suites.HandleSuite1()->host_unlock_handle(inputData->sequence_data);
    const PF_Err result = allocateSequenceData(inputData, outputData, copy);
    if (result == PF_Err_NONE && disposeInput) {
        suites.HandleSuite1()->host_dispose_handle(inputData->sequence_data);
    }
    return result;
}

LabelEditorValues readLabelSettings(PF_InData* inputData) {
    LabelEditorValues result{};
    bool loaded = false;
    const PF_EffectSequenceDataSuite1* sequenceSuite = nullptr;
    if (inputData->pica_basicP != nullptr &&
        inputData->pica_basicP->AcquireSuite(
            kPFEffectSequenceDataSuite,
            kPFEffectSequenceDataSuiteVersion1,
            reinterpret_cast<const void**>(&sequenceSuite)) == kSPNoError &&
        sequenceSuite != nullptr) {
        PF_ConstHandle handle = nullptr;
        if (sequenceSuite->PF_GetConstSequenceData(
                inputData->effect_ref, &handle) == PF_Err_NONE &&
            handle != nullptr && *handle != nullptr) {
            result = decodeSequenceData(*handle);
            loaded = true;
        }
        inputData->pica_basicP->ReleaseSuite(
            kPFEffectSequenceDataSuite,
            kPFEffectSequenceDataSuiteVersion1);
    }
    if (!loaded && inputData->sequence_data != nullptr) {
        AEGP_SuiteHandler suites(inputData->pica_basicP);
        const void* sequence =
            suites.HandleSuite1()->host_lock_handle(inputData->sequence_data);
        if (sequence != nullptr) result = decodeSequenceData(sequence);
        if (sequence != nullptr) {
            suites.HandleSuite1()->host_unlock_handle(inputData->sequence_data);
        }
    }
    return result;
}

PF_Err editLabelSettings(PF_InData* inputData, PF_OutData* outputData) {
    if (inputData->sequence_data == nullptr) return PF_Err_NONE;
    LabelEditorValues values = readLabelSettings(inputData);
    if (!showLabelEditor(values)) return PF_Err_NONE;
    AEGP_SuiteHandler suites(inputData->pica_basicP);
    PF_HandleSuite1* handleSuite = suites.HandleSuite1();
    PF_Handle handle = inputData->sequence_data;
    if (handleSuite->host_get_handle_size(handle) < sizeof(SequenceData)) {
        const PF_Err resizeError = handleSuite->host_resize_handle(
            static_cast<A_HandleSize>(sizeof(SequenceData)), &handle);
        if (resizeError != PF_Err_NONE) return resizeError;
        inputData->sequence_data = handle;
        outputData->sequence_data = handle;
    }
    auto* sequence = static_cast<SequenceData*>(
        handleSuite->host_lock_handle(handle));
    if (sequence == nullptr) return PF_Err_OUT_OF_MEMORY;
    *sequence = encodeSequenceData(values);
    handleSuite->host_unlock_handle(handle);
    outputData->out_flags |= PF_OutFlag_REFRESH_UI | PF_OutFlag_FORCE_RERENDER;
    return PF_Err_NONE;
}

PF_Err about(PF_InData* inputData, PF_OutData* outputData) {
    AEGP_SuiteHandler suites(inputData->pica_basicP);
    suites.ANSICallbacksSuite1()->sprintf(
        outputData->return_msg,
        "%s v%d.%d.%d\rNative color-region analysis and technical overlays.",
        LINEAR_PRODUCT_NAME,
        LINEAR_VERSION_MAJOR,
        LINEAR_VERSION_MINOR,
        LINEAR_VERSION_PATCH);
    return PF_Err_NONE;
}

PF_Err globalSetup(PF_OutData* outputData) {
    outputData->my_version =
        PF_VERSION(LINEAR_VERSION_MAJOR, LINEAR_VERSION_MINOR,
                   LINEAR_VERSION_PATCH, PF_Stage_DEVELOP, 1);
    outputData->out_flags =
        PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PIX_INDEPENDENT |
        PF_OutFlag_USE_OUTPUT_EXTENT | PF_OutFlag_WIDE_TIME_INPUT |
        PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING;
    outputData->out_flags2 =
        PF_OutFlag2_SUPPORTS_THREADED_RENDERING |
        PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG |
        PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA;
    return PF_Err_NONE;
}

PF_Err parameterSetup(PF_InData* in_data, PF_OutData* outputData) {
    PF_Err err = PF_Err_NONE;
    PF_ParamDef def{};

    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPIC("01  DETECTION", DetectionGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Target Color", PF_MAX_CHAN8, 32, 32, TargetColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Color Space", 3, 1, "RGB|HSV|Lab", ColorSpaceDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Color Threshold", 0, 100, 0, 100,
                         kThresholdDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, ThresholdDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Minimum Region Area", 1, 100000, 1, 10000,
                         kMinimumAreaDefault, PF_Precision_INTEGER,
                         0, 0, MinimumAreaDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Invert Selection", "Select colors outside the tolerance",
                    FALSE, 0, InvertDetectionDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Ignore Transparent Pixels",
                    "Exclude pixels below the alpha threshold",
                    TRUE, 0, IgnoreTransparentDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Alpha Threshold", 0, 100, 0, 100,
                         kAlphaThresholdDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, AlphaThresholdDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Pre Blur Radius", 0, 8, 0, 8, 0,
                         PF_Precision_INTEGER, 0, 0, PreBlurRadiusDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Post Blur Radius", 0, 8, 0, 8, 0,
                         PF_Precision_INTEGER, 0, 0, PostBlurRadiusDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Morphology Radius", 0, 8, 0, 8,
                         kMorphologyRadiusDefault, PF_Precision_INTEGER,
                         0, 0, MorphologyRadiusDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(DetectionGroupEndDiskId);

    PF_ADD_TOPICX("02  TRACKING & SMOOTHING", PF_ParamFlag_START_COLLAPSED,
                  TrackingGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Stabilization Mode", 5, 1,
                 "Legacy Raw|Basic Smooth|Adaptive Smooth|Cinematic Smooth|Predictive Smooth",
                 StabilizationModeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Position Smoothing", 0, 100, 0, 100,
                         kSmoothingStrengthDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, PositionSmoothingDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Size Smoothing", 0, 100, 0, 100,
                         kSmoothingStrengthDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, SizeSmoothingDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Capture Persistence", 0, 100, 0, 100, 0,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         CapturePersistenceDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Detection Source", 4, 1,
                 "Color|Motion|Color + Motion|Color AND Motion",
                 DetectionSourceDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Motion Threshold", 0, 100, 0, 100,
                         kMotionThresholdDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         MotionThresholdDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Motion Softness", 0, 100, 0, 100,
                         kMotionSoftnessDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         MotionSoftnessDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Scene Cut Guard", 5, 100, 5, 100,
                         kSceneCutDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         MotionSceneCutDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Motion Cleanup", 0, 4, 0, 4, 1,
                         PF_Precision_INTEGER, 0, 0,
                         MotionNoiseRadiusDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Camera Compensation",
                    "Compensate small whole-frame translations",
                    FALSE, 0, MotionCompensateDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Maximum Camera Shift", 0, 32, 0, 16, 8,
                         PF_Precision_INTEGER, 0, 0,
                         MotionMaximumTranslationDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(TrackingGroupEndDiskId);

    PF_ADD_TOPICX("03  OUTPUT", PF_ParamFlag_START_COLLAPSED,
                  OutputGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Mode", 3, 1,
                 "Source + Overlay|Overlay Only|Detection Mask",
                 OutputModeDiskId);

    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    PF_ADD_COLOR("Overlay Color", 0, PF_MAX_CHAN8, 192, StrokeColorDiskId);

    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    PF_ADD_FLOAT_SLIDERX("Stroke Width", 1, 20, 1, 20,
                         kStrokeWidthDefault, PF_Precision_HUNDREDTHS,
                         0, 0, StrokeWidthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Overlay Opacity", 0, 100, 0, 100,
                         kOpacityDefault, PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, OverlayOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    def.ui_flags = PF_PUI_INVISIBLE;
    PF_ADD_POPUP("Style Source", 2, 1,
                 "Global (Compatible)|Per Section", StyleSourceDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Bounding Boxes", "Draw a box around each detected region",
                    TRUE, 0, BoxesEnabledDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Box Shape", 3, 1,
                 "Corner Brackets|Rectangle|Ellipse", BoxShapeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Box Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, BoxOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Box Stroke Width", 1, 20, 1, 20, 2,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         BoxStrokeWidthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Box Padding", 0, 200, 0, 100, 4,
                         PF_Precision_HUNDREDTHS, 0, 0, BoxPaddingDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Corner Length", 5, 50, 5, 50, 25,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         BoxCornerLengthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Center Markers", "Draw a marker at each region centroid",
                    TRUE, 0, MarkersEnabledDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Marker Shape", 9, 1,
                 "Ring|Dot|Plus|Cross|Triangle|Star|Diamond|Target|Chevron",
                 MarkerShapeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Marker Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, MarkerOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Marker Stroke Width", 1, 20, 1, 20, 2,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         MarkerStrokeWidthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Marker Size", 2, 200, 2, 100, 8,
                         PF_Precision_HUNDREDTHS, 0, 0, MarkerSizeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(OutputGroupEndDiskId);

    PF_ADD_TOPICX("04  NETWORK", PF_ParamFlag_START_COLLAPSED,
                  NetworkGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Connection Mode", 7, 1,
                 "Off|Sequential|Nearest|Minimum Spanning Tree|Star|Full Mesh|Radius (Proximity)",
                 ConnectionModeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Maximum Connections", 1, 256, 1, 256,
                         kMaximumConnectionsDefault, PF_Precision_INTEGER,
                         0, 0, MaximumConnectionsDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Connection Radius", 1, 2000, 1, 1000,
                         kConnectionRadiusDefault, PF_Precision_HUNDREDTHS,
                         0, 0, ConnectionRadiusDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Connection Style", 5, 1,
                 "Straight|Rounded Arc|Smooth Bend|Step Bend|PCB Trace",
                 ConnectionStyleDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Arc Curvature", 0, 100, 0, 100, 25,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         ConnectionCurvatureDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Network Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         NetworkOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Network Thickness", 1, 20, 1, 20, 2,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         NetworkThicknessDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Dashed Lines", "Enable dash and gap pattern",
                    FALSE, 0, NetworkDashEnabledDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Dash Length", 1, 200, 1, 100, 10,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         NetworkDashLengthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Gap Length", 1, 200, 1, 100, 5,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         NetworkGapLengthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Arrowheads", "Draw arrowheads at connection ends",
                    FALSE, 0, NetworkArrowsEnabledDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Arrow Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         NetworkArrowOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Arrow Size", 2, 100, 2, 60, 12,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         NetworkArrowSizeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(NetworkGroupEndDiskId);

    PF_ADD_TOPICX("05  LABELS & CALLOUTS", PF_ParamFlag_START_COLLAPSED,
                  LabelsGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPICX("Content & Typography", PF_ParamFlag_START_COLLAPSED,
                  LabelContentGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Label Content", 7, 1,
                 "Off|Node ID|Area|Dimensions|Track ID|Confidence|Custom Text",
                 LabelModeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_BUTTON("Text & Font", "Edit Label...",
                  PF_PUI_NONE, PF_ParamFlag_SUPERVISE,
                  LabelEditorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Text Size", 1, 5, 1, 5, 2,
                         PF_Precision_INTEGER, 0, 0, LabelScaleDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Label Limit", 1, 128, 1, 64, 32,
                         PF_Precision_INTEGER, 0, 0, LabelLimitDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Text Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         LabelTextOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(LabelContentGroupEndDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPICX("Panel Appearance", PF_ParamFlag_START_COLLAPSED,
                  LabelPanelGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Fill Opacity", 0, 100, 0, 100, 72,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, LabelBackgroundDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Padding", 0, 32, 0, 24, 3,
                         PF_Precision_INTEGER, 0, 0,
                         LabelPanelPaddingDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Corner Radius", 0, 64, 0, 32, 4,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         LabelPanelCornerRadiusDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Border Opacity", 0, 100, 0, 100, 0,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         LabelPanelStrokeOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Border Thickness", 1, 12, 1, 8, 1,
                         PF_Precision_INTEGER, 0, 0,
                         LabelPanelStrokeThicknessDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Shadow Opacity", 0, 100, 0, 100, 0,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         LabelPanelShadowOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Shadow Offset X", -64, 64, -32, 32, 3,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         LabelPanelShadowOffsetXDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Shadow Offset Y", -64, 64, -32, 32, 3,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         LabelPanelShadowOffsetYDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(LabelPanelGroupEndDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_TOPICX("Placement & Leaders", PF_ParamFlag_START_COLLAPSED,
                  LabelLayoutGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Horizontal Offset", -1000, 1000, -500, 500, 0,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         LabelOffsetXDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Vertical Offset", -1000, 1000, -500, 500, 0,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         LabelOffsetYDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Leader Lines", "Connect labels to regions",
                    TRUE, 0, LabelLeadersDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Leader Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         LabelLeaderOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(LabelLayoutGroupEndDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(LabelsGroupEndDiskId);

    PF_ADD_TOPICX("06  REGION FILL", PF_ParamFlag_START_COLLAPSED,
                  FillGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Mode", 7, 1,
                 "Off|Solid|Hatch|X-Ray|Duotone|Crosshatch|Invert",
                 FillModeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Opacity", 0, 100, 0, 100, 35,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, FillOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Box Inset", 0, 200, 0, 100, 0,
                         PF_Precision_HUNDREDTHS, 0, 0, FillInsetDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Edge Feather", 0, 100, 0, 50, 0,
                         PF_Precision_HUNDREDTHS, 0, 0, FillFeatherDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Invert Strength", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         FillInvertStrengthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Hatch Spacing", 3, 64, 3, 32, 12,
                         PF_Precision_INTEGER, 0, 0, HatchSpacingDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Hatch Width", 1, 32, 1, 16, 3,
                         PF_Precision_INTEGER, 0, 0, HatchWidthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_ANGLE("Hatch Angle", 45, HatchAngleDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(FillGroupEndDiskId);

    PF_ADD_TOPICX("07  MEASUREMENT HUD", PF_ParamFlag_START_COLLAPSED,
                  HudGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Grid", "Draw a technical background grid",
                    FALSE, 0, GridEnabledDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("View Mode", 2, 2,
                 "Simple|Cartesian", GridModeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Grid Columns", 2, 64, 2, 32,
                         kGridColumnsDefault, PF_Precision_INTEGER,
                         0, 0, GridColumnsDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Grid Rows", 2, 64, 2, 32,
                         kGridRowsDefault, PF_Precision_INTEGER,
                         0, 0, GridRowsDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Grid Opacity", 0, 100, 0, 100, 35,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, GridOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Grid Thickness", 1, 10, 1, 10, 1,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         GridThicknessDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Axis Labels", "Show -100 to 100 axis values",
                    TRUE, 0, GridAxisLabelsDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Tick Size", 0, 32, 0, 24, 6,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         GridTickSizeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Node Projections", "Project detected nodes to both axes",
                    TRUE, 0, GridProjectionsDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Projection Opacity", 0, 100, 0, 100, 85,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0,
                         ProjectionOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Projection Dash", 1, 200, 1, 100, 8,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         ProjectionDashLengthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Projection Gap", 1, 200, 1, 100, 6,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         ProjectionGapLengthDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(HudGroupEndDiskId);

    PF_ADD_TOPICX("08  TECHNICAL PATHS", PF_ParamFlag_START_COLLAPSED,
                  PathsGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Path Mode", 3, 1,
                 "Off|Orbit|Contour", TechnicalPathModeDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Orbit Padding", 0, 200, 0, 100,
                         kOrbitPaddingDefault, PF_Precision_HUNDREDTHS,
                         0, 0, OrbitPaddingDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Repeater", 5, 1,
                 "Off|Ring|Box|Arrow|Tick", RepeaterPrimitiveDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Repeater Spacing", 2, 200, 2, 100,
                         kRepeaterSpacingDefault, PF_Precision_HUNDREDTHS,
                         0, 0, RepeaterSpacingDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Path Opacity", 0, 100, 0, 100, 100,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_PERCENT, 0, PathOpacityDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_FLOAT_SLIDERX("Path Thickness", 1, 20, 1, 20, 2,
                         PF_Precision_HUNDREDTHS, 0, 0,
                         PathThicknessDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(PathsGroupEndDiskId);

    PF_ADD_TOPICX("09  COLORS", PF_ParamFlag_START_COLLAPSED,
                  ColorsGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Box Stroke", 0, PF_MAX_CHAN8, 192,
                 BoxStrokeColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Center Marker", 0, PF_MAX_CHAN8, 192,
                 MarkerColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Network Lines", 0, PF_MAX_CHAN8, 192,
                 NetworkColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Network Arrows", 210, 236, PF_MAX_CHAN8,
                 NetworkArrowColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Grid Lines", PF_MAX_CHAN8, PF_MAX_CHAN8, PF_MAX_CHAN8,
                 GridColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Grid Projections", 140, PF_MAX_CHAN8, 38,
                 ProjectionColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Label Text", 0, PF_MAX_CHAN8, 192,
                 LabelTextColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Label Panel Fill", 5, 20, 18, LabelPanelColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Label Leaders", 0, PF_MAX_CHAN8, 192,
                 LabelLeaderColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Label Border", 0, PF_MAX_CHAN8, 192,
                 LabelPanelStrokeColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Label Shadow", 0, 0, 0,
                 LabelPanelShadowColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Fill Primary", 0, PF_MAX_CHAN8, 192, FillColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Fill Secondary", 24, 4, 48, FillSecondaryColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_COLOR("Technical Paths", 0, PF_MAX_CHAN8, 192,
                 PathColorDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(ColorsGroupEndDiskId);

    PF_ADD_TOPICX("10  PERFORMANCE", PF_ParamFlag_START_COLLAPSED,
                  PerformanceGroupStartDiskId);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Analysis Resolution", 3, 2,
                 "Full|Half|Quarter", AnalysisResolutionDiskId);

    AEFX_CLR_STRUCT(def);
    PF_END_TOPIC(PerformanceGroupEndDiskId);

    outputData->num_params = ParameterCount;
    return err;
}

template <typename Pixel, int Maximum>
Image decodeWorld(const PF_EffectWorld& world) {
    if (world.width < 0 || world.height < 0 || world.data == nullptr) {
        throw std::invalid_argument("Invalid input world");
    }
    Image image(static_cast<int>(world.width), static_cast<int>(world.height));
    auto& pixels = image.pixels();
    const auto* data = reinterpret_cast<const std::byte*>(world.data);
    for (A_long y = 0; y < world.height; ++y) {
        const auto* row = reinterpret_cast<const Pixel*>(
            data + static_cast<std::ptrdiff_t>(y) * world.rowbytes);
        for (A_long x = 0; x < world.width; ++x) {
            const float alpha =
                static_cast<float>(row[x].alpha) / static_cast<float>(Maximum);
            Color color{
                static_cast<float>(row[x].red) / static_cast<float>(Maximum),
                static_cast<float>(row[x].green) / static_cast<float>(Maximum),
                static_cast<float>(row[x].blue) / static_cast<float>(Maximum),
                alpha};
            if (alpha > 1.0e-7F && alpha < 1.0F) {
                color.r /= alpha;
                color.g /= alpha;
                color.b /= alpha;
            }
            pixels[static_cast<std::size_t>(y) * world.width + x] = color;
        }
    }
    return image;
}

template <typename Pixel, int Maximum>
Image decodeWorldSampled(const PF_EffectWorld& world, int divisor) {
    if (world.width < 0 || world.height < 0 || world.data == nullptr) {
        throw std::invalid_argument("Invalid input world");
    }
    divisor = std::clamp(divisor, 1, 4);
    const int width = std::max(1, (static_cast<int>(world.width) + divisor - 1) /
                                      divisor);
    const int height = std::max(1, (static_cast<int>(world.height) + divisor - 1) /
                                       divisor);
    Image image(width, height);
    auto& pixels = image.pixels();
    const auto* data = reinterpret_cast<const std::byte*>(world.data);
    for (int y = 0; y < height; ++y) {
        const int sourceY = std::min(static_cast<int>(world.height) - 1, y * divisor);
        const auto* row = reinterpret_cast<const Pixel*>(
            data + static_cast<std::ptrdiff_t>(sourceY) * world.rowbytes);
        const std::size_t destinationRow = static_cast<std::size_t>(y) * width;
        for (int x = 0; x < width; ++x) {
            const int sourceX = std::min(static_cast<int>(world.width) - 1, x * divisor);
            const float alpha = static_cast<float>(row[sourceX].alpha) /
                                static_cast<float>(Maximum);
            Color color{
                static_cast<float>(row[sourceX].red) / Maximum,
                static_cast<float>(row[sourceX].green) / Maximum,
                static_cast<float>(row[sourceX].blue) / Maximum,
                alpha};
            if (alpha > 1.0e-7F && alpha < 1.0F) {
                color.r /= alpha;
                color.g /= alpha;
                color.b /= alpha;
            }
            pixels[destinationRow + static_cast<std::size_t>(x)] = color;
        }
    }
    return image;
}

template <typename Pixel, int Maximum>
void encodeWorld(const Image& image, PF_EffectWorld& world) {
    if (image.width() != world.width || image.height() != world.height ||
        world.data == nullptr) {
        throw std::invalid_argument("Invalid output world");
    }
    auto* data = reinterpret_cast<std::byte*>(world.data);
    const auto& pixels = image.pixels();
    for (A_long y = 0; y < world.height; ++y) {
        auto* row = reinterpret_cast<Pixel*>(
            data + static_cast<std::ptrdiff_t>(y) * world.rowbytes);
        for (A_long x = 0; x < world.width; ++x) {
            Color color = pixels[static_cast<std::size_t>(y) * world.width + x];
            color.a = clamp01(color.a);
            const float red = clamp01(color.r) * color.a;
            const float green = clamp01(color.g) * color.a;
            const float blue = clamp01(color.b) * color.a;
            row[x].alpha = static_cast<decltype(row[x].alpha)>(
                color.a * Maximum + 0.5F);
            row[x].red = static_cast<decltype(row[x].red)>(
                red * Maximum + 0.5F);
            row[x].green = static_cast<decltype(row[x].green)>(
                green * Maximum + 0.5F);
            row[x].blue = static_cast<decltype(row[x].blue)>(
                blue * Maximum + 0.5F);
        }
    }
}

Image decodeInput(const PF_LayerDef* input) {
    if (PF_WORLD_IS_DEEP(input)) {
        return decodeWorld<PF_Pixel16, PF_MAX_CHAN16>(*input);
    }
    return decodeWorld<PF_Pixel8, PF_MAX_CHAN8>(*input);
}

Image decodeInputSampled(const PF_LayerDef* input, int divisor) {
    if (PF_WORLD_IS_DEEP(input)) {
        return decodeWorldSampled<PF_Pixel16, PF_MAX_CHAN16>(*input, divisor);
    }
    return decodeWorldSampled<PF_Pixel8, PF_MAX_CHAN8>(*input, divisor);
}

void encodeOutput(const Image& image, PF_LayerDef* output) {
    if (PF_WORLD_IS_DEEP(output)) {
        encodeWorld<PF_Pixel16, PF_MAX_CHAN16>(image, *output);
    } else {
        encodeWorld<PF_Pixel8, PF_MAX_CHAN8>(image, *output);
    }
}

Color colorParameter(const PF_ParamDef* parameter) noexcept {
    return {
        parameter->u.cd.value.red / static_cast<float>(PF_MAX_CHAN8),
        parameter->u.cd.value.green / static_cast<float>(PF_MAX_CHAN8),
        parameter->u.cd.value.blue / static_cast<float>(PF_MAX_CHAN8),
        1.0F};
}

OutputMode outputModeParameter(const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2:
        return OutputMode::OverlayOnly;
    case 3:
        return OutputMode::DetectionMask;
    default:
        return OutputMode::SourceAndOverlay;
    }
}

StabilizationMode stabilizationModeParameter(
    const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2: return StabilizationMode::BasicSmooth;
    case 3: return StabilizationMode::AdaptiveSmooth;
    case 4: return StabilizationMode::CinematicSmooth;
    case 5: return StabilizationMode::PredictiveSmooth;
    default: return StabilizationMode::LegacyRaw;
    }
}

bool connectionsEnabledParameter(const PF_ParamDef* parameter) noexcept {
    return parameter->u.pd.value >= 2 && parameter->u.pd.value <= 7;
}

ConnectionTopology connectionTopologyParameter(
    const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2:
        return ConnectionTopology::Sequential;
    case 4:
        return ConnectionTopology::MinimumSpanningTree;
    case 5:
        return ConnectionTopology::Star;
    case 6:
        return ConnectionTopology::FullMesh;
    case 7:
        return ConnectionTopology::Radius;
    case 3:
    default:
        return ConnectionTopology::NearestNeighbor;
    }
}

ConnectionStyle connectionStyleParameter(
    const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2: return ConnectionStyle::RoundedArc;
    case 3: return ConnectionStyle::SmoothBend;
    case 4: return ConnectionStyle::StepBend;
    case 5: return ConnectionStyle::PcbTrace;
    default: return ConnectionStyle::Straight;
    }
}

ColorSpace colorSpaceParameter(const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2:
        return ColorSpace::Hsv;
    case 3:
        return ColorSpace::Lab;
    case 1:
    default:
        return ColorSpace::Rgb;
    }
}

BoxShape boxShapeParameter(const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2:
        return BoxShape::Rectangle;
    case 3:
        return BoxShape::Ellipse;
    case 1:
    default:
        return BoxShape::CornerBrackets;
    }
}

MarkerShape markerShapeParameter(const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2:
        return MarkerShape::Dot;
    case 3:
        return MarkerShape::Plus;
    case 4:
        return MarkerShape::Cross;
    case 5: return MarkerShape::Triangle;
    case 6: return MarkerShape::Star;
    case 7: return MarkerShape::Diamond;
    case 8: return MarkerShape::Target;
    case 9: return MarkerShape::Chevron;
    case 1:
    default:
        return MarkerShape::Ring;
    }
}

int analysisDivisorParameter(const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 1: return 1;
    case 3: return 4;
    case 2: default: return 2;
    }
}

LabelMode labelModeParameter(const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2: return LabelMode::NodeId;
    case 3: return LabelMode::Area;
    case 4: return LabelMode::Dimensions;
    case 5: return LabelMode::TrackId;
    case 6: return LabelMode::Confidence;
    case 7: return LabelMode::CustomText;
    default: return LabelMode::Off;
    }
}

enum class DetectionSource {
    Color,
    Motion,
    ColorOrMotion,
    ColorAndMotion
};

DetectionSource detectionSourceParameter(
    const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2: return DetectionSource::Motion;
    case 3: return DetectionSource::ColorOrMotion;
    case 4: return DetectionSource::ColorAndMotion;
    default: return DetectionSource::Color;
    }
}

FillMode fillModeParameter(const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2: return FillMode::Solid;
    case 3: return FillMode::Hatch;
    case 4: return FillMode::XRay;
    case 5: return FillMode::Duotone;
    case 6: return FillMode::Crosshatch;
    case 7: return FillMode::Invert;
    default: return FillMode::Off;
    }
}

GridMode gridModeParameter(const PF_ParamDef* parameter) noexcept {
    return parameter->u.pd.value == 1
        ? GridMode::Simple
        : GridMode::Cartesian;
}

enum class TechnicalPathMode {
    Off,
    Orbit,
    Contour
};

TechnicalPathMode technicalPathModeParameter(
    const PF_ParamDef* parameter) noexcept {
    switch (parameter->u.pd.value) {
    case 2:
        return TechnicalPathMode::Orbit;
    case 3:
        return TechnicalPathMode::Contour;
    case 1:
    default:
        return TechnicalPathMode::Off;
    }
}

bool repeaterPrimitiveParameter(
    const PF_ParamDef* parameter,
    RepeaterPrimitive& primitive) noexcept {
    switch (parameter->u.pd.value) {
    case 2:
        primitive = RepeaterPrimitive::Ring;
        return true;
    case 3:
        primitive = RepeaterPrimitive::Box;
        return true;
    case 4:
        primitive = RepeaterPrimitive::Arrow;
        return true;
    case 5:
        primitive = RepeaterPrimitive::Tick;
        return true;
    case 1:
    default:
        return false;
    }
}

struct Analysis {
    Mask mask;
    std::vector<Region> regions;
    int divisor{1};
};

struct AnalysisCacheKey {
    std::uint64_t contentHash{};
    std::uint64_t settingsHash{};
    int width{};
    int height{};
    int divisor{};

    bool operator==(const AnalysisCacheKey& other) const noexcept {
        return contentHash == other.contentHash &&
               settingsHash == other.settingsHash && width == other.width &&
               height == other.height && divisor == other.divisor;
    }
};

class AnalysisCache {
public:
    struct State {
        std::mutex mutex;
        std::condition_variable readyCondition;
        std::shared_ptr<const Analysis> analysis;
        std::exception_ptr failure;
        bool ready{};
    };

    struct Ticket {
        std::shared_ptr<State> state;
        bool producer{};
    };

    Ticket acquire(const AnalysisCacheKey& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = std::find_if(
            entries_.begin(), entries_.end(),
            [&](const Entry& entry) { return entry.key == key; });
        if (found != entries_.end()) {
            entries_.splice(entries_.begin(), entries_, found);
            return {entries_.front().state, false};
        }
        auto state = std::make_shared<State>();
        entries_.push_front({key, state, 0U});
        return {std::move(state), true};
    }

    void complete(
        const AnalysisCacheKey& key,
        const std::shared_ptr<State>& state,
        std::shared_ptr<const Analysis> analysis) {
        const std::size_t bytes =
            analysis->mask.values().size() * sizeof(float) +
            analysis->regions.size() * sizeof(Region);
        {
            std::lock_guard<std::mutex> stateLock(state->mutex);
            state->analysis = std::move(analysis);
            state->ready = true;
        }
        state->readyCondition.notify_all();
        std::lock_guard<std::mutex> cacheLock(mutex_);
        const auto entry = std::find_if(
            entries_.begin(), entries_.end(), [&](const Entry& candidate) {
                return candidate.key == key && candidate.state == state;
            });
        if (entry != entries_.end()) {
            bytes_ -= entry->bytes;
            entry->bytes = bytes;
            bytes_ += bytes;
            entries_.splice(entries_.begin(), entries_, entry);
        }
        evictReadyEntries();
    }

    void fail(
        const AnalysisCacheKey& key,
        const std::shared_ptr<State>& state,
        std::exception_ptr failure) noexcept {
        {
            std::lock_guard<std::mutex> stateLock(state->mutex);
            state->failure = failure;
            state->ready = true;
        }
        state->readyCondition.notify_all();
        std::lock_guard<std::mutex> cacheLock(mutex_);
        const auto entry = std::find_if(
            entries_.begin(), entries_.end(), [&](const Entry& candidate) {
                return candidate.key == key && candidate.state == state;
            });
        if (entry != entries_.end()) {
            bytes_ -= entry->bytes;
            entries_.erase(entry);
        }
    }

    static std::shared_ptr<const Analysis> wait(
        const std::shared_ptr<State>& state) {
        std::unique_lock<std::mutex> lock(state->mutex);
        state->readyCondition.wait(lock, [&] { return state->ready; });
        if (state->failure) std::rethrow_exception(state->failure);
        return state->analysis;
    }

private:
    struct Entry {
        AnalysisCacheKey key;
        std::shared_ptr<State> state;
        std::size_t bytes{};
    };
    void evictReadyEntries() {
        while (entries_.size() > kMaximumEntries ||
               (bytes_ > kMaximumBytes && entries_.size() > 1U)) {
            auto candidate = entries_.end();
            for (auto reverse = entries_.rbegin();
                 reverse != entries_.rend(); ++reverse) {
                std::lock_guard<std::mutex> stateLock(reverse->state->mutex);
                if (reverse->state->ready) {
                    candidate = std::prev(reverse.base());
                    break;
                }
            }
            if (candidate == entries_.end()) break;
            bytes_ -= candidate->bytes;
            entries_.erase(candidate);
        }
    }
    static constexpr std::size_t kMaximumEntries = 16U;
    static constexpr std::size_t kMaximumBytes = 128U * 1024U * 1024U;
    std::mutex mutex_;
    std::list<Entry> entries_;
    std::size_t bytes_{};
};

AnalysisCache& analysisCache() {
    static AnalysisCache cache;
    return cache;
}

void hashBytes(
    std::uint64_t& hash, const void* data, std::size_t byteCount) noexcept {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    while (byteCount >= sizeof(std::uint64_t)) {
        std::uint64_t lane{};
        std::memcpy(&lane, bytes, sizeof(lane));
        hash ^= lane + 0x9E3779B97F4A7C15ULL +
                (hash << 6U) + (hash >> 2U);
        hash *= 1099511628211ULL;
        hash ^= hash >> 29U;
        bytes += sizeof(lane);
        byteCount -= sizeof(lane);
    }
    while (byteCount-- > 0U) {
        hash ^= *bytes++;
        hash *= 1099511628211ULL;
    }
}

template <typename Value>
void hashValue(std::uint64_t& hash, const Value& value) noexcept {
    hashBytes(hash, &value, sizeof(value));
}

std::uint64_t imageContentHash(const Image& image) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const int width = image.width();
    const int height = image.height();
    hashValue(hash, width);
    hashValue(hash, height);
    const auto& pixels = image.pixels();
    if (!pixels.empty()) {
        hashBytes(hash, pixels.data(), pixels.size() * sizeof(Color));
    }
    return hash;
}

std::uint64_t analysisSettingsHash(
    PF_ParamDef* parameters[], int divisor) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto hashLong = [&](A_long value) { hashValue(hash, value); };
    const auto hashDouble = [&](double value) { hashValue(hash, value); };
    hashLong(parameters[TargetColor]->u.cd.value.red);
    hashLong(parameters[TargetColor]->u.cd.value.green);
    hashLong(parameters[TargetColor]->u.cd.value.blue);
    hashLong(parameters[ColorSpaceParam]->u.pd.value);
    hashDouble(parameters[Threshold]->u.fs_d.value);
    hashDouble(parameters[MinimumArea]->u.fs_d.value);
    hashLong(parameters[InvertDetectionParam]->u.bd.value);
    hashLong(parameters[IgnoreTransparentParam]->u.bd.value);
    hashDouble(parameters[AlphaThresholdParam]->u.fs_d.value);
    hashDouble(parameters[PreBlurRadiusParam]->u.fs_d.value);
    hashDouble(parameters[PostBlurRadiusParam]->u.fs_d.value);
    hashDouble(parameters[MorphologyRadiusParam]->u.fs_d.value);
    hashLong(parameters[DetectionSourceParam]->u.pd.value);
    if (detectionSourceParameter(parameters[DetectionSourceParam]) !=
        DetectionSource::Color) {
        hashDouble(parameters[MotionThresholdParam]->u.fs_d.value);
        hashDouble(parameters[MotionSoftnessParam]->u.fs_d.value);
        hashDouble(parameters[MotionSceneCutParam]->u.fs_d.value);
        hashDouble(parameters[MotionNoiseRadiusParam]->u.fs_d.value);
        hashLong(parameters[MotionCompensateParam]->u.bd.value);
        hashDouble(parameters[MotionMaximumTranslationParam]->u.fs_d.value);
    }
    hashValue(hash, divisor);
    return hash;
}

void scaleRegions(std::vector<Region>& regions, int divisor) {
    if (divisor <= 1) return;
    const float scale = static_cast<float>(divisor);
    for (Region& region : regions) {
        region.bounds.x *= scale;
        region.bounds.y *= scale;
        region.bounds.width *= scale;
        region.bounds.height *= scale;
        region.centroid.x *= scale;
        region.centroid.y *= scale;
        region.weightedCentroid.x *= scale;
        region.weightedCentroid.y *= scale;
        region.perimeter *= scale;
        region.pixelArea *= static_cast<std::size_t>(divisor * divisor);
    }
}

Analysis analyzeSampledUncached(
    const Image& sampledSource,
    const Image* motionReference,
    PF_ParamDef* parameters[], int divisor) {
    divisor = std::clamp(divisor, 1, 4);
    ColorDetectionSettings detection{};
    detection.target = colorParameter(parameters[TargetColor]);
    detection.colorSpace = colorSpaceParameter(parameters[ColorSpaceParam]);
    detection.threshold =
        clamp01(static_cast<float>(parameters[Threshold]->u.fs_d.value / 100.0));
    detection.softness = 0.03F;
    detection.ignoreTransparent =
        parameters[IgnoreTransparentParam]->u.bd.value != FALSE;
    detection.alphaThreshold = clamp01(static_cast<float>(
        parameters[AlphaThresholdParam]->u.fs_d.value / 100.0));
    detection.invert =
        parameters[InvertDetectionParam]->u.bd.value != FALSE;
    const auto scaledRadius = [divisor](double value) {
        const int radius = static_cast<int>(std::clamp(value, 0.0, 8.0));
        return radius == 0 ? 0 : std::max(1, radius / divisor);
    };
    detection.preBlurRadius = scaledRadius(
        parameters[PreBlurRadiusParam]->u.fs_d.value);
    detection.postBlurRadius = scaledRadius(
        parameters[PostBlurRadiusParam]->u.fs_d.value);
    detection.morphologyRadius = scaledRadius(
        parameters[MorphologyRadiusParam]->u.fs_d.value);
    const DetectionSource sourceMode =
        detectionSourceParameter(parameters[DetectionSourceParam]);
    const bool needsColorMask = sourceMode != DetectionSource::Motion ||
                                motionReference == nullptr;
    Mask colorMask = needsColorMask
        ? detectColor(sampledSource, detection)
        : Mask(sampledSource.width(), sampledSource.height());
    Mask mask = colorMask;
    if (sourceMode != DetectionSource::Color && motionReference != nullptr) {
        MotionDetectionSettings motion{};
        motion.threshold = clamp01(static_cast<float>(
            parameters[MotionThresholdParam]->u.fs_d.value / 100.0));
        motion.softness = clamp01(static_cast<float>(
            parameters[MotionSoftnessParam]->u.fs_d.value / 100.0));
        motion.sceneCutThreshold = clamp01(static_cast<float>(
            parameters[MotionSceneCutParam]->u.fs_d.value / 100.0));
        const int cleanup = static_cast<int>(std::clamp(
            parameters[MotionNoiseRadiusParam]->u.fs_d.value, 0.0, 4.0));
        motion.noiseRadius = cleanup == 0
            ? 0 : std::max(1, cleanup / divisor);
        motion.compensateTranslation =
            parameters[MotionCompensateParam]->u.bd.value != FALSE;
        motion.maximumTranslation = static_cast<int>(std::clamp(
            parameters[MotionMaximumTranslationParam]->u.fs_d.value /
                static_cast<double>(divisor),
            0.0, 32.0));
        MotionResult motionResult = detectMotion(
            sampledSource, *motionReference, motion);
        switch (sourceMode) {
        case DetectionSource::Motion:
            mask = std::move(motionResult.confidence);
            break;
        case DetectionSource::ColorOrMotion:
            mask = combineMasks(
                colorMask, motionResult.confidence,
                CombinedOperation::ColorOrMotion);
            break;
        case DetectionSource::ColorAndMotion:
            mask = combineMasks(
                colorMask, motionResult.confidence,
                CombinedOperation::ColorAndMotion);
            break;
        case DetectionSource::Color:
            break;
        }
    }

    RegionFilter filter{};
    filter.minimumArea = static_cast<std::size_t>(std::max(
        1.0, parameters[MinimumArea]->u.fs_d.value /
                 static_cast<double>(divisor * divisor)));
    filter.maximumRegions = 256U;
    // No current overlay consumes per-region average RGB/luminance. Avoid a
    // second source-image read for every active pixel.
    auto regions = connectedComponents(mask, nullptr, filter);
    scaleRegions(regions, divisor);
    return {std::move(mask), std::move(regions), divisor};
}

Analysis analyzeSampled(
    const Image& sampledSource,
    const Image* motionReference,
    PF_ParamDef* parameters[], int divisor,
    bool keepMask = true) {
    std::uint64_t contentHash = imageContentHash(sampledSource);
    if (motionReference != nullptr) {
        const std::uint64_t referenceHash = imageContentHash(*motionReference);
        hashValue(contentHash, referenceHash);
    } else {
        constexpr std::uint64_t noReference = 0x4E4F5F4D4F54494FULL;
        hashValue(contentHash, noReference);
    }
    const AnalysisCacheKey key{
        contentHash,
        analysisSettingsHash(parameters, divisor),
        sampledSource.width(), sampledSource.height(), divisor};
    AnalysisCache& cache = analysisCache();
    const AnalysisCache::Ticket ticket = cache.acquire(key);
    std::shared_ptr<const Analysis> cached;
    if (ticket.producer) {
        try {
            cached = std::make_shared<const Analysis>(
                analyzeSampledUncached(
                    sampledSource, motionReference, parameters, divisor));
            cache.complete(key, ticket.state, cached);
        } catch (...) {
            cache.fail(key, ticket.state, std::current_exception());
            throw;
        }
    } else {
        cached = AnalysisCache::wait(ticket.state);
    }
    Analysis result{};
    result.divisor = cached->divisor;
    result.regions = cached->regions;
    if (keepMask) result.mask = cached->mask;
    return result;
}

Analysis analyze(
    const Image& source,
    PF_ParamDef* parameters[], int divisor) {
    divisor = std::clamp(divisor, 1, 4);
    Image reducedSource;
    const Image* sampledSource = &source;
    if (divisor > 1) {
        reducedSource = resizeImageNearest(
            source, std::max(1, (source.width() + divisor - 1) / divisor),
            std::max(1, (source.height() + divisor - 1) / divisor));
        sampledSource = &reducedSource;
    }
    return analyzeSampled(*sampledSource, nullptr, parameters, divisor);
}

bool boxesVisible(PF_ParamDef* parameters[]) noexcept {
    return parameters[BoxesEnabled]->u.bd.value != FALSE &&
           parameters[BoxOpacityParam]->u.fs_d.value > 0.0;
}

bool markersVisible(PF_ParamDef* parameters[]) noexcept {
    return parameters[MarkersEnabled]->u.bd.value != FALSE &&
           parameters[MarkerOpacityParam]->u.fs_d.value > 0.0;
}

bool connectionsVisible(PF_ParamDef* parameters[]) noexcept {
    return connectionsEnabledParameter(parameters[ConnectionModeParam]) &&
           (parameters[NetworkOpacityParam]->u.fs_d.value > 0.0 ||
            (parameters[NetworkArrowsEnabledParam]->u.bd.value != FALSE &&
             parameters[NetworkArrowOpacityParam]->u.fs_d.value > 0.0));
}

bool labelsVisible(PF_ParamDef* parameters[]) noexcept {
    return parameters[LabelModeParam]->u.pd.value != 1 &&
           (parameters[LabelTextOpacityParam]->u.fs_d.value > 0.0 ||
            parameters[LabelBackgroundParam]->u.fs_d.value > 0.0 ||
            parameters[LabelPanelStrokeOpacityParam]->u.fs_d.value > 0.0 ||
            parameters[LabelPanelShadowOpacityParam]->u.fs_d.value > 0.0 ||
            (parameters[LabelLeadersParam]->u.bd.value != FALSE &&
             parameters[LabelLeaderOpacityParam]->u.fs_d.value > 0.0));
}

bool fillVisible(PF_ParamDef* parameters[]) noexcept {
    const FillMode mode = fillModeParameter(parameters[FillModeParam]);
    if (mode == FillMode::Off) return false;
    if (mode == FillMode::Invert) {
        return parameters[FillInvertStrengthParam]->u.fs_d.value > 0.0;
    }
    return parameters[FillOpacityParam]->u.fs_d.value > 0.0;
}

bool projectionsVisible(PF_ParamDef* parameters[]) noexcept {
    return parameters[GridEnabledParam]->u.bd.value != FALSE &&
           parameters[GridProjectionsParam]->u.bd.value != FALSE &&
           parameters[ProjectionOpacityParam]->u.fs_d.value > 0.0;
}

bool gridVisible(PF_ParamDef* parameters[]) noexcept {
    return parameters[GridEnabledParam]->u.bd.value != FALSE &&
           (parameters[GridOpacityParam]->u.fs_d.value > 0.0 ||
            projectionsVisible(parameters));
}

bool pathVisible(PF_ParamDef* parameters[]) noexcept {
    return technicalPathModeParameter(parameters[TechnicalPathModeParam]) !=
               TechnicalPathMode::Off &&
           parameters[PathOpacityParam]->u.fs_d.value > 0.0;
}

bool requiresRegionAnalysis(PF_ParamDef* parameters[]) noexcept {
    if (outputModeParameter(parameters[OutputModeParam]) ==
        OutputMode::DetectionMask) {
        return true;
    }
    if (parameters[OverlayOpacity]->u.fs_d.value <= 0.0) return false;
    return boxesVisible(parameters) || markersVisible(parameters) ||
           connectionsVisible(parameters) || labelsVisible(parameters) ||
           fillVisible(parameters) || projectionsVisible(parameters) ||
           pathVisible(parameters);
}

bool hasVisibleOverlay(PF_ParamDef* parameters[]) noexcept {
    if (parameters[OverlayOpacity]->u.fs_d.value <= 0.0) return false;
    return boxesVisible(parameters) || markersVisible(parameters) ||
           connectionsVisible(parameters) || labelsVisible(parameters) ||
           fillVisible(parameters) || gridVisible(parameters) ||
           pathVisible(parameters);
}

Image processFrame(
    PF_ParamDef* parameters[],
    const std::vector<std::pair<std::int64_t, const PF_LayerDef*>>& history,
    std::int64_t currentTime,
    const LabelEditorValues& labelSettings) {
    Image source = decodeInput(&parameters[Input]->u.ld);
    const bool needsAnalysis = requiresRegionAnalysis(parameters);
    const int analysisDivisor =
        analysisDivisorParameter(parameters[AnalysisResolutionParam]);
    const float capturePersistence = clamp01(static_cast<float>(
        parameters[CapturePersistenceParam]->u.fs_d.value / 100.0));
    Image sampledCurrent;
    Analysis current;
    const OutputMode outputMode =
        outputModeParameter(parameters[OutputModeParam]);
    const TechnicalPathMode technicalPathMode =
        parameters[OverlayOpacity]->u.fs_d.value > 0.0 &&
                pathVisible(parameters)
            ? technicalPathModeParameter(parameters[TechnicalPathModeParam])
            : TechnicalPathMode::Off;
    const bool keepCurrentMask =
        outputMode == OutputMode::DetectionMask ||
        technicalPathMode == TechnicalPathMode::Contour ||
        capturePersistence > 0.0F;
    const bool usesMotion = detectionSourceParameter(
        parameters[DetectionSourceParam]) != DetectionSource::Color;
    const auto referenceLayerFor = [&](std::int64_t timeKey) {
        const PF_LayerDef* bestLayer = nullptr;
        std::int64_t bestTime = std::numeric_limits<std::int64_t>::min();
        if (currentTime < timeKey) {
            bestTime = currentTime;
            bestLayer = &parameters[Input]->u.ld;
        }
        for (const auto& candidate : history) {
            if (candidate.second != nullptr && candidate.second->data != nullptr &&
                candidate.first < timeKey && candidate.first > bestTime) {
                bestTime = candidate.first;
                bestLayer = candidate.second;
            }
        }
        return bestLayer;
    };
    Image currentMotionReference;
    const Image* currentMotionReferenceView = nullptr;
    if (needsAnalysis && usesMotion) {
        const PF_LayerDef* reference = referenceLayerFor(currentTime);
        if (reference != nullptr) {
            currentMotionReference = decodeInputSampled(reference, analysisDivisor);
            currentMotionReferenceView = &currentMotionReference;
        }
    }
    if (!needsAnalysis) {
        current.divisor = analysisDivisor;
    } else if (analysisDivisor > 1) {
        sampledCurrent = decodeInputSampled(
            &parameters[Input]->u.ld, analysisDivisor);
        current = analyzeSampled(
            sampledCurrent, currentMotionReferenceView,
            parameters, analysisDivisor, keepCurrentMask);
    } else {
        current = analyzeSampled(
            source, currentMotionReferenceView,
            parameters, 1, keepCurrentMask);
    }
    if (capturePersistence <= 0.0F) sampledCurrent = {};
    const StabilizationMode stabilizationMode =
        stabilizationModeParameter(parameters[StabilizationModeParam]);
    std::vector<Analysis> historicalAnalyses;
    historicalAnalyses.reserve(history.size());
    for (const auto& sample : history) {
        if (sample.second == nullptr || sample.second->data == nullptr) continue;
        Image historicalSource = decodeInputSampled(
            sample.second, analysisDivisor);
        Image historicalReference;
        const Image* historicalReferenceView = nullptr;
        if (usesMotion) {
            const PF_LayerDef* reference = referenceLayerFor(sample.first);
            if (reference != nullptr) {
                historicalReference = decodeInputSampled(
                    reference, analysisDivisor);
                historicalReferenceView = &historicalReference;
            }
        }
        Analysis historical = analyzeSampled(
            historicalSource, historicalReferenceView,
            parameters, analysisDivisor, capturePersistence > 0.0F);
        historicalAnalyses.push_back(std::move(historical));
    }
    if (capturePersistence > 0.0F && !historicalAnalyses.empty()) {
        std::vector<const Mask*> masks;
        masks.reserve(historicalAnalyses.size() + 1U);
        for (const Analysis& sample : historicalAnalyses) {
            masks.push_back(&sample.mask);
        }
        masks.push_back(&current.mask);
        current.mask = temporalConfidenceWindow(masks, capturePersistence);
        RegionFilter filter{};
        filter.minimumArea = static_cast<std::size_t>(std::max(
            1.0, parameters[MinimumArea]->u.fs_d.value /
                     static_cast<double>(analysisDivisor * analysisDivisor)));
        filter.maximumRegions = 256U;
        current.regions = connectedComponents(
            current.mask, nullptr, filter);
        scaleRegions(current.regions, analysisDivisor);
    }
    sampledCurrent = {};
    for (Analysis& sample : historicalAnalyses) sample.mask = {};
    if (stabilizationMode != StabilizationMode::LegacyRaw &&
        !history.empty()) {
        std::vector<FrameRegions> frameWindow;
        frameWindow.reserve(history.size() + 1U);
        std::size_t analysisIndex = 0U;
        for (const auto& sample : history) {
            if (sample.second == nullptr || sample.second->data == nullptr) continue;
            frameWindow.push_back(
                {sample.first,
                 std::move(historicalAnalyses[analysisIndex++].regions)});
        }
        frameWindow.push_back({currentTime, std::move(current.regions)});
        TrackingSettings tracking{};
        auto tracked = trackFrameWindow(std::move(frameWindow), tracking);
        StabilizationSettings stabilization{};
        stabilization.mode = stabilizationMode;
        stabilization.positionStrength = clamp01(static_cast<float>(
            parameters[PositionSmoothingParam]->u.fs_d.value / 100.0));
        stabilization.sizeStrength = clamp01(static_cast<float>(
            parameters[SizeSmoothingParam]->u.fs_d.value / 100.0));
        tracked = stabilizeFrameWindow(tracked, stabilization);
        const auto currentFrame = std::find_if(
            tracked.begin(), tracked.end(),
            [&](const FrameRegions& frame) {
                return frame.timeKey == currentTime;
            });
        if (currentFrame != tracked.end()) {
            current.regions = currentFrame->regions;
        }
    }

    OverlayStyle style{};
    style.boxesEnabled = boxesVisible(parameters);
    style.markersEnabled = markersVisible(parameters);
    style.boxShape = boxShapeParameter(parameters[BoxShapeParam]);
    style.markerShape = markerShapeParameter(parameters[MarkerShapeParam]);
    style.boxColor = colorParameter(parameters[BoxStrokeColorParam]);
    style.globalOpacity = clamp01(
        static_cast<float>(parameters[OverlayOpacity]->u.fs_d.value / 100.0));
    style.boxOpacity = clamp01(static_cast<float>(
        parameters[BoxOpacityParam]->u.fs_d.value / 100.0));
    style.strokeWidth = static_cast<float>(std::clamp(
        parameters[BoxStrokeWidthParam]->u.fs_d.value, 1.0, 20.0));
    style.padding = static_cast<float>(std::clamp(
        parameters[BoxPaddingParam]->u.fs_d.value, 0.0, 200.0));
    style.cornerLengthFraction = clamp01(static_cast<float>(
        parameters[BoxCornerLengthParam]->u.fs_d.value / 100.0));
    style.markerColor = colorParameter(parameters[MarkerColorParam]);
    style.markerOpacity = clamp01(static_cast<float>(
        parameters[MarkerOpacityParam]->u.fs_d.value / 100.0));
    style.markerStrokeWidth = static_cast<float>(std::clamp(
        parameters[MarkerStrokeWidthParam]->u.fs_d.value, 1.0, 20.0));
    style.markerSize = static_cast<float>(std::clamp(
        parameters[MarkerSizeParam]->u.fs_d.value, 2.0, 200.0));
    style.connectionsEnabled = connectionsVisible(parameters);
    style.topology =
        connectionTopologyParameter(parameters[ConnectionModeParam]);
    style.maximumConnections = static_cast<std::size_t>(std::clamp(
        parameters[MaximumConnectionsParam]->u.fs_d.value, 1.0, 256.0));
    style.connectionRadius = static_cast<float>(std::clamp(
        parameters[ConnectionRadiusParam]->u.fs_d.value, 1.0, 2000.0));
    style.connectionStyle =
        connectionStyleParameter(parameters[ConnectionStyleParam]);
    style.connectionCurvature = clamp01(static_cast<float>(
        parameters[ConnectionCurvatureParam]->u.fs_d.value / 100.0));
    style.connectionColor = colorParameter(parameters[NetworkColorParam]);
    style.connectionOpacity = clamp01(static_cast<float>(
        parameters[NetworkOpacityParam]->u.fs_d.value / 100.0));
    style.connectionWidth = static_cast<float>(std::clamp(
        parameters[NetworkThicknessParam]->u.fs_d.value, 1.0, 20.0));
    style.connectionDashEnabled =
        parameters[NetworkDashEnabledParam]->u.bd.value != FALSE;
    style.connectionDashLength = static_cast<float>(std::clamp(
        parameters[NetworkDashLengthParam]->u.fs_d.value, 1.0, 200.0));
    style.connectionGapLength = static_cast<float>(std::clamp(
        parameters[NetworkGapLengthParam]->u.fs_d.value, 1.0, 200.0));
    style.connectionArrowsEnabled =
        parameters[NetworkArrowsEnabledParam]->u.bd.value != FALSE;
    style.connectionArrowColor =
        colorParameter(parameters[NetworkArrowColorParam]);
    style.connectionArrowOpacity = clamp01(static_cast<float>(
        parameters[NetworkArrowOpacityParam]->u.fs_d.value / 100.0));
    style.connectionArrowSize = static_cast<float>(std::clamp(
        parameters[NetworkArrowSizeParam]->u.fs_d.value, 2.0, 100.0));
    style.gridEnabled = gridVisible(parameters);
    style.gridMode = gridModeParameter(parameters[GridModeParam]);
    style.gridDivisionsX = static_cast<int>(std::clamp(
        parameters[GridColumnsParam]->u.fs_d.value, 2.0, 64.0));
    style.gridDivisionsY = static_cast<int>(std::clamp(
        parameters[GridRowsParam]->u.fs_d.value, 2.0, 64.0));
    style.gridColor = colorParameter(parameters[GridColorParam]);
    style.gridOpacity = clamp01(static_cast<float>(
        parameters[GridOpacityParam]->u.fs_d.value / 100.0));
    style.gridWidth = static_cast<float>(std::clamp(
        parameters[GridThicknessParam]->u.fs_d.value, 1.0, 10.0));
    style.gridAxisLabels =
        parameters[GridAxisLabelsParam]->u.bd.value != FALSE;
    style.gridTickSize = static_cast<float>(std::clamp(
        parameters[GridTickSizeParam]->u.fs_d.value, 0.0, 32.0));
    style.gridProjections =
        parameters[GridProjectionsParam]->u.bd.value != FALSE;
    style.projectionColor = colorParameter(parameters[ProjectionColorParam]);
    style.projectionOpacity = clamp01(static_cast<float>(
        parameters[ProjectionOpacityParam]->u.fs_d.value / 100.0));
    style.projectionDashLength = static_cast<float>(std::clamp(
        parameters[ProjectionDashLengthParam]->u.fs_d.value, 1.0, 200.0));
    style.projectionGapLength = static_cast<float>(std::clamp(
        parameters[ProjectionGapLengthParam]->u.fs_d.value, 1.0, 200.0));
    style.labelMode = labelsVisible(parameters)
        ? labelModeParameter(parameters[LabelModeParam]) : LabelMode::Off;
    style.labelStyle.scale = static_cast<int>(std::clamp(
        parameters[LabelScaleParam]->u.fs_d.value, 1.0, 5.0));
    style.labelStyle.foreground =
        colorParameter(parameters[LabelTextColorParam]);
    const float labelTextOpacity = clamp01(static_cast<float>(
        parameters[LabelTextOpacityParam]->u.fs_d.value / 100.0));
    style.labelStyle.foreground.a =
        style.globalOpacity * labelTextOpacity;
    style.labelStyle.background =
        colorParameter(parameters[LabelPanelColorParam]);
    style.labelStyle.background.a = clamp01(static_cast<float>(
        parameters[LabelBackgroundParam]->u.fs_d.value / 100.0)) *
        style.globalOpacity;
    style.labelStyle.cornerRadius = static_cast<float>(std::clamp(
        parameters[LabelPanelCornerRadiusParam]->u.fs_d.value, 0.0, 64.0));
    style.labelStyle.border =
        colorParameter(parameters[LabelPanelStrokeColorParam]);
    style.labelStyle.border.a = clamp01(static_cast<float>(
        parameters[LabelPanelStrokeOpacityParam]->u.fs_d.value / 100.0)) *
        style.globalOpacity;
    style.labelStyle.borderThickness = static_cast<int>(std::clamp(
        parameters[LabelPanelStrokeThicknessParam]->u.fs_d.value, 1.0, 12.0));
    style.labelStyle.shadow =
        colorParameter(parameters[LabelPanelShadowColorParam]);
    style.labelStyle.shadow.a = clamp01(static_cast<float>(
        parameters[LabelPanelShadowOpacityParam]->u.fs_d.value / 100.0)) *
        style.globalOpacity;
    style.labelStyle.shadowOffset = {
        static_cast<float>(std::clamp(
            parameters[LabelPanelShadowOffsetXParam]->u.fs_d.value,
            -64.0, 64.0)),
        static_cast<float>(std::clamp(
            parameters[LabelPanelShadowOffsetYParam]->u.fs_d.value,
            -64.0, 64.0))};
    style.customLabelText = labelSettings.text.empty()
        ? "CUSTOM" : labelSettings.text;
    style.labelStyle.fontFamily = labelSettings.fontFamily;
    style.maximumLabels = static_cast<std::size_t>(std::clamp(
        parameters[LabelLimitParam]->u.fs_d.value, 1.0, 128.0));
    style.labelLeaders = parameters[LabelLeadersParam]->u.bd.value != FALSE;
    style.labelLeaderColor =
        colorParameter(parameters[LabelLeaderColorParam]);
    style.labelLeaderOpacity = clamp01(static_cast<float>(
        parameters[LabelLeaderOpacityParam]->u.fs_d.value / 100.0));
    style.labelStyle.padding = static_cast<int>(std::clamp(
        parameters[LabelPanelPaddingParam]->u.fs_d.value, 0.0, 32.0));
    style.labelOffsetX = static_cast<float>(std::clamp(
        parameters[LabelOffsetXParam]->u.fs_d.value, -1000.0, 1000.0));
    style.labelOffsetY = static_cast<float>(std::clamp(
        parameters[LabelOffsetYParam]->u.fs_d.value, -1000.0, 1000.0));
    style.fillMode = fillVisible(parameters)
        ? fillModeParameter(parameters[FillModeParam]) : FillMode::Off;
    style.fillColor = colorParameter(parameters[FillColorParam]);
    style.fillSecondaryColor =
        colorParameter(parameters[FillSecondaryColorParam]);
    style.fillOpacity = clamp01(static_cast<float>(
        parameters[FillOpacityParam]->u.fs_d.value / 100.0));
    style.fillInset = static_cast<float>(std::clamp(
        parameters[FillInsetParam]->u.fs_d.value, 0.0, 200.0));
    style.fillFeather = static_cast<float>(std::clamp(
        parameters[FillFeatherParam]->u.fs_d.value, 0.0, 100.0));
    style.fillInvertStrength = clamp01(static_cast<float>(
        parameters[FillInvertStrengthParam]->u.fs_d.value / 100.0));
    style.hatchSpacing = static_cast<int>(std::clamp(
        parameters[HatchSpacingParam]->u.fs_d.value, 3.0, 64.0));
    style.hatchWidth = static_cast<int>(std::clamp(
        parameters[HatchWidthParam]->u.fs_d.value, 1.0, 32.0));
    style.hatchAngleDegrees = static_cast<float>(
        parameters[HatchAngleParam]->u.ad.value / 65536.0);

    Mask fullResolutionMask;
    const Mask* renderMask = &current.mask;
    const bool needsFullResolutionMask =
        outputMode == OutputMode::DetectionMask;
    if (needsFullResolutionMask && analysisDivisor > 1) {
        fullResolutionMask = resizeNearest(
            current.mask, source.width(), source.height());
        renderMask = &fullResolutionMask;
    }
    Image output =
        renderOverlay(std::move(source), *renderMask, current.regions, style, outputMode);
    if (outputMode == OutputMode::DetectionMask) {
        return output;
    }

    std::vector<Polyline> paths;
    switch (technicalPathMode) {
    case TechnicalPathMode::Orbit: {
        paths.reserve(current.regions.size());
        OrbitSettings orbit{};
        orbit.padding = static_cast<float>(std::clamp(
            parameters[OrbitPaddingParam]->u.fs_d.value, 0.0, 200.0));
        orbit.segments = 64U;
        orbit.maximumSegments = 256U;
        for (const Region& region : current.regions) {
            const std::uint64_t stableId =
                region.trackId != 0U
                    ? region.trackId
                    : static_cast<std::uint64_t>(region.componentId);
            paths.push_back(makeOrbitalPath(region.bounds, stableId, orbit));
        }
        break;
    }
    case TechnicalPathMode::Contour: {
        ContourSettings contour{};
        contour.minimumArea = static_cast<std::size_t>(std::max(
            1.0, parameters[MinimumArea]->u.fs_d.value /
                     static_cast<double>(analysisDivisor * analysisDivisor)));
        contour.maximumContours = 256U;
        contour.maximumPointsPerContour = 2048U;
        contour.simplificationTolerance =
            0.5F / static_cast<float>(analysisDivisor);
        paths = extractContourPaths(*renderMask, contour);
        if (analysisDivisor > 1) {
            const float scale = static_cast<float>(analysisDivisor);
            for (Polyline& path : paths) {
                for (Point& point : path.points) {
                    point.x *= scale;
                    point.y *= scale;
                }
            }
        }
        break;
    }
    case TechnicalPathMode::Off:
        break;
    }

    if (!paths.empty()) {
        Color pathColor = colorParameter(parameters[PathColorParam]);
        const float pathOpacity = clamp01(static_cast<float>(
            parameters[PathOpacityParam]->u.fs_d.value / 100.0));
        const float pathWidth = static_cast<float>(std::clamp(
            parameters[PathThicknessParam]->u.fs_d.value, 1.0, 20.0));
        pathColor.a *= style.globalOpacity * pathOpacity;
        rasterizePolylines(output, paths, pathColor, pathWidth);

        RepeaterPrimitive primitive{};
        if (repeaterPrimitiveParameter(
                parameters[RepeaterPrimitiveParam], primitive)) {
            std::size_t remainingInstances = 512U;
            std::vector<Polyline> glyphs;
            PathRepeaterSettings repeater{};
            repeater.primitive = primitive;
            repeater.spacing = static_cast<float>(std::clamp(
                parameters[RepeaterSpacingParam]->u.fs_d.value, 2.0, 200.0));
            repeater.scale = std::max(1.0F, pathWidth * 1.5F);
            for (const Polyline& path : paths) {
                if (remainingInstances == 0U) {
                    break;
                }
                repeater.maximumInstances = remainingInstances;
                const auto instances = repeatAlongPath(path, repeater);
                remainingInstances -= instances.size();
                for (const RepeaterInstance& instance : instances) {
                    auto strokes = buildRepeaterGlyph(instance);
                    glyphs.insert(
                        glyphs.end(),
                        std::make_move_iterator(strokes.begin()),
                        std::make_move_iterator(strokes.end()));
                }
            }
            rasterizePolylines(
                output, glyphs, pathColor,
                std::max(1.0F, pathWidth * 0.75F));
        }
    }
    return output;
}

void copyInputToOutput(PF_ParamDef* parameters[], PF_LayerDef* output) noexcept {
    try {
        encodeOutput(decodeInput(&parameters[Input]->u.ld), output);
    } catch (...) {
        // Nothing else is safe to do if even the checked source world is invalid.
    }
}

template <typename Pixel>
void copyWorldDirect(const PF_LayerDef& input, PF_LayerDef& output) {
    if (input.data == nullptr || output.data == nullptr ||
        input.width < 0 || input.height < 0 ||
        input.width != output.width || input.height != output.height) {
        throw std::invalid_argument("Incompatible direct-copy worlds");
    }
    const std::size_t activeBytes =
        static_cast<std::size_t>(input.width) * sizeof(Pixel);
    const auto* source = reinterpret_cast<const std::byte*>(input.data);
    auto* destination = reinterpret_cast<std::byte*>(output.data);
    for (A_long y = 0; y < input.height; ++y) {
        std::memcpy(
            destination + static_cast<std::ptrdiff_t>(y) * output.rowbytes,
            source + static_cast<std::ptrdiff_t>(y) * input.rowbytes,
            activeBytes);
    }
}

void copyInputDirect(PF_ParamDef* parameters[], PF_LayerDef* output) {
    const PF_LayerDef& input = parameters[Input]->u.ld;
    if (PF_WORLD_IS_DEEP(&input) != PF_WORLD_IS_DEEP(output)) {
        throw std::invalid_argument("Input/output depth mismatch");
    }
    if (PF_WORLD_IS_DEEP(&input)) {
        copyWorldDirect<PF_Pixel16>(input, *output);
    } else {
        copyWorldDirect<PF_Pixel8>(input, *output);
    }
}

template <typename Pixel>
void clearWorldDirect(PF_LayerDef& output) {
    if (output.data == nullptr || output.width < 0 || output.height < 0) {
        throw std::invalid_argument("Invalid output world");
    }
    const std::size_t activeBytes =
        static_cast<std::size_t>(output.width) * sizeof(Pixel);
    auto* destination = reinterpret_cast<std::byte*>(output.data);
    for (A_long y = 0; y < output.height; ++y) {
        std::memset(
            destination + static_cast<std::ptrdiff_t>(y) * output.rowbytes,
            0, activeBytes);
    }
}

void clearOutputDirect(PF_LayerDef* output) {
    if (PF_WORLD_IS_DEEP(output)) {
        clearWorldDirect<PF_Pixel16>(*output);
    } else {
        clearWorldDirect<PF_Pixel8>(*output);
    }
}

PF_Err render(
    PF_InData* inputData,
    PF_ParamDef* parameters[],
    PF_LayerDef* output) {
    const OutputMode requestedOutput =
        outputModeParameter(parameters[OutputModeParam]);
    if (!hasVisibleOverlay(parameters) &&
        requestedOutput != OutputMode::DetectionMask) {
        try {
            if (requestedOutput == OutputMode::SourceAndOverlay) {
                copyInputDirect(parameters, output);
            } else {
                clearOutputDirect(output);
            }
            return PF_Err_NONE;
        } catch (...) {
            // Fall through to the format-converting path for unusual host worlds.
        }
    }
    std::array<PF_ParamDef, 4U> previousInputs{};
    std::array<bool, 4U> checkedOut{};
    const bool needsAnalysis = requiresRegionAnalysis(parameters);
    const StabilizationMode mode =
        stabilizationModeParameter(parameters[StabilizationModeParam]);
    std::vector<int> temporalOffsets;
    if (needsAnalysis) switch (mode) {
    case StabilizationMode::BasicSmooth: temporalOffsets = {1}; break;
    case StabilizationMode::AdaptiveSmooth: temporalOffsets = {2, 1}; break;
    case StabilizationMode::CinematicSmooth:
        temporalOffsets = {2, 1, -1, -2};
        break;
    case StabilizationMode::PredictiveSmooth:
        temporalOffsets = {3, 2, 1};
        break;
    case StabilizationMode::LegacyRaw: break;
    }
    const bool wantsCaptureHistory = needsAnalysis &&
        parameters[CapturePersistenceParam]->u.fs_d.value > 0.0;
    if (wantsCaptureHistory && temporalOffsets.size() < 2U) {
        temporalOffsets = {2, 1};
    }
    const bool wantsMotionHistory = needsAnalysis &&
        detectionSourceParameter(parameters[DetectionSourceParam]) !=
            DetectionSource::Color;
    if (wantsMotionHistory &&
        std::find(temporalOffsets.begin(), temporalOffsets.end(), 1) ==
            temporalOffsets.end()) {
        temporalOffsets.push_back(1);
    }
    if (inputData->time_step != 0) {
        for (std::size_t i = 0; i < temporalOffsets.size(); ++i) {
            AEFX_CLR_STRUCT(previousInputs[i]);
            const int offset = temporalOffsets[i];
            const PF_Err checkoutError = PF_CHECKOUT_PARAM(
                inputData, Input,
                inputData->current_time - inputData->time_step * offset,
                inputData->time_step, inputData->time_scale,
                &previousInputs[i]);
            checkedOut[i] = checkoutError == PF_Err_NONE;
        }
    }

    PF_Err renderError = PF_Err_NONE;
    try {
        std::vector<std::pair<std::int64_t, const PF_LayerDef*>> history;
        for (std::size_t i = 0; i < temporalOffsets.size(); ++i) {
            if (!checkedOut[i]) continue;
            const int offset = temporalOffsets[i];
            history.push_back({
                static_cast<std::int64_t>(
                    inputData->current_time - inputData->time_step * offset),
                &previousInputs[i].u.ld});
        }
        const Image result = processFrame(
            parameters, history,
            static_cast<std::int64_t>(inputData->current_time),
            readLabelSettings(inputData));
        encodeOutput(result, output);
    } catch (const std::bad_alloc&) {
        copyInputToOutput(parameters, output);
        renderError = PF_Err_OUT_OF_MEMORY;
    } catch (...) {
        copyInputToOutput(parameters, output);
        renderError = PF_Err_NONE;
    }
    for (std::size_t i = 0; i < checkedOut.size(); ++i) {
        if (checkedOut[i]) {
            const PF_Err checkinError =
                PF_CHECKIN_PARAM(inputData, &previousInputs[i]);
            if (renderError == PF_Err_NONE && checkinError != PF_Err_NONE) {
                renderError = checkinError;
            }
        }
    }
    return renderError;
}

} // namespace
} // namespace linear::plugin

extern "C" DllExport PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr pluginData,
    PF_PluginDataCB2 callback,
    SPBasicSuite*,
    const char*,
    const char*) {
    PF_Err result = PF_Err_INVALID_CALLBACK;
    result = PF_REGISTER_EFFECT_EXT2(
        pluginData,
        callback,
        LINEAR_PRODUCT_NAME,
        LINEAR_MATCH_NAME,
        LINEAR_EFFECT_CATEGORY,
        AE_RESERVED_INFO,
        "EffectMain",
        "");
    return result;
}

extern "C" DllExport PF_Err EffectMain(
    PF_Cmd command,
    PF_InData* inputData,
    PF_OutData* outputData,
    PF_ParamDef* parameters[],
    PF_LayerDef* output,
    void* extra) {
    PF_Err error = PF_Err_NONE;
    try {
        switch (command) {
        case PF_Cmd_ABOUT:
            error = linear::plugin::about(inputData, outputData);
            break;
        case PF_Cmd_GLOBAL_SETUP:
            error = linear::plugin::globalSetup(outputData);
            break;
        case PF_Cmd_PARAMS_SETUP:
            error = linear::plugin::parameterSetup(inputData, outputData);
            break;
        case PF_Cmd_SEQUENCE_SETUP:
            error = linear::plugin::sequenceSetup(inputData, outputData);
            break;
        case PF_Cmd_SEQUENCE_SETDOWN:
            error = linear::plugin::sequenceSetdown(inputData, outputData);
            break;
        case PF_Cmd_SEQUENCE_FLATTEN:
            error = linear::plugin::copySequenceData(
                inputData, outputData, true);
            break;
        case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA:
        case PF_Cmd_SEQUENCE_RESETUP:
            error = linear::plugin::copySequenceData(
                inputData, outputData, false);
            break;
        case PF_Cmd_USER_CHANGED_PARAM: {
            const auto* changed = static_cast<PF_UserChangedParamExtra*>(extra);
            if (changed != nullptr &&
                changed->param_index == linear::plugin::LabelEditorParam) {
                error = linear::plugin::editLabelSettings(inputData, outputData);
            }
            break;
        }
        case PF_Cmd_RENDER:
            error = linear::plugin::render(inputData, parameters, output);
            break;
        default:
            break;
        }
    } catch (const PF_Err& sdkError) {
        error = sdkError;
    } catch (const std::bad_alloc&) {
        error = PF_Err_OUT_OF_MEMORY;
    } catch (...) {
        error = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    return error;
}
