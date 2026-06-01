/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct LookAndFeel : juce::LookAndFeel_V4
{
    virtual void drawRotarySlider (juce::Graphics&,
                                   int x, int y, int width, int height,
                                   float sliderPosProportional,
                                   float rotaryStartAngle,
                                   float rotaryEndAngle,
                                   juce::Slider&) override;
};

struct RotarySliderWithLabels: juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) :
    juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag, juce::Slider::TextEntryBoxPosition::NoTextBox),
    param(&rap),
    suffix(unitSuffix)
    {
        setLookAndFeel(&lnf);
    }
    
    // remember to unset LookAndFeel in the destructor
    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }
    
    struct LabelPos
    {
        float pos;
        juce::String label;
    };
    juce::Array<LabelPos> labels;
    
    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;
private:
    // instance of LookAndFeel class
    LookAndFeel lnf;
    
    // base class for member functions that allow info to be pulled from APVTS
    juce::RangedAudioParameter* param;
    juce::String suffix;
};

// creating external components
struct ResponseCurveComponent: juce::Component,
// register as a listener to parameters in order to respond to parameter changes
juce::AudioProcessorParameter::Listener,
juce::Timer
{
    ResponseCurveComponent(SimpleEQAudioProcessor&);
    ~ResponseCurveComponent();
    // necessary callbacks from Listener
    void parameterValueChanged (int parameterIndex, float newValue) override;

    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { }
    
    // necessary callbacks from Timer
    void timerCallback() override;
    
    void paint (juce::Graphics& g) override;
    void resized() override;
    
private:
    SimpleEQAudioProcessor& audioProcessor;
    juce::Atomic<bool> parametersChanged { false };

    MonoChain monoChain;
    
    void updateChain();
    
    juce::Image background;
    
    juce::Rectangle<int> getRenderArea();
    
    juce::Rectangle<int> getAnalysisArea();
};

//==============================================================================
/**
*/
class SimpleEQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor&);
    ~SimpleEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SimpleEQAudioProcessor& audioProcessor;
        
    RotarySliderWithLabels
    peakFreqSlider,
    peakGainSlider,
    peakQualitySlider,
    lowCutFreqSlider,
    highCutFreqSlider,
    lowCutSlopeSlider,
    highCutSlopeSlider;
    
    ResponseCurveComponent responseCurveComponent;
    
    // create an alias to reference apvts
    using APVTS = juce::AudioProcessorValueTreeState;
    // use apvts attachment class to connect sliders to audio params
    using Attachment = APVTS::SliderAttachment;
    
    // declare attachments for each slider
    Attachment peakFreqSliderAttachment,
                peakGainSliderAttachment,
                peakQualitySliderAttachment,
                lowCutFreqSliderAttachment,
                highCutFreqSliderAttachment,
                lowCutSlopeSliderAttachment,
                highCutSlopeSliderAttachment;
    
    // create a vector to store values so it's easy to modify all of them at once by just iterating through
    std::vector<juce::Component*> getComps();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessorEditor)
};
