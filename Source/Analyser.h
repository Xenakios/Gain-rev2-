/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 6.0.7

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include <JuceHeader.h>
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
template <typename Type>
class Analyser  : public juce::Thread
{
public:
    //==============================================================================
    Analyser() : Thread("Spectrum Analyser")
    {

    }

    virtual ~Analyser() = default;

    void addAudioData(const juce::AudioBuffer<Type>& buffer, int startChannel, int numChannels)
    {
        if (mAbstractFifo.getFreeSpace() < buffer.getNumSamples())
        {           
           return;
        }
        int start1, size1, start2, size2;
        mAbstractFifo.prepareToWrite(buffer.getNumSamples(), start1, size1, start2, size2);
        mAudioFifo.copyFrom(0, start1, buffer.getReadPointer(startChannel), size1);
        if (size2 > 0)
            mAudioFifo.copyFrom(0, start2, buffer.getReadPointer(startChannel, size1), size2);
        for (int channel = startChannel + 1; channel < startChannel + numChannels; ++channel)
        {
            if (size1 > 0)
                mAudioFifo.addFrom(0, start1, buffer.getReadPointer(channel), size1);
            if (size2 > 0)
                mAudioFifo.addFrom(0, start2, buffer.getReadPointer(channel, size1), size2);
        }
        mAbstractFifo.finishedWrite(size1 + size2);
        mWaitData.signal();

    }

    void setupAnalyser(int audioFifoSize, Type sampleRateToUse)
    {        
        mSampleRate = sampleRateToUse;
        mAudioFifo.setSize(1, audioFifoSize);
        mAbstractFifo.setTotalSize(audioFifoSize);

        startThread(7);
    }
    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    //[/UserMethods]

    //void paint (juce::Graphics& g) override;
    //void resized() override;
    

    void run() override
    {
        while (!threadShouldExit()) 
        {
            if (mAbstractFifo.getNumReady() >= mFFT.getSize())
            {
                mFFTbuffer.clear();

                int start1, size1, start2, size2;
                mAbstractFifo.prepareToRead(mFFT.getSize(), start1, size1, start2, size2);
                if (size1 > 0)
                    mFFTbuffer.copyFrom(0, 0, mAudioFifo.getReadPointer(0, start1), size1);
                if (size2 > 0)
                    mFFTbuffer.copyFrom(0, size1, mAudioFifo.getReadPointer(0, start2), size2);
                mAbstractFifo.finishedRead((size1 + size2) / 2);

                mWindowing.multiplyWithWindowingTable(mFFTbuffer.getWritePointer(0), size_t(mFFT.getSize()));
                mFFT.performFrequencyOnlyForwardTransform(mFFTbuffer.getWritePointer(0));

                juce::ScopedLock mLockedForWriting(mPathCreationLock);
                mAvger.addFrom(0, 0, mAvger.getReadPointer(mAvgerPtr), mAvger.getNumSamples(), -1.0f);
                mAvger.copyFrom(mAvgerPtr, 0, mFFTbuffer.getReadPointer(0), mAvger.getNumSamples(),
                    1.0f / (mAvger.getNumSamples() * (mAvger.getNumChannels() - 1)));
                mAvger.addFrom(0, 0, mAvger.getReadPointer(mAvgerPtr), mAvger.getNumSamples());
                if (++mAvgerPtr == mAvger.getNumChannels())
                    mAvgerPtr = 1;

                mNewDataAvailable = true;
            }
            if (mAbstractFifo.getNumReady() < mFFT.getSize())
                mWaitData.wait(100);
        }
    }

    bool checkDataAvailable()
    {
        auto available = mNewDataAvailable.load();
        mNewDataAvailable.store(false);
        return available;
    }

    void createPath(juce::Path& p, const juce::Rectangle<float> bounds, float minFreq)
    {
        p.clear();
        p.preallocateSpace(8 + mAvger.getNumSamples() * 3);

        juce::ScopedLock lockedforReading(mPathCreationLock);
        const auto* fftData = mAvger.getReadPointer(0);
        const auto factor = bounds.getWidth() / 10.0f;

        p.startNewSubPath(bounds.getX() + factor * indexToX(0, minFreq), binToY(fftData[0], bounds));
        for (int i = 0; i < mAvger.getNumSamples(); ++i)
            p.lineTo(bounds.getX() + factor * indexToX(i, minFreq), binToY(fftData[i], bounds));
    }
private:

    inline float indexToX(float index, float minFreq) const
    {
        const auto freq = (mSampleRate * index) / mFFT.getSize();
        return (freq > 0.01f) ? std::log(freq / minFreq) / std::log(2.0f) : 0.0f;
    }

    inline float binToY(float bin, const juce::Rectangle<float> bounds) const
    {
        const float infinity = -80.0f;
        return juce::jmap(juce::Decibels::gainToDecibels(bin, infinity), infinity, 0.0f, bounds.getBottom(), bounds.getY());
    }
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    juce::dsp::FFT mFFT{ 12 };
    juce::AudioBuffer<float> mFFTbuffer{ 1, mFFT.getSize() * 2 };
    juce::dsp::WindowingFunction<Type> mWindowing{ size_t(mFFT.getSize()), juce::dsp::WindowingFunction<Type>::hann, true };

    juce::AudioBuffer<float> mAvger{ 5, mFFT.getSize() / 2 };
    int mAvgerPtr = 1;

    //==============================================================================
    juce::AbstractFifo mAbstractFifo{ 48000 };
    juce::AudioBuffer<Type> mAudioFifo;
    
    juce::WaitableEvent mWaitData;

    std::atomic<bool> mNewDataAvailable;
    Type mSampleRate{};

    juce::CriticalSection mPathCreationLock;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Analyser)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

