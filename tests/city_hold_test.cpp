// `city_hold_side`'s pure half (controller\src\impl\city_hold.h): the away side from the platforms' centroid in M13's
// city geometry (DATA\MISSIONS\M13\m13.ini: final_generator1 and platform1-3, St03b), the ab11 death bearings off it,
// the gap, the re-anchor, the ordnance verdict and the deeper-point rule.
#include "../controller/src/impl/city_hold.h"

#include "test.h"

void Test_city_hold()
{
    // m13.ini 889-899 / 944-976: final_generator1 and its three platforms (each ~1.56-1.6 km out)
    const float gen[3] = {-12155.1f, 0.661432f, 17539.0f};
    const float plat[3][3] = {{-12505.2f, -28.6537f, 15998.4f},
                              {-10607.9f, -28.6537f, 17178.3f},
                              {-11793.1f, -28.6538f, 19073.5f}};
    for (const auto& p : plat)
    {
        const float d[3] = {p[0] - gen[0], p[1] - gen[1], p[2] - gen[2]};
        CHECK(geom::Len(d) > 1500.0f && geom::Len(d) < 1650.0f);
    }
    // the city centre: the five final_generators' centroid (m13.ini 889-940)
    const float centre[3] = {(-12155.1f - 16247.3f - 23352.0f - 24787.5f - 18156.8f) / 5.0f, 0.66f,
                             (17539.0f + 24563.0f + 22568.3f + 17654.2f + 13442.1f) / 5.0f};
    float away[3];
    CHECK(cityhold::AwayDir(gen, plat, 3, away));
    CHECK_NEAR(away[1], 0.0f, 1e-6);
    CHECK_NEAR(geom::Len(away), 1.0f, 1e-4);
    const float toCentre[3] = {centre[0] - gen[0], 0.0f, centre[2] - gen[2]};
    // the side found from the platforms alone faces the city centre (within the +/-45 deg the rule takes)
    CHECK(cityhold::OffDeg(away, toCentre) < 45.0f);
    CHECK(cityhold::OnSide(away, toCentre));
    // every platform lies off the side (outward, +/-90 deg: none on the city-centre side)
    for (const auto& p : plat)
    {
        const float d[3] = {p[0] - gen[0], 0.0f, p[2] - gen[2]};
        CHECK(!cityhold::OnSide(away, d));
    }
    // ab11's deaths: 100-163 deg off the city-centre direction - never on the side
    float cdir[3] = {toCentre[0], 0.0f, toCentre[2]};
    geom::Norm(cdir);
    for (float deg : {100.0f, 130.0f, 163.0f, -100.0f, -163.0f})
    {
        float d[3];
        cityhold::SideDir(cdir, deg, 0.0f, d);
        CHECK(cityhold::OffDeg(cdir, d) > 99.0f);
        CHECK(!cityhold::OnSide(away, d));
    }
    // the candidates: azimuth 0 / +-22.5 / +-45 on the side, in the height band at elevation 0; tilted ones below it
    for (float az : {0.0f, 22.5f, -22.5f, 45.0f, -45.0f})
    {
        float d[3];
        cityhold::SideDir(away, az, 0.0f, d);
        CHECK_NEAR(geom::Len(d), 1.0f, 1e-4);
        CHECK(cityhold::OnSide(away, d));
        CHECK_NEAR(cityhold::OffDeg(away, d), fabsf(az), 0.05);
        // a city generator's hull ~915 m out + the gap: the point in the band
        CHECK(cityhold::InBand(gen[1], gen[1] + d[1] * (915.0f + 120.0f)));
    }
    {
        float d[3];
        cityhold::SideDir(away, 0.0f, -20.0f, d);
        CHECK(d[1] < 0);
        CHECK(cityhold::OnSide(away, d));
        CHECK(!cityhold::InBand(gen[1], gen[1] + d[1] * (915.0f + 120.0f))); // ~354 m below: the deeper candidate
        float out[3];
        cityhold::SideDir(away, 50.0f, 0.0f, out);
        CHECK(!cityhold::OnSide(away, out));
        const float up[3] = {0, 1, 0};
        CHECK(!cityhold::OnSide(away, up)); // straight above / below has no side
    }
    // no platform, or the centroid on the target's vertical: no side
    CHECK(!cityhold::AwayDir(gen, plat, 0, away));
    {
        const float above[1][3] = {{gen[0], gen[1] + 1500.0f, gen[2]}};
        float a2[3];
        CHECK(!cityhold::AwayDir(gen, above, 1, a2));
    }
    // the gap: 120-130 m whatever the base rule asks (the launch need ~47 m, static_hold_guns' steps to +300)
    CHECK_NEAR(cityhold::Gap(40.0f), 120.0f, 1e-6);
    CHECK_NEAR(cityhold::Gap(125.0f), 125.0f, 1e-6);
    CHECK_NEAR(cityhold::Gap(340.0f), 130.0f, 1e-6);
    // the re-anchor: our line on the side and a gap of at least 100 m
    CHECK(cityhold::ReanchorOk(away, toCentre, 120.0f));
    CHECK(!cityhold::ReanchorOk(away, toCentre, 61.0f)); // ab11: re-anchored at 31-61 m
    {
        float outward[3];
        cityhold::SideDir(away, 180.0f, 0.0f, outward);
        CHECK(!cityhold::ReanchorOk(away, outward, 120.0f));
    }
    // ordnance: the burst and the nearest hull both >= 100 m (none = clear)
    CHECK(cityhold::OrdnanceOk(150.0f, 120.0f));
    CHECK(cityhold::OrdnanceOk(-1.0f, -1.0f));
    CHECK(cityhold::OrdnanceOk(100.0f, 100.0f));
    CHECK(!cityhold::OrdnanceOk(94.0f, 130.0f)); // bsc0: a burst 94 m off killed us
    CHECK(!cityhold::OrdnanceOk(129.0f, 72.0f)); // ab11-b_i4: a 129 m gap, another hull 72 m off
    CHECK(!cityhold::OrdnanceOk(-1.0f, 31.0f));
    // the deeper point only when clearly safer by the platform-cover score
    CHECK(cityhold::DeeperSafer(16, 8));
    CHECK(cityhold::DeeperSafer(8, 6));
    CHECK(!cityhold::DeeperSafer(8, 7));   // one gun fewer: not clear
    CHECK(!cityhold::DeeperSafer(16, 13)); // under a quarter fewer
    CHECK(!cityhold::DeeperSafer(0, 0));
    CHECK(!cityhold::DeeperSafer(-1, 0));
    CHECK(!cityhold::DeeperSafer(10, -1));
    // the way to the side point (probe ch1: final_generator5, the ship held at bearing -35 deg, 472-489 m from the
    // centre; its platforms 13-15 put the side near +96 deg)
    {
        const float g5[3] = {-18156.8f, 0.66f, 13442.1f};
        const float p5[3][3] = {{-19720.0f, -28.65f, 13294.5f}, {-18011.6f, -28.65f, 11854.7f},
                                {-16575.4f, -28.65f, 13567.6f}};
        float away5[3];
        CHECK(cityhold::AwayDir(g5, p5, 3, away5));
        const float b = -35.0f * 3.14159265f / 180.0f;
        const float ship[3] = {g5[0] + 480.0f * cosf(b), g5[1], g5[2] + 480.0f * sinf(b)};
        const float shipDir[3] = {cosf(b), 0.0f, sinf(b)};
        CHECK(cityhold::OffDeg(away5, shipDir) > 120.0f); // the run-judge's 130-135 deg [M]
        float sd[3];
        cityhold::SideDir(away5, 45.0f, 0.0f, sd);
        const float point[3] = {g5[0] + sd[0] * 495.0f, g5[1], g5[2] + sd[2] * 495.0f};
        float wp[3];
        const float left = cityhold::TransitStep(g5, ship, point, wp);
        CHECK(left > cityhold::kTransitDeg);
        // the step: at the point's distance from the centre, 35 deg round from us, closer to the point than we are
        const float wr[3] = {wp[0] - g5[0], wp[1] - g5[1], wp[2] - g5[2]};
        CHECK_NEAR(geom::Len(wr), 495.0f, 0.5);
        const float sr[3] = {ship[0] - g5[0], ship[1] - g5[1], ship[2] - g5[2]};
        CHECK_NEAR(acosf(geom::Dot(wr, sr) / (geom::Len(wr) * geom::Len(sr))) * 180.0f / 3.14159265f, 35.0f, 0.1);
        CHECK(geom::Dist(wp, point) < geom::Dist(ship, point));
        // stepping on reaches the point itself
        float at[3] = {wp[0], wp[1], wp[2]};
        int steps = 0;
        while (cityhold::TransitStep(g5, at, point, wp) > cityhold::kTransitDeg && steps < 10)
            memcpy(at, wp, sizeof(at)), ++steps;
        CHECK(steps < 6);
        CHECK(geom::Dist(wp, point) < 0.5f);
        // opposite directions: a horizontal way round (no NaN)
        const float opp[3] = {g5[0] - (ship[0] - g5[0]), g5[1], g5[2] - (ship[2] - g5[2])};
        CHECK(cityhold::TransitStep(g5, ship, opp, wp) > 179.0f);
        CHECK(wp[0] == wp[0] && fabsf(wp[1] - g5[1]) < 0.5f);
    }
}
