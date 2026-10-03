#include "Editor.hpp"
#include "CineolUIData.h"
#include <algorithm>

namespace {
constexpr int panel_width=1470,panel_height=1070;
constexpr const char* captions[]={"BASS","MID","CROSSOVER","TREBLE","DEPTH","PRE-DELAY",
    "DIFFUSION","INPUT GAIN","DRY / WET"};
const juce::Colour ink(0xff171612),led(0xffff4035);
juce::Font font(float height,bool bold=false) {
    return juce::Font(juce::FontOptions("Arial",height,bold?juce::Font::bold:juce::Font::plain));
}
// Only the graphics thread loads/decodes these embedded assets.
juce::Image loadImage(const char* name) {
    int size=0;auto* data=CineolUIData::getNamedResource(name,size);
    return juce::ImageFileFormat::loadFrom(data,size_t(size));
}
juce::Image trimTransparentPadding(const juce::Image& source) {
    if(!source.isValid()) return {};
    int left=source.getWidth(),top=source.getHeight(),right=0,bottom=0;
    juce::Image::BitmapData pixels(source,juce::Image::BitmapData::readOnly);
    for(int y=0;y<source.getHeight();++y) for(int x=0;x<source.getWidth();++x)
        if(pixels.getPixelColour(x,y).getAlpha()>32) {
            left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);
        }
    return right>=left && bottom>=top?source.getClippedImage({left,top,right-left+1,bottom-top+1}):source;
}
class InstrumentLook final : public juce::LookAndFeel_V4 {
public:
    InstrumentLook():cap_(trimTransparentPadding(loadImage("fadercap_png"))) {
        setColour(juce::ComboBox::textColourId,juce::Colour(0xfff1ece0));
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff25231f));
        setColour(juce::PopupMenu::textColourId,juce::Colour(0xfff1ece0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff6d2c28));
        setColour(juce::PopupMenu::highlightedTextColourId,juce::Colours::white);
        setColour(juce::TooltipWindow::backgroundColourId,juce::Colour(0xffece7db));
        setColour(juce::TooltipWindow::textColourId,ink);
    }
    int getSliderThumbRadius(juce::Slider&) override {return 33;}
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& s) override {
        juce::Slider::SliderLayout l;
        l.sliderBounds={0,33,s.getWidth(),s.getHeight()-116};
        l.textBoxBounds={0,s.getHeight()-44,s.getWidth(),40};return l;
    }
    juce::Label* createSliderTextBox(juce::Slider&) override {
        auto* label=new juce::Label;
        label->setFont(font(27));label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId,ink);
        label->setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
        label->setColour(juce::Label::outlineColourId,juce::Colours::transparentBlack);
        label->setColour(juce::Label::textWhenEditingColourId,ink);
        label->setColour(juce::Label::backgroundWhenEditingColourId,juce::Colour(0xfff4efe4));
        label->setColour(juce::Label::outlineWhenEditingColourId,juce::Colour(0xff765e4d));
        return label;
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float,float,
                          juce::Slider::SliderStyle,juce::Slider& s) override {
        const float cx=float(x)+float(width)/2;
        g.setOpacity(s.isEnabled()?1.0f:0.35f);
        juce::Rectangle<float> rail(cx-10,float(y-33),20,float(height+66));
        g.setColour(juce::Colour(0xff5c5951));g.fillRoundedRectangle(rail.expanded(3),5);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff090907),cx-10,0,
                                              juce::Colour(0xff37342d),cx+10,0,false));
        g.fillRoundedRectangle(rail,4);
        g.setColour(juce::Colour(0xffddd7c9));g.drawRoundedRectangle(rail.expanded(3),5,1);
        for(int i=0;i<=20;++i) {
            const float ty=float(y)+float(height)*float(i)/20;
            constexpr float length=11;
            const bool major=i%10==0;
            const float thickness=major?3.0f:1.0f;
            g.setColour(ink.withAlpha(major?0.75f:0.5f));
            g.fillRect(cx-23-length,ty-thickness/2,length,thickness);
            g.fillRect(cx+23,ty-thickness/2,length,thickness);
        }
        juce::Rectangle<float> cap(cx-26,pos-33,52,66);
        juce::Path shadow;shadow.addRoundedRectangle(cap,3);
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f),9,{4,6}).drawForPath(g,shadow);
        g.setOpacity(s.isEnabled()?1.0f:0.4f);
        g.drawImage(cap_,cap,juce::RectanglePlacement::stretchToFit);
    }
    void drawComboBox(juce::Graphics& g,int width,int height,bool down,int,int,int,int,juce::ComboBox& c) override {
        auto r=juce::Rectangle<float>(1,1,float(width-2),float(height-2));
        g.setColour(juce::Colour(0xff5d5a52));g.fillRoundedRectangle(r,5);
        g.setColour(juce::Colour(0xffded8c8));g.drawRoundedRectangle(r,5,1.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(down?0xff151411:0xff34312a),0,4,
                                             juce::Colour(0xff1b1915),0,float(height),false));
        g.fillRoundedRectangle(r.reduced(4),3);
        if(c.hasKeyboardFocus(true)) {g.setColour(juce::Colour(0xff9f4d3d));g.drawRoundedRectangle(r.reduced(1),4,2);}
        juce::Path arrow;float ax=float(width)-32,ay=float(height)/2;
        arrow.addTriangle(ax-8,ay-4,ax+8,ay-4,ax,ay+5);
        g.setColour(juce::Colour(0xffe7e0d2));g.fillPath(arrow);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override {return font(28);}
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override {
        label.setBounds(18,1,box.getWidth()-70,box.getHeight()-2);label.setFont(getComboBoxFont(box));
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool highlighted,bool down) override {
        juce::Rectangle<float> square(3,float(b.getHeight()-50)/2,50,50);
        g.setColour(juce::Colour(0xff545148));g.fillRoundedRectangle(square,5);
        g.setColour(juce::Colour(highlighted?0xfff6efdd:0xffd8d0bc));g.drawRoundedRectangle(square.reduced(2),4,1.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(down?0xff141310:0xff333029),0,square.getY(),
                                              juce::Colour(0xff1c1a16),0,square.getBottom(),false));
        g.fillRoundedRectangle(square.reduced(5),2);
        if(b.getToggleState()) {
            juce::Path tick;tick.startNewSubPath(15,square.getY()+27);tick.lineTo(25,square.getY()+35);
            tick.lineTo(41,square.getY()+16);g.setColour(juce::Colour(0xfff2e8d0));
            g.strokePath(tick,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        }
        if(b.hasKeyboardFocus(true)) {g.setColour(juce::Colour(0xff873d32));g.drawRoundedRectangle(square.expanded(2),5,2);}
        g.setColour(ink);g.setFont(font(26));
        g.drawText(b.getButtonText(),76,0,b.getWidth()-78,b.getHeight(),juce::Justification::centredLeft);
    }
