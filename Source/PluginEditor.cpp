/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics & g,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   float sliderPosProportional,
                                   float rotaryStartAngle,
                                   float rotaryEndAngle,
                                   juce::Slider & slider)
{
    using namespace juce;
    
    // bounding box
    auto bounds = Rectangle<float>(x, y, width, height);
    
    // draw ellipse
    g.setColour(Colour(97u, 18u, 167u));
    g.fillEllipse(bounds);
    
    // draw border
    g.setColour(Colour(255u, 154u, 1u));
    g.drawEllipse(bounds, 1.f);
    
    if( auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider) )
    {
        // draw "hand"
        auto center = bounds.getCentre();
        Path p;
        // rectangle is ± 2px from center
        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5); // need to subtract text height from center so it doesn't overlap
        
        p.addRoundedRectangle(r, 2.f);
        
        jassert(rotaryStartAngle < rotaryEndAngle);
        
        // map slider's normalized value to a radian angle
        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
        
        // rotate rectangle to sliderAngRad using an affine transform
        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));
        
        g.fillPath(p);
        
        // set text size and font
        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text);
        
        r.setSize(strWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(center);
        
        // black background
        g.setColour(Colours::black);
        g.fillRect(r);
        
        // render text
        g.setColour(Colours::white);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}
//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics &g)
{
    using namespace juce;
    
    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;
    
    auto range = getRange();
    
    auto sliderBounds = getSliderBounds();
    
//    g.setColour(Colours::red);
//    g.drawRect(getLocalBounds());
//    g.setColour(Colours::yellow);
//    g.drawRect(sliderBounds);
    
    
    getLookAndFeel().drawRotarySlider(g,
                                      sliderBounds.getX(),
                                      sliderBounds.getY(),
                                      sliderBounds.getWidth(),
                                      sliderBounds.getHeight(),
                                      jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0), // this is where we turn the slider's value into a normalized value
                                      startAng,
                                      endAng,
                                      *this);
    
    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;
    
    g.setColour(Colour(0u, 172u, 1u));
    g.setFont(getTextHeight());
    
    auto numChoices = labels.size();
    for( int i = 0; i < numChoices; ++i )
    {
        auto pos = labels[i].pos;
        jassert(0.f <= pos);
        jassert(pos <= 1.f);
        
        auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);
                
        // use this to find centerpoint of bounding box for this particular angle
        // go some distance away from it that's further out from the edge of the circle
        auto c = center.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1, ang);
        
        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), str), getTextHeight());
        r.setCentre(c);
        // move the text down a little from the edge offset
        r.setY(r.getY() + getTextHeight());
        
        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }
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

juce::String RotarySliderWithLabels::getDisplayString() const
{
    // if our parameter is a choice, we can use that param directly
    if( auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param) )
        return choiceParam->getCurrentChoiceName();
    
    juce::String str;
    bool addK = false;
    
    // use a cast to check if it is actually an audio parameter float.
    if( auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param) )
    {
        // check if value is over 1000 then convert to kHz value
        float val = getValue();
        
        if ( val > 999.f )
        {
            val /= 1000.f;
            addK = true;
        }
        
        // only want 2 decimal places
        str = juce::String(val, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse; // this shouldn't happen!
    }
    
    if( suffix.isNotEmpty() )
    {
        str << " ";
        if( addK )
            str << "k";
        
        str << suffix;
    }
    
    return str;
}

//==============================================================================
ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p) :
audioProcessor(p),
leftChannelFifo(&audioProcessor.leftChannelFifo)
{
    // listen for when the parameters change
    const auto& params = audioProcessor.getParameters();
    for ( auto param : params )
    {
        param->addListener(this);
    }
    
    /*
     We are splitting up the audio spectrum (from 20Hz to 20000Hz) into 2048 or 4096, etc. equal sized frequency bins, which store the magnitude level for each range of frequencies.
     If our sample rate is 48000 and we have 2048 bins, 48000 / 2048 = 23
     Which means each bin is 23Hz in width.
     This means that the lower end of the frequency spectrum will have a lower resolution, because 20Hz to 50Hz is 2 bins whereas 2kHz to 5kHz is like 100 bins.
     When you make the resolution higher, i.e. 4096, you get higher quality on the lower end of the spectrum at the expense of CPU.
     */
    // configure FFT data generator
    leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
    // initialize the mono buffer with the proper size
    monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    
    // update the monochain
    updateChain();
    
    // start timer with a 60hz refresh rate
    startTimerHz(60);
}

// if we register as a listener, we need to deregister as a listener
ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for ( auto param : params )
    {
        param->removeListener(this);
    }
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    // set atomic flag to true
    parametersChanged.set(true);
}

