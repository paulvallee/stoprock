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
    const Stone many[] = {{347, 1392}, {329, -1},  {351, 1366}, {-1, 1402}, {338, -1},  {342, 1381},
                          {361, 1420}, {333, 1375}, {-1, 1388}, {340, 1405}, {355, -1}, {336, 1397}};
    // competition: 25 stones, you lead, the third active, as tools/mockups/render.py COMP_STONES
    Stone comp[25];
    for (int i = 0; i < 25; ++i) {
        comp[i].bh = i % 5 == 3 ? -1 : 320 + i * 37 % 60;
        comp[i].hh = i % 7 == 4 ? -1 : 1350 + i * 53 % 90;
        comp[i].player = static_cast<uint8_t>(1 + (i + 3) / 2 % 3);  // you're lead: 2nd, 3rd, Skip in turn
    }
    // league: the same stones, your rocks as tools/mockups/render.py BASE + [False]
    ShotLog shots;
    for (bool m : {true, true, false, true, true, true, false}) shots.add(m);
    drawLeague(c, comp, 25, shots, 0, 1, false);
    snprintf(path, sizeof path, "%s/league.pbm", dir);
    writePbm(c, path);
    drawLeague(c, comp, 25, shots, 0, 1, true);
    snprintf(path, sizeof path, "%s/league-battery.pbm", dir);
    writePbm(c, path);
    for (int n = 0; n <= 16; ++n) {  // the cursor at every position; each position as yours, each column active
        ShotLog s;
        for (int i = 0; i < n; ++i) s.add(i % 3 != 2);
        drawLeague(c, comp, 25, s, n % 4, n % 3, false);
        snprintf(path, sizeof path, "%s/league%02d.pbm", dir, n);
        writePbm(c, path);
    }

    drawCompetition(c, comp, 25, 0, 1, false);
    snprintf(path, sizeof path, "%s/competition.pbm", dir);
    writePbm(c, path);
    drawCompetition(c, comp, 25, 0, 1, true);
    snprintf(path, sizeof path, "%s/competition-battery.pbm", dir);
    writePbm(c, path);
    for (uint8_t me = 0; me < 4; ++me) {  // each position as yours, the cursor on each column
        drawCompetition(c, comp, 25, me, me % 3, false);
        snprintf(path, sizeof path, "%s/competition-me%d.pbm", dir, me);
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

    // switch-on: no stones yet, the position tip over the times
    drawCompetition(c, comp, 0, 0, 0, false, true);
    snprintf(path, sizeof path, "%s/competition-tip.pbm", dir);
    writePbm(c, path);
    drawLeague(c, comp, 0, ShotLog(), 2, 0, false, true);
    snprintf(path, sizeof path, "%s/league-tip.pbm", dir);
    writePbm(c, path);

    drawHelp(c, "Last game 9/12 shots made", 75, 84, "~11 games battery left");
    snprintf(path, sizeof path, "%s/help.pbm", dir);
    writePbm(c, path);
    return 0;
}
