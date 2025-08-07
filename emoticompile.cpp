#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <string>
#include <regex>
#include <filesystem>
#include <random>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <iomanip>

using namespace std;
namespace fs = std::filesystem;

bool is_valid_string_literal(const string& tk) {
    if (tk.rfind(":S_\"", 0) != 0) return false;
    if (tk.size() < 5) return false;

    size_t last_quote_pos = tk.size() - 1;
    if (tk[last_quote_pos] != '"') return false;

    size_t bs_count = 0;
    size_t i = last_quote_pos;
    while (i > 0 && tk[i - 1] == '\\') {
        --i;
        ++bs_count;
    }

    return (bs_count % 2 == 0);
}

vector<string> tokenize(const string& source) {
    vector<string> tokens;
    size_t i = 0;

    while (i < source.size()) {
        while (i < source.size() && isspace((unsigned char)source[i])) ++i;
        if (i >= source.size()) break;

        if (source.compare(i, 4, ":S_\"") == 0) {
            size_t start = i;
            i += 4;
            bool escaped = false;
            while (i < source.size()) {
                if (source[i] == '\\' && !escaped) {
                    escaped = true;
                } else if (source[i] == '"' && !escaped) {
                    ++i;
                    break;
                } else {
                    escaped = false;
                }
                ++i;
            }
            tokens.push_back(source.substr(start, i - start));
            continue;
        }

        if (source[i] == ':' && i + 2 < source.size()) {
            size_t start = i;
            while (i < source.size() && source[i] != '[' && !isspace((unsigned char)source[i])) {
                ++i;
            }
            if (i < source.size() && source[i] == '[') {
                ++i;
                int depth = 1;
                while (i < source.size() && depth > 0) {
                    if (source[i] == '[') ++depth;
                    else if (source[i] == ']') --depth;
                    ++i;
                }
            }
            tokens.push_back(source.substr(start, i - start));
            continue;
        }

        size_t start = i;
        while (i < source.size() && !isspace((unsigned char)source[i])) ++i;
        tokens.push_back(source.substr(start, i - start));
    }

    return tokens;
}

vector<string> preprocess(const vector<string>& tokens) {
    unordered_map<string, vector<string>> macros;
    bool in_macro = false;
    string current_macro;
    vector<string> macro_buffer;

    for (const string& tk : tokens) {
        if (is_valid_string_literal(tk)) {
            if (in_macro) macro_buffer.push_back(tk);
            continue;
        }

        if (!in_macro && regex_match(tk, regex(R"(^:O_\[.+\]$)"))) {
            in_macro = true;
            current_macro = tk.substr(4, tk.size() - 5);
            macro_buffer.clear();
            continue;
        }

        if (in_macro && regex_match(tk, regex(R"(^:O_\[.+\]$)"))) {
            throw runtime_error("Nested macros not supported: " + tk);
        }

        if (tk.rfind(":O_", 0) == 0) {
            throw runtime_error("Deprecated macro syntax: '" + tk + "' use :O_[name] in v2.3+.");
        }

        if (in_macro && tk == ":|") {
            in_macro = false;
            macros[current_macro] = macro_buffer;
            current_macro.clear();
            continue;
        }

        if (in_macro) {
            macro_buffer.push_back(tk);
            continue;
        }
    }
    if (in_macro) throw runtime_error("Unclosed macro definition: " + current_macro);

    function<vector<string>(const vector<string>&, vector<string>&)> expand =
    [&](const vector<string>& lst, vector<string>& call_stack) {
        vector<string> out;
        for (const string& tk : lst) {
            if (is_valid_string_literal(tk)) {
                out.push_back(tk);
                continue;
            }

            if (regex_match(tk, regex(R"(^:>_\[.+\]$)"))) {
                string name = tk.substr(4, tk.size() - 5);
                if (!macros.count(name)) throw runtime_error("Undefined macro: " + name);
                if (find(call_stack.begin(), call_stack.end(), name) != call_stack.end())
                    throw runtime_error("Recursive macro call: " + name);
                call_stack.push_back(name);
                vector<string> sub = expand(macros[name], call_stack);
                call_stack.pop_back();
                out.insert(out.end(), sub.begin(), sub.end());
                continue;
            }

            if (tk.rfind(":>_", 0) == 0) {
                throw runtime_error("Deprecated macro call: '" + tk + "' — use :>_[name] in v2.3+.");
            }

            if (regex_match(tk, regex(R"(^:-?\d+_.*$)"))) {
                int count = stoi(tk.substr(1, tk.find('_') - 1));
                if (count < 0) throw runtime_error("Negative repeat count in: " + tk);
                string subtk = tk.substr(tk.find('_') + 1);
                for (int i = 0; i < count; ++i) {
                    vector<string> rec = expand({subtk}, call_stack);
                    out.insert(out.end(), rec.begin(), rec.end());
                }
                continue;
            }

            out.push_back(tk);
        }
        return out;
    };

    vector<string> result;
    bool skipping = false;
    for (const string& tk : tokens) {
        if (is_valid_string_literal(tk)) {
            result.push_back(tk);
            continue;
        }

        if (!skipping && regex_match(tk, regex(R"(^:O_\[.+\]$)"))) {
            skipping = true;
            continue;
        }
        if (skipping && tk == ":|") {
            skipping = false;
            continue;
        }
        if (skipping) continue;

        vector<string> stack;
        vector<string> expanded = expand({tk}, stack);
        result.insert(result.end(), expanded.begin(), expanded.end());
    }

    return result;
}

