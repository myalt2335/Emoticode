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
#include <cstring>

using namespace std;
namespace fs = std::filesystem;

bool is_valid_string_literal(const string& tk){
    if(tk.rfind(":S_\"",0)!=0) return false;
    if(tk.size()<5) return false;
    if(tk.back()!='"') return false;
    size_t i=tk.size()-1, bs=0;
    while(i>0 && tk[i-1]=='\\'){ --i; ++bs; }
    return (bs%2==0);
}

static bool starts_with(const string &s, size_t i, const string &pat){
    return s.size() >= i + pat.size() && s.compare(i, pat.size(), pat) == 0;
}

static string parse_string_literal(const string &src, size_t &i){
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

static string parse_bracketed(const string &src, size_t &i){
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

static string parse_token_at(const string &src, size_t &i);

static string parse_repeat_token(const string &src, size_t &i){
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

static string parse_colon_token_simple(const string &src, size_t &i){
    size_t st = i;
    ++i;
    if (i >= src.size()) return src.substr(st, 1);
    if (isspace((unsigned char)src[i]) || src[i] == '[' || src[i] == ':') {
        return src.substr(st, 1);
    }
    ++i;
    return src.substr(st, i - st);
}

static const vector<string> orderedColons = {
    ":()", ":)", ":(", ":P", ":D", ":q", ":|"
};

static string parse_token_at(const string &src, size_t &i){
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
        if (i + 1 < src.size() && (src[i+1] == '-' || isdigit((unsigned char)src[i+1]))) {
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

vector<string> tokenize(const string& source){
    vector<string> t;
    size_t i=0;
    while(i<source.size()){
        if(isspace((unsigned char)source[i])){ ++i; continue; }
        if (starts_with(source, i, ":C")) {
            i += 2;
            while (i + 1 < source.size()) {
                if (source[i] == ':' && source[i+1] == 'l') { i += 2; break; }
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

vector<string> preprocess(const vector<string>&tokens){
    unordered_map<string,vector<string>> mac;
    bool inmac=false;
    string cur;
    vector<string> mb;
    for(auto&tk:tokens){
        if(is_valid_string_literal(tk)){
            if(inmac) mb.push_back(tk);
            continue;
        }
        if(!inmac && regex_match(tk,regex(R"(^:O_\[.+\]$)"))){
            inmac=true;
            cur=tk.substr(4,tk.size()-5);
            mb.clear();
            continue;
        }
        if(inmac && regex_match(tk,regex(R"(^:O_\[.+\]$)"))) throw runtime_error("Nested macros: "+tk);
        if(tk.rfind(":O_",0)==0) throw runtime_error("Deprecated macro syntax: '"+tk+"'");
        if(inmac && tk==":|"){
            inmac=false;
            mac[cur]=mb;
            cur.clear();
            continue;
        }
        if(inmac){
            mb.push_back(tk);
            continue;
        }
    }
    if(inmac) throw runtime_error("Unclosed macro: "+cur);

    function<vector<string>(const vector<string>&,vector<string>&)> expand = [&](const vector<string>&lst, vector<string>&stack)->vector<string>{
        vector<string> out;
        for(auto&tk:lst){
            if(is_valid_string_literal(tk)){
                out.push_back(tk);
                continue;
            }
            if(regex_match(tk,regex(R"(^:>_\[.+\]$)"))){
                string name=tk.substr(4,tk.size()-5);
                if(!mac.count(name)) throw runtime_error("Undefined macro: "+name);
                if(find(stack.begin(),stack.end(),name)!=stack.end()) throw runtime_error("Recursive macro: "+name);
                stack.push_back(name);
                auto sub=expand(mac[name],stack);
                stack.pop_back();
                out.insert(out.end(),sub.begin(),sub.end());
                continue;
            }
            if(tk.rfind(":>_",0)==0) throw runtime_error("Deprecated macro call: '"+tk+"'");
            if(regex_match(tk,regex(R"(^:-?\d+_.*$)"))){
                int count=stoi(tk.substr(1,tk.find('_')-1));
                if(count<0) throw runtime_error("Negative repeat count in: "+tk);
                string subtk=tk.substr(tk.find('_')+1);
                for(int i=0;i<count;++i){
                    auto rec=expand(vector<string>{subtk},stack);
                    out.insert(out.end(),rec.begin(),rec.end());
                }
                continue;
            }
            out.push_back(tk);
        }
        return out;
    };

    vector<string> res;
    bool skipping=false;
    for(auto&tk:tokens){
        if(is_valid_string_literal(tk)){
            res.push_back(tk);
            continue;
        }
        if(!skipping && regex_match(tk,regex(R"(^:O_\[.+\]$)"))){
            skipping=true;
            continue;
        }
        if(skipping && tk==":|"){
            skipping=false;
            continue;
        }
        if(skipping) continue;
        vector<string> st;
        auto e=expand(vector<string>{tk},st);
        res.insert(res.end(),e.begin(),e.end());
    }
    return res;
}

string escape_for_c_string(const string& raw){
    stringstream ss;
    for(size_t i=0;i<raw.size();++i){
        unsigned char c=raw[i];
        if(c=='\\'&& i+1<raw.size()){
            char n=raw[++i];
            switch(n){
                case 'n': ss<<"\\n"; break;
                case 't': ss<<"\\t"; break;
                case 'r': ss<<"\\r"; break;
                case '0': ss<<"\\0"; break;
                case '\\': ss<<"\\\\"; break;
                case '"': ss<<"\\\""; break;
                default: ss<<'\\'<<n; break;
            }
        } else if(c=='\n') ss<<"\\n";
        else if(c=='\r') ss<<"\\r";
        else if(c=='\t') ss<<"\\t";
        else if(c=='"') ss<<"\\\"";
        else if(isprint(c)) ss<<c;
        else {
            stringstream h;
            h<<uppercase<<hex<<setw(2)<<setfill('0')<<(int)c;
            ss<<"\\x"<<h.str();
            ss<<dec;
        }
    }
    return ss.str();
}

vector<string> optimize_simple(const vector<string>& code){
    vector<string> out;
    for(size_t i=0;i<code.size();){
        if(code[i]==":)"||code[i]==":("){
            int d=0;
            while(i<code.size()&&(code[i]==":)"||code[i]==":(")){
                d += (code[i]==":)")?1:-1;
                ++i;
            }
            if(d>0) out.push_back(":+ "+to_string(d));
            else out.push_back(":- "+to_string(-d));
            continue;
        }
        if(code[i]==":D"||code[i]=="D:"){
            int d=0;
            while(i<code.size()&&(code[i]==":D"||code[i]=="D:")){
                d += (code[i]==":D")?1:-1;
                ++i;
            }
            if(d>0) out.push_back(":>p+ "+to_string(d));
            else out.push_back(":>p- "+to_string(-d));
            continue;
        }
        out.push_back(code[i++]);
    }
    return out;
}

string translate_token(const string& tk, const string& source){
    if(tk==":P") return "putchar(tape[ptr]);\n";
    if(tk=="F_F") return "fflush(stdout);\n";
    if(tk=="O_O") return "while(tape[ptr]){\n";
    if(tk=="-_-") return "}\n";
    if(tk==":()") return "int ch=my_getchar(); tape[ptr] = ch==EOF?0:(unsigned char)ch;\n";
    if(tk.rfind(":+ ",0)==0){ int n=stoi(tk.substr(3)); stringstream s; s<<"tape[ptr] += "<<n<<";\n"; return s.str(); }
    if(tk.rfind(":- ",0)==0){ int n=stoi(tk.substr(3)); stringstream s; s<<"tape[ptr] -= "<<n<<";\n"; return s.str(); }
    if(tk.rfind(":>p+ ",0)==0){ int n=stoi(tk.substr(5)); stringstream s; s<<"ptr += "<<n<<"; ENSURE_CAP(ptr);\n"; return s.str(); }
    if(tk.rfind(":>p- ",0)==0){ int n=stoi(tk.substr(5)); stringstream s; s<<"if (ptr >= "<<n<<") ptr -= "<<n<<"; else ptr = 0;\n"; return s.str(); }
    if(tk==":q"){ string e=escape_for_c_string(source); stringstream s; s<<"{ size_t o=ptr; const char* src = \""<<e<<"\"; size_t len = strlen(src); if(len) { size_t need = ptr + len - 1; ENSURE_CAP(need); for(size_t i=0;i<len;++i) tape[ptr++] = (unsigned char)src[i]; } ptr=o; }\n"; return s.str(); }
    if(tk.rfind(":S_\"",0)==0 && tk.back()=='\"'){ string raw=tk.substr(4,tk.size()-5); string e=escape_for_c_string(raw); stringstream s; s<<"{ size_t o=ptr; const char* str = \""<<e<<"\"; size_t len = strlen(str); if(len){ size_t need = ptr + len - 1; ENSURE_CAP(need); for(size_t i=0;i<len;++i) tape[ptr++] = (unsigned char)str[i]; } ptr=o; }\n"; return s.str(); }
    if(tk==":)") return "++tape[ptr];\n";
    if(tk==":(") return "--tape[ptr];\n";
    if(tk==":D") return "++ptr; ENSURE_CAP(ptr);\n";
    if(tk=="D:") return "if (ptr > 0) --ptr;\n";
    return string("// Unknown: ")+tk+"\n";
}

string generate_c_code(const vector<string>& code, const string& source, bool needs_input){
    stringstream out;
    out<<"#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n";
    out<<"#define ENSURE_CAP(idx) do { if ((idx) >= size) { size_t newsize = size?size:1; while (newsize <= (idx)) newsize <<= 1; unsigned char* newt = (unsigned char*)realloc(tape, newsize); if (!newt) { fprintf(stderr, \"Allocation failure\\n\"); exit(1); } memset(newt + size, 0, newsize - size); tape = newt; size = newsize; } } while(0)\n\n";
    if(needs_input){
        out<<"static const char* argv_input = NULL; static size_t argv_input_len = 0; static size_t argv_input_pos = 0;\n";
        out<<"static int my_getchar(void){ if (argv_input) { if (argv_input_pos < argv_input_len) return (unsigned char)argv_input[argv_input_pos++]; return EOF; } return getchar(); }\n\n";
        out<<"int main(int argc, char* argv[]) {\n";
        out<<"    if (argc > 1) { size_t total = 0; for (int i = 1; i < argc; ++i) total += strlen(argv[i]) + 1; char* buf = (char*)malloc(total + 1); if (!buf) { fprintf(stderr, \"Allocation failure\\n\"); return 1; } buf[0] = '\\0'; for (int i = 1; i < argc; ++i) { strcat(buf, argv[i]); if (i + 1 < argc) strcat(buf, \" \"); } argv_input = buf; argv_input_len = strlen(buf); argv_input_pos = 0; }\n";
    } else out<<"int main(void) {\n";
    out<<"    size_t size = 1, ptr = 0;\n";
    out<<"    unsigned char* tape = (unsigned char*)calloc(size, 1);\n";
    auto opt = optimize_simple(code);
    for(auto&tk:opt) out<<"    "<<translate_token(tk,source);
    out<<"    free(tape);\n";
    if(needs_input) out<<"    if (argv_input) free((void*)argv_input);\n";
    out<<"    return 0;\n}\n";
    return out.str();
}

string rand_suf(int l=6){
    static const char cs[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    random_device rd;
    mt19937 g(rd());
    uniform_int_distribution<> d(0,(int)sizeof(cs)-2);
    string r;
    for(int i=0;i<l;++i) r+=cs[d(g)];
    return r;
}

int main(int argc,char*argv[]){
    if(argc<2||argc>3){ cerr<<"Usage: emoticompile <file.O_O> [-k|--keep] or emoticompile --version\n"; return 1; }
    string a=argv[1];
    bool keep=false;
    if(a=="--version"){ cout<<"Emoticode compiler version 2.6.0 :)\n"; return 0; }
    if(argc==3){ string f=argv[2]; if(f=="-k"||f=="--keep") keep=true; else { cerr<<"Unknown option: "<<f<<"\n"; return 1; } }
    if(a.size()<5||a.substr(a.size()-4)!=".O_O"){ cerr<<"Error: Input file must end in .O_O\n"; return 1; }
    ifstream in(a); if(!in){ cerr<<"Error: Cannot open source file\n"; return 1; }
    stringstream buf; buf<<in.rdbuf(); string src=buf.str();
    try{
        auto toks=tokenize(src);
        auto code=preprocess(toks);
        bool needs_input = any_of(code.begin(),code.end(),[](const string&s){return s==":()";});
        string csrc=generate_c_code(code,src,needs_input);
        fs::path base=fs::path(a).parent_path();
        string stem=fs::path(a).stem().string();
        fs::path cfile = keep? base/(stem+".c") : fs::temp_directory_path()/(string("emoticode_")+rand_suf()+"/transpiled.c");
        if(!keep) fs::create_directories(cfile.parent_path());
        ofstream out(cfile); out<<csrc; out.close();
        fs::path exe = keep? base/(stem+".exe") : cfile.parent_path()/"a.out";
        string cmd = "g++ -std=c++17 -O2 -Wall -Wextra \""+cfile.string()+"\" -o \""+exe.string()+"\"";
        int r=system(cmd.c_str());
        if(r){ cerr<<"Compilation failed.\n"; if(!keep) fs::remove_all(cfile.parent_path()); return 1; }
        fs::path finalexe = base/(stem+".exe");
        if(!keep){
            if(fs::exists(finalexe)) fs::remove(finalexe);
            if(exe!=finalexe) fs::copy_file(exe,finalexe,fs::copy_options::overwrite_existing);
            fs::remove_all(cfile.parent_path());
            cout<<"Compiled successfully to: "<<finalexe<<"\n";
        } else {
            cout<<"Compiled successfully to: "<<exe<<"\nC source stored at: "<<cfile<<"\n";
        }
    } catch(const exception& e){ cerr<<"Error: "<<e.what()<<"\n"; return 1; }
    return 0;
}
