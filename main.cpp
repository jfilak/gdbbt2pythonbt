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

            while (*end != ' ' && end != line.end()) {
                ++end;
            }

            if (end == start || end == line.end()) {
                std::cerr << "Failed to found end of frame ID\n";
                continue;
            }

            frame f;
            f.id.assign(start, end);

            while (*end == ' ' && end != line.end()) {
                ++end;
            }

            if (end == line.end()) {
                std::cerr << "Frame misses function name\n";
                continue;
            }

            start = end++;
            if (*start == '0' && end != line.end() && *end == 'x') {
                while (*end != ' ' && end != line.end()) {
                    ++end;
                }

                while (*end == ' ' && end != line.end()) {
                    ++end;
                }

                if (   *end != 'i' || ++end == line.end()
                    || *end != 'n' || ++end == line.end()
                    || *end != ' ' || ++end == line.end()) {
                    std::cerr << "Frame misses in\n";
                    continue;
                }

                start = end++;
            }

            while (*end != ' ' && end != line.end()) {
                ++end;
            }

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
            while (*end == ' ' && end != line.end()) {
                ++end ;
            }

            auto start(end++);

            while (*end != ' ' && end != line.end()) {
                ++end ;
            }

            if (   *end != ' ' || ++end == line.end()
                || *end != '=' || ++end == line.end()
                || *end != ' ' || ++end == line.end()) {
                std::cerr << "Argument misses =\n" << line << std::endl;
                continue;
            }

            if (!std::equal(OPTIMIZEDOUT.begin(), OPTIMIZEDOUT.end(), end)) {
                current->arguments.insert(std::make_pair(std::string(start, end - 3), std::string(end, line.end())));
            }
        }
    }

    bool reached_python = false;
    for (auto iter(frames.begin()); iter != frames.end(); ++iter) {
        if (!reached_python) {
            if (iter->name == "import_submodule") {
                std::cout << iter->id << " " << iter->name << " => ";
                auto search(iter->arguments.find("buf"));
                if (search == iter->arguments.end()) {
                    std::cout << "<no imported module>\n";
                }
                else {
                    std::cout << search->second << std::endl;
                }
            }
        }

        if (iter->name == "PyEval_EvalCodeEx") {
            reached_python = true;
            std::cout << iter->id << " " << iter->name << " => ";
            auto search(iter->arguments.find("f"));
            if (search == iter->arguments.end()) {
                std::cout << "<no python frame>\n";
            }
            else {
                std::cout << search->second << std::endl;
            }
        }
    }

    infs.close();

    return 0;
}