// check if atomic flag was changed in the timer callback
void ResponseCurveComponent::timerCallback()
{
    juce::AudioBuffer<float> tempIncomingBuffer;
    // while there are buffers to pull,
    while( leftChannelFifo->getNumCompleteBuffersAvailable() > 0 )
    {
        // if we can pull a buffer,
        if( leftChannelFifo->getAudioBuffer(tempIncomingBuffer) )
        {
            // send it to the FFT data generator.
            
            // shift the mono buffer forward by however many samples are in the temporary incoming buffer
            auto size = tempIncomingBuffer.getNumSamples();
            // shifting over the data
            // copy everything from 0th index, start reading from that point, continue reading until the size of the buffer
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                                              monoBuffer.getReadPointer(0, size),
                                              monoBuffer.getNumSamples() - size);
            // copy everything from our temp buffer to the end of our mono buffer
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                                              tempIncomingBuffer.getReadPointer(0, 0),
                                              size);
            // send mono buffers to the generator
            leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f); // -48 instead of -infinity since -48 is the bottom of the analyzer
            
        }
    }
    
    const auto fftBounds = getAnalysisArea().toFloat();
    const auto fftSize = leftChannelFFTDataGenerator.getFFTSize();
    const auto bindWidth = audioProcessor.getSampleRate() / (double)fftSize;
    // while there are FFT data buffers to pull,
    while( leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0 )
    {
        std::vector<float> fftData;
        // if we can pull a buffer,
        if( leftChannelFFTDataGenerator.getFFTData(fftData))
        {
            // feed to our path producer.
            pathProducer.generatePath(fftData, fftBounds, fftSize, bindWidth, -48.f);
        }
    }
    
    // while there are paths that can be pulled,
    while( pathProducer.getNumPathsAvailable() )
    {
        // pull as many paths as possible,
        pathProducer.getPath(leftChannelFFTPath);
        // display the most recent path
    }
    
    // if it is true that parameters have been changed, we are going to set the parameters changed value back to false to indicate nothing has changed since the flag was checked
    if ( parametersChanged.compareAndSetBool(false, true) )
    {
        DBG( "params changed" ); // debug statement
        // update the monochain
        updateChain();
        // signal a repaint
//        repaint();
    }
    
    repaint(); // need to repaint all the time because new paths are being produced all the time
}

void ResponseCurveComponent::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
    
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
}

void ResponseCurveComponent::paint (juce::Graphics& g)
{
    using namespace juce; // so we don't have to type "juce::" everywhere
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::black);
    
    // draw the grid
    g.drawImage(background, getLocalBounds().toFloat());

    // area we will draw response curve in
//    auto responseArea = getRenderArea();
    auto responseArea = getAnalysisArea();
    
    auto w = responseArea.getWidth();
    
    // get individual chain element, call getMagnitudeForFrequency for each element in the chain
    auto& lowcut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highcut = monoChain.get<ChainPositions::HighCut>();
    
    // need samplerate for getMagnitudeForFrequency
    auto sampleRate = audioProcessor.getSampleRate();
    
    // need a place to store all the magnitudes, which are returned from getMagnitudeForFrequency as dobules
    std::vector<double> mags;
    
    mags.resize(w);
    
    // iterate through each pixel and compute the magnitude at that frequency
    for ( int i = 0; i < w; ++i )
    {
        // start at 1 because gain is multiplicative
        double mag = 1.f;
        // map normalized pixel number to its frequency within the human hearing range
        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);
        
        // call getMagnitudeForFrequency with this freq and multiply results by mag
        // for each type of chain
        if (! monoChain.isBypassed<ChainPositions::Peak>() )
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        // lowcut links
        if (! lowcut.isBypassed<0>())
            mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowcut.isBypassed<1>())
            mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowcut.isBypassed<2>())
            mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowcut.isBypassed<3>())
            mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        // highcut links
        if (! highcut.isBypassed<0>())
            mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highcut.isBypassed<1>())
            mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highcut.isBypassed<2>())
            mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highcut.isBypassed<3>())
            mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        // convert magnitude into decibels and store it
        mags[i] = Decibels::gainToDecibels(mag);
    }
    
    // convert vector of magnitudes into a path and then draw it
    Path responseCurve;
    
    // map decibel value to response area
    // define maximums and minimums of response area
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input)
    {
        // peak control can go from -24 to +24, so window should reflect that
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    // map our decibels to screen coordinates
    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));
    
    // create lineTos for every other magnitude
    for ( size_t i = 1; i < mags.size(); ++i )
    {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    }
    
    // path
    g.setColour(Colours::blue);
    g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));
    
    // background border
    g.setColour(Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);
    
    // draw the path
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.f));
}

