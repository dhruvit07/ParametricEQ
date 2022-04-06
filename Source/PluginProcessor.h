/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope {
  Slope_12,
  Slope_24,
  Slope_36,
  Slope_48
}; 

struct ChainSettings
{
  float peakFreq{0}, peakGain{0}, peakQuality{0};
  float lowCutFreq{0}, highCutFreq{0};
  Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState &apvts);

//==============================================================================
/**
 */
class ParametricEQAudioProcessor : public juce::AudioProcessor
{
public:
  //==============================================================================
  ParametricEQAudioProcessor();
  ~ParametricEQAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  //==============================================================================
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
  juce::AudioProcessorValueTreeState apvts{
      *this, nullptr, "Parameters", createParameterLayout()};

private:
  using Filter = juce::dsp::IIR::Filter<float>;
  using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
  using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

  MonoChain leftChain, rightChain;

  using Coefficients = Filter::CoefficientsPtr;
  static void updateCoefficients(Coefficients& old, const Coefficients& replacements);

  enum ChainPositions 
  {
    LowCut,
    Peak,
    HighCut
  };

  void updatePeakFilter(const ChainSettings& chainSettings);

  template<int Index, typename ChainType, typename CoefficientType>
  void update(ChainType& chain, CoefficientType& Coefficients)
  {
      updateCoefficients(chain.template get<Index>().coefficients, Coefficients[Index]);
      chain.template setBypassed<Index>(false);

  }
  
  template<typename ChainType, typename CoefficientType>
  void updateCutFilter(ChainType& leftLowCut, 
                       CoefficientType& cutCoefficients, 
                       Slope& slope) {


      leftLowCut.template setBypassed<0>(true);
      leftLowCut.template setBypassed<1>(true);
      leftLowCut.template setBypassed<2>(true);
      leftLowCut.template setBypassed<3>(true);


      switch ( slope )
      {
      case Slope_48:
          update<3>(leftLowCut, cutCoefficients);

      case Slope_36:
          update <2>(leftLowCut, cutCoefficients);

      case Slope_24:
          update<1>(leftLowCut, cutCoefficients);
        
      case Slope_12:
          update<0>(leftLowCut, cutCoefficients);
         

      }
  }

  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQAudioProcessor)
};
