#include "Editor.hpp"
#include <functional>

using namespace cineol::pitch_lab;
namespace {
juce::Colour ink(){return juce::Colour(0xffe4eeef);}juce::Colour muted(){return juce::Colour(0xff9db2bc);}
float db(float x){return 20*std::log10(std::max(x,1e-7f));}
float layoutScale(const juce::Component& c){return std::min(c.getWidth()/1360.f,c.getHeight()/1000.f);}
}
struct PitchLabEditor::Row:juce::Component {
    juce::Label label;juce::Slider slider;juce::ComboBox combo;juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider_attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo_attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button_attachment;
    Kind kind;
    Row(PitchLabProcessor& p,unsigned index,const char* short_name):kind(definitions[index].kind) {
        const auto& d=definitions[index];label.setText(short_name,juce::dontSendNotification);label.setFont(juce::FontOptions(12));
        label.setColour(juce::Label::textColourId,ink());addAndMakeVisible(label);
        if(kind==Kind::toggle) {
            toggle.setName(d.name);toggle.setButtonText(short_name);toggle.setColour(juce::ToggleButton::textColourId,ink());addAndMakeVisible(toggle);label.setVisible(false);
            button_attachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,d.id,toggle);
        } else if(kind==Kind::choice) {
            auto choices=juce::StringArray::fromTokens(d.choices,"|","");combo.addItemList(choices,1);combo.setName(d.name);addAndMakeVisible(combo);
            combo_attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,d.id,combo);
        } else {
            slider.setName(d.name);slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,94,24);
            slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);slider.setColour(juce::Slider::trackColourId,juce::Colour(0xff75cabf));
            addAndMakeVisible(slider);slider_attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,d.id,slider);
        }
        setName(d.name);
    }
    void resized()override {
        auto r=getLocalBounds();if(kind==Kind::toggle){toggle.setBounds(r);return;}
        label.setBounds(r.removeFromLeft(119));if(kind==Kind::choice)combo.setBounds(r.reduced(1,2));else slider.setBounds(r);
    }
};
PitchLabEditor::PitchLabEditor(PitchLabProcessor& p):AudioProcessorEditor(p),processor_(p) {
    spectrum_.fill(-140);
    const auto row=[&](unsigned id,const char* text,int x,int y,int width=316) {
        auto r=std::make_unique<Row>(p,id,text);r->setBounds(x,y,width,29);addAndMakeVisible(*r);rows_.push_back(std::move(r));
    };
    preset_.setTextWhenNothingSelected("Load routing preset...");
    for(unsigned i=0;i<std::size(preset_names);++i)preset_.addItem(preset_names[i],int(i+1));
    addAndMakeVisible(preset_);preset_.onChange=[this]{if(preset_.getSelectedId()>0){processor_.applyPreset(unsigned(preset_.getSelectedId()-1));preset_.setSelectedId(0,juce::dontSendNotification);}};
    row(monitor,"Monitor",367,68,332);
    for(auto* b:{&panic_,&storeA_,&recallA_,&storeB_,&recallB_})addAndMakeVisible(*b);
    panic_.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff773d38));panic_.onClick=[this]{processor_.requestPanic();};
    storeA_.onClick=[this]{snapshot(true,true);};recallA_.onClick=[this]{snapshot(true,false);};
    storeB_.onClick=[this]{snapshot(false,true);};recallB_.onClick=[this]{snapshot(false,false);};
    int y=234;const int step=31;
    for(auto pair:{std::pair<unsigned,const char*>{semitones,"Semitones"},{cents,"Fine"},{glide,"Glide"},{window,"Window"},{overlap,"Overlap"},{mono,"Mono input"},{formant_preserve,"Preserve formants"},{formant_shift,"Formant shift"}}){row(pair.first,pair.second,30,y);y+=step;}
    for(unsigned f=0;f<4;++f) {
        const unsigned at=pre_hp+f*4;const int yy=234+int(f)*115;
        const char* name[]={"PRE-PITCH","POST-PITCH","HALL INPUT","FEEDBACK LOOP"};
        row(at+2,name[f],365,yy);row(at,"HP",365,yy+27);row(at+1,"LP",365,yy+54);row(at+3,"Slope",365,yy+81);
    }
    y=234;
    for(auto pair:{std::pair<unsigned,const char*>{hall_on,"Hall enabled"},{clean,"Precision"},{bass,"Bass decay"},{mid,"Mid decay"},{crossover,"Crossover"},{treble,"Treble decay"},{depth,"Depth"},{predelay,"Pre-delay"},{diffusion,"Diffusion"},{mode_enh,"Mod enhancement"},{decay_opt,"Decay optimization"},{mod_rate,"Mod rate"},{mod_depth,"Mod depth"},{opt_response,"Opt response"}}){row(pair.first,pair.second,700,y);y+=step;}
    y=234;
    for(auto pair:{std::pair<unsigned,const char*>{input_pitch_db,"Input to Pitch"},{direct_hall_db,"Input to Hall"},{pitch_hall_db,"Pitch to Hall"},{hall_input_db,"Hall input trim"},{feedback_source,"Return source"},{feedback,"Return level"},{loop_delay,"Loop delay"},{feedback_invert,"Invert return"},{guard,"Feedback guard"},{align_sends,"Align Hall sends"},{limiter,"Output safety ceiling"}}){row(pair.first,pair.second,1035,y);y+=step;}
    const unsigned mixer[]={input_db,dry_level,pitch_level,hall_level,output_db};
    const char* mixer_names[]={"Input gain","Dry level","Pitch level","Hall level","Master"};
    for(unsigned i=0;i<5;++i)row(mixer[i],mixer_names[i],25+int(i)*268,751,257);
    addAndMakeVisible(status_);status_.setColour(juce::Label::textColourId,muted());status_.setFont(juce::FontOptions(12));
    setResizable(true,true);setResizeLimits(884,650,2040,1500);
    setSize(1088,800);startTimerHz(12);timerCallback();
}
PitchLabEditor::~PitchLabEditor()=default;
void PitchLabEditor::snapshot(bool a,bool store) {
    auto& target=a?processor_.snapshotA:processor_.snapshotB;
    if(store) {
        target.removeAllChildren(nullptr);target.addChild(processor_.state.copyState(),-1,nullptr);
    } else if(target.getNumChildren()) {
        const auto saved=target.getChild(0);Values v=defaults();
        for(unsigned i=0;i<count;++i)if(auto child=saved.getChildWithProperty("id",definitions[i].id);child.isValid())v[i]=float(child.getProperty("value"));
        processor_.setValues(v);
    }
}
void PitchLabEditor::timerCallback() {
    const int available=processor_.analyzerFifo.getNumReady();
    int start1,size1,start2,size2;processor_.analyzerFifo.prepareToRead(available,start1,size1,start2,size2);
    for(int n=0;n<size1+size2;++n) {
        const int at=n<size1?start1+n:start2+n-size1;
        for(unsigned c=0;c<2;++c)fft_work_[c][spectrum_frames_]=processor_.analyzerSamples[c][size_t(at)];
        if(++spectrum_frames_==2048) {
            for(unsigned c=0;c<2;++c) {
                for(unsigned i=0;i<2048;++i)fft_work_[c][i]*=.5f-.5f*std::cos(6.283185307179586f*i/2047);
                std::fill(fft_work_[c].begin()+2048,fft_work_[c].end(),0);fft_.performFrequencyOnlyForwardTransform(fft_work_[c].data());
            }
            for(unsigned i=0;i<1024;++i) {
                const float power=.5f*(fft_work_[0][i]*fft_work_[0][i]+fft_work_[1][i]*fft_work_[1][i])/(512.f*512);
                spectrum_[i]=std::max(-140.f,10*std::log10(std::max(1e-14f,power)));
            }
            spectrum_frames_=0;
        }
    }
    processor_.analyzerFifo.finishedRead(size1+size2);
    juce::String text=processor_.hallReady()?"224 v4.4 ready":"Hall bank unavailable; Pitch Only remains available";
    text+="  |  Pitch latency "+juce::String(processor_.pitchLatencyMs.load(),1)+" ms  |  Host latency "+juce::String(processor_.getLatencySamples())+" samples";
    text+="  |  Callback "+juce::String(processor_.callbackLoad.load(),1)+"%";
    if(processor_.guardTripped.load())text+="  |  GUARD TRIPPED: return muted until Clear";
    status_.setText(text,juce::dontSendNotification);repaint();
}
void PitchLabEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff111d25));g.addTransform(juce::AffineTransform::scale(layoutScale(*this)));g.setColour(ink());g.setFont(juce::FontOptions(28));g.drawText("Cineol / Pitch Lab",25,15,600,39,juce::Justification::centredLeft);
    g.setColour(muted());g.setFont(juce::FontOptions(13));g.drawText("Signalsmith reference core  /  one unchanged Hall B  /  independent sends and monitor",530,24,800,24,juce::Justification::centredRight);
    const int xs[]={35,435,835};const char* names[]={"INPUT","PITCH REFERENCE","HALL B"};
    for(unsigned i=0;i<3;++i) {g.setColour(juce::Colour(0xff213640));g.fillRoundedRectangle(float(xs[i]),119,260,54,6);g.setColour(ink());g.setFont(juce::FontOptions(15));g.drawText(names[i],xs[i],132,260,24,juce::Justification::centred);}
    const auto value=[this](unsigned i){return processor_.state.getRawParameterValue(definitions[i].id)->load();};
    const bool hall_active=value(hall_on)>.5f;
    const auto path_colour=[&](bool active){g.setColour(active?juce::Colour(0xff77cdbf):juce::Colour(0xff344b55));};
    path_colour(value(input_pitch_db)>-79.9f);g.drawArrow({295,146,430,146},1.5f,8,6);
    path_colour(hall_active && value(pitch_hall_db)>-79.9f);g.drawArrow({695,146,830,146},1.5f,8,6);
    path_colour(hall_active && value(direct_hall_db)>-79.9f);
    g.drawLine(165,119,165,109);g.drawLine(165,109,965,109);g.drawArrow({965,109,965,119},1.2f,7,5);
    g.setFont(juce::FontOptions(10));g.drawText("direct Input to Hall",710,98,155,12,juce::Justification::centred);
    const int return_source=int(value(feedback_source));
    const float return_x=return_source==2?960.f:560.f;
    const bool return_active=return_source!=0 && value(feedback)>0 && (return_source!=2 || hall_active) && !processor_.guardTripped.load();
    g.setColour(return_active?juce::Colour(0xffdcb478):juce::Colour(0xff344b55));
    g.drawLine(return_x,177,return_x,187);g.drawLine(return_x,187,410,187);g.drawLine(410,187,410,158);g.drawArrow({410,158,430,158},1.2f,7,5);
    g.setFont(juce::FontOptions(11));g.drawText(return_source==2?"Hall return":return_source==1?"Pitch return":"Return off",610,174,180,20,juce::Justification::centred);
    const char* groups[]={"PITCHER","FILTERS","LEXICON HALL B","SENDS / LOOP"};
    for(unsigned i=0;i<4;++i){const float x=float(20+i*335);g.setColour(juce::Colour(0xff182a33));g.fillRoundedRectangle(x,201,323,503,6);g.setColour(juce::Colour(0xff77cdbf));g.setFont(juce::FontOptions(15));g.drawText(groups[i],int(x)+12,205,300,25,juce::Justification::centredLeft);}
    g.setColour(muted());g.setFont(juce::FontOptions(12));g.drawFittedText("Window / overlap changes clear histories.\nOnly the selected reference configuration runs.\n\nMonitor changes preserve the routing and tails.\nRouting presets and A/B recalls start fresh histories.",33,516,298,153,juce::Justification::topLeft,8);
    g.setColour(ink());g.setFont(juce::FontOptions(15));g.drawText("OUTPUT MIXER  /  levels here do not change feedback",26,714,1000,26,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff0c151d));g.fillRoundedRectangle(25,808,628,151,5);g.setColour(muted());g.setFont(juce::FontOptions(11));g.drawText("MONITOR SPECTRUM  /  pooled stereo power, Hann 2048",35,809,600,19,juce::Justification::centredLeft);
    for(int d=-120;d<=0;d+=30){const float y=948-(-float(d)/120)*112;g.setColour(juce::Colour(0xff223641));g.drawLine(57,y,640,y);}
    juce::Path path;
    for(unsigned px=0;px<580;++px) {
        const float hz=30*std::pow(20000.f/30,px/579.f);const unsigned bin=std::clamp(unsigned(hz*2048/48000),1u,1023u);
        const float y=948-(std::clamp(spectrum_[bin],-120.f,0.f)+120)/120*112;
        if(px==0)path.startNewSubPath(57,y);else path.lineTo(57+float(px),y);
    }
    g.setColour(juce::Colour(0xff77cdbf));g.strokePath(path,juce::PathStrokeType(1.3f));
    g.setColour(muted());g.drawText("30 Hz",53,945,90,16,juce::Justification::centredLeft);g.drawText("20 kHz",560,945,80,16,juce::Justification::centredRight);
    g.drawText("NODE",674,806,155,21,juce::Justification::centredLeft);g.drawText("PEAK / RMS dBFS",1055,806,180,21,juce::Justification::centredRight);g.drawText("CLIPS",1245,806,80,21,juce::Justification::centredRight);
    for(unsigned i=0;i<node_count;++i) {
        const float peak=processor_.meters[i].peak.load(),power=processor_.meters[i].power.load();const int y=828+int(i)*16;
        g.setColour(ink());g.drawText(node_names[i],674,y,145,17,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff29414b));g.fillRect(822,y+4,225,8);g.setColour(peak>=1?juce::Colour(0xffef8d76):juce::Colour(0xff77cdbf));g.fillRect(822,y+4,int(std::clamp((db(peak)+90)/90,0.f,1.f)*225),8);
        g.setColour(muted());g.drawText(juce::String(db(peak),1)+" / "+juce::String(db(std::sqrt(power)),1),1056,y,177,17,juce::Justification::centredRight);
        g.drawText(juce::String(processor_.meters[i].clips.load()),1242,y,80,17,juce::Justification::centredRight);
    }
}
void PitchLabEditor::resized() {
    preset_.setBounds(25,68,310,30);storeA_.setBounds(730,68,90,30);recallA_.setBounds(825,68,90,30);
    storeB_.setBounds(930,68,90,30);recallB_.setBounds(1025,68,90,30);panic_.setBounds(1160,68,175,30);
    status_.setBounds(24,971,1312,24);
    const auto transform=juce::AffineTransform::scale(layoutScale(*this));
    for(auto& row:rows_)row->setTransform(transform);
    preset_.setTransform(transform);status_.setTransform(transform);
    for(auto* b:{&panic_,&storeA_,&recallA_,&storeB_,&recallB_})b->setTransform(transform);
}
