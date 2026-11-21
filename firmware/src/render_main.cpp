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

    drawHelp(c, "Last game 9/12 shots made", 75, 84, "~11 games battery left");
    snprintf(path, sizeof path, "%s/help.pbm", dir);
    writePbm(c, path);
    return 0;
}