private:
    juce::Image cap_;
};
// 5x7 LED alphabet; UI-only, so the native DSP has no display work to do.
const char* glyph(char c) {
    constexpr const char* letters[]={"0E11111F111111","1E11111E11111E","0F10101010100F","1E11111111111E",
        "1F10101E10101F","1F10101E101010","0F10101711110F","1111111F111111","0E04040404040E",
        "0702020212120C","11121418141211","1010101010101F","111B1515111111","11191513111111",
        "0E11111111110E","1E11111E101010","0E11111115120D","1E11111E141211","0F10100E01011E",
        "1F040404040404","1111111111110E","11111111110A04","11111115151B11","11110A040A1111",
        "11110A04040404","1F01020408101F"};
    constexpr const char* digits[]={"0E11131519110E","040C040404040E","0E11010204081F","1E01010E01011E",
        "02060A121F0202","1F10101E01011E","0E10101E11110E","1F010204080808","0E11110E11110E","0E11110F01010E"};
    if(c>='A' && c<='Z') return letters[c-'A'];
    if(c>='0' && c<='9') return digits[c-'0'];
    switch(c) {
        case '-':return "0000001F000000";case '.':return "00000000000C0C";
        case '%':return "19190204081313";case '/':return "01010204081010";
        default:return "00000000000000";
    }
}
void dotText(juce::Graphics& g,const juce::String& text,juce::Rectangle<float> area,float maxPitch,bool centred) {
    const float pitch=std::min({maxPitch,area.getHeight()/7,area.getWidth()/float(std::max(1,text.length()*6-1))});
    const float textWidth=float(text.length()*6-1)*pitch;
    float x=centred?area.getCentreX()-textWidth/2:area.getX();
    const float y=area.getCentreY()-3.5f*pitch;
    auto hex=[](char n){return n<='9'?n-'0':n-'A'+10;};
    for(auto c:text.toUpperCase()) {
        const char* bits=glyph(char(c));
        for(int row=0;row<7;++row) {
            unsigned mask=unsigned(hex(bits[row*2])*16+hex(bits[row*2+1]));
            for(int col=0;col<5;++col) if(mask&(1u<<(4-col))) {
                const float dx=x+float(col)*pitch,dy=y+float(row)*pitch;
                g.setColour(led.withAlpha(0.12f));g.fillEllipse(dx-pitch*0.24f,dy-pitch*0.24f,pitch*1.1f,pitch*1.1f);
                g.setColour(led);g.fillEllipse(dx,dy,pitch*0.58f,pitch*0.58f);
            }
        }
        x+=6*pitch;
    }
}
void digit(juce::Graphics& g,int value,float x,float y) {
    constexpr unsigned masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    std::array<juce::Path,7> shapes;
    // One symmetric grid: 12-wide bars, 45-degree ends, 2*sqrt(2) gaps.
    // Both halves share the same columns; no per-segment slant or offsets.
    constexpr float half=6,gap=2,left=6,right=66,pitch=52;
    for(unsigned segment=0;segment<shapes.size();++segment) {
        auto& shape=shapes[segment];
        auto horizontal=[&](float centreY){
            const float start=left+gap,end=right-gap;
            shape.startNewSubPath(x+start,y+centreY);shape.lineTo(x+start+half,y+centreY-half);
            shape.lineTo(x+end-half,y+centreY-half);shape.lineTo(x+end,y+centreY);
            shape.lineTo(x+end-half,y+centreY+half);shape.lineTo(x+start+half,y+centreY+half);
            shape.closeSubPath();};
        auto vertical=[&](float centreX,float centreY){
            const float start=centreY+gap,end=centreY+pitch-gap;
            shape.startNewSubPath(x+centreX,y+start);shape.lineTo(x+centreX+half,y+start+half);
            shape.lineTo(x+centreX+half,y+end-half);shape.lineTo(x+centreX,y+end);
            shape.lineTo(x+centreX-half,y+end-half);shape.lineTo(x+centreX-half,y+start+half);
            shape.closeSubPath();};
        switch(segment) {
            case 0:horizontal(half);break;case 1:vertical(right,half);break;
            case 2:vertical(right,half+pitch);break;case 3:horizontal(half+2*pitch);break;
            case 4:vertical(left,half+pitch);break;case 5:vertical(left,half);break;
            case 6:horizontal(half+pitch);break;
        }
    }
    // Paint the restrained bloom first, then the crisp cores of all segments.
    for(unsigned segment=0;segment<shapes.size();++segment)
        if(masks[value]&(1u<<segment))
            juce::DropShadow(led.withAlpha(0.22f),3,{0,0}).drawForPath(g,shapes[segment]);
    for(unsigned segment=0;segment<shapes.size();++segment) {
        const bool active=(masks[value]&(1u<<segment))!=0;
        g.setColour(active?led:led.withAlpha(0.065f));g.fillPath(shapes[segment]);
    }
}
class Display final : public juce::Component {
public:
    Display(){setComponentID("display");setTitle("Algorithm and parameter display");}
    void update(int program,const juce::String& name,const juce::String& parameter,const juce::String& value) {
        if(program==program_ && name==name_ && parameter==parameter_ && value==value_) return;
        program_=program;name_=name;parameter_=parameter;value_=value;
        setName(juce::String(program+1).paddedLeft('0',2)+" | "+name+" | "+parameter+" "+value);repaint();
    }
    void paint(juce::Graphics& g) override {
        digit(g,(program_+1)/10,62,5);digit(g,(program_+1)%10,152,5);
        g.setColour(led.withAlpha(0.3f));g.drawVerticalLine(287,10,115);
        dotText(g,name_,{320,28,720,70},9,true);
        dotText(g,parameter_,{1100,15,190,26},3,false);
        dotText(g,value_,{1100,52,190,54},7,false);
    }
private:
    int program_=2;
    juce::String name_,parameter_,value_;
};

