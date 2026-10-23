#pragma once
#include "Processor.hpp"

// UI-only explanations. See docs/Parameter_Help_2026-10-11.md for the source
// audit and program/page coverage. Firmware captions identify the visible role;
// prepared control metadata determines availability and implementation variants.
namespace cineol::help {
inline juce::String tip(const juce::String& title,const juce::String& body) {return title+"\n"+body;}
inline juce::String unavailable(const juce::String& title) {
    return tip(title,"This control is unavailable in the current program. Moving it does not change the sound in Cineol-X.");
}
inline juce::String global(const juce::String& id) {
    if(id=="input_db") return tip("Input","Sets the level feeding the effect. More gain drives the reverb or delay harder and can change its level-dependent response.");
    if(id=="mix") return tip("Dry / Wet","Blends the original signal with the effect. Use 100% wet when the plugin is on an auxiliary return.");
    if(id=="analog" || id=="dirt") return tip("Dirt","Opens the input and output filter bandwidths. At 100% the modeled filters are bypassed while converter quantization remains. The final part of the range removes the input reconstruction filter.");
    if(id=="space") return tip("Space","Raise Space to slow the selected algorithm's DSP clock from its nominal rate to exactly half. Delays and modulation periods grow. The screen shows the actual DSP rate in kHz.");
    if(id=="filters_follow_clock") return tip("Analog filters follow DSP rate","On: Space scales analog filter frequencies with the DSP clock. Off: analog frequencies stay fixed as Space changes. Dirt opens the filters independently in either mode.");
    if(id=="low_latency") return tip("Low latency","Uses a direct dry path and reports zero plugin latency. The wet effect keeps its own processing delay and pre-delay.");
    if(id=="spillover") return tip("Spillover","Keeps the previous program's tail when switching algorithms. That tail fades out while the new program receives the input.");
    if(id=="spillover_time") return tip("Spillover time","Sets how long the previous program's tail fades during a switch. The new duration applies to the next transition.");
    if(id=="display_brightness") return tip("Display brightness","Adjusts the brightness of text and indicators on the red screen. The upper part of the range adds a little extra light to the LED cores.");
    if(id=="display_emphasis") return tip("Option illumination","Adjusts the illuminated outlines and emphasis of screen buttons. At zero the added illumination is removed.");
    if(id=="compact_mode") return tip("Compact display mode","Shows the screen in a shorter housing and hides the physical faders. Drag the screen values vertically to adjust them; hold Shift for finer changes.");
    return {};
}
inline juce::String original(unsigned program,const juce::String& id) {
    if(auto text=global(id);text.isNotEmpty()) return text;
    if(id=="bass") return tip("Bass decay","Sets how long low frequencies reverberate below Crossover. A longer setting keeps more warmth in the tail.");
    if(id=="mid") return tip("Mid decay","Sets how long frequencies above Crossover reverberate. Treble Decay can make the highest frequencies fade faster.");
    if(id=="crossover") return tip("Crossover","Sets the frequency separating the Bass and Mid decay regions. Raise it to let the Bass setting affect more of the spectrum.");
    if(id=="treble") return tip("Treble decay","Sets the frequency above which the reverb dies away more quickly. Lower values give a darker tail.");
    if(id=="depth") return tip("Depth","Changes the balance of early and later reverberation to suggest distance within the space. Higher values place the sound farther away.");
    if(id=="predelay") return tip("Pre-delay","Sets the delay before the reverberation starts. The minimum delay depends on the selected 224 program.");
    if(id=="diffusion") return program==3?unavailable("Diffusion"):
        tip("Diffusion","Changes how the reflections build into a continuous reverb texture. Its response varies with the program, so the highest value is not always the smoothest setting.");
    return {};
}
inline juce::String route(const juce::String& name) {
    const auto marker=name.indexOfChar(')');
    if(marker<0) return {};
    const auto outputs=name.substring(marker+1).trim();
    const auto input=name.substring(0,marker).trimEnd();
    const auto source=input.endsWith("L+R")?"both inputs":input.endsWith("L")?"the left input":input.endsWith("R")?"the right input":"";
    if(juce::String(source).isEmpty()) return {};
    if(outputs.isEmpty()) return " Uses "+juce::String(source)+".";
    return " Routes "+juce::String(source)+" to output"+(outputs.length()>1?"s ":" ")+outputs+".";
}
inline juce::String xl(unsigned program,const cineol::xl::ProgramData& data,unsigned page_index,unsigned slot_index) {
    if(page_index>=data.page_count || slot_index>=6) return {};
    const auto& page=data.pages[page_index];const unsigned cell=page.cells[slot_index];
    if(cell>=48) return unavailable("Unused slot");
    const juce::String title(page.names[slot_index].data());
    if(!data.control_active(cell)) return unavailable(title);
    auto name=title.toUpperCase().removeCharacters(" ");
    juce::String body;
    const bool split=program>=23;
    const bool inverse=program==15;
    if(name.startsWith("LFSTOP") || name.startsWith("LFSTP"))
        body="Sets the low-frequency tail decay used after the input level falls. It takes effect when Dynamic Decay is on.";
    else if(name.startsWith("MIDSTOP") || name.startsWith("MIDSTP"))
        body=inverse?"Sets the stopped-state decay reference used when Dynamic Decay is on. In Inverse Room it acts through the low-frequency feedback and the decay controller.":
            "Sets the mid-frequency tail decay used after the input level falls. It takes effect when Dynamic Decay is on.";
    else if(name.startsWith("LFDECAY")) body=inverse?
        "Adjusts low-frequency feedback in the inverse envelope. Use it together with Decay Slope and Size to shape the effect.":
        "Sets the decay of frequencies below Crossover. Longer values keep more low-frequency energy in the reverb.";
    else if(name.startsWith("MIDDECAY"))
        body="Sets the main reverb decay above Crossover. Treble Decay controls how quickly the highest frequencies fade.";
    else if(name.startsWith("CROSSOVER"))
        body="Sets the transition frequency in the reverb's low-frequency feedback filtering. Where LF Decay is available, it separates the LF and Mid decay regions.";
    else if(name.startsWith("TREBLEDECAY") || name.startsWith("TREBDECAY"))
        body="Sets the frequency above which the reverb fades more quickly. Lower values darken the tail.";
    else if(name.startsWith("DEPTH"))
        body="Changes the balance of early and later sound to shape the apparent position within the space. Its character depends on the selected room or hall.";
    else if(name.startsWith("ATTACK"))
        body="Shapes the initial burst of reverberation and its buildup. Adjust it to change how sharply or gradually the effect begins.";
    else if(name.startsWith("PREDELAY")) body=page.variable_predelay[slot_index]?
        "Delays the start of the reverberation. Changing Size also changes its available range and the displayed minimum delay.":
        "Delays the start of the reverberation. Longer values separate the effect from the initial sound.";
    else if(name.startsWith("CHORUS"))
        body="Sets the speed of the program's delay modulation. Higher values produce faster movement; Mode Enhancement must be on to hear it.";
    else if(name.startsWith("HFBANDWIDTH") || name.startsWith("HFBNDWTH"))
        body="Sets the high-frequency limit of the signal entering the effect. Lower values soften both early reflections and the reverberation.";
    else if(name.startsWith("DIFFUSION")) {
        if(program==22) body="Spreads the input transient before it reaches the delay bands. Higher values turn a sharp hit into a more spread-out texture.";
        else if(program==20) body="Spreads each echo into a less sharply defined sound. Use lower settings for distinct repeats and higher settings for a softer texture.";
        else if(program==17 || program==18) body="Sets the initial density of the plate's reflections. These CD programs keep their characteristic constant density after the initial buildup.";
        else body="Controls the initial buildup of reflection density. Low values leave early reflections more distinct; higher values blend them sooner.";
    } else if(name.startsWith("DEFINITION") || name.startsWith("DEFINIT"))
        body="Reduces the buildup of density in the later reverb as it is raised. Higher settings make repeating echoes more distinct, with the result also depending on decay.";
    else if(name.startsWith("DECAYSLOPE"))
        body="Shapes the Inverse Room envelope from falling through level to rising. Size controls the duration before its abrupt ending.";
    else if(name.startsWith("SIZE"))
        body="Scales the internal delay network to change the size of the space. This changes reflection spacing, the decay-time scale and, in applicable programs, the pre-delay range.";
    else if(name.startsWith("REVSTOPDLY"))
        body="Sets the hold time used when switching between running and stopped decay. It affects the response only with Dynamic Decay enabled.";
    else if(name.startsWith("NOTEPITCH"))
        body="Tunes the resonant voice by changing its short loop delay, shown in milliseconds. A longer delay lowers the resonant pitch.";
    else if(name.startsWith("RESONANCE"))
        body="Sets the feedback of this resonant voice. Larger positive or negative amounts make it ring longer; the sign changes the harmonic pattern.";
    else if(name.startsWith("CROSSFEED"))
        body=slot_index==0?"Feeds delayed voices back to their own input sides. Raising it builds additional repeats and interactions between the resonant voices.":
            "Feeds delayed voices back to the opposite input sides. Raising it adds interaction between the left and right groups of resonant voices.";
    else if(name.startsWith("LFCUTOFF"))
        body="Sets the low-frequency edge of this delay band. Raising it removes more bass from that band's delayed sound.";
    else if(name.startsWith("HFCUTOFF"))
        body=program==22?"Sets the high-frequency edge of this delay band. Lower values make that band's delayed sound darker.":
            "Filters high frequencies in the feedback on the indicated input side. Lower values make successive resonant repeats darker.";
    else if(name.startsWith("FEEDBACK")) {
        if(program==22) body="Returns the corresponding delay output to the common input. Higher values build more repeats across the delay bands.";
        else body="Returns part of this voice's output to its delay loop. Larger positive or negative amounts give more repeats; negative feedback reverses the return polarity.";
    } else if(name.startsWith("PAN"))
        body="Positions this voice or band between output groups CB and AD. Higher values move it toward AD; the audible stereo position depends on your selected outputs.";
    else if(name.startsWith("FINPDLY"))
        body="Fine-adjusts the pre-delay on the indicated input side. Small differences between the sides change the timing of the stereo reverberation.";
    else if(name.startsWith("VOICEDELAY"))
        body="Sets the base delay of this chorus voice. Short values thicken the sound; longer values separate it into an echo.";
    else if(name.startsWith("BANDDELAY"))
        body="Sets the delay time for this filtered band. Different times across the bands separate bass, mids and highs into different echoes.";
    else if(name.startsWith("DELAY"))
        body=program==21?"Sets when this resonant voice starts after the input. Different delays stagger the notes into a strum or rhythmic pattern.":
            "Sets the timing of this individual early reflection. Combine it with the matching Level control to place the reflection in the space.";
    else if(name.startsWith("LEVEL")) {
        if(program==22) body="Sets the output level of this filtered delay band. Balance the bands to change the tone of the combined echoes.";
        else if(program==21) body="Sets the level of this resonant voice. Balance the voices to choose which notes stand out.";
        else if(program==20 || program==26) body="Sets the level of this individual chorus voice. Balance it with the other voices to shape the combined chorus and echoes.";
        else body="Sets the level of this individual early reflection. Combine it with the matching Delay control to shape the start of the effect.";
    }
    if(body.isEmpty()) return {}; // Coverage check rejects an unexplained active control.
    if(name.startsWith("LEVEL") || (name.startsWith("DELAY") && program!=21)) body+=route(title);
    else if(split) {
        if(page.type==1) body+=" Applies to the left effect, heard at A/C.";
        else if(page.type==2) body+=" Applies to the right effect, heard at B/D.";
        else if(name.endsWith("LR")) body+=" Shared by the two effects.";
        else if(name.endsWith("L") || name.contains("(L)")) body+=" Applies to the left effect.";
        else if(name.endsWith("R") || name.contains("(R)")) body+=" Applies to the right effect.";
    }
    return tip(title,body);
}
inline juce::String mode(unsigned program,const cineol::xl::ProgramData* data,bool optimization) {
    const auto title=optimization?"Decay optimization":"Mode enhancement";
    if(program>=6 && (!data || (optimization?!data->dynamics.enabled:!(data->modulation.flags&15)))) return unavailable(title);
    if(program>=6 && optimization && data->controls.feedback.count==0)
        return tip(title,"This program has no feedback-diffusion stage for Decay Optimization. The switch is stored, but it does not apply diffusion changes here.");
    if(optimization) return tip(title,"Adapts feedback diffusion as the input level changes and the tail fades. It can change the tail's texture and coloration.");
    return tip(title,program>=6?"Enables the program's delay modulation. Use the Chorus control to change its speed and the amount of audible movement.":
        "Enables moving delay taps in the reverb. This breaks up stationary resonances and can add gentle motion to the tail.");
}
inline juce::String dynamic(const cineol::xl::ProgramData* data) {
    if(!data || !data->dynamics.enabled) return unavailable("Dynamic decay");
    return tip("Dynamic decay","Switches to the LF and Mid Stop Decay settings when the input level falls. Use it to shorten pauses or let tails bloom, depending on the stopped decay settings.");
}
inline juce::String output(unsigned program,bool left) {
    return tip(left?"Left output":"Right output",program>=23?
        "Selects which of the four effect outputs feeds this plugin channel. Split programs use A/C for the left-input effect and B/D for the right-input effect.":
        "Selects which of the four effect outputs feeds this plugin channel. Different pairs can change the stereo balance and the reflections you hear.");
}
} // namespace cineol::help
