#pragma once

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"

#ifdef AE_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"

#include "linear/ProductConfig.h"
#include "linear/VersionConfig.h"

namespace linear::plugin {

enum Parameter {
    Input = 0,

    DetectionGroupStartParam,
    TargetColor,
    ColorSpaceParam,
    Threshold,
    MinimumArea,
    InvertDetectionParam,
    IgnoreTransparentParam,
    AlphaThresholdParam,
    PreBlurRadiusParam,
    PostBlurRadiusParam,
    MorphologyRadiusParam,
    DetectionGroupEndParam,

    TrackingGroupStartParam,
    StabilizationModeParam,
    PositionSmoothingParam,
    SizeSmoothingParam,
    CapturePersistenceParam,
    DetectionSourceParam,
    MotionThresholdParam,
    MotionSoftnessParam,
    MotionSceneCutParam,
    MotionNoiseRadiusParam,
    MotionCompensateParam,
    MotionMaximumTranslationParam,
    TrackingGroupEndParam,

    OutputGroupStartParam,
    OutputModeParam,
    StrokeColor,
    StrokeWidth,
    OverlayOpacity,
    StyleSourceParam,
    BoxesEnabled,
    BoxShapeParam,
    BoxOpacityParam,
    BoxStrokeWidthParam,
    BoxPaddingParam,
    BoxCornerLengthParam,
    MarkersEnabled,
    MarkerShapeParam,
    MarkerOpacityParam,
    MarkerStrokeWidthParam,
    MarkerSizeParam,
    OutputGroupEndParam,

    NetworkGroupStartParam,
    ConnectionModeParam,
    MaximumConnectionsParam,
    ConnectionRadiusParam,
    ConnectionStyleParam,
    ConnectionCurvatureParam,
    NetworkOpacityParam,
    NetworkThicknessParam,
    NetworkDashEnabledParam,
    NetworkDashLengthParam,
    NetworkGapLengthParam,
    NetworkArrowsEnabledParam,
    NetworkArrowOpacityParam,
    NetworkArrowSizeParam,
    NetworkGroupEndParam,

    LabelsGroupStartParam,
    LabelContentGroupStartParam,
    LabelModeParam,
    LabelEditorParam,
    LabelScaleParam,
    LabelLimitParam,
    LabelTextOpacityParam,
    LabelContentGroupEndParam,
    LabelPanelGroupStartParam,
    LabelBackgroundParam,
    LabelPanelPaddingParam,
    LabelPanelCornerRadiusParam,
    LabelPanelStrokeOpacityParam,
    LabelPanelStrokeThicknessParam,
    LabelPanelShadowOpacityParam,
    LabelPanelShadowOffsetXParam,
    LabelPanelShadowOffsetYParam,
    LabelPanelGroupEndParam,
    LabelLayoutGroupStartParam,
    LabelOffsetXParam,
    LabelOffsetYParam,
    LabelLeadersParam,
    LabelLeaderOpacityParam,
    LabelLayoutGroupEndParam,
    LabelsGroupEndParam,

    FillGroupStartParam,
    FillModeParam,
    FillOpacityParam,
    FillInsetParam,
    FillFeatherParam,
    FillInvertStrengthParam,
    HatchSpacingParam,
    HatchWidthParam,
    HatchAngleParam,
    FillGroupEndParam,

    HudGroupStartParam,
    GridEnabledParam,
    GridModeParam,
    GridColumnsParam,
    GridRowsParam,
    GridOpacityParam,
    GridThicknessParam,
    GridAxisLabelsParam,
    GridTickSizeParam,
    GridProjectionsParam,
    ProjectionOpacityParam,
    ProjectionDashLengthParam,
    ProjectionGapLengthParam,
    HudGroupEndParam,

    PathsGroupStartParam,
    TechnicalPathModeParam,
    OrbitPaddingParam,
    RepeaterPrimitiveParam,
    RepeaterSpacingParam,
    PathOpacityParam,
    PathThicknessParam,
    PathsGroupEndParam,