class SettingsButton final : public juce::Button {
public:
    SettingsButton():Button("Settings") {
        setComponentID("settings");setTitle("Settings");setClickingTogglesState(true);
        setTooltip("Open settings and fine tuning.");
    }
    void paintButton(juce::Graphics& g,bool highlighted,bool down) override {
        auto bounds=getLocalBounds().toFloat().reduced(5);
        if(highlighted || getToggleState()) {
            g.setColour(ink.withAlpha(0.08f));g.fillRoundedRectangle(bounds,8);
        }
        juce::Path gear;const auto centre=bounds.getCentre();
        for(int i=0;i<32;++i) {
            const float angle=juce::MathConstants<float>::twoPi*float(i)/32;
            const float radius=(i%4==1 || i%4==2)?19.0f:15.0f;
            const auto point=centre+juce::Point<float>(std::sin(angle)*radius,std::cos(angle)*radius);
            if(i==0) gear.startNewSubPath(point);else gear.lineTo(point);
        }
        gear.closeSubPath();gear.addEllipse(centre.x-7,centre.y-7,14,14);
        gear.setUsingNonZeroWinding(false);
        g.setColour(getToggleState() || down?juce::Colour(0xff873d32):ink);g.fillPath(gear);
        if(hasKeyboardFocus(true)) {g.setColour(ink);g.drawRoundedRectangle(bounds,8,1.5f);}
    }
};

