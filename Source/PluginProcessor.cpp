#include "PluginProcessor.h"

struct ParamDef { const char* id; const char* name; float stonoga::Params::* mem; float lo, hi; };
static const ParamDef kDefs[StonogaProcessor::kNumParams] = {
    {"drive",    "Przester",              &stonoga::Params::drive,    0.f, 1.f},
    {"starve",   "Głód baterii",          &stonoga::Params::starve,   0.f, 1.f},
    {"gate",     "Rozpad (bramka)",       &stonoga::Params::gate,     0.f, 1.f},
    {"octave",   "Oktawa",                &stonoga::Params::octave,   0.f, 1.f},
    {"rust",     "Rdza",                  &stonoga::Params::rust,     0.f, 1.f},
    {"crackle",  "Trzaski",               &stonoga::Params::crackle,  0.f, 1.f},
    {"tone",     "Barwa",                 &stonoga::Params::tone,     0.f, 1.f},
    {"mask",     "Maska",                 &stonoga::Params::mask,     0.f, 1.f},
    {"vowel",    "Samogłoska u-o-a-e-i",  &stonoga::Params::vowel,    0.f, 1.f},
    {"feedback", "Sprzężenie",            &stonoga::Params::feedback, 0.f, 1.f},
    {"fbtime",   "Czas sprzężenia",       &stonoga::Params::fbTime,   0.f, 1.f},
    {"howl",     "Wycie",                 &stonoga::Params::howl,     0.f, 1.f},
    {"revfb",    "Pętla pogłosu",         &stonoga::Params::revfb,    0.f, 1.f},
    {"mix",      "Mix",                   &stonoga::Params::mix,      0.f, 1.f},
    {"reverb",   "Pogłos 1 (śpiew)",      &stonoga::Params::reverb,   0.f, 1.f},
    {"size",     "Rozmiar 1",             &stonoga::Params::size,     0.f, 1.f},
    {"sing",     "Śpiew",                 &stonoga::Params::sing,     0.f, 1.f},
    {"choir",    "Chór",                  &stonoga::Params::choir,    0.f, 1.f},
    {"soft",     "Miękkość",              &stonoga::Params::soft,     0.f, 1.f},
    {"body",     "Pełnia",                &stonoga::Params::body,     0.f, 1.f},
    {"swell",    "Mgła",                  &stonoga::Params::swell,    0.f, 1.f},
    {"chorus",   "Chorus",                &stonoga::Params::chorus,   0.f, 1.f},
    {"chrate",   "Tempo chorusa",         &stonoga::Params::chRate,   0.f, 1.f},
    {"rev2",     "Pogłos 2 (jasny)",      &stonoga::Params::rev2,     0.f, 1.f},
    {"size2",    "Rozmiar 2",             &stonoga::Params::size2,    0.f, 1.f},
    {"rev3",     "Pogłos 3 (otchłań)",    &stonoga::Params::rev3,     0.f, 1.f},
    {"size3",    "Rozmiar 3",             &stonoga::Params::size3,    0.f, 1.f},
    {"output",   "Wyjście (dB)",          &stonoga::Params::outDb,  -40.f, 6.f},
};

const char* StonogaProcessor::paramId (int i) { return kDefs[i].id; }

static juce::String pl (const char* s) { return juce::String::fromUTF8 (s); }

juce::AudioProcessorValueTreeState::ParameterLayout StonogaProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    const stonoga::Params d = stonoga::kPresets[0].p;
    for (const auto& def : kDefs)
        v.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { def.id, 1 }, pl (def.name),
            juce::NormalisableRange<float> (def.lo, def.hi, def.hi > 1.f ? 0.1f : 0.f), d.*(def.mem)));
    return { v.begin(), v.end() };
}

StonogaProcessor::StonogaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STONOGA", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        params[(size_t) i] = apvts.getRawParameterValue (kDefs[i].id);
}

bool StonogaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void StonogaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int nch = juce::jmax (1, juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels()));
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) nch, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    engine.prepare (sampleRate * 4.0, nch);
    setLatencySamples ((int) std::round (oversampling->getLatencyInSamples()));
}

void StonogaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    // gitara mono na ścieżce stereo: kopiujemy lewy kanał na prawy
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int c = juce::jmax (numIn, 2); c < numOut; ++c)
        buffer.clear (c, 0, n);
    if (oversampling == nullptr || n == 0)
        return;

    stonoga::Params p;
    for (int i = 0; i < kNumParams; ++i)
        p.*(kDefs[i].mem) = params[(size_t) i]->load();

    juce::dsp::AudioBlock<float> block (buffer);
    auto up = oversampling->processSamplesUp (block);
    std::array<float*, 8> ptrs {};
    const int nch = (int) std::min<size_t> (up.getNumChannels(), ptrs.size());
    for (int c = 0; c < nch; ++c)
        ptrs[(size_t) c] = up.getChannelPointer ((size_t) c);
    engine.process (ptrs.data(), nch, (int) up.getNumSamples(), p);
    oversampling->processSamplesDown (block);
}

void StonogaProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= stonoga::kNumPresets)
        return;
    currentProgram = index;
    const auto& p = stonoga::kPresets[index].p;
    for (int i = 0; i < kNumParams; ++i)
        if (auto* prm = apvts.getParameter (kDefs[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (p.*(kDefs[i].mem)));
}

const juce::String StonogaProcessor::getProgramName (int index)
{
    if (index < 0 || index >= stonoga::kNumPresets)
        return {};
    return pl (stonoga::kPresets[index].name);
}

void StonogaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void StonogaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// Prosty edytor: pokrętła + wybór presetu, w kolorach drzeworytu.
namespace
{
struct Look : juce::LookAndFeel_V4
{
    Look()
    {
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffc23b2a));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff22385e));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff1b1a1a));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff1b1a1a));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, juce::Colour (0xff1b1a1a));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffe1d1ae));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xff1b1a1a));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff1b1a1a));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xffebdfc4));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xff1b1a1a));
    }
};

class Editor : public juce::AudioProcessorEditor
{
public:
    explicit Editor (StonogaProcessor& p) : AudioProcessorEditor (p), proc (p)
    {
        setLookAndFeel (&look);
        for (int i = 0; i < StonogaProcessor::kNumParams; ++i)
        {
            auto* s = sliders.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
            addAndMakeVisible (s);
            auto* l = labels.add (new juce::Label());
            if (auto* prm = p.apvts.getParameter (StonogaProcessor::paramId (i)))
                l->setText (prm->getName (40), juce::dontSendNotification);
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            addAndMakeVisible (l);
            attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (p.apvts, StonogaProcessor::paramId (i), *s));
        }
        for (int i = 0; i < stonoga::kNumPresets; ++i)
            presets.addItem (p.getProgramName (i), i + 1);
        presets.setTextWhenNothingSelected (juce::String::fromUTF8 ("Presety…"));
        presets.onChange = [this] { const int id = presets.getSelectedId(); if (id > 0) proc.setCurrentProgram (id - 1); };
        addAndMakeVisible (presets);
        setSize (760, 700);
    }
    ~Editor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xffebdfc4));
        auto r = getLocalBounds().reduced (8);
        g.setColour (juce::Colour (0xff1b1a1a));
        g.drawRect (r, 3);
        g.drawRect (r.reduced (6), 1);
        g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)));
        g.drawText ("STONOGA", 26, 18, 300, 40, juce::Justification::left);
        auto seal = juce::Rectangle<float> (230.f, 20.f, 34.f, 40.f);
        g.setColour (juce::Colour (0xffc23b2a));
        g.fillRect (seal);
        g.setColour (juce::Colour (0xfff6ead2));
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawText (juce::String::fromUTF8 ("百足"), seal.toNearestInt(), juce::Justification::centred);
        g.setColour (juce::Colour (0xff6d5f4d));
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (juce::String::fromUTF8 ("przester do noise rocka — nie bój się go zepsuć"), 26, 58, 420, 18, juce::Justification::left);
    }

    void resized() override
    {
        presets.setBounds (getWidth() - 250, 26, 220, 28);
        const int cols = 8, w = 88, h = 150, x0 = 26, y0 = 86;
        for (int i = 0; i < sliders.size(); ++i)
        {
            const int row = i / cols, col = i % cols;
            const int x = x0 + col * w, y = y0 + row * h;
            labels[i]->setBounds (x - 4, y, w + 8, 34);
            sliders[i]->setBounds (x, y + 30, w, h - 40);
        }
    }

private:
    StonogaProcessor& proc;
    Look look;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    juce::ComboBox presets;
};
} // namespace

juce::AudioProcessorEditor* StonogaProcessor::createEditor() { return new Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StonogaProcessor(); }
