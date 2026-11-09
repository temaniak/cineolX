#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

// One canonical, lock-free control snapshot. Parameter writes must not
// call host listeners from processing. Their mirrors are published by the
// processor's existing message-thread timer; getValue() is correct immediately.
class CineolOriginal224Parameters {
public:
    enum Field : unsigned {Rate,Depth,Optimization};
    static constexpr unsigned defaults=1u|(4u<<8); // Rate 2, Depth 1, Opt 5.
    static constexpr unsigned program_mask=31u<<12,mode_bit=1u<<17,restore_bit=1u<<18;
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    unsigned snapshot() const noexcept {return value_.load(std::memory_order_acquire);}
    static int get(unsigned state,Field field) noexcept {return int((state>>(4*field))&15)+1;}
    int get(Field field) const noexcept {return get(snapshot(),field);}
    void set(Field field,int value) noexcept {
        update([&](unsigned state) {
            const unsigned shift=4*field,mask=15u<<shift;
            state=(state&~mask)|(unsigned(std::clamp(value,1,16)-1)<<shift);
            // Desktop control behavior: editing Rate or Depth preserves the
            // other value. The native period/step laws and saved values stay
            // unchanged; only v4.4's destructive Shift+7 coupling is removed.
            return state;
        });
    }
    void mode(bool enabled) noexcept {
        update([&](unsigned state) {
            const bool changed=bool(state&mode_bit)!=enabled;
            if(changed && !(state&restore_bit) && ((state&program_mask)>>12)<6)
                state=(state&~4095u)|defaults;
            return enabled?state|mode_bit:state&~mode_bit;
        });
    }
    void program(int program) noexcept {
        update([&](unsigned state) {
            const unsigned bits=unsigned(std::clamp(program,0,27))<<12;
            if((state&program_mask)!=bits && !(state&restore_bit))state=(state&~4095u)|defaults;
            return (state&~program_mask)|bits;
        });
    }
    void beginRestore() noexcept {value_.fetch_or(restore_bit,std::memory_order_acq_rel);}
    void endRestore() noexcept {value_.fetch_and(~restore_bit,std::memory_order_release);}
private:
    template<class Change> void update(Change change) noexcept {
        unsigned before=value_.load(std::memory_order_relaxed);
        while(!value_.compare_exchange_weak(before,change(before),std::memory_order_release,std::memory_order_relaxed)) {}
    }
    std::atomic<unsigned> value_{defaults|(2u<<12)|mode_bit};
};

class CineolHidden224Parameter final : public juce::RangedAudioParameter {
public:
    CineolHidden224Parameter(const char* id,const char* name,CineolOriginal224Parameters::Field field,
        std::shared_ptr<CineolOriginal224Parameters> controls)
        :RangedAudioParameter(juce::ParameterID{id,8},name),controls_(std::move(controls)),field_(field) {}
    const juce::NormalisableRange<float>& getNormalisableRange() const override {return range_;}
    float getValue() const override {return convertTo0to1(float(controls_->get(field_)));}
    void setValue(float normalized) override {
        controls_->set(field_,int(convertFrom0to1(std::isfinite(normalized)?normalized:0.f)));
    }
    float getDefaultValue() const override {return convertTo0to1(float(CineolOriginal224Parameters::get(CineolOriginal224Parameters::defaults,field_)));}
    bool isDiscrete() const override {return true;}
    juce::String getText(float normalized,int) const override {return juce::String(int(convertFrom0to1(normalized)));}
    float getValueForText(const juce::String& text) const override {return convertTo0to1(float(text.getIntValue()));}
    CineolOriginal224Parameters& controls() noexcept {return *controls_;}
private:
    std::shared_ptr<CineolOriginal224Parameters> controls_;
    CineolOriginal224Parameters::Field field_;
    const juce::NormalisableRange<float> range_{1,16,1};
};

class CineolMode224Parameter final : public juce::AudioParameterBool {
public:
    explicit CineolMode224Parameter(std::shared_ptr<CineolOriginal224Parameters> controls)
        :AudioParameterBool(juce::ParameterID{"mode_enh",1},"Mode Enhancement",true),controls_(std::move(controls)) {}
private:
    void valueChanged(bool enabled) override {controls_->mode(enabled);}
    std::shared_ptr<CineolOriginal224Parameters> controls_;
};
class CineolAlgorithmParameter final : public juce::AudioParameterChoice {
public:
    CineolAlgorithmParameter(const juce::StringArray& names,std::shared_ptr<CineolOriginal224Parameters> controls)
        :AudioParameterChoice(juce::ParameterID{"algorithm",2},"Algorithm",names,2),controls_(std::move(controls)) {}
private:
    void valueChanged(int program) override {controls_->program(program);}
    std::shared_ptr<CineolOriginal224Parameters> controls_;
};
