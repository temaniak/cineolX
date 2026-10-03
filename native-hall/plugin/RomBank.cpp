#include "RomBank.hpp"
#include <stdexcept>

namespace {
constexpr int64_t bank_file_size=sizeof(native_hall::BankHeader)+sizeof(native_hall::ProgramBank);
constexpr const char* missing_roms=
    "A complete original Lexicon 224 v4.4 set is required: ROM1-ROM5.\n224X, 224XL and other firmware versions are not supported.";
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
CineolRomBank::CineolRomBank():Thread("Cineol 224 ROM import") {
    const auto file=cacheFile();juce::MemoryBlock bytes;
    if(file.getSize()==bank_file_size && file.loadFileAsData(bytes) &&
       native_hall::read_bank(bytes.getData(),bytes.getSize(),bank_)) {
        status_="Original 224 v4.4 ROMs imported.";
        ready_.store(true,std::memory_order_release);
    } else {
        status_=file.existsAsFile()?juce::String("The saved bank is damaged or incompatible. Please import your 224 v4.4 ROMs again."):
            "224X, 224XL and other firmware versions are not supported.";
    }
}
CineolRomBank::~CineolRomBank() {
    // Cancellation is checked during emulation; never force-kill a thread
    // that may be writing a temporary cache file.
    signalThreadShouldExit();stopThread(-1);
}
juce::String CineolRomBank::status() const {const juce::ScopedLock lock(status_lock_);return status_;}
void CineolRomBank::setStatus(const juce::String& text) {const juce::ScopedLock lock(status_lock_);status_=text;}
bool CineolRomBank::startImport(const juce::Array<juce::File>& files) {
    if(files.isEmpty() || ready() || isThreadRunning()) return false;
    bool expected=false;
    if(!importing_.compare_exchange_strong(expected,true,std::memory_order_acq_rel)) return false;
    sources_=files;progress_.store(0);setStatus("Checking original 224 v4.4 ROMs...");
    if(startThread(Priority::low)) return true;
    setStatus("Could not start ROM import. Please try again.");importing_.store(false,std::memory_order_release);return false;
}
void CineolRomBank::run() {
    try {
        native_hall::import::RomSet roms{};unsigned mask=0;
        auto cancelled=[&] {if(threadShouldExit()) throw std::runtime_error("Import cancelled.");};
        auto accept=[&](const juce::MemoryBlock& bytes) {
            const int chip=native_hall::import::rom_chip(static_cast<const uint8_t*>(bytes.getData()),bytes.getSize());
            if(chip>=0) {
                std::memcpy(roms[unsigned(chip)].data(),bytes.getData(),2048);mask|=1u<<chip;
            }
        };
        auto read=[&](const juce::File& file) {
            cancelled();
            if(file.hasFileExtension("zip")) {
                juce::ZipFile zip(file);
                if(zip.getNumEntries()>10000) throw std::runtime_error("This ZIP contains too many files. Select the five 224 v4.4 ROM files directly.");
                for(int i=0;i<zip.getNumEntries();++i) {
                    cancelled();
                    const auto* entry=zip.getEntry(i);
                    if(!entry || entry->uncompressedSize!=2048) continue;
                    std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(i));
                    if(!stream) continue;
                    juce::MemoryBlock bytes(2048);
                    if(stream->read(bytes.getData(),2048)==2048) accept(bytes);
                }
            } else if(file.getSize()==2048) {
                juce::MemoryBlock bytes;if(file.loadFileAsData(bytes) && bytes.getSize()==2048) accept(bytes);
            }
        };
        for(const auto& source:sources_) {
            if(source.isDirectory()) {
                for(const auto& entry:juce::RangedDirectoryIterator(source,true,"*",juce::File::findFiles))
                    read(entry.getFile());
            } else read(source);
        }
        cancelled();
        if(mask!=31) throw std::runtime_error(missing_roms);
        native_hall::import::Callbacks callbacks;const char* last_stage=nullptr;
        callbacks.progress=[&](double value,const char* stage) {
            progress_.store(value,std::memory_order_relaxed);
            if(stage!=last_stage) {setStatus("Preparing "+juce::String(stage)+"...");last_stage=stage;}
            return !threadShouldExit();
        };
        auto prepared=native_hall::import::prepare_bank(roms,callbacks);
        cancelled();
        const auto destination=cacheFile();
        if(destination.getParentDirectory().createDirectory().failed())
            throw std::runtime_error("Could not create the local data folder. Check its write permissions and try again.");
        juce::TemporaryFile temporary(destination);
        native_hall::BankHeader header;
        header.checksum=native_hall::profile_checksum(prepared.get(),sizeof(*prepared));
        {
            juce::FileOutputStream stream(temporary.getFile());
            if(!stream.openedOk() || !stream.write(&header,sizeof header) || !stream.write(prepared.get(),sizeof(*prepared)))
                throw std::runtime_error("Could not save the prepared bank. Check free disk space and try again.");
            stream.flush();
            if(stream.getStatus().failed()) throw std::runtime_error("Could not finish saving the prepared bank.");
        }
        cancelled();
        if(!temporary.overwriteTargetFileWithTemporary()) throw std::runtime_error("Could not replace the local bank. Please try again.");
        bank_=*prepared;
        setStatus("Original 224 v4.4 ROMs imported. Ready.");
        ready_.store(true,std::memory_order_release);
    } catch(const std::exception& error) {setStatus(juce::String::fromUTF8(error.what()));}
    catch(...) {setStatus("ROM import failed. Please try again.");}
    importing_.store(false,std::memory_order_release);
}
