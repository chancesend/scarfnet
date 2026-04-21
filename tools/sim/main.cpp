// Scarfnet pattern simulator — terminal/ANSI, no dependencies.
//
// Each LED renders as two background-colored spaces using ANSI truecolor.
// Raw terminal mode gives instant keypress response at ~30 fps.
//
// Controls:
//   SPACE — next pattern
//   R     — new global seed (same pattern, different look)
//   T     — tap tempo (two taps sets BPM; third+ tap refines)
//   +     — add scarf  (max 8)
//   -     — remove scarf (min 1)
//   Q / ESC — quit

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <algorithm>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

#include "defines.h"
#include "patterns.h"
#include "palettes.h"
#include "PatternManager.h"
#include "tap_tempo.h"

using namespace Scarfnet;

// ── Terminal raw mode ─────────────────────────────────────────────────────────

static struct termios s_origTermios;

static void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_origTermios);
    // Restore cursor
    printf("\033[?25h\033[0m\n");
    fflush(stdout);
}

static void enableRawMode() {
    tcgetattr(STDIN_FILENO, &s_origTermios);
    atexit(disableRawMode);

    struct termios raw = s_origTermios;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;   // non-blocking read
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Hide cursor during animation
    printf("\033[?25l");
    fflush(stdout);
}

// Returns the pressed key, or 0 if no input available.
static char readKey() {
    char c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}

// ── Time ──────────────────────────────────────────────────────────────────────

static uint32_t wallMs() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

// ── ANSI helpers ──────────────────────────────────────────────────────────────

// Print two spaces with background color r,g,b.
static void printLed(uint8_t r, uint8_t g, uint8_t b) {
    printf("\033[48;2;%u;%u;%um  \033[0m", r, g, b);
}

// Move cursor to top-left without clearing (avoids flicker).
static void cursorHome() { printf("\033[H"); }

// ── Scarf state ───────────────────────────────────────────────────────────────

struct SimScarf {
    PatternManager pm;
    Leds           leds;
    SimScarf() : leds(kNumLeds) {}
};

// ── Main ──────────────────────────────────────────────────────────────────────

int main()
{
    enableRawMode();

    // Clear screen once at startup
    printf("\033[2J");
    fflush(stdout);

    srand((unsigned)wallMs());

    Rnd      globalRnd = (Rnd)(rand() & 0xFFFF);
    TapTempo tapTempo;

    // Scarves — each PatternManager gets a fresh localRnd via random16() on
    // each changePatternFromString call, so they naturally differ.
    std::vector<SimScarf*> scarves;

    auto addScarf = [&]() {
        auto* s = new SimScarf();
        if (!scarves.empty()) {
            // Sync new scarf to the current fleet pattern + seed.
            s->pm.changePatternFromString(
                scarves[0]->pm.getCurrentPattern(), globalRnd);
        }
        scarves.push_back(s);
    };

    auto removeScarf = [&]() {
        if (!scarves.empty()) { delete scarves.back(); scarves.pop_back(); }
    };

    for (int i = 0; i < 4; ++i) addScarf();

    auto nextPattern = [&]() {
        globalRnd = (Rnd)wallMs();
        scarves[0]->pm.incrementPattern(globalRnd);
        auto pat = scarves[0]->pm.getCurrentPattern();
        for (size_t i = 1; i < scarves.size(); ++i)
            scarves[i]->pm.changePatternFromString(pat, globalRnd);
    };

    auto newSeed = [&]() {
        globalRnd = (Rnd)wallMs();
        auto pat = scarves[0]->pm.getCurrentPattern();
        for (auto* s : scarves)
            s->pm.changePatternFromString(pat, globalRnd);
    };

    uint32_t lastBlendMs  = wallMs();
    uint32_t lastFrameMs  = wallMs();
    constexpr uint32_t kFrameMs = 33;   // ~30 fps

    while (true)
    {
        uint32_t now = wallMs();

        // ── Input ──────────────────────────────────────────────────────────────
        char key = readKey();
        switch (key) {
            case ' ':              nextPattern(); break;
            case 'r': case 'R':   newSeed();     break;
            case 't': case 'T':   tapTempo.tap(now); break;
            case '+': case '=':
                if ((int)scarves.size() < 8) addScarf();
                break;
            case '-':
                if (scarves.size() > 1) removeScarf();
                break;
            case 'q': case 'Q': case 27 /* ESC */:
                for (auto* s : scarves) delete s;
                return 0;
        }

        // ── Simulate ───────────────────────────────────────────────────────────
        BeatInfo beat = tapTempo.beatInfo(now);

        if (now - lastBlendMs >= 20) {
            for (auto* s : scarves) s->pm.blendPalette();
            lastBlendMs = now;
        }

        // Cap frame rate
        if (now - lastFrameMs < kFrameMs) {
            usleep(2000);
            continue;
        }
        lastFrameMs = now;

        for (auto* s : scarves)
            s->pm.runCurrentPattern(s->leds, now, beat);

        // ── Render ─────────────────────────────────────────────────────────────
        cursorHome();

        // ── Header ────────────────────────────────────────────────────────────
        std::string patName = scarves[0]->pm.getCurrentPattern();

        // Beat indicator: bright green dot when on-beat, dim otherwise
        std::string beatStr;
        if (beat.isActive()) {
            int bpm = (int)std::round(60000.0 / beat.intervalMs);
            bool onBeat = beat.isOnBeat(beat.intervalMs / 4);
            beatStr = onBeat
                ? "\033[92m● " + std::to_string(bpm) + " BPM\033[0m"
                : "\033[32m○ " + std::to_string(bpm) + " BPM\033[0m";
        } else {
            beatStr = "\033[90mno tempo\033[0m";
        }

        char seedStr[16];
        snprintf(seedStr, sizeof(seedStr), "0x%04X", (int)globalRnd);

        printf("\033[1;36mScarfnet Simulator\033[0m"
               "  pattern: \033[1;97m%-12s\033[0m"
               "  %s"
               "  seed: \033[90m%s\033[0m"
               "  scarves: \033[97m%d\033[0m  \n",
               patName.c_str(), beatStr.c_str(), seedStr, (int)scarves.size());

        // Separator
        printf("\033[90m");
        for (int i = 0; i < kNumLeds * 2 + 6; ++i) putchar('-');
        printf("\033[0m\n");

        // ── Scarf rows ────────────────────────────────────────────────────────
        for (int si = 0; si < (int)scarves.size(); ++si) {
            printf("\033[90mS%d\033[0m ", si + 1);
            for (int li = 0; li < kNumLeds; ++li) {
                CRGB c = scarves[si]->leds[li];
                printLed(c.r, c.g, c.b);
            }
            printf(" \n");
        }

        // Separator
        printf("\033[90m");
        for (int i = 0; i < kNumLeds * 2 + 6; ++i) putchar('-');
        printf("\033[0m\n");

        // ── Controls ──────────────────────────────────────────────────────────
        printf("\033[90m[SPC]\033[0m next  "
               "\033[90m[R]\033[0m seed  "
               "\033[90m[T]\033[0m tap tempo  "
               "\033[90m[+/-]\033[0m scarves  "
               "\033[90m[Q]\033[0m quit     \n");

        fflush(stdout);
    }
}
