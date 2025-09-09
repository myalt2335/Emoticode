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
#include <stdexcept>

using namespace std;

static string output_buffer;
static string argv_input;
static size_t argv_input_pos = 0;

static bool starts_with(const string &s, size_t i, const string &pat) {
    return s.size() >= i + pat.size() && s.compare(i, pat.size(), pat) == 0;
}

bool is_valid_string_literal(const string &tk) {
    if (tk.rfind(":S_\"", 0) != 0) return false;
    if (tk.size() < 5) return false;
    if (tk.back() != '"') return false;
    size_t i = tk.size() - 1, bs = 0;
    while (i > 0 && tk[i - 1] == '\\') { --i; ++bs; }
    return (bs % 2 == 0);
}

static string parse_string_literal(const string &src, size_t &i) {
    size_t st = i;
    i += 4;
    bool esc = false;
    while (i < src.size()) {
        char c = src[i++];
        if (c == '\\' && !esc) esc = true;
        else if (c == '"' && !esc) break;
        else esc = false;
    }
    return src.substr(st, i - st);
}

static string parse_bracketed(const string &src, size_t &i) {
    if (i >= src.size() || src[i] != '[') return string();
    size_t st = i;
    int depth = 0;
    while (i < src.size()) {
        if (src[i] == '[') ++depth;
        else if (src[i] == ']') { --depth; ++i; break; }
        ++i;
    }
    return src.substr(st, i - st);
}

static string parse_colon_token_simple(const string &src, size_t &i) {
    size_t st = i;
    ++i;
    if (i >= src.size()) return src.substr(st, 1);

    if (src[i] == '[' || src[i] == ':') {
        return src.substr(st, 1);
    }

    if (isspace((unsigned char)src[i])) {
        size_t j = i;
        while (j < src.size() && isspace((unsigned char)src[j])) ++j;
        if (j < src.size() && (isdigit((unsigned char)src[j]) || src[j] == '+' || src[j] == '-' ||
                               src[j] == '>' || src[j] == 'p')) {
            size_t k = j;
            while (k < src.size() && !isspace((unsigned char)src[k])) ++k;
            i = k;
            return src.substr(st, i - st);
        }
        return src.substr(st, 1);
    }

    ++i;
    return src.substr(st, i - st);
}

static const vector<string> orderedColons = {
    ":()", ":)", ":(", ":P", ":D", ":q", ":|"
};

static string parse_repeat_token(const string &src, size_t &i);

static string parse_token_at(const string &src, size_t &i) {
    if (i >= src.size()) return string();
    if (src[i] == ':') {
        if (starts_with(src, i, ":S_\"")) return parse_string_literal(src, i);
        if (starts_with(src, i, ":O_[") || starts_with(src, i, ":>_[")) {
            size_t st = i;
            while (i < src.size() && src[i] != '[') ++i;
            if (i < src.size() && src[i] == '[') {
                string br = parse_bracketed(src, i);
                (void)br;
            }
            return src.substr(st, i - st);
        }
        if (i + 1 < src.size() && (src[i + 1] == '-' || isdigit((unsigned char)src[i + 1]))) {
            size_t saved = i;
            string rpt = parse_repeat_token(src, i);
            if (!rpt.empty()) return rpt;
            i = saved;
        }
        for (const auto &pat : orderedColons) {
            if (starts_with(src, i, pat)) {
                size_t st = i;
                i += pat.size();
                return src.substr(st, pat.size());
            }
        }
        return parse_colon_token_simple(src, i);
    }
    if (starts_with(src, i, "F_F")) { i += 3; return "F_F"; }
    if (starts_with(src, i, "O_O")) { i += 3; return "O_O"; }
    if (starts_with(src, i, "-_-")) { i += 3; return "-_-"; }
    if (starts_with(src, i, "D:"))  { i += 2; return "D:"; }
    size_t st = i;
    while (i < src.size() && !isspace((unsigned char)src[i])) ++i;
    return src.substr(st, i - st);
}

