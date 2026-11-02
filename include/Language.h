// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "LocaleCatalog.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

namespace Localization {
    inline std::map<std::size_t, std::string> translations;
    inline const char* L(std::size_t index) {
        const auto found = translations.find(index);
        return found == translations.end() ? entries.at(index).english.data() : found->second.c_str();
    }
    inline std::string NormalizeLanguage(std::wstring_view value) {
        const auto first = value.find_first_not_of(L" \t\r\n");
        if (first == value.npos) { return "english"; }
        value = value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
        std::string result;
        for (auto c : value) {
            if (c >= L'A' && c <= L'Z') { c += L'a' - L'A'; }
            if (!((c >= L'a' && c <= L'z') || c == L'_')) { return "english"; }
            result += static_cast<char>(c);
        }
        // Read old 1.1 settings, but new files/settings use Skyrim language names.
        if (result == "en") { return "english"; }
        if (result == "ko") { return "korean"; }
        return result;
    }
    inline bool Utf8(std::string_view s) {
        for (std::size_t i = 0; i < s.size();) {
            auto c = static_cast<unsigned char>(s[i++]);
            if (!c) { return false; }
            if (c < 0x80) { continue; }
            unsigned n, cp, minimum;
            if (c >= 0xC2 && c <= 0xDF) { n=1; cp=c&31; minimum=0x80; }
            else if (c >= 0xE0 && c <= 0xEF) { n=2; cp=c&15; minimum=0x800; }
            else if (c >= 0xF0 && c <= 0xF4) { n=3; cp=c&7; minimum=0x10000; }
            else { return false; }
            if (i+n > s.size()) { return false; }
            while (n--) { auto b=static_cast<unsigned char>(s[i++]); if ((b&0xC0)!=0x80) {return false;} cp=(cp<<6)|(b&63); }
            if (cp<minimum || cp>0x10FFFF || (cp>=0xD800 && cp<=0xDFFF)) { return false; }
        }
        return true;
    }
    using Rows = std::vector<std::vector<std::string>>;
    // RFC-style CSV: quoted commas/newlines and doubled quotes; strict malformed-file fallback.
    inline bool ParseCsv(std::string_view s, Rows& rows) {
        rows.clear();
        if (s.starts_with("\xEF\xBB\xBF")) { s.remove_prefix(3); }
        if (!Utf8(s)) { return false; }
        std::vector<std::string> row; std::string field;
        bool quoted=false, closed=false, started=false;
        auto finishField=[&] { row.push_back(std::move(field)); field.clear(); closed=false; started=false; };
        auto finishRow=[&] { finishField(); rows.push_back(std::move(row)); row.clear(); };
        for (std::size_t i=0;i<s.size();++i) {
            char c=s[i];
            if (quoted) {
                if (c=='"') {
                    if (i+1<s.size() && s[i+1]=='"') { field+='"'; ++i; }
                    else { quoted=false; closed=true; }
                } else { field+=c; }
                continue;
            }
            if (c==',') {finishField();continue;}
            if (c=='\n' || c=='\r') {
                if (c=='\r' && i+1<s.size() && s[i+1]=='\n') {++i;}
                finishRow();continue;
            }
            if (closed) {return false;}
            if (c=='"') {if(started){return false;} quoted=true;started=true;}
            else {field+=c;started=true;}
        }
        if (quoted) {return false;}
        if (started || closed || !row.empty()) {finishRow();}
        return true;
    }
    inline bool Decode(std::string_view input, std::string& out) {
        out.clear();
        for(std::size_t i=0;i<input.size();++i) {
            char c=input[i];
            if(c=='\\') {
                if(++i==input.size()){return false;}
                switch(input[i]) {
                case '0':c='\0';break;case 'n':c='\n';break;case 'r':c='\r';break;
                case 't':c='\t';break;case '\\':c='\\';break;default:return false;
                }
            }
            out+=c;
        }
        return true;
    }
    inline std::vector<std::string> Formats(std::string_view s) {
        std::vector<std::string> result;
        for(std::size_t i=0;i<s.size();++i) {
            if(s[i]!='%'){continue;}
            if(i+1<s.size() && s[i+1]=='%'){++i;continue;}
            auto start=i++;
            while(i<s.size() && ((s[i]>='0'&&s[i]<='9') || s[i]=='.' || s[i]=='-' || s[i]=='+' || s[i]==' ' || s[i]=='#')){++i;}
            if(i==s.size() || (s[i]!='s' && s[i]!='u' && s[i]!='f')) {return {"INVALID"};}
            result.emplace_back(s.substr(start,i-start+1));
        }
        return result;
    }
    inline bool Compatible(std::string_view original, std::string_view text) {
        if(text.empty() || Formats(original)!=Formats(text)){return false;}
        if(std::count(original.begin(),original.end(),'\0')!=std::count(text.begin(),text.end(),'\0')){return false;}
        if(original.find('\0')!=original.npos) {
            if(text.back()!='\0' || text.front()=='\0' || text.find(std::string_view("\0\0",2))!=text.npos){return false;}
        }
        auto marker=original.find("###");
        if(marker!=original.npos) {
            auto target=text.find("###");
            if(target==text.npos || original.substr(marker)!=text.substr(target)){return false;}
        } else if(text.find("##")!=text.npos){return false;}
        return true;
    }
    struct LoadResult {std::size_t loaded=0,rejected=0;std::string status="missing";};
    inline LoadResult Load(const std::filesystem::path& file) {
        translations.clear(); LoadResult result;
        try {
            std::ifstream stream(file,std::ios::binary);
            if(!stream){return result;}
            stream.seekg(0,std::ios::end);auto size=stream.tellg();
            if(size<0 || size>4*1024*1024){result.status="invalid size";return result;}
            stream.seekg(0);std::string content(static_cast<std::size_t>(size),'\0');
            if(!stream.read(content.data(),size)){result.status="read error";return result;}
            Rows rows;
            if(!ParseCsv(content,rows) || rows.empty() || rows[0]!=std::vector<std::string>{"key","text"}){result.status="invalid UTF-8/CSV/header";return result;}
            for(std::size_t row=1;row<rows.size();++row) {
                const auto& cells=rows[row];
                if(cells==std::vector<std::string>{""}){continue;}
                if(cells.size()!=2){++result.rejected;continue;}
                std::size_t i=0;while(i<entries.size() && entries[i].key!=cells[0]){++i;}
                std::string decoded;
                if(i==entries.size() || translations.contains(i) || !Decode(cells[1],decoded) || !Compatible(entries[i].english,decoded)){++result.rejected;continue;}
                translations.emplace(i,std::move(decoded));++result.loaded;
            }
            result.status="ok";
        } catch(const std::exception&) {translations.clear();result.loaded=0;result.status="read error";}
        return result;
    }
}