class SettingsPanel final : public juce::Component {
public:
    explicit SettingsPanel(NativeHallProcessor& processor) {
        setComponentID("settings_panel");setName("Settings");setWantsKeyboardFocus(true);
        low_latency_.setComponentID("low_latency");low_latency_.setButtonText("Low latency");
        low_latency_.setTitle("Low latency");
        low_latency_.setTooltip("Direct dry signal with zero reported plugin latency. Reverb still passes through its filters and pre-delay.");
        addAndMakeVisible(low_latency_);
        attachment_=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.state,"low_latency",low_latency_);
    }
    void paint(juce::Graphics& g) override {
        auto bounds=getLocalBounds().toFloat().reduced(4);
        g.setColour(juce::Colour(0xffe9e3d6));g.fillRoundedRectangle(bounds,12);
        g.setColour(ink.withAlpha(0.3f));g.drawRoundedRectangle(bounds,12,2);
        g.setColour(ink);g.setFont(font(30,true));
        g.drawText("Settings",28,20,getWidth()-56,38,juce::Justification::centredLeft);
        g.setFont(font(22));
        g.drawText("When enabled: zero-latency dry signal.",28,148,getWidth()-56,30,juce::Justification::centredLeft);
        g.drawText("Reverb and pre-delay keep their timing.",28,178,getWidth()-56,30,juce::Justification::centredLeft);
        g.setColour(ink.withAlpha(0.16f));g.drawHorizontalLine(229,28,float(getWidth()-28));
        // Space below this divider is reserved for future fine-tuning rows.
    }
    void resized() override {low_latency_.setBounds(28,75,getWidth()-56,64);}
private:
    juce::ToggleButton low_latency_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