static string parse_repeat_token(const string &src, size_t &i) {
    size_t st = i;
    ++i;
    bool negative = false;
    if (i < src.size() && src[i] == '-') { negative = true; ++i; }
    if (i >= src.size() || !isdigit((unsigned char)src[i])) { i = st; return string(); }
    string digits;
    while (i < src.size() && isdigit((unsigned char)src[i])) { digits.push_back(src[i++]); }
    if (digits.empty()) { i = st; return string(); }
    if (i >= src.size() || src[i] != '_') { i = st; return string(); }
    ++i;
    string sub = parse_token_at(src, i);
    if (sub.empty()) { i = st; return string(); }
    string tok = ":";
    if (negative) tok.push_back('-');
    tok += digits;
    tok.push_back('_');
    tok += sub;
    return tok;
}

vector<string> tokenize(const string &source) {
    vector<string> t;
    size_t i = 0;
    while (i < source.size()) {
        if (isspace((unsigned char)source[i])) { ++i; continue; }
        if (starts_with(source, i, ":C")) {
            i += 2;
            while (i + 1 < source.size()) {
                if (source[i] == ':' && source[i + 1] == 'l') { i += 2; break; }
                ++i;
            }
            continue;
        }
        string tk = parse_token_at(source, i);
        if (!tk.empty()) t.push_back(tk);
        else ++i;
    }
    return t;
}

vector<string> preprocess(const vector<string> &tokens) {
    unordered_map<string, vector<string>> macros;
    bool in_macro = false;
    string current_macro;
    vector<string> macro_buffer;

    for (const auto &tk : tokens) {
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
        if (in_macro && regex_match(tk, regex(R"(^:O_\[.+\]$)")))
            throw runtime_error("Nested macros: " + tk);
        if (tk.rfind(":O_", 0) == 0)
            throw runtime_error("Deprecated macro syntax: '" + tk + "'");
        if (in_macro && tk == ":|") {
            in_macro = false;
            macros[current_macro] = macro_buffer;
            current_macro.clear();
            continue;
        }
        if (in_macro) { macro_buffer.push_back(tk); continue; }
    }
    if (in_macro) throw runtime_error("Unclosed macro: " + current_macro);

    function<vector<string>(const vector<string> &, vector<string> &)> expand =
        [&](const vector<string> &lst, vector<string> &stack) -> vector<string> {
            vector<string> out;
            for (auto &tk : lst) {
                if (is_valid_string_literal(tk)) { out.push_back(tk); continue; }
                if (regex_match(tk, regex(R"(^:>_\[.+\]$)"))) {
                    string name = tk.substr(4, tk.size() - 5);
                    if (!macros.count(name)) throw runtime_error("Undefined macro: " + name);
                    if (find(stack.begin(), stack.end(), name) != stack.end())
                        throw runtime_error("Recursive macro: " + name);
                    stack.push_back(name);
                    auto sub = expand(macros[name], stack);
                    stack.pop_back();
                    out.insert(out.end(), sub.begin(), sub.end());
                    continue;
                }
                if (tk.rfind(":>_", 0) == 0)
                    throw runtime_error("Deprecated macro call: '" + tk + "'");
                if (regex_match(tk, regex(R"(^:-?\d+_.*$)"))) {
                    int count = stoi(tk.substr(1, tk.find('_') - 1));
                    if (count < 0) throw runtime_error("Negative repeat count in: " + tk);
                    string subtk = tk.substr(tk.find('_') + 1);
                    for (int i = 0; i < count; ++i) {
                        auto rec = expand(vector<string>{subtk}, stack);
                        out.insert(out.end(), rec.begin(), rec.end());
                    }
                    continue;
                }
                out.push_back(tk);
            }
            return out;
        };

    vector<string> res;
    bool skipping = false;
    for (auto &tk : tokens) {
        if (is_valid_string_literal(tk)) { res.push_back(tk); continue; }
        if (!skipping && regex_match(tk, regex(R"(^:O_\[.+\]$)"))) { skipping = true; continue; }
        if (skipping && tk == ":|") { skipping = false; continue; }
        if (skipping) continue;
        vector<string> st;
        auto e = expand(vector<string>{tk}, st);
        res.insert(res.end(), e.begin(), e.end());
    }
    return res;
}

