#include "../import/xl_import.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc,char** argv) {
    if(argc!=3 && !(argc==4 && std::string(argv[3])=="--full-emulation")) {
        std::cerr<<"usage: cineol_xl_extract ROM_DIRECTORY BANK_FILE [--full-emulation]\n";return 1;
    }
    try {
        cineol::xl::import::RomSet roms;
        for(const auto& file:std::filesystem::directory_iterator(argv[1])) {
            if(!file.is_regular_file() || file.file_size()>4096) continue;
            std::ifstream input(file.path(),std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
            const int chip=cineol::xl::import::rom_chip(bytes.data(),bytes.size());
            if(chip>=0) roms[unsigned(chip)]=std::move(bytes);
        }
        native_hall::import::Callbacks callbacks;const char* previous=nullptr;
        callbacks.full_emulation=argc==4;
        callbacks.log=&std::cout;
        callbacks.progress=[&](double,const char* stage) {if(stage!=previous) {std::cout<<stage<<std::endl;previous=stage;}return true;};
        auto bank=cineol::xl::import::prepare_bank(roms,callbacks);
        cineol::xl::BankHeader header;header.checksum=native_hall::profile_checksum(bank.get(),sizeof(*bank));
        std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(&header),sizeof header);
        file.write(reinterpret_cast<const char*>(bank.get()),sizeof(*bank));if(!file) throw std::runtime_error("Could not write XL bank.");
        std::cout<<"Prepared "<<bank->programs.size()<<" native XL programs; ROMs and bank remain private.\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