class RomSetup final : public juce::Component,private juce::Timer {
public:
    explicit RomSetup(NativeHallProcessor& processor):processor_(processor),progress_bar_(progress_) {
        setLookAndFeel(&look_);
        setComponentID("rom_setup");setName("Import original Lexicon 224 ROMs");
        title_.setText("Load your Lexicon 224 ROMs",juce::dontSendNotification);
        title_.setFont(font(38,true));title_.setJustificationType(juce::Justification::centred);
        instructions_.setText("Original 224 v4.4: ROM1-ROM5\nSelect a folder, ZIP, or the five ROM files.\nYour files stay on this computer.",juce::dontSendNotification);
        instructions_.setFont(font(25));instructions_.setJustificationType(juce::Justification::centred);
        status_.setComponentID("rom_status");status_.setFont(font(22));
        status_.setJustificationType(juce::Justification::centred);
        for(auto* label:{&title_,&instructions_,&status_}) {
            label->setColour(juce::Label::textColourId,ink);addAndMakeVisible(*label);
        }
        choose_.setComponentID("choose_roms");choose_.setButtonText("Choose ROMs...");
        choose_.setColour(juce::TextButton::buttonColourId,ink);
        choose_.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
        choose_.setTooltip("Only original Lexicon 224 v4.4 is supported. 224X and 224XL ROMs are rejected.");
        choose_.onClick=[this]{chooseRoms();};addAndMakeVisible(choose_);
        cancel_.setComponentID("cancel_rom_import");cancel_.setButtonText("Cancel import");
        cancel_.setColour(juce::TextButton::buttonColourId,ink);
        cancel_.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
        cancel_.onClick=[this]{processor_.cancelRomImport();};addAndMakeVisible(cancel_);
        addAndMakeVisible(progress_bar_);update();startTimerHz(10);
    }
    ~RomSetup() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black.withAlpha(0.64f));
        g.setColour(juce::Colour(0xffe4dfd2));g.fillRoundedRectangle(245,285,980,500,18);
        g.setColour(ink.withAlpha(0.25f));g.drawRoundedRectangle(245,285,980,500,18,2);
    }
    void resized() override {
        title_.setBounds(285,315,900,60);instructions_.setBounds(285,390,900,115);
        status_.setBounds(290,520,890,95);progress_bar_.setBounds(365,625,740,25);
        choose_.setBounds(515,680,440,62);cancel_.setBounds(515,680,440,62);
    }
private:
    struct SetupLook final : juce::LookAndFeel_V4 {
        void drawButtonText(juce::Graphics& g,juce::TextButton& button,bool,bool) override {
            g.setColour(button.findColour(juce::TextButton::textColourOffId).withAlpha(button.isEnabled()?1.0f:0.5f));
            g.setFont(font(25,true));
            g.drawText(button.getButtonText(),button.getLocalBounds(),juce::Justification::centred);
        }
    } look_;
    void chooseRoms() {
        chooser_open_=true;
        chooser_=std::make_unique<juce::FileChooser>("Choose original Lexicon 224 v4.4 ROM1-ROM5",juce::File{},"*");
        choose_.setEnabled(false);
        chooser_->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|
            juce::FileBrowserComponent::canSelectDirectories|juce::FileBrowserComponent::canSelectMultipleItems,
            [safe=juce::Component::SafePointer<RomSetup>(this)](const juce::FileChooser& chooser) {
                if(!safe) return;
                safe->chooser_open_=false;
                const auto files=chooser.getResults();
                if(!files.isEmpty()) safe->processor_.importRoms(files);
                safe->update();
            });
    }
    void update() {
        const bool busy=processor_.importingRoms();progress_=processor_.importProgress();
        status_.setText(processor_.romStatus(),juce::dontSendNotification);
        choose_.setVisible(!busy);choose_.setEnabled(!busy && !processor_.ready() && !chooser_open_);
        cancel_.setVisible(busy);progress_bar_.setVisible(busy);
    }
    void timerCallback() override {update();}
    NativeHallProcessor& processor_;
    juce::Label title_,instructions_,status_;
    juce::TextButton choose_,cancel_;
    double progress_=0;
    juce::ProgressBar progress_bar_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool chooser_open_=false;
};
}

