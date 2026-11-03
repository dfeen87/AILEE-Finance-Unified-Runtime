#ifndef AILEE_BROKER_JSON_HPP
#define AILEE_BROKER_JSON_HPP
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
namespace AILLE::Plugins::Detail {
// A bounded JSON reader for broker acknowledgments. It validates the entire
// document, rejects duplicate object keys, and only exposes top-level scalars.
// Nested values are parsed but can never impersonate an acknowledgment field.
struct JsonScalar { enum Kind { STRING, NUMBER, OTHER } kind{OTHER}; std::string text; };
class BrokerJson {
    const std::string& source;
    std::size_t pos{0};
    [[noreturn]] static void invalid() { throw std::invalid_argument("invalid broker JSON"); }
    void ws() { while (pos < source.size() && (source[pos]==' ' || source[pos]=='\n' || source[pos]=='\r' || source[pos]=='\t')) ++pos; }
    bool take(char c) { ws(); if (pos<source.size() && source[pos]==c) { ++pos; return true; } return false; }
    void need(char c) { if (!take(c)) invalid(); }
    unsigned hex4() {
        unsigned out=0;
        for (int n=0;n<4;++n) {
            if (pos>=source.size()) invalid();
            const char c=source[pos++];
            out <<= 4;
            if (c>='0' && c<='9') out += c-'0';
            else if(c>='a' && c<='f') out += c-'a'+10;
            else if(c>='A' && c<='F') out += c-'A'+10;
            else invalid();
        }
        return out;
    }
    static void utf8(std::string& out,unsigned c) {
        if (c<0x80) out += static_cast<char>(c);
        else if(c<0x800) { out += static_cast<char>(0xc0|(c>>6)); out += static_cast<char>(0x80|(c&63)); }
        else if(c<0x10000) { out += static_cast<char>(0xe0|(c>>12)); out += static_cast<char>(0x80|((c>>6)&63)); out += static_cast<char>(0x80|(c&63)); }
        else { out += static_cast<char>(0xf0|(c>>18)); out += static_cast<char>(0x80|((c>>12)&63)); out += static_cast<char>(0x80|((c>>6)&63)); out += static_cast<char>(0x80|(c&63)); }
    }
    std::string string() {
        need('"'); std::string out;
        while(pos<source.size()) {
            unsigned char c=source[pos++];
            if(c=='"') return out;
            if(c<32) invalid();
            if(c!='\\') {
                if(c<0x80) { out+=static_cast<char>(c); continue; }
                unsigned code=0; unsigned count=0; unsigned minimum=0;
                if(c>=0xc2 && c<=0xdf) { code=c&31; count=1; minimum=0x80; }
                else if(c>=0xe0 && c<=0xef) { code=c&15; count=2; minimum=0x800; }
                else if(c>=0xf0 && c<=0xf4) { code=c&7; count=3; minimum=0x10000; }
                else invalid();
                for(unsigned n=0;n<count;++n) { if(pos>=source.size()) invalid(); unsigned char next=source[pos++]; if((next&0xc0)!=0x80) invalid(); code=(code<<6)|(next&63); }
                if(code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff)) invalid();
                utf8(out,code); continue;
            }
            if(pos>=source.size()) invalid();
            switch(source[pos++]) {
                case '"': out+='"'; break; case '\\': out+='\\'; break; case '/': out+='/'; break;
                case 'b': out+='\b'; break; case 'f': out+='\f'; break; case 'n': out+='\n'; break;
                case 'r': out+='\r'; break; case 't': out+='\t'; break;
                case 'u': {
                    unsigned code=hex4();
                    if(code>=0xd800 && code<=0xdbff) {
                        if(pos+2>source.size() || source[pos++]!='\\' || source[pos++]!='u') invalid();
                        const unsigned low=hex4();
                        if(low<0xdc00 || low>0xdfff) invalid();
                        code=0x10000+((code-0xd800)<<10)+(low-0xdc00);
                    } else if(code>=0xdc00 && code<=0xdfff) invalid();
                    utf8(out,code); break;
                }
                default: invalid();
            }
        }
        invalid();
    }
    JsonScalar value(unsigned depth) {
        if(depth>32) invalid(); ws(); if(pos>=source.size()) invalid();
        if(source[pos]=='"') return {JsonScalar::STRING,string()};
        if(source[pos]=='{') { object(depth+1); return {}; }
        if(source[pos]=='[') { need('['); if(!take(']')) { do { value(depth+1); } while(take(',')); need(']'); } return {}; }
        for(const std::string word : {"true","false","null"}) if(source.compare(pos,word.size(),word)==0) { pos+=word.size(); return {}; }
        const std::size_t start=pos;
        if(source[pos]=='-') ++pos;
        if(pos>=source.size()) invalid();
        if(source[pos]=='0') ++pos;
        else { if(source[pos]<'1' || source[pos]>'9') invalid(); while(pos<source.size() && source[pos]>='0' && source[pos]<='9') ++pos; }
        if(pos<source.size() && source[pos]=='.') { ++pos; const auto first=pos; while(pos<source.size() && source[pos]>='0' && source[pos]<='9') ++pos; if(pos==first) invalid(); }
        if(pos<source.size() && (source[pos]=='e' || source[pos]=='E')) { ++pos; if(pos<source.size() && (source[pos]=='+'||source[pos]=='-')) ++pos; const auto first=pos; while(pos<source.size() && source[pos]>='0' && source[pos]<='9') ++pos; if(pos==first) invalid(); }
        return {JsonScalar::NUMBER,source.substr(start,pos-start)};
    }
    std::map<std::string,JsonScalar> object(unsigned depth) {
        if(depth>32) invalid(); need('{'); std::map<std::string,JsonScalar> fields;
        if(!take('}')) { do { const std::string key=string(); need(':'); auto scalar=value(depth+1); if(!fields.emplace(key,std::move(scalar)).second) invalid(); } while(take(',')); need('}'); }
        return fields;
    }
public:
    explicit BrokerJson(const std::string& body):source(body) {}
    std::map<std::string,JsonScalar> parse() {
        if(source.size()>262144) invalid(); auto fields=object(0); ws(); if(pos!=source.size()) invalid(); return fields;
    }
};
}
#endif
