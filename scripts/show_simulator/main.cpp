//
// show_simulator: host CLI that drives any registered show against a
// MockStrip and writes raw RGB bytes to stdout.
//
// Wire format: width * iterations * 3 bytes, row-major, no header, no
// per-row framing. One row per iteration, RGB triplets left-to-right.
// See docs/SHOW_PREVIEWS.md for the rationale.
//

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "show/factory/ShowFactory.h"
#include "support/Random.h"

#include "MockStrip.h"
#include "simulator_clock.h"

namespace {

constexpr int kDefaultWidth = 300;
constexpr int kDefaultIterations = 1000;
constexpr int kSeedUnset = -1;

// JSON keys read by each show's factory lambda. Mirrors the `doc["key"]`
// reads in src/show/factory/ShowFactory.cpp — kept here rather than in the
// factory so the firmware build stays untouched. A new show with parameters
// must add an entry here.
const std::map<std::string, std::vector<std::string>> &parameterKeysByShow() {
    static const std::map<std::string, std::vector<std::string>> table = {
        {"Solid",        {"colors", "ranges", "gradient"}},
        {"Fire",         {"cooling", "spread", "ignition", "spark_amount", "start_offset", "spark_range"}},
        {"Starlight",    {"probability", "length", "fade", "r", "g", "b"}},
        {"Stroboscope",  {"r", "g", "b", "on_cycles", "off_cycles"}},
        {"ColorRun",     {}},
        {"Jump",         {}},
        {"Rainbow",      {"time_step", "pixel_step"}},
        {"Wave",         {"mode", "decay_rate", "brightness_frequency"}},
        {"TheaterChase", {"num_steps_per_cycle"}},
        {"MorseCode",    {"message", "speed", "dot_length", "dash_length", "symbol_space", "letter_space", "word_space"}},
        {"Chaos",        {"Rmin", "Rmax", "Rdelta"}},
        {"Mandelbrot",   {"Cre0", "Cim0", "Cim1", "scale", "max_iterations", "color_scale"}},
    };
    return table;
}

const std::vector<std::string> &parameterKeysFor(const std::string &name) {
    static const std::vector<std::string> empty;
    const auto &table = parameterKeysByShow();
    auto it = table.find(name);
    if (it == table.end()) return empty;
    return it->second;
}

struct Options {
    bool list = false;
    bool listParams = false;
    std::string listParamsShow;
    std::string show;
    std::string params = "{}";
    int width = kDefaultWidth;
    int iterations = kDefaultIterations;
    long seed = kSeedUnset;
};

[[noreturn]] void die(const std::string &msg, int code = 1) {
    std::cerr << "show_simulator: " << msg << "\n"
              << "Try `show_simulator --help` for usage.\n";
    std::exit(code);
}

std::string nextValue(int &i, int argc, char **argv, const char *flag) {
    if (i + 1 >= argc) {
        die(std::string(flag) + " requires a value", 2);
    }
    return argv[++i];
}

Options parseArgs(int argc, char **argv) {
    Options opts;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: show_simulator [options]\n"
                << "  --show <name>            Show to render (required for render mode)\n"
                << "  --width <pixels>         Strip length; default " << kDefaultWidth << "\n"
                << "  --iterations <count>     Number of iterations; default " << kDefaultIterations << "\n"
                << "  --params <json>          JSON params for the show; default {}\n"
                << "  --seed <n>               RNG seed for deterministic random shows\n"
                << "  --list                   Print every registered show name and exit\n"
                << "  --list-params <name>     Print the JSON keys accepted by a show and exit\n"
                << "Raw RGB bytes (width * iterations * 3) are written to stdout.\n";
            std::exit(0);
        }

        if (arg == "--list") {
            opts.list = true;
            continue;
        }
        if (arg == "--list-params") {
            opts.listParams = true;
            opts.listParamsShow = nextValue(i, argc, argv, "--list-params");
            continue;
        }
        if (arg == "--show") {
            opts.show = nextValue(i, argc, argv, "--show");
            continue;
        }
        if (arg == "--width") {
            opts.width = std::stoi(nextValue(i, argc, argv, "--width"));
            continue;
        }
        if (arg == "--iterations") {
            opts.iterations = std::stoi(nextValue(i, argc, argv, "--iterations"));
            continue;
        }
        if (arg == "--params") {
            opts.params = nextValue(i, argc, argv, "--params");
            continue;
        }
        if (arg == "--seed") {
            opts.seed = std::stol(nextValue(i, argc, argv, "--seed"));
            continue;
        }

