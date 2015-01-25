#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <utility>
#include <cstdlib>

typedef std::map<std::string, std::string> args_map;

struct frame {
    std::string id;
    std::string name;
    args_map arguments;
};

const std::string OPTIMIZEDOUT("<optimized out>");
const std::string THREAD("Thread");
const std::string NOSYMBOL("No symbol table info available.");
const std::string FROM("From");
const std::string NOLOCALS("No locals");

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Need exactly one argument\n";
        exit(EXIT_FAILURE);
    }

    std::ifstream infs(argv[1]);
    if (!infs) {
        std::cerr << "Failed to open : " << argv[1] << std::endl;
        exit(EXIT_FAILURE);
    }

    std::vector<frame> frames;
    frame *current = NULL;
    for (std::string line; std::getline(infs, line); ) {
        if (std::equal(THREAD.begin(), THREAD.end(), line.begin())) {
            break;
        }
    }

    for (std::string line; std::getline(infs, line); ) {
        if (line.size() > 0 && line[0] == '#') {
            auto start(line.begin());
            auto end(start);

            end = std::find_if(end, line.end(), [](char c) { return c == ' ';});

            if (end == start || end == line.end()) {
                std::cerr << "Failed to found end of frame ID\n";
                continue;
            }

            frame f;
            f.id.assign(start, end);

            end = std::find_if_not(end, line.end(), [](char c) { return c == ' ';});

            if (end == line.end()) {
                std::cerr << "Frame misses function name\n";
                continue;
            }

            start = end++;
            if (*start == '0' && end != line.end() && *end == 'x') {
                end = std::find_if(end, line.end(), [](char c) { return c == ' ';});
                end = std::find_if_not(end, line.end(), [](char c) { return c == ' ';});

                if (   *end != 'i' || ++end == line.end()
                    || *end != 'n' || ++end == line.end()
                    || *end != ' ' || ++end == line.end()) {
                    std::cerr << "Frame misses in\n";
                    continue;
                }

                start = end++;
            }

            end = std::find_if(end, line.end(), [](char c) { return c == ' ';});

            f.name.assign(start, end);

            frames.push_back(f);
            current = &(frames[frames.size() - 1]);
        }
        else if (   std::equal(NOSYMBOL.begin(), NOSYMBOL.end(), line.begin())
                 || std::equal(FROM.begin(), FROM.end(), line.begin())) {
            break;
        }
        else if (current) {
            if (std::equal(NOLOCALS.begin(), NOLOCALS.end(), line.begin())) {
                continue;
            }

            auto end(line.begin());
            end = std::find_if_not(end, line.end(), [](char c) { return c == ' ';});

            auto start(end++);
            end = std::find_if(end, line.end(), [](char c) { return c == ' ';});

            if (   *end != ' ' || ++end == line.end()
                || *end != '=' || ++end == line.end()
                || *end != ' ' || ++end == line.end()) {
                std::cerr << "Argument misses =\n'" << line << "'\n";
                continue;
            }

            if (!std::equal(OPTIMIZEDOUT.begin(), OPTIMIZEDOUT.end(), end)) {
                current->arguments.insert(std::make_pair(std::string(start, end - 3), std::string(end, line.end())));
            }
        }
    }

    std::vector<std::pair<std::string, std::pair<std::string, std::string> > > frame_order {
        {"import_submodule",  { "buf", "import submodule"} },
        {"PyEval_EvalCodeEx", { "f" , "python frame" } }
    };

    unsigned level { 0 };
    for (auto iter(frames.begin()); iter != frames.end(); ++iter) {
        for (unsigned test_level { level }; test_level < frame_order.size(); ++test_level) {
            auto &print_frame(frame_order.at(test_level));
            if (iter->name == print_frame.first) {
                std::cout << iter->id << " " << iter->name << " => ";
                auto search(iter->arguments.find(print_frame.second.first));
                if (search == iter->arguments.end()) {
                    std::cout << "<no " << print_frame.second.second << ">\n";
                }
                else {
                    std::cout << search->second << std::endl;
                }
                level = test_level;
                break;
            }
        }
    }

    infs.close();

    return 0;
}