static int my_getchar(void) {
    if (!argv_input.empty()) {
        if (argv_input_pos < argv_input.size())
            return (unsigned char)argv_input[argv_input_pos++];
        return EOF;
    }
    return getchar();
}

string parse_escaped_string(const string &raw) {
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
        } else result += raw[i];
    }
    return result;
}

enum OpCode {
    OP_INC_PTR, OP_DEC_PTR,
    OP_INC_VAL, OP_DEC_VAL,
    OP_ADD_VAL, OP_ADD_PTR,
    OP_PRINT, OP_FLUSH,
    OP_STORE_STRING,
    OP_INPUT, OP_QUOTE_SOURCE,
    OP_JUMP_FWD, OP_JUMP_BACK,
    OP_CLEAR_CELL
};


struct Instruction {
    OpCode op;
    int    arg = 0;
    string data;
};

static Instruction ins_simple(OpCode op) { return Instruction{op, 0, {}}; }

vector<Instruction> compile_to_bytecode_raw(const vector<string>& code) {
    vector<Instruction> program;
    program.reserve(code.size());

    for (const string& cmd : code) {
        if (cmd == ":D") program.push_back(ins_simple(OP_INC_PTR));
        else if (cmd == "D:") program.push_back(ins_simple(OP_DEC_PTR));
        else if (cmd == ":)") program.push_back(ins_simple(OP_INC_VAL));
        else if (cmd == ":(") program.push_back(ins_simple(OP_DEC_VAL));
        else if (cmd.rfind(":+ ", 0) == 0) { Instruction i{OP_ADD_VAL, stoi(cmd.substr(3))}; program.push_back(i); }
        else if (cmd.rfind(":- ", 0) == 0) { Instruction i{OP_ADD_VAL, -stoi(cmd.substr(3))}; program.push_back(i); }
        else if (cmd.rfind(":>p+ ", 0) == 0) { Instruction i{OP_ADD_PTR, stoi(cmd.substr(5))}; program.push_back(i); }
        else if (cmd.rfind(":>p- ", 0) == 0) { Instruction i{OP_ADD_PTR, -stoi(cmd.substr(5))}; program.push_back(i); }
        else if (cmd == ":P") program.push_back(ins_simple(OP_PRINT));
        else if (cmd == "F_F") program.push_back(ins_simple(OP_FLUSH));
        else if (cmd.rfind(":S_\"", 0) == 0 && cmd.back() == '"') {
            Instruction i{OP_STORE_STRING, 0, parse_escaped_string(cmd.substr(4, cmd.size() - 5))};
            program.push_back(i);
        }
        else if (cmd == ":()") program.push_back(ins_simple(OP_INPUT));
        else if (cmd == ":q") program.push_back(ins_simple(OP_QUOTE_SOURCE));
        else if (cmd == "O_O") program.push_back(ins_simple(OP_JUMP_FWD));
        else if (cmd == "-_-") program.push_back(ins_simple(OP_JUMP_BACK));
        else {
            throw runtime_error("Unknown token: " + cmd);
        }
    }
    return program;
}

bool is_val_add(const Instruction& ins) {
    return ins.op == OP_INC_VAL || ins.op == OP_DEC_VAL || ins.op == OP_ADD_VAL;
}
int val_add_delta(const Instruction& ins) {
    switch (ins.op) {
        case OP_INC_VAL: return 1;
        case OP_DEC_VAL: return -1;
        case OP_ADD_VAL: return ins.arg;
        default: return 0;
    }
}

bool is_ptr_add_clamp(const Instruction& ins) {
    return ins.op == OP_ADD_PTR;
}
bool is_ptr_inc(const Instruction& ins) {
    return ins.op == OP_INC_PTR;
}