    ColorsGroupStartParam,
    BoxStrokeColorParam,
    MarkerColorParam,
    NetworkColorParam,
    NetworkArrowColorParam,
    GridColorParam,
    ProjectionColorParam,
    LabelTextColorParam,
    LabelPanelColorParam,
    LabelLeaderColorParam,
    LabelPanelStrokeColorParam,
    LabelPanelShadowColorParam,
    FillColorParam,
    FillSecondaryColorParam,
    PathColorParam,
    ColorsGroupEndParam,

    PerformanceGroupStartParam,
    AnalysisResolutionParam,
    PerformanceGroupEndParam,

    ParameterCount
};

enum DiskId {
    TargetColorDiskId = 1,
    ThresholdDiskId,
    MinimumAreaDiskId,
    BoxesEnabledDiskId,
    MarkersEnabledDiskId,
    OutputModeDiskId,
    StrokeColorDiskId,
    StrokeWidthDiskId,
    OverlayOpacityDiskId,
    StabilizationModeDiskId = 10,
    ConnectionModeDiskId = 11,
    MaximumConnectionsDiskId = 12,
    ColorSpaceDiskId = 13,
    InvertDetectionDiskId = 14,
    IgnoreTransparentDiskId = 15,
    AlphaThresholdDiskId = 16,
    PreBlurRadiusDiskId = 17,
    PostBlurRadiusDiskId = 18,
    MorphologyRadiusDiskId = 19,
    BoxShapeDiskId = 20,
    MarkerShapeDiskId = 21,
    GridEnabledDiskId = 22,
    GridColumnsDiskId = 23,
    GridRowsDiskId = 24,
    TechnicalPathModeDiskId = 25,
    OrbitPaddingDiskId = 26,
    RepeaterPrimitiveDiskId = 27,
    RepeaterSpacingDiskId = 28,
    PositionSmoothingDiskId = 29,
    SizeSmoothingDiskId = 30,
    ConnectionRadiusDiskId = 31,
    CapturePersistenceDiskId = 32,
    AnalysisResolutionDiskId = 33,
    LabelModeDiskId = 34,
    LabelScaleDiskId = 35,
    LabelLimitDiskId = 36,
    LabelBackgroundDiskId = 37,
    LabelLeadersDiskId = 38,
    FillModeDiskId = 39,
    FillOpacityDiskId = 40,
    HatchSpacingDiskId = 41,
    DetectionGroupStartDiskId = 42,
    DetectionGroupEndDiskId = 43,
    TrackingGroupStartDiskId = 44,
    TrackingGroupEndDiskId = 45,
    OutputGroupStartDiskId = 46,
    OutputGroupEndDiskId = 47,
    NetworkGroupStartDiskId = 48,
    NetworkGroupEndDiskId = 49,
    LabelsGroupStartDiskId = 50,
    LabelsGroupEndDiskId = 51,
    FillGroupStartDiskId = 52,
    FillGroupEndDiskId = 53,
    HudGroupStartDiskId = 54,
    HudGroupEndDiskId = 55,
    PathsGroupStartDiskId = 56,
    PathsGroupEndDiskId = 57,
    PerformanceGroupStartDiskId = 58,
    PerformanceGroupEndDiskId = 59,
    ConnectionStyleDiskId = 60,
    ConnectionCurvatureDiskId = 61,
    FillColorDiskId = 62,
    FillSecondaryColorDiskId = 63,
    FillInsetDiskId = 64,
    FillFeatherDiskId = 65,
    HatchWidthDiskId = 66,
    HatchAngleDiskId = 67,
    StyleSourceDiskId = 68,
    BoxStrokeColorDiskId = 69,
    BoxOpacityDiskId = 70,
    BoxStrokeWidthDiskId = 71,
    BoxPaddingDiskId = 72,
    BoxCornerLengthDiskId = 73,
    MarkerColorDiskId = 74,
    MarkerOpacityDiskId = 75,
    MarkerStrokeWidthDiskId = 76,
    MarkerSizeDiskId = 77,
    NetworkColorDiskId = 78,
    NetworkOpacityDiskId = 79,
    NetworkThicknessDiskId = 80,
    GridColorDiskId = 81,
    GridOpacityDiskId = 82,
    GridThicknessDiskId = 83,
    LabelTextColorDiskId = 84,
    LabelTextOpacityDiskId = 85,
    LabelPanelColorDiskId = 86,
    LabelLeaderColorDiskId = 87,
    LabelLeaderOpacityDiskId = 88,
    PathColorDiskId = 89,
    PathOpacityDiskId = 90,
    PathThicknessDiskId = 91,
    NetworkDashEnabledDiskId = 92,
    NetworkDashLengthDiskId = 93,
    NetworkGapLengthDiskId = 94,
    NetworkArrowsEnabledDiskId = 95,
    NetworkArrowColorDiskId = 96,
    NetworkArrowOpacityDiskId = 97,
    NetworkArrowSizeDiskId = 98,
    LabelPanelPaddingDiskId = 99,
    LabelOffsetXDiskId = 100,
    LabelOffsetYDiskId = 101,
    ColorsGroupStartDiskId = 102,
    ProjectionColorDiskId = 103,
    ColorsGroupEndDiskId = 104,
    GridModeDiskId = 105,
    GridAxisLabelsDiskId = 106,
    GridTickSizeDiskId = 107,
    GridProjectionsDiskId = 108,
    ProjectionOpacityDiskId = 109,
    ProjectionDashLengthDiskId = 110,
    ProjectionGapLengthDiskId = 111,
    FillInvertStrengthDiskId = 112,
    DetectionSourceDiskId = 113,
    MotionThresholdDiskId = 114,
    MotionSoftnessDiskId = 115,
    MotionSceneCutDiskId = 116,
    MotionNoiseRadiusDiskId = 117,
    MotionCompensateDiskId = 118,
    MotionMaximumTranslationDiskId = 119,
    LabelEditorDiskId = 120,
    LabelPanelCornerRadiusDiskId = 121,
    LabelPanelStrokeOpacityDiskId = 122,
    LabelPanelStrokeThicknessDiskId = 123,
    LabelPanelShadowOpacityDiskId = 124,
    LabelPanelShadowOffsetXDiskId = 125,
    LabelPanelShadowOffsetYDiskId = 126,
    LabelPanelStrokeColorDiskId = 127,
    LabelPanelShadowColorDiskId = 128,
    LabelContentGroupStartDiskId = 129,
    LabelContentGroupEndDiskId = 130,
    LabelPanelGroupStartDiskId = 131,
    LabelPanelGroupEndDiskId = 132,
    LabelLayoutGroupStartDiskId = 133,
    LabelLayoutGroupEndDiskId = 134
};

static_assert(TargetColorDiskId == 1 && HatchSpacingDiskId == 41,
              "Serialized creative parameter IDs must remain stable");
static_assert(DetectionGroupStartDiskId == 42 &&
              PerformanceGroupEndDiskId == 59,
              "UI topic IDs must remain append-only");
static_assert(ConnectionStyleDiskId == 60 && ConnectionCurvatureDiskId == 61,
              "Rounded connection controls must remain append-only");
static_assert(FillColorDiskId == 62 && HatchAngleDiskId == 67,
              "Region fill customization IDs must remain append-only");
static_assert(StyleSourceDiskId == 68 && PathThicknessDiskId == 91,
              "Per-section style IDs must remain append-only");
static_assert(NetworkDashEnabledDiskId == 92 && LabelOffsetYDiskId == 101,
              "Dash, arrow, and label layout IDs must remain append-only");
static_assert(ColorsGroupStartDiskId == 102 &&
              FillInvertStrengthDiskId == 112,
              "Color and Cartesian grid IDs must remain append-only");
static_assert(DetectionSourceDiskId == 113 &&
              LabelPanelShadowColorDiskId == 128,
              "Motion, custom text, and label panel IDs must remain append-only");
static_assert(LabelContentGroupStartDiskId == 129 &&
              LabelLayoutGroupEndDiskId == 134,
              "Nested label category IDs must remain append-only");
static_assert(ParameterCount == 135,
              "Update the categorized parameter layout audit when adding controls");

} // namespace linear::plugin

extern "C" {

DllExport PF_Err EffectMain(
    PF_Cmd command,
    PF_InData* inputData,
    PF_OutData* outputData,
    PF_ParamDef* parameters[],
    PF_LayerDef* output,
    void* extra);

DllExport PF_Err PluginDataEntryFunction2(
    PF_PluginDataPtr pluginData,
    PF_PluginDataCB2 callback,
    SPBasicSuite* basicSuite,
    const char* hostName,
    const char* hostVersion);

}
