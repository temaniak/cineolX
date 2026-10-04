#include "RomBank.hpp"
#include <juce-plugin/source/roms/firmware_sets_data.hpp>
#include <juce-plugin/source/roms/sha256.hpp>
#include <bit>
#include <stdexcept>

namespace {
constexpr int64_t bank_file_size=sizeof(native_hall::BankHeader)+sizeof(native_hall::ProgramBank);
constexpr const char* missing_roms=
    "Select all 5 original 224 v4.4 ROMs (ROM1-ROM5) or all 11 224XL v8.21 ROMs. Other revisions are not supported.";
}
juce::File CineolRomBank::cacheFile() {
    // An explicit override keeps tests and portable development installations
    // separate from the user's imported assets. Never auto-import source ROMs.
    const auto override=juce::SystemStats::getEnvironmentVariable("CINEOL224_CACHE_DIR",{});
    if(override.isNotEmpty()) return juce::File(override).getChildFile("programs-v44-import-v1.bank224");
    auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    folder=folder.getChildFile("Application Support");
   #endif
    return folder.getChildFile("Cineol-X 224").getChildFile("programs-v44-import-v1.bank224");
}
CineolRomBank::CineolRomBank():Thread("Cineol ROM preparation") {
    juce::MemoryBlock bytes;
    const auto original=cacheFile(),xl=xlCacheFile();
    if(original.getSize()==bank_file_size && original.loadFileAsData(bytes) &&
       native_hall::read_bank(bytes.getData(),bytes.getSize(),bank_)) ready_.store(true,std::memory_order_release);
    bytes.reset();
    if(xl.getSize()==int64_t(sizeof(cineol::xl::BankHeader)+sizeof(cineol::xl::Bank)) && xl.loadFileAsData(bytes) &&
       cineol::xl::read_bank(bytes.getData(),bytes.getSize(),xl_bank_)) xl_ready_.store(true,std::memory_order_release);
    status_=ready() && xlReady()?"224 v4.4 and 224XL v8.21 ready.":
        ready()?"224 v4.4 ready. Import 224XL v8.21 to add native XL programs.":
        xlReady()?"224XL v8.21 ready. Import 224 v4.4 to add the original programs.":missing_roms;
    if(!xlReady() && xl.getSiblingFile("programs-v821-native-v3.bankxl").existsAsFile())
        status_="Re-import the original 224XL v8.21 ROM set once to prepare the native dynamics update.";
}
CineolRomBank::~CineolRomBank() {
    // Cancellation is checked during emulation; never force-kill a thread
    // that may be writing a temporary cache file.
    signalThreadShouldExit();stopThread(-1);
}
juce::String CineolRomBank::status() const {const juce::ScopedLock lock(status_lock_);return status_;}
void CineolRomBank::setStatus(const juce::String& text) {const juce::ScopedLock lock(status_lock_);status_=text;}
bool CineolRomBank::startImport(const juce::Array<juce::File>& files) {
    if(files.isEmpty() || (ready() && xlReady()) || isThreadRunning()) return false;
    bool expected=false;
    if(!importing_.compare_exchange_strong(expected,true,std::memory_order_acq_rel)) return false;
    sources_=files;progress_.store(0);setStatus("Checking 224 / 224XL ROMs...");
    if(startThread(Priority::low)) return true;
    setStatus("Could not start ROM import. Please try again.");importing_.store(false,std::memory_order_release);return false;
}
void CineolRomBank::run() {
    try {
        native_hall::import::RomSet roms{};cineol::xl::import::RomSet xl_roms{};unsigned mask=0,xl_mask=0;
        juce::StringArray unsupported;
        auto cancelled=[&] {if(threadShouldExit()) throw std::runtime_error("Import cancelled.");};
        auto accept=[&](const juce::MemoryBlock& bytes) {
            const int chip=native_hall::import::rom_chip(static_cast<const uint8_t*>(bytes.getData()),bytes.getSize());
            if(chip>=0) {
                std::memcpy(roms[unsigned(chip)].data(),bytes.getData(),2048);mask|=1u<<chip;
            }
            const int xl_chip=cineol::xl::import::rom_chip(static_cast<const uint8_t*>(bytes.getData()),bytes.getSize());
            if(xl_chip>=0) {
                const auto* data=static_cast<const uint8_t*>(bytes.getData());
                xl_roms[unsigned(xl_chip)].assign(data,data+bytes.getSize());xl_mask|=1u<<xl_chip;
            }
            if(chip<0 && xl_chip<0) {
                const auto hash=lexplug::roms::Sha256::of(static_cast<const uint8_t*>(bytes.getData()),bytes.getSize());
                for(const auto& set:lexplug::roms::data::known_sets)
                    for(int i=0;i<set.chip_count;++i)
                        if(hash==set.chips[i].sha256) unsupported.addIfNotAlreadyThere(set.name);
            }
        };
        auto read=[&](const juce::File& file) {
            cancelled();
            if(file.hasFileExtension("zip")) {
                juce::ZipFile zip(file);
                if(zip.getNumEntries()>10000) throw std::runtime_error("This ZIP contains too many files. Select the complete ROM set directly.");
                for(int i=0;i<zip.getNumEntries();++i) {
                    cancelled();
                    const auto* entry=zip.getEntry(i);
                    if(!entry || (entry->uncompressedSize!=2048 && entry->uncompressedSize!=4096)) continue;
                    std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(i));
                    if(!stream) continue;
                    juce::MemoryBlock bytes(size_t(entry->uncompressedSize));
                    if(stream->read(bytes.getData(),int(bytes.getSize()))==int(bytes.getSize())) accept(bytes);
                }
            } else if(file.getSize()==2048 || file.getSize()==4096) {
                juce::MemoryBlock bytes;if(file.loadFileAsData(bytes) && (bytes.getSize()==2048 || bytes.getSize()==4096)) accept(bytes);
            }
        };
        for(const auto& source:sources_) {
            if(source.isDirectory()) {
                for(const auto& entry:juce::RangedDirectoryIterator(source,true,"*",juce::File::findFiles))
                    read(entry.getFile());
            } else read(source);
        }
        cancelled();
        const bool original=mask==31 && !ready(),xl=xl_mask==2047 && !xlReady();
        if(!original && !xl) {
            juce::String message;
            if(mask && !ready()) message+="224 v4.4: found "+juce::String(std::popcount(mask))+"/5 chips. ";
            if(xl_mask && !xlReady()) message+="224XL v8.21: found "+juce::String(std::popcount(xl_mask))+"/11 chips. ";
            if(unsupported.size()) message+="Unsupported firmware: "+unsupported.joinIntoString(", ")+". ";
            if(mask==31 && ready()) message+="224 v4.4 is already loaded. ";
            if(xl_mask==2047 && xlReady()) message+="224XL v8.21 is already loaded. ";
            if(message.isEmpty()) message="No supported ROM chips found. ";
            throw std::runtime_error((message+missing_roms).toStdString());
        }
        native_hall::import::Callbacks callbacks;const char* last_stage=nullptr;
        callbacks.progress=[&](double value,const char* stage) {
            progress_.store(value,std::memory_order_relaxed);
            if(stage!=last_stage) {setStatus("Preparing "+juce::String(stage)+"...");last_stage=stage;}
            return !threadShouldExit();
        };
        auto save=[&](const juce::File& destination,auto& header,const auto& prepared) {
            cancelled();
            if(destination.getParentDirectory().createDirectory().failed())
                throw std::runtime_error("Could not create the local data folder.");
            juce::TemporaryFile temporary(destination);
            header.checksum=native_hall::profile_checksum(prepared.get(),sizeof(*prepared));
            {
                juce::FileOutputStream stream(temporary.getFile());
                if(!stream.openedOk() || !stream.write(&header,sizeof header) || !stream.write(prepared.get(),sizeof(*prepared)))
                    throw std::runtime_error("Could not save the prepared bank.");
                stream.flush();if(stream.getStatus().failed()) throw std::runtime_error("Could not finish saving the bank.");
            }
            cancelled();
            if(!temporary.overwriteTargetFileWithTemporary()) throw std::runtime_error("Could not replace the local bank.");
        };
        if(original) {
            auto prepared=native_hall::import::prepare_bank(roms,callbacks);native_hall::BankHeader header;
            save(cacheFile(),header,prepared);bank_=*prepared;ready_.store(true,std::memory_order_release);
        }
        if(xl) {
            auto prepared=cineol::xl::import::prepare_bank(xl_roms,callbacks);cineol::xl::BankHeader header;
            save(xlCacheFile(),header,prepared);xl_bank_=*prepared;xl_ready_.store(true,std::memory_order_release);
        }
        setStatus(ready() && xlReady()?"224 v4.4 and 224XL v8.21 ready.":ready()?"224 v4.4 ready.":"224XL v8.21 ready.");
    } catch(const std::exception& error) {setStatus(juce::String::fromUTF8(error.what()));}
    catch(...) {setStatus("ROM import failed. Please try again.");}
    importing_.store(false,std::memory_order_release);
}
