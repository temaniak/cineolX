#include "Processor.hpp"
#include <cmath>
#include <sstream>

namespace {
constexpr const char* preset_tag="Cineol224Preset";
constexpr const char* selection_tag="CINEOL_PRESET";
juce::Result validatePreset(NativeHallProcessor& processor,const juce::ValueTree& preset,std::array<float,15>& normalized) {
    if(preset.getNumChildren()!=int(std::size(NativeHallProcessor::ids)))
        return juce::Result::fail("The preset does not contain a complete set of parameters.");
    for(unsigned i=0;i<normalized.size();++i) {
        auto value=preset.getChildWithProperty("id",NativeHallProcessor::ids[i]);auto* parameter=processor.state.getParameter(NativeHallProcessor::ids[i]);
        const auto raw=value.getProperty("value");
        if(!value.hasType("PARAM") || !value.hasProperty("value"))
            return juce::Result::fail("The preset has a missing or invalid parameter.");
        // XML attributes are strings after ValueTree::fromXml; require a full
        // numeric parse rather than silently interpreting malformed text as 0.
        const auto text=raw.toString().trim();std::istringstream input(text.toStdString());
        input.imbue(std::locale::classic());double number=0;input>>number;
        const auto& range=parameter->getNormalisableRange();
        if(!input || !input.eof() || !std::isfinite(number) || number<range.start || number>range.end)
            return juce::Result::fail("The preset has a parameter outside its supported range.");
        normalized[i]=parameter->convertTo0to1(range.snapToLegalValue(float(number)));
    }
    return juce::Result::ok();
}

juce::ValueTree capture(NativeHallProcessor& processor,const juce::String& name) {
    juce::ValueTree preset(preset_tag);
    preset.setProperty("version",1,nullptr);preset.setProperty("name",name,nullptr);
    for(const auto* id:NativeHallProcessor::ids) {
        const auto* parameter=processor.state.getParameter(id);
        juce::ValueTree value("PARAM");value.setProperty("id",id,nullptr);
        value.setProperty("value",parameter->convertFrom0to1(parameter->getValue()),nullptr);
        preset.appendChild(value,nullptr);
    }
    return preset;
}
void remember(NativeHallProcessor& processor,const juce::ValueTree& preset,const juce::String& name) {
    auto& state=processor.state.state;
    state.removeChild(state.getChildWithName(selection_tag),nullptr);
    juce::ValueTree selection(selection_tag);selection.setProperty("name",name,nullptr);
    for(const auto& value:preset) selection.appendChild(value.createCopy(),nullptr);
    state.appendChild(selection,nullptr);
}
}