struct CineolEditor::Panel final : public juce::Component,private juce::Timer {
    explicit Panel(NativeHallProcessor& processor):processor_(processor),background_(loadImage("panel_png")),rom_setup_(processor),settings_panel_(processor) {
        setLookAndFeel(&look_);setSize(panel_width,panel_height);addAndMakeVisible(display_);
        display_.setBounds(88,100,1294,125);
        for(unsigned i=0;i<sliders_.size();++i) {
            auto& slider=sliders_[i];auto* param=processor_.state.getParameter(NativeHallProcessor::ids[i]);
            slider.setLookAndFeel(&look_);
            slider.setColour(juce::Slider::textBoxTextColourId,ink);
            slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
            slider.setComponentID(NativeHallProcessor::ids[i]);slider.setName(param->getName(40));
            slider.setTitle(param->getName(40));slider.setSliderStyle(juce::Slider::LinearVertical);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,140,40);
            slider.setTextValueSuffix(param->getLabel().isEmpty()?juce::String{}:" "+param->getLabel());
            slider.setSliderSnapsToMousePosition(false);
            slider.setTooltip(param->getName(40)+": drag, use arrow keys, or type a value. Double-click resets.");
            addAndMakeVisible(slider);slider.setBounds(51+int(i)*153,329,140,424);
            slider_attachments_[i]=std::make_unique<SliderAttachment>(processor_.state,NativeHallProcessor::ids[i],slider);
            slider.onDragStart=[this,i]{focused_=i;refreshDisplay();};
            slider.onValueChange=[this,i]{focused_=i;refreshDisplay();};
        }
        algorithm_.setComponentID("algorithm");algorithm_.setName("Algorithm");algorithm_.setTitle("Algorithm");
        for(int i=0;i<processor_.getNumPrograms();++i) algorithm_.addItem(processor_.getProgramName(i),i+1);
        algorithm_.setTooltip("Select algorithm. Changing algorithm clears the reverb tail; other controls keep their positions.");
        addAndMakeVisible(algorithm_);algorithm_.setBounds(98,857,601,60);
        algorithm_attachment_=std::make_unique<ComboAttachment>(processor_.state,"algorithm",algorithm_);
        algorithm_.onChange=[this]{refreshProgram();};
        for(unsigned i=0;i<outputs_.size();++i) {
            const char* id=NativeHallProcessor::ids[12+i];auto& output=outputs_[i];
            output.setComponentID(id);output.setName(i==0?"Left Output":"Right Output");output.setTitle(output.getName());
            output.addItemList(juce::StringArray{"A","B","C","D"},1);addAndMakeVisible(output);
            output.setBounds(i==0?786:1108,857,i==0?253:261,60);
            output_attachments_[i]=std::make_unique<ComboAttachment>(processor_.state,id,output);
        }
        for(unsigned i=0;i<toggles_.size();++i) {
            auto& button=toggles_[i];button.setComponentID(i==0?"digital_dirt":NativeHallProcessor::ids[9+i]);
            button.setButtonText(i==0?"Digital Dirt":processor_.state.getParameter(NativeHallProcessor::ids[9+i])->getName(40));
            button.setName(button.getButtonText());button.setTitle(button.getButtonText());
            addAndMakeVisible(button);button.setBounds(150+int(i)*425,951,i==0?320:400,70);
            if(i>0) button_attachments_[i-1]=std::make_unique<ButtonAttachment>(processor_.state,NativeHallProcessor::ids[9+i],button);
        }
        toggles_[0].setTooltip("Digital Dirt ON: digital bypass with aliasing and coarser quantization. OFF: filtered path.");
        // An inverse UI attachment preserves the old 'analog' parameter ID,
        // normalized automation, defaults, and all existing saved sessions.
        dirt_attachment_=std::make_unique<juce::ParameterAttachment>(*processor_.state.getParameter("analog"),
            [this](float clean){toggles_[0].setToggleState(clean<0.5f,juce::dontSendNotification);});
        dirt_attachment_->sendInitialUpdate();
        toggles_[0].onClick=[this]{dirt_attachment_->setValueAsCompleteGesture(toggles_[0].getToggleState()?0.0f:1.0f);};
        addChildComponent(rom_setup_);rom_setup_.setBounds(0,0,panel_width,panel_height);
        addChildComponent(settings_panel_);settings_panel_.setBounds(820,76,550,315);
        addAndMakeVisible(settings_button_);settings_button_.setBounds(1310,12,64,52);
        settings_button_.onClick=[this] {
            settings_panel_.setVisible(settings_button_.getToggleState());
            if(settings_panel_.isVisible()) {settings_panel_.toFront(false);settings_panel_.grabKeyboardFocus();}
        };
        refreshProgram();startTimerHz(30);
    }
    ~Panel() override {stopTimer();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {
        g.drawImage(background_,getLocalBounds().toFloat(),juce::RectanglePlacement::stretchToFit);
        g.setColour(ink);g.setFont(font(40,true));g.drawText("Cineol-X 224",84,17,800,44,juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions("Arial Narrow",23,juce::Font::bold)));
        for(unsigned i=0;i<sliders_.size();++i) {
            g.setColour(ink.withAlpha(sliders_[i].isEnabled()?1.0f:0.38f));
            g.drawText(captions[i],51+int(i)*153,275,140,35,juce::Justification::centred);
        }
        g.setColour(ink.withAlpha(0.55f));g.drawVerticalLine(1113,278,752);
        g.setColour(ink);g.drawText("ALGORITHM",98,820,600,34,juce::Justification::centredLeft);
        g.drawText("LEFT",786,820,250,34,juce::Justification::centredLeft);
        g.drawText("RIGHT",1108,820,260,34,juce::Justification::centredLeft);
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress::escapeKey && settings_panel_.isVisible()) {
            settings_panel_.setVisible(false);settings_button_.setToggleState(false,juce::dontSendNotification);
            settings_button_.grabKeyboardFocus();return true;
        }
        return false;
    }
