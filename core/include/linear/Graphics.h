#pragma once

#include "linear/PathGeometry.h"
#include "linear/Regions.h"
#include "linear/TextRaster.h"

#include <string>
#include <vector>

namespace linear {

enum class OutputMode { SourceAndOverlay, OverlayOnly, DetectionMask };
enum class BoxShape { Rectangle, CornerBrackets, Ellipse };
enum class MarkerShape {
    Dot, Ring, Plus, Cross, Triangle, Star, Diamond, Target, Chevron
};
enum class LabelMode {
    Off, NodeId, Area, Dimensions, TrackId, Confidence, CustomText
};
enum class FillMode { Off, Solid, Hatch, XRay, Duotone, Crosshatch, Invert };
enum class GridMode { Simple, Cartesian };
enum class ConnectionStyle {
    Straight,
    RoundedArc,
    SmoothBend,
    StepBend,
    PcbTrace
};
enum class ConnectionTopology {
    Sequential,
    NearestNeighbor,
    MinimumSpanningTree,
    Star,
    FullMesh,
    Radius
};

struct OverlayStyle {
    bool boxesEnabled{true};
    BoxShape boxShape{BoxShape::CornerBrackets};
    Color boxColor{0.0F, 1.0F, 0.75F, 0.9F};
    float boxOpacity{1.0F};
    float strokeWidth{2.0F};
    float padding{4.0F};
    float cornerLengthFraction{0.25F};
    bool markersEnabled{true};
    MarkerShape markerShape{MarkerShape::Ring};
    Color markerColor{0.0F, 1.0F, 0.75F, 0.9F};
    float markerOpacity{1.0F};
    float markerStrokeWidth{2.0F};
    float markerSize{8.0F};
    bool connectionsEnabled{};
    ConnectionTopology topology{ConnectionTopology::NearestNeighbor};
    std::size_t maximumConnections{128};
    float connectionRadius{160.0F};
    Color connectionColor{0.0F, 1.0F, 0.75F, 0.9F};
    float connectionOpacity{1.0F};
    float connectionWidth{2.0F};
    ConnectionStyle connectionStyle{ConnectionStyle::Straight};
    float connectionCurvature{0.25F};
    bool connectionDashEnabled{};
    float connectionDashLength{10.0F};
    float connectionGapLength{5.0F};
    bool connectionArrowsEnabled{};
    Color connectionArrowColor{0.85F, 0.95F, 1.0F, 1.0F};
    float connectionArrowOpacity{1.0F};
    float connectionArrowSize{12.0F};
    bool gridEnabled{};
    GridMode gridMode{GridMode::Cartesian};
    int gridDivisionsX{5};
    int gridDivisionsY{5};
    Color gridColor{1.0F, 1.0F, 1.0F, 1.0F};
    float gridOpacity{0.35F};
    float gridWidth{1.0F};
    bool gridAxisLabels{true};
    float gridTickSize{6.0F};
    bool gridProjections{true};
    Color projectionColor{0.55F, 1.0F, 0.15F, 1.0F};
    float projectionOpacity{0.85F};
    float projectionDashLength{8.0F};
    float projectionGapLength{6.0F};
    float globalOpacity{1.0F};
    LabelMode labelMode{LabelMode::Off};
    BitmapTextStyle labelStyle{};
    std::size_t maximumLabels{32U};
    bool labelLeaders{true};
    Color labelLeaderColor{0.0F, 1.0F, 0.75F, 0.9F};
    float labelLeaderOpacity{1.0F};
    float labelOffsetX{};
    float labelOffsetY{};
    std::string customLabelText{"CUSTOM"};
    FillMode fillMode{FillMode::Off};
    Color fillColor{0.0F, 1.0F, 0.75F, 1.0F};
    Color fillSecondaryColor{0.06F, 0.02F, 0.12F, 1.0F};
    float fillOpacity{0.35F};
    float fillInset{};
    float fillFeather{};
    float fillInvertStrength{1.0F};
    int hatchSpacing{12};
    int hatchWidth{3};
    float hatchAngleDegrees{45.0F};
};

struct Connection {
    std::size_t first{};
    std::size_t second{};
};

[[nodiscard]] std::vector<Connection> buildConnections(
    const std::vector<Region>& regions,
    ConnectionTopology topology,
    std::size_t maximumConnections,
    float maximumDistance = 160.0F);
void rasterizePolylines(
    Image& image,
    const std::vector<Polyline>& paths,
    Color color,
    float strokeWidth);
[[nodiscard]] Image renderOverlay(
    Image source,
    const Mask& detectionMask,
    const std::vector<Region>& regions,
    const OverlayStyle& style,
    OutputMode outputMode);

} // namespace linear