juce::File NativeHallProcessor::presetDirectory() {
    return CineolRomBank::cacheFile().getParentDirectory().getChildFile("Presets");
}
juce::String NativeHallProcessor::presetName() {
    const juce::ScopedLock lock(preset_write_lock_);
    return state.state.getChildWithName(selection_tag).getProperty("name","Unsaved settings").toString();
}
bool NativeHallProcessor::presetModified() {
    const juce::ScopedLock lock(preset_write_lock_);
    auto selection=state.state.getChildWithName(selection_tag);
    if(!selection.isValid()) return false;
    for(const auto* id:ids) {
        auto saved=selection.getChildWithProperty("id",id);
        auto* parameter=state.getParameter(id);
        if(!saved.isValid() || std::abs(float(saved.getProperty("value"))-
            parameter->convertFrom0to1(parameter->getValue()))>0.00001f) return true;
    }
    return false;
}
juce::Result NativeHallProcessor::savePreset(const juce::File& file) {
    const juce::ScopedLock lock(preset_write_lock_);
    auto preset=capture(*this,file.getFileNameWithoutExtension());
    auto result=file.getParentDirectory().createDirectory();if(result.failed()) return result;
    juce::TemporaryFile temporary(file);
    if(!preset.createXml()->writeTo(temporary.getFile()) || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not save the preset. Check the folder's permissions and free disk space.");
    remember(*this,preset,file.getFileNameWithoutExtension());
    return juce::Result::ok();
}
juce::Result NativeHallProcessor::loadPreset(const juce::File& file) {
    // Validate the whole file before changing any parameter. Presets contain
    // public controls only: no ROM data, DSP state, or instance latency setting.
    if(!file.existsAsFile() || file.getSize()>128*1024)
        return juce::Result::fail("This preset is missing or too large.");
    auto xml=juce::XmlDocument::parse(file);
    if(!xml || !xml->hasTagName(preset_tag) || xml->getIntAttribute("version")!=1)
        return juce::Result::fail("Choose a Cineol-X 224 preset (.cineol224) with format version 1.");
    return applyPreset(juce::ValueTree::fromXml(*xml),file.getFileNameWithoutExtension());
}
juce::Result NativeHallProcessor::applyPreset(const juce::ValueTree& preset,const juce::String& name) {
    std::array<float,15> normalized{};
    const auto valid=validatePreset(*this,preset,normalized);if(valid.failed()) return valid;
    const juce::ScopedLock lock(preset_write_lock_);
    parameter_transaction_.fetch_add(1,std::memory_order_acq_rel);
    preset_recall_revision_.fetch_add(1,std::memory_order_release);
    for(unsigned i=0;i<normalized.size();++i) {
        auto* parameter=state.getParameter(ids[i]);
        if(parameter->getValue()!=normalized[i]) {
            parameter->beginChangeGesture();parameter->setValueNotifyingHost(normalized[i]);parameter->endChangeGesture();
        }
    }
    remember(*this,capture(*this,name),name);
    parameter_transaction_.fetch_add(1,std::memory_order_release);
    return juce::Result::ok();
}

namespace {
constexpr const char* bank_tag="Cineol224PresetBank";
juce::Result readBank(NativeHallProcessor& processor,const juce::File& file,juce::ValueTree& bank) {
    if(file.existsAsFile()) {
        if(file.getSize()>8*1024*1024) return juce::Result::fail("The preset bank is too large.");
        auto xml=juce::XmlDocument::parse(file);
        if(!xml || !xml->hasTagName(bank_tag) || xml->getIntAttribute("version")!=1)
            return juce::Result::fail("The preset bank could not be read. The existing bank has been preserved.");
        bank=juce::ValueTree::fromXml(*xml);
        juce::StringArray names;
        for(auto entry:bank) {
            auto name=entry.getProperty("name").toString();
            if(!entry.hasType(preset_tag) || int(entry.getProperty("version"))!=1 || name.trim().isEmpty() || names.contains(name,true))
                return juce::Result::fail("The preset bank contains an invalid or duplicate entry.");
            std::array<float,15> normalized{};
            auto valid=validatePreset(processor,entry,normalized);if(valid.failed()) return valid;
            names.add(name);
        }
    } else {
        bank=juce::ValueTree(bank_tag);bank.setProperty("version",1,nullptr);
        // Adopt presets from the previous default folder without removing them.
        auto files=file.getParentDirectory().findChildFiles(juce::File::findFiles,false,"*.cineol224");files.sort();
        for(const auto& legacy:files) {
            if(legacy.getSize()>128*1024) continue;
            auto xml=juce::XmlDocument::parse(legacy);
            if(!xml || !xml->hasTagName(preset_tag) || xml->getIntAttribute("version")!=1) continue;
            auto entry=juce::ValueTree::fromXml(*xml);
            std::array<float,15> normalized{};if(validatePreset(processor,entry,normalized).failed()) continue;
            const auto name=legacy.getFileNameWithoutExtension();bool duplicate=false;
            for(auto existing:bank) if(existing.getProperty("name").toString().equalsIgnoreCase(name)) duplicate=true;
            if(duplicate) continue;
            entry.setProperty("name",name,nullptr);bank.appendChild(entry,nullptr);
        }
    }
    return juce::Result::ok();
}
int presetIndex(const juce::ValueTree& bank,const juce::String& name) {
    for(int i=0;i<bank.getNumChildren();++i)
        if(bank.getChild(i).getProperty("name").toString().equalsIgnoreCase(name)) return i;
    return -1;
}
juce::CriticalSection& bankWriteMutex() {static juce::CriticalSection mutex;return mutex;}
struct BankLock {
    explicit BankLock(const juce::File& file):local(bankWriteMutex()),lock("Cineol224Presets-"+juce::String::toHexString(file.getFullPathName().hashCode64())),held(lock.enter(0)) {}
    ~BankLock() {if(held) lock.exit();}
    const juce::ScopedLock local; // Serialize writers in different instances of this process.
    juce::InterProcessLock lock;bool held;
};
}
juce::File NativeHallProcessor::presetBankFile() {
    return presetDirectory().getChildFile("User Presets.cineolbank");
}
juce::Result NativeHallProcessor::presetBankEntries(juce::Array<PresetInfo>& entries,const juce::File& file) {
    juce::ValueTree bank;auto result=readBank(*this,file,bank);entries.clear();
    if(result.failed()) return result;
    for(auto entry:bank) {
        auto algorithm=entry.getChildWithProperty("id","algorithm");
        std::istringstream input(algorithm.getProperty("value").toString().trim().toStdString());
        input.imbue(std::locale::classic());double program=-1;input>>program;
        if(!algorithm.hasType("PARAM") || !input || !input.eof() || !std::isfinite(program) ||
            program<0 || program>=native_hall::program_count || program!=std::floor(program)) {
            entries.clear();return juce::Result::fail("A preset in the bank has an invalid algorithm.");
        }
        entries.add({entry.getProperty("name").toString(),int(program)});
    }
    struct ByName {int compareElements(const PresetInfo& a,const PresetInfo& b) const {return a.name.compareIgnoreCase(b.name);}} order;
    entries.sort(order);return juce::Result::ok();
}
juce::Result NativeHallProcessor::presetNames(juce::StringArray& names,const juce::File& file) {
    juce::Array<PresetInfo> entries;auto result=presetBankEntries(entries,file);names.clear();
    if(result.failed()) return result;
    for(const auto& entry:entries) names.add(entry.name);
    return juce::Result::ok();
}
juce::Result NativeHallProcessor::saveBankPreset(const juce::String& requested,bool replace,const juce::File& file) {
    auto name=requested.trim();
    if(name.isEmpty() || name.length()>80 || name.containsAnyOf("\r\n\t"))
        return juce::Result::fail("Enter a preset name of 1 to 80 characters.");
    BankLock bankLock(file);
    if(!bankLock.held) return juce::Result::fail("The preset bank is busy in another instance. Please try again.");
    const juce::ScopedLock lock(preset_write_lock_);
    juce::ValueTree bank;auto result=readBank(*this,file,bank);if(result.failed()) return result;
    const int index=presetIndex(bank,name);
    if(index>=0 && !replace) return juce::Result::fail("A preset with this name already exists.");
    auto preset=capture(*this,name);
    if(index>=0) bank.removeChild(index,nullptr);
    bank.appendChild(preset,nullptr);
    result=file.getParentDirectory().createDirectory();if(result.failed()) return result;
    juce::TemporaryFile temporary(file);
    if(!bank.createXml()->writeTo(temporary.getFile()) || temporary.getFile().getSize()>8*1024*1024 || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not save the preset bank. Check permissions and free disk space.");
    remember(*this,preset,name);return juce::Result::ok();
}
juce::Result NativeHallProcessor::loadBankPreset(const juce::String& name,const juce::File& file) {
    juce::ValueTree bank;auto result=readBank(*this,file,bank);if(result.failed()) return result;
    const int index=presetIndex(bank,name);
    if(index<0) return juce::Result::fail("This preset is no longer in the bank.");
    auto entry=bank.getChild(index);
    return applyPreset(entry,entry.getProperty("name").toString());
}
