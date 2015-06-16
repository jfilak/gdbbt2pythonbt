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
const std::string TRUNCATED("(truncated)");

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

            /* Parse the frame's arguments
             *
             * frame (arg1=text (.*...(truncated), arg2=arg2@entry=text 2 (.*))
             */
            start = std::find_if(end, line.end(), [](char c) { return c == '(';});
            if (start == line.end()) {
                std::cerr << "Frame has not open brace\n";
                continue;
            }
            end = ++start;
            while (*end != ')' && end != line.end()) {
                end = std::find_if(start, line.end(), [](char c) { return c == '=';});
                std::string arg_name(start, end);
                std::string kw_entry("=" + arg_name + "@entry");
                unsigned long line_size = static_cast<unsigned long>(std::distance(end, line.end()));
                if (static_cast<unsigned long>(kw_entry.size()) < line_size) {
                    if (std::equal(kw_entry.begin(), kw_entry.end(), end)) {
                        end += kw_entry.size();
                    }
                }

                start = ++end;
                if (start == line.end()) {
                    std::cerr << "Frame's argument " << arg_name << " does not have value\n";
                    break;
                }

                unsigned brackets = 0;
                bool body = false;
                unsigned not_truncated = 1;
                while (++end != line.end()) {
                    if (*end == '(') {
                        body = true;
                        if (std::equal(TRUNCATED.begin(), TRUNCATED.end(), end)) {
                            brackets = 0;
                            not_truncated = 0;
                            end += TRUNCATED.size() - 1;
                        }
                        ++brackets;
                    }
                    if (*end == ')') {
                        if (brackets == 0) {
                            break;
                        }
                        --brackets;
                    }
                    if (body && brackets == 0 && *end == ',') {
                        break;
                    }
                }
                current->arguments.insert(std::make_pair(arg_name, std::string(start, end - (not_truncated))));
                start = end + 1;
            }
        }
        else if (std::equal(NOSYMBOL.begin(), NOSYMBOL.end(), line.begin())) {
            continue;
        }
        else if (std::equal(FROM.begin(), FROM.end(), line.begin())) {
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
                    std::string prefix;
                    for (auto arg_iter (search->second.begin()); arg_iter != search->second.end(); ++arg_iter) {
                        std::string suffix;
                        if (*arg_iter == '(' || *arg_iter == '[') {
                            prefix.push_back(' ');
                            suffix = "\n " + prefix;
                        }
                        else if (*arg_iter == ',' && prefix.size() > 0) {
                            suffix = "\n" + prefix;
                        }
                        else if (*arg_iter == ')' || *arg_iter == ']') {
                            if (prefix.size() > 0) {
                                prefix.erase(prefix.begin());
                            }
                            suffix = "\n" + prefix;
                        }
                        std::cout << *arg_iter << suffix;
                    }
                    std::cout << std::endl;
                }
                level = test_level;
                break;
            }
        }
    }

    infs.close();

    return 0;
}
