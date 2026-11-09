// Offline signal host. Loads the actual format bundle, not NativeHall224's
// shared-code target. AU registration is process-local; nothing is installed.
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <fstream>
#include <iostream>
#include <vector>
#if JUCE_MAC
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#endif
using namespace juce;
static void require(bool ok,const String& text) {if(!ok)throw std::runtime_error(text.toStdString());}
static void set(AudioPluginInstance& plugin,const String& name,float value) {
    for(auto* parameter:plugin.getParameters())if(parameter->getName(128)==name) {
        parameter->setValueNotifyingHost(value);return;
    }
    throw std::runtime_error("Missing hosted parameter: "+name.toStdString());
}
static float normalized(int value,int minimum,int maximum) {return float(value-minimum)/float(maximum-minimum);}
static void xl(AudioPluginInstance& plugin,unsigned cell,int raw) {set(plugin,"XL Control "+String(cell+1),normalized(raw,-1,255));}
#if JUCE_MAC
static String register_au(const File& path) {
    const auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(path.getFullPathName().toRawUTF8()),path.getFullPathName().getNumBytesAsUTF8(),true);
    require(url!=nullptr,"AU URL failed");const auto bundle=CFBundleCreate(nullptr,url);CFRelease(url);
    require(bundle && CFBundleLoadExecutable(bundle),"Cannot load built AU bundle");
    const auto components=static_cast<CFArrayRef>(CFBundleGetValueForInfoDictionaryKey(bundle,CFSTR("AudioComponents")));
    require(components && CFArrayGetCount(components)>0,"AU metadata missing");
    const auto data=static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(components,0));
    const auto code=[&](CFStringRef key) {char value[16]{};const auto text=static_cast<CFStringRef>(CFDictionaryGetValue(data,key));
        require(text && CFStringGetCString(text,value,sizeof value,kCFStringEncodingUTF8),"AU FourCC missing");
        return (UInt32(uint8_t(value[0]))<<24)|(UInt32(uint8_t(value[1]))<<16)|(UInt32(uint8_t(value[2]))<<8)|UInt32(uint8_t(value[3]));};
    AudioComponentDescription description{};description.componentType=code(CFSTR("type"));
    description.componentSubType=code(CFSTR("subtype"));description.componentManufacturer=code(CFSTR("manufacturer"));
    const auto factoryName=static_cast<CFStringRef>(CFDictionaryGetValue(data,CFSTR("factoryFunction")));
    require(factoryName!=nullptr,"AU factory name missing");
    const auto factory=reinterpret_cast<AudioComponentFactoryFunction>(CFBundleGetFunctionPointerForName(bundle,factoryName));
    require(factory!=nullptr,"AU factory entry point missing");
    // Core Audio can select an installed component ahead of a new registration
    // with identical IDs. Use an isolated, process-local subtype for this exact
    // loaded factory. Neither the bundle nor its public IDs are edited.
    description.componentSubType=0x43743941; // Ct9A, validation only.
    const auto name=static_cast<CFStringRef>(CFDictionaryGetValue(data,CFSTR("name")));
    const auto component=AudioComponentRegister(&description,name,0x10100,factory);
    require(component!=nullptr,"Built AU process-local registration failed");
    require(AudioComponentFindNext(nullptr,&description)==component,"Built AU factory was not selected");
    const auto fourcc=[](UInt32 value){char chars[]{char(value>>24),char(value>>16),char(value>>8),char(value),0};return String(chars);};
    // Keep bundle loaded until process exit, while instances use its factory.
    return "AudioUnit:"+fourcc(description.componentType)+","+fourcc(description.componentSubType)+","+fourcc(description.componentManufacturer);
}
#endif
struct Host {
    AudioPluginFormatManager formats;PluginDescription description;
    explicit Host(const File& path,const String& auIdentifier) {
        if(path.hasFileExtension("component"))formats.addFormat(std::make_unique<AudioUnitPluginFormatHeadless>());
        else formats.addFormat(std::make_unique<VST3PluginFormatHeadless>());
        OwnedArray<PluginDescription> descriptions;
        formats.getFormat(0)->findAllTypesForFile(descriptions,auIdentifier.isEmpty()?path.getFullPathName():auIdentifier);
        require(descriptions.size()==1,"Expected one plugin in built bundle");description=*descriptions[0];
    }
    std::unique_ptr<AudioPluginInstance> create(unsigned program,bool clean) {
        String error;auto result=formats.createPluginInstance(description,48000,128,error);
        require(result!=nullptr,"Built plugin instantiation failed: "+error);
        set(*result,"Algorithm",normalized(int(program),0,27));set(*result,"Clean",clean?1.f:0.f);
        set(*result,"Left Output",0);set(*result,"Right Output",normalized(program>=23?1:2,0,3));
        set(*result,"Dry / Wet",1);set(*result,"Mode Enhancement",1);set(*result,"Decay Optimization",1);
        result->setRateAndBufferSizeDetails(48000,128);return result;
    }
};
using Audio=std::vector<std::array<float,2>>;
using Edit=std::function<void(AudioPluginInstance&,unsigned)>;
static Audio render(Host& host,unsigned program,bool clean,const Edit& configure,const Edit& edit={},bool continuous=false) {
    auto plugin=host.create(program,clean);configure(*plugin,0);plugin->prepareToPlay(48000,128);
    Audio result(288000);juce::AudioBuffer<float> block(jmax(2,plugin->getTotalNumInputChannels(),plugin->getTotalNumOutputChannels()),128);MidiBuffer midi;uint32_t random=17;
    for(unsigned at=0;at<result.size();at+=128) {
        if(edit)edit(*plugin,at);
        block.clear();
        for(unsigned i=0;i<128;++i) {
            random=random*1664525u+1013904223u;const unsigned n=at+i;
            const bool active=continuous?n<192000:n>=48000 && n<144000;
            const float sample=active?.03f*float(int32_t(random))/2147483648.f+.01f*std::sin(float(n)*.02879793f):0;
            block.setSample(0,int(i),sample);block.setSample(1,int(i),sample*.7f);
        }
        plugin->processBlock(block,midi);
        for(unsigned i=0;i<128;++i)for(unsigned c=0;c<2;++c) {
            const float value=block.getSample(int(c),int(i));require(std::isfinite(value) && std::abs(value)<2,"Nonfinite/runaway built-plugin output");result[at+i][c]=value;
        }
    }
    plugin->releaseResources();double energy=0;for(auto frame:result)for(float value:frame)energy+=double(value)*value;
    require(energy>1e-5,"Built plugin lost wet signal (bank or DSP failure)");return result;
}
static void wav(const File& path,const Audio& audio) {
    std::ofstream stream(path.getFullPathName().toStdString(),std::ios::binary);
    const auto word=[&](unsigned value,unsigned bytes){for(unsigned i=0;i<bytes;++i)stream.put(char(value>>(8*i)));};
    stream.write("RIFF",4);word(unsigned(audio.size()*8+36),4);stream.write("WAVEfmt ",8);word(16,4);
    word(3,2);word(2,2);word(48000,4);word(384000,4);word(8,2);word(32,2);stream.write("data",4);word(unsigned(audio.size()*8),4);
    for(auto frame:audio)for(float sample:frame)stream.write(reinterpret_cast<const char*>(&sample),4);
    require(bool(stream),"Cannot write signal WAV");
}
int main(int argc,char** argv)try {
    require(argc==5,"usage: cineol_built_plugin_check BUNDLE ORIGINAL_BANK XL_BANK OUTPUT_DIRECTORY");
    ScopedJuceInitialiser_GUI init;const File bundle(String::fromUTF8(argv[1])),out(String::fromUTF8(argv[4]));
    require(out.createDirectory().wasOk(),"Cannot create validation output directory");const auto cache=out.getChildFile("private-test-cache");
    require(cache.createDirectory().wasOk(),"Cannot create isolated cache");
    require(File(String::fromUTF8(argv[2])).copyFileTo(cache.getChildFile("programs-v44-import-v1.bank224")) &&
        File(String::fromUTF8(argv[3])).copyFileTo(cache.getChildFile("programs-v821-native-v5.bankxl")),"Cannot seed isolated test cache");
    require(setenv("CINEOL224_CACHE_DIR",cache.getFullPathName().toRawUTF8(),1)==0,"Cannot set isolated cache");
   #if JUCE_MAC
    const auto au=bundle.hasFileExtension("component")?register_au(bundle):String{};
   #else
    const String au;
   #endif
    Host host(bundle,au);std::ofstream metrics(out.getChildFile("signals.csv").getFullPathName().toStdString());
    metrics<<"case,program,clean,peak,signal_rms,tail_rms\n";
    const auto record=[&](const char* name,unsigned program,bool clean,const Audio& samples) {
        double signal=0,tail=0;float peak=0;for(unsigned n=0;n<samples.size();++n)for(float value:samples[n]) {
            peak=std::max(peak,std::abs(value));if(n>=96000 && n<144000)signal+=double(value)*value;if(n>=192000)tail+=double(value)*value;
        }
        metrics<<name<<','<<program<<','<<clean<<','<<peak<<','<<std::sqrt(signal/96000)<<','<<std::sqrt(tail/192000)<<'\n';metrics.flush();
    };
    for(bool clean:{false,true}) {
        for(unsigned program:{1u,4u}) {
            const auto config=[](auto& p,unsigned){set(p,"Pre-delay",0);};
            const auto ramp=[&](bool capped){return [=](auto& p,unsigned at){const int ms=std::clamp((int(at)-48000)/1200,0,128);set(p,"Pre-delay",float(capped?std::min(ms,107):ms)/128.f);};};
            auto a=render(host,program,clean,config,ramp(false),true),b=render(host,program,clean,config,ramp(true),true);
            require(a==b,"Built Plate predelay bypassed 107 ms cap");record("predelay",program,clean,a);
            if(program==1 && !clean)wav(out.getChildFile("plate-predelay.wav"),a);
        }
        for(unsigned program:{9u,16u,24u,25u}) {
            const auto config=[&](int maximum){return [=](auto& p,unsigned){xl(p,4,maximum);if(program>=24)xl(p,10,maximum);set(p,"Mode Enhancement",0);set(p,"Decay Optimization",0);};};
            auto a=render(host,program,clean,config(255)),b=render(host,program,clean,config(254));
            require(a==b,"Built XL Attack/Depth 99 reaches mute sentinel");record("attack99",program,clean,a);
            if(program==9 && !clean)wav(out.getChildFile("xl-attack99.wav"),a);
        }
        for(unsigned program:{19u,27u}) {
            const auto config=[](int chorus,bool mod){return [=](auto& p,unsigned){set(p,"Chorus",float(chorus)/31);set(p,"Mode Enhancement",mod?1.f:0.f);set(p,"Decay Optimization",1);};};
            auto low=render(host,program,clean,config(0,true)),mid=render(host,program,clean,config(16,true)),high=render(host,program,clean,config(31,true));
            require(mid==high && low!=high,"Built Rich Chorus parameter path failed");
            require(render(host,program,clean,config(0,false))==render(host,program,clean,config(31,false)),"Built Rich Chorus changes audio with Mod off");
            record("rich_chorus",program,clean,high);
        }
        for(unsigned program=0;program<6;++program) {
            const auto config=[](auto& p,unsigned){set(p,"Modulation Rate",normalized(2,1,16));set(p,"Modulation Depth",normalized(8,1,16));};
            const auto edit=[](bool depth_again){return [=](auto& p,unsigned at){if(at==96000){set(p,"Modulation Rate",1);if(depth_again)set(p,"Modulation Depth",normalized(8,1,16));}};};
            auto a=render(host,program,clean,config,edit(false)),b=render(host,program,clean,config,edit(true));
            require(a==b,"Built Rate edit changed existing Mod Depth");record("independent_rate",program,clean,a);
            if(program==2 && !clean)wav(out.getChildFile("independent-rate-depth.wav"),a);
        }
    }
    require(bool(metrics),"Cannot finish signal metrics");
    std::cout<<bundle.getFileName()<<": all four regression blocks pass through the actual format; Original/Clean, 48 kHz / 128 frames, seeded noise and input-silent tails\n";
    require(cache.deleteRecursively(),"Cannot remove disposable test-bank copies");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