void ResponseCurveComponent::resized()
{
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    
    Graphics g(background);
    
    // draw values for frequency lines
    Array<float> freqs
    {
        20, /* 30, 40,*/ 50, 100,
        200, /*300, 400,*/ 500, 1000,
        2000, /*3000, 4000,*/ 5000, 10000,
        20000
    };
    
    auto renderArea = getAnalysisArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();
    
    Array<float> xs;
    for( auto f : freqs )
    {
        // convert frequency value to normalized position
        auto normX = mapFromLog10(f, 20.f, 20000.f);
        // window based off the edge of our render area
        xs.add(left + width * normX);
    }
    
    g.setColour(Colours::dimgrey);
    for( auto x : xs )
    {
        g.drawVerticalLine(x, top, bottom);
    }
    
    // draw values for gain lines
    Array<float> gain
    {
        -24, -12, 0, 12, 24
    };
    
    for( auto gDb : gain )
    {
        auto y = jmap(gDb, -24.f, 24.f, float(bottom), float(top));
        g.setColour(gDb == 0.f ? Colour(0u, 172u, 1u) : Colours::darkgrey);
        g.drawHorizontalLine(y, left, right);
    }
    
//    g.drawRect(getAnalysisArea());
    
    g.setColour(Colours::lightgrey);
    const int fontHeight = 10;
    g.setFont(fontHeight);
    
    // loop through vertical lines and add frequency marker
    for( int i = 0; i < freqs.size(); ++i )
    {
        auto f = freqs[i];
        auto x = xs[i];
        
        bool addK = false;
        String str;
        if( f > 999.f )
        {
            addK = true;
            f /= 1000.f;
        }
        
        str << f;
        if( addK )
            str << "k";
        str << "Hz";
        
        auto textWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), str);
        
        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);
        r.setY(1);
        
        g.drawFittedText(str, r, juce::Justification::centred, 1);
        
    }
    
    // loop through horizontal lines and add horizontal labels
    for( auto gDb : gain )
    {
        auto y = jmap(gDb, -24.f, 24.f, float(bottom), float(top));
        
        String str;
        if( gDb > 0 )
            str << "+";
        str << gDb;
        
        auto textWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), str);
        
        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        // gain labels on the right
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(), y);
        
        g.setColour(gDb == 0.f ? Colour(0u, 172u, 1u) : Colours::lightgrey);
        
        g.drawFittedText(str, r, juce::Justification::centred, 1);
        
        // decibel labels on the left
        str.clear();
        str << (gDb - 24.f);
        
        r.setX(1);
        textWidth = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), str);
        r.setSize(textWidth, fontHeight);
        g.setColour(Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centred, 1);

        
    }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();
    
    // JUCE_LIVE_CONSTANT must be on separate lines
//    auto xPad = JUCE_LIVE_CONSTANT(5);
//    auto yPad = JUCE_LIVE_CONSTANT(5);
//    bounds.reduce(xPad, yPad);
//    bounds.reduce(10, 8);
    // manually doing all 4 sides since reduce does x and y only
    bounds.removeFromTop(12);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    
    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    return bounds;
}

//==============================================================================
SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "dB/Oct"),
responseCurveComponent(audioProcessor),
peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    // add min and max ranges
    peakFreqSlider.labels.add({0.f, "20Hz"});
    peakFreqSlider.labels.add({1.f, "20kHz"});
    
    peakGainSlider.labels.add({0.f, "-24dB"});
    peakGainSlider.labels.add({1.f, "+24dB"});

    peakQualitySlider.labels.add({0.f, "0.1"});
    peakQualitySlider.labels.add({1.f, "10.0"});
    
    lowCutFreqSlider.labels.add({0.f, "20Hz"});
    lowCutFreqSlider.labels.add({1.f, "20kHz"});
    
    highCutFreqSlider.labels.add({0.f, "20Hz"});
    highCutFreqSlider.labels.add({1.f, "20kHz"});
    
    lowCutSlopeSlider.labels.add({0.f, "12dB/oct"});
    lowCutSlopeSlider.labels.add({1.f, "48dB/oct"});
    
    highCutSlopeSlider.labels.add({0.f, "12dB/oct"});
    highCutSlopeSlider.labels.add({1.f, "48dB/oct"});
    
    // make sliders visible
    for( auto* comp : getComps() )
    {
        addAndMakeVisible(comp);
    }
    
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 480);
}

// if we register as a listener, we need to deregister as a listener
SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{
}

//==============================================================================
void SimpleEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce; // so we don't have to type "juce::" everywhere
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::black);
}

void SimpleEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    // bounding box for the component
    auto bounds = getLocalBounds();
    
    // dedicate an area for the top (waveform visualizer)
//    float hRatio = JUCE_LIVE_CONSTANT(33) / 100.f; // use JUCE_LIVE_CONSTANT to adjust values live!!
    float hRatio = 25.f / 100.f;
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * hRatio);
    
    responseCurveComponent.setBounds(responseArea);
    
    // remove 5px from the top of the bounds to create a larger gap between the knobs and the visualizer
    bounds.removeFromTop(5);
    
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    // now that 33% is removed from left, the remaining area is 66%. So 50% of that is 33%
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    
    // set sliders to be in bounds
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);
    
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);
    
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    // same as highCutArea
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    // set slider to be in bounds
    peakQualitySlider.setBounds(bounds);
}

// return vector with all slider components
std::vector<juce::Component*> SimpleEQAudioProcessorEditor::getComps()
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
    };
}