string parse_escaped_string_for_c(const string& raw) {
    stringstream result;

    for (size_t i = 0; i < raw.size(); ++i) {
        unsigned char c = raw[i];
        if (c == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            switch (next) {
                case 'n': result << "\\n"; break;
                case 't': result << "\\t"; break;
                case 'r': result << "\\r"; break;
                case '0': result << "\\0"; break;
                case '\\': result << "\\\\"; break;
                case '"': result << "\\\""; break;
                default:
                    result << "\\" << next;
                    break;
            }
            ++i;
        } else if (c == '\n') {
            result << "\\n";
        } else if (c == '\r') {
            result << "\\r";
        } else if (c == '\t') {
            result << "\\t";
        } else if (c == '\"') {
            result << "\\\"";
        } else if (isprint(c)) {
            result << c;
        } else {
            result << "\\x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)c;
        }
    }

    return result.str();
}

string translate_command(const string& cmd, const string& source) {
    if (cmd == ":)") return "++tape[ptr];";
    if (cmd == ":(") return "--tape[ptr];";
    if (cmd == ":D") return "++ptr; if (ptr >= size) { size++; tape = (unsigned char*)realloc(tape, size); tape[ptr] = 0; }";
    if (cmd == "D:") return "if (ptr > 0) --ptr;";
    if (cmd == ":P") return "putchar(tape[ptr]);";
    if (cmd == ":()") return "int c = getchar(); tape[ptr] = c == EOF ? 0 : (unsigned char)c;";
    if (cmd == ":F") return "fflush(stdout);";
    if (cmd == "O_O") return "while (tape[ptr]) {";
    if (cmd == "-_-") return "}";
    if (cmd == ":q") {
        stringstream ss;
        string esc = parse_escaped_string_for_c(source);
        ss << "{ size_t orig = ptr; const char* src = \"" << esc << "\";"
           << " for (size_t i=0; src[i]; ++i) { if(ptr>=size){size++; tape=(unsigned char*)realloc(tape,size);} tape[ptr++] = (unsigned char)src[i]; } ptr=orig;}";
        return ss.str();
    }
    if (cmd.rfind(":S_\"", 0) == 0 && cmd.back() == '"') {
        string raw = cmd.substr(4, cmd.size() - 5);
        string escaped = parse_escaped_string_for_c(raw);
        stringstream code;
        code << "{ size_t original_ptr = ptr; const char* str = \"" << escaped << "\";"
             << " for (size_t i = 0; str[i]; ++i) { if(ptr>=size){size++; tape=(unsigned char*)realloc(tape,size);} tape[ptr++] = (unsigned char)str[i]; } ptr = original_ptr;}";
        return code.str();
    }
    return "// Unknown: " + cmd;
}

string generate_c_code(const vector<string>& code, const string& source) {
    stringstream out;
    out << "#include <stdio.h>\n#include <stdlib.h>\n\nint main() {\n";
    out << "    size_t size = 1, ptr = 0;\n";
    out << "    unsigned char* tape = calloc(size, 1);\n";
    for (const string& cmd : code) {
        out << "    " << translate_command(cmd, source) << "\n";
    }
    out << "    free(tape);\n";
    out << "    return 0;\n}";
    return out.str();
}

string random_suffix(int length = 6) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    string result;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    for (int i = 0; i < length; ++i)
        result += charset[dist(gen)];
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        cerr << "Usage: transpiler <file.O_O> [-k|--keep] or transpiler --version" << endl;
        return 1;
    }

    string arg1 = argv[1];
    bool keep_files = false;

    if (arg1 == "--version") {
        cout << "Emoticode compiler version 2.4.1 :)" << endl;
        return 0;
    }

    if (argc == 3) {
        string flag = argv[2];
        if (flag == "-k" || flag == "--keep") {
            keep_files = true;
        } else {
            cerr << "Unknown option: " << flag << "\n";
            return 1;
        }
    }

    string input_path = arg1;
    if (input_path.size() < 5 || input_path.substr(input_path.size() - 4) != ".O_O") {
        cerr << "Error: Input file must end in .O_O\n";
        return 1;
    }

    ifstream in(input_path);
    if (!in) {
        cerr << "Error: Cannot open source file. Are you sure it exists?\n";
        return 1;
    }

    stringstream buffer;
    buffer << in.rdbuf();
    string source = buffer.str();

    try {
        auto tokens = tokenize(source);
        auto code = preprocess(tokens);
        string c_code = generate_c_code(code, source);

        fs::path base_path = fs::path(input_path).parent_path();
        string stem = fs::path(input_path).stem().string();

        fs::path c_file = keep_files
            ? base_path / (stem + ".c")
            : fs::temp_directory_path() / ("emoticode_" + random_suffix() + "/transpiled.c");

        if (!keep_files) fs::create_directories(c_file.parent_path());

        ofstream out(c_file);
        out << c_code;
        out.close();

        fs::path exe_path = keep_files
            ? base_path / (stem + ".exe")
            : c_file.parent_path() / "a.out";

        string compile_cmd = "gcc \"" + c_file.string() + "\" -o \"" + exe_path.string() + "\"";
        int compile_result = system(compile_cmd.c_str());

        if (compile_result != 0) {
            cerr << "Compilation failed.\n";
            if (!keep_files) fs::remove_all(c_file.parent_path());
            return 1;
        }

        if (!keep_files) {
            fs::path final_exe = base_path / (stem + ".exe");
            fs::copy_file(exe_path, final_exe, fs::copy_options::overwrite_existing);
            fs::remove_all(c_file.parent_path());
            cout << "Compiled successfully to: " << final_exe << "\n";
        } else {
            cout << "Compiled successfully to: " << exe_path << "\n";
            cout << "C source stored at: " << c_file << "\n";
        }

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
