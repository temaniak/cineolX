// Command-line preparation uses the same importer as Cineol-X 224.
#include "../import/bank_import.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) try {
    using namespace native_hall;
    if(argc!=3) throw std::runtime_error("usage: extract_bank ROM_DIRECTORY OUTPUT.bank224");
    import::RomSet roms{};unsigned mask=0;
    for(const auto& entry:std::filesystem::directory_iterator(argv[1])) {
        if(!entry.is_regular_file() || entry.file_size()!=2048) continue;
        std::array<uint8_t,2048> data{};std::ifstream file(entry.path(),std::ios::binary);
        if(!file.read(reinterpret_cast<char*>(data.data()),data.size())) continue;
        const int chip=import::rom_chip(data.data(),data.size());
        if(chip>=0) {roms[unsigned(chip)]=data;mask|=1u<<chip;}
    }
    if(mask!=31) throw std::runtime_error("Original Lexicon 224 v4.4 ROM1-ROM5 required; 224X/224XL are not supported.");
    import::Callbacks callbacks;callbacks.log=&std::cout;callbacks.capture_prefix=argv[2];
    auto bank=import::prepare_bank(roms,callbacks);
    BankHeader header;header.checksum=profile_checksum(bank.get(),sizeof(ProgramBank));
    std::ofstream out(argv[2],std::ios::binary);
    out.write(reinterpret_cast<const char*>(&header),sizeof header);
    out.write(reinterpret_cast<const char*>(bank.get()),sizeof(ProgramBank));
    if(!out) throw std::runtime_error("bank write failed");
    std::cout<<"Prepared all six v4.4 programs: "<<sizeof(ProgramBank)<<" bytes\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
