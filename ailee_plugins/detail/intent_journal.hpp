#ifndef AILEE_INTENT_JOURNAL_HPP
#define AILEE_INTENT_JOURNAL_HPP
#include <openssl/sha.h>
#include <fstream>
#include <cmath>
#include <locale>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace AILLE::Plugins::Alpaca {
inline std::string intentDigest(const std::string& text) {
    unsigned char bytes[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(text.data()),text.size(),bytes);
    std::ostringstream out; out << std::hex << std::setfill('0');
    for(unsigned char byte:bytes) out << std::setw(2) << static_cast<unsigned>(byte);
    return out.str();
}
// One exclusive writer owns this durable reservation store. Every intent is
// fsync'd BEFORE network submission. A pending/ambiguous intent is never retried.
class IntentJournal {
    struct Entry { std::string fingerprint; std::string id; std::string symbol; double notional{0}; };
    std::map<std::string,Entry> entries;
    std::string previous;
    std::size_t bytes_written{0};
#ifdef _WIN32
    HANDLE file{INVALID_HANDLE_VALUE};
#else
    int file{-1};
#endif
    void append(const std::string& core) {
        const std::string checksum=intentDigest(previous+"\n"+core);
        writeAll(core+"\t"+checksum+"\n");
        previous=checksum;
    }
    void writeAll(const std::string& data) {
        if(data.size()>16777216-bytes_written) throw std::runtime_error("journal byte capacity exceeded");
#ifdef _WIN32
        LARGE_INTEGER end{}; if(!SetFilePointerEx(file,end,nullptr,FILE_END)) throw std::runtime_error("journal seek failed");
        DWORD written=0;
        if(!WriteFile(file,data.data(),static_cast<DWORD>(data.size()),&written,nullptr) || written!=data.size() || !FlushFileBuffers(file)) throw std::runtime_error("journal persistence failed");
#else
        if(::lseek(file,0,SEEK_END)<0) throw std::runtime_error("journal seek failed");
        std::size_t written=0;
        while(written<data.size()) { const auto count=::write(file,data.data()+written,data.size()-written); if(count<=0) throw std::runtime_error("journal write failed"); written+=static_cast<std::size_t>(count); }
        if(::fsync(file)!=0) throw std::runtime_error("journal persistence failed");
#endif
        bytes_written+=data.size();
    }
    void close() noexcept {
#ifdef _WIN32
        if(file!=INVALID_HANDLE_VALUE) { CloseHandle(file); file=INVALID_HANDLE_VALUE; }
#else
        if(file>=0) { ::close(file); file=-1; }
#endif
    }
public:
    explicit IntentJournal(const std::string& path) {
        try {
#ifdef _WIN32
            (void)path;
            throw std::runtime_error("Non-mock intent journals require verified POSIX private-file/locking semantics; unavailable on Windows");
            std::string data;
#else
            file=::open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
            if(file<0 || ::flock(file,LOCK_EX|LOCK_NB)!=0) throw std::runtime_error("journal unavailable/locked");
            struct stat stat{};
            if(::fstat(file,&stat)!=0 || !S_ISREG(stat.st_mode) || stat.st_uid!=::geteuid() || stat.st_nlink!=1 ||
               (stat.st_mode & 077)!=0 || stat.st_size<0 || stat.st_size>16777216) throw std::runtime_error("unsafe journal file");
            std::string data(static_cast<std::size_t>(stat.st_size),'\0');
            std::size_t read=0;
            while(read<data.size()) { auto count=::read(file,data.data()+read,data.size()-read); if(count<=0) throw std::runtime_error("journal read failed"); read+=static_cast<std::size_t>(count); }
#endif
            bytes_written=data.size();
            if(data.empty()) {
                writeAll("AILEE-INTENT-V1\n");
#ifndef _WIN32
                const auto parent=std::filesystem::path(path).parent_path();
                int directory=::open((parent.empty()?std::filesystem::path("."):parent).c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
                if(directory<0) throw std::runtime_error("journal directory unavailable");
                const int result=::fsync(directory); ::close(directory);
                if(result!=0) throw std::runtime_error("journal directory persistence failed");
#endif
                return;
            }
            const std::string header="AILEE-INTENT-V1\n";
            if(data.rfind(header,0)!=0 || data.back()!='\n') throw std::runtime_error("incomplete journal");
            std::istringstream stream(data.substr(header.size())); std::string line;
            while(std::getline(stream,line)) {
                const auto last=line.rfind('\t');
                if(last==std::string::npos || line.substr(last+1)!=intentDigest(previous+"\n"+line.substr(0,last))) throw std::runtime_error("journal integrity failure");
                std::string core=line.substr(0,last), state, ref, fingerprint, id, symbol, amount, extra;
                std::istringstream fields(core);
                if(!std::getline(fields,state,'\t') || !std::getline(fields,ref,'\t') || !std::getline(fields,fingerprint,'\t') ||
                   !std::getline(fields,id,'\t') || !std::getline(fields,symbol,'\t') || !std::getline(fields,amount,'\t') || std::getline(fields,extra,'\t') || ref.empty() || fingerprint.size()!=64) throw std::runtime_error("invalid journal record");
                std::istringstream parsed(amount); parsed.imbue(std::locale::classic()); double notional=0; parsed>>std::noskipws>>notional;
                if(!parsed || parsed.peek()!=std::char_traits<char>::eof() || !std::isfinite(notional) || notional<=0 || symbol.empty()) throw std::runtime_error("invalid journal notional");
                auto found=entries.find(ref);
                if(state=="P" && id=="-" && found==entries.end()) entries.emplace(ref,Entry{fingerprint,{},symbol,notional});
                else if(state=="A" && id!="-" && !id.empty() && found!=entries.end() && found->second.fingerprint==fingerprint && found->second.symbol==symbol && found->second.notional==notional && found->second.id.empty()) found->second.id=id;
                else throw std::runtime_error("invalid journal transition");
                previous=line.substr(last+1);
                if(entries.size()>100000) throw std::runtime_error("journal capacity exceeded");
            }
        } catch(...) { close(); throw; }
    }
    ~IntentJournal() { close(); }
    bool lookup(const std::string& ref,const std::string& fingerprint,std::string& id) const {
        auto found=entries.find(ref); if(found==entries.end()) return false;
        if(found->second.fingerprint!=fingerprint) throw std::runtime_error("client_ref reused with different order");
        id=found->second.id; return true;
    }
    bool hasPending() const { for(const auto& item:entries) if(item.second.id.empty()) return true; return false; }
    bool containsSymbol(const std::string& symbol) const { for(const auto& item:entries) if(item.second.symbol==symbol) return true; return false; }
    double reserved(const std::string& symbol) const { double total=0; for(const auto& item:entries) if(item.second.symbol==symbol) total+=item.second.notional; return total; }
    static std::string amountText(double amount) { std::ostringstream out; out.imbue(std::locale::classic()); out<<std::setprecision(std::numeric_limits<double>::max_digits10)<<amount; return out.str(); }
    void reserve(const std::string& ref,const std::string& fingerprint,const std::string& symbol,double notional) {
        if(entries.size()>=100000) throw std::runtime_error("journal capacity exceeded");
        append("P\t"+ref+"\t"+fingerprint+"\t-\t"+symbol+"\t"+amountText(notional)); entries.emplace(ref,Entry{fingerprint,{},symbol,notional});
    }
    void acknowledge(const std::string& ref,const std::string& fingerprint,const std::string& id) {
        const auto& entry=entries.at(ref); append("A\t"+ref+"\t"+fingerprint+"\t"+id+"\t"+entry.symbol+"\t"+amountText(entry.notional)); entries.at(ref).id=id;
    }
};
}
#endif
