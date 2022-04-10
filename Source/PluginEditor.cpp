/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    using namespace juce;

      auto bounds = Rectangle<float>(x, y, width, height);
    
    //auto enabled = slider.isEnabled();
    
    /*g.setColour(enabled ? Colour(97u, 18u, 167u) : Colours::darkgrey );
    g.fillEllipse(bounds);
    
    g.setColour(enabled ? Colour(255u, 154u, 1u) : Colours::grey);
    g.drawEllipse(bounds, 1.f);*/

    g.setColour(Colour(33u, 33u, 33u));
    g.fillEllipse(bounds);

    g.setColour(Colour(31u, 207u, 54u));
    g.drawEllipse(bounds, 1.f);

    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        Path p;

        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);

        p.addRoundedRectangle(r, 2.f);

        jassert(rotaryStartAngle < rotaryEndAngle);

        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);

        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

        g.fillPath(p);

        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);

        r.setSize(strWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(bounds.getCentre());

        //g.setColour(enabled ? Colours::black : Colours::darkgrey);
        g.setColour(Colours::black);
        g.fillRect(r);

        //g.setColour(enabled ? Colours::white : Colours::lightgrey);
        g.setColour(Colours::white);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    juce::String str;
    bool addK = false;

    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = getValue();

        if (val > 999.f)
        {
            val /= 1000.f; //1001 / 1000 = 1.001
            addK = true;
        }

        str = juce::String(val, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse; //this shouldn't happen!
    }

    if (suffix.isNotEmpty())
    {
        str << " ";
        if (addK)
            str << "k";

        str << suffix;
    }

    return str;

}

void RotarySliderWithLabels::paint(juce::Graphics& g) {

    using namespace juce;

    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    auto range = getRange();

    auto sliderBounds = getSliderBounds();

    /*g.setColour(Colours::white);
    g.drawRect(getLocalBounds()); 
    g.setColour(Colours::white);
    g.drawRect(sliderBounds);*/

    getLookAndFeel().drawRotarySlider(g,
        sliderBounds.getX(),
        sliderBounds.getY(),
        sliderBounds.getWidth(),
        sliderBounds.getHeight(),
        jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng,
        endAng,
        *this);


}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();

    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

    size -= getTextHeight() * 2;
    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);

    return r;

}

ResponseCurveComponent::ResponseCurveComponent(ParametricEQAudioProcessor& p) : audioProcessor(p)
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    } 
    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    }

}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback() {

    if (parametersChanged.compareAndSetBool(false, true))
    {
        //updateChain();

        auto chainSettings = getChainSettings(audioProcessor.apvts);
        auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

        auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
        auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());

        updateCutFilter(monoChain.get<ChainPositions::LowCut>(),
            lowCutCoefficients,
            chainSettings.lowCutSlope);

        updateCutFilter(monoChain.get<ChainPositions::HighCut>(),
            highCutCoefficients,
            chainSettings.highCutSlope);

        repaint();
        //updateResponseCurve();
    }

}

void ResponseCurveComponent::paint(juce::Graphics& g)
{

    using namespace juce;
    auto responseArea = getLocalBounds();

    auto w = responseArea.getWidth();

    auto& lowcut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highcut = monoChain.get<ChainPositions::HighCut>();

    auto sampleRate = audioProcessor.getSampleRate();

    std::vector<double> mags;

    mags.resize(w);

    for (int i = 0; i < w; ++i)
    {
        double mag = 1.f;

        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);

        if (!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!monoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (!lowcut.isBypassed<0>())
                mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<1>())
                mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<2>())
                mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<3>())
                mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (!monoChain.isBypassed<ChainPositions::HighCut>())
        {
            if (!highcut.isBypassed<0>())
                mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<1>())
                mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<2>())
                mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<3>())
                mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        mags[i] = Decibels::gainToDecibels(mag);
    }

    Path responseCurve;

    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input)
    {
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };

    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));

    for (size_t i = 1; i < mags.size(); ++i)
    {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    }

    g.setColour(Colours::grey);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
    g.setColour(Colours::ghostwhite);
    g.strokePath(responseCurve, PathStrokeType(1.5f));

}

//==============================================================================
ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p) 
    : AudioProcessorEditor (&p), audioProcessor (p),
     peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
     peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB/Oct"),
     peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
     lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
     highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
     lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
     highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "db/Oct"),

     responseCurveComponent(audioProcessor),

     peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
     peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
     peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
     lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
     highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
     lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
     highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{

    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }

    


    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 400);
}

ParametricEQAudioProcessorEditor::~ParametricEQAudioProcessorEditor()
{
   
}

//==============================================================================
void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{

    g.fillAll(juce::Colours::wheat);
   

}

void ParametricEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    auto bounds = getLocalBounds();
    bounds.removeFromTop(4);

    auto analyzerEnabledArea = bounds.removeFromTop(25);

    analyzerEnabledArea.setWidth(50);
    analyzerEnabledArea.setX(5);
    analyzerEnabledArea.removeFromTop(2);

    //analyzerEnabledButton.setBounds(analyzerEnabledArea);

    bounds.removeFromTop(5);

    float hRatio = 25.f / 100.f; //JUCE_LIVE_CONSTANT(25) / 100.f;
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * hRatio); //change from 0.33 to 0.25 because I needed peak hz text to not overlap the slider thumb

    responseCurveComponent.setBounds(responseArea);

    bounds.removeFromTop(5);

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    //lowcutBypassButton.setBounds(lowCutArea.removeFromTop(25));
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);

    //highcutBypassButton.setBounds(highCutArea.removeFromTop(25));
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);

    //peakBypassButton.setBounds(bounds.removeFromTop(25));
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQualitySlider.setBounds(bounds);
}



std::vector<juce::Component*> ParametricEQAudioProcessorEditor::getComps()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider,
        &responseCurveComponent

        //,
       // &responseCurveComponent,
        //&lowcutBypassButton,
       // &peakBypassButton,
       // &highcutBypassButton,
       // &analyzerEnabledButton
    };
}