private:
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void refreshDisplay() {
        auto* parameter=processor_.state.getParameter(NativeHallProcessor::ids[focused_]);
        auto value=parameter->getText(parameter->convertTo0to1(float(sliders_[focused_].getValue())),0);
        if(parameter->getLabel().isNotEmpty()) value+=" "+parameter->getLabel();
        const int program=processor_.getCurrentProgram();
        display_.update(program,native_hall::program_names[program],captions[focused_],value);
    }
    void refreshProgram() {
        const int program=processor_.getCurrentProgram();
        const bool ready=processor_.ready();
        rom_setup_.setVisible(!ready);
        for(unsigned i=0;i<sliders_.size();++i) sliders_[i].setEnabled(ready && (i!=6 || program!=3));
        algorithm_.setEnabled(ready);
        for(auto& output:outputs_) output.setEnabled(ready);
        for(auto& toggle:toggles_) toggle.setEnabled(ready);
        if(program!=displayed_program_) {
            displayed_program_=program;sliders_[5].updateText();
            sliders_[6].setTooltip(program==3?"Acoustic Chamber does not use Diffusion.":
                "Diffusion: drag, use arrow keys, or type a value. Double-click resets.");
            repaint();
        }
        refreshDisplay();
    }
    void timerCallback() override {refreshProgram();}
    NativeHallProcessor& processor_;
    juce::Image background_;
    InstrumentLook look_;
    juce::TooltipWindow tooltips_{this,650};
    Display display_;
    std::array<juce::Slider,9> sliders_;
    juce::ComboBox algorithm_;
    std::array<juce::ComboBox,2> outputs_;
    std::array<juce::ToggleButton,3> toggles_;
    RomSetup rom_setup_;
    SettingsButton settings_button_;
    SettingsPanel settings_panel_;
    std::array<std::unique_ptr<SliderAttachment>,9> slider_attachments_;
    std::unique_ptr<ComboAttachment> algorithm_attachment_;
    std::array<std::unique_ptr<ComboAttachment>,2> output_attachments_;
    std::array<std::unique_ptr<ButtonAttachment>,2> button_attachments_;
    std::unique_ptr<juce::ParameterAttachment> dirt_attachment_;
    unsigned focused_=5;
    int displayed_program_=-1;
};

CineolEditor::CineolEditor(NativeHallProcessor& processor):AudioProcessorEditor(processor),panel_(std::make_unique<Panel>(processor)) {
    addAndMakeVisible(*panel_);setResizable(true,true);setResizeLimits(882,642,1470,1070);
    getConstrainer()->setFixedAspectRatio(double(panel_width)/panel_height);setSize(1029,749);
}
CineolEditor::~CineolEditor()=default;
void CineolEditor::resized() {
    panel_->setTransform(juce::AffineTransform::scale(float(getWidth())/panel_width,float(getHeight())/panel_height));
}
