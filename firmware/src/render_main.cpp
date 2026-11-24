// Host tool: draws the firmware's screens to PBM files so they can be compared with
// the mockups (tools/compare_render.py). Built only in the `render` environment.
#include <stdio.h>

#include <initializer_list>

#include "screens.h"

using namespace stoprock;

static void writePbm(const Canvas& c, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P4\n%d %d\n", Canvas::W, Canvas::H);
    fwrite(c.data(), 1, Canvas::STRIDE * Canvas::H, f);
    fclose(f);
    printf("wrote %s\n", path);
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    char path[256];
    Canvas c;

    // start-up modes: the same samples as tools/mockups/render.py (COACH_STONES, COACH_SHOTS)
    const Stone many[] = {{347, 1392, 4}, {329, -1, 1},  {351, 1366, 4}, {-1, 1402, 1}, {338, -1, 4},
                          {342, 1381, 0}, {361, 1420, 4}, {333, 1375, 0}, {-1, 1388, 4},  {340, 1405, 3},
                          {355, -1, 4},  {336, 1397, 3}};
    // competition and league: you're 2nd, as tools/mockups/render.py LEAGUE_STONES; your rocks BASE + [False]
    const Stone league[] = {{341, 1386, 4}, {347, 1392, 2}, {329, -1, 4},  {351, 1366, 2}, {-1, 1402, 4},
                            {338, 1371, 4}, {342, 1381, 0}, {361, 1420, 4}, {333, 1375, 0}, {340, 1405, 4},
                            {355, -1, 3},  {336, 1397, 4}, {344, 1362, 3}, {358, 1411, 4}};
    const uint8_t nl = sizeof league / sizeof league[0];
    ShotLog shots;
    for (bool m : {true, true, false, true, true, true, false}) shots.add(m);
    drawLeague(c, league, nl, shots, 1, 1, false);
    snprintf(path, sizeof path, "%s/league.pbm", dir);
    writePbm(c, path);
    drawLeague(c, league, nl, shots, 1, 1, true);
    snprintf(path, sizeof path, "%s/league-battery.pbm", dir);
    writePbm(c, path);
    for (int n = 0; n <= 16; ++n) {  // the cursor at every position; each position as yours, each column active
        ShotLog s;
        for (int i = 0; i < n; ++i) s.add(i % 3 != 2);
        drawLeague(c, league, nl, s, n % 4, n % 4, false, Tip::None, Waiting::None, n % 2 ? Focus::Rocks : Focus::Players);
        snprintf(path, sizeof path, "%s/league%02d.pbm", dir, n);
        writePbm(c, path);
    }

    drawCompetition(c, league, nl, 1, 1, false);
    snprintf(path, sizeof path, "%s/competition.pbm", dir);
    writePbm(c, path);
    drawCompetition(c, league, nl, 1, 1, true);
    snprintf(path, sizeof path, "%s/competition-battery.pbm", dir);
    writePbm(c, path);
    for (uint8_t me = 0; me < 4; ++me) {  // each position as yours, the cursor on each column
        drawCompetition(c, league, nl, me, (me + 1) % 4, false);
        snprintf(path, sizeof path, "%s/competition-me%d.pbm", dir, me);
        writePbm(c, path);
    }
    for (int n : {0, 8, 35, 63, 64}) {
        ShotLog s(ShotLog::kCoachSlots);
        for (int i = 0; i < n; ++i) s.add(i % 5 != 3 && i % 7 != 5);
        drawCoachTimer(c, many, n ? 12 : 0, s, Waiting::None, static_cast<uint8_t>(n % 5));
        snprintf(path, sizeof path, "%s/coach1-%02d.pbm", dir, n);
        writePbm(c, path);
        drawCoachRocks(c, many, n ? 12 : 0, s);
        snprintf(path, sizeof path, "%s/coach2-%02d.pbm", dir, n);
        writePbm(c, path);
    }

    // switch-on: no stones yet, the position tip over the times
    drawCompetition(c, league, 0, 0, 0, false, Tip::SwitchOn);
    snprintf(path, sizeof path, "%s/competition-tip.pbm", dir);
    writePbm(c, path);
    drawLeague(c, league, 0, ShotLog(), 2, 0, false, Tip::SwitchOn);
    snprintf(path, sizeof path, "%s/league-tip.pbm", dir);
    writePbm(c, path);
    drawCoachTimer(c, many, 0, ShotLog(ShotLog::kCoachSlots), Waiting::None, 0, Tip::SwitchOn);
    snprintf(path, sizeof path, "%s/coach1-tip.pbm", dir);
    writePbm(c, path);
    // the rocker centre held: "Keep holding to undo"
    drawCompetition(c, league, nl, 1, 1, false, Tip::Undo);
    snprintf(path, sizeof path, "%s/competition-undo.pbm", dir);
    writePbm(c, path);
    drawLeague(c, league, nl, shots, 1, 1, false, Tip::Undo);
    snprintf(path, sizeof path, "%s/league-undo.pbm", dir);
    writePbm(c, path);
    {
        ShotLog s35(ShotLog::kCoachSlots);
        for (int i = 0; i < 35; ++i) s35.add(i % 5 != 3 && i % 7 != 5);
        drawCoachTimer(c, many, 12, s35, Waiting::None, 0, Tip::Undo);
        snprintf(path, sizeof path, "%s/coach1-undo.pbm", dir);
        writePbm(c, path);
        drawCoachRocks(c, many, 12, s35, Waiting::None, Tip::Undo);
        snprintf(path, sizeof path, "%s/coach2-undo.pbm", dir);
        writePbm(c, path);
    }

    // a stone being timed: the stopwatch where the awaited time will appear
    drawCompetition(c, league, nl, 1, 1, false, Tip::None, Waiting::BackToHog);
    snprintf(path, sizeof path, "%s/competition-wait-b2h.pbm", dir);
    writePbm(c, path);
    drawLeague(c, league, nl, shots, 1, 1, false, Tip::None, Waiting::BackToHog, Focus::Rocks);
    snprintf(path, sizeof path, "%s/league-rocks.pbm", dir);
    writePbm(c, path);
    // 5.5 s with no second click: an H2H under way, the stopwatch in the H2H box
    drawLeague(c, league, nl, shots, 1, 3, false, Tip::None, Waiting::HogToHogOnly);
    snprintf(path, sizeof path, "%s/league-wait-h2h-only.pbm", dir);
    writePbm(c, path);
    drawCompetition(c, league, nl, 1, 3, false, Tip::None, Waiting::HogToHogOnly);
    snprintf(path, sizeof path, "%s/competition-wait-h2h-only.pbm", dir);
    writePbm(c, path);
    Stone lhalf[sizeof league / sizeof league[0]];
    for (int i = 0; i < nl; ++i) lhalf[i] = league[i];
    lhalf[0].hh = -1;
    drawLeague(c, lhalf, nl, shots, 1, 3, false, Tip::None, Waiting::HogToHog);
    snprintf(path, sizeof path, "%s/league-wait-h2h.pbm", dir);
    writePbm(c, path);
    {
        ShotLog s(ShotLog::kCoachSlots);
        for (int i = 0; i < 35; ++i) s.add(i % 5 != 3 && i % 7 != 5);
        Stone coach[12];
        for (int i = 0; i < 12; ++i) coach[i] = many[i];
        drawCoachTimer(c, coach, 12, s, Waiting::BackToHog);
        snprintf(path, sizeof path, "%s/coach1-wait-b2h.pbm", dir);
        writePbm(c, path);
        coach[0].hh = -1;
        drawCoachRocks(c, coach, 12, s, Waiting::HogToHog);
        snprintf(path, sizeof path, "%s/coach2-wait-h2h.pbm", dir);
        writePbm(c, path);
        drawCoachRocks(c, many, 12, s, Waiting::HogToHogOnly);
        snprintf(path, sizeof path, "%s/coach2-wait-h2h-only.pbm", dir);
        writePbm(c, path);
    }

    drawHelp(c, "Last game 9/12 shots made", 75, 84, "~11 games battery left");
    snprintf(path, sizeof path, "%s/help.pbm", dir);
    writePbm(c, path);
    return 0;
}