bool is_barrier(const Instruction& ins) {
    return ins.op == OP_JUMP_FWD || ins.op == OP_JUMP_BACK ||
           ins.op == OP_INPUT || ins.op == OP_PRINT || ins.op == OP_FLUSH ||
           ins.op == OP_STORE_STRING || ins.op == OP_QUOTE_SOURCE ||
           ins.op == OP_DEC_PTR || ins.op == OP_CLEAR_CELL;
}

vector<Instruction> peephole_optimize(const vector<Instruction>& in) {
    vector<Instruction> out;
    out.reserve(in.size());

    for (size_t i = 0; i < in.size(); ) {
        const Instruction& cur = in[i];

        if (cur.op == OP_JUMP_FWD) {
            if (i + 2 < in.size() && in[i + 1].op == OP_DEC_VAL && in[i + 2].op == OP_JUMP_BACK) {L
                out.push_back(ins_simple(OP_CLEAR_CELL));
                i += 3;
                continue;
            }
        }

        if (is_val_add(cur)) {
            int delta = 0;
            size_t j = i;
            while (j < in.size() && is_val_add(in[j]) && !is_barrier(in[j])) {
                delta += val_add_delta(in[j]);
                ++j;
            }
            if (delta != 0) {
                Instruction folded{OP_ADD_VAL, delta, {}};
                out.push_back(folded);
            }
            i = j;
            continue;
        }

        if (is_ptr_inc(cur)) {
            int k = 0;
            size_t j = i;
            while (j < in.size() && is_ptr_inc(in[j]) && !is_barrier(in[j])) {
                ++k; ++j;
            }
            if (k != 0) {
                Instruction folded{OP_ADD_PTR, k, {}};
                out.push_back(folded);
            }
            i = j;
            continue;
        }

        if (is_ptr_add_clamp(cur)) {
            long long sum = 0;
            size_t j = i;
            while (j < in.size() && is_ptr_add_clamp(in[j]) && !is_barrier(in[j])) {
                sum += in[j].arg;
                ++j;
            }
            if (sum != 0) {
                Instruction folded{OP_ADD_PTR, (int)sum, {}};
                out.push_back(folded);
            }
            i = j;
            continue;
        }

        if ((cur.op == OP_ADD_PTR || cur.op == OP_ADD_VAL) && cur.arg == 0) {
            ++i;
            continue;
        }

        out.push_back(cur);
        ++i;
    }

    return out;
}

void patch_jumps(vector<Instruction>& prog) {
    vector<size_t> stack;
    for (size_t i = 0; i < prog.size(); ++i) {
        if (prog[i].op == OP_JUMP_FWD) {
            stack.push_back(i);
        } else if (prog[i].op == OP_JUMP_BACK) {
            if (stack.empty()) throw runtime_error("Unmatched -_- after optimization");
            size_t start = stack.back(); stack.pop_back();
            prog[start].arg = (int)i;
            prog[i].arg = (int)start;
        }
    }
    if (!stack.empty()) throw runtime_error("Unmatched O_O after optimization");
}

