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

    const Stone stones[] = {{347, 1392}, {329, -1}, {351, 1366}, {-1, 1402}, {338, -1}, {342, 1381}};
    ShotLog shots;
    for (bool m : {true, true, false, true, true, true, false}) shots.add(m);

    MainView v;
    v.stones = stones;
    v.stoneCount = 6;
    v.shots = &shots;
    v.tip1 = "Top or side button both";
    v.tip2 = "time. Use either one.";
    drawMain(c, v);
    snprintf(path, sizeof path, "%s/main.pbm", dir);
    writePbm(c, path);

    v.tip1 = "Battery low";
    v.tip2 = "Charge after this game";
    v.warning = true;
    drawMain(c, v);
    snprintf(path, sizeof path, "%s/battery.pbm", dir);
    writePbm(c, path);

    // The cursor at every position: 0-16 rocks logged, a mix of made and missed.
    v.warning = false;
    for (int n = 0; n <= 16; ++n) {
        ShotLog s;
        for (int i = 0; i < n; ++i) s.add(i % 3 != 2);
        v.shots = &s;
        v.tip1 = "Rocker up: made shot";
        v.tip2 = "Rocker down: missed";
        drawMain(c, v);
        snprintf(path, sizeof path, "%s/cursor%02d.pbm", dir, n);
        writePbm(c, path);
    }

    // start-up modes: the same samples as tools/mockups/render.py (COACH_STONES, COACH_SHOTS)
    const Stone many[] = {{347, 1392}, {329, -1},  {351, 1366}, {-1, 1402}, {338, -1},  {342, 1381},
                          {361, 1420}, {333, 1375}, {-1, 1388}, {340, 1405}, {355, -1}, {336, 1397}};
    // competition: 25 stones and column 2 active, as tools/mockups/render.py COMP_STONES
    Stone comp[25];
    for (int i = 0; i < 25; ++i) {
        comp[i].bh = i % 5 == 3 ? -1 : 320 + i * 37 % 60;
        comp[i].hh = i % 7 == 4 ? -1 : 1350 + i * 53 % 90;
        comp[i].col = static_cast<uint8_t>(i % 4 == 2 ? 1 : i % 4 == 3 ? 2 : 0);
    }
    drawCompetition(c, comp, 25, 1, false);
    snprintf(path, sizeof path, "%s/competition.pbm", dir);
    writePbm(c, path);
    drawCompetition(c, comp, 25, 1, true);
    snprintf(path, sizeof path, "%s/competition-battery.pbm", dir);
    writePbm(c, path);
    for (uint8_t a : {0, 2}) {  // the cursor on the other columns
        drawCompetition(c, comp, 25, a, false);
        snprintf(path, sizeof path, "%s/competition-col%d.pbm", dir, a + 1);
        writePbm(c, path);
    }
    for (int n : {0, 8, 35, 63, 64}) {
        ShotLog s(ShotLog::kCoachSlots);
        for (int i = 0; i < n; ++i) s.add(i % 5 != 3 && i % 7 != 5);
        drawCoachTimer(c, many, n ? 12 : 0, s);
        snprintf(path, sizeof path, "%s/coach1-%02d.pbm", dir, n);
        writePbm(c, path);
        drawCoachRocks(c, many, n ? 12 : 0, s);
        snprintf(path, sizeof path, "%s/coach2-%02d.pbm", dir, n);
        writePbm(c, path);
    }

    drawHelp(c, "Last game 9/12 shots made", 75, 84, "~11 games battery left");
    snprintf(path, sizeof path, "%s/help.pbm", dir);
    writePbm(c, path);
    return 0;
}
