// SPDX-License-Identifier: GPL-3.0-or-later
#include "Language.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
int main(int argc,char**argv) {
 using namespace Localization;
 assert(NormalizeLanguage(L" KOREAN ")=="korean");
 assert(NormalizeLanguage(L"english")=="english");
 assert(NormalizeLanguage(L"Japanese")=="japanese");
 assert(NormalizeLanguage(L"ko")=="korean");
 assert(NormalizeLanguage(L"en")=="english");
 assert(NormalizeLanguage(L"../korean")=="english");
 assert(NormalizeLanguage(L" ")=="english");
 Rows rows;
 assert(ParseCsv("\xEF\xBB\xBFkey,text\r\nfirst,\"hello, \"\"world\"\"\nnext line\"\r\n",rows));
 assert(rows.size()==2 && rows[1][1]=="hello, \"world\"\nnext line");
 assert(!ParseCsv("key,text\na,\"unclosed",rows));
 assert(!ParseCsv("key,text\na,\"closed\"junk",rows));
 assert(!ParseCsv(std::string("key,text\na,\xC0\xAF"),rows));
 assert(!Compatible("Value: %s","Value: %n"));
 assert(!Compatible("Value: %s","Value: %s %s"));
 assert(Compatible("Step %.3f / %.2f","\xEA\xB0\x80 %.3f / %.2f"));
 assert(!Compatible("Title###stable","Title###changed"));
 assert(!Compatible("Button","Button##different"));
 std::string decoded;
 assert(Decode("One\\0Two\\0Three\\0",decoded));
 assert(Compatible(std::string_view("A\0B\0C\0",6),decoded));
 assert(!Decode("bad\\q",decoded));
 assert(!Compatible("Text",std::string_view("A\0B",3)));
 const auto file=std::filesystem::path("build/tests/language-test.csv");
 {std::ofstream f(file);f<<"key,text\nui.001,Custom Hair\nui.001,Duplicate\nunknown,Ignored\nui.002,\n";}
 auto result=Load(file);
 assert(result.loaded==1 && result.rejected==3);
 assert(std::string(L(0))=="Custom Hair");
 assert(std::string_view(L(1))==entries[1].english);
 {std::ofstream f(file);f<<"key,text\na,\"broken";}
 assert(Load(file).loaded==0 && translations.empty());
 std::filesystem::remove(file);
 assert(Load(file).status=="missing" && translations.empty());
 assert(argc==2);
 const auto directory=std::filesystem::path(argv[1]);
 for(const auto* language:{"english","korean"}) {
  result=Load(directory/(std::string(language)+".csv"));
  assert(result.status=="ok" && result.loaded==entries.size() && result.rejected==0);
  for(std::size_t i=0;i<entries.size();++i){assert(Compatible(entries[i].english,translations.at(i)));}
 }
 std::cout<<"CSV parsing, UTF-8, placeholders, fallback, language names and both shipped catalogs passed\n";
}