        die("unknown flag: " + arg);
    }

    return opts;
}

void runList(Show::Factory::ShowFactory &factory) {
    // Use std::cout for human-readable output; not part of the raw RGB stream.
    std::ios::sync_with_stdio(true);
    for (const auto &info : factory.listShows()) {
        std::cout << info.name << "\n";
    }
    std::cout.flush();
}

void runListParams(Show::Factory::ShowFactory &factory, const std::string &name) {
    if (!factory.hasShow(name)) {
        die("unknown show: " + name);
    }
    std::ios::sync_with_stdio(true);
    for (const auto &key : parameterKeysFor(name)) {
        std::cout << key << "\n";
    }
    std::cout.flush();
}

void runRender(const Options &opts) {
    if (opts.width < 1) die("--width must be >= 1");
    if (opts.iterations < 1) die("--iterations must be >= 1");
    if (opts.show.empty()) die("--show is required (or pass --list / --list-params)");

    // Starlight uses C rand(); set it before any show construction so even
    // shows that consume it lazily in their constructor see the seed.
    if (opts.seed != kSeedUnset) {
        Support::setRandomSeedOverride(static_cast<Support::Random::result_type>(opts.seed));
        std::srand(static_cast<unsigned>(opts.seed));
    }

    Show::Factory::ShowFactory factory;
    auto showPtr = factory.createShow(opts.show, opts.params);
    if (!showPtr) {
        // Mirror ShowFactory's behaviour: log and list available names so the
        // caller can spot typos without digging through docs.
        std::string names;
        for (const auto &info : factory.listShows()) {
            if (!names.empty()) names += ", ";
            names += info.name;
        }
        die("unknown show: " + opts.show + " (available: " + names + ")");
    }

    MockStrip strip(static_cast<Strip::PixelIndex>(opts.width));

    // No human-readable stdout — raw RGB stream is the contract.
    std::ios::sync_with_stdio(false);
    std::cout.setf(std::ios::unitbuf);

    const std::size_t rowBytes = static_cast<std::size_t>(opts.width) * 3;
    std::string row;
    row.resize(rowBytes);

    auto emitRow = [&]() {
        const auto &buf = strip.buffer();
        for (int i = 0; i < opts.width; ++i) {
            const auto c = buf[i];
            row[i * 3 + 0] = static_cast<char>((c >> 16) & 0xFF);
            row[i * 3 + 1] = static_cast<char>((c >> 8) & 0xFF);
            row[i * 3 + 2] = static_cast<char>(c & 0xFF);
        }
        std::cout.write(row.data(), static_cast<std::streamsize>(rowBytes));
    };

    // Reset the simulator's deterministic clock to zero so SmoothBlend-driven
    // shows (Solid) hit the same elapsed-time value regardless of how fast
    // the simulator loop runs on this host.
    SimulatorClock::reset();

    // Row i = strip state AFTER execute(i). Iteration 0 starts from the
    // factory-constructed strip (all zeros for most shows); subsequent
    // iterations carry the show's internal state forward, matching how the
    // device's 100 Hz loop calls execute() in sequence.
    for (Show::Iteration it = 0; it < static_cast<Show::Iteration>(opts.iterations); ++it) {
        SimulatorClock::advance();
        showPtr->execute(strip, it);
        emitRow();
    }

    std::cout.flush();
}

} // namespace

int main(int argc, char **argv) {
    Options opts = parseArgs(argc, argv);

    // Build the factory exactly once; needed for both --list branches and the
    // render branch (so the --list branch still benefits from any constructor-
    // time logging).
    Show::Factory::ShowFactory factory;

    if (opts.list) {
        runList(factory);
        return 0;
    }
    if (opts.listParams) {
        runListParams(factory, opts.listParamsShow);
        return 0;
    }
    runRender(opts);
    return 0;
}
