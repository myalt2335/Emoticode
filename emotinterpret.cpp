#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <string>
#include <regex>
#include <algorithm>
#include <functional>
#include <cctype>

using namespace std;

std::string output_buffer;

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

string parse_escaped_string(const string& raw) {
    string result;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            switch (next) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '0': result += '\0'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += next; break;
            }
            ++i;
        } else {
            result += raw[i];
        }
    }
    return result;
}

void run_emoticode(const vector<string>& code, const string& source) {
    vector<unsigned char> tape(1, 0);
    size_t ptr = 0;

    vector<size_t> loop_stack;
    unordered_map<size_t, size_t> jump_map;

    for (size_t i = 0; i < code.size(); ++i) {
        if (code[i] == "O_O") loop_stack.push_back(i);
        else if (code[i] == "-_-") {
            if (loop_stack.empty()) throw runtime_error("Unmatched -_-");
            size_t start = loop_stack.back();
            loop_stack.pop_back();
            jump_map[start] = i;
            jump_map[i] = start;
        }
    }
    if (!loop_stack.empty()) throw runtime_error("Unmatched O_O");

    for (size_t pc = 0; pc < code.size(); ++pc) {
        const string& cmd = code[pc];

        if (cmd == ":D") {
            ++ptr;
            if (ptr >= tape.size()) tape.resize(ptr + 1, 0);
        }
        else if (cmd == "D:") {
            if (ptr == 0) throw runtime_error("Pointer out of bounds (left).");
            --ptr;
        }
        else if (cmd == ":)") ++tape[ptr];
        else if (cmd == ":(") --tape[ptr];
        else if (cmd == ":P") {
            char ch = tape[ptr];
            output_buffer += ch;
            if (ch == '\n') {
                cout << output_buffer;
                cout.flush();
                output_buffer.clear();
            }
        }
        else if (cmd == ":F") {
            cout << output_buffer;
            cout.flush();
            output_buffer.clear();
        }
        else if (cmd.rfind(":S_\"", 0) == 0 && cmd.back() == '"') {
            string raw_str = cmd.substr(4, cmd.size() - 5);
            string parsed = parse_escaped_string(raw_str);
            size_t original_ptr = ptr;
            for (char ch : parsed) {
                if (ptr >= tape.size()) tape.resize(ptr + 1, 0);
                tape[ptr] = static_cast<unsigned char>(ch);
                ++ptr;
            }
            ptr = original_ptr;
        }
        else if (cmd == ":()") {
            int c = getchar();
            if (c == EOF) c = 0;
            tape[ptr] = static_cast<unsigned char>(c);
        }
        else if (cmd == ":q") {
            size_t original_ptr = ptr;
            for (char ch : source) {
                if (ptr >= tape.size()) tape.resize(ptr + 1, 0);
                tape[ptr] = static_cast<unsigned char>(ch);
                ++ptr;
            }
            ptr = original_ptr;
        }
        else if (cmd == "O_O") {
            if (tape[ptr] == 0) pc = jump_map[pc];
        }
        else if (cmd == "-_-") {
            if (tape[ptr] != 0) pc = jump_map[pc];
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: ./emotinterpret <file.O_O> or ./emotinterpret --version" << endl;
        return 1;
    }

    string arg = argv[1];
    if (arg == "--version") {
        cout << "Emoticode interpreter version 2.4.1 :)" << endl;
        return 0;
    }

    string filename = argv[1];
    if (filename.size() < 5 || filename.substr(filename.size() - 4) != ".O_O") {
        cerr << "Error: File must have .O_O extension." << endl;
        return 1;
    }

    ifstream file(filename);
    if (!file) {
        cerr << "Error: Cannot open file. Are you sure it exists?" << endl;
        return 1;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string source = buffer.str();

    try {
        vector<string> tokens = tokenize(source);
        vector<string> code = preprocess(tokens);
        run_emoticode(code, source);
    } catch (const exception& e) {
        cerr << "[Runtime Error] " << e.what() << endl;
        return 1;
    }

    if (!output_buffer.empty()) {
        cout << output_buffer;
        cout.flush();
        output_buffer.clear();
    }

    return 0;
}
