// Port of Source/SoundCompass/Tests/SoundHaloGeometryTest.cpp: same inputs, same tolerances.
#include "Check.h"
#include "Geometry.h"

#include <vector>

using namespace halo;
using namespace halo::geom;

static int Sign(double v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

static void projection()
{
    const View view;
    const Projected origin = Project(Vec3{}, view);
    const Projected front = Project(RingPoint(0), view);
    const Projected back = Project(RingPoint(180), view);
    const Projected right = Project(RingPoint(90), view);
    CHECK(Length(origin.screen) <= 1e-4);
    CHECK(front.screen.y < 0.0);
    CHECK(back.screen.y > 0.0);
    CHECK(front.scale < back.scale);
    CHECK(FarAmount(front.depth, view) > FarAmount(back.depth, view));
    CHECK(right.screen.x > 0.0);
}

static void edge_points()
{
    const View view;
    const Vec2 half{800.0, 400.0};
    CHECK(Length(EdgePoint(0, half) - Vec2{0.0, -400.0}) <= 1e-3);
    CHECK(Length(EdgePoint(90, half) - Vec2{800.0, 0.0}) <= 1e-3);
    CHECK(Length(EdgePoint(180, half) - Vec2{0.0, 400.0}) <= 1e-3);
    for (const float bearing : {30.0f, 120.0f, -60.0f, -150.0f})
    {
        const Vec2 edge = EdgePoint(bearing, half);
        const Vec2 ring = Project(RingPoint(bearing), view).screen;
        CHECK(Sign(edge.x) == Sign(ring.x) && Sign(edge.y) == Sign(ring.y));
    }
}

static void distance_radius()
{
    CHECK(Near(DistanceToRadius(0, 2.6f), 1.0f));
    CHECK(DistanceToRadius(5, 2.6f) < DistanceToRadius(15, 2.6f));
    CHECK(Near(DistanceToRadius(1000, 2.6f), 2.6f));
    CHECK(Near(RadiusToDistance(DistanceToRadius(12, 9)), 12.0f, 1e-3));
}

static void pointers()
{
    CHECK(Length(PointerDirection(0, 10, 0) - Vec3{1, 0, 0}) <= 1e-4);
    const Vec3 upstairs = PointerDirection(90, 1.5f, 5);
    CHECK(upstairs.z > 0.5 && upstairs.y > 0.0);
    const Vec3 basement = PointerDirection(180, 5, -5);
    CHECK(basement.z < 0.0 && basement.x < 0.0);
    CHECK(PointerDirection(0, 0, 100).z <= std::sin(DegToRad(75.0)) + 1e-4);
    CHECK(Near(Length(PointerDirection(37, 3, 2)), 1.0, 1e-4));
}

static void nearness()
{
    CHECK(Near(Nearness(0, 40), 1.0f));
    CHECK(Near(Nearness(40, 40), 0.0f) && Near(Nearness(500, 40), 0.0f));
    CHECK(Nearness(3, 40) > Nearness(10, 40) && Nearness(10, 40) > Nearness(30, 40));
}

static void stacked_markers()
{
    std::vector<MarkerLayout> markers(3);
    for (MarkerLayout& item : markers)
    {
        item.anchor = Vec2{400.0, 300.0};
        item.iconRadius = 20.0f;
        item.labelSize = Vec2{70.0, 30.0};
    }
    LayoutMarkers(markers);
    bool clear = true;
    for (size_t a = 0; a < markers.size(); ++a)
    {
        const Box2 labelA{markers[a].labelTopLeft, markers[a].labelTopLeft + markers[a].labelSize};
        for (size_t b = 0; b < markers.size(); ++b)
        {
            clear &= !Overlaps(labelA, IconBox(markers[b]));
            if (b != a)
            {
                clear &= Length(markers[a].iconPos - markers[b].iconPos) >= 38.0;
                clear &= !Overlaps(labelA, Box2{markers[b].labelTopLeft, markers[b].labelTopLeft + markers[b].labelSize});
            }
        }
    }
    CHECK(clear);
    bool below = true;
    for (const MarkerLayout& item : markers)
    {
        below &= item.labelTopLeft.y > item.iconPos.y + item.iconRadius
            && Near(item.labelTopLeft.x + 0.5 * item.labelSize.x, item.iconPos.x, 1e-3);
    }
    CHECK(below);
}

int main()
{
    RUN(projection);
    RUN(edge_points);
    RUN(distance_radius);
    RUN(pointers);
    RUN(nearness);
    RUN(stacked_markers);
    return g_failures;
}
