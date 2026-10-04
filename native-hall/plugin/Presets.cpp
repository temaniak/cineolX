#include "Processor.hpp"
#include <cmath>
#include <sstream>

namespace {
constexpr const char* preset_tag="Cineol224Preset";
constexpr const char* selection_tag="CINEOL_PRESET";
juce::Result validatePreset(NativeHallProcessor& processor,const juce::ValueTree& preset,std::array<float,NativeHallProcessor::parameter_count>& normalized,bool require_available=true) {
    const int version=int(preset.getProperty("version",0));
    if(version!=1 && version!=2 && version!=3 && version!=4) return juce::Result::fail("This preset format is not supported.");
    const auto identity=preset.getProperty("firmware",version==1?"224-v4.4":"").toString();
    const auto* firmware=cineol::find_firmware(identity.toStdString());
    if(!firmware) return juce::Result::fail("The preset has an unknown firmware identity.");
    if(require_available && !firmware->selectable) return juce::Result::fail("This preset requires "+juce::String(firmware->name.data())+", which is not supported by this build.");
    if(preset.getNumChildren()!=(version<3?15:version==3?17:int(NativeHallProcessor::parameter_count)))
        return juce::Result::fail("The preset does not contain a complete set of parameters.");
    for(unsigned i=0;i<normalized.size();++i) {
        auto value=preset.getChildWithProperty("id",NativeHallProcessor::ids[i]);auto* parameter=processor.state.getParameter(NativeHallProcessor::ids[i]);
        if(version<4 && i>=17) {normalized[i]=parameter->convertTo0to1(-1.0f);continue;}
        if(version<3 && i>=15) {normalized[i]=parameter->convertTo0to1(16.0f);continue;}
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
    const int raw_program=int(processor.state.getParameter("algorithm")->convertFrom0to1(normalized[14]));
    const bool xl=identity=="224xl-v8.21";
    const int program=raw_program+(version<3 && xl?int(native_hall::program_count):0);
    if(firmware->selectable && (program<0 || program>=int(NativeHallProcessor::program_count) || NativeHallProcessor::isXL(program)!=xl))
        return juce::Result::fail("The preset algorithm does not match its firmware identity.");
    if(require_available && !processor.programAvailable(program))
        return juce::Result::fail("Import the ROMs required by this preset first.");
    normalized[14]=processor.state.getParameter("algorithm")->convertTo0to1(float(program));
    return juce::Result::ok();
}

juce::ValueTree capture(NativeHallProcessor& processor,const juce::String& name) {
    juce::ValueTree preset(preset_tag);
    preset.setProperty("version",4,nullptr);preset.setProperty("name",name,nullptr);
    preset.setProperty("firmware",processor.firmwareId(),nullptr);
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
    selection.setProperty("firmware",preset.getProperty("firmware","224-v4.4"),nullptr);
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
    if(selection.getProperty("firmware","224-v4.4")!=firmwareId()) return true;
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
    if(!xml || !xml->hasTagName(preset_tag) || (xml->getIntAttribute("version")!=1 && xml->getIntAttribute("version")!=2 && xml->getIntAttribute("version")!=3 && xml->getIntAttribute("version")!=4))
        return juce::Result::fail("Choose a supported Cineol preset (.cineol224).");
    return applyPreset(juce::ValueTree::fromXml(*xml),file.getFileNameWithoutExtension());
}
juce::Result NativeHallProcessor::applyPreset(const juce::ValueTree& preset,const juce::String& name) {
    std::array<float,NativeHallProcessor::parameter_count> normalized{};
    const auto valid=validatePreset(*this,preset,normalized);if(valid.failed()) return valid;
    const juce::ScopedLock lock(preset_write_lock_);
    parameter_transaction_.fetch_add(1,std::memory_order_acq_rel);
    state.state.setProperty("firmware_id",preset.getProperty("firmware","224-v4.4"),nullptr);
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
            if(!entry.hasType(preset_tag) || (int(entry.getProperty("version"))!=1 && int(entry.getProperty("version"))!=2 && int(entry.getProperty("version"))!=3 && int(entry.getProperty("version"))!=4) || name.trim().isEmpty() || names.contains(name,true))
                return juce::Result::fail("The preset bank contains an invalid or duplicate entry.");
            std::array<float,NativeHallProcessor::parameter_count> normalized{};
            auto valid=validatePreset(processor,entry,normalized,false);if(valid.failed()) return valid;
            names.add(name);
        }
    } else {
        bank=juce::ValueTree(bank_tag);bank.setProperty("version",1,nullptr);
        // Adopt presets from the previous default folder without removing them.
        auto files=file.getParentDirectory().findChildFiles(juce::File::findFiles,false,"*.cineol224");files.sort();
        for(const auto& legacy:files) {
            if(legacy.getSize()>128*1024) continue;
            auto xml=juce::XmlDocument::parse(legacy);
            if(!xml || !xml->hasTagName(preset_tag) || (xml->getIntAttribute("version")!=1 && xml->getIntAttribute("version")!=2 && xml->getIntAttribute("version")!=3 && xml->getIntAttribute("version")!=4)) continue;
            auto entry=juce::ValueTree::fromXml(*xml);
            std::array<float,NativeHallProcessor::parameter_count> normalized{};if(validatePreset(processor,entry,normalized,false).failed()) continue;
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
            program<0 || program>=NativeHallProcessor::program_count || program!=std::floor(program)) {
            entries.clear();return juce::Result::fail("A preset in the bank has an invalid algorithm.");
        }
        if(int(entry.getProperty("version"))<3 && entry.getProperty("firmware","224-v4.4").toString()=="224xl-v8.21") program+=native_hall::program_count;
        entries.add({entry.getProperty("name").toString(),int(program),entry.getProperty("firmware","224-v4.4").toString()});
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
std::array<juce::String,NativeHallProcessor::quick_preset_count> NativeHallProcessor::quickPresetNames() {
    const juce::ScopedLock lock(preset_write_lock_);
    std::array<juce::String,quick_preset_count> names;
    for(unsigned slot=0;slot<names.size();++slot)
        names[slot]=state.state.getProperty("quick_preset_"+juce::String(slot+1)).toString().substring(0,80);
    return names;
}
int NativeHallProcessor::activeQuickPreset() {
    const juce::ScopedLock lock(preset_write_lock_);
    if(presetModified()) return -1;
    const auto current=presetName();const auto names=quickPresetNames();
    const int preferred=int(state.state.getProperty("quick_preset_active",-1));
    if(juce::isPositiveAndBelow(preferred,int(names.size())) && names[unsigned(preferred)].isNotEmpty() &&
        names[unsigned(preferred)]==current) return preferred;
    for(unsigned slot=0;slot<names.size();++slot) if(names[slot].isNotEmpty() && names[slot]==current) return int(slot);
    return -1;
}
juce::Result NativeHallProcessor::assignQuickPreset(unsigned slot,const juce::String& name,const juce::File& bank) {
    if(slot>=quick_preset_count) return juce::Result::fail("Choose a quick preset key from 1 to 8.");
    juce::Array<PresetInfo> entries;auto result=presetBankEntries(entries,bank);if(result.failed()) return result;
    for(const auto& entry:entries) if(entry.name.equalsIgnoreCase(name)) {
        {
            const juce::ScopedLock lock(preset_write_lock_);
            // Instance metadata only. Assignment never loads sound or emits parameter events.
            state.state.setProperty("quick_preset_"+juce::String(slot+1),entry.name,nullptr);
        }
        updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
        return juce::Result::ok();
    }
    return juce::Result::fail("This preset is no longer in the bank.");
}
juce::Result NativeHallProcessor::loadQuickPreset(unsigned slot,const juce::File& bank) {
    if(slot>=quick_preset_count) return juce::Result::fail("Choose a quick preset key from 1 to 8.");
    const auto name=quickPresetNames()[slot];
    if(name.isEmpty()) return juce::Result::fail("Assign a preset to this key in the preset bank first.");
    auto result=loadBankPreset(name,bank);
    if(result.wasOk()) {
        {
            const juce::ScopedLock lock(preset_write_lock_);
            state.state.setProperty("quick_preset_active",int(slot),nullptr);
        }
        updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
    }
    return result;
}