void run_bytecode(const vector<Instruction>& program, const string& source) {
    vector<unsigned char> tape(1, 0);
    size_t ptr = 0;
    output_buffer.clear();

    size_t step_count = 0;
    const size_t max_steps = 1000000;
    bool had_output = false;

    for (size_t pc = 0; pc < program.size(); ++pc) {
        if (++step_count > max_steps) {
            if (!had_output) {
                cerr << "[Runtime Warning] Auto-breaking suspected infinite loop at PC "
                     << pc << " after " << step_count << " iterations." << endl;
            }
            break;
        }

        const Instruction& ins = program[pc];

        switch (ins.op) {
            case OP_INC_PTR:
                ++ptr;
                if (ptr >= tape.size()) tape.resize(ptr + 1, 0);
                break;

            case OP_DEC_PTR:
                if (ptr == 0) throw runtime_error("Pointer out of bounds (left).");
                --ptr;
                break;

            case OP_ADD_PTR: {
                if (ins.arg >= 0) {
                    ptr += (size_t)ins.arg;
                    if (ptr >= tape.size()) tape.resize(ptr + 1, 0);
                } else {
                    int n = -ins.arg;
                    if (ptr >= (size_t)n) ptr -= (size_t)n;
                    else ptr = 0;
                }
                break;
            }

            case OP_INC_VAL:
                ++tape[ptr];
                break;

            case OP_DEC_VAL:
                --tape[ptr];
                break;

            case OP_ADD_VAL: {
                int v = (int)tape[ptr];
                v += ins.arg;
                tape[ptr] = static_cast<unsigned char>(v);
                break;
            }

            case OP_PRINT: {
                char ch = (char)tape[ptr];
                output_buffer += ch;
                had_output = true;
                if (ch == '\n') {
                    cout << output_buffer;
                    cout.flush();
                    output_buffer.clear();
                }
                break;
            }

            case OP_FLUSH:
                if (!output_buffer.empty()) {
                    cout << output_buffer;
                    cout.flush();
                    output_buffer.clear();
                    had_output = true;
                }
                break;

            case OP_STORE_STRING: {
                size_t original_ptr = ptr;
                for (char ch : ins.data) {
                    if (ptr >= tape.size()) tape.resize(ptr + 1, 0);
                    tape[ptr] = static_cast<unsigned char>(ch);
                    ++ptr;
                }
                ptr = original_ptr;
                break;
            }

            case OP_INPUT: {
                int c = my_getchar();
                tape[ptr] = (c == EOF) ? 0 : static_cast<unsigned char>(c);
                break;
            }

            case OP_QUOTE_SOURCE: {
                size_t original_ptr = ptr;
                for (char ch : source) {
                    if (ptr >= tape.size()) tape.resize(ptr + 1, 0);
                    tape[ptr] = static_cast<unsigned char>(ch);
                    ++ptr;
                }
                ptr = original_ptr;
                break;
            }

            case OP_JUMP_FWD:
                if (tape[ptr] == 0) pc = (size_t)ins.arg;
                break;

            case OP_JUMP_BACK:
                if (tape[ptr] != 0) pc = (size_t)ins.arg;
                break;

            case OP_CLEAR_CELL:
                tape[ptr] = 0;
                break;
        }
    }

    if (!output_buffer.empty()) {
        cout << output_buffer;
        cout.flush();
        output_buffer.clear();
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: ./emotinterpret <file.O_O> [args...] or ./emotinterpret --version" << endl;
        return 1;
    }
    string arg = argv[1];
    if (arg == "--version") {
        cout << "Emoticode interpreter version 2.7.1 :)" << endl;
        return 0;
    }
    string filename = argv[1];
    if (filename.size() < 5 || filename.substr(filename.size() - 4) != ".O_O") {
        cerr << "Error: File must have .O_O extension." << endl;
        return 1;
    }
    if (argc > 2) {
        stringstream ss;
        for (int i = 2; i < argc; ++i) {
            if (i > 2) ss << " ";
            ss << argv[i];
        }
        argv_input = ss.str();
        argv_input_pos = 0;
    }

    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "Error: Cannot open file. Are you sure it exists?" << endl;
        return 1;
    }
    stringstream buffer;
    buffer << file.rdbuf();
    string source = buffer.str();
    source.erase(std::remove(source.begin(), source.end(), '\r'), source.end());

    try {
        vector<string> tokens = tokenize(source);
        vector<string> expanded = preprocess(tokens);

        auto raw = compile_to_bytecode_raw(expanded);

        auto opt = peephole_optimize(raw);

        patch_jumps(opt);

        run_bytecode(opt, source);
    } catch (const exception &e) {
        cerr << "[Runtime Error] " << e.what() << endl;
        return 1;
    }

    return 0;
}
